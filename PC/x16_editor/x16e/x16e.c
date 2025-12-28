#include "inc.h"
#include "defs.h"
#include "list.h"
#include "engine.h"
#include "editor.h"
#include "things.h"
#include "tick.h"
#include "x16e.h"
#include "x16g.h"
#include "x16r.h"
#include "x16t.h"

#define MAP_VERSION	25
#define MAP_MAGIC	0x36315870614D676B

#define WALL_BANK_COUNT	16
#define WALL_BANK_SIZE	4096
#define WALL_BANK_WCNT	(WALL_BANK_SIZE / 16)

#define MAX_LIGHTS	8	// engine limit is 8
#define MAX_TEXTURES	128	// engine limit is 128
#define MAX_THINGS	200	// engine limit is 255; but there should be some space for players, projectiles and so on
#define MAX_PLAYER_STARTS	256

#define WALL_MARK_SWAP	0x8000
#define WALL_MARK_EXTENDED	0x4000
#define WALL_MARK_XORIGIN	0x2000

#define ERROR_TEXT_TITLE	"Map export failed!"
#define ERROR_COMMON_TEXT	"Your map can not be exported!\nThe reason for failed export is:\n\n%s"
#define ERROR_COLOR	0x2020F0
#define PASS_COLOR	0xA0A0A0

typedef struct
{
	int16_t x, y;
} __attribute__((packed)) vertex_t;

typedef struct
{
	uint8_t texture;
	uint8_t ox;
	uint8_t oy;
} __attribute__((packed)) tex_info_t;

typedef struct
{
	vertex_t vtx; // $00
	vertex_t dist; // $04
	uint16_t angle; // $08
	uint8_t next; // $0A
	uint8_t backsector; // $0B
	uint8_t tflags; // $0C
	tex_info_t top; // $0D
	tex_info_t bot; // $10
	tex_info_t mid; // $13
	uint8_t blocking; // $16
	uint8_t blockmid; // $17
	int16_t split; // $18
	uint8_t special; // $1A
	uint8_t padding[5]; // $1B
} __attribute__((packed)) wall_t;

typedef struct
{
	int16_t height;
	uint8_t texture;
	uint8_t ox, oy;
	uint8_t sx, sy;
	uint8_t angle;
	uint8_t link;
} __attribute__((packed)) sector_plane_t;

typedef struct
{
	// 32 bytes total
	sector_plane_t floor;
	sector_plane_t ceiling;
	struct
	{
		uint8_t bank;
		uint8_t first;
	} wall;
	uint8_t flags; // light, palette, underwater
	uint8_t midheight;
	//
	uint8_t filler[9];
	// internal
	uint8_t midhit;
} __attribute__((packed)) sector_t;

typedef struct
{
	uint32_t hash;
	int16_t x, y, z;
	uint8_t sector;
	uint8_t angle;
	uint8_t flags;
	uint8_t extra;
} __attribute__((packed)) thing_t;

typedef struct
{
	int16_t x, y, z;
	uint8_t sector;
	uint8_t angle;
	uint8_t pitch;
	uint8_t flags;
	uint8_t unused[6];
} player_start_t;

typedef struct
{
	uint64_t magic;
	uint8_t version;
	uint8_t flags;
	//
	uint8_t count_lights;
	uint8_t count_ptex;
	uint8_t count_wtex;
	uint8_t count_textures;
	uint8_t count_starts_normal;
	uint8_t count_starts_coop;
	uint8_t count_starts_dm;
	uint8_t count_wbanks;
	uint8_t count_things;
	//
	uint8_t unused[9];
	//
	uint32_t hash_sky;
} map_head_t;

typedef struct
{
	uint32_t hash;
	uint8_t lights;
} __attribute__((packed)) marked_texture_t;

typedef struct
{
	uint32_t nhash;
	uint32_t vhash;
} marked_variant_t;

typedef struct
{
	uint32_t used;
	uint8_t data[WALL_BANK_SIZE];
} wall_block_t;

//

uint_fast8_t x16e_enable_logo;
uint8_t x16e_logo_data[160 * 120];

static uint8_t temp_data[160 * 120];

static uint32_t validcount;

//

static uint32_t marked_lights;

static uint32_t count_ptex;
static marked_texture_t marked_planes[MAX_TEXTURES];

static uint32_t count_wtex;
static marked_texture_t marked_walls[MAX_TEXTURES];

static uint32_t count_textures;
static marked_variant_t marked_variant[MAX_TEXTURES];

static uint32_t count_starts[3];

static sector_t map_sectors[255];
static uint8_t map_block_data[8192];

static wall_block_t wall_block[WALL_BANK_COUNT];

static kge_thing_t *player_starts[3][MAX_PLAYER_STARTS];

//

static map_head_t map_head =
{
	.magic = MAP_MAGIC,
	.version = MAP_VERSION,
};

//
// errors

static void error_generic(const char *text)
{
	sprintf(edit_info_box_text, ERROR_COMMON_TEXT, text);
	edit_ui_info_box(ERROR_TEXT_TITLE, ERROR_COLOR);
}

static void error_texture_count()
{
	error_generic("Too many unique textures!");
}

static void error_light_count()
{
	error_generic("Too many unique lights!");
}

//
// stuff

static uint8_t convert_pitch(uint16_t pitch)
{
	pitch += 0x8000;
	pitch >>= 8;

	if(pitch < 0x4A)
		pitch = 0x4A;
	else
	if(pitch > 0xB6)
		pitch = 0xB6;

	return pitch;
}

static void fix_marked_lights(marked_texture_t *mt, uint32_t mc)
{
	for(uint32_t i = 0; i < mc; i++)
	{
		uint32_t lights = 0;
		uint32_t li = 1;

		for(uint32_t j = 0; j < x16_num_lights; j++)
		{
			if(!(marked_lights & (1 << j)))
				continue;

			if(mt->lights & (1 << j))
				lights |= li;

			li <<= 1;
		}

		mt->lights = lights;
		mt++;
	}
}

static uint32_t add_texture(marked_texture_t *mt, uint32_t *mc, uint32_t hash, uint32_t light)
{
	uint32_t i;

	for(i = 0; i < *mc; i++, mt++)
	{
		if(mt->hash == hash)
		{
			mt->lights |= 1 << light;
			return 0;
		}
	}

	if(i >= 256)
		return 1;

	*mc = i + 1;

	mt->hash = hash;
	mt->lights |= 1 << light;

	return 0;
}

static int32_t mark_texture(uint32_t idx, uint32_t light)
{
	editor_texture_t *et = editor_texture + idx;

	if(idx == 1)
		return 0xFF;

	if(!idx)
		return MAX_TEXTURES;

	marked_lights |= 1 << light;

	if(et->type == X16G_TEX_TYPE_WALL || et->type == X16G_TEX_TYPE_WALL_MASKED)
	{
		if(add_texture(marked_walls, &count_wtex, et->nhash, light))
			return -1;
	} else
	{
		if(add_texture(marked_planes, &count_ptex, et->nhash, light))
			return -1;
	}

	for(uint32_t i = 0; i < count_textures; i++)
	{
		if(marked_variant[i].nhash == et->nhash)
		{
			if(et->type == X16G_TEX_TYPE_PLANE_8BPP)
				return i;
			if(marked_variant[i].vhash == et->vhash)
				return i;
		}
	}

	if(count_textures >= MAX_TEXTURES)
		return -1;

	marked_variant[count_textures].nhash = et->nhash;
	marked_variant[count_textures].vhash = et->vhash;

	return count_textures++;
}

static uint32_t find_light_id(uint32_t idx)
{
	uint32_t ret = 0;

	for(uint32_t i = 0; i < x16_num_lights; i++)
	{
		if(!(marked_lights & (1 << i)))
			continue;
		if(idx == i)
			return ret;
		ret++;
	}

	return 0;
}

static void place_struct(uint8_t *base, uint32_t idx, void *src, uint32_t ss)
{
	for(uint32_t i = 0; i < ss; i++)
	{
		uint8_t *dst = base + i * 256;
		dst[idx] = *(uint8_t*)(src + i);
	}
}

static void conv_player_start(kge_thing_t *th, player_start_t *ps)
{
	memset(ps, 0, sizeof(player_start_t));

	ps->x = th->pos.x;
	ps->y = th->pos.y;
	ps->z = th->pos.z;
	ps->sector = list_get_idx(&edit_list_sector, (link_entry_t*)th->pos.sector - 1) + 1;
	ps->angle = 0x100 - (th->pos.angle >> 8);
	ps->pitch = convert_pitch(th->pos.pitch);
	ps->flags = 0;

}

static void write_player_starts(int32_t fd)
{
	uint32_t idx = 0;
	player_start_t ps;

	memset(map_block_data, 0xFF, sizeof(map_block_data));

	for(uint32_t j = 0; j < 3; j++)
	{
		for(uint32_t i = 0; i < count_starts[j]; i++)
		{
			conv_player_start(player_starts[j][i], &ps);
			place_struct(map_block_data, idx, &ps, sizeof(player_start_t));
			idx++;
		}
	}

	write(fd, map_block_data, 256 * 16);
}

//
// API

uint32_t x16_init()
{
	// export buffer
	edit_cbor_buffer = malloc(EDIT_EXPORT_BUFFER_SIZE);
	if(!edit_cbor_buffer)
		return 1;

	mkdir(X16_DIR_DATA, 0777);
	mkdir(X16_PATH_DEFS, 0777);
	mkdir(X16_PATH_MAPS, 0777);
	mkdir(X16_PATH_EXPORT, 0777);

	if(x16r_init())
		return 1;

	if(x16g_init())
		return 1;

	if(x16t_init())
		return 1;

	return 0;
}

void x16_export_map()
{
	sector_t *map_sector;
	link_entry_t *ent;
	int32_t ret;
	int32_t fd;
	uint32_t count_walls = 0;
	uint32_t count_things = 0;
	uint32_t count_wbanks = 0;
	wall_t wall_tmp[EDIT_MAX_SECTOR_LINES];
	uint8_t widx[EDIT_MAX_SECTOR_LINES];

	if(edit_check_map())
		return;

	validcount++;

	marked_lights = 1;

	count_textures = 0;
	count_ptex = 0;
	count_wtex = 0;

	count_starts[0] = 0;
	count_starts[1] = 0;
	count_starts[2] = 0;

	memset(marked_planes, 0, sizeof(marked_planes));
	memset(marked_walls, 0, sizeof(marked_walls));

	memset(map_sectors, 0, sizeof(map_sectors));

	memset(wall_block, 0, sizeof(wall_block));
	wall_block[0].used = 1; // very fist wall is used for hacks

	memset(map_block_data, 0, sizeof(map_block_data));

	// count things, add player starts
	ent = tick_list_normal.top;
	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			kge_thing_t *th = (kge_thing_t*)tickable->data;

			if(th->pos.sector)
			{
				switch(th->prop.type & 0xFF0000)
				{
					case 0x000000:
						if(count_things >= MAX_THINGS)
						{
							error_generic("Too many things!");
							return;
						}
						count_things++;
					break;
					case 0x020000:
						switch(th->prop.type)
						{
							case THING_TYPE_START_NORMAL:
								if(count_starts[0] >= MAX_PLAYER_STARTS)
								{
									error_generic("Too many normal player starts!");
									return;
								}
								player_starts[0][count_starts[0]] = th;
								count_starts[0]++;
							break;
							case THING_TYPE_START_COOP:
								if(count_starts[1] >= MAX_PLAYER_STARTS)
								{
									error_generic("Too many coop player starts!");
									return;
								}
								player_starts[1][count_starts[1]] = th;
								count_starts[1]++;
							break;
							case THING_TYPE_START_DM:
								if(count_starts[2] >= MAX_PLAYER_STARTS)
								{
									error_generic("Too many deathmatch player starts!");
									return;
								}
								player_starts[2][count_starts[2]] = th;
								count_starts[2]++;
							break;
						}
					break;
				}
			}
		}

		ent = ent->next;
	}

	// checks
	if(!count_things)
	{
		error_generic("At least one thing must be placed!");
		return;
	}

	if(count_starts[0] + count_starts[1] + count_starts[2] > MAX_PLAYER_STARTS)
	{
		error_generic("Too many combined player starts!");
		return;
	}

	// go trough sectors
	map_sector = map_sectors;
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);
		uint32_t mid_height = 0;
		int32_t wall_bank = -1;
		int32_t wall_first;
		int32_t wfrst = -1;
		uint32_t wcnt;

		memset(wall_tmp, 0, sizeof(wall_tmp));

		// go trough lines
		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = &sec->line[i];
			wall_t *wall = wall_tmp + i;
			uint16_t aflags = 0;
			kge_vertex_t *v0, *v1;
			float dist, xx, yy, dx, dy;

			count_walls++;

			v0 = line->vertex[1];
			v1 = line->vertex[0];

			wall->vtx.x = v0->x;
			wall->vtx.y = v0->y;

			wall->mid.texture = 0xFF;
			wall->split = -32768;

			if(line->backsector)
			{
				aflags = WALL_MARK_EXTENDED;

				if(line->vc.x16port != validcount)
				{
					kge_line_t *ol;

					ol = e2d_find_other_line(line->backsector, sec, line);
					if(ol)
						ol->vc.x16port = validcount;
				}

				ret = mark_texture(line->texture[1].idx, sec->light.idx);
				if(ret < 0)
				{
					error_texture_count();
					return;
				}

				wall->backsector = list_get_idx(&edit_list_sector, (link_entry_t*)line->backsector - 1) + 1;

				wall->blocking = line->info.blocking;
				wall->bot.texture = ret;
				wall->bot.ox = line->texture[1].ox;
				wall->bot.oy = line->texture[1].oy;

				wall->tflags = line->texture[1].flags << 4;

				if(line->texture[2].idx)
				{
					editor_texture_t *et = editor_texture + line->texture[2].idx;

					ret = mark_texture(line->texture[2].idx, sec->light.idx);
					if(ret < 0)
					{
						error_texture_count();
						return;
					}

					if(line->texture[2].flags & TEXFLAG_MIRROR_X)
						wall->tflags |= 0b00001000;

					if(line->texture[2].flags & TEXFLAG_PEG_MID_BACK)
						wall->tflags |= 0b10000000;
					else
					{
						editor_texture_t *et = editor_texture + line->texture[2].idx;
						mid_height = et->height + line->texture[2].oy;
					}

					wall->blockmid = line->info.blockmid & 0b01111111;
					wall->mid.texture = ret;
					wall->mid.ox = line->texture[2].ox;
					wall->mid.oy = line->texture[2].oy;
				}
			} else
			{
				wfrst = i;

				if(line->texture_split != INFINITY)
				{
					aflags = WALL_MARK_EXTENDED;

					ret = mark_texture(line->texture[1].idx, sec->light.idx);
					if(ret < 0)
					{
						error_texture_count();
						return;
					}

					wall->split = line->texture_split;

					wall->bot.texture = ret;
					wall->bot.ox = line->texture[1].ox;
					wall->bot.oy = line->texture[1].oy;

					wall->tflags |= line->texture[1].flags << 4;
				}

				if(line->texture[2].idx)
				{
					aflags = WALL_MARK_EXTENDED;

					ret = mark_texture(line->texture[2].idx, sec->light.idx);
					if(ret < 0)
					{
						error_texture_count();
						return;
					}

					wall->mid.texture = ret;
					wall->mid.ox = line->texture[2].ox;
					wall->mid.oy = line->texture[2].oy;

					if(line->texture[2].flags & TEXFLAG_MIRROR_X)
						wall->tflags |= 0b00001000;
				}
			}

			if(wfrst < 0)
			{
				// find first line
				for(uint32_t i = 0; i < sec->line_count; i++)
				{
					uint32_t ii = sec->line_count - i - 1; // reverse order
					kge_line_t *line = sec->line + ii;
					kge_line_t *lprv = line + 1;

					if(lprv >= sec->line + sec->line_count)
						lprv = sec->line;

					if(line->backsector == lprv->backsector)
						continue;

					wfrst = i;
					break;
				}
			}

			ret = mark_texture(line->texture[0].idx, sec->light.idx);
			if(ret < 0)
			{
				error_texture_count();
				return;
			}

			wall->top.texture = ret;
			wall->top.ox = line->texture[0].ox;
			wall->top.oy = line->texture[0].oy;
			wall->tflags |= line->texture[0].flags;

			if(line->info.flags & WALLFLAG_PEG_X)
				aflags |= WALL_MARK_XORIGIN;

			if(line->vc.x16port == validcount)
				aflags |= WALL_MARK_SWAP;

			wall->special = 0;

			xx = v0->x - v1->x;
			yy = v0->y - v1->y;

			dist = sqrtf(xx * xx + yy * yy) / 256.0f;

			dx = xx / dist;
			dy = yy / dist;

			wall->dist.x = dx;
			wall->dist.y = dy;

			wall->angle = aflags | line->stuff.x16angle;
		}

		// index walls
		wcnt = 0;
		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			wall_t *wall = wall_tmp + i;

			widx[i] = wcnt++;

			if(wall->angle & WALL_MARK_EXTENDED)
				wcnt++;
		}

		// link walls
		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			wall_t *wall = wall_tmp + i;
			int32_t ii = i - 1;
			if(ii < 0)
				ii = sec->line_count - 1;
			wall->next = widx[ii];
		}

		// find wall bank
		for(uint32_t i = 0; i < WALL_BANK_COUNT; i++)
		{
			wall_block_t *wb = wall_block + i;
			uint32_t total = wb->used + wcnt;
			uint32_t idx;

			if(total > WALL_BANK_WCNT)
				continue;

			if(count_wbanks < i)
				count_wbanks = i;

			// fix base and expand
			wall_bank = i;
			idx = wb->used;
			for(uint32_t i = 0; i < sec->line_count; i++)
			{
				wall_t *wall = wall_tmp + i;

				wall->next += wb->used;

				if(i == wfrst)
					wall_first = idx;

				place_struct(wb->data, idx++, wall, 16);
				if(wall->angle & WALL_MARK_EXTENDED)
					place_struct(wb->data, idx++, (void*)wall + 16, 16);
			}

			wb->used = total;

			break;
		}

		if(wall_bank < 0)
		{
			error_generic("Out of space for walls!");
			return;
		}

		// export sector
		ret = mark_texture(sec->plane[PLANE_BOT].texture.idx, sec->light.idx);
		if(ret < 0)
		{
			error_texture_count();
			return;
		}
		map_sector->floor.texture = ret;

		ret = mark_texture(sec->plane[PLANE_TOP].texture.idx, sec->light.idx);
		if(ret < 0)
		{
			error_texture_count();
			return;
		}
		map_sector->ceiling.texture = ret;

		map_sector->floor.height = sec->plane[PLANE_BOT].height;
		map_sector->floor.ox = sec->plane[PLANE_BOT].texture.ox;
		map_sector->floor.oy = sec->plane[PLANE_BOT].texture.oy;
		map_sector->floor.angle = sec->plane[PLANE_BOT].texture.angle;
		if(sec->plane[PLANE_BOT].link)
			map_sector->floor.link = list_get_idx(&edit_list_sector, (link_entry_t*)sec->plane[PLANE_BOT].link - 1) + 1;

		map_sector->ceiling.height = sec->plane[PLANE_TOP].height;
		map_sector->ceiling.ox = sec->plane[PLANE_TOP].texture.ox;
		map_sector->ceiling.oy = sec->plane[PLANE_TOP].texture.oy;
		map_sector->ceiling.angle = sec->plane[PLANE_TOP].texture.angle;
		if(sec->plane[PLANE_TOP].link)
			map_sector->ceiling.link = list_get_idx(&edit_list_sector, (link_entry_t*)sec->plane[PLANE_TOP].link - 1) + 1;

		map_sector->wall.bank = wall_bank;
		map_sector->wall.first = wall_first;

		map_sector->midheight = mid_height;

		// flags will be added later

		map_sector++;

		ent = ent->next;
	}

	count_wbanks++;

	// count lights and make remap table
	map_head.count_lights = 0;
	for(uint32_t i = 1; i < x16_num_lights; i++)
	{
		if(!(marked_lights & (1 << i)))
			continue;

		if(map_head.count_lights >= MAX_LIGHTS)
		{
			error_light_count();
			return;
		}

		map_head.count_lights++;
	}

	// fix marked lights in every texture
	fix_marked_lights(marked_planes, count_ptex);
	fix_marked_lights(marked_walls, count_wtex);

	// get lights in sectors
	map_sector = map_sectors;
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		map_sector->flags = sec->stuff.palette << 2;
		map_sector->flags |= sec->stuff.x16flags;
		map_sector->flags |= find_light_id(sec->light.idx) << 4;
		map_sector++;

		ent = ent->next;
	}

	// save map info
	map_head.count_ptex = count_ptex;
	map_head.count_wtex = count_wtex;
	map_head.count_textures = count_textures;
	map_head.count_starts_normal = count_starts[0];
	map_head.count_starts_coop = count_starts[1];
	map_head.count_starts_dm = count_starts[2];
	map_head.count_wbanks = count_wbanks;
	map_head.count_things = count_things;
	map_head.hash_sky = edit_sky_num < 0 ? 0 : editor_sky[edit_sky_num].hash;
	map_head.flags = 0;

	if(x16e_enable_logo)
		map_head.flags |= 0x80;

	// save file
	fd = open("data/export/DEFAULT.MAP", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(fd < 0)
	{
		error_generic("Unable to create map file!");
		return;
	}

	// map header
	write(fd, &map_head, sizeof(map_head));

	// map logo
	if(x16e_enable_logo)
	{
		uint8_t *dst = temp_data;

		for(uint32_t x = 0; x < 160; x++)
		{
			uint8_t *src = x16e_logo_data + x;

			for(uint32_t y = 0; y < 120; y++)
			{
				*dst++ = *src;
				src += 160;
			}
		}
		write(fd, temp_data, 160 * 120);
	}

	// light list; skip white light
	for(uint32_t i = 1; i < x16_num_lights; i++)
	{
		if(!(marked_lights & (1 << i)))
			continue;
		write(fd, &editor_light[i].hash, sizeof(uint32_t));
	}

	// plane list
	write(fd, marked_planes, count_ptex * sizeof(marked_texture_t));

	// wall list
	write(fd, marked_walls, count_wtex * sizeof(marked_texture_t));

	// texture list
	write(fd, marked_variant, count_textures * sizeof(marked_variant_t));

	// sectors
	for(uint32_t i = 1; i < 256; i++)
		place_struct(map_block_data, i, map_sectors + i - 1, sizeof(sector_t));
	write(fd, map_block_data, 256 * 32);

	// player starts
	write_player_starts(fd);

	// wall banks
	for(uint32_t i = 0; i < count_wbanks; i++)
		write(fd, wall_block[i].data, WALL_BANK_SIZE);

	// things
	ent = tick_list_normal.top;
	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			kge_thing_t *th = (kge_thing_t*)tickable->data;

			if(	th->pos.sector &&
				th->prop.type < MAX_X16_THING_TYPES
			){
				thing_t mt;

				mt.hash = thing_info[th->prop.type].name.hash;
				mt.x = th->pos.x;
				mt.y = th->pos.y;
				mt.z = th->pos.z;
				mt.sector = list_get_idx(&edit_list_sector, (link_entry_t*)th->pos.sector - 1) + 1;
				mt.angle = 0x100 - (th->pos.angle >> 8);
				mt.flags = 0;
				mt.extra = 0;

				write(fd, &mt, sizeof(mt));
			}
		}

		ent = ent->next;
	}

	// done
	close(fd);

	// export OK
printf("EXPORTED OK; sec %u ln %u th %u wb %u\n", edit_list_sector.count, count_walls, count_things, count_wbanks);
/*
	sprintf(edit_info_box_text,	"Sector count: %u\n"
					"Wall count: %u\n"
					"Unique light count: %u\n"
					"Map data size: %u out of 32768 bytes\n\n"
					"Texture count: %u out of %u\n"
					"Texture memory: %u out of %u blocks, %.2f %%\n"
					"Colormap count: %u out of %u\n"
					"Unused tile count: %u in %u texture%s\n"
					"Wasted tile count: %u\n"
					"Worst combo ratio: %.2f between \"%s\" and \"%s\"",
					edit_list_sector.count,
					count_walls,
					num_export_lights,
					size,
					num_export_textures, MAX_EXPORT_TEXTURES,
					num_dmap_base, MAX_TILE_BASES, (float)num_dmap_base * 100.f / (float)MAX_TILE_BASES,
					num_export_cmap, MAX_EXPORT_COLORMAPS - num_export_lights,
					num_unused_tiles, num_wasted_combos, num_wasted_combos == 1 ? "" : "s",
					num_wasted_tiles,
					worst_combo_ratio, combo_n[0], combo_n[1]
				);
	edit_ui_info_box("-= Export statistics =-", PASS_COLOR);
*/
}

