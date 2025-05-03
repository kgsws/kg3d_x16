#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "defs.h"
#include "things.h"
#include "hitscan.h"

typedef struct
{
	int16_t x, y, z;
	uint8_t origin;
	uint8_t sector;
	uint8_t angle;
	uint8_t type;
	uint8_t axis;
	uint8_t radius;
	uint8_t height;
	uint8_t blockedby;
	int16_t sin;
	int16_t cos;
	int16_t idiv;
	int16_t wtan;
	int16_t ptan;
} hitscan_t;

// hitscan stuff
hitscan_t hitscan;

//
// attack callback

uint32_t cb_attack(wall_combo_t *wall, uint8_t tdx)
{
	vertex_t d0;
	vertex_t *vtx;
	int32_t dist, dd, zz;
	uint8_t angle;
	sector_t *sec = map_sectors + hitscan.sector;

	// V0 diff
	vtx = &wall->solid.vtx;
	d0.x = vtx->x - hitscan.x;
	d0.y = vtx->y - hitscan.y;

	// get distance
	dist = (d0.x * wall->solid.dist.y - d0.y * wall->solid.dist.x) >> 8;

	// get angle
	angle = wall->solid.angle >> 4;
	angle -= hitscan.angle;

	// get location A
	d0.x = hitscan.x;
	d0.x += (wall->solid.dist.y * dist) >> 8;
	d0.y = hitscan.y;
	d0.y -= (wall->solid.dist.x * dist) >> 8;

	// get location B
	dist *= tab_tan_hs[angle];
	dist >>= 8;
	d0.x += (wall->solid.dist.x * dist) >> 8;
	d0.y += (wall->solid.dist.y * dist) >> 8;

	// get range
	if(hitscan.axis & 0x80)
		dist = d0.x - hitscan.x;
	else
		dist = d0.y - hitscan.y;

	dist *= hitscan.idiv * 2;
	dist >>= 8;

	// get Z
	dist *= hitscan.wtan;
	zz = hitscan.z + (dist >> 8);

	// check floor
	if(zz < sec->floor.height)
	{
		zz = sec->floor.height;
		dd = sec->floor.height;
hit_plane:
		dist = hitscan.z - dd;
		dist *= hitscan.ptan;
		dist >>= 8;
		d0.x = hitscan.x;
		d0.x += (hitscan.sin * dist) >> 8;
		d0.y = hitscan.y;
		d0.y += (hitscan.cos * dist) >> 8;
		goto do_hit;
	}

	// check ceiling
	if(zz > sec->ceiling.height)
	{
		zz = sec->ceiling.height - hitscan.height;
		dd = sec->ceiling.height;
		goto hit_plane;
	}

	// check backsector
	if(	wall->solid.angle & MARK_PORTAL &&
		wall->portal.backsector
	){
		sector_t *bs = map_sectors + wall->portal.backsector;

		// check blocking and heights
		if(	!(wall->portal.blocking & hitscan.blockedby) &&
			zz > bs->floor.height &&
			zz < bs->ceiling.height
		)
			return 0;
	}
#if 0
	// ceiling check
	dd = sec->ceiling.height - hitscan.height;
	if(zz > dd)
		zz = dd;
#endif
	// radius offset
	d0.x -= (wall->solid.dist.y * hitscan.radius) >> 8;
	d0.y += (wall->solid.dist.x * hitscan.radius) >> 8;

do_hit:
	// spawn thing
	thing_spawn((int32_t)d0.x << 8, (int32_t)d0.y << 8, zz << 8, hitscan.sector, hitscan.type, 0);

	return 1;
}

//
// generic hitscan

void hitscan_func(uint8_t tdx, uint8_t hang, uint32_t (*cb)(wall_combo_t*,uint8_t))
{
	thing_t *th = things + tdx;
	uint8_t sdx = thingsec[tdx][0];
	int16_t x = th->x >> 8;
	int16_t y = th->y >> 8;

	hitscan.angle = hang;

	hitscan.x = x;
	hitscan.y = y;

	while(1)
	{
		sector_t *sec = map_sectors + sdx;
		void *wall_ptr = (void*)map_data + sec->walls;
		uint8_t last_angle;

		{
			wall_combo_t *wall = wall_ptr;
			vertex_t *vtx;

			// V0 diff
			vtx = &wall->solid.vtx;
			p2a_coord.x = vtx->x - x;
			p2a_coord.y = vtx->y - y;

			// point angle
			last_angle = point_to_angle() >> 4;
		}

		while(1)
		{
			wall_combo_t *wall = wall_ptr;
			wall_end_t *waln;
			vertex_t *vtx;
			uint8_t angle, hit;

			// wall ptr
			wall_ptr += wall_size_tab[(wall->solid.angle & MARK_MID_BITS) >> 12];
			waln = wall_ptr;

			// V0 diff
			vtx = &waln->vtx;
			p2a_coord.x = vtx->x - x;
			p2a_coord.y = vtx->y - y;

			// point angle
			angle = point_to_angle() >> 4;

			hit = angle - hang;
			hit |= hang - last_angle;

			if(!(hit & 0x80))
			{
				hitscan.sector = sdx;
				if(cb(wall, 0))
					return;

				if(!(wall->solid.angle & MARK_PORTAL))
					return;

				if(!wall->portal.backsector)
					return;

				sdx = wall->portal.backsector;

				break;
			}

			if(wall->solid.angle & MARK_LAST)
			{
				// this should never happen
				printf("NO HITSCAN WALL!\n");
				return;
			}

			last_angle = angle;
		}
	}
}

//
// hitscan bullet attack

void hitscan_attack(uint8_t tdx, uint8_t zadd, uint8_t hang, uint8_t halfpitch, uint8_t type)
{
	thing_t *th = things + tdx;
	thing_type_t *info = thing_type + type;

	hitscan.radius = info->radius + 1;
	hitscan.height = info->height;
	hitscan.blockedby = info->blockedby;

	hitscan.z = th->z >> 8;
	hitscan.z += zadd;

	hitscan.sin = tab_sin[hang];
	hitscan.cos = tab_cos[hang];

	hitscan.axis = (hang + 0x20) << 1;
	if(hitscan.axis & 0x80)
		hitscan.idiv = inv_div[hitscan.sin];
	else
		hitscan.idiv = inv_div[hitscan.cos];

	hitscan.wtan = tab_tan_hs[halfpitch];

	if(halfpitch & 0x40)
		halfpitch -= 0x40;
	else
		halfpitch += 0x40;

	hitscan.ptan = tab_tan_hs[halfpitch];

	hitscan.type = type;

	hitscan_func(tdx, hang, cb_attack);
}

