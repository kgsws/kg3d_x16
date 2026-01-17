#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "defs.h"
#include "tick.h"
#include "things.h"
#include "hitscan.h"

//
hitscan_t hitscan;

static uint8_t sector_list[256];
static uint8_t sector_idx;

static uint8_t thing_list[256];
static uint8_t thing_idx;

//
// stuff

static uint32_t get_angle(wall_t *wall, int32_t x, int32_t y)
{
	vertex_t *vtx;

	// V0 diff
	vtx = &wall->vtx;
	p2a_coord.x = vtx->x - x;
	p2a_coord.y = vtx->y - y;

	// point angle
	return point_to_angle() >> 4;
}
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
			thing_t *th = thing_ptr(tdx);

			if(	tdx &&
				tdx != hitscan.origin &&
				th->blocking & hitscan.blockedby &&
				!in_thing_list(tdx)
			){
				thing_list[thing_idx] = tdx;

				dd = hitscan_thing_dd(th, &d1);
				if(dd <= 0)
				{
					dt = hitscan_thing_dt(th, &d1);
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

static uint32_t cb_attack(wall_t *wall)
{
	vertex_t d0;
	thing_t *th;
	int32_t dist, dd, zz;
	uint8_t texture, tdx;
	sector_t *sec = map_sectors + hitscan.sector;

	zz = hitscan_wall_pos(wall, &d0, &dist);

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
	texture = wall->top.texture;

	// check wall type
	if(wall->angle & WALL_MARK_EXTENDED)
	{
		// check backsector
		if(wall->backsector)
		{
			sector_t *bs = map_sectors + wall->backsector;

			// bottom
			if(zz < bs->floor.height)
			{
				texture = wall->bot.texture;
				goto do_wall;
			}

			// top
			if(zz > bs->ceiling.height)
				goto do_wall;

			// blocking
			if(!(wall->blocking & hitscan.blockedby))
				return 0;

			texture = 0x80;
		} else
		if(wall->split != 0x8000)
		{
			if(zz <= wall->split)
				texture = wall->bot.texture;
		}
	}
#if 0
	// ceiling check
	dd = sec->ceiling.height - hitscan.height;
	if(zz > dd)
		zz = dd;
#endif
do_wall:
	// radius offset
	d0.x -= (wall->dist.y * hitscan.radius) >> 8;
	d0.y += (wall->dist.x * hitscan.radius) >> 8;

do_hit:
	hitscan.dist = dist;

	if(hitscan.blockedby)
	{
		scan_things(cb_attack_thing);
		if(hitscan.thing_pick)
		{
			thing_type_t *info = thing_type + thing_ptr(hitscan.thing_pick)->ticker.type;

			hitscan.sector = hitscan.thing_sdx;

			d0.x = (hitscan.sin * (int32_t)hitscan.dist) >> 8;
			d0.x += hitscan.x;
			d0.y = (hitscan.cos * (int32_t)hitscan.dist) >> 8;
			d0.y += hitscan.y;

			zz = hitscan.thing_zz;

			if(!(info->spawn[3] & 0x80))
				hitscan.type = info->spawn[3];

			thing_damage(hitscan.thing_pick, hitscan.origin, hitscan.angle, hitscan.damage);

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

	th = thing_ptr(tdx);
	th->target = hitscan.origin;
	th->angle = hitscan.angle;

	return 1;
}

//
// generic hitscan

void hitscan_func(uint8_t tdx, uint8_t hang, uint32_t (*cb)(wall_t*))
{
	thing_t *th = thing_ptr(tdx);
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
		wall_t *wall;
		wall_t *walf;
		uint8_t last_angle;

		if(sec->sobj_hi)
		{
			map_secobj_t *sobj = (map_secobj_t*)(wram + sec->sobj_lo + (sec->sobj_hi & 0x7F) * 65536);

			for(uint32_t i = 0; i < MAX_SOBJ && !(sobj->bank & 0x80); i++, sobj++)
			{
				wall = map_walls[sobj->bank] + sobj->first;
				walf = wall;
				last_angle = get_angle(wall, x, y);

				do
				{
					wall_t *waln = map_walls[sobj->bank] + wall->next;
					uint8_t angle, hit;

					angle = get_angle(waln, x, y);

					hit = angle - hang;
					hit |= hang - last_angle;

					if(!(hit & 0x80))
					{
						hitscan.sector = sdx;
						cb(wall);
						return;
					}

					last_angle = angle;
					wall = waln;

				} while(wall != walf);
			}
		}

		wall = map_walls[sec->wall.bank] + sec->wall.first;
		walf = wall;
		last_angle = get_angle(wall, x, y);

		while(1)
		{
			wall_t *waln = map_walls[sec->wall.bank] + wall->next;
			uint8_t angle, hit;

			angle = get_angle(waln, x, y);

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

				if(!wall->backsector)
					return;

				sdx = wall->backsector;

				sector_list[sector_idx++] = sdx;

				break;
			}

			last_angle = angle;
			wall = waln;

			if(wall == walf)
			{
				// this should never happen
				// THIS CHECK IS NOT PRESENT IN 6502 CODE
				printf("NO HITSCAN WALL!\n");
				return;
			}
		}
	}
}

//
// hitscan bullet attack

void hitscan_attack(uint8_t tdx, uint8_t zadd, uint8_t hang, uint8_t halfpitch, uint8_t type)
{
	thing_t *th = thing_ptr(tdx);
	thing_type_t *info = thing_type + type;

	hitscan.origin = tdx;

	hitscan.radius = info->alt_radius + 1;
	hitscan.height = info->height;
	hitscan.blockedby = info->alt_block;
	hitscan.damage = info->health;

	hitscan.z = th->z >> 8;
	hitscan.z += zadd;

	hitscan_angles(hang, halfpitch);

	if(halfpitch & 0x40)
		halfpitch -= 0x40;
	else
		halfpitch += 0x40;

	hitscan.ptan = tab_tan_hs[halfpitch];

	hitscan.type = type;

	hitscan.link = 0; // HAX

	hitscan_func(tdx, hang, cb_attack);
}

//
// wall hit calculation

void hitscan_angles(uint8_t hang, uint8_t halfpitch)
{
	hitscan.sin = tab_sin[hang];
	hitscan.cos = tab_cos[hang];

	hitscan.axis = (hang + 0x20) << 1;
	if(hitscan.axis & 0x80)
		hitscan.idiv = inv_div[hitscan.sin];
	else
		hitscan.idiv = inv_div[hitscan.cos];

	hitscan.wtan = tab_tan_hs[halfpitch];
}

int32_t hitscan_wall_pos(wall_t *wall, vertex_t *d0, int32_t *dout)
{
	vertex_t *vtx;
	uint8_t angle;
	int32_t dist, dd;

	// V0 diff
	vtx = &wall->vtx;
	d0->x = vtx->x - hitscan.x;
	d0->y = vtx->y - hitscan.y;

	// get distance
	dist = (d0->x * wall->dist.y - d0->y * wall->dist.x) >> 8;

	// get angle
	angle = wall->angle >> 4;
	angle -= hitscan.angle;

	// get location A
	d0->x = hitscan.x;
	d0->x += (wall->dist.y * dist) >> 8;
	d0->y = hitscan.y;
	d0->y -= (wall->dist.x * dist) >> 8;

	// get location B
	dist *= tab_tan_hs[angle];
	dist >>= 8;
	d0->x += (wall->dist.x * dist) >> 8;
	d0->y += (wall->dist.y * dist) >> 8;

	// get range
	if(hitscan.axis & 0x80)
		dist = d0->x - hitscan.x;
	else
		dist = d0->y - hitscan.y;

	dist *= hitscan.idiv * 2;
	dist >>= 8;
	if(dout)
		*dout = dist;

	// get Z
	dd = (dist * hitscan.wtan) >> 8;
	return hitscan.z + dd;
}

int32_t hitscan_thing_dd(thing_t *th, vertex_t *d1)
{
	int32_t dd;

	d1->x = (th->x >> 8) - hitscan.x;
	d1->y = (th->y >> 8) - hitscan.y;

	dd = (d1->x * hitscan.cos - d1->y * hitscan.sin) >> 8;
	if(dd < 0)
		dd = -dd;

	if(th->iflags & THING_IFLAG_ALTRADIUS)
		dd -= thing_type[th->ticker.type].alt_radius;
	else
		dd -= th->radius;

	return dd;
}

int32_t hitscan_thing_dt(thing_t *th, vertex_t *d1)
{
	return (d1->y * hitscan.cos + d1->x * hitscan.sin) >> 8;
}
