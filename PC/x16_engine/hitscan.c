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
	uint8_t link;
	uint8_t radius;
	uint8_t height;
	uint8_t blockedby;
	int16_t sin;
	int16_t cos;
	int16_t idiv;
	int16_t wtan;
	int16_t ptan;
	//
	int16_t dist;
	int16_t ztmp;
	//
	uint8_t thing_pick;
	uint8_t thing_sdx;
	int16_t thing_zz;
} hitscan_t;

//
static hitscan_t hitscan;

static uint8_t sector_list[256];
static uint8_t sector_idx;

static uint8_t thing_list[256];
static uint8_t thing_idx;

//
// thing scan

static uint32_t in_thing_list(uint8_t tdx)
{
	for(uint32_t i = 0; i < thing_idx; i++)
		if(thing_list[i] == tdx)
			return 1;

	return 0;
}

static void scan_things(uint32_t (*cb)(thing_t*,int16_t))
{
	vertex_t d1;
	int32_t dd, dt;

	thing_idx = 0;
	hitscan.thing_pick = 0;

	// check sectors
	for(uint32_t i = 0; i < sector_idx; i++)
	{
		uint8_t sdx = sector_list[i];

		// things in this sector
		for(uint32_t i = 0; i < 31; i++)
		{
			uint8_t tdx = sectorth[sdx][i];
			thing_t *th = things + tdx;

			if(	tdx &&
				tdx != hitscan.origin &&
				th->blocking & hitscan.blockedby &&
				!in_thing_list(tdx)
			){
				thing_list[thing_idx] = tdx;

				d1.x = (th->x >> 8) - hitscan.x;
				d1.y = (th->y >> 8) - hitscan.y;

				dd = (d1.x * hitscan.cos - d1.y * hitscan.sin) >> 8;
				if(dd < 0)
					dd = -dd;

				dd -= th->radius;
				if(dd <= 0)
				{
					dt = (d1.y * hitscan.cos + d1.x * hitscan.sin) >> 8;
					if(dt >= 0)
					{
						dd += dt;
						if(	dd < hitscan.dist &&
							cb(th, dd)
						){
							hitscan.dist = dd;
							hitscan.thing_pick = tdx;
							hitscan.thing_sdx = sdx;
							hitscan.thing_zz = hitscan.ztmp;
						}
					}
				}

				thing_idx++;
			}
		}
	}
}

//
// attack callback

static uint32_t cb_attack_thing(thing_t *th, int16_t dd)
{
	int32_t tz = th->z >> 8;

	dd = (dd * (int32_t)hitscan.wtan) >> 8;
	hitscan.ztmp = hitscan.z + dd;

	if(hitscan.ztmp < tz)
		return 0;

	tz += th->height;
	if(hitscan.ztmp > tz)
		return 0;

	return 1;
}

static uint32_t cb_attack(wall_combo_t *wall)
{
	vertex_t d0;
	vertex_t *vtx;
	thing_t *th;
	int32_t dist, dd, zz;
	uint8_t angle, texture, tdx;
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
	dd = (dist * hitscan.wtan) >> 8;
	zz = hitscan.z + dd;

	// check floor
	if(zz < sec->floor.height)
	{
		if(sec->floor.link)
		{
			hitscan.link = sec->floor.link;
			return 0;
		}

		zz = sec->floor.height;
		dd = sec->floor.height;
		texture = sec->floor.texture;
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
		if(sec->ceiling.link)
		{
			hitscan.link = sec->ceiling.link;
			return 0;
		}

		zz = sec->ceiling.height - hitscan.height;
		dd = sec->ceiling.height;
		texture = sec->ceiling.texture;

		goto hit_plane;
	}

	// default texture
	texture = wall->portal.texture_top;

	// check wall type
	if(wall->solid.angle & MARK_PORTAL)
	{
		// check backsector
		if(wall->portal.backsector)
		{
			sector_t *bs = map_sectors + wall->portal.backsector;

			// bottom
			if(zz < bs->floor.height)
			{
				texture = wall->portal.texture_bot;
				goto do_wall;
			}

			// top
			if(zz > bs->ceiling.height)
				goto do_wall;

			// blocking
			if(!(wall->portal.blocking & hitscan.blockedby))
				return 0;

			texture = 0x80;
		}
	} else
	if((wall->solid.angle & MARK_MID_BITS) == MARK_SPLIT)
	{
		if(zz <= wall->split.height_split)
			texture = wall->split.texture_bot;
	}
#if 0
	// ceiling check
	dd = sec->ceiling.height - hitscan.height;
	if(zz > dd)
		zz = dd;
#endif
do_wall:
	// radius offset
	d0.x -= (wall->solid.dist.y * hitscan.radius) >> 8;
	d0.y += (wall->solid.dist.x * hitscan.radius) >> 8;

do_hit:
	hitscan.dist = dist;

	if(hitscan.blockedby)
	{
		scan_things(cb_attack_thing);
		if(hitscan.thing_pick)
		{
			thing_type_t *info = thing_type + things[hitscan.thing_pick].type;

			hitscan.sector = hitscan.thing_sdx;

			d0.x = (hitscan.sin * (int32_t)hitscan.dist) >> 8;
			d0.x += hitscan.x;
			d0.y = (hitscan.cos * (int32_t)hitscan.dist) >> 8;
			d0.y += hitscan.y;

			zz = hitscan.thing_zz;

			if(!(info->spawn[3] & 0x80))
				hitscan.type = info->spawn[3];

			goto do_spawn;
		}
	} else
		hitscan.thing_pick = 0;

	// check texture
	if(texture == 0xFF)
		return 1;

do_spawn:
	// spawn thing
	tdx = thing_spawn((int32_t)d0.x << 8, (int32_t)d0.y << 8, zz << 8, hitscan.sector, hitscan.type, hitscan.thing_pick);
	if(!tdx)
		return 1;

	th = things + tdx;
	th->target = hitscan.origin;
	th->angle = hitscan.angle ^ 0x80;

	return 1;
}

//
// generic hitscan

void hitscan_func(uint8_t tdx, uint8_t hang, uint32_t (*cb)(wall_combo_t*))
{
	thing_t *th = things + tdx;
	uint8_t sdx = thingsec[tdx][0];
	int16_t x = th->x >> 8;
	int16_t y = th->y >> 8;

	sector_idx = 1;
	sector_list[0] = sdx;

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
				if(cb(wall))
					return;

				if(hitscan.link) // HAX
				{
					sdx = hitscan.link;
					hitscan.link = 0;
					break;
				}

				if(!(wall->solid.angle & MARK_PORTAL))
					return;

				if(!wall->portal.backsector)
					return;

				sdx = wall->portal.backsector;

				sector_list[sector_idx++] = sdx;

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

	hitscan.origin = tdx;

	hitscan.radius = info->alt_radius + 1;
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

	hitscan.link = 0; // HAX

	hitscan_func(tdx, hang, cb_attack);
}

