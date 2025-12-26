#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "defs.h"
#include "tick.h"
#include "things.h"
#include "actions.h"
#include "hitscan.h"

//#define DBG_NO_CAMERA_DIP

//

typedef struct
{
	uint8_t sector;
	uint8_t islink;
	uint8_t touch;
	uint8_t midhit;
} portal_t;

typedef struct
{
	int16_t floorz, ceilingz;
	int16_t tfz, tcz;
	int16_t th_zh, th_sh;
	uint8_t portal_rd, portal_wr;
	uint8_t floors, floort;
	uint8_t ceilings, ceilingt;
	uint8_t thing;
	uint8_t water_height;
	uint8_t blockedby;
	uint8_t noradius;
	uint8_t radius, height;
	uint8_t sector, slot;
	uint8_t islink;
	uint8_t pthit, ptwall;
	uint8_t htype, hidx;
	uint8_t hitang;
	uint8_t midhit, midsec;
} pos_check_t;

//

static uint8_t thing_data[8192];
thing_type_t thing_type[MAX_X16_THING_TYPES];
thing_anim_t thing_anim[MAX_X16_THING_TYPES][NUM_THING_ANIMS];
uint32_t thing_hash[MAX_X16_THING_TYPES];
uint32_t sprite_hash[128];
uint8_t sprite_remap[128];
thing_state_t thing_state[MAX_X16_STATES];

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

static void place_to_sector(uint8_t tdx, uint8_t sdx, uint8_t mh)
{
	uint32_t slot;

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
	thingces[tdx][poscheck.slot] = slot | mh;
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
	uint8_t bkup = sdx;

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
			return;
		}
	}
	printf("SWAP FAIL! (%u -> %u) %u\n", thingsec[tdx][0], bkup, level_tick);
}

static int32_t mlimit(int32_t val)
{
	if(val > 32767)
		return 32767;
	if(val < -32768)
		return -32768;
	return val;
}

static void prepare_pos_check(uint8_t tdx, int32_t nz, int32_t fz, int32_t mz, uint32_t sflags)
{
	thing_t *th = thing_ptr(tdx);

	poscheck.thing = tdx;

	poscheck.radius = th->radius;
	poscheck.height = th->height;

	poscheck.blockedby = th->blockedby;
	poscheck.noradius = th->eflags & THING_EFLAG_NORADIUS;

	poscheck.th_sh = nz;
	if(	mz >= 0 &&
		fz - nz + thing_type[th->ticker.type].step_height >= 0
	)
		poscheck.th_sh += thing_type[th->ticker.type].step_height;

	if(	!(sflags & SECTOR_FLAG_WATER) &&
		th->mz / 256 > -32
	)
		poscheck.water_height = thing_type[th->ticker.type].water_height;
	else
		poscheck.water_height = 0xFF;

	poscheck.th_zh = nz + poscheck.height;

	poscheck.floorz = -16384;
	poscheck.ceilingz = 16384;

	poscheck.floors = 0;
	poscheck.ceilings = 0;
	poscheck.floort = 0;
	poscheck.ceilingt = 0;
}

static void get_sector_floorz(sector_t *sec, int32_t th_z)
{
	if(sec->floor.link)
	{
		if(	poscheck.water_height < 0xFF &&
			map_sectors[sec->floor.link].flags & SECTOR_FLAG_WATER
		)
			poscheck.tfz = sec->floor.height - poscheck.water_height;
		else
			poscheck.tfz = -16384;

		return;
	}

	poscheck.tfz = sec->floor.height;

	if(poscheck.midhit)
		poscheck.tfz += (int32_t)sec->midheight * 2;
}

static void get_sector_ceilingz(sector_t *sec)
{
	if(sec->ceiling.link)
	{
		poscheck.tcz = 16384;
		return;
	}

	poscheck.tcz = sec->ceiling.height;
}

static uint32_t check_line_block(sector_t *sec, wall_t *wall, int32_t th_z, uint32_t touch)
{
	sector_t *bs;
	int16_t dist;

	// thing limit
	if(sectorth[wall->backsector][31] >= 31)
		return 1;

	// do not care
	if(poscheck.noradius)
		return 0;

	// mid block
	if(	touch &&
		wall->blockmid & poscheck.blockedby &&
		!(wall->tflags & 0b10000000)
	){
		uint8_t bkup = poscheck.midhit;

		poscheck.midhit = 0x80;
		get_sector_floorz(sec, th_z);

		poscheck.midhit = bkup;

		dist = poscheck.ceilingz - poscheck.tfz;
		dist -= poscheck.height;
		if(dist < 0)
			return 1;

		dist = poscheck.th_sh - poscheck.tfz;
		if(dist < 0)
			return 1;

		poscheck.midsec = sec - map_sectors;
	}

	// backsector

	bs = map_sectors + wall->backsector;

	get_sector_floorz(bs, th_z);
	get_sector_ceilingz(bs);

	dist = poscheck.tcz - poscheck.tfz;
	dist -= poscheck.height;
	if(dist < 0)
		return 1;

	dist = poscheck.th_sh - poscheck.tfz;
	if(dist < 0)
		return 1;

	dist = poscheck.tcz - poscheck.th_zh;
	if(dist < 0)
		return 1;

	dist = poscheck.tcz - poscheck.floorz;
	if(dist < poscheck.height)
		return 1;

	dist = poscheck.ceilingz - poscheck.tfz;
	if(dist < poscheck.height)
		return 1;

	return 0;
}

static void check_planes(uint8_t sdx, int32_t nz)
{
	sector_t *sec = map_sectors + sdx;

	get_sector_ceilingz(sec);
	if(poscheck.tcz < poscheck.ceilingz)
	{
		poscheck.ceilingz = poscheck.tcz;
		poscheck.ceilings = sdx;
	}

	get_sector_floorz(sec, nz);
	if(poscheck.floorz < poscheck.tfz)
	{
		poscheck.floorz = poscheck.tfz;
		poscheck.floors = poscheck.midhit ? 0 : sdx;
	}
}

static void check_point(vertex_t *dd, uint32_t wdx)
{
	int32_t dist;

	p2a_coord.x = dd->x;
	p2a_coord.y = dd->y;
	dist = point_to_dist();

	if(dist >= poscheck.radius)
		return;

	poscheck.pthit = 2;
	poscheck.ptwall = wdx;
}

static uint32_t check_things(uint8_t tdx, uint8_t sdx, int32_t nx, int32_t ny)
{
	thing_t *th = thing_ptr(tdx);

	for(uint32_t i = 0; i < 31; i++)
	{
		uint8_t odx = sectorth[sdx][i];
		int32_t radius;
		int32_t dist;
		thing_t *ht;

		if(!odx)
			continue;

		if(odx == tdx)
			continue;

		if(th->origin == odx)
			continue;

		ht = thing_ptr(odx);

		if(!(poscheck.blockedby & ht->blocking))
			continue;

		radius = ht->radius + poscheck.radius;

		dist = nx - ht->x / 256;
		p2a_coord.x = dist;
		if(dist >= 0)
		{
			if(dist - radius >= 0)
				continue;
		} else
		{
			if(dist + radius < 0)
				continue;
		}

		dist = ny - ht->y / 256;
		p2a_coord.y = dist;
		if(dist >= 0)
		{
			if(dist - radius >= 0)
				continue;
		} else
		{
			if(dist + radius < 0)
				continue;
		}

		dist = point_to_dist();
		if(dist - radius >= 0)
			continue;

		dist = ht->z / 256;
		if(dist - poscheck.th_zh >= 0)
		{
			if(dist < poscheck.ceilingz)
			{
				poscheck.ceilingz = dist;
				poscheck.ceilingt = odx;
			}

set_iflags:
			th->iflags |= THING_IFLAG_HEIGHTCHECK;
			ht->iflags |= THING_IFLAG_HEIGHTCHECK;

			continue;
		}

		dist = ht->z / 256 + ht->height;
		if(poscheck.th_sh - dist >= 0)
		{
			if(poscheck.floorz < dist)
			{
				poscheck.floorz = dist;
				poscheck.floort = odx;
			}
			goto set_iflags;
		}

		return odx;
	}

	return 0;
}

static void add_sector_raw(uint8_t sdx, uint8_t touch, uint8_t islink)
{
	uint32_t i;

	for(i = 0; i < poscheck.portal_wr; i++)
	{
		if(portals[i].sector == sdx)
		{
			portals[i].touch |= touch;
			portals[i].midhit |= poscheck.midhit;
			break;
		}
	}

	if(i == poscheck.portal_wr)
	{
		portals[poscheck.portal_wr].sector = sdx;
		portals[poscheck.portal_wr].islink = islink;
		portals[poscheck.portal_wr].touch = touch;
		portals[poscheck.portal_wr].midhit = poscheck.midhit;
		poscheck.portal_wr++;
	}

}

static void add_sector_links(uint8_t sdx)
{
	sector_t *sec = map_sectors + sdx;

	if(sec->floor.link)
		add_sector_raw(sec->floor.link, 1, 1);

	if(sec->ceiling.link)
		add_sector_raw(sec->ceiling.link, 1, 1);
}

static void add_sector(uint8_t sdx, uint8_t touch)
{
	add_sector_raw(sdx, touch, 0);
	if(touch)
		add_sector_links(sdx);
}

//
// position

void thing_apply_heights(thing_t *th)
{
	// save heights
	th->floorz = poscheck.floorz;
	th->ceilingz = poscheck.ceilingz - poscheck.height;

	// save planes
	th->floors = poscheck.floors;
	th->floort = poscheck.floort;
	th->ceilings = poscheck.ceilings;
	th->ceilingt = poscheck.ceilingt;
}

void thing_apply_pos()
{
	thing_t *th = thing_ptr(poscheck.thing);

	// remove from all sectors
	remove_from_sectors();

	// place to first sector
	poscheck.slot = 0;
	place_to_sector(poscheck.thing, poscheck.sector, poscheck.midsec == poscheck.sector ? 0x80 : 0x00);

	// place to detected sectors
	for(uint32_t i = 0; i < poscheck.portal_wr; i++)
	{
		uint8_t sec = portals[i].sector;
		if(portals[i].touch && sec != poscheck.sector)
			place_to_sector(poscheck.thing, sec, portals[i].midhit);
	}

	// plane info
	thing_apply_heights(th);
}

void thing_check_heights(uint8_t tdx)
{
	thing_t *th = thing_ptr(tdx);
	int32_t nz = th->z / 256;
	uint8_t sdx = thingsec[tdx][0];

	prepare_pos_check(tick_idx, nz, th->floorz, th->mz, map_sectors[sdx].flags);

	// hack for things
	poscheck.th_sh = nz + 256;

	// go trough sectors
	for(uint32_t i = 0; i < 16; i++)
	{
		sdx = thingsec[tdx][i];

		if(!sdx)
			break;

		// floor and ceiling
		poscheck.midhit = thingces[tdx][i] & 0x80;
		check_planes(sdx, nz);

		// go trough things
		if(th->blockedby)
			check_things(tdx, sdx, th->x / 256, th->y / 256);

		// special
		if(poscheck.noradius)
			break;
	}
}

static uint32_t cb_hitfix(wall_t *wall)
{
	poscheck.sector = wall->backsector;
	return 1;
}

uint32_t thing_check_pos(uint8_t tdx, int16_t nx, int16_t ny, int16_t nz, uint8_t sdx)
{
	thing_t *th = thing_ptr(tdx);
	int32_t zz = th->z / 256;
	uint32_t hitfix = 0;
	sector_t *sec;

	// target sector
	if(!sdx)
		sdx = thingsec[tdx][0];

failsafe:
	// prepare
	prepare_pos_check(tdx, nz, th->floorz, th->mz, map_sectors[sdx].flags);

	poscheck.htype = 0;

	poscheck.sector = 0;
	poscheck.midsec = 0;

	poscheck.portal_rd = 0;
	poscheck.portal_wr = 1;

	portals[0].sector = sdx;
	portals[0].islink = 0;
	portals[0].midhit = 0;

	while(poscheck.portal_rd < poscheck.portal_wr)
	{
		sdx = portals[poscheck.portal_rd].sector;
		poscheck.islink = portals[poscheck.portal_rd].islink;
		poscheck.midhit = portals[poscheck.portal_rd].midhit;
		poscheck.portal_rd++;

		sec = map_sectors + sdx;

		if(!poscheck.noradius)
			check_planes(sdx, nz);

		if(!poscheck.islink)
		{
			wall_t *wall = map_walls[sec->wall.bank] + sec->wall.first;
			wall_t *walf = wall;
			uint8_t inside = 1;

			poscheck.pthit = 0;

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
					goto do_next;

				// V1 diff
				vtx = &waln->vtx;
				dd.x = vtx->x - nx;
				dd.y = vtx->y - ny;

				// midblock
				poscheck.midhit = touch & wall->blockmid & poscheck.blockedby ? 0x80 : 0x00;

				// get right side
				dist = (dd.x * wall->dist.x + dd.y * wall->dist.y) >> 8;
				if(dist >= 0)
				{
					if(	!poscheck.htype &&
						dist - poscheck.radius < 0
					)
						check_point(&dd, wall - map_walls[sec->wall.bank]);
					goto do_next;
				}

				if(	!poscheck.htype &&
					dist + poscheck.radius >= 0
				)
					check_point(&dd, wall - map_walls[sec->wall.bank]);

				// check backsector
				if(	wall->backsector &&
					!(wall->blocking & poscheck.blockedby & 0x7F) &&
					!check_line_block(sec, wall, zz, touch)
				){
					add_sector(wall->backsector, touch);
					goto do_next;
				}

				// render hack
				if(poscheck.noradius)
					goto do_next;

				// solid wall
				poscheck.htype = sec->wall.bank | 0x10;
				poscheck.hidx = wall - map_walls[sec->wall.bank];
				poscheck.hitang = wall->angle >> 4;

				return 0;

do_next:
				wall = waln;
			} while(wall != walf);

			// point check
			if(poscheck.pthit)
			{
				wall_t *waln;

				wall = map_walls[sec->wall.bank] + poscheck.ptwall;
				waln = map_walls[sec->wall.bank] + wall->next;

				p2a_coord.x = waln->vtx.x;
				p2a_coord.y = waln->vtx.y;

				while(poscheck.pthit)
				{
					// check blocking
					if(	!wall->backsector ||
						(wall->blocking & poscheck.blockedby & 0x7F)
					){
pt_block:
						if(poscheck.noradius)
							goto radpass;

						p2a_coord.x -= th->x / 256;
						p2a_coord.y -= th->y / 256;
						poscheck.hitang = point_to_angle() >> 4;
						poscheck.hitang += 0x40;
						poscheck.pthit = 0;
						poscheck.htype = sec->wall.bank | 0x10;
						poscheck.hidx = poscheck.ptwall;
					} else
					{
						// midblock
						poscheck.midhit = wall->blockmid & poscheck.blockedby ? 0x80 : 0x00;

						// check backsector
						if(check_line_block(sec, wall, zz, 1))
							goto pt_block;
						else
							add_sector(wall->backsector, 1);
radpass:
						poscheck.pthit--;
						poscheck.ptwall = wall->next;
						wall = map_walls[sec->wall.bank] + poscheck.ptwall;
					}
				}
			}

			if(inside && !poscheck.sector)
			{
				poscheck.sector = sdx;

				if(poscheck.midsec == sdx)
				{
					poscheck.midhit = 0x80;

					// re-check floor
					get_sector_floorz(sec, nz);
					if(poscheck.floorz < poscheck.tfz)
					{
						poscheck.floorz = poscheck.tfz;
						poscheck.floors = 0;
					}
				}

				add_sector_links(sdx);
			}
		}
	}

	if(poscheck.htype)
		return 0;

	poscheck.portal_rd = 0;
	while(poscheck.portal_rd < poscheck.portal_wr)
	{
		uint8_t odx;

		sdx = portals[poscheck.portal_rd].sector;
		poscheck.portal_rd++;

		odx = check_things(tdx, sdx, nx, ny);
		if(odx)
		{
			thing_t *ht = thing_ptr(odx);

			p2a_coord.x = ht->x / 256 - th->x / 256;
			p2a_coord.y = ht->y / 256 - th->y / 256;
			poscheck.hitang = point_to_angle() >> 4;
			poscheck.hitang += 0x40;
			poscheck.htype = 0xFF;
			poscheck.hidx = odx;

			return 0;
		}
	}

	if(	!poscheck.sector &&
		!hitfix &&
		(
			th->mx ||
			th->my
		)
	){
		p2a_coord.x = th->mx;
		p2a_coord.y = th->my;
		hitscan_func(tdx, point_to_angle() >> 4, cb_hitfix);

		if(poscheck.sector)
		{
			sdx = poscheck.sector;
			hitfix = 0xFF;
			goto failsafe;
		}
	}

	if(poscheck.noradius)
		check_planes(poscheck.sector, nz);

	if(poscheck.ceilingz - poscheck.floorz < poscheck.height)
	{
		poscheck.htype = 0;
		return 0;
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

	th->view_height = ti->view_height;

	th->angle = 0;
	th->pitch = 0x80;
	th->iflags = 0;
	th->origin = origin;
	th->target = 0;
	th->counter = 0;

	th->mx = 0;
	th->my = 0;
	th->mz = 0;

	th->floort = 0;
	th->ceilingt = 0;

	state = thing_anim[type][ANIM_SPAWN].state;

	st = thing_state + (state & (MAX_X16_STATES-1));

	th->next_state = state;
	th->ticks = 1;

	if(st->sprite & 0x80)
		th->sprite = 0xFF; // 'NONE'
	else
		th->sprite = sprite_remap[st->sprite] + (st->frm_nxt & 0x1F);

	th->floorz = -16384; // no step height
	if(!thing_check_pos(tdx, x / 256, y / 256, z / 256, sector))
	{
		sector_t *sec = map_sectors + sector;

		printf("BAD thing spawn\n");

		poscheck.slot = 0;

		place_to_sector(tdx, sector, 0);

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

	projection.wh = th->view_height;
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
// projectile

static void projectile_death(uint8_t tdx)
{
	thing_t *th = thing_ptr(tdx);
	uint8_t type = th->ticker.type;
	int32_t nx, ny, nz;
	uint8_t texture = 0;
	uint8_t thing = 0;

	hitscan.radius = th->radius;

	th->mx = 0;
	th->my = 0;
	th->mz = 0;

	th->blocking = 0;
	th->blockedby = 0;

	th->radius = thing_type[type].alt_radius;
	th->eflags &= ~THING_EFLAG_PROJECTILE;
	th->eflags |= THING_EFLAG_NORADIUS;

	th->iflags &= ~THING_IFLAG_HEIGHTCHECK;

	th->ticks = 1;
	th->next_state = thing_anim[type][ANIM_DEATH].state;

	nx = th->x;
	ny = th->y;
	nz = th->z;

	if(poscheck.htype & 0x80)
	{
		// hit a thing
		thing_t *ht = thing_ptr(poscheck.hidx);
		uint8_t angle = th->angle;
		int32_t dist;
		vertex_t d1;

		thing = poscheck.hidx;

		hitscan.x = th->x / 256;
		hitscan.y = th->y / 256;
		hitscan.z = th->z / 256;

		hitscan_angles(angle, th->pitch / 2);

		dist = hitscan_thing_dd(ht, &d1);
		dist += hitscan_thing_dt(th, &d1);
		dist -= hitscan.radius;

		d1.x = (tab_sin[angle] * dist) >> 8;
		d1.y = (tab_cos[angle] * dist) >> 8;
		dist = (dist * hitscan.wtan) >> 8;

		nx = (hitscan.x + d1.x) << 8;
		ny = (hitscan.y + d1.y) << 8;
		nz = (hitscan.z + dist) << 8;
	} else
	if(poscheck.htype & 0x10)
	{
		// hit a wall
		wall_t *wsrc = map_walls[poscheck.htype & 15] + poscheck.hidx;
		int32_t zz, ws, wc;
		wall_t wall;
		vertex_t d0;

		wall.angle = wsrc->angle;
		wall.dist = wsrc->dist;

		wc = (wall.dist.y * hitscan.radius) >> 8;
		ws = (wall.dist.x * hitscan.radius) >> 8;

		wall.vtx.x = wsrc->vtx.x - wc;
		wall.vtx.y = wsrc->vtx.y + ws;

		hitscan.x = th->x / 256;
		hitscan.y = th->y / 256;
		hitscan.z = th->z / 256;
		hitscan.angle = th->angle;

		hitscan_angles(hitscan.angle, th->pitch / 2);
		zz = hitscan_wall_pos(&wall, &d0, NULL);

		nx = d0.x << 8;
		ny = d0.y << 8;
		nz = zz << 8;

		///

		texture = wsrc->top.texture;

		if(wsrc->backsector)
		{
			sector_t *sec = map_sectors + wsrc->backsector;
			if(zz <= sec->floor.height)
				texture = wsrc->bot.texture;
			else
			if(zz <= sec->ceiling.height)
				texture = 0;
		} else
		if(	wsrc->angle & WALL_MARK_EXTENDED &&
			wsrc->split != -32768 &&
			zz <= wsrc->split
		)
			texture = wsrc->bot.texture;
	} else
	if(!poscheck.htype)
	{
		// hit nothing
		return;
	} else
	{
		// hit a plane
		if(poscheck.htype & 0x40)
		{
			thing = th->floort;
			if(	!thing &&
				th->floors
			)
				texture = map_sectors[th->floors].floor.texture;
		} else
		{
			thing = th->ceilingt;
			if(	!thing &&
				th->ceilings
			)
				texture = map_sectors[th->ceilings].ceiling.texture;
		}
	}

	if(texture == 0xFF)
	{
		th->next_state = 0;
		return;
	}

	if(thing_check_pos(tdx, nx / 256, ny / 256, nz / 256, 0))
	{
		th->x = nx;
		th->y = ny;
		th->z = nz;
		thing_apply_pos();
	}

	if(	thing &&
		th->health
	)
		thing_damage(thing, th->origin, th->angle, th->health);
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

	return 0;
}

//
//

void thing_frame()
{
	thing_t *th = thing_ptr(tick_idx);
	thing_state_t *st;
	uint32_t tmp;

	if(!th->ticks)
		return;

	th->ticks--;
	if(th->ticks)
		return;

repeat:
	tmp = th->next_state & (MAX_X16_STATES-1);
	if(!tmp)
	{
		poscheck.thing = tick_idx;
		remove_from_sectors();
		tick_del(tick_idx);
		return;
	}

	st = thing_state + tmp;

	tmp = st->action & 0x7F;
	if(tmp && action_func(tick_idx, tmp, st))
		goto repeat;

	th->next_state = st->next;
	th->next_state |= (st->frm_nxt & 0xE0) << 3;
	th->next_state |= (st->action & 0x80) << 8;

	th->ticks = st->ticks;

	if(st->sprite & 0x80)
		th->sprite = 0xFF; // 'NONE'
	else
		th->sprite = sprite_remap[st->sprite] + (st->frm_nxt & 0x1F);
}

void thing_tick()
{
	thing_t *th = thing_ptr(tick_idx);
	uint32_t repeat = 0;
	sector_t *sec;
	int32_t zz;

	//
	// camera stuff
#ifdef DBG_NO_CAMERA_DIP
	projection.wh = th->view_height;
	projection.wd = 0;
#else
	if(tick_idx == camera_thing)
	{
		projection.wh += projection.wd;
		if(projection.wh > th->view_height)
		{
			projection.wh = th->view_height;
			projection.wd = 0;
		}
	}
#endif

	// projectile repeat
	if(th->eflags & THING_EFLAG_PROJECTILE)
		repeat = thing_type[th->ticker.type].jump_pwr;

do_repeat:
	//
	// XY movement
	th->iflags &= ~(THING_IFLAG_BLOCKED | THING_IFLAG_GOTHIT);
	if(th->mx || th->my)
	{
		int32_t nx, ny;

		nx = th->x + th->mx;
		ny = th->y + th->my;

		if(thing_check_pos(tick_idx, nx / 256, ny / 256, th->z / 256, 0))
		{
apply_pos:
			th->x = nx;
			th->y = ny;
			thing_apply_pos();
		} else
		{
			th->iflags |= THING_IFLAG_BLOCKED;

			if(th->eflags & THING_EFLAG_PROJECTILE)
			{
				projectile_death(tick_idx);
				goto do_animation;
			} else
			if(!poscheck.htype)
			{
				th->mx = 0;
				th->my = 0;
			} else
			{
				vertex_t vect;
				int32_t dist;
				int8_t ang;

				p2a_coord.x = th->mx;
				p2a_coord.y = th->my;
				dist = point_to_dist();

				ang = 0x40 - (poscheck.hitang - p2a_coord.a);

				dist *= ang * 4;
				dist >>= 8;

				vect.x = (tab_sin[poscheck.hitang] * dist) >> 8;
				vect.y = (tab_cos[poscheck.hitang] * dist) >> 8;

				nx = th->x + vect.x;
				ny = th->y + vect.y;

				if(thing_check_pos(tick_idx, nx / 256, ny / 256, th->z / 256, 0))
				{
					th->mx = vect.x;
					th->my = vect.y;
					goto apply_pos;
				} else
				{
					uint8_t ang = level_tick << 5;

					th->mx = 0;
					th->my = 0;

					nx = th->x + tab_sin[ang];
					ny = th->y + tab_cos[ang];

					if(thing_check_pos(tick_idx, nx / 256, ny / 256, th->z / 256, 0))
						goto apply_pos;
				}
			}
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
	} else
	if(th->iflags & THING_IFLAG_HEIGHTCHECK)
	{
		th->iflags &= ~THING_IFLAG_HEIGHTCHECK;

		// new heights
		thing_check_heights(tick_idx);

		// plane info
		thing_apply_heights(th);
	}

	//
	// Z movement
	th->z += th->mz;
	zz = th->z / 256;

	// ceiling check
	if(zz > th->ceilingz)
	{
		th->z = th->ceilingz << 8;
		th->mz = 0;

		if(th->eflags & THING_EFLAG_PROJECTILE)
		{
			poscheck.htype = 0x01;
			projectile_death(tick_idx);
			goto do_animation;
		}
	}

	// floor check
	if(zz < th->floorz)
	{
#ifndef DBG_NO_CAMERA_DIP
		// camera stuff
		if(tick_idx == camera_thing)
		{
			int32_t diff;

			if(th->mz >= 0)
				diff = zz - th->floorz;
			else
				diff = th->mz >> 10;

			diff = projection.wh + diff;
			if(diff < 0)
				diff = 0;

			if((uint8_t)diff <= projection.wh)
			{
				projection.wh = diff;
				projection.wd = (th->view_height - projection.wh) >> 1;
				if(!projection.wd)
					projection.wd = 1;
			}
		}
#endif
		th->z = th->floorz << 8;
		th->mz = 0;
		zz = th->floorz;

		if(th->eflags & THING_EFLAG_PROJECTILE)
		{
			poscheck.htype = 0x41;
			projectile_death(tick_idx);
			goto do_animation;
		}
	}

	// sector
	sec = map_sectors + thingsec[tick_idx][0];

	// links
	if(sec->floor.link)
	{
		int32_t tmp = th->view_height;

		tmp += th->z / 256;
		if(tmp < sec->floor.height)
		{
			th->iflags |= THING_IFLAG_HEIGHTCHECK;
			swap_thing_sector(tick_idx, sec->floor.link);
			goto did_swap;
		}
	}
	if(sec->ceiling.link)
	{
		int32_t tmp = th->view_height;

		tmp += th->z / 256;
		if(tmp > sec->ceiling.height)
		{
			th->iflags |= THING_IFLAG_HEIGHTCHECK;
			swap_thing_sector(tick_idx, sec->ceiling.link);
		}
	}

did_swap:
	sec = map_sectors + thingsec[tick_idx][0];

	// water or gravity
	if(sec->flags & SECTOR_FLAG_WATER)
	{
		// under water
		if(th->eflags & THING_EFLAG_WATERSPEC)
		{
			if(thing_type[th->ticker.type].view_height < thing_type[th->ticker.type].water_height)
			{
				// sinks
				if(zz > th->floorz)
					th->mz -= th->gravity;
			} else
				// floats
				th->mz += th->gravity;

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
	// projectile repeat
	if(repeat)
	{
		repeat--;
		goto do_repeat;
	}

do_animation:

	//
	// animation
	thing_frame();
}

void thing_tick_plr()
{
	sector_t *sec = map_sectors + thingsec[tick_idx][0];
	thing_t *th = thing_ptr(tick_idx);
	uint8_t bkup = tick_idx;
	uint8_t ntype = THING_TYPE_PLAYER_N;

	if(!th->gravity)
		ntype = thing_state->plr_fly;

	if(sec->flags & SECTOR_FLAG_WATER)
		ntype = thing_state->plr_swim;

	if(th->counter)
	{
		// frozen
		ticcmd.angle = th->angle;
		ticcmd.pitch = th->pitch;
	} else
	{
		thing_type_t *ti = thing_type + th->ticker.type;
		int32_t jump = ti->jump_pwr;
		int32_t speed = ti->speed;

		th->angle = ticcmd.angle;
		th->pitch = ticcmd.pitch;

/*		if(ticcmd.bits_l & TCMD_USE)
		{
			if(!(th->iflags & THING_IFLAG_USED))
			{
				th->iflags |= THING_IFLAG_USED;
				printf("USE KEY\n");
			}
		} else
			th->iflags &= ~THING_IFLAG_USED;*/

		if(ticcmd.bits_l & TCMD_GO_UP)
		{
			if(sec->flags & SECTOR_FLAG_WATER)
			{
				th->mz += jump << 8;
			} else
			if(	th->mz >= 0 &&
				!(th->iflags & THING_IFLAG_JUMPED) &&
				th->floorz - (th->z / 256) >= 0
			){
				if(	th->floort ||
					!sec->floor.link ||
					th->iflags & THING_IFLAG_BLOCKED ||
					th->floors != thingsec[tick_idx][0]
				){
					th->iflags |= THING_IFLAG_JUMPED;
					th->mz += jump << 8;
				}
			}
		} else
			th->iflags &= ~THING_IFLAG_JUMPED;

		if(ticcmd.bits_l & TCMD_GO_DOWN)
		{
			if(sec->flags & SECTOR_FLAG_WATER)
				th->mz -= jump << 8;
			else
				ntype = thing_state->plr_crouch;
		}

		if(ticcmd.bits_l & TCMD_MOVING)
		{
			uint8_t angle = ticcmd.angle + (ticcmd.bits_h & 0xE0);
			thing_launch_ang(tick_idx, angle, speed);
		}
	}

	if(th->ticker.type != ntype)
	{
		thing_type_t *ti = thing_type + ntype;
		int32_t diff = th->ceilingz - th->floorz + th->height;
		int32_t nz;

		if(ti->height - diff < 0)
		{
			diff = th->view_height - ti->view_height;
			nz = th->z / 256 + diff;

			diff = nz + (int32_t)ti->height - (int32_t)th->height;
			if(th->ceilingz - diff >= 0)
			{
				th->z = (nz * 256) | (th->z & 0xFF);

				th->height = ti->height;
				th->view_height = ti->view_height;
				th->ticker.type = ntype;

				th->iflags |= THING_IFLAG_HEIGHTCHECK;

				projection.wh = ti->view_height;
			}
		}
	}

	thing_tick();

	// weapon
	tick_idx = 0;
	thing_frame();
	tick_idx = bkup;
}

