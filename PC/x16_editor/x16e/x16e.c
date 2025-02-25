#include "inc.h"
#include "defs.h"
#include "engine.h"
#include "editor.h"
#include "things.h"
#include "list.h"
#include "tick.h"
#include "x16e.h"
#include "x16g.h"
#include "x16r.h"
#include "x16t.h"

#define MAP_VERSION	16
#define MAP_MAGIC	0x36315870614D676B

#define MAX_BANKS	4
#define BANK_SIZE	8192

#define MAX_LIGHTS	8	// engine limit is 8
#define MAX_TEXTURES	128	// engine limit is 128
#define MAX_THINGS	200	// engine limit is 255; but there should be some space for players, projectiles and so on
#define MAX_PLAYER_STARTS	32	// amount per start type

#define MARK_SPLIT	0x1000
#define MARK_PORTAL	0x2000
#define MARK_SCROLL	0x4000
#define MARK_LAST	0x8000
#define MARK_TYPE_BITS	(MARK_SCROLL | MARK_PORTAL | MARK_SPLIT)
#define MARK_MID_BITS	(MARK_PORTAL | MARK_SPLIT)
#define MARK_MASKED	MARK_MID_BITS

#define ERROR_TEXT_TITLE	"Map export failed!"
#define ERROR_COMMON_TEXT	"Your map can not be exported!\nThe reason for failed export is:\n\n%s"
#define ERROR_COLOR	0x2020F0
#define PASS_COLOR	0xA0A0A0

typedef union
{
	uint32_t c;
	int16_t t[2];
} tile_combo_t;

typedef struct
{
	int8_t x, y;
} wall_scroll_t;

typedef struct
{
	int16_t x, y;
} vertex_t;

typedef struct
{
	vertex_t vtx; // A
} wall_end_t;

typedef struct
{
	vertex_t vtx; // A
	vertex_t dist; // B
	uint16_t angle; // C
	uint16_t special; // D
	uint8_t tflags; // E
	uint8_t texture; // F
	uint8_t tex_ox; // G
	uint8_t tex_oy; // H
	// MARK_SCROLL
	wall_scroll_t scroll;
} __attribute__((packed)) wall_solid_t;

typedef struct
{
	vertex_t vtx; // A
	vertex_t dist; // B
	uint16_t angle; // C
	uint16_t special; // D
	uint8_t tflags; // E
	uint8_t texture_top; // F
	uint8_t tex_top_ox; // G
	uint8_t tex_top_oy; // H
	uint8_t texture_bot; // I
	uint8_t tex_bot_ox; // J
	uint8_t tex_bot_oy; // K
	// this order is forced by 6502 ASM
	uint8_t backsector; // L
	uint8_t blocking; // M
	// MARK_SCROLL
	wall_scroll_t scroll_top;
	wall_scroll_t scroll_bot;
} __attribute__((packed)) wall_portal_t;

typedef struct
{
	vertex_t vtx; // A
	vertex_t dist; // B
	uint16_t angle; // C
	uint16_t special; // D
	uint8_t tflags; // E
	uint8_t texture_top; // F
	uint8_t tex_top_ox; // G
	uint8_t tex_top_oy; // H
	uint8_t texture_bot; // I
	uint8_t tex_bot_ox; // J
	uint8_t tex_bot_oy; // K
	int16_t height_split; // L M
	// MARK_SCROLL
	wall_scroll_t scroll_top;
	wall_scroll_t scroll_bot;
} __attribute__((packed)) wall_split_t;

typedef struct
{
	vertex_t vtx; // A
	vertex_t dist; // B
	uint16_t angle; // C
	uint16_t special; // D
	uint8_t tflags; // E
	uint8_t texture_top; // F
	uint8_t tex_top_ox; // G
	uint8_t tex_top_oy; // H
	uint8_t texture_bot; // I
	uint8_t tex_bot_ox; // J
	uint8_t tex_bot_oy; // K
	uint8_t backsector; // L
	uint8_t blocking; // M
	uint8_t texture_mid;
	uint8_t tex_mid_ox;
	uint8_t tex_mid_oy;
	uint8_t blockmid;
	// MARK_SCROLL
	wall_scroll_t scroll_top;
	wall_scroll_t scroll_bot;
	wall_scroll_t scroll_mid;
} __attribute__((packed)) wall_masked_t;

typedef union
{
	wall_end_t end;
	wall_solid_t solid;
	wall_portal_t portal;
	wall_split_t split;
	wall_masked_t masked;
} __attribute__((packed)) wall_combo_t;

typedef struct
{
	int16_t height;
	uint8_t texture;
	uint8_t ox, oy;
	uint8_t angle;
	uint8_t link;
} __attribute__((packed)) sector_plane_t;

typedef struct
{
	// limit is 32 bytes
	sector_plane_t floor;
	sector_plane_t ceiling;
	uint16_t walls;
	uint16_t wall_last;
	uint8_t flags; // light, palette, underwater
	int8_t floordist;
	uint8_t floormasked;
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
	uint8_t count_sectors;
	uint8_t count_starts_normal;
	uint8_t count_starts_coop;
	uint8_t count_starts_dm;
	uint8_t count_things;
	//
	uint8_t unused[7];
	//
	uint16_t size_data;
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

//

uint_fast8_t x16e_enable_logo;
uint8_t x16e_logo_data[160 * 120];

static uint8_t temp_data[160 * 120];

//

static uint32_t marked_lights;

static uint32_t count_ptex;
static marked_texture_t marked_planes[MAX_TEXTURES];

static uint32_t count_wtex;
static marked_texture_t marked_walls[MAX_TEXTURES];

static uint32_t count_textures;
static marked_variant_t marked_variant[MAX_TEXTURES];

static uint32_t map_bank[MAX_BANKS];
static uint8_t map_data[MAX_BANKS * BANK_SIZE];

static sector_t map_sectors[255];

static kge_thing_t *player_starts[3][MAX_PLAYER_STARTS];

//

static map_head_t map_head =
{
	.magic = MAP_MAGIC,
	.version = MAP_VERSION,
};

//

static const uint32_t wall_size_tab[] =
{
	// without texture scrolling
	sizeof(wall_solid_t) - sizeof(wall_scroll_t) * 1,
	sizeof(wall_split_t) - sizeof(wall_scroll_t) * 2,
	sizeof(wall_portal_t) - sizeof(wall_scroll_t) * 2,
	sizeof(wall_masked_t) - sizeof(wall_scroll_t) * 3,
	// with texture scrolling
	sizeof(wall_solid_t),
	sizeof(wall_split_t),
	sizeof(wall_portal_t),
	sizeof(wall_masked_t),
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

static uint8_t convert_pitch(int16_t pitch)
{
	if((pitch & 0x80))
	{
		if(pitch < 0xCA)
			pitch = 0xCA;
	} else
	if(pitch > 0x36)
		pitch = 0x36;

	return pitch;
}

static void write_player_starts(int32_t fd, kge_thing_t **tha, uint32_t count)
{
	player_start_t ps;

	for(uint32_t i = 0; i < count; i++)
	{
		kge_thing_t *th = tha[i];

		ps.x = th->pos.x;
		ps.y = th->pos.y;
		ps.z = th->pos.z;
		ps.sector = list_get_idx(&edit_list_sector, (link_entry_t*)th->pos.sector - 1) + 1;
		ps.angle = 0x100 - (th->pos.angle >> 8);
		ps.pitch = convert_pitch(th->pos.pitch >> 8) ^ 0x80;
		ps.flags = 0;

		write(fd, &ps, sizeof(ps));
	}
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
	uint32_t size;
	int32_t ret;
	int32_t fd;
	uint32_t count_walls = 0;
	uint32_t count_things = 0;
	uint32_t count_starts_normal = 0;
	uint32_t count_starts_coop = 0;
	uint32_t count_starts_dm = 0;
	uint32_t masked_height;
	uint8_t wall_data[EDIT_MAX_SECTOR_LINES * sizeof(wall_portal_t)];

	if(edit_check_map())
		return;

	marked_lights = 1;

	count_textures = 0;
	count_ptex = 0;
	count_wtex = 0;

	memset(marked_planes, 0, sizeof(marked_planes));
	memset(marked_walls, 0, sizeof(marked_walls));

	memset(map_data, 0, sizeof(map_data));
	memset(map_sectors, 0, sizeof(map_sectors));

	for(uint32_t i = 0; i < MAX_BANKS; i++)
		map_bank[i] = 0;

	// TODO: count valid things; at least 1 must exist

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
								if(count_starts_normal >= MAX_PLAYER_STARTS)
								{
									error_generic("Too many normal player starts!");
									return;
								}
								player_starts[0][count_starts_normal] = th;
								count_starts_normal++;
							break;
							case THING_TYPE_START_COOP:
								if(count_starts_coop >= MAX_PLAYER_STARTS)
								{
									error_generic("Too many coop player starts!");
									return;
								}
								player_starts[1][count_starts_coop] = th;
								count_starts_coop++;
							break;
							case THING_TYPE_START_DM:
								if(count_starts_dm >= MAX_PLAYER_STARTS)
								{
									error_generic("Too many deathmatch player starts!");
									return;
								}
								player_starts[2][count_starts_dm] = th;
								count_starts_dm++;
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
		error_generic("At least one thing must be placed!");

	// go trough sectors
	map_sector = map_sectors;
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);
		uint32_t wall_size, wall_offs, wall_stop;
		void *wall_ptr = wall_data;
		uint32_t lidx;

		masked_height = 0;

		memset(wall_data, 0, sizeof(wall_data));

		// find first line
		for(lidx = 0; lidx < sec->line_count; lidx++)
		{
			uint32_t ii = sec->line_count - lidx - 1; // reverse order
			kge_line_t *line = sec->line + ii;
			kge_line_t *lprv = line + 1;

			if(lprv >= sec->line + sec->line_count)
				lprv = sec->line;

			if(!line->backsector)
				break;

			if(line->backsector != lprv->backsector)
				break;
		}		

		// go trough lines
		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = &sec->line[sec->line_count - lidx - 1]; // reverse order
			wall_combo_t *wall = wall_ptr;
			kge_vertex_t *v0, *v1;
			float dist, xx, yy, dx, dy;

			lidx++;
			if(lidx >= sec->line_count)
				lidx = 0;

			count_walls++;

			v0 = line->vertex[1];
			v1 = line->vertex[0];

			wall->solid.vtx.x = v0->x;
			wall->solid.vtx.y = v0->y;

			if(line->backsector)
			{
				ret = mark_texture(line->texture[1].idx, sec->light.idx);
				if(ret < 0)
				{
					error_texture_count();
					return;
				}

				wall->portal.backsector = list_get_idx(&edit_list_sector, (link_entry_t*)line->backsector - 1) + 1;
				wall->portal.blocking = line->info.blocking;
				wall->portal.texture_bot = ret;
				wall->portal.tex_bot_ox = line->texture[1].ox;
				wall->portal.tex_bot_oy = line->texture[1].oy;
				wall->portal.tflags = line->texture[1].flags << 4;
				wall->portal.angle = MARK_PORTAL;

				if(line->texture[2].idx)
				{
					ret = mark_texture(line->texture[2].idx, sec->light.idx);
					if(ret < 0)
					{
						error_texture_count();
						return;
					}

					if(line->texture[2].flags & TEXFLAG_MIRROR_X)
						wall->masked.tflags |= 0b00001000;

					wall->masked.blockmid = line->info.blockmid & 0b01111111;
					if(line->texture[2].flags & TEXFLAG_PEG_MID_BACK)
						wall->masked.blockmid |= 0b10000000;
					else
					{
						editor_texture_t *et = editor_texture + line->texture[2].idx;
						masked_height = et->height + line->texture[2].oy;
					}

					wall->masked.angle = MARK_MASKED;
					wall->masked.texture_mid = ret;
					wall->masked.tex_mid_ox = line->texture[2].ox;
					wall->masked.tex_mid_oy = line->texture[2].oy;
				}
			} else
			if(line->texture_split != INFINITY)
			{
				ret = mark_texture(line->texture[1].idx, sec->light.idx);
				if(ret < 0)
				{
					error_texture_count();
					return;
				}

				wall->split.height_split = line->texture_split;
				wall->split.texture_bot = ret;
				wall->split.tex_bot_ox = line->texture[1].ox;
				wall->split.tex_bot_oy = line->texture[1].oy;
				wall->split.tflags = line->texture[1].flags << 4;
				wall->split.angle = MARK_SPLIT;
			} else
			{
				wall->solid.tflags = 0;
				wall->solid.angle = 0;

				if(line->texture[2].idx)
				{
					ret = mark_texture(line->texture[2].idx, sec->light.idx);
					if(ret < 0)
					{
						error_texture_count();
						return;
					}

					if(line->texture[2].flags & TEXFLAG_MIRROR_X)
						wall->masked.tflags |= 0b00001000;

					wall->portal.backsector = 0;
					wall->masked.blockmid = 0;
					wall->masked.angle = MARK_MASKED;
					wall->masked.texture_bot = 0;
					wall->masked.texture_mid = ret;
					wall->masked.tex_mid_ox = line->texture[2].ox;
					wall->masked.tex_mid_oy = line->texture[2].oy;
				}
			}

			ret = mark_texture(line->texture[0].idx, sec->light.idx);
			if(ret < 0)
			{
				error_texture_count();
				return;
			}

			wall->solid.texture = ret;
			wall->solid.tex_ox = line->texture[0].ox;
			wall->solid.tex_oy = line->texture[0].oy;
			wall->solid.tflags |= line->texture[0].flags;

			if(line->info.flags & WALLFLAG_PEG_X)
				wall->solid.tflags |= 0b10000000;

			if(i == sec->line_count-1)
				wall->solid.angle |= MARK_LAST;

			wall->solid.special = 0;

			xx = v0->x - v1->x;
			yy = v0->y - v1->y;

			dist = sqrtf(xx * xx + yy * yy) / 256.0f;

			dx = xx / dist;
			dy = yy / dist;

			wall->solid.dist.x = dx;
			wall->solid.dist.y = dy;

			wall->solid.angle |= line->stuff.x16angle;

			wall_ptr += wall_size_tab[(wall->solid.angle & MARK_TYPE_BITS) >> 12];
		}

		// wall terminator (first vertex)
		{
			wall_portal_t *wall = (wall_portal_t*)wall_data;
			wall_end_t *wend = wall_ptr;
			wend->vtx = wall->vtx;
			wall_ptr += sizeof(wall_end_t);
		}

		// get wall space
		wall_size = (uint8_t*)wall_ptr - wall_data;
		wall_ptr = NULL;

		// get bank with free space
		for(uint32_t bank = 0; bank < MAX_BANKS; bank++)
		{
			wall_offs = map_bank[bank];
			if(BANK_SIZE - wall_offs >= wall_size)
			{
				map_bank[bank] += wall_size;
				wall_stop = wall_offs + wall_size - sizeof(wall_end_t);
				wall_offs += bank * BANK_SIZE;
				wall_ptr = (void*)map_data + wall_offs;
				break;
			}
		}

		if(!wall_ptr)
		{
			error_generic("Out of space for walls!");
			return;
		}

		memcpy(wall_ptr, wall_data, wall_size);

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

		map_sector->walls = wall_offs;
		map_sector->wall_last = wall_stop;

		map_sector->floordist = sec->plane[PLANE_BOT].dist;
		map_sector->floormasked = masked_height * 2;

		// flags will be added later

		map_sector++;

		ent = ent->next;
	}

	// get data size
	for(uint32_t bank = 0; bank < MAX_BANKS; bank++)
	{
		if(!map_bank[bank])
			break;
		size = bank * BANK_SIZE + map_bank[bank];
	}

	// count lights and make remap table
	map_head.count_lights = 0;
	for(uint32_t i = 0; i < x16_num_lights; i++)
	{
		if(!(marked_lights & (1 << i)))
			continue;

		if(map_head.count_lights + 1 >= MAX_LIGHTS)
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
	map_head.count_sectors = edit_list_sector.count;
	map_head.count_starts_normal = count_starts_normal;
	map_head.count_starts_coop = count_starts_coop;
	map_head.count_starts_dm = count_starts_dm;
	map_head.count_things = count_things;
	map_head.hash_sky = edit_sky_num < 0 ? 0 : editor_sky[edit_sky_num].hash;
	map_head.flags = 0;
	map_head.size_data = size;

	if(x16e_enable_logo)
		map_head.flags |= 0x80;

	// save file
	fd = open("data/export/DEFAULT.MAP", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(fd < 0)
	{
		error_generic("Unable to create map file!");
		return;
	}

	// write map header
	write(fd, &map_head, sizeof(map_head));

	// write map logo
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

	// write light list
	for(uint32_t i = 0; i < x16_num_lights; i++)
	{
		if(!(marked_lights & (1 << i)))
			continue;
		write(fd, &editor_light[i].hash, sizeof(uint32_t));
	}

	// write plane list
	write(fd, marked_planes, count_ptex * sizeof(marked_texture_t));

	// write wall list
	write(fd, marked_walls, count_wtex * sizeof(marked_texture_t));

	// write texture list
	write(fd, marked_variant, count_textures * sizeof(marked_variant_t));

	// write sectors
	write(fd, map_sectors, edit_list_sector.count * sizeof(sector_t));

	// write map data
	write(fd, map_data, size);

	// write player starts
	write_player_starts(fd, player_starts[0], count_starts_normal);
	write_player_starts(fd, player_starts[1], count_starts_coop);
	write_player_starts(fd, player_starts[2], count_starts_dm);
	write_player_starts(fd, &edit_camera_thing, 1);

	// write things
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
				mt.angle = 0x80 - (th->pos.angle >> 8);
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
printf("EXPORTED OK\n");
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

