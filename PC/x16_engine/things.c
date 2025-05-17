#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "defs.h"
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
	int16_t floorz;
	int16_t ceilingz;
	uint8_t floors, ceilings;
	int16_t waterz;
	int16_t tfz, tcz;
	int32_t th_z_h;
	int32_t th_z_w;
	vertex_t vect;
	uint8_t thing;
	uint8_t sector;
	uint8_t portal;
	uint8_t iflags;
	uint8_t islink;
	uint8_t radius;
	uint8_t height;
	uint8_t blockedby;
	uint8_t step_height;
	uint8_t water_height;
	int8_t blocked;
	uint8_t angle;
	uint8_t maskblock;
	uint8_t fallback;
	uint8_t hit_thing;
	wall_masked_t *wall;
} pos_check_t;

//

static uint8_t thing_data[8192 * 2];
thing_type_t thing_type[MAX_X16_THING_TYPES];
thing_anim_t thing_anim[MAX_X16_THING_TYPES][NUM_THING_ANIMS];
uint32_t thing_hash[MAX_X16_THING_TYPES];
uint32_t sprite_hash[128];
uint8_t sprite_remap[128];
thing_state_t *const thing_state = (thing_state_t*)(thing_data + 8192);

uint32_t num_sprlnk_thg;

// thing ZERO is not valid
thing_t things[128];
uint8_t thingsec[128][16];
uint8_t thingces[128][16];

// command
ticcmd_t ticcmd;

// player
uint8_t player_thing;

// camera
uint8_t camera_thing;

// movement
static uint8_t place_slot;
static portal_t portals[256];

// position check variables
uint8_t portal_rd, portal_wr, portal_bs, portal_bl;

// position check stuff
pos_check_t poscheck;

//
//

uint32_t decode_state(uint16_t ss)
{
	uint8_t s0 = ss;
	uint8_t s1 = ss >> 8;
	uint32_t state;

	// these bit shifts are useful for 6502 code, trust me
	state = ((s0 >> 3) | (s0 << 5)) & 0xFF;
	state |= (s1 & 0x60) << 3;

	return state;
}

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
	thingsec[tdx][place_slot] = sdx;
	thingces[tdx][place_slot] = slot | maskblock;
	place_slot++;
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

//
//

void thing_clear()
{
	thing_t *th;
	uint32_t state;
	thing_state_t *st;

	// all thing slots
	for(uint32_t i = 1; i < 128; i++)
		things[i].type = 0xFF;

	// player weapon // TODO
	th = things;
	th->type = THING_WEAPON_FIRST;
	th->height = 64;

	state = thing_anim[th->type][ANIM_RAISE].state;

	st = thing_state + decode_state(state);

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

	// find free thing slot
	for(tdx = 1; tdx < 128; tdx++)
	{
		if(things[tdx].type >= 128)
			// found one
			break;
	}

	if(tdx >= 128)
		// no free slots
		return 0;

	th = things + tdx;
	th->type = type;

	ti = thing_type + type;

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

	st = thing_state + decode_state(state);

	th->next_state = state;
	th->ticks = 1;

	if(st->sprite & 0x80)
		th->sprite = 0xFF; // 'NONE'
	else
		th->sprite = sprite_remap[st->sprite] + (st->frm_nxt & 0x1F);

	if(!thing_check_pos(tdx, &x, &y, z >> 8, 1, sector))
	{
		sector_t *sec = map_sectors + sector;

		printf("BAD thing spawn\n");

		place_slot = 0;
		map_secext[sector].maskblock = 0;
		place_to_sector(tdx, sector);

		th->ceilingz = sec->ceiling.height;
		th->floorz = sec->floor.height;
		th->ceilings = sector;
		th->floors = sector;
	} else
		thing_apply_position();

	return tdx;
}

void thing_remove(uint8_t tdx)
{
	thing_t *th = things + tdx;
	th->next_state = 0;
	th->ticks = 1;
}

void thing_launch(uint8_t tdx, uint8_t speed)
{
	thing_t *th = things + tdx;
	uint8_t angle = th->angle;
	uint8_t pitch = th->pitch / 2 - 64;

	if(pitch)
	{
		th->mz += tab_sin[pitch] * speed;
		speed = (tab_cos[pitch] * speed) >> 8;
	}

	th->mx += tab_sin[angle] * speed;
	th->my += tab_cos[angle] * speed;
}

//
//

static void add_sector(uint8_t sdx, uint8_t islink, uint8_t touch)
{
	uint32_t i;

	for(i = 0; i < portal_wr; i++)
	{
		if(portals[i].sector == sdx)
		{
			portals[i].touch |= touch;
			break;
		}
	}

	if(i == portal_wr)
	{
		portals[portal_wr].sector = sdx;
		portals[portal_wr].islink = islink;
		portals[portal_wr].touch = touch;
		portal_wr++;
	}

}

static void get_sector_floorz(sector_t *sec)
{
	int16_t fz;

	if(poscheck.islink & 0x80)
	{
		poscheck.tfz = -16384;
		return;
	}

	poscheck.tfz = sec->floor.height;

	if(poscheck.maskblock)
	{
		poscheck.tfz += sec->floormasked;
		return;
	}

	if(!sec->floor.link)
	{
		poscheck.tfz -= sec->floordist;
		return;
	}

	fz = sec->floor.height;

	sec = map_sectors + sec->floor.link;

	poscheck.tfz = sec->floor.height;

	if(sec->flags & SECTOR_FLAG_WATER)
	{
		if(	poscheck.islink & 0x40 &&
			fz > poscheck.th_z_w
		)
			return;

		fz -= poscheck.water_height;
		if(fz > poscheck.tfz)
		{
			poscheck.tfz = fz;
			if(fz > poscheck.waterz)
				poscheck.waterz = fz;
		}
	}
}

static void get_sector_ceilingz(sector_t *sec)
{
	poscheck.tcz = sec->ceiling.height;

	if(!sec->ceiling.link)
		return;

	poscheck.tcz = map_sectors[sec->ceiling.link].ceiling.height;
}

static void check_wall_point(thing_t *th, vertex_t *vtx, vertex_t d0)
{
	uint8_t ang;
	int16_t dist;

	p2a_coord.x = vtx->x - (th->x >> 8);
	p2a_coord.y = vtx->y - (th->y >> 8);

	ang = point_to_angle() >> 4;
	ang += 0x40;
	poscheck.angle = ang;

	ang += 0x80;
	poscheck.vect.x = tab_sin[ang];
	poscheck.vect.y = tab_cos[ang];

	dist = (d0.y * poscheck.vect.x - d0.x * poscheck.vect.y) >> 8;
	if(-dist < poscheck.radius)
	{
		if(portal_bl)
			poscheck.blocked = 1;
		else
		if(portal_bs && portal_wr < 16)
			add_sector(portal_bs, 0, 0xFF);
	}
}

static uint32_t check_backsector(wall_masked_t *wall, int32_t th_z)
{
	sector_t *bs;
	int16_t dist;

	if(!(wall->angle & MARK_PORTAL))
		return 1;

	if(!wall->backsector)
		return 1;

	if(sectorth[wall->backsector][31] >= 31)
		return 1;

	if(wall->blocking & poscheck.blockedby & 0x7F)
		return 1;

	bs = map_sectors + wall->backsector;
	portal_bs = wall->backsector;

	map_secext[wall->backsector].maskblock = poscheck.maskblock;

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

	dist = poscheck.tcz - poscheck.th_z_h;
	if(dist < 0)
		return 1;

	return 0;
}

static uint32_t check_things(uint8_t sdx, uint8_t tdx, int32_t x, int32_t y, int32_t z)
{
	thing_t *th = things + tdx;

	for(uint32_t i = 0; i < 31; i++)
	{
		uint8_t tdo = sectorth[sdx][i];
		thing_t *tho;
		uint8_t ang;
		int16_t dist;
		int16_t temp;
		int16_t zzz;
		vertex_t d0;

		if(!tdo)
			continue;

		if(tdo == tdx)
			continue;

		tho = things + tdo;

		if(tho->origin == tdx)
			continue;

		if(th->origin == tdo)
			continue;

		if(!(tho->blocking & poscheck.blockedby))
			continue;

		p2a_coord.x = (tho->x >> 8) - (th->x >> 8);
		p2a_coord.y = (tho->y >> 8) - (th->y >> 8);

		ang = point_to_angle() >> 4;
		ang += 0x40;
		poscheck.angle = ang;

		ang += 0x80;
		poscheck.vect.x = tab_sin[ang];
		poscheck.vect.y = tab_cos[ang];

		d0.x = (tho->x >> 8) - x;
		d0.y = (tho->y >> 8) - y;

		temp = (int16_t)poscheck.radius + (int16_t)tho->radius;

		dist = (d0.x * poscheck.vect.y - d0.y * poscheck.vect.x) >> 8;
		dist -= temp;
		if(dist >= 0)
			continue;

		zzz = tho->z >> 8;
		temp = zzz - poscheck.th_z_h;
		if(temp >= 0)
		{
			if(!(th->eflags & THING_EFLAG_PROJECTILE) && zzz < poscheck.ceilingz)
			{
				poscheck.ceilingz = zzz;
				poscheck.ceilings = 0;
			}
			poscheck.iflags = THING_IFLAG_HEIGHTCHECK;
			tho->iflags |= THING_IFLAG_HEIGHTCHECK;
			continue;
		}

		zzz += tho->height;
		temp = z - zzz;

		if(tho->eflags & THING_EFLAG_CLIMBABLE)
			temp += poscheck.step_height;

		if(temp >= 0)
		{
			if(!(th->eflags & THING_EFLAG_PROJECTILE) && zzz > poscheck.floorz)
			{
				poscheck.floorz = zzz;
				poscheck.floors = 0;
			}
			poscheck.iflags = THING_IFLAG_HEIGHTCHECK;
			tho->iflags |= THING_IFLAG_HEIGHTCHECK;
			continue;
		}

		if(tho->eflags & THING_EFLAG_PUSHABLE)
		{
			// TODO: mass calculation
			tho->mx += th->mx;
			tho->my += th->my;
		}

		poscheck.blocked = 2; // blocked by thing
		poscheck.hit_thing = tdo;
		return 1;
	}

	return 0;
}

static void prepare_pos_check(uint8_t tdx, int32_t z)
{
	thing_t *th = things + tdx;

	poscheck.thing = tdx;
	poscheck.radius = th->radius;
	poscheck.height = th->height;
	poscheck.blockedby = th->eflags & THING_EFLAG_NOCLIP ? 0 : th->blockedby;
	poscheck.water_height = thing_type[th->type].water_height;
	poscheck.step_height = thing_type[th->type].step_height;
	poscheck.th_z_h = z + poscheck.height;
}

//
//

uint32_t thing_init(const char *file)
{
	if(load_file(file, thing_data, sizeof(thing_data)) != sizeof(thing_data))
		return 1;

	// sprite count
	num_sprlnk_thg = thing_state->raw[0];

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

	return 0;
}

int32_t thing_find_type(uint32_t hash)
{
	for(uint32_t i = 0; i < 128; i++)
		if(thing_hash[i] == hash)
			return i;
	return -1;
}

static void fallback_backsector(wall_masked_t *wall)
{
	if(!(wall->angle & MARK_PORTAL))
		return;

	if(!wall->backsector)
		return;

	poscheck.fallback = wall->backsector;
}

uint32_t thing_check_pos(uint8_t tdx, int32_t *nx, int32_t *ny, int16_t z, uint32_t on_floor, uint8_t sdx)
{
	thing_t *th = things + tdx;
	sector_t *sec;
	int32_t x, y;
	uint8_t newsec = 0;
	uint8_t failsafe = 3;

	if(!sdx)
		sdx = thingsec[tdx][0];

	prepare_pos_check(tdx, z);

	if(poscheck.step_height & 0x80)
	{
		poscheck.step_height &= 0x7F;
		sec = map_sectors + sdx;
		if(!on_floor)
		{
			if(	sec->floor.link &&
				map_sectors[sec->floor.link].flags & SECTOR_FLAG_WATER &&
				z < sec->floor.height
			)
				poscheck.step_height <<= 1;
			else
				poscheck.step_height >>= 1;
		}
	}

	poscheck.fallback = sdx;

	portals[0].sector = sdx;
	portals[0].islink = 0;
	portals[0].touch = 0;

fail_safe:
	x = *nx >> 8;
	y = *ny >> 8;

	poscheck.iflags = THING_IFLAG_HEIGHTCHECK;
	poscheck.floorz = -16384;
	poscheck.ceilingz = 16383;
	poscheck.waterz = -16383;
	poscheck.blocked = 0;

	portal_rd = 0;
	portal_wr = 1;

	map_secext[portals[0].sector].maskblock = 0;

	while(portal_rd < portal_wr)
	{
		void *wall_ptr;
		uint8_t inside = 1;

		sdx = portals[portal_rd].sector;
		poscheck.islink = portals[portal_rd].islink;
		portal_rd++;

		sec = map_sectors + sdx;

		if(!poscheck.islink)
		{
			wall_ptr = map_data + sec->walls;
			while(1)
			{
				uint8_t touch = 0xC0;
				wall_masked_t *wall = wall_ptr;
				wall_end_t *waln;
				int16_t dist;
				vertex_t d0;
				vertex_t *vtx;

				// wall ptr
				wall_ptr += wall_size_tab[(wall->angle & MARK_TYPE_BITS) >> 12];
				waln = wall_ptr;

				// V0 diff
				vtx = &wall->vtx;
				d0.x = vtx->x - x;
				d0.y = vtx->y - y;

				// get distance
				dist = (d0.x * wall->dist.y - d0.y * wall->dist.x) >> 8;
				if(dist >= poscheck.radius)
					goto do_next;

				poscheck.maskblock = 0;

				if(dist + poscheck.radius < 0)
					touch = 0;
				else
				if(	!(th->eflags & THING_EFLAG_NOCLIP) &&
					(wall->angle & MARK_MID_BITS) == MARK_MASKED &&
					wall->blockmid & 0x80 &&
					wall->blockmid & poscheck.blockedby & 0x7F
				)
					poscheck.maskblock = 0x80;

				if(dist < 0)
				{
					fallback_backsector(wall);
					inside = 0;
				}

				portal_bs = 0;
				if(!poscheck.blocked)
				{
					portal_bl = check_backsector(wall, z);
					if(th->eflags & THING_EFLAG_NOCLIP)
						portal_bl = 0;
				}

				// get left side
				dist = (d0.x * wall->dist.x + d0.y * wall->dist.y) >> 8;
				if(dist < 0)
				{
					if(!poscheck.blocked && -dist < poscheck.radius)
						check_wall_point(th, vtx, d0);
					goto do_next;
				}

				// V1 diff
				vtx = &waln->vtx;
				d0.x = vtx->x - x;
				d0.y = vtx->y - y;

				// get right side
				dist = (d0.x * wall->dist.x + d0.y * wall->dist.y) >> 8;
				if(dist >= 0)
				{
					if(!poscheck.blocked && dist < poscheck.radius)
						check_wall_point(th, vtx, d0);
					goto do_next;
				}

				if(poscheck.blocked)
					portal_bl = check_backsector(wall, z);

				if(th->eflags & THING_EFLAG_NOCLIP)
				{
					if(!portal_bs)
						goto do_next;
				}

				if(!portal_bl && portal_wr < 16)
				{
					add_sector(portal_bs, 0, touch);
					goto do_next;
				}

				poscheck.angle = wall->angle >> 4;
				poscheck.vect = wall->dist;
				poscheck.blocked = 1; // blocked by wall

				if(!inside)
					// blocked by other side
					poscheck.blocked = 0x41; 

				poscheck.wall = wall;

				return 0;

do_next:
				if(wall->angle & MARK_LAST)
				{
					if(poscheck.blocked)
					{
						poscheck.wall = wall;
						return 0;
					}

					if(sec->floor.link)
					{
						if(portal_wr < 16)
						{
							map_secext[sec->floor.link].maskblock = 0;
							add_sector(sec->floor.link, 0x01, 0x80);
						} else
							goto do_block_extra;
					}

					if(sec->ceiling.link)
					{
						if(portal_wr < 16)
						{
							map_secext[sec->ceiling.link].maskblock = 0;
							add_sector(sec->ceiling.link, 0x80, 0x80);
						} else
							goto do_block_extra;
					}

					break;
				}
			}
		}

		poscheck.maskblock = map_secext[sdx].maskblock;

		if(!(th->eflags & THING_EFLAG_NOCLIP))
		{
			get_sector_floorz(sec);
			if(poscheck.tfz > poscheck.floorz)
			{
				poscheck.floorz = poscheck.tfz;
				poscheck.floors = poscheck.maskblock ? 0 : sec - map_sectors;
			}

			get_sector_ceilingz(sec);
			if(poscheck.tcz < poscheck.ceilingz)
			{
				poscheck.ceilingz = poscheck.tcz;
				poscheck.ceilings = sec - map_sectors;
			}
		}

		if(!newsec && inside && !poscheck.islink)
			newsec = sdx;
	}

	if(!newsec)
	{
		// nothing blocked this movement, but thing is not in any sector
		if(!(th->eflags & THING_EFLAG_PROJECTILE))
		{
			if(failsafe)
			{
				failsafe--;

				if(failsafe & 1)
					*nx = *nx ^ 512;
				else
					*ny = *ny ^ 512;

				goto fail_safe;
			}
		} else
			newsec = poscheck.fallback;
	}

	if(th->eflags & THING_EFLAG_NOCLIP)
	{
		poscheck.islink = 0x40;
		poscheck.maskblock = 0;

		get_sector_floorz(map_sectors + newsec);
		poscheck.floorz = poscheck.tfz;

		get_sector_ceilingz(map_sectors + newsec);
		poscheck.ceilingz = poscheck.tcz;

		poscheck.floors = newsec;
		poscheck.ceilings = newsec;
	} else
	{
		for(uint32_t i = 0; i < portal_wr; i++)
		{
			if(check_things(portals[i].sector, tdx, x, y, z))
				return 0;
		}
	}

	poscheck.maskblock = map_secext[portals[0].sector].maskblock;
	if(poscheck.maskblock)
	{
		sec = map_sectors + portals[0].sector;

		get_sector_floorz(sec);
		if(poscheck.tfz > poscheck.floorz)
		{
			poscheck.floorz = poscheck.tfz;
			poscheck.floors = 0;
		}

		get_sector_ceilingz(sec);
		if(poscheck.tcz < poscheck.ceilingz)
		{
			poscheck.ceilingz = poscheck.tcz;
			poscheck.ceilings = portals[0].sector;
		}
	}

	poscheck.ceilingz -= poscheck.height;

	if(!(th->eflags & THING_EFLAG_NOCLIP))
	{
		if(poscheck.ceilingz < poscheck.floorz)
		{
do_block_extra:
			poscheck.blocked = -1;	// blocked by space
			return 0;
		}

		if(poscheck.floorz <= poscheck.waterz)
			poscheck.iflags |= THING_IFLAG_NOJUMP;
	}

	poscheck.portal = portal_wr;
	poscheck.sector = newsec;

	return newsec;
}

//
//

void thing_apply_position()
{
	thing_t *th = things + poscheck.thing;

	// remove from all sectors
	remove_from_sectors();

	// place to first sector
	place_slot = 0;
	place_to_sector(poscheck.thing, poscheck.sector);

	// place to detected sectors
	for(uint32_t i = 0; i < poscheck.portal; i++)
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
	th->ceilingz = poscheck.ceilingz;

	// save sectors
	th->floors = poscheck.floors;
	th->ceilings = poscheck.ceilings;
}

//
//

uint32_t thing_check_heights(uint8_t tdx)
{
	thing_t *th = things + tdx;

	prepare_pos_check(tdx, th->z >> 8);

	poscheck.th_z_w = (th->z >> 8) + thing_type[th->type].view_height;

	poscheck.islink = 0x40;
	poscheck.iflags = 0;
	poscheck.floorz = -16384;
	poscheck.ceilingz = 16383;
	poscheck.waterz = -16383;
	poscheck.blocked = 0;
	poscheck.step_height &= 0x7F;

	if(th->eflags & THING_EFLAG_NOCLIP)
	{
		uint8_t sdx = thingsec[tdx][0];
		sector_t *sec = map_sectors + sdx;

		poscheck.maskblock = 0;

		poscheck.floors = sdx;
		poscheck.ceilings = sdx;

		get_sector_floorz(sec);
		poscheck.floorz = poscheck.tfz;

		get_sector_ceilingz(sec);
		poscheck.ceilingz = poscheck.tcz;

		return 1;
	}

	for(uint32_t i = 0; i < 16; i++)
	{
		uint8_t sdx = thingsec[tdx][i];
		sector_t *sec;

		if(!sdx)
			break;

		poscheck.maskblock = thingces[tdx][i] & 0x80;

		sec = map_sectors + sdx;

		get_sector_floorz(sec);
		if(poscheck.tfz > poscheck.floorz)
		{
			poscheck.floorz = poscheck.tfz;
			poscheck.floors = poscheck.maskblock ? 0 : sdx;
		}

		get_sector_ceilingz(sec);
		if(poscheck.tcz < poscheck.ceilingz)
		{
			poscheck.ceilingz = poscheck.tcz;
			poscheck.ceilings = sdx;
		}

		if(check_things(sdx, tdx, th->x >> 8, th->y >> 8, th->z >> 8))
			return 0;
	}

	poscheck.ceilingz -= poscheck.height;
	if(poscheck.ceilingz < poscheck.floorz)
	{
		poscheck.blocked = -1;
		return 0;
	}

	if(poscheck.floorz <= poscheck.waterz)
		poscheck.iflags |= THING_IFLAG_NOJUMP;

	return 1;
}

//
//

static void projectile_death(thing_t *th)
{
	uint32_t state;
	thing_state_t *st;
	int32_t nx, ny;
	uint8_t radius;

//	printf("block %d; %u\n", poscheck.blocked, poscheck.hit_thing);

	// XY
	nx = th->x;
	ny = th->y;

	// stop
	th->mx = 0;
	th->my = 0;

	// update Z
	th->z += th->mz / 2;
	th->mz = 0;

	// unblock
	th->blocking = 0;
	th->blockedby = 0;

	// radius
	radius = th->radius;
	th->radius = thing_type[th->type].alt_radius;

	// unflag
	th->eflags &= ~THING_EFLAG_PROJECTILE;

	// noclip
	th->eflags |= THING_EFLAG_NOCLIP;

	// change animation
	state = thing_anim[th->type][ANIM_DEATH].state;
	st = thing_state + decode_state(state);
	th->next_state = state;
	th->ticks = 1;

	// test NUL state
	if(!th->next_state)
		return;

	// wall hit check
	if(poscheck.blocked & 1)
	{
		uint8_t *tex = &poscheck.wall->texture_top;
		uint16_t backsector = 0xFF00;

		if(poscheck.wall->angle & MARK_PORTAL)
		{
			if(poscheck.wall->backsector)
				backsector = poscheck.wall->backsector;
		} else
		if((poscheck.wall->angle & MARK_MID_BITS) == MARK_SPLIT)
		{
			wall_split_t *ws = (wall_split_t*)poscheck.wall;
			backsector = 0;
			map_sectors[0].ceiling.height = ws->height_split;
		}

		if(backsector < 0x100)
		{
			sector_t *sec = map_sectors + backsector;
			if((th->z >> 8) <= sec->ceiling.height - th->height)
				tex = &poscheck.wall->texture_bot;
		}

		if(*tex == 0xFF)
		{
			th->next_state = 0;
			return;
		}
	}

	// floor check
	if(poscheck.blocked & 4)
	{
		sector_t *sec = map_sectors + th->floors;

		if(th->floors && sec->floor.texture == 0xFF)
		{
			th->next_state = 0;
			return;
		}
	}

	// ceiling check
	if(poscheck.blocked & 8)
	{
		sector_t *sec = map_sectors + th->ceilings;

		if(th->ceilings && sec->ceiling.texture == 0xFF)
		{
			th->next_state = 0;
			return;
		}
	}

	// fix position
	if(poscheck.blocked & 3)
	{
		int32_t dist;
		vertex_t d0;

		if(poscheck.blocked & 1)
		{
			d0.x = poscheck.wall->vtx.x - (nx >> 8);
			d0.y = poscheck.wall->vtx.y - (ny >> 8);
		} else
		{
			d0.x = (things[poscheck.hit_thing].x - nx) >> 8;
			d0.y = (things[poscheck.hit_thing].y - ny) >> 8;
		}

		dist = (d0.x * poscheck.vect.y - d0.y * poscheck.vect.x) >> 8;

		if(poscheck.blocked & 2)
			dist -= things[poscheck.hit_thing].radius / 2;

		dist -= radius;
		dist -= 2; // integer precision safety

		if(	dist > 0 &&
			dist < 128
		)
		{
			nx += tab_sin[th->angle] * dist;
			ny += tab_cos[th->angle] * dist;
			radius = 0;
		}
	}

	if(radius == th->radius)
		return;

	// update position / radius
	if(thing_check_pos(th - things, &nx, &ny, th->z >> 8, 1, 0))
	{
		th->x = nx;
		th->y = ny;
		thing_apply_position();
	}
}

//
//

void things_tick()
{
	int16_t zz, gravity;
	int16_t old_fz;
	uint32_t on_floor;
	sector_t *sec;
	uint8_t move_again;

	for(uint32_t i = 1; ; i++)
	{
		thing_t *th = things + i;

		if(i >= 128)
		{
			// special case for local player weapon
			i = 0;
			th = things;
			goto do_animation;
		}

		if(th->type >= 128)
			continue;

		// decrement counter
		if(th->counter)
			th->counter--;

		// on floor?
		on_floor = th->floorz >= (th->z >> 8);

		// player stuff
		if(i == player_thing)
		{
			if(th->counter)
			{
				// frozen
				ticcmd.angle = th->angle;
				ticcmd.pitch = th->pitch;
			} else
			{
				uint8_t new_type = th->type;

				th->angle = ticcmd.angle;
				th->pitch = ticcmd.pitch;

				if(ticcmd.bits_l & TCMD_USE)
				{
					if(!(th->iflags & THING_IFLAG_USED))
					{
						th->iflags |= THING_IFLAG_USED;

						// TEST
						int32_t nx, ny;

						nx = th->x + tab_sin[th->angle] * 256;
						ny = th->y + tab_cos[th->angle] * 256;

						if(thing_check_pos(i, &nx, &ny, th->z >> 8, on_floor, 0))
						{
							th->x = nx;
							th->y = ny;
							thing_apply_position();
						}
					}
				} else
					th->iflags &= ~THING_IFLAG_USED;

				if(ticcmd.bits_l & TCMD_MOVING)
				{
					uint8_t speed = thing_type[th->type].speed;
					uint8_t angle = ticcmd.angle + (ticcmd.bits_h & 0xE0);
					th->mx += tab_sin[angle] * speed;
					th->my += tab_cos[angle] * speed;
				}

				sec = map_sectors + thingsec[i][0];
				gravity = 0;
				if(!(sec->flags & SECTOR_FLAG_WATER)) // not under water
					gravity = th->gravity << 4;

				if(gravity)
				{
					if(ticcmd.bits_l & TCMD_GO_DOWN)
						new_type = THING_TYPE_PLAYER_C;
					else
						new_type = THING_TYPE_PLAYER_N;

					if(ticcmd.bits_l & TCMD_GO_UP)
					{
						if(on_floor && !(th->iflags & (THING_IFLAG_NOJUMP | THING_IFLAG_JUMPED)))
						{
							th->iflags |= THING_IFLAG_JUMPED;
							th->mz = (int32_t)thing_type[th->type].jump_pwr << 8;
						}
					} else
						th->iflags &= ~THING_IFLAG_JUMPED;
				} else
				{
					if(sec->flags & SECTOR_FLAG_WATER)
						new_type = THING_TYPE_PLAYER_S;
					else
						new_type = THING_TYPE_PLAYER_F;

					if(ticcmd.bits_l & TCMD_GO_DOWN)
						th->mz -= thing_type[new_type].jump_pwr * 256;
					else
					if(ticcmd.bits_l & TCMD_GO_UP)
						th->mz += thing_type[new_type].jump_pwr * 256;
				}

				if(	new_type != th->type &&
					th->type >= THING_TYPE_PLAYER_F
				){
					thing_type_t *ti = thing_type + new_type;

					if(ti->height)
					{
						if(ti->height < th->height)
						{
							int16_t diff = thing_type[th->type].view_height - ti->view_height;

							th->ceilingz += th->height;
							th->ceilingz -= ti->height;

							th->z += diff << 8;
							th->mz -= th->gravity;

							th->type = new_type;
							th->height = ti->height;
							th->iflags |= THING_IFLAG_HEIGHTCHECK;

							on_floor = 0;

							if(i == camera_thing)
							{
								projection.viewheight = ti->view_height;
								projection.wh = ti->view_height;
								projection.wd = 0;
							}
						} else
						{
							int16_t diff, wiff;

							diff = th->ceilingz + th->height;
							diff -= ti->height;

							if(diff >= (th->z >> 8))
							{
								th->ceilingz = diff;

								wiff = ti->view_height - thing_type[th->type].view_height;
								th->z -= wiff << 8;
								diff = (th->z >> 8) - th->floorz;
								if(diff < 0)
								{
									th->z = th->floorz << 8;
									wiff += diff;
									on_floor = 1;
								}

								th->type = new_type;
								th->height = ti->height;
								th->iflags |= THING_IFLAG_HEIGHTCHECK;

								if(i == camera_thing)
								{
									projection.viewheight = ti->view_height;
									projection.wh += wiff;

									projection.wd = projection.viewheight - projection.wh;
									if(projection.wd > projection.viewheight / 2)
										projection.wd = projection.viewheight / 2;

									projection.wd >>= 1;
									if(!projection.wd)
										projection.wd = 1;
								}
							}
						}
					}
				}
			}
		}

		// camera stuff
		if(i == camera_thing)
		{
			projection.wh += projection.wd;
			if(projection.wh > projection.viewheight)
			{
				projection.wh = projection.viewheight;
				projection.wd = 0;
			}

			old_fz = th->floorz;
		}

		// projectile multi-move
		if(th->eflags & THING_EFLAG_PROJECTILE)
			move_again = thing_type[th->type].jump_pwr;

multi_move:
		// XY movement
		if(th->mx || th->my)
		{
			int32_t nx, ny;

			nx = th->x + th->mx;
			ny = th->y + th->my;

			if(thing_check_pos(i, &nx, &ny, th->z >> 8, on_floor, 0))
			{
apply_position:
				th->x = nx;
				th->y = ny;
				thing_apply_position();
			} else
			{
				if(th->eflags & THING_EFLAG_PROJECTILE)
				{
					projectile_death(th);
					move_again = 0;
				} else
				if(poscheck.blocked > 0 && th->eflags & THING_EFLAG_SLIDING)
				{
					// sliding
					int8_t a;
					int32_t dist;
					int32_t mx, my;

					mx = abs(th->mx);
					my = abs(th->my);

					dist = mx > my ? mx + my / 2 : my + mx / 2;

					p2a_coord.x = th->mx;
					p2a_coord.y = th->my;

					a = poscheck.angle;
					if(poscheck.blocked & 0x40)
						a ^= 0x80;

					a -= point_to_angle() >> 4;
					a += 0xC0;

					dist = (dist * a * 4) >> 8;

					if(poscheck.blocked & 0x40)
						dist = -dist;

					th->mx = (poscheck.vect.x * dist) >> 8;
					th->my = (poscheck.vect.y * dist) >> 8;

					nx = th->x + th->mx;
					ny = th->y + th->my;

					if(!thing_check_pos(i, &nx, &ny, th->z >> 8, on_floor, 0)) // sliding vector
					{
						uint8_t ang = level_tick << 5;

						// stop
						th->mx = 0;
						th->my = 0;

						// try to wiggle out
						nx = th->x + tab_sin[ang];
						ny = th->y + tab_cos[ang];

						if(thing_check_pos(i, &nx, &ny, th->z >> 8, on_floor, 0))
						{
							poscheck.iflags &= ~THING_IFLAG_NOJUMP; // allow jumping in water
							goto apply_position;
						}
					} else
					{
						poscheck.iflags &= ~THING_IFLAG_NOJUMP; // allow jumping in water
						goto apply_position;
					}
				} else
				{
					// not sliding
					th->mx = 0;
					th->my = 0;
				}
			}

			// friction
			if(!(th->eflags & THING_EFLAG_PROJECTILE))
			{
				th->mx /= 2;
				th->my /= 2;

				switch(th->mx & 0xFF00)
				{
					case 0x0000:
					case 0xFF00:
						th->mx = 0;
					break;
				}

				switch(th->my & 0xFF00)
				{
					case 0x0000:
					case 0xFF00:
						th->my = 0;
					break;
				}
			}
		} else
		if(th->iflags & THING_IFLAG_HEIGHTCHECK)
		{
			if(thing_check_heights(i))
			{
				th->iflags &= ~(THING_IFLAG_HEIGHTCHECK | THING_IFLAG_NOJUMP);
				th->iflags |= poscheck.iflags;
				th->floorz = poscheck.floorz;
				th->ceilingz = poscheck.ceilingz;
				th->floors = poscheck.floors;
				th->ceilings = poscheck.ceilings;
			}
		}

		// Z movement
		th->z += th->mz;

		zz = th->z >> 8;

		if(zz > th->ceilingz)
		{
			zz = th->ceilingz;
			th->z = zz << 8;
			th->mz = 0;

			if(th->eflags & THING_EFLAG_PROJECTILE)
			{
				poscheck.blocked = 8;
				projectile_death(th);
				move_again = 0;
			}
		}

		if(zz < th->floorz)
		{
			if(i == camera_thing)
			{
				int16_t diff = th->floorz - zz;

				if(old_fz < th->floorz)
					diff = th->floorz - zz; // step-up
				else
					diff = -th->mz >> 9; // landing dip

				projection.wh -= diff;

				if(projection.wh & 0x80)
					projection.wh = 0;
//				if(projection.wh < projection.viewheight / 2)
//					projection.wh = projection.viewheight / 2;

				projection.wd = (projection.viewheight - projection.wh) >> 1;
				if(!projection.wd)
					projection.wd = 1;
			}

			zz = th->floorz;
			th->z = zz << 8;
			th->mz = 0;
			on_floor = 1;

			if(th->eflags & THING_EFLAG_PROJECTILE)
			{
				poscheck.blocked = 4;
				projectile_death(th);
				move_again = 0;
			}
		}

		// sector stuff
		sec = map_sectors + thingsec[i][0];

		// gravity
		gravity = 0;
		if(sec->flags & SECTOR_FLAG_WATER)
		{
			if(th->eflags & THING_EFLAG_WATERSPEC)
			{
				if(thing_type[th->type].view_height > thing_type[th->type].water_height)
					th->mz = th->gravity << 5;
				else
					th->mz = -th->gravity << 5;
				goto skip_gravity_friction;
			}
		} else
			// not under water
			gravity = th->gravity;

		if(gravity)
		{
			if(!on_floor)
				th->mz -= gravity << 4;
		} else
		if(!(th->eflags & THING_EFLAG_PROJECTILE))
		{
			// friction
			th->mz /= 2;
			switch(th->mz & 0xFF00)
			{
				case 0x0000:
				case 0xFF00:
					th->mz = 0;
				break;
			}
		}

skip_gravity_friction:

		// linked sectors
		if(sec->floor.link || sec->ceiling.link)
		{
			zz = (th->z >> 8) + thing_type[th->type].view_height;

			if(sec->floor.link)
			{
				if(zz < sec->floor.height)
				{
					swap_thing_sector(i, sec->floor.link);
					th->iflags |= THING_IFLAG_HEIGHTCHECK;
					goto skip_links;
				}
			}

			if(sec->ceiling.link)
			{
				if(zz > sec->ceiling.height)
				{
					swap_thing_sector(i, sec->ceiling.link);
					th->iflags |= THING_IFLAG_HEIGHTCHECK;
					goto skip_links;
				}
			}
		}
skip_links:

		// projectile double move
		if(move_again)
		{
			move_again--;
			on_floor = 0;
			goto multi_move;
		}

do_animation:
		// animation
		if(th->ticks)
		{
			th->ticks--;
			if(!th->ticks)
			{
act_next:
				if(th->next_state & 0x7FFF)
				{
					thing_state_t *st;

					st = thing_state + decode_state(th->next_state);

					if(action_func(i, st->action))
						goto act_next;

					th->next_state = st->next | ((st->frm_nxt & 0xE0) << 8);

					th->ticks = st->ticks;
					if(st->sprite & 0x80)
						th->sprite = 0xFF; // 'NONE'
					else
						th->sprite = sprite_remap[st->sprite] + (st->frm_nxt & 0x1F);
				} else
				if(!i)
				{
					th->next_state = thing_anim[th->type][ANIM_READY].state;
					if(th->next_state)
						goto act_next;
				} else
				{
					poscheck.thing = i;
					remove_from_sectors();

					th->type = 0xFF;

					for(uint32_t j = 0; j < 256; j++)
					{
						thing_t *th = things + j;

						if(th->origin == i)
							th->origin = 0;

						if(th->target == i)
							th->target = 0;
					}
				}
			}
		}

		// stop
		if(!i)
			break;
	}
}

