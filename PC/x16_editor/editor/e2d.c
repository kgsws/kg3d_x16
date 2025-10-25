#include "inc.h"
#include "defs.h"
#include "engine.h"
#include "system.h"
#include "shader.h"
#include "matrix.h"
#include "render.h"
#include "input.h"
#include "list.h"
#include "tick.h"
#include "image.h"
#include "things.h"
#include "editor.h"
#include "glui.h"
#include "ui_def.h"
#include "x16e.h"
#include "x16g.h"
#include "x16r.h"
#include "x16t.h"

enum
{
	EDIT_MODE_LINE,
	EDIT_MODE_SECTOR,
	EDIT_MODE_THING,
};

typedef struct
{
	struct
	{
		float *x;
		float *y;
	} dst;
	struct
	{
		float x, y;
	} old;
} point_drag_t;

// projection
static float edit_x = 0;
static float edit_y = 0;
static float edit_scale = 1.0f;
static int32_t edit_grid = 128;
static float edge_x0, edge_x1;
static float edge_y0, edge_y1;

// drag & drop
static float drag_map_x = NAN;
static float drag_map_y;
static linked_list_t drag_list_point;
static float drag_item_x;
static float drag_item_y;
static uint32_t drag_input_type;

// planes
static uint32_t plane_mode = 2;

// mode
static uint32_t select_mode;
static int32_t select_mode_tick;

static const char *const mode_text[] =
{
	[EDIT_MODE_LINE] = "Lines",
	[EDIT_MODE_SECTOR] = "Sectors",
	[EDIT_MODE_THING] = "Things",
};

// shapes
static const gl_vertex_t shape_camera[] =
{
	{0.0f, -0.5f, 0.0f},
	{0.0f, 0.5f, 0.0f},
	{0.375f, 0.125f, 0.0f},
	{0.0f, 0.5f, 0.0f},
	{-0.375f, 0.125f, 0.0f},
	{0.0f, 0.5f, 0.0f} // intentional duplicate for highlight
};

//
static void drag_fix_things();
static uint32_t e2d_disconnect_lines(kge_sector_t *sec, kge_line_t *line);

//
// functions

static void location_from_mouse(float *x, float *y, int use_grid)
{
	// find map XY location of mouse pointer
	float mx = (float)(mouse_x - (int32_t)(screen_width / 2)) / edit_scale;
	float my = (float)(mouse_yy - (int32_t)(screen_height / 2)) / edit_scale;

	mx += edit_x;
	my += edit_y;

	if(use_grid)
	{
		int32_t tmp;

		if(mx < 0)
			tmp = (int32_t)(mx - (float)edit_grid / 2) / edit_grid;
		else
			tmp = (int32_t)(mx + (float)edit_grid / 2) / edit_grid;
		mx = tmp * edit_grid;

		if(my < 0)
			tmp = (int32_t)(my - (float)edit_grid / 2) / edit_grid;
		else
			tmp = (int32_t)(my + (float)edit_grid / 2) / edit_grid;
		my = tmp * edit_grid;
	}

	*x = mx;
	*y = my;
}

static uint32_t is_vertex_in_sector(kge_sector_t *sec, kge_vertex_t *vtx)
{
	for(uint32_t i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = sec->line + i;

		if(line->vertex[0] == vtx)
			return 1;

		if(line->vertex[1] == vtx)
			return 1;
	}

	return 0;
}

static uint32_t is_vertex_visible(kge_vertex_t *vtx)
{
	link_entry_t *ent;

	if(!edit_grp_show)
	{
		if(!edit_subgrp_show)
			return 1;
		return is_vertex_in_sector(thing_local_player->pos.sector, vtx);
	}

	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(!edit_is_sector_hidden(sec) && is_vertex_in_sector(sec, vtx))
			return 1;

		ent = ent->next;
	}

	return 0;
}

static kge_vertex_t *find_cursor_vertex(float *found_dist)
{
	link_entry_t *ent;
	float mx, my;
	kge_vertex_t *pick = NULL;
	float dist = INFINITY;
	float eg = (float)edit_grid * 0.75f;

	location_from_mouse(&mx, &my, 0);

	ent = edit_list_vertex.top;
	while(ent)
	{
		float xx, yy;
		kge_vertex_t *vtx = (kge_vertex_t*)(ent + 1);

		xx = vtx->x - mx;
		yy = vtx->y - my;

		if(is_vertex_visible(vtx))
		{
			xx = xx * xx + yy * yy;
			if(xx < dist)
			{
				dist = xx;
				pick = vtx;
			}
		}

		ent = ent->next;
	}

	if(dist > (float)eg * (float)eg)
		pick = NULL;
	else
		if(found_dist)
			*found_dist = sqrtf(dist) / (float)edit_grid;

	return pick;
}

static kge_line_t *find_cursor_line(kge_sector_t **sector)
{
	// TODO: optional - detect lines by vertexes too
	float mx, my;
	link_entry_t *ent;
	kge_line_t *ret = NULL;
	kge_sector_t *ret_sec = NULL;
	float dist_now = edit_grid;

	location_from_mouse(&mx, &my, 0);

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(!edit_is_sector_hidden(sec))
		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = sec->line + i;
			kge_vertex_t *v0, *v1;
			float dist, dx, dy;
			int s0, s1;

			v0 = line->vertex[0];
			v1 = line->vertex[1];

			if(v0->x < edge_x0 && v1->x < edge_x0)
				continue;
			if(v0->y < edge_y0 && v1->y < edge_y0)
				continue;
			if(v0->x > edge_x1 && v1->x > edge_x1)
				continue;
			if(v0->y > edge_y1 && v1->y > edge_y1)
				continue;

			dx = v1->x - v0->x;
			dy = v1->y - v0->y;

			// 'box' check
			s0 = (-(v0->y - v1->y) * (my - v1->y) - (v0->x - v1->x) * (mx - v1->x)) > 0; // v0 side check
			s1 = (-dy * (my - v0->y) - dx * (mx - v0->x)) > 0; // v1 side check
			if(s0 != s1)
				continue;

			// side check
			s0 = edit_is_point_in_sector(mx, my, sec, 0);

			// double sided check
			if(line->backsector && !s0)
				continue; // only select side facing into the sector

			// distance check
			dist = fabs(dx * (v0->y - my) - (v0->x - mx) * dy) / sqrtf(dx*dx + dy*dy);

			// single sided check
			if(!line->backsector && !s0)
				dist += 0.00001f; // add a tiny distance so other side, if any, is preffered

			if(dist < dist_now)
			{
				dist_now = dist;
				ret = line;
				ret_sec = sec;
			}
		}

		ent = ent->next;
	}

	if(sector)
		*sector = ret_sec;

	return ret;
}

static kge_sector_t *find_cursor_sector()
{
	link_entry_t *ent;
	float mx, my;

	location_from_mouse(&mx, &my, 0);

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(edit_is_point_in_sector(mx, my, sec, 1))
			return sec;

		ent = ent->next;
	}

	return NULL;
}

static kge_thing_t *find_cursor_thing()
{
	float mx, my;
	link_entry_t *ent = tick_list_normal.top;
	kge_thing_t *pick = NULL;
	float dist = INFINITY;

	location_from_mouse(&mx, &my, 0);

	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			float xx, yy;
			kge_thing_t *th = (kge_thing_t*)tickable->data;

			xx = th->pos.x - mx;
			yy = th->pos.y - my;

			xx = xx * xx + yy * yy;
			if(xx < dist)
			{
				dist = xx;
				pick = th;
			}
		}

		ent = ent->next;
	}

	if(dist > (float)edit_grid * (float)edit_grid)
		pick = NULL;

	return pick;
}

static void check_cursor_highlight()
{
	// highlight also affects drag & drop
	float dist;

	edit_clear_hit(0);

	if(edit_list_draw_new.top)
		return;

	switch(select_mode)
	{
		case EDIT_MODE_LINE:
			edit_hit.vertex = find_cursor_vertex(&dist);
			if(!edit_hit.vertex || dist >= 0.333333f)
			{
				edit_hit.line = find_cursor_line(&edit_hit.extra_sector);
				if(edit_hit.line)
					edit_hit.vertex = NULL;
			}
		break;
		case EDIT_MODE_SECTOR:
			edit_hit.sector = find_cursor_sector();
		break;
		case EDIT_MODE_THING:
			edit_hit.thing = find_cursor_thing();
		break;
	}

	edit_highlight_changed();
}

static void prep_rect(float x, float y, float w, float h)
{
	w += x;
	h += y;

	gl_vertex_buf[0].x = x;
	gl_vertex_buf[0].y = y;
	gl_vertex_buf[0].z = DEPTH_TEXT;
	gl_vertex_buf[0].s = 0.0f;
	gl_vertex_buf[0].t = 1.0f;
	gl_vertex_buf[1].x = w;
	gl_vertex_buf[1].y = y;
	gl_vertex_buf[1].z = DEPTH_TEXT;
	gl_vertex_buf[1].s = 1.0f;
	gl_vertex_buf[1].t = 1.0f;
	gl_vertex_buf[2].x = x;
	gl_vertex_buf[2].y = h;
	gl_vertex_buf[2].z = DEPTH_TEXT;
	gl_vertex_buf[2].s = 0.0f;
	gl_vertex_buf[2].t = 0.0f;
	gl_vertex_buf[3].x = w;
	gl_vertex_buf[3].y = h;
	gl_vertex_buf[3].z = DEPTH_TEXT;
	gl_vertex_buf[3].s = 1.0f;
	gl_vertex_buf[3].t = 0.0f;
}

//
// sector stuff

static int32_t e2d_area_angle()
{
	// returns 1 if CW, 0 if CCW
	// returns -1 if invalid
	link_entry_t *ent;
	kge_vertex_t *v[3];
	float yy = -INFINITY;
	float side = 0;

	// check vertex count
	if(edit_list_draw_new.count < 3)
		// invalid
		return -1;

	// count angles
	ent = edit_list_draw_new.top;
	while(ent)
	{
		link_entry_t *ee;
		kge_vertex_t *v0, *v1, *v2;
		float ss, dist[2];

		v0 = (kge_vertex_t*)(ent + 1);

		ee = ent->next;
		if(!ee)
			ee = edit_list_draw_new.top;
		v1 = (kge_vertex_t*)(ee + 1);

		ee = ee->next;
		if(!ee)
			ee = edit_list_draw_new.top;
		v2 = (kge_vertex_t*)(ee + 1);

		if(v1->y > yy)
		{
			dist[0] = v0->x - v1->x;
			dist[1] = v0->y - v1->y;

			ss = kge_divline_point_side(v0->xy, dist, v2->x, v2->y);
			if(ss != 0)
			{
				yy = v1->y;
				side = ss;
			}
		}

		ent = ent->next;
	}

	if(!side)
		return -1;

	return side > 0;
}

static uint32_t e2d_create_sector()
{
	kge_sector_t *sector;
	kge_line_t *line;
	kge_vertex_t *vtx_last;
	link_entry_t *ent;
	int32_t dir;

	// count check
	if(edit_list_sector.count >= 255)
		return 3;

	// size check
	if(edit_list_draw_new.count > EDIT_MAX_SECTOR_LINES)
		return 2;

	// area check
	dir = e2d_area_angle();
	if(dir < 0)
		return 1;

	// create sector
	sector = list_add_entry(&edit_list_sector, sizeof(kge_sector_t) + sizeof(kge_line_t) * EDIT_MAX_SECTOR_LINES);

	line = sector->line;

	memset(sector, 0, sizeof(kge_sector_t));

	sector->light.idx = 0;
	strcpy(sector->light.name, editor_light[0].name);
	sector->plane[PLANE_BOT] = edit_floor_plane;
	sector->plane[PLANE_TOP] = edit_ceiling_plane;
	sector->line_count = edit_list_draw_new.count;

	sector->stuff.group[0] = edit_grp_show;
	sector->stuff.group[1] = edit_subgrp_show;

	// go trough vertexes
	if(dir)
		ent = edit_list_draw_new.cur;
	else
		ent = edit_list_draw_new.top;

	while(ent)
	{
		kge_vertex_t *vtx;
		kge_vertex_t *pt = (kge_vertex_t*)(ent + 1); // current point

		// new vertex (V0)
		vtx = list_add_entry(&edit_list_vertex, sizeof(kge_vertex_t));
		*vtx = *pt;
		vtx->vc_move = 0;
		vtx->vc_editor = 0;

		memset(line, 0, sizeof(kge_line_t));

		line->vertex[0] = vtx;
		line->frontsector = sector;
		line->texture[0] = edit_line_default;
		line->texture[1] = edit_empty_texture;
		line->texture[2] = edit_empty_texture;
		line->texture_split = INFINITY;

		if(dir)
			ent = ent->prev;
		else
			ent = ent->next;

		line++;
	}

	// fill V1
	vtx_last = sector->line->vertex[0];
	line = sector->line + sector->line_count;
	for(uint32_t i = 0; i < sector->line_count; i++)
	{
		line--;

		line->vertex[1] = vtx_last;
		vtx_last = line->vertex[0];

		engine_update_line(line);
	}

	engine_update_sector(sector);

	edit_place_thing_in_sector(edit_camera_thing, sector);

	return 0;
}

static void e2d_delete_sector(kge_sector_t *sec)
{
	link_entry_t *ent = tick_list_normal.top;

	// unlink
	if(sec->plane[PLANE_TOP].link)
	{
		sec->plane[PLANE_TOP].link->plane[PLANE_BOT].link = NULL;
		sec->plane[PLANE_TOP].link = NULL;
	}
	if(sec->plane[PLANE_BOT].link)
	{
		sec->plane[PLANE_BOT].link->plane[PLANE_TOP].link = NULL;
		sec->plane[PLANE_BOT].link = NULL;
	}

	// go trough lines
	for(uint32_t i = 0; i < sec->line_count; i++)
		e2d_disconnect_lines(sec, sec->line + i);

	// go trough lines
	for(uint32_t i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = sec->line + i;
		list_del_entry(&edit_list_vertex, LIST_ENTRY(line->vertex[0]));
	}

	// go trough things
	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable) && tickable->thing.pos.sector == sec)
		{
			tickable->thing.pos.sector = NULL;
			thing_update_sector(&tickable->thing, 1);
		}

		ent = ent->next;
	}	

	// delete sector
	list_del_entry(&edit_list_sector, LIST_ENTRY(sec));
}

static void e2d_replace_vertex(kge_vertex_t *vA, kge_vertex_t *vB)
{
	/// replace every reference to VA with VB
	link_entry_t *ent;
//	editor_selection_t *sel;

	if(vA == vB)
		return;

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		// go trough lines
		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = sec->line + i;
			uint32_t fix = 0;

			if(line->vertex[0] == vA)
			{
				line->vertex[0] = vB;
				fix = 1;
			}

			if(line->vertex[1] == vA)
			{
				line->vertex[1] = vB;
				fix = 1;
			}

			if(fix)
			{
				engine_update_line(line);
				engine_update_sector(sec);
			}
		}

		ent = ent->next;
	}
/*
	// remove from selection TODO
	sel = editor_find_in_selection(vA);
	if(sel)
		list_del_entry(&list_selection, LIST_ENTRY(sel));
*/
	// delete
	list_del_entry(&edit_list_vertex, LIST_ENTRY(vA));
}

static uint32_t e2d_check_vtx_usage(kge_vertex_t *vtx, kge_sector_t *ignore)
{
	/// find if any sector uses specified vertex
	link_entry_t *ent;

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(sec != ignore)
		{
			// go trough lines
			for(uint32_t i = 0; i < sec->line_count; i++)
			{
				kge_line_t *line = sec->line + i;

				if(line->vertex[0] == vtx)
					return 1;

				if(line->vertex[1] == vtx)
					return 1;
			}
		}

		ent = ent->next;
	}

	return 0;
}

static uint32_t e2d_find_connect_line(kge_sector_t *front, kge_line_t *ln, kge_sector_t **sret, kge_line_t **lret)
{
	/// find sector with matching line for backsector
	link_entry_t *ent;

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		// go trough all lines
		if(	sec != front &&
			!edit_is_sector_hidden(sec)
		){
			kge_line_t *ll = NULL;
			kge_vertex_t *vA = ln->vertex[0];
			kge_vertex_t *vB = ln->vertex[1];

			// find line in sector with matching vertex coords
			for(uint32_t i = 0; i < sec->line_count; i++)
			{
				kge_line_t *line = sec->line + i;
				kge_vertex_t *v0;
				kge_vertex_t *v1;

				if(line->backsector)
					continue;

				v0 = line->vertex[0];
				v1 = line->vertex[1];

				if(	vA->x == v1->x && vA->y == v1->y &&
					vB->x == v0->x && vB->y == v0->y
				){
					ll = line;
					break;
				}
			}

			if(ll)
			{
				*sret = sec;
				*lret = ll;
				return 0;
			}
		}

		ent = ent->next;
	}

	return 1;
}

kge_line_t *e2d_find_other_line(kge_sector_t *sec, kge_sector_t *os, kge_line_t *ol)
{
	// find line in sector with matching vertex coords
	for(uint32_t i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = sec->line + i;

		if(line->backsector != os)
			continue;

		if(	line->vertex[0] != ol->vertex[1] ||
			line->vertex[1] != ol->vertex[0]
		)
			continue;

		return line;
	}

	return NULL;
}

static uint32_t e2d_connect_lines(kge_sector_t *sec, kge_line_t *line)
{
	/// connect two sectors (create two-sided line)
	kge_line_t *ln;
	kge_sector_t *ss;
	uint32_t ret;

	// check limit
	if(line->backsector)
		return 1;

	// find other side
	if(e2d_find_connect_line(sec, line, &ss, &ln))
		return 1;

	// vertex replacement
	e2d_replace_vertex(line->vertex[0], ln->vertex[1]);
	e2d_replace_vertex(line->vertex[1], ln->vertex[0]);

	// link sectors
	line->backsector = ss;
	ln->backsector = sec;

	line->texture[0] = edit_empty_texture;
	line->texture[1] = edit_empty_texture;
	line->texture[2] = edit_empty_texture;
	line->texture_split = INFINITY;
	line->info.blocking = 0;
	line->info.blockmid = 0;

	ln->texture[0] = edit_empty_texture;
	ln->texture[1] = edit_empty_texture;
	ln->texture[2] = edit_empty_texture;
	ln->texture_split = INFINITY;
	ln->info.blocking = 0;
	ln->info.blockmid = 0;

	return 0;
}

static uint32_t e2d_connect_sectors(kge_sector_t *sector)
{
	// NOTE: this will connect even to hidden sectors
	link_entry_t *ent;

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(edit_check_sector_overlap(sec, sector))
		{
			if(sec->plane[PLANE_TOP].height == sector->plane[PLANE_BOT].height)
			{
				sec->plane[PLANE_TOP].link = sector;
				sector->plane[PLANE_BOT].link = sec;
			} else
			{
				sec->plane[PLANE_BOT].link = sector;
				sector->plane[PLANE_TOP].link = sec;
			}

			return 0;
		}

		ent = ent->next;
	}

	return 1;
}

static void e2d_make_onesided(kge_sector_t *sec, kge_line_t *line)
{
	kge_line_t *lA, *lB;
	kge_vertex_t *vtx;

	lA = line - 1;
	if(lA < sec->line)
		lA += sec->line_count;

	lB = line + 1;
	if(lB >= sec->line + sec->line_count)
		lB = sec->line;

	if(!lA->backsector)
	{
		if(e2d_check_vtx_usage(line->vertex[0], sec))
		{
			vtx = list_add_entry(&edit_list_vertex, sizeof(kge_vertex_t));
			*vtx = *line->vertex[0];
			vtx->vc_move = 0;
			vtx->vc_editor = 0;
			lA->vertex[1] = vtx;
			line->vertex[0] = vtx;
		}
	}

	if(!lB->backsector)
	{
		if(e2d_check_vtx_usage(line->vertex[1], sec))
		{
			vtx = list_add_entry(&edit_list_vertex, sizeof(kge_vertex_t));
			*vtx = *line->vertex[1];
			vtx->vc_move = 0;
			vtx->vc_editor = 0;
			lB->vertex[0] = vtx;
			line->vertex[1] = vtx;
		}
	}

	line->backsector = NULL;
	line->texture[0] = edit_empty_texture;
	line->texture[1] = edit_empty_texture;
	line->texture[2] = edit_empty_texture;

	line->info.blocking = 0;
	line->info.blockmid = 0;
}

static uint32_t e2d_disconnect_lines(kge_sector_t *sec, kge_line_t *line)
{
	/// disconnect two sectors
	kge_line_t *other_line;
	kge_sector_t* other_sec;

	// check connection
	if(!line->backsector)
		return 1;

	other_sec = line->backsector;
	other_line = e2d_find_other_line(other_sec, sec, line);

	e2d_make_onesided(sec, line);

	if(other_line)
		e2d_make_onesided(other_sec, other_line);

	return 0;
}

static uint32_t e2d_split_line(kge_sector_t *sec, kge_line_t *line, float x, float y)
{
	/// split existing line (insert vertex)
	kge_vertex_t *vtx;
	uint32_t idx;

	// check limit
	if(sec->line_count >= EDIT_MAX_SECTOR_LINES)
		return 1;

	// check backsector
	if(line->backsector)
		return 2;

	// check vertexes
	if(	(line->vertex[0]->x == x && line->vertex[0]->y == y) ||
		(line->vertex[1]->x == x && line->vertex[1]->y == y)
	)
		return 3;

	// new vertex
	vtx = list_add_entry(&edit_list_vertex, sizeof(kge_vertex_t));
	vtx->vc_move = 0;
	vtx->vc_editor = 0;
	vtx->x = x;
	vtx->y = y;

	// copy lines
	idx = line - sec->line;
	for(uint32_t i = sec->line_count; i > idx; i--)
	{
		kge_line_t *ldst = sec->line + i;
		kge_line_t *lsrc = ldst - 1;

		*ldst = *lsrc;
	}
	sec->line_count++;

	// new vertex
	line[0].vertex[1] = vtx;
	line[1].vertex[0] = vtx;

	// update
	engine_update_line(line + 0);
	engine_update_line(line + 1);
	engine_update_sector(sec);

	// fix things
	vc_editor++;
	edit_mark_sector_things(sec);
	drag_fix_things();

	edit_place_thing_in_sector(edit_camera_thing, sec);

	return 0;
}

//
// drawing

static void draw_grid()
{
	apply_4f(shader_buffer.shading.color, editor_color[EDITCOLOR_GRID].color);
	shader_changed = 1;
	shader_update();

	if((float)edit_grid * edit_scale > 1.0f)
	{
		float af, ab;
		float step = (float)edit_grid * edit_scale;

		// grid Y
		af = (screen_height / 2) - step;
		af -= (edit_y - ((int32_t)edit_y / edit_grid) * edit_grid) * edit_scale;
		ab = af;
		while(af < screen_height)
		{
			ab -= step;

			gl_vertex_buf[0].x = 0;
			gl_vertex_buf[0].y = af;
			gl_vertex_buf[0].z = DEPTH_GRID;

			gl_vertex_buf[1].x = screen_width;
			gl_vertex_buf[1].y = af;
			gl_vertex_buf[1].z = DEPTH_GRID;

			gl_vertex_buf[2].x = 0;
			gl_vertex_buf[2].y = ab;
			gl_vertex_buf[2].z = DEPTH_GRID;

			gl_vertex_buf[3].x = screen_width;
			gl_vertex_buf[3].y = ab;
			gl_vertex_buf[3].z = DEPTH_GRID;

			glDrawArrays(GL_LINES, 0, 4);

			af += step;
		}

		// grid X
		af = (screen_width / 2) - step;
		af -= (edit_x - ((int32_t)edit_x / edit_grid) * edit_grid) * edit_scale;
		ab = af;
		while(af < screen_width)
		{
			ab -= step;

			gl_vertex_buf[0].x = af;
			gl_vertex_buf[0].y = 0;
			gl_vertex_buf[0].z = DEPTH_GRID;

			gl_vertex_buf[1].x = af;
			gl_vertex_buf[1].y = screen_height;
			gl_vertex_buf[1].z = DEPTH_GRID;

			gl_vertex_buf[2].x = ab;
			gl_vertex_buf[2].y = 0;
			gl_vertex_buf[2].z = DEPTH_GRID;

			gl_vertex_buf[3].x = ab;
			gl_vertex_buf[3].y = screen_height;
			gl_vertex_buf[3].z = DEPTH_GRID;

			glDrawArrays(GL_LINES, 0, 4);

			af += step;
		}
	}
}

static void draw_origin()
{
	apply_4f(shader_buffer.shading.color, editor_color[EDITCOLOR_ORIGIN].color);
	shader_changed = 1;
	shader_update();

	gl_vertex_buf[0].x = 16.0f / -edit_scale;
	gl_vertex_buf[0].y = 0;
	gl_vertex_buf[0].z = DEPTH_GRID;

	gl_vertex_buf[1].x = 16.0f / edit_scale;
	gl_vertex_buf[1].y = 0;
	gl_vertex_buf[1].z = DEPTH_GRID;

	gl_vertex_buf[2].x = 0;
	gl_vertex_buf[2].y = 16.0f / -edit_scale;
	gl_vertex_buf[2].z = DEPTH_GRID;

	gl_vertex_buf[3].x = 0;
	gl_vertex_buf[3].y = 16.0f / edit_scale;
	gl_vertex_buf[3].z = DEPTH_GRID;

	glDrawArrays(GL_LINES, 0, 4);
}

static void draw_vertex(kge_vertex_t *vtx, float intensity, uint32_t color, float depth)
{
	float offset;
	float *colf = editor_color[color].color;
/*
	if(!point_in_view(vtx))
		return;
*/
	offset = EDIT_VERTEX_SIZE / edit_scale;

	gl_vertex_buf[0].x = vtx->x + offset;
	gl_vertex_buf[0].y = vtx->y + offset;
	gl_vertex_buf[0].z = depth;

	gl_vertex_buf[1].x = vtx->x - offset;
	gl_vertex_buf[1].y = vtx->y + offset;
	gl_vertex_buf[1].z = depth;

	gl_vertex_buf[2].x = vtx->x - offset;
	gl_vertex_buf[2].y = vtx->y - offset;
	gl_vertex_buf[2].z = depth;

	gl_vertex_buf[3].x = vtx->x + offset;
	gl_vertex_buf[3].y = vtx->y - offset;
	gl_vertex_buf[3].z = depth;

	shader_buffer.shading.color[0] = colf[0] * intensity;
	shader_buffer.shading.color[1] = colf[1] * intensity;
	shader_buffer.shading.color[2] = colf[2] * intensity;

	glDepthFunc(GL_LESS);
	glLineWidth(EDIT_HIGHLIGHT_LINE_WIDTH);

	if(vtx == edit_hit.vertex)
	{
//		if(editor_find_in_selection(vtx))
//			shader_buffer.shading.color[3] = edit_highlight_alpha + EDIT_HIGHLIGHT_ALPHA_OFFSET + EDIT_SELECTION_ALPHA;
//		else
			shader_buffer.shading.color[3] = edit_highlight_alpha + EDIT_HIGHLIGHT_ALPHA_OFFSET;
		shader_changed = 1;
		shader_update();
		glDrawArrays(GL_LINE_LOOP, 0, 4);
	}/* else
	if(editor_find_in_selection(vtx))
	{
		shader_buffer.shading.color[3] = SELECTION_ALPHA;
		shader_changed = 1;
		shader_update();
		glDrawArrays(GL_LINE_LOOP, 0, 4);
	}
*/
	glLineWidth(1.0f);
	glDepthFunc(GL_LEQUAL);

	shader_buffer.shading.color[3] = 1.0f;
	shader_changed = 1;
	shader_update();
	glDrawArrays(GL_LINE_LOOP, 0, 4);
}

static void draw_line(kge_vertex_t *v0, kge_vertex_t *v1, float depth)
{
	gl_vertex_buf[0].x = v0->x;
	gl_vertex_buf[0].y = v0->y;
	gl_vertex_buf[0].z = depth;

	gl_vertex_buf[1].x = v1->x;
	gl_vertex_buf[1].y = v1->y;
	gl_vertex_buf[1].z = depth;

	glDrawArrays(GL_LINES, 0, 2);
}

static void draw_wall(kge_line_t *line, uint32_t color, float depth, uint32_t highlight)
{
	float intensity;
	float *colf = editor_color[color].color;
	kge_vertex_t *v0 = line->vertex[0];
	kge_vertex_t *v1 = line->vertex[1];

	if(edit_list_draw_new.top)
		intensity = 0.4f;
	else
		intensity = 1.0f;

	// blocking
	if(!highlight && (line->info.blocking || line->info.blockmid))
	{
		glLineWidth(EDIT_HIGHLIGHT_LINE_WIDTH);
		shader_buffer.shading.color[0] = editor_color[EDITCOLOR_LINE_PORTAL].color[0] * intensity;
		shader_buffer.shading.color[1] = editor_color[EDITCOLOR_LINE_PORTAL].color[1] * intensity;
		shader_buffer.shading.color[2] = editor_color[EDITCOLOR_LINE_PORTAL].color[2] * intensity;
		shader_buffer.shading.color[3] = 0.2f;
		shader_changed = 1;
		shader_update();
		draw_line(v0, v1, depth);
		glLineWidth(1.0f);
	}

	// invalid angle
	if(highlight & 4)
	{
		glLineWidth(EDIT_HIGHLIGHT_LINE_WIDTH);
		shader_buffer.shading.color[0] = editor_color[EDITCOLOR_LINE_BAD].color[0] * intensity;
		shader_buffer.shading.color[1] = editor_color[EDITCOLOR_LINE_BAD].color[1] * intensity;
		shader_buffer.shading.color[2] = editor_color[EDITCOLOR_LINE_BAD].color[2] * intensity;
		shader_buffer.shading.color[3] = edit_highlight_alpha + EDIT_HIGHLIGHT_ALPHA_OFFSET_BAD;
		shader_changed = 1;
		shader_update();
		draw_line(v0, v1, depth);
		glLineWidth(1.0f);
	}

	shader_buffer.shading.color[0] = colf[0] * intensity;
	shader_buffer.shading.color[1] = colf[1] * intensity;
	shader_buffer.shading.color[2] = colf[2] * intensity;
	shader_changed = 1;

	// selected line
	if(highlight & 2)
	{
		glLineWidth(EDIT_HIGHLIGHT_LINE_WIDTH);
		shader_buffer.shading.color[3] = EDIT_SELECTION_ALPHA;
		shader_update();
		draw_line(v0, v1, depth);
		glLineWidth(1.0f);
	}

	// highlighted line
	if(highlight & 1)
	{
		glLineWidth(EDIT_HIGHLIGHT_LINE_WIDTH);
		shader_buffer.shading.color[3] = edit_highlight_alpha + EDIT_HIGHLIGHT_ALPHA_OFFSET;
		shader_update();
		draw_line(v0, v1, depth);
		glLineWidth(1.0f);
	}

	shader_buffer.shading.color[3] = 1.0f;
	shader_changed = 1;
	shader_update();

	// line normal
	if(highlight & 3)
	{
		gl_vertex_buf[0].x = line->stuff.center.x;
		gl_vertex_buf[0].y = line->stuff.center.y;
		gl_vertex_buf[0].z = depth;

		gl_vertex_buf[1].x = line->stuff.center.x + line->stuff.normal.x * 12.0f;
		gl_vertex_buf[1].y = line->stuff.center.y + line->stuff.normal.y * 12.0f;
		gl_vertex_buf[1].z = depth;

		glDrawArrays(GL_LINES, 0, 2);
	}

	// the line
	draw_line(v0, v1, depth);

	// first vertex
	draw_vertex(v0, intensity, EDITCOLOR_VERTEX, DEPTH_VERTEX);
}

static void draw_sector(kge_sector_t *sector)
{
	int32_t flat_color = -1;

	if(edit_is_sector_hidden(sector))
		return;

	// plane draw
	if(!(plane_mode & 2))
	{
		uint32_t flags = PLANE_CUSTOMZ(DEPTH_PLANE_B);
/*
		if(edit_list_draw_new.top)
		{
			color_plane_texture[0] = 0.5f;
			color_plane_texture[1] = 0.5f;
			color_plane_texture[2] = 0.5f;
		} else
		{
			color_plane_texture[0] = 0.8f;
			color_plane_texture[1] = 0.8f;
			color_plane_texture[2] = 0.8f;
		}
*/
		shader_buffer.lightmap = LIGHTMAP_IDX(sector->light.idx);

		flags |= plane_mode & 1;
		r_draw_plane(sector, flags);

		shader_mode_color();
		shader_update();
	}

/*	// selection highlight
	if(editor_find_in_selection(sector))
	{
		edit_color_higlight[3] = EDIT_HIGHLIGHT_ALPHA_OFFSET;
		apply_4f(shader_buffer.shading.color, edit_color_higlight);
		draw_plane_overlay(sector, NULL, DEPTH_PLANE_H);
	}
*/
	// cursor highlight check
	if(sector == edit_hit.sector)
	{
		flat_color = EDITCOLOR_SECTOR_HIGHLIGHT;
		if(sector->plane[PLANE_TOP].link || sector->plane[PLANE_BOT].link)
			flat_color = EDITCOLOR_SECTOR_HIGHLIGHT_LINK;
	}

	// sector highlight
	if(flat_color >= 0)
	{
		apply_4f(shader_buffer.shading.color, editor_color[flat_color].color);
		shader_buffer.shading.color[3] = edit_highlight_alpha;
		r_draw_plane(sector, PLANE_TOP | PLANE_USE_COLOR | PLANE_CUSTOMZ(DEPTH_PLANE_H));
	}

	// line draw
	for(uint32_t i = 0; i < sector->line_count; i++)
	{
		kge_line_t *line = sector->line + i;
		kge_line_t *nine = sector->line + (i + 1 < sector->line_count ? i + 1 : 0);
		kge_line_t *pine = sector->line + (i ? i - 1 : sector->line_count - 1);
		kge_vertex_t *v0, *v1;
		uint32_t highlight = 0;
		uint32_t color;

		if(line->backsector)
			color = EDITCOLOR_LINE_PORTAL;
		else
			color = EDITCOLOR_LINE_SOLID;

		v0 = line->vertex[0];
		v1 = line->vertex[1];
		if(v0->x == v1->x && v0->y == v1->y)
		{
			kge_vertex_t *backup = edit_hit.vertex;
			edit_hit.vertex = v0;
			draw_vertex(v0, 1.0f, EDITCOLOR_LINE_BAD, DEPTH_CAMERA);
			edit_hit.vertex = backup;
		}

		if(!edit_list_draw_new.top)
		{
			if(line == edit_hit.line)
				highlight = 1;

//			if(editor_find_in_selection(line))
//				highlight |= 2;

			if(kge_line_point_side(line, nine->vertex[1]->x, nine->vertex[1]->y) < 0.0f)
				highlight |= 4;
			else
			if(kge_line_point_side(pine, line->vertex[1]->x, line->vertex[1]->y) < 0.0f)
				highlight |= 4;

			if(line->backsector && pine->backsector)
				highlight |= 8;
		}

		draw_wall(line, color, DEPTH_WALL, highlight);
	}
}

static void draw_sectors()
{
	link_entry_t *ent = edit_list_sector.top;
	while(ent)
	{
		draw_sector((kge_sector_t*)(ent + 1));
		ent = ent->next;
	}
}

static void draw_thing_shape(kge_thing_t *th, uint32_t count, float depth, uint32_t color, float highlight)
{
	float radius = th->prop.radius;

	if(th == edit_camera_thing)
		radius = 24.0f;
	else
	if(radius < 1.0f)
		radius = 1.0f;

	r_work_matrix[2] = *((matrix3d_t*)shader_buffer.projection.matrix);

	matrix_translate(r_work_matrix + 0, &mat_identity, th->pos.x, th->pos.y, depth);
	matrix_rotate_z(r_work_matrix + 1, r_work_matrix + 0, ANGLE_TO_RAD(th->pos.angle));
	matrix_scale((matrix3d_t*)shader_buffer.projection.extra, r_work_matrix + 1, radius, radius, 1.0f);
	matrix_mult((matrix3d_t*)shader_buffer.projection.matrix, r_work_matrix + 2, (matrix3d_t*)shader_buffer.projection.extra);

	if(th->pos.sector)
		apply_4f(shader_buffer.shading.color, editor_color[color].color);
	else
		apply_4f(shader_buffer.shading.color, editor_color[color+1].color);

	if(edit_list_draw_new.top)
	{
		shader_buffer.shading.color[0] *= 0.5f;
		shader_buffer.shading.color[1] *= 0.5f;
		shader_buffer.shading.color[2] *= 0.5f;
	}

	if(!isnan(highlight))
		shader_buffer.shading.color[3] = highlight;

	shader_changed = 1;
	shader_update();
	glDrawArrays(GL_LINE_STRIP, 0, count);

	*((matrix3d_t*)shader_buffer.projection.matrix) = r_work_matrix[2];
	shader_changed = 1;
}

static void draw_extra()
{
	// camera
	memcpy(gl_vertex_buf, shape_camera, sizeof(shape_camera));

	if(edit_hit.thing == edit_camera_thing)
	{
		glLineWidth(EDIT_HIGHLIGHT_LINE_WIDTH);
		draw_thing_shape(edit_camera_thing, sizeof(shape_camera) / sizeof(gl_vertex_t), DEPTH_CAMERA, EDITCOLOR_CAMERA, edit_highlight_alpha + EDIT_HIGHLIGHT_ALPHA_OFFSET);
		glLineWidth(1.0f);
	}

	draw_thing_shape(edit_camera_thing, sizeof(shape_camera) / sizeof(gl_vertex_t), DEPTH_CAMERA, EDITCOLOR_CAMERA, NAN);
}

static void draw_things()
{
	link_entry_t *ent = tick_list_normal.top;

	memcpy(gl_vertex_buf, edit_circle_vtx, sizeof(edit_circle_vtx));

	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(	IS_THING_TICKER(tickable) &&
			&tickable->thing != edit_camera_thing &&
			!edit_is_sector_hidden(tickable->thing.pos.sector)
		){
			if(edit_hit.thing == &tickable->thing)
			{
				glLineWidth(EDIT_HIGHLIGHT_LINE_WIDTH);
				draw_thing_shape(&tickable->thing, sizeof(edit_circle_vtx) / sizeof(gl_vertex_t), DEPTH_THING, EDITCOLOR_THING, edit_highlight_alpha + EDIT_HIGHLIGHT_ALPHA_OFFSET);
				glLineWidth(1.0f);
			}
			draw_thing_shape(&tickable->thing, sizeof(edit_circle_vtx) / sizeof(gl_vertex_t), DEPTH_THING, EDITCOLOR_THING, NAN);
		}

		ent = ent->next;
	}
}

static void draw_new_lines()
{
	link_entry_t *ent = edit_list_draw_new.top;
	while(ent)
	{
		kge_vertex_t vvv;
		kge_vertex_t *vtx = (kge_vertex_t*)(ent + 1);

		if(ent->next)
			vvv = *((kge_vertex_t*)(ent->next + 1));
		else
			location_from_mouse(&vvv.x, &vvv.y, 1);

		apply_4f(shader_buffer.shading.color, editor_color[EDITCOLOR_LINE_NEW].color);
		shader_changed = 1;
		shader_update();

		draw_line(vtx, &vvv, DEPTH_DRAWING);
		draw_vertex(vtx, 1.0f, EDITCOLOR_VERTEX_NEW, DEPTH_DRAWING);

		ent = ent->next;
	}
}

//
// drag & drop

static void drag_fix_things()
{
	link_entry_t *ent = tick_list_normal.top;

	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			float xx, yy;
			kge_thing_t *th = (kge_thing_t*)tickable->data;

			if(th->vc.editor == vc_editor)
			{
				kge_sector_t *sec = th->pos.sector;

				if(sec)
				{
					if(!edit_is_point_in_sector(th->pos.x, th->pos.y, sec, 0))
						sec = NULL;
				}

				if(!sec)
					sec = edit_find_sector_by_point(th->pos.x, th->pos.y, NULL, 1);

				th->pos.sector = sec;
				thing_update_sector(th, 1);
				edit_fix_thing_z(th, 0);
			}
		}

		ent = ent->next;
	}
}

static void drag_list_finish()
{
	link_entry_t *ent;
	uint32_t line_del = 0;

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(sec->vc.editor == vc_editor)
		{
			// go trough lines
			while(1)
			{
				uint32_t i;

				for(i = 0; i < sec->line_count; i++)
				{
					kge_line_t *line = sec->line + i;
					kge_vertex_t *v0 = line->vertex[0];
					kge_vertex_t *v1 = line->vertex[1];

					if(line->vc.editor != vc_editor)
						continue;

					if(	sec->line_count > 3 &&
						!line->backsector &&
						v0->x == v1->x &&
						v0->y == v1->y
					){
						memcpy(sec->line + i, sec->line + i + 1, sizeof(kge_line_t) * (sec->line_count - i - 1));
						sec->line_count--;

						if(i >= sec->line_count)
							line = sec->line;

						line->vertex[0] = v0;
						e2d_replace_vertex(v1, v0);

						i = 0;
						line_del++;

						break;
					}

					engine_update_line(line);
				}

				if(i >= sec->line_count)
					break;
			}

			engine_update_sector(sec);
		}

		ent = ent->next;
	}

	// go trough things
	drag_fix_things();

	// done
	list_clear(&drag_list_point);

	if(line_del == 1)
		edit_status_printf("Line deleted.");
	else
	if(line_del)
		edit_status_printf("%u lines deleted.", line_del);
}

static void drag_list_vertex(kge_vertex_t *vtx)
{
	point_drag_t *vd;
	link_entry_t *ent;

	if(vtx->vc_editor == vc_editor)
		return;

	vtx->vc_editor = vc_editor;

	vd = list_add_entry(&drag_list_point, sizeof(point_drag_t));
	vd->dst.x = &vtx->x;
	vd->dst.y = &vtx->y;
	vd->old.x = drag_item_x - vtx->x;
	vd->old.y = drag_item_y - vtx->y;

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);
		uint32_t changed = 0;

		// go trough lines
		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = sec->line + i;

			if(	line->vertex[0] == vtx ||
				line->vertex[1] == vtx
			){
				changed = 1;
				line->vc.editor = vc_editor;
			}
		}

		if(changed && sec->vc.editor != vc_editor)
		{
			sec->vc.editor = vc_editor;
			edit_mark_sector_things(sec);
		}

		ent = ent->next;
	}
}

static void drag_list_thing(kge_thing_t *thing)
{
	point_drag_t *vd;

	if(thing->vc.editor == vc_editor)
		return;

	thing->vc.editor = vc_editor;

	vd = list_add_entry(&drag_list_point, sizeof(point_drag_t));
	vd->dst.x = &thing->pos.x;
	vd->dst.y = &thing->pos.y;
	vd->old.x = drag_item_x - thing->pos.x;
	vd->old.y = drag_item_y - thing->pos.y;
}

static void drag_list_move(float x, float y)
{
	link_entry_t *ent = drag_list_point.top;

	// point movement
	while(ent)
	{
		point_drag_t *vd = (point_drag_t*)(ent + 1);

		*vd->dst.x = x - vd->old.x;
		*vd->dst.y = y - vd->old.y;

		ent = ent->next;
	}

	// update lines and sectors right away
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(sec->vc.editor == vc_editor)
		{
			// go trough lines
			for(uint32_t i = 0; i < sec->line_count; i++)
			{
				kge_line_t *line = sec->line + i;

				if(line->vc.editor != vc_editor)
					continue;

				engine_update_line(line);
			}

			engine_update_sector(sec);
		}

		ent = ent->next;
	}
}

static uint32_t drag_single_item(uint32_t in_type)
{
	link_entry_t *ent;
	point_drag_t *pt;

	drag_input_type = in_type;

	location_from_mouse(&drag_item_x, &drag_item_y, 1);

	vc_editor++;

	if(edit_hit.vertex)
	{
		if(in_type == INPUT_E2D_DRAG_SOLO)
		{
			drag_item_x = edit_hit.vertex->x;
			drag_item_y = edit_hit.vertex->y;
		}
		drag_list_vertex(edit_hit.vertex);
		return -1;
	}

	if(edit_hit.line)
	{
		if(in_type == INPUT_E2D_DRAG_SOLO)
		{
			drag_item_x = edit_hit.line->vertex[0]->x;
			drag_item_y = edit_hit.line->vertex[0]->y;
		}
		drag_list_vertex(edit_hit.line->vertex[0]);
		drag_list_vertex(edit_hit.line->vertex[1]);
		return -1;
	}

	if(edit_hit.sector)
	{
		if(in_type == INPUT_E2D_DRAG_SOLO)
		{
			drag_item_x = edit_hit.vertex->x;
			drag_item_y = edit_hit.vertex->y;
		}

		for(uint32_t i = 0; i < edit_hit.sector->line_count; i++)
		{
			kge_line_t *line = edit_hit.sector->line + i;
			drag_list_vertex(line->vertex[0]);
			drag_list_vertex(line->vertex[1]);
		}

		return -1;
	}

	if(edit_hit.thing)
	{
		if(in_type == INPUT_E2D_DRAG_SOLO)
		{
			drag_item_x = edit_hit.thing->pos.x;
			drag_item_y = edit_hit.thing->pos.y;
		}
		drag_list_thing(edit_hit.thing);
		return -1;
	}

	return 1;
}

//
// UI

static void ui_mode_changed()
{
	glui_set_text(&ui_edit_mode, mode_text[select_mode], glui_font_huge_kfn, GLUI_ALIGN_CENTER_CENTER);
	ui_edit_mode.base.disabled = 0;
	select_mode_tick = TICKRATE / 2;
}

//
// mode

void e2d_mode_set()
{
	x16g_update_palette(0);

	system_mouse_grab(0);
	system_draw = e2d_draw;
	system_input = e2d_input;
	system_tick = e2d_tick;

	ui_mode_changed();
}

//
// DRAW

void e2d_draw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	edge_x0 = edit_x - (screen_width / 2) / edit_scale;
	edge_x1 = edit_x + (screen_width / 2) / edit_scale;
	edge_y0 = edit_y - (screen_height / 2) / edit_scale;
	edge_y1 = edit_y + (screen_height / 2) / edit_scale;

	memcpy(shader_buffer.projection.matrix, r_projection_2d.raw, sizeof(matrix3d_t));

	shader_mode_color();

	// grid
	draw_grid();

	// center + offset
	matrix_translate(r_work_matrix + 0, &mat_identity, screen_width / 2, screen_height / 2, 0.0f);
	matrix_scale(r_work_matrix + 1, r_work_matrix + 0, edit_scale, edit_scale, 1.0f);
	matrix_translate(r_work_matrix + 0, r_work_matrix + 1, -edit_x, -edit_y, 0.0f);
	matrix_mult((matrix3d_t*)shader_buffer.projection.matrix, &r_projection_2d, r_work_matrix + 0);

	// origin (0, 0, 0)
	draw_origin();

	// new lines
	draw_new_lines();

	// sectors
	draw_sectors();

	// things
	draw_things();

	// extra stuff
	draw_extra();

	// UI elements
	memcpy(shader_buffer.projection.matrix, r_projection_ui.raw, sizeof(matrix3d_t));
	edit_draw_ui();
}

//
// logo

static void fs_logo(char *file)
{
	image_t *img;
	uint32_t size;

	img = img_png_load(file, 0);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(img->width != 160 || img->height != 120)
	{
		edit_status_printf("Invalid PNG resolution!");
		goto done;
	}

	x16g_img_pal_conv(img, x16e_logo_data);
	x16e_enable_logo = 1;

done:
	free(img);
}

//
// intput cb

static void qe_map_new(uint32_t ret)
{
	if(!ret)
		return;
	edit_new_map();
}

static void group_select_set(uint32_t magic)
{
	edit_grp_last = magic >> 8;
	edit_subgrp_last = magic & 0xFF;

	edit_hit.sector->stuff.group[0] = edit_grp_last;
	edit_hit.sector->stuff.group[1] = edit_subgrp_last;

	if(edit_grp_last)
		edit_status_printf("Sector group set to \"%s / %s\".", edit_group[edit_grp_last].name, edit_group[edit_grp_last].sub[edit_subgrp_last].name);
	else
		edit_status_printf("Sector group cleared.");
}

//
// input

static int32_t in2d_change_mode()
{
	if(input_ctrl)
	{
		if(!edit_list_sector.count)
		{
			edit_status_printf("There are no sectors!");
			return 1;
		}
		editor_change_mode(EDIT_MODE_WIREFRAME);
	} else
	{
		if(edit_check_map())
			return 1;

		editor_change_mode(EDIT_MODE_3D);
	}

	return 1;
}

static int32_t in2d_drag_map()
{
	location_from_mouse(&drag_map_x, &drag_map_y, 0);
	return -1;
}

static int32_t in2d_drag_sel()
{
	return drag_single_item(INPUT_E2D_DRAG_SEL);
}

static int32_t in2d_drag_solo()
{
	return drag_single_item(INPUT_E2D_DRAG_SOLO);
}

static int32_t in2d_grid_more()
{
	if(edit_grid < 1024)
		edit_grid *= 2;
	edit_status_printf("Grid: %d", edit_grid);
	return 1;
}

static int32_t in2d_grid_less()
{
	if(edit_grid > 1)
		edit_grid /= 2;
	edit_status_printf("Grid: %d", edit_grid);
	return 1;
}

static int32_t in2d_zoom_in()
{
	float mx = (float)(mouse_x - (int32_t)(screen_width / 2));
	float my = (float)(mouse_yy - (int32_t)(screen_height / 2));
	float ox = (mx / edit_scale) + edit_x;
	float oy = (my / edit_scale) + edit_y;
	int32_t xx, yy;

	if(edit_scale < 64)
		edit_scale *= 2;

	xx = ox - (mx / edit_scale);
	yy = oy - (my / edit_scale);

	edit_x = xx;
	edit_y = yy;

	edit_status_printf("Scale: %.3f", edit_scale);

	return 1;
}

static int32_t in2d_zoom_out()
{
	float mx = (float)(mouse_x - (int32_t)(screen_width / 2));
	float my = (float)(mouse_yy - (int32_t)(screen_height / 2));
	float ox = (mx / edit_scale) + edit_x;
	float oy = (my / edit_scale) + edit_y;
	int32_t xx, yy;

	if(edit_scale > 0.1f)
		edit_scale *= 0.5f;

	xx = ox - (mx / edit_scale);
	yy = oy - (my / edit_scale);

	edit_x = xx;
	edit_y = yy;

	edit_status_printf("Scale: %.3f", edit_scale);

	return 1;
}

static int32_t in2d_draw_start()
{
	float mx, my;
	kge_vertex_t *vtx;

	if(!edit_grp_show && edit_subgrp_show)
	{
		edit_status_printf("Can't draw in this filter mode!");
		return 1;
	}

	if(edit_grp_show)
		edit_status_printf("Drawing sector to group \"%s / %s\".", edit_group[edit_grp_show].name, edit_group[edit_grp_show].sub[edit_subgrp_show].name);

	location_from_mouse(&mx, &my, 1);

	if(!edit_list_draw_new.cur)
	{
		// new point
		vtx = list_add_entry(&edit_list_draw_new, sizeof(kge_vertex_t));
		vtx->x = mx;
		vtx->y = my;

		return 1;
	}

	// drawing new lines

	return 1;
}

static int32_t in2d_mode_lines()
{
	edit_clear_hit(1);
	select_mode = EDIT_MODE_LINE;
	ui_mode_changed();
	return 1;
}

static int32_t in2d_mode_sector()
{
	if(select_mode == EDIT_MODE_SECTOR)
	{
		if(!plane_mode)
			plane_mode = 2;
		else
			plane_mode--;

		return 1;
	}

	edit_clear_hit(1);
	select_mode = EDIT_MODE_SECTOR;
	ui_mode_changed();

	return 1;
}

static int32_t in2d_mode_things()
{
	edit_clear_hit(1);
	select_mode = EDIT_MODE_THING;
	ui_mode_changed();
	return 1;
}

static int32_t in2d_options()
{
	uin_map_options(NULL, -1, -1);
	return 1;
}

static int32_t in2d_groups()
{
	if(input_ctrl)
	{
		if(!edit_hit.sector)
			return 1;

		edit_hit.sector->stuff.group[0] = edit_grp_last;
		edit_hit.sector->stuff.group[1] = edit_subgrp_last;

		if(edit_grp_last)
			edit_status_printf("Sector group set to \"%s / %s\".", edit_group[edit_grp_last].name, edit_group[edit_grp_last].sub[edit_subgrp_last].name);
		else
			edit_status_printf("Sector group cleared.");

		return 1;
	}

	if(!input_shift)
	{
		uint8_t text[32];

		if(!edit_hit.sector)
			return 1;

		sprintf(text, "Sector #%u group.", list_get_idx(&edit_list_sector, (link_entry_t*)edit_hit.sector - 1) + 1);
		edit_ui_group_select(text, EDIT_GRPSEL_FLAG_ALLOW_MODIFY | EDIT_GRPSEL_FLAG_ENABLE_NONE, group_select_set);

		return 1;
	}

	uin_map_opt_grpsel(NULL, -1, -1);

	return 1;
}

static int32_t in2d_connect_sec()
{
	if(edit_hit.sector)
	{
		if(!e2d_connect_sectors(edit_hit.sector))
			edit_status_printf("Sectors linked.");
	}

	if(edit_hit.line)
	{
		if(!e2d_connect_lines(edit_hit.extra_sector, edit_hit.line))
			edit_status_printf("Sectors connected.");
	}

	return 1;
}

static int32_t in2d_disconnect_sec()
{
	if(edit_hit.sector)
	{
		kge_sector_t *sec = edit_hit.sector;
		uint32_t ul = 0;

		if(sec->plane[PLANE_TOP].link)
		{
			sec->plane[PLANE_TOP].link->plane[PLANE_BOT].link = NULL;
			sec->plane[PLANE_TOP].link = NULL;
			ul = 1;
		}
		if(sec->plane[PLANE_BOT].link)
		{
			sec->plane[PLANE_BOT].link->plane[PLANE_TOP].link = NULL;
			sec->plane[PLANE_BOT].link = NULL;
			ul = 1;
		}

		if(ul)
			edit_status_printf("Sectors unlinked.");
	}

	if(edit_hit.line)
	{
		if(!e2d_disconnect_lines(edit_hit.extra_sector, edit_hit.line))
			edit_status_printf("Sectors disconnected.");
	}

	return 1;
}

static int32_t in2d_blocking()
{
	if(edit_hit.line && edit_hit.line->backsector)
		edit_ui_blocking_select("Select line blocking bits.", &edit_hit.line->info.blocking, 1);

	return 1;
}

static int32_t in2d_masked_line()
{
	if(edit_hit.line)
		edit_ui_texture_select("Select masked texture.", edit_hit.line->texture + 2, 2);
	return 1;
}

static int32_t in2d_insert()
{
	float mx, my;

	location_from_mouse(&mx, &my, 1);

	if(select_mode == EDIT_MODE_LINE)
	{
		if(!edit_hit.line)
			return 1;

		switch(e2d_split_line(edit_hit.extra_sector, edit_hit.line, mx, my))
		{
			case 0:
				edit_status_printf("Line split.");
			break;
			case 1:
				edit_status_printf("Too many lines in this sector!");
			break;
			case 2:
				edit_status_printf("Can't split connected lines!");
			break;
			case 3:
				edit_status_printf("Can't place vertex at this location!");
			break;
		}

		return 1;
	}

	if(select_mode == EDIT_MODE_THING)
	{
		kge_sector_t *sec;
		kge_thing_t *th;
		float sz = 0;

		sec = edit_find_sector_by_point(mx, my, NULL, 1);
		if(sec)
			sz = sec->plane[PLANE_BOT].height;

		th = thing_spawn(mx, my, sz, sec);
		x16g_set_sprite_texture(th);
		thing_update_sector(th, 1);

		return 1;
	}

	return 1;
}

static int32_t in2d_delete()
{
	switch(select_mode)
	{
		case EDIT_MODE_THING:
			if(!edit_hit.thing)
				break;

			if(edit_hit.thing == edit_camera_thing)
				break;

			thing_remove(edit_hit.thing);
			edit_clear_hit(1);
		break;
		case EDIT_MODE_SECTOR:
			if(!edit_hit.sector)
				break;

			if(input_ctrl)
			{
				e2d_delete_sector(edit_hit.sector);
				edit_clear_hit(1);
			} else
				edit_status_printf("Hold CTRL to delete sectors.");
		break;
	}

	return 1;
}

static int32_t in2d_draw_insert()
{
	kge_vertex_t *vtx;
	float mx, my;

	location_from_mouse(&mx, &my, 1);

	vtx = (kge_vertex_t*)(edit_list_draw_new.cur + 1);

	if(vtx->x != mx || vtx->y != my)
	{
		// this vertex does not match last drawn
		vtx = (kge_vertex_t*)(edit_list_draw_new.top + 1);
		if(vtx->x == mx && vtx->y == my)
		{
			// closed the loop
			switch(e2d_create_sector())
			{
				case 0:
					edit_status_printf("Sector created.");
				break;
				case 1:
					edit_status_printf("Invalid sector shape!");
				break;
				case 2:
					edit_status_printf("Too many lines for sector!");
				break;
				case 3:
					edit_status_printf("Too many sectors!");
				break;
			}
			// clear draw list
			list_clear(&edit_list_draw_new);
		} else
		{
			if(edit_list_draw_new.count < EDIT_MAX_SECTOR_LINES)
			{
				// a new point
				vtx = list_add_entry(&edit_list_draw_new, sizeof(kge_vertex_t));
				vtx->x = mx;
				vtx->y = my;
			} else
				edit_status_printf("Line limit hit! Close the sector now.");
		}
	}

	return 1;
}

static int32_t in2d_draw_remove()
{
	list_del_entry(&edit_list_draw_new, edit_list_draw_new.cur);
	return 1;
}

// lists
static const edit_input_func_t infunc_normal[] =
{
	{INPUT_EDITOR_MENU, edit_infunc_menu},
	{INPUT_EDITOR_EDITMODE, in2d_change_mode},
	{INPUT_E2D_DRAG_MAP, in2d_drag_map},
	{INPUT_E2D_DRAG_SEL, in2d_drag_sel},
	{INPUT_E2D_DRAG_SOLO, in2d_drag_solo},
	{INPUT_E2D_GRID_MORE, in2d_grid_more},
	{INPUT_E2D_GRID_LESS, in2d_grid_less},
	{INPUT_E2D_ZOOM_IN, in2d_zoom_in},
	{INPUT_E2D_ZOOM_OUT, in2d_zoom_out},
	{INPUT_E2D_DRAW_POINT, in2d_draw_start},
	{INPUT_E2D_MODE_LINES, in2d_mode_lines},
	{INPUT_E2D_MODE_SECTOR, in2d_mode_sector},
	{INPUT_E2D_MODE_THINGS, in2d_mode_things},
	{INPUT_E2D_OPTIONS, in2d_options},
	{INPUT_E2D_GROUPS, in2d_groups},
	{INPUT_E2D_SECTOR_CONNECT, in2d_connect_sec},
	{INPUT_E2D_SECTOR_DISCONNECT, in2d_disconnect_sec},
	{INPUT_E2D_BLOCKING, in2d_blocking},
	{INPUT_E2D_MASKED_LINE, in2d_masked_line},
	{INPUT_EDITOR_INSERT, in2d_insert},
	{INPUT_EDITOR_DELETE, in2d_delete},
	// terminator
	{-1}
};
static const edit_input_func_t infunc_drawing[] =
{
	{INPUT_E2D_DRAG_MAP, in2d_drag_map},
	{INPUT_E2D_GRID_MORE, in2d_grid_more},
	{INPUT_E2D_GRID_LESS, in2d_grid_less},
	{INPUT_E2D_ZOOM_IN, in2d_zoom_in},
	{INPUT_E2D_ZOOM_OUT, in2d_zoom_out},
	{INPUT_E2D_DRAW_POINT, in2d_draw_insert},
	{INPUT_E2D_DRAW_BACK, in2d_draw_remove},
	// terminator
	{-1}
};
static const edit_input_func_t infunc_itemdrag[] =
{
	{INPUT_E2D_DRAG_MAP, in2d_drag_map},
	{INPUT_E2D_GRID_MORE, in2d_grid_more},
	{INPUT_E2D_GRID_LESS, in2d_grid_less},
	{INPUT_E2D_ZOOM_IN, in2d_zoom_in},
	{INPUT_E2D_ZOOM_OUT, in2d_zoom_out},
	// terminator
	{-1}
};

void e2d_input()
{
	if(edit_ui_input())
		return;

	if(!isnan(drag_map_x))
	{
		float pos_x = (float)(mouse_x - (int32_t)(screen_width / 2)) / edit_scale;
		float pos_y = (float)(mouse_yy - (int32_t)(screen_height / 2)) / edit_scale;
		edit_x = drag_map_x - pos_x;
		edit_y = drag_map_y - pos_y;

		if(!input_state[INPUT_E2D_DRAG_MAP])
			drag_map_x = NAN;

		return;
	}

	if(drag_list_point.count)
	{
		float mx, my;

		location_from_mouse(&mx, &my, 1);

		if(mx != drag_item_x || my != drag_item_y)
		{
			drag_item_x = mx;
			drag_item_y = my;
			drag_list_move(mx, my);
		}

		if(input_state[drag_input_type])
			edit_handle_input(infunc_itemdrag);
		else
			drag_list_finish();

		return;
	}

	check_cursor_highlight();

	if(edit_list_draw_new.top)
	{
		edit_handle_input(infunc_drawing);
		return;
	}

	edit_handle_input(infunc_normal);
}

//
// tick

void e2d_tick(uint32_t count)
{
	edit_highlight_alpha = 0.2f + 0.1f * sinf((float)gametick * EDIT_HIGHLIGHT_FREQ_MULT);

	if(select_mode_tick)
	{
		select_mode_tick -= count;
		if(select_mode_tick <= 0)
		{
			select_mode_tick = 0;
			ui_edit_mode.base.disabled = 1;
		}
	}

	// level time is not advanced in this mode
	for(uint32_t i = 0; i < count; i++)
		edit_ui_tick();
}

