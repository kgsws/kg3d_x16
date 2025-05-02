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
	int16_t sin;
	int16_t cos;
	int16_t ptan;
	int16_t pcid;
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
	thing_type_t *info = thing_type + hitscan.type;
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
	dist = d0.x - hitscan.x;
	if(dist < 0)
		dist = -dist;

	dd = d0.y - hitscan.y;
	if(dd < 0)
		dd = -dd;

	if(dd > dist)
		dist = dd * inv_div[hitscan.cos] * 2;
	else
		dist = dist * inv_div[hitscan.sin] * 2;
	dist >>= 8;
	if(dist < 0)
		dist = -dist;

	// get Z
	dist *= hitscan.ptan;
	zz = hitscan.z;
	zz += dist >> 8;

	// check floor
	if(zz <= sec->floor.height)
	{
		zz = sec->floor.height;
		dist = hitscan.z - sec->floor.height;
hit_plane:
		dist *= hitscan.pcid;
		dist >>= 8;
		d0.x = hitscan.x;
		d0.x += (hitscan.sin * dist) >> 8;
		d0.y = hitscan.y;
		d0.y += (hitscan.cos * dist) >> 8;
		goto do_hit;
	}

	// check ceiling
	if(zz >= sec->ceiling.height)
	{
		zz = sec->ceiling.height - info->height;
		dist = hitscan.z - sec->ceiling.height;
		goto hit_plane;
	}

	// check backsector
	if(	wall->solid.angle & MARK_PORTAL &&
		wall->portal.backsector
	){
		sector_t *bs = map_sectors + wall->portal.backsector;

		// check blocking and heights
		if(	!(wall->portal.blocking & info->blockedby) &&
			zz > bs->floor.height &&
			zz < bs->ceiling.height
		)
			return 0;
	}

	// ceiling check
	dd = sec->ceiling.height - info->height;
	if(zz > dd)
		zz = dd;

	// radius offset
	dist = info->radius + 1;
	d0.x -= (wall->solid.dist.y * dist) >> 8;
	d0.y += (wall->solid.dist.x * dist) >> 8;

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

void hitscan_attack(uint8_t tdx, uint8_t zadd, uint8_t hang, uint8_t pitch, uint8_t type)
{
	thing_t *th = things + tdx;

	hitscan.z = th->z >> 8;
	hitscan.z += zadd;

	hitscan.sin = tab_sin[hang];
	hitscan.cos = tab_cos[hang];

	pitch >>= 1;
	hitscan.ptan = tab_tan_hs[pitch];
	hitscan.pcid = inv_div[tab_cos[pitch]] * 2;

	hitscan.type = type;

	hitscan_func(tdx, hang, cb_attack);
}

