#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "defs.h"
#include "tick.h"
#include "things.h"
#include "actions.h"

//

typedef struct
{
	uint8_t sector;
	uint8_t islink;
	uint8_t touch;
} portal_t;

typedef struct
{
	int16_t floorz, ceilingz, waterz;
	int16_t tfz, tcz;
	int16_t th_zh;
	uint8_t portal_rd, portal_wr;
	uint8_t floors, ceilings;
	uint8_t thing, iflags;
	uint8_t blockedby;
	uint8_t radius, height;
	uint8_t step_height, water_height;
	uint8_t sector, slot;
	uint8_t maskblock;
} pos_check_t;

//

static uint8_t thing_data[8192];
thing_type_t thing_type[MAX_X16_THING_TYPES];
thing_anim_t thing_anim[MAX_X16_THING_TYPES][NUM_THING_ANIMS];
uint32_t thing_hash[MAX_X16_THING_TYPES];
uint32_t sprite_hash[128];
uint8_t sprite_remap[128];
thing_state_t thing_state[MAX_X16_STATES];

uint32_t num_sprlnk_thg;
uint32_t logo_spr_idx;

// thing ZERO is not valid
uint8_t thingsec[128][16];
uint8_t thingces[128][16];

// command
ticcmd_t ticcmd;

// player
uint8_t player_thing;

// camera
uint8_t camera_thing;
uint8_t camera_damage;

// movement
static portal_t portals[256];

// position check stuff
pos_check_t poscheck;

//
//

static void place_to_sector(uint8_t tdx, uint8_t sdx)
{
	uint32_t slot;
	uint8_t maskblock = map_secext[sdx].maskblock;

	// get free sector slot
	for(slot = 0; slot < 31; slot++)
	{
		if(!sectorth[sdx][slot])
			break;
	}

	if(slot >= 31)
	{
		printf("P2S: no free slots; sec %u\n", sdx);
		return;
	}

//	printf("P2S tdx %u sdx %u slot %u\n", tdx, sdx, slot);

	sectorth[sdx][31]++;
	sectorth[sdx][slot] = tdx;
	thingsec[tdx][poscheck.slot] = sdx;
	thingces[tdx][poscheck.slot] = slot | maskblock;
	poscheck.slot++;
}

static void remove_from_sectors()
{
	for(uint32_t slot = 0; slot < 16; slot++)
	{
		uint32_t sdx = thingsec[poscheck.thing][slot];

		if(!sdx)
			break;

//		printf("RFS tdx %u sdx %u slot %u\n", poscheck.thing, sdx, thingces[poscheck.thing][slot]);
		sectorth[sdx][thingces[poscheck.thing][slot] & 31] = 0;
		sectorth[sdx][31]--;
		thingsec[poscheck.thing][slot] = 0;
	}
}

static void swap_thing_sector(uint8_t tdx, uint8_t sdx)
{
	for(uint32_t j = 1; j < 16; j++)
	{
		if(!thingsec[tdx][j])
			break;
		if(thingsec[tdx][j] == sdx)
		{
			thingsec[tdx][j] = thingsec[tdx][0];
			thingsec[tdx][0] = sdx;
			sdx = thingces[tdx][j];
			thingces[tdx][j] = thingces[tdx][0];
			thingces[tdx][0] = sdx;
			break;
		}
	}
}

static int32_t mlimit(int32_t val)
{
	if(val > 32767)
		return 32767;
	if(val < -32768)
		return -32768;
	return val;
}

static void prepare_pos_check(uint8_t tdx, int32_t z)
{
	thing_t *th = thing_ptr(tdx);

	poscheck.thing = tdx;

	poscheck.radius = th->radius;
	poscheck.height = th->height;

	poscheck.blockedby = th->blockedby;

	poscheck.step_height = thing_type[th->ticker.type].step_height;
	poscheck.water_height = thing_type[th->ticker.type].water_height;

	poscheck.th_zh = z + poscheck.height;

	poscheck.floorz = -16384;
	poscheck.ceilingz = 16384;
	poscheck.waterz = -16384;

	poscheck.floors = 0;
	poscheck.ceilings = 0;

	poscheck.iflags = 0;
}

static void get_sector_floorz(sector_t *sec)
{
	poscheck.tfz = sec->floor.height;
}

static void get_sector_ceilingz(sector_t *sec)
{
	poscheck.tcz = sec->ceiling.height;
}

static uint32_t check_backsector(uint8_t sec, int32_t th_z)
{
	sector_t *bs;
	int16_t dist;

	if(sectorth[sec][31] >= 31)
		return 1;

	bs = map_sectors + sec;

	map_secext[sec].maskblock = poscheck.maskblock;

	get_sector_floorz(bs);
	get_sector_ceilingz(bs);

	dist = poscheck.tcz - poscheck.tfz;
	dist -= poscheck.height;
	if(dist < 0)
		return 1;

	dist = th_z - poscheck.tfz;
	dist += poscheck.step_height;
	if(dist < 0)
		return 1;

	dist = poscheck.tcz - poscheck.th_zh;
	if(dist < 0)
		return 1;

	return 0;
}

static void add_sector(uint8_t sdx, uint8_t islink, uint8_t touch)
{
	uint32_t i;

	for(i = 0; i < poscheck.portal_wr; i++)
	{
		if(portals[i].sector == sdx)
		{
			portals[i].touch |= touch;
			break;
		}
	}

	if(i == poscheck.portal_wr)
	{
		portals[poscheck.portal_wr].sector = sdx;
		portals[poscheck.portal_wr].islink = islink;
		portals[poscheck.portal_wr].touch = touch;
		poscheck.portal_wr++;
	}

}

//
// position

void thing_apply_pos()
{
	thing_t *th = thing_ptr(poscheck.thing);

	// remove from all sectors
	remove_from_sectors();

	// place to first sector
	poscheck.slot = 0;
	place_to_sector(poscheck.thing, poscheck.sector);

	// place to detected sectors
	for(uint32_t i = 0; i < poscheck.portal_wr; i++)
	{
		uint8_t sec = portals[i].sector;
		if(portals[i].touch & 0x80 && sec != poscheck.sector)
			place_to_sector(poscheck.thing, sec);
	}

	// update iflags
	th->iflags &= ~(THING_IFLAG_HEIGHTCHECK | THING_IFLAG_NOJUMP);
	th->iflags |= poscheck.iflags;

	// save heights
	th->floorz = poscheck.floorz;
	th->ceilingz = poscheck.ceilingz - poscheck.height;

	// save sectors
	th->floors = poscheck.floors;
	th->ceilings = poscheck.ceilings;
}

uint32_t thing_check_pos(uint8_t tdx, int16_t nx, int16_t ny, int16_t nz, uint8_t sdx)
{
	thing_t *th = thing_ptr(tdx);
	int32_t zz = th->z >> 8;
	sector_t *sec;

	// prepare
	prepare_pos_check(tdx, nz);

	// target sector
	if(!sdx)
		sdx = thingsec[tdx][0];

	// special step height
	if(poscheck.step_height & 0x80)
	{
		poscheck.step_height &= 0x7F;
		sec = map_sectors + sdx;
		if((th->z >> 8) > th->floorz)
		{
			if(	sec->floor.link &&
				map_sectors[sec->floor.link].flags & SECTOR_FLAG_WATER &&
				nz < sec->floor.height
			)
				poscheck.step_height <<= 1;
			else
				poscheck.step_height >>= 1;
		}
	}

	poscheck.sector = 0;
	poscheck.maskblock = 0;

	poscheck.portal_rd = 0;
	poscheck.portal_wr = 1;

	portals[0].sector = sdx;
	portals[0].islink = 0;

	while(poscheck.portal_rd < poscheck.portal_wr)
	{
		sdx = portals[poscheck.portal_rd].sector;
		poscheck.portal_rd++;

		sec = map_sectors + sdx;

		get_sector_floorz(sec);
		if(poscheck.floorz < poscheck.tfz)
		{
			poscheck.floorz = poscheck.tfz;
			poscheck.floors = sdx;
		}

		get_sector_ceilingz(sec);
		if(poscheck.tcz < poscheck.ceilingz)
		{
			poscheck.ceilingz = poscheck.tcz;
			poscheck.ceilings = sdx;
		}

		//if(!poscheck.islink)
		{
			wall_t *wall = map_walls[sec->wall.bank] + sec->wall.first;
			wall_t *walf = wall;
			uint8_t inside = 1;

			do
			{
				wall_t *waln = map_walls[sec->wall.bank] + wall->next;
				uint8_t touch = 0xFF;
				vertex_t *vtx;
				uint8_t cross;
				int32_t dist;
				vertex_t dd;

				// flip check
				if(wall->angle & WALL_MARK_SWAP)
				{
					// V1 diff
					vtx = &waln->vtx;
					dd.x = vtx->x - nx;
					dd.y = vtx->y - ny;
				} else
				{
					// V0 diff
					vtx = &wall->vtx;
					dd.x = vtx->x - nx;
					dd.y = vtx->y - ny;
				}

				// get distance
				dist = (dd.x * wall->dist.y - dd.y * wall->dist.x) >> 8;

				if(dist < 0)
					inside = 0;

				if(dist >= poscheck.radius)
					goto do_next;

				if(dist + poscheck.radius < 0)
					touch = 0;

				// V0 diff
				vtx = &wall->vtx;
				dd.x = vtx->x - nx;
				dd.y = vtx->y - ny;

				// get left side
				dist = (dd.x * wall->dist.x + dd.y * wall->dist.y) >> 8;
				if(dist < 0)
				{
					// TODO: point check
					goto do_next;
				}

				// V1 diff
				vtx = &waln->vtx;
				dd.x = vtx->x - nx;
				dd.y = vtx->y - ny;

				// get right side
				dist = (dd.x * wall->dist.x + dd.y * wall->dist.y) >> 8;
				if(dist >= 0)
				{
					// TODO: point check
					goto do_next;
				}

				// check backsector
				if(	wall->backsector &&
					!(wall->blocking & poscheck.blockedby & 0x7F) &&
					!check_backsector(wall->backsector, zz)
				){
					add_sector(wall->backsector, 0, touch);
					goto do_next;
				}

				return 0;

do_next:
				wall = waln;
			} while(wall != walf);

			if(inside && !poscheck.sector)
				poscheck.sector = sdx;
		}
	}

	return poscheck.sector;
}

//
//

int32_t thing_find_type(uint32_t hash)
{
	for(uint32_t i = 0; i < 128; i++)
		if(thing_hash[i] == hash)
			return i;
	return -1;
}

void thing_clear()
{
	thing_t *th = (thing_t*)&ticker;
	uint32_t state;
	thing_state_t *st;

	// player weapon // TODO

	th->ticker.type = THING_WEAPON_FIRST;
	th->height = 64;

	state = thing_anim[th->ticker.type][ANIM_RAISE].state;

	st = thing_state + (state & (MAX_X16_STATES-1));

	th->next_state = state;
	th->ticks = 1;
	th->iflags = 0;

	if(st->sprite & 0x80)
		th->sprite = 0xFF; // 'NONE'
	else
		th->sprite = sprite_remap[st->sprite] + (st->frm_nxt & 0x1F);

}

uint8_t thing_spawn(int32_t x, int32_t y, int32_t z, uint8_t sector, uint8_t type, uint8_t origin)
{
	thing_t *th;
	uint32_t tdx;
	uint32_t state;
	thing_state_t *st;
	thing_type_t *ti;

	if(sectorth[sector][31] >= 30)
	{
		printf("NOPE thing spawn\n");
		return 0;
	}

	tdx = tick_add();
	if(!tdx)
		// no free slots
		return 0;

	th = thing_ptr(tdx);
	th->ticker.type = type;

	ti = thing_type + type;

	th->ticker.func = TFUNC_THING;

	th->eflags = ti->eflags;
	th->radius = ti->radius;
	th->height = ti->height;
	th->gravity = ti->gravity;
	th->scale = ti->scale;
	th->blocking = ti->blocking;
	th->blockedby = ti->blockedby;
	th->health = ti->health;

	th->x = x;
	th->y = y;
	th->z = z;

	th->angle = 0;
	th->pitch = 0x80;
	th->iflags = 0;
	th->origin = origin;
	th->target = 0;
	th->counter = 0;

	th->mx = 0;
	th->my = 0;
	th->mz = 0;

	state = thing_anim[type][ANIM_SPAWN].state;

	st = thing_state + (state & (MAX_X16_STATES-1));

	th->next_state = state;
	th->ticks = 1;

	if(st->sprite & 0x80)
		th->sprite = 0xFF; // 'NONE'
	else
		th->sprite = sprite_remap[st->sprite] + (st->frm_nxt & 0x1F);

	if(!thing_check_pos(tdx, x >> 8, y >> 8, z >> 8, sector))
	{
		sector_t *sec = map_sectors + sector;

		printf("BAD thing spawn\n");

		poscheck.slot = 0;
		map_secext[sector].maskblock = 0;
		place_to_sector(tdx, sector);

		th->ceilingz = sec->ceiling.height - th->height;
		th->floorz = sec->floor.height;
		th->ceilings = sector;
		th->floors = sector;
	} else
		thing_apply_pos();

	return tdx;
}

void thing_spawn_player()
{
	thing_t *th;

	player_thing = thing_spawn((int32_t)player_starts[0].x << 8, (int32_t)player_starts[0].y << 8, (int32_t)player_starts[0].z << 8, (int32_t)player_starts[0].sector, THING_TYPE_PLAYER_N, 0);
	camera_thing = player_thing;
	th = thing_ptr(player_thing);

	th->ticker.func = TFUNC_PLAYER;
	th->angle = player_starts[0].angle;
	ticcmd.angle = player_starts[0].angle;
	ticcmd.pitch = player_starts[0].pitch;

	projection.viewheight = thing_type[th->ticker.type].view_height;
	projection.wh = projection.viewheight;
	projection.wd = 0;
}

void thing_remove(uint8_t tdx)
{
	thing_t *th = thing_ptr(tdx);
	th->health = 0;
	th->blocking = 0;
	th->next_state = 0;
	th->ticks = 1;
}

void thing_launch(uint8_t tdx, uint8_t speed)
{
	thing_t *th = thing_ptr(tdx);
	uint8_t angle = th->angle;
	uint8_t pitch = th->pitch / 2 - 64;
	int32_t nm;

	if(speed > 127)
		speed = 127;

	if(pitch)
	{
		// NOTE: no 'mlimit'
		th->mz += tab_sin[pitch] * speed;
		speed = (tab_cos[pitch] * speed) >> 8;
	}

	th->mx = mlimit(th->mx + tab_sin[angle] * speed);
	th->my = mlimit(th->my + tab_cos[angle] * speed);
}

void thing_launch_ang(uint8_t tdx, uint8_t angle, uint8_t speed)
{
	thing_t *th = thing_ptr(tdx);
	if(speed > 127)
		speed = 127;
	th->mx = mlimit(th->mx + tab_sin[angle] * speed);
	th->my = mlimit(th->my + tab_cos[angle] * speed);
}

void thing_damage(uint8_t tdx, uint8_t odx, uint8_t angle, uint16_t damage)
{
	thing_t *th = thing_ptr(tdx);
	thing_type_t *info = thing_type + th->ticker.type;
	int32_t hp = 1;

	if(th->health)
	{
		hp = th->health - damage;
		if(hp <= 0)
		{
			th->health = 0;

			th->next_state = thing_anim[th->ticker.type][ANIM_DEATH].state;
			th->ticks = 1;

			th->blocking = info->death_bling;
			th->blockedby = info->death_blby;

			th->iflags |= THING_IFLAG_CORPSE;
		} else
			th->health = hp;
	}

	if(	hp > 0 &&
		!(th->iflags & (THING_IFLAG_GOTHIT|THING_IFLAG_CORPSE)) &&
		info->pain_chance &&
		info->pain_chance > (rng_get() & 0x7F)
	){
		th->next_state = thing_anim[th->ticker.type][ANIM_PAIN].state;
		th->ticks = 1;
	}

	th->iflags |= THING_IFLAG_GOTHIT;

	if(tdx == camera_thing)
	{
		if(damage + camera_damage >= 255 - 15)
			camera_damage = 255 - 15;
		else
			camera_damage += damage;
	}

	if(	odx &&
		info->imass
	){
		if(damage > 127)
			damage = 127;
		damage = (damage * info->imass) >> 5;
		thing_launch_ang(tdx, angle, damage);
	}
}

//
//

uint32_t thing_init(const char *file)
{
	int32_t fd;
	uint8_t state_data[MAX_X16_STATES * sizeof(thing_state_t)];

	fd = open(file, O_RDONLY);
	if(fd < 0)
		return 1;

	if(read(fd, thing_data, sizeof(thing_data)) != sizeof(thing_data))
	{
		close(fd);
		return 1;
	}

	if(read(fd, state_data, sizeof(state_data)) != sizeof(state_data))
	{
		close(fd);
		return 2;
	}

	close(fd);

	// load thing types
	for(uint32_t i = 0; i < 128; i++)
	{
		thing_type_t *type = thing_type + i;
		uint8_t *tdst = (uint8_t*)type;
		thing_anim_t *tanm = thing_anim[i];
		uint8_t *shsh = thing_data + i + 128 + 24 * 256; // 24 = offset for sprite names, stored in 'thing animation info' area; 0xB880
		uint8_t *thsh = thing_data + i + 128 + 28 * 256; // 28 = offset for thing hash, stored in 'thing animation info' area; 0xBC80
		uint8_t *tsrc = thing_data + i; // thing attributes; 0xA000
		uint8_t *tan0 = thing_data + i + 128; // thing animations LO; 0xA080
		uint8_t *tan1 = thing_data + i + 128 + NUM_THING_ANIMS * 256; // thing animations HI; 0xA880
		uint8_t *tan2 = thing_data + i + 128 + NUM_THING_ANIMS * 256 * 2; // thing animation sizes; 0xB080

		thing_hash[i] = thsh[0 * 256];
		thing_hash[i] |= thsh[1 * 256] << 8;
		thing_hash[i] |= thsh[2 * 256] << 16;
		thing_hash[i] |= thsh[3 * 256] << 24;

		sprite_hash[i] = shsh[0 * 256];
		sprite_hash[i] |= shsh[1 * 256] << 8;
		sprite_hash[i] |= shsh[2 * 256] << 16;
		sprite_hash[i] |= shsh[3 * 256] << 24;

		for(uint32_t j = 0; j < sizeof(thing_type_t); j++)
			tdst[j] = tsrc[j * 256];

		for(uint32_t j = 0; j < NUM_THING_ANIMS; j++)
		{
			tanm->state = tan0[j * 256] | (tan1[j * 256] << 8);
			tanm->count = tan2[j * 256];
			tanm++;
		}
	}

	// expand states
	for(uint32_t i = 0; i < MAX_X16_STATES / 256; i++)
		for(uint32_t x = 0; x < sizeof(thing_state_t); x++)
			for(uint32_t y = 0; y < 256; y++)
				thing_state[i * 256 + y].raw[x] = state_data[y + x * 256 + i * 2048];

	// extra info
	num_sprlnk_thg = thing_state->arg[0];
	logo_spr_idx = thing_state->arg[1];

	return 0;
}

//
//

void thing_tick()
{
	thing_t *th = thing_ptr(tick_idx);
	sector_t *sec;
	int32_t zz;

	// camera stuff
	if(tick_idx == camera_thing)
	{
		projection.wh += projection.wd;
		if(projection.wh > projection.viewheight)
		{
			projection.wh = projection.viewheight;
			projection.wd = 0;
		}
	}

	// XY movement
	if(th->mx || th->my)
	{
		int32_t nx, ny;

		nx = th->x + th->mx;
		ny = th->y + th->my;

		if(thing_check_pos(tick_idx, nx >> 8, ny >> 8, th->z >> 8, 0))
		{
			th->x = nx;
			th->y = ny;
			thing_apply_pos();
		}

		// friction
		if(!(th->eflags & THING_EFLAG_PROJECTILE))
		{
			switch(th->mx & 0xFF00)
			{
				case 0x0000:
				case 0xFF00:
					th->mx = 0;
				break;
				default:
					th->mx /= 2;
				break;
			}

			switch(th->my & 0xFF00)
			{
				case 0x0000:
				case 0xFF00:
					th->my = 0;
				break;
				default:
					th->my /= 2;
				break;
			}
		}
	}

	// Z movement
	th->z += th->mz;
	zz = th->z >> 8;

	// ceiling check
	if(zz > th->ceilingz)
	{
		th->z = th->ceilingz << 8;
		th->mz = 0;
	}

	// floor check
	if(zz < th->floorz)
	{
		th->z = th->floorz << 8;
		th->mz = 0;
		zz = th->floorz;
	}

	// gravity
	sec = map_sectors + thingsec[tick_idx][0];

	if(sec->flags & SECTOR_FLAG_WATER)
	{
		// under water
		if(th->eflags & THING_EFLAG_WATERSPEC)
		{
			if(thing_type[th->ticker.type].view_height > thing_type[th->ticker.type].water_height)
				// floats
				th->mz = th->gravity << 4;
			else
				// sinks
				th->mz = -th->gravity << 4;
			goto skip_z_friction;
		}
	} else
	if(th->gravity)
	{
		if(zz > th->floorz)
			th->mz -= th->gravity << 4;
		goto skip_z_friction;
	}

	if(th->eflags & THING_EFLAG_PROJECTILE)
		goto skip_z_friction;

	// friction
	switch(th->mz & 0xFF00)
	{
		case 0x0000:
		case 0xFF00:
			th->mz = 0;
		break;
		default:
			th->mz /= 2;
		break;
	}

skip_z_friction:
	return;
}

void thing_tick_plr()
{
	thing_t *th = thing_ptr(tick_idx);

	if(th->counter)
	{
		// frozen
		ticcmd.angle = th->angle;
		ticcmd.pitch = th->pitch;
	} else
	{
		th->angle = ticcmd.angle;
		th->pitch = ticcmd.pitch;

		if(ticcmd.bits_l & TCMD_USE)
		{
			if(!(th->iflags & THING_IFLAG_USED))
			{
				th->iflags |= THING_IFLAG_USED;
				printf("USE KEY\n");
			}
		} else
			th->iflags &= ~THING_IFLAG_USED;

		if(ticcmd.bits_l & TCMD_MOVING)
		{
			uint8_t speed = thing_type[th->ticker.type].speed;
			uint8_t angle = ticcmd.angle + (ticcmd.bits_h & 0xE0);
			thing_launch_ang(tick_idx, angle, speed);
		}
	}

	thing_tick();
}

