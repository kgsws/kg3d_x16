#include "inc.h"
#include "defs.h"
#include "list.h"
#include "engine.h"
#include "tick.h"
#include "things.h"
#include "x16g.h"
#include "x16t.h"

typedef struct
{
	kge_xy_vec_t src;
	kge_xy_vec_t dst;
	kge_xy_vec_t dir;
	kge_xy_vec_t norm;
	kge_sector_t *sec;
	struct
	{
		int32_t hit;
		float dist;
		kge_line_t *line;
		kge_xy_vec_t spot;
		kge_xy_vec_t vect;
		kge_xy_vec_t norm;
		kge_xy_vec_t vtx;
	} pick;
	struct
	{
		float botz;
		float topz;
		float height;
		float radius;
		float r2;
	} th;
	float dist;
	kge_vertex_t pointvtx;
	kge_line_t pointline;
} thing_xy_move_t;

//

kge_thing_t *thing_local_player;
kge_thing_t *thing_local_camera;

static thing_xy_move_t xymove;

uint32_t vc_move;

//
static uint32_t recursive_xy_move(kge_thing_t *thing, kge_sector_t *sec, uint32_t recursion);

//
// functions

static void sector_transition(kge_thing_t *thing, kge_xy_vec_t *origin)
{
	kge_sector_t *sec = thing->pos.sector;
	kge_xy_vec_t vec;

	vec.x = thing->pos.x - origin->x;
	vec.y = thing->pos.y - origin->y;

	vc_move++;

	while(1)
	{
		kge_sector_t *pick_s = NULL;
		kge_line_t *pick_l = NULL;
		float pick_d = 1.0f;

		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = sec->line + i;
			uint32_t s0, s1;
			float d;

			if(line->vc.move == vc_move)
				continue;

			if(!line->backsector)
			{
				line->vc.move = vc_move;
				continue;
			}

			// reject lines not crossed
			if(kge_line_point_side(line, thing->pos.x, thing->pos.y) >= 0.0f)
			{
				line->vc.move = vc_move;
				continue;
			}
			if(kge_line_point_side(line, origin->x, origin->y) < 0.0f)
			{
				line->vc.move = vc_move;
				continue;
			}

			// actually check line cross
			d = kge_divline_cross(origin->xy, vec.xy, line->vertex[0]->xy, line->stuff.dist.xy);
			if(d >= 0.0f && d <= 1.0f)
			{
				// get cross distance
				d = kge_divline_cross(line->vertex[0]->xy, line->stuff.dist.xy, origin->xy, vec.xy);
				if(d >= 0.0f && d < pick_d)
				{
					pick_d = d;
					pick_l = line;
					pick_s = line->backsector;
				}
			}
		}

		if(!pick_s)
			break;

		pick_l->vc.move = vc_move;
		sec = pick_s;
	}

	thing->pos.sector = sec;
}

static uint32_t process_back_sector(kge_thing_t *thing, kge_line_t *line, float *coord, uint32_t recursion)
{
	kge_sector_t *bs = line->backsector;
	float floorz, ceilingz;

	// check back sector
	if(!bs)
		return 1;

	floorz = bs->plane[PLANE_BOT].height;
	if(bs->plane[PLANE_BOT].link)
		floorz -= xymove.th.height;

	ceilingz = bs->plane[PLANE_TOP].height;
	if(bs->plane[PLANE_TOP].link)
		ceilingz += xymove.th.height;

	if(ceilingz - floorz < xymove.th.height)
		return 1;

	if(xymove.th.botz < floorz)
		return 1;

	if(xymove.th.topz > ceilingz)
		return 1;

	if(bs->vc.move != vc_move && recursive_xy_move(thing, bs, recursion + 1))
		return 2;

	return 0;
}

static uint32_t recursive_xy_move(kge_thing_t *thing, kge_sector_t *sec, uint32_t recursion)
{
	kge_line_t *first;

	if(recursion > MAX_COLLISION_RECURSION)
	{
		xymove.pick.hit = -1;
		xymove.pick.dist = 0;
		return 1;
	}

	sec->vc.move = vc_move;

	// store first line
	first = sec->line;

	// check all lines
	for(uint32_t i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = sec->line + i;
		kge_line_t *ln;
		kge_vertex_t *vn;
		kge_xy_vec_t vtx;
		kge_vertex_t coord;
		float d0, d1;

		// check next line
		if(i < sec->line_count-1)
			ln = line + 1;
		else
			ln = first;

		// check for split
		if(ln->vertex[0] != line->vertex[1])
		{
			kge_line_t *tmp = ln;
			ln = first;
			first = tmp;
		}

		vn = ln->vertex[1];

		// check angle
		if(line->backsector)
			d0 = -1.0f;
		else
			d0 = kge_line_point_side(line, vn->x, vn->y);
		if(d0 <= 0.0f)
		{
			kge_vertex_t *vv = line->vertex[0];

			if(	d0 < 0.0f ||
				(vv->x == vn->x && vv->y == vn->y) // very rare special case
			){
				vv = line->vertex[1];

				// check direction
				if(kge_divline_point_side(xymove.src.xy, xymove.norm.xy, vv->x, vv->y) > 0)
				{
					float t, d;

					t = xymove.dir.x * (vv->x - xymove.src.x) + xymove.dir.y * (vv->y - xymove.src.y);

					// check distance
					if(t - 0.01f - thing->prop.radius < xymove.dist)
					{
						// check circle collision
						coord.x = xymove.src.x + xymove.dir.x * t;
						coord.x -= vv->x;
						coord.y = xymove.src.y + xymove.dir.y * t;
						coord.y -= vv->y;

						d = coord.x * coord.x + coord.y * coord.y;
						if(d <= xymove.th.r2)
						{
							kge_vertex_t vect;

							d = t - sqrtf(xymove.th.r2 - d);
							d /= xymove.dist;

							if(d <= xymove.pick.dist)
							{
								uint32_t blocked;

								// get collision spot
								coord.x = xymove.src.x + thing->mom.x * d;
								coord.y = xymove.src.y + thing->mom.y * d;

								// check first line
								blocked = process_back_sector(thing, line, coord.xy, recursion);
								if(blocked == 2)
									return 1;

								// check next line
								if(!blocked)
								{
									blocked = process_back_sector(thing, ln, coord.xy, recursion);
									if(blocked == 2)
										return 1;
								}

								if(blocked)
								{
									// found closer hit
									xymove.pick.hit = 2;
									xymove.pick.line = NULL; // TODO: pick a line?
									xymove.pick.dist = d;

									xymove.pick.spot.x = coord.x;
									xymove.pick.spot.y = coord.y;

									xymove.pick.norm.x = xymove.pick.spot.x - vv->x;
									xymove.pick.norm.y = xymove.pick.spot.y - vv->y;

									d = sqrtf(xymove.pick.norm.x * xymove.pick.norm.x + xymove.pick.norm.y * xymove.pick.norm.y);

									xymove.pick.norm.x /= d;
									xymove.pick.norm.y /= d;

									xymove.pick.vect.x = xymove.pick.norm.y;
									xymove.pick.vect.y = -xymove.pick.norm.x;

									xymove.pick.vtx.x = vv->x;
									xymove.pick.vtx.y = vv->y;
								}
							}
						}
					}
				}
			}
		}

		// offset line by thing radius
		vtx.x = line->vertex[0]->x + line->stuff.normal.x * thing->prop.radius;
		vtx.y = line->vertex[0]->y + line->stuff.normal.y * thing->prop.radius;

		// reject lines without possible collision
		if(kge_divline_point_side(vtx.xy, line->stuff.dist.xy, xymove.dst.x, xymove.dst.y) > 0.0f)
			continue;

		// reject lines this thing starts behind
		if(kge_divline_point_side(vtx.xy, line->stuff.dist.xy, xymove.src.x, xymove.src.y) < 0.0f)
		{
			// check if inside line area
			if(	kge_line_point_side(line, xymove.src.x, xymove.src.y) > 0.0f &&
				kge_divline_point_side(line->vertex[0]->xy, line->stuff.normal.xy, xymove.src.x, xymove.src.y) <= 0.0f &&
				kge_divline_point_side(line->vertex[1]->xy, line->stuff.normal.xy, xymove.src.x, xymove.src.y) >= 0.0f
			){
				// get collision spot
				d0 = xymove.th.radius - kge_line_point_distance(line, xymove.src.x, xymove.src.y);
				coord.x = thing->pos.x + line->stuff.normal.x * d0;
				coord.y = thing->pos.y + line->stuff.normal.y * d0;
				// fake collision distance
				d1 = -1.0f;
				goto do_collide;
			}
			continue;
		}

		// check for actual collision
		d0 = kge_divline_cross(xymove.src.xy, thing->mom.xyz, vtx.xy, line->stuff.dist.xy);
		if(d0 < 0.0f || d0 > 1.0f)
			continue;

		// get collision distance
		d1 = kge_divline_cross(vtx.xy, line->stuff.dist.xy, xymove.src.xy, thing->mom.xyz);
do_collide:
		if(d1 <= xymove.pick.dist)
		{
			uint32_t blocked = 1;

			// get collision spot
			if(d1 >= 0.0f)
			{
				coord.x = thing->pos.x + thing->mom.x * d1;
				coord.y = thing->pos.y + thing->mom.y * d1;
			}

			// check back sectors
			if(line->backsector)
			{
				// check first line
				blocked = process_back_sector(thing, line, coord.xy, recursion);
				if(blocked == 2)
					return 1;
			}

			if(blocked)
			{
				// found closer hit
				xymove.pick.hit = 1;
				xymove.pick.line = line;
				xymove.pick.dist = d1;
				xymove.pick.spot = vtx;
				xymove.pick.vect = line->stuff.dist;
				xymove.pick.norm = line->stuff.normal;
				xymove.pick.spot.x = coord.x;
				xymove.pick.spot.y = coord.y;
			}
		}
	}

	if(xymove.pick.dist <= 0.0f)
		return 1;

	return 0;
}

static int32_t xy_move(kge_thing_t *thing)
{
	int32_t ret = 0;

	// prepare
	vc_move++;
	xymove.src.x = thing->pos.x;
	xymove.src.y = thing->pos.y;
	xymove.dst.x = thing->pos.x + thing->mom.x;
	xymove.dst.y = thing->pos.y + thing->mom.y;
	xymove.sec = thing->pos.sector;
	xymove.pick.hit = 0;
	xymove.pick.dist = 1.0f;
	xymove.pick.line = NULL;

	xymove.dist = sqrtf(thing->mom.x * thing->mom.x + thing->mom.y * thing->mom.y);

	xymove.dir.x = thing->mom.x / xymove.dist;
	xymove.dir.y = thing->mom.y / xymove.dist;
	xymove.norm.x = xymove.dir.y;
	xymove.norm.y = -xymove.dir.x;

	xymove.th.botz = thing->pos.z;
	xymove.th.topz = thing->pos.z + thing->prop.height;
	xymove.th.height = thing->prop.height;
	xymove.th.radius = thing->prop.radius;
	xymove.th.r2 = thing->prop.radius * thing->prop.radius;

	// check for collisions
	recursive_xy_move(thing, thing->pos.sector, 0);
	if(xymove.pick.hit < 0)
	{
		// out of recursions
		// do not move
		return -1;
	} else
	if(xymove.pick.hit)
	{
		kge_vertex_t vtx;
		float step = 0.0001f;
		float sx, sy;

		// blocked
		thing->pos.x = xymove.pick.spot.x;
		thing->pos.y = xymove.pick.spot.y;

		// check sector transitions
		sector_transition(thing, &xymove.src);

		// backup, again
		xymove.src.x = thing->pos.x;
		xymove.src.y = thing->pos.y;

		// step size
		sx = xymove.pick.norm.x * step;
		sy = xymove.pick.norm.y * step;

		// correct position
		if(xymove.pick.hit == 1)
		{
			// TODO: this might cause anoter collision and transition; check again?
			do
			{
				float nx, ny;

				while(1)
				{
					nx = thing->pos.x + sx;
					ny = thing->pos.y + sy;

					if(thing->pos.x != nx || thing->pos.y != ny)
						break;

					// step does not seem to change the position (float precision issue)
					step += 0.0001f;
					sx = xymove.pick.norm.x * step;
					sy = xymove.pick.norm.y * step;
				}

				thing->pos.x = nx;
				thing->pos.y = ny;

				// make sure this point is not behind the line
			} while(kge_line_point_distance(xymove.pick.line, thing->pos.x, thing->pos.y) <= xymove.th.radius);
		} else
		{
			// TODO: this might cause anoter collision and transition; check again?
			do
			{
				float nx, ny;

				while(1)
				{
					nx = thing->pos.x + sx;
					ny = thing->pos.y + sy;

					if(thing->pos.x != nx || thing->pos.y != ny)
						break;

					// step does not seem to change the position (float precision issue)
					step += 0.0001f;
					sx = xymove.pick.norm.x * step;
					sy = xymove.pick.norm.y * step;
				}

				thing->pos.x = nx;
				thing->pos.y = ny;

				// make sure this point is outside of the circle
			} while(kge_point_point_dist2(thing->pos.xyz, xymove.pick.vtx.xy) <= xymove.th.r2);
		}

		// check sector transitions, again
		if(xymove.src.x != thing->pos.x || xymove.src.y != thing->pos.y)
			sector_transition(thing, &xymove.src);

		ret = 1;
	} else
	{
		// not blocked
		thing->pos.x = xymove.dst.x;
		thing->pos.y = xymove.dst.y;
		// check sector transitions
		sector_transition(thing, &xymove.src);
		//
		ret = 0;
	}

	// position fallback if something fails
	// just keep thing inside the sector box

	if(thing->pos.x < thing->pos.sector->stuff.box[BOX_XMIN])
	{
		thing->pos.x = thing->pos.sector->stuff.box[BOX_XMIN];
		ret = -1;
	} else
	if(thing->pos.x > thing->pos.sector->stuff.box[BOX_XMAX])
	{
		thing->pos.x = thing->pos.sector->stuff.box[BOX_XMAX];
		ret = -1;
	}

	if(thing->pos.y < thing->pos.sector->stuff.box[BOX_YMIN])
	{
		thing->pos.y = thing->pos.sector->stuff.box[BOX_YMIN];
		ret = -1;
	} else
	if(thing->pos.y > thing->pos.sector->stuff.box[BOX_YMAX])
	{
		thing->pos.y = thing->pos.sector->stuff.box[BOX_YMAX];
		ret = -1;
	}

	return ret;
}

static void recursive_update_sector(kge_thing_t *thing, kge_sector_t *sec, uint32_t forced, uint32_t recursion)
{
	float floorz;
	float ceilingz;

	engine_add_sector_link(thing, sec);

	sec->vc.move = vc_move;

	for(uint32_t i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = sec->line + i;
		float dist;

		if(!line->backsector)
			continue;

		if(kge_point_point_dist2(thing->pos.xyz, line->vertex[0]->xy) < xymove.th.r2)
			goto do_check;

		if(kge_point_point_dist2(thing->pos.xyz, line->vertex[1]->xy) < xymove.th.r2)
			goto do_check;

		dist = kge_line_point_distance(line, thing->pos.x, thing->pos.y);
		if(dist < 0)
			continue;
		if(dist >= thing->prop.radius)
			continue;

		dist = kge_divline_cross(thing->pos.xyz, line->stuff.normal.xy, line->vertex[0]->xy, line->stuff.dist.xy);
		if(dist < 0.0f || dist >= 1.0f)
			continue;

do_check:
		if(line->backsector->vc.move != vc_move)
		{
			kge_sector_t *bs = line->backsector;
			float floorz, ceilingz;

			floorz = bs->plane[PLANE_BOT].height;
			if(bs->plane[PLANE_BOT].link)
				floorz -= xymove.th.height;

			ceilingz = bs->plane[PLANE_TOP].height;
			if(bs->plane[PLANE_TOP].link)
				ceilingz += xymove.th.height;

			if(!forced)
			{
				if(ceilingz - floorz < xymove.th.height)
					continue;

				if(xymove.th.botz < floorz)
					continue;

				if(xymove.th.topz > ceilingz)
					continue;
			}

			if(floorz > thing->prop.floorz)
				thing->prop.floorz = floorz;

			if(ceilingz < thing->prop.ceilingz)
				thing->prop.ceilingz = ceilingz;

			recursive_update_sector(thing, bs, forced, recursion + 1);
		}
	}
}

//
// thing ticker

uint32_t thing_ticker(kge_thing_t *thing)
{
	uint32_t moved = 0;

	if(!thing->pos.sector)
		return 0;

	// handle player input
	if(thing->ticcmd)
	{
		kge_ticcmd_t *tick_move = thing->ticcmd;

		if(	tick_move->fmove != -32768 &&
			tick_move->smove != -32768 &&
			tick_move->pitch != -32768
		){
			float speed;

			// angles
			thing->pos.angle = tick_move->angle;
			thing->pos.pitch = tick_move->pitch;

			// forward speed
			speed = (float)tick_move->fmove * thing->prop.speed * TICCMD_SPEED_MULT * (1.0f - FRICTION_DEFAULT);
			if(speed)
				thing_thrust(thing, speed, thing->pos.angle, thing->prop.gravity > 0.0f ? 0 : thing->pos.pitch);

			// side speed
			speed = (float)tick_move->smove * thing->prop.speed * TICCMD_SPEED_MULT * (1.0f - FRICTION_DEFAULT);
			if(speed)
				thing_thrust(thing, speed, thing->pos.angle - 0x4000, 0);
		}
	}

	// movement
	if(thing->mom.x || thing->mom.y)
	{
		// X Y
		int32_t ret;

		ret = xy_move(thing);
		moved = ret >= 0;

		if(ret < 0)
		{
			// movement failed; stop
			thing->mom.x = 0;
			thing->mom.y = 0;
		} else
		if(ret > 0)
		{
			// collision detected
			{
				// sliding
				kge_xy_vec_t bkup;
				kge_xy_vec_t diff;
				kge_xy_vec_t slide;
				float dot;

				diff.x = xymove.dst.x - thing->pos.x;
				diff.y = xymove.dst.y - thing->pos.y;

				slide.x = -xymove.pick.norm.y;
				slide.y = xymove.pick.norm.x;

				dot = thing->mom.x * slide.x + thing->mom.y * slide.y;
				slide.x *= dot;
				slide.y *= dot;

				thing->mom.x = slide.x;
				thing->mom.y = slide.y;

				xy_move(thing);
			}
		}
	}

	if(thing->mom.z)
	{
		// Z
		thing->pos.z += thing->mom.z;

		if(thing->pos.z > thing->prop.ceilingz - thing->prop.height)
			thing->pos.z = thing->prop.ceilingz - thing->prop.height;

		if(thing->pos.z < thing->prop.floorz)
			thing->pos.z = thing->prop.floorz;

		moved |= thing_check_links(thing);
	}

	// update sector links
	if(moved)
		thing_update_sector(thing, 0);

//	if(moved & 2)
		thing->prop.viewz = thing->pos.z + thing->prop.viewheight;

	// apply friction and check minimal velocity //

	if(thing->mom.x)
	{
		if(thing->mom.x < MIN_VELOCITY && thing->mom.x > -MIN_VELOCITY)
			thing->mom.x = 0;
		else
			thing->mom.x *= FRICTION_DEFAULT;
	}

	if(thing->mom.y)
	{
		if(thing->mom.y < MIN_VELOCITY && thing->mom.y > -MIN_VELOCITY)
			thing->mom.y = 0;
		else
			thing->mom.y *= FRICTION_DEFAULT;
	}

	if(thing->mom.z)
	{
		if(thing->mom.z < MIN_VELOCITY && thing->mom.z > -MIN_VELOCITY)
			thing->mom.z = 0;
		else
		if(thing->prop.gravity <= 0.0f)
			thing->mom.z *= FRICTION_DEFAULT;
	}

	return 0;
}

//
// API

kge_thing_t *thing_spawn(float x, float y, float z, kge_sector_t *sec)
{
	kge_ticker_t *ticker;
	kge_thing_t *th;

	ticker = tick_create(sizeof(kge_thing_t), &tick_list_normal);
	ticker->info.tick = (void*)thing_ticker;

	th = (kge_thing_t*)&ticker->thing;

	th->pos.x = x;
	th->pos.y = y;
	th->pos.z = z;
	th->pos.sector = sec;

	th->prop.type = THING_TYPE_UNKNOWN;
	th->prop.name[0] = '\t';
	th->prop.radius = 16;
	th->prop.height = 32;

	th->prop.viewheight = 16;

	return th;
}

void thing_remove(kge_thing_t *th)
{
	kge_ticker_t *ticker = (void*)th - sizeof(kge_tickhead_t);

	th->pos.sector = NULL;
	thing_update_sector(th, 0);

	list_del_entry(&tick_list_normal, LIST_ENTRY(ticker));
}

void thing_update_sector(kge_thing_t *thing, uint32_t forced)
{
	sector_link_t *link;

	// unset old poistion
	link = thing->thinglist;
	while(link)
	{
		sector_link_t *next;

		if(link->next_thing)
			link->next_thing->prev_thing = link->prev_thing;

		*link->prev_thing = link->next_thing;

		next = link->next_sector;
		link->next_sector = NULL;

		free(link);

		link = next;
	}
	thing->thinglist = NULL;

	if(!thing->pos.sector)
		return;

	thing->prop.floorz = thing->pos.sector->plane[PLANE_BOT].height;
	if(thing->pos.sector->plane[PLANE_BOT].link)
		thing->prop.floorz -= thing->prop.height;

	thing->prop.ceilingz = thing->pos.sector->plane[PLANE_TOP].height;
	if(thing->pos.sector->plane[PLANE_TOP].link)
		thing->prop.ceilingz += thing->prop.height;

	xymove.th.botz = thing->pos.z;
	xymove.th.topz = thing->pos.z + thing->prop.height;
	xymove.th.r2 = thing->prop.radius * thing->prop.radius;

	engine_link_ptr = &thing->thinglist;

	vc_move++;
	recursive_update_sector(thing, thing->pos.sector, forced, 0);

	engine_link_ptr = NULL;
}

uint32_t thing_check_links(kge_thing_t *thing)
{
	if(	thing->pos.sector->plane[PLANE_TOP].link &&
		thing->pos.z + thing->prop.viewheight > thing->pos.sector->plane[PLANE_TOP].height
	){
		thing->pos.sector = thing->pos.sector->plane[PLANE_TOP].link;
		return 1;
	}

	if(	thing->pos.sector->plane[PLANE_BOT].link &&
		thing->pos.z + thing->prop.viewheight < thing->pos.sector->plane[PLANE_BOT].height
	){
		thing->pos.sector = thing->pos.sector->plane[PLANE_BOT].link;
		return 1;
	}

	return 0;
}

void thing_thrust(kge_thing_t *th, float speed, uint32_t angle, int32_t pitch)
{
	float aaa = ANGLE_TO_RAD(angle);

	if(pitch)
	{
		float vvv = PITCH_TO_RAD(pitch);
		th->mom.z += sinf(vvv) * speed;
		speed *= cosf(vvv);
	}

	th->mom.x -= sinf(aaa) * speed;
	th->mom.y += cosf(aaa) * speed;
}

