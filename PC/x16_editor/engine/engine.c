#include "inc.h"
#include "defs.h"
#include "list.h"
#include "engine.h"
#include "x16r.h"

sector_link_t **engine_link_ptr;

//
// API

void engine_reset_box(float *box)
{
	box[BOX_XMIN] = +INFINITY;
	box[BOX_XMAX] = -INFINITY;
	box[BOX_YMIN] = +INFINITY;
	box[BOX_YMAX] = -INFINITY;
}

void engine_add_to_box(float *box, float x, float y)
{
	if(x < box[BOX_XMIN])
		box[BOX_XMIN] = x;

	if(x > box[BOX_XMAX])
		box[BOX_XMAX] = x;

	if(y < box[BOX_YMIN])
		box[BOX_YMIN] = y;

	if(y > box[BOX_YMAX])
		box[BOX_YMAX] = y;
}

void engine_update_sector(kge_sector_t *sec)
{
	uint32_t flags = PSF_IS_CONVEX;
	kge_vertex_t *vtx = NULL;

	// reset bounding box
	engine_reset_box(sec->stuff.box);

	// go trough lines
	for(uint32_t i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = sec->line + i;

		engine_add_to_box(sec->stuff.box, line->vertex[0]->x, line->vertex[0]->y);
		engine_add_to_box(sec->stuff.box, line->vertex[1]->x, line->vertex[1]->y);

		if(vtx && line->vertex[0] != vtx)
			flags = 0;

		if(flags & PSF_IS_CONVEX)
		{
			kge_line_t *ll;

			if(i + 1 == sec->line_count)
				ll = sec->line;
			else
				ll = line + 1;

			if(kge_line_point_side(line, ll->vertex[1]->x, ll->vertex[1]->y) < 0)
				flags = 0;
		}

		vtx = line->vertex[1];
	}

	// center location
	sec->stuff.center.x = sec->stuff.box[BOX_XMIN] + (sec->stuff.box[BOX_XMAX] - sec->stuff.box[BOX_XMIN]) * 0.5f;
	sec->stuff.center.y = sec->stuff.box[BOX_YMIN] + (sec->stuff.box[BOX_YMAX] - sec->stuff.box[BOX_YMIN]) * 0.5f;

	// flags
	sec->stuff.flags = flags;
}

void engine_update_line(kge_line_t *line)
{
	line->stuff.dist.x = line->vertex[1]->x - line->vertex[0]->x;
	line->stuff.dist.y = line->vertex[1]->y - line->vertex[0]->y;

	line->stuff.center.x = line->vertex[0]->x + line->stuff.dist.x * 0.5f;
	line->stuff.center.y = line->vertex[0]->y + line->stuff.dist.y * 0.5f;

	line->stuff.length = sqrtf(line->stuff.dist.x * line->stuff.dist.x + line->stuff.dist.y * line->stuff.dist.y);

	line->stuff.normal.x = -line->stuff.dist.y / line->stuff.length;
	line->stuff.normal.y = line->stuff.dist.x / line->stuff.length;

	line->stuff.x16angle = x16r_line_angle(line);
}

void engine_add_sector_link(kge_thing_t *th, kge_sector_t *sector)
{
	sector_link_t **link = &sector->thinglist;
	sector_link_t *add = malloc(sizeof(sector_link_t));

	if(!add)
		return;

	add->thing = th;

	add->next_thing = *link;
	if(*link)
		(*link)->prev_thing = &add->next_thing;

	add->prev_thing = link;
	*link = add;

	add->prev_sector = engine_link_ptr;
	add->next_sector = NULL;

	*engine_link_ptr = add;
	engine_link_ptr = &add->next_sector;
}

