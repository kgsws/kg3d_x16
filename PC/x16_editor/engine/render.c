#include "inc.h"
#include "defs.h"
#include "engine.h"
#include "matrix.h"
#include "shader.h"
#include "things.h"
#include "render.h"
#include "x16g.h"

//#define RENDER_STATS

#define MIN_GLVERT_COUNT	64

//

matrix3d_t r_projection_3d;
matrix3d_t r_projection_2d;
matrix3d_t r_projection_ui;
matrix3d_t r_work_matrix[3];

gl_vertex_t *gl_vertex_buf;

float viewangle[2];
float view_sprite_vec[2];
kge_xyz_vec_t viewpos;

static kge_thing_t *render_camera;

gl_plane_t plane_left;
gl_plane_t plane_right;
gl_plane_t plane_bot;
gl_plane_t plane_top;
gl_plane_t plane_far;

uint32_t vc_render;

uint32_t ident_idx;
ident_slot_t ident_slot[MAX_IDENT_SLOTS];

#ifdef RENDER_STATS
uint32_t render_stat_sectors;
uint32_t render_stat_lines;
uint32_t render_stat_planes;
uint32_t render_stat_walls;
#endif

static uint32_t frustum_wall;

//
// visibility checks

static uint32_t frustum_point_check(gl_plane_t *plane)
{
	uint32_t ret;
	ret = plane->val[0] * gl_vertex_buf[0].x + plane->val[1] * gl_vertex_buf[0].y + plane->val[2] * gl_vertex_buf[0].z + plane->val[3] >= 0;
	ret += plane->val[0] * gl_vertex_buf[0].x + plane->val[1] * gl_vertex_buf[0].y + plane->val[2] * gl_vertex_buf[2].z + plane->val[3] >= 0;
	ret += plane->val[0] * gl_vertex_buf[1].x + plane->val[1] * gl_vertex_buf[1].y + plane->val[2] * gl_vertex_buf[1].z + plane->val[3] >= 0;
	ret += plane->val[0] * gl_vertex_buf[1].x + plane->val[1] * gl_vertex_buf[1].y + plane->val[2] * gl_vertex_buf[3].z + plane->val[3] >= 0;
	return ret;
}

static uint32_t frustum_wall_check(uint32_t check_clip)
{
	// actual frustum check
	if(!frustum_point_check(&plane_left))
		return 0;
	if(!frustum_point_check(&plane_right))
		return 0;
	if(!frustum_point_check(&plane_bot))
		return 0;
	if(!frustum_point_check(&plane_top))
		return 0;
	if(!frustum_point_check(&plane_far))
		return 0;
	// clipping
	frustum_wall = 0;
	if(check_clip)
	{
		uint32_t check;

		check = frustum_point_check((gl_plane_t*)shader_buffer.clipping.plane0);
		if(!check)
			return 0;

		if(check == 2)
			frustum_wall = 1;

		check = frustum_point_check((gl_plane_t*)shader_buffer.clipping.plane1);
		if(check >= 4)
			return 0;

		if(check == 2)
			frustum_wall |= 2;
	}
	// potentially visible
	return 1;
}

//
// math

static inline float calc_dot(float *p0, float *p1)
{
	return p0[0] * p1[0] + p0[1] * p1[1] + p0[2] * p1[2];
}

static inline void calc_cross(float *result, float *p0, float *p1)
{
	result[0] = p0[1] * p1[2] - p0[2] * p1[1];
	result[1] = p0[2] * p1[0] - p0[0] * p1[2];
	result[2] = p0[0] * p1[1] - p0[1] * p1[0];
}

static inline void calc_plane(float *target, float *vtx, float *p2)
{
	// calculate new plane
	float p0[3] = {vtx[0], vtx[1], +128.0f};
	float p1[3] = {vtx[0], vtx[1], -128.0f};
	calc_cross(target, p0, p1);
	target[3] = -calc_dot(target, p2);
}

//
// identification

static void set_identity(kge_sector_t *sec, void *ptr, uint32_t type)
{
	uint32_t id = 0;

	switch(type)
	{
		case IDENT_WALL_TOP:
		case IDENT_WALL_BOT:
		case IDENT_WALL_MID:
		{
			kge_line_t *ln = ptr;
			uint32_t slot = type - IDENT_WALL_TOP;

			if(ln->tex_ident[slot].validcount != vc_render)
			{
				if(ident_idx >= MAX_IDENT_SLOTS)
					goto no_slot;

				ln->tex_ident[slot].validcount = vc_render;
				ln->tex_ident[slot].id = ident_idx;

				ident_slot[ident_idx].type = type;
				ident_slot[ident_idx].sector = sec;
				ident_slot[ident_idx].line = ln;

				ident_idx++;
			}

			id = ln->tex_ident[slot].id;
		}
		break;
		case IDENT_PLANE_TOP:
		case IDENT_PLANE_BOT:
		{
			uint32_t slot = type - IDENT_PLANE_TOP;

			if(sec->pl_ident[slot].validcount != vc_render)
			{
				if(ident_idx >= MAX_IDENT_SLOTS)
					goto no_slot;

				sec->pl_ident[slot].validcount = vc_render;
				sec->pl_ident[slot].id = ident_idx;

				ident_slot[ident_idx].type = type;
				ident_slot[ident_idx].sector = sec;
				ident_slot[ident_idx].line = NULL;

				ident_idx++;
			}

			id = sec->pl_ident[slot].id;
		}
		break;
		case IDENT_THING:
		{
			kge_thing_t *th = ptr;

			if(th->ident.validcount != vc_render)
			{
				if(ident_idx >= MAX_IDENT_SLOTS)
					goto no_slot;

				th->ident.validcount = vc_render;
				th->ident.id = ident_idx;

				ident_slot[ident_idx].type = type;
				ident_slot[ident_idx].sector = sec;
				ident_slot[ident_idx].thing = th;

				ident_idx++;
			}

			id = th->ident.id;
		}
		break;
	}

	shader_buffer.shading.identity[2] = (float)(id & 0xFF) * (1.0f / 255.0f);
	shader_buffer.shading.identity[3] = (float)(id >> 8) * (1.0f / 255.0f);

	return;

no_slot:
	shader_buffer.shading.identity[2] = 0;
	shader_buffer.shading.identity[3] = 0;
}

//
// drawing

void r_draw_plane(kge_sector_t *sector, uint32_t plane)
{
	float height;
	kge_secplane_t *sp;

	sp = &sector->plane[plane & PLANE_MASK];

	if(!(plane & PLANE_USE_COLOR))
	{
		float angle = (float)sp->texture.angle * ((M_PI*2) / 256.0f);
		editor_texture_t *et = editor_texture + sp->texture.idx;

		switch(et->type)
		{
			case X16G_TEX_TYPE_WALL:
			case X16G_TEX_TYPE_WALL_MASKED:
			case X16G_TEX_TYPE_PLANE_8BPP:
				shader_buffer.mode.fragment = SHADER_FRAGMENT_PALETTE_LIGHT;
			break;
			case X16G_TEX_TYPE_PLANE_4BPP:
				shader_buffer.colormap = COLORMAP_IDX(et->cmap);
				shader_buffer.mode.fragment = SHADER_FRAGMENT_COLORMAP_LIGHT;
			break;
			default:
				shader_buffer.mode.fragment = SHADER_FRAGMENT_RGB;
			break;
		}

		shader_buffer.mode.vertex = SHADER_VERTEX_PLANE;
		shader_changed = 1;

		glBindTexture(GL_TEXTURE_2D, et->gltex);
		shader_buffer.scaling.vec[0] = et->width * 2.0f;
		shader_buffer.scaling.vec[1] = et->height * -2.0f;
		shader_buffer.scaling.vec[2] = (float)sp->texture.ox * 2.0f;
		shader_buffer.scaling.vec[3] = (float)sp->texture.oy * -2.0f;
		shader_buffer.scaling.rot[0] = cosf(angle);
		shader_buffer.scaling.rot[1] = sinf(angle);
		shader_buffer.scaling.rot[4] = -sinf(angle);
		shader_buffer.scaling.rot[5] = cosf(angle);
	} else
		shader_mode_color();

	if(plane & PLANE_USE_Z)
	{
		float z = (plane & PLANE_ZMASK) >> PLANE_ZSHIFT;
		height = z;
	} else
		height = sp->height;

	gl_vertex_buf[0].z = height;
	gl_vertex_buf[1].z = height;
	gl_vertex_buf[2].z = height;
	gl_vertex_buf[3].z = height;

	shader_update();

	if(!(sector->stuff.flags & PSF_IS_CONVEX))
	{
		// prepare stencil pass

		glDepthMask(GL_FALSE);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glStencilMask(GL_TRUE);
		glEnable(GL_STENCIL_TEST);

		// 1st pass; clear stencil area
		glStencilFunc(GL_ALWAYS, 0, 1);
		glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

		gl_vertex_buf[0].x = sector->stuff.box[BOX_XMIN];
		gl_vertex_buf[0].y = sector->stuff.box[BOX_YMIN];
		gl_vertex_buf[1].x = sector->stuff.box[BOX_XMAX];
		gl_vertex_buf[1].y = sector->stuff.box[BOX_YMIN];
		gl_vertex_buf[2].x = sector->stuff.box[BOX_XMIN];
		gl_vertex_buf[2].y = sector->stuff.box[BOX_YMAX];
		gl_vertex_buf[3].x = sector->stuff.box[BOX_XMAX];
		gl_vertex_buf[3].y = sector->stuff.box[BOX_YMAX];

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// 2nd pass; draw all the lines, modifying stencil buffer
		glStencilOp(GL_KEEP, GL_INVERT, GL_INVERT);
	}

	// all the lines
	{
		kge_line_t *line;

		line = sector->line;

		gl_vertex_buf[2].x = line->vertex[0]->x;
		gl_vertex_buf[2].y = line->vertex[0]->y;

		for(uint32_t i = 1; i < sector->line_count; i++)
		{
			float *color;

			line++;

			gl_vertex_buf[0].x = line->vertex[0]->x;
			gl_vertex_buf[0].y = line->vertex[0]->y;

			gl_vertex_buf[1].x = line->vertex[1]->x;
			gl_vertex_buf[1].y = line->vertex[1]->y;

			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	}

	if(!(sector->stuff.flags & PSF_IS_CONVEX))
	{
		// 3rd pass; draw colored polygon over, using stencil buffer
		glDepthMask(GL_TRUE);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glStencilMask(GL_FALSE);

		glStencilFunc(GL_EQUAL, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		gl_vertex_buf[0].x = sector->stuff.box[BOX_XMIN];
		gl_vertex_buf[0].y = sector->stuff.box[BOX_YMIN];
		gl_vertex_buf[1].x = sector->stuff.box[BOX_XMAX];
		gl_vertex_buf[1].y = sector->stuff.box[BOX_YMIN];
		gl_vertex_buf[2].x = sector->stuff.box[BOX_XMIN];
		gl_vertex_buf[2].y = sector->stuff.box[BOX_YMAX];
		// gl_vertex_buf[3] is already filled

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// done
		glDisable(GL_STENCIL_TEST);
	}
}

static void draw_wall(kge_sector_t *sec, kge_line_t *ln, uint32_t texflg, float top, float bot, float oy)
{
	kge_x16_tex_t *ti = ln->texture + (texflg & 3);
	editor_texture_t *et = editor_texture + ti->idx;
	float tt, tb, tl, tr;
	uint32_t flags = ti->flags & TEXFLAG_MIRROR_X;

	if(bot >= top)
		return;

	set_identity(sec, ln, IDENT_WALL_TOP + (texflg & 3));

	if(!(texflg & 2))
	{
		oy = ti->oy;
		flags = ti->flags;
	}

#ifdef RENDER_STATS
	render_stat_walls++;
#endif

	switch(et->type)
	{
		case X16G_TEX_TYPE_WALL:
		case X16G_TEX_TYPE_WALL_MASKED:
		case X16G_TEX_TYPE_PLANE_8BPP:
			shader_buffer.mode.fragment = SHADER_FRAGMENT_PALETTE_LIGHT;
		break;
		case X16G_TEX_TYPE_PLANE_4BPP:
			shader_buffer.colormap = COLORMAP_IDX(et->cmap);
			shader_buffer.mode.fragment = SHADER_FRAGMENT_COLORMAP_LIGHT;
		break;
		default:
			shader_buffer.mode.fragment = SHADER_FRAGMENT_RGB;
		break;
	}

	shader_buffer.mode.vertex = SHADER_VERTEX_NORMAL;
	shader_changed = 1;

	glBindTexture(GL_TEXTURE_2D, et->gltex);
	shader_update();

	if(flags & TEXFLAG_PEG_Y)
	{
		tt = (float)oy / (float)et->height;
		tb = tt + (top - bot) / (float)et->height * 0.5f;
	} else
	{
		tb = (float)oy / (float)et->height;
		tt = tb - (top - bot) / (float)et->height * 0.5f;
	}

	if(flags & TEXFLAG_MIRROR_Y && (et->type == X16G_TEX_TYPE_WALL || et->type == X16G_TEX_TYPE_WALL_MASKED))
	{
		tb = -tb;
		tt = -tt;
	}

	if((texflg >> 8) & WALLFLAG_PEG_X)
	{
		if(flags & TEXFLAG_MIRROR_X)
		{
			tr = -(float)ti->ox / (float)et->width;
			tl = tr + ln->stuff.length / (float)et->width * 0.5f;
		} else
		{
			tr = (float)ti->ox / (float)et->width;
			tl = tr - ln->stuff.length / (float)et->width * 0.5f;
		}
	} else
	{
		if(flags & TEXFLAG_MIRROR_X)
		{
			tl = -(float)ti->ox / (float)et->width;
			tr = tl - ln->stuff.length / (float)et->width * 0.5f;
		} else
		{
			tl = (float)ti->ox / (float)et->width;
			tr = tl + ln->stuff.length / (float)et->width * 0.5f;
		}
	}

	gl_vertex_buf[0].z = bot;
	gl_vertex_buf[0].s = tr;
	gl_vertex_buf[0].t = tb;
	gl_vertex_buf[1].z = bot;
	gl_vertex_buf[1].s = tl;
	gl_vertex_buf[1].t = tb;
	gl_vertex_buf[2].z = top;
	gl_vertex_buf[2].s = tr;
	gl_vertex_buf[2].t = tt;
	gl_vertex_buf[3].z = top;
	gl_vertex_buf[3].s = tl;
	gl_vertex_buf[3].t = tt;

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

//
// sector scan

void render_scan_sector(kge_sector_t *sec, uint32_t recursion)
{
	float fb, ft;
	float bt, bb;
	kge_sector_t *bs;
	sector_link_t *link;
	kge_vertex_t *v0, *v1;
	shader_clip_info_t clipinfo;

	if(recursion > MAX_RENDER_RECURSION)
		return;

#ifdef RENDER_STATS
	render_stat_sectors++;
#endif

	fb = sec->plane[PLANE_BOT].height;
	ft = sec->plane[PLANE_TOP].height;

	shader_buffer.lightmap = LIGHTMAP_IDX(sec->light.idx);

	// draw planes
	if(sec->plane[PLANE_BOT].height < viewpos.z)
	{
#ifdef RENDER_STATS
		render_stat_planes++;
#endif
		set_identity(sec, NULL, IDENT_PLANE_BOT);
		r_draw_plane(sec, PLANE_BOT);
	}
	if(sec->plane[PLANE_TOP].height > viewpos.z)
	{
#ifdef RENDER_STATS
		render_stat_planes++;
#endif
		set_identity(sec, NULL, IDENT_PLANE_TOP);
		r_draw_plane(sec, PLANE_TOP);
	}

	// draw things
	glEnable(GL_ALPHA_TEST);
	shader_buffer.mode.vertex = 0;

	link = sec->thinglist;
	while(link)
	{
		kge_thing_t *th = link->thing;

		if(th != render_camera && th->prop.type != THING_TYPE_EDIT_CAMERA)
		{
			float cx, cy, cz;
			float xx, yy, zz;
			float vs, vc;
			float scale;

			scale = th->sprite.scale;

			if(th->sprite.bright)
				shader_buffer.lightmap = 0;
			else
				shader_buffer.lightmap = LIGHTMAP_IDX(th->pos.sector->light.idx);

			cx = th->pos.x - view_sprite_vec[0] * th->sprite.ox * 2 * scale;
			cy = th->pos.y - view_sprite_vec[1] * th->sprite.ox * 2 * scale;

			cz = th->pos.z + th->sprite.oy * 2 * scale;
			zz = cz + th->sprite.height * scale;

			vc = view_sprite_vec[0] * th->sprite.width * scale;
			vs = view_sprite_vec[1] * th->sprite.width * scale;

			xx = cx - vc;
			yy = cy - vs;
			gl_vertex_buf[0].x = xx;
			gl_vertex_buf[0].y = yy;
			gl_vertex_buf[0].z = zz;
			gl_vertex_buf[0].s = 0.0f;
			gl_vertex_buf[0].t = 0.0f;
			gl_vertex_buf[1].x = xx;
			gl_vertex_buf[1].y = yy;
			gl_vertex_buf[1].z = cz;
			gl_vertex_buf[1].s = 0.0f;
			gl_vertex_buf[1].t = 1.0f;

			xx = cx + vc;
			yy = cy + vs;
			gl_vertex_buf[2].x = xx;
			gl_vertex_buf[2].y = yy;
			gl_vertex_buf[2].z = zz;
			gl_vertex_buf[2].s = th->sprite.ws;
			gl_vertex_buf[2].t = 0.0f;
			gl_vertex_buf[3].x = xx;
			gl_vertex_buf[3].y = yy;
			gl_vertex_buf[3].z = cz;
			gl_vertex_buf[3].s = th->sprite.ws;
			gl_vertex_buf[3].t = 1.0f;

			set_identity(sec, th, IDENT_THING);
			shader_buffer.mode.fragment = th->sprite.shader;
			shader_changed = 1;
			shader_update();

			glBindTexture(GL_TEXTURE_2D, th->sprite.gltex);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}

		link = link->next_thing;
	}
	glDisable(GL_ALPHA_TEST);

	shader_buffer.lightmap = LIGHTMAP_IDX(sec->light.idx);

	// draw walls and check portals
	for(uint32_t i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = sec->line + i;
		uint32_t wflags = line->info.flags << 8;

		if(kge_line_point_side(line, viewpos.x, viewpos.y) < 0.0f)
			continue;
#ifdef RENDER_STATS
		render_stat_lines++;
#endif
		v0 = line->vertex[0];
		v1 = line->vertex[1];

		shader_buffer.lightmap = LIGHTMAP_IDX(sec->light.idx);

		if(!line->backsector)
		{
			if(line->texture_split != INFINITY)
			{
				bt = line->texture_split;
				bb = line->texture_split;
				goto use_split;
			}

			gl_vertex_buf[0].x = v0->x;
			gl_vertex_buf[0].y = v0->y;
			gl_vertex_buf[1].x = v1->x;
			gl_vertex_buf[1].y = v1->y;
			gl_vertex_buf[2].x = v0->x;
			gl_vertex_buf[2].y = v0->y;
			gl_vertex_buf[3].x = v1->x;
			gl_vertex_buf[3].y = v1->y;

			bt = ft;
			bb = fb;

			draw_wall(sec, line, wflags | 0, ft, fb, 0);
		} else
		{
			bs = line->backsector;
			bt = bs->plane[PLANE_TOP].height;
			bb = bs->plane[PLANE_BOT].height;
use_split:
			// setup coords
			gl_vertex_buf[0].x = v0->x;
			gl_vertex_buf[0].y = v0->y;
			gl_vertex_buf[1].x = v1->x;
			gl_vertex_buf[1].y = v1->y;
			gl_vertex_buf[2].x = v0->x;
			gl_vertex_buf[2].y = v0->y;
			gl_vertex_buf[3].x = v1->x;
			gl_vertex_buf[3].y = v1->y;

			// bottom wall
			draw_wall(sec, line, wflags | 1, bb, fb, 0);

			// top wall
			draw_wall(sec, line, wflags | 0, ft, bt, 0);

			// backsector portal
			if(	bt > fb &&
				bb < ft &&
				bb < bt
			){
				if(bt < ft)
				{
					gl_vertex_buf[0].z = bt;
					gl_vertex_buf[1].z = bt;
				} else
				{
					gl_vertex_buf[0].z = ft;
					gl_vertex_buf[1].z = ft;
				}
				if(bb > fb)
				{
					gl_vertex_buf[2].z = bb;
					gl_vertex_buf[3].z = bb;
				} else
				{
					gl_vertex_buf[2].z = fb;
					gl_vertex_buf[3].z = fb;
				}
				if(frustum_wall_check(recursion))
				{
					float vtx[2];

					clipinfo = shader_buffer.clipping;

					if(!(frustum_wall & 1))
					{
						vtx[0] = v0->x - viewpos.x;
						vtx[1] = v0->y - viewpos.y;
						calc_plane(shader_buffer.clipping.plane0, vtx, viewpos.xyz);
					}

					if(!(frustum_wall & 2))
					{
						vtx[0] = v1->x - viewpos.x;
						vtx[1] = v1->y - viewpos.y;
						calc_plane(shader_buffer.clipping.plane1, vtx, viewpos.xyz);
					}

					render_scan_sector(bs, recursion + 1);

					shader_buffer.clipping = clipinfo;
				}
			}
		}

		// mid wall
		if(line->texture[2].idx)
		{
			kge_x16_tex_t *ti = line->texture + 2;
			editor_texture_t *et = editor_texture + ti->idx;
			float top, bot, bbb;
			float offs = 0;

			top = line->stuff.normal.x * 0.1f;
			bot = line->stuff.normal.y * 0.1f;

			gl_vertex_buf[0].x = v0->x + top;
			gl_vertex_buf[0].y = v0->y + bot;
			gl_vertex_buf[1].x = v1->x + top;
			gl_vertex_buf[1].y = v1->y + bot;
			gl_vertex_buf[2].x = gl_vertex_buf[0].x;
			gl_vertex_buf[2].y = gl_vertex_buf[0].y;
			gl_vertex_buf[3].x = gl_vertex_buf[1].x;
			gl_vertex_buf[3].y = gl_vertex_buf[1].y;

			bbb = ti->flags & TEXFLAG_PEG_MID_BACK ? bb : fb;
			bot = bbb + ti->oy * 2;
			top = bot + et->height * 2;

			shader_buffer.lightmap = LIGHTMAP_IDX(sec->light.idx);

			glEnable(GL_ALPHA_TEST);
			draw_wall(sec, line, wflags | 2, top, bot, offs * 0.5f);
			glDisable(GL_ALPHA_TEST);
		}
	}
}

//
// render

void render_raw_view(kge_thing_pos_t *pos, float z)
{
	float v0[2];

	vc_render++;

	viewangle[0] = ANGLE_TO_RAD(pos->angle);
	viewangle[1] = PITCH_TO_RAD(pos->pitch);

	view_sprite_vec[0] = cosf(viewangle[0]);
	view_sprite_vec[1] = sinf(viewangle[0]);

	viewpos.x = pos->x;
	viewpos.y = pos->y;
	viewpos.z = z;

	// update matrix
	matrix_rotate_x(r_work_matrix + 0, &mat_identity, viewangle[1] + M_PI * 0.5f);
	matrix_rotate_z(r_work_matrix + 1, r_work_matrix + 0, -viewangle[0]);
	matrix_translate(r_work_matrix + 0, r_work_matrix + 1, -viewpos.x, -viewpos.y, -viewpos.z);
	matrix_mult((matrix3d_t*)shader_buffer.projection.matrix, &r_projection_3d, r_work_matrix + 0);

	// get frustum planes
	for(uint32_t i = 0; i < 4; i++)
	{
		plane_left.val[i] = shader_buffer.projection.matrix[3 + i * 4] + shader_buffer.projection.matrix[i * 4];
		plane_right.val[i] = shader_buffer.projection.matrix[3 + i * 4] - shader_buffer.projection.matrix[i * 4];
		plane_bot.val[i] = shader_buffer.projection.matrix[3 + i * 4] + shader_buffer.projection.matrix[1 + i * 4];
		plane_top.val[i] = shader_buffer.projection.matrix[3 + i * 4] - shader_buffer.projection.matrix[1 + i * 4];
		plane_far.val[i] = shader_buffer.projection.matrix[3 + i * 4] - shader_buffer.projection.matrix[2 + i * 4];
	}

	shader_changed = 1;
	shader_update();

	glEnable(GL_CLIP_DISTANCE0);
	glEnable(GL_CLIP_DISTANCE1);

	if(pos->sector)
		render_scan_sector(pos->sector, 0);

	glDisable(GL_CLIP_DISTANCE0);
	glDisable(GL_CLIP_DISTANCE1);
}

//
// API

uint32_t render_init()
{
	gl_vertex_buf = malloc(MIN_GLVERT_COUNT * sizeof(gl_vertex_t));
	if(!gl_vertex_buf)
		return 1;

	// prepare rendering
	glEnableVertexAttribArray(shader_attr_vertex);
	glEnableVertexAttribArray(shader_attr_coord);
	glVertexAttribPointer(shader_attr_vertex, 3, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_t), &gl_vertex_buf->x);
	glVertexAttribPointer(shader_attr_coord, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_t), &gl_vertex_buf->s);

	return 0;
}

void render_view(kge_thing_t *camera)
{
	if(!camera || !camera->pos.sector)
	{
		return;
	}

	if(camera->pos.sector->stuff.palette)
		x16g_update_palette(camera->pos.sector->stuff.palette * 4);

	render_camera = camera;

	ident_idx = 1;

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

#ifdef RENDER_STATS
	render_stat_sectors = 0;
	render_stat_lines = 0;
	render_stat_planes = 0;
	render_stat_walls = 0;
#endif

	render_raw_view(&camera->pos, camera->prop.viewz);

#ifdef RENDER_STATS
	printf("-= FRAME STATS =-\n");
	printf("sectors %u\n", render_stat_sectors);
	printf("lines %u\n", render_stat_lines);
	printf("planes %u\n", render_stat_planes);
	printf("walls %u\n", render_stat_walls);
#endif

	if(camera->pos.sector->stuff.palette)
		x16g_update_palette(0);
}

