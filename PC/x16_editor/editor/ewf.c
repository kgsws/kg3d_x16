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
#include "things.h"
#include "editor.h"

//
// INPUT

static int32_t inwf_change_mode()
{
	// find camera position
	kge_thing_t *thing = thing_local_player;
	float z = thing->pos.z + thing->prop.viewheight;
	link_entry_t *ent;

	thing->pos.sector = NULL;

	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);
		float height[2];

		height[0] = sec->plane[PLANE_BOT].height;
		height[1] = sec->plane[PLANE_TOP].height;

		if(	z >= height[0] && z <= height[1] &&
			edit_is_point_in_sector(thing->pos.x, thing->pos.y, sec, 1)
		){
			thing->pos.sector = sec;
			break;
		}

		ent = ent->next;
	}

	thing_update_sector(thing, 1);

	if(thing->pos.sector && edit_camera_thing != thing)
	{
		edit_camera_thing->pos = thing->pos;
		thing_update_sector(edit_camera_thing, 1);
	}

	// change mode
	editor_change_mode(EDIT_MODE_2D_OR_3D);

	return 1;
}

// lists
static const edit_input_func_t infunc_normal[] =
{
	{INPUT_EDITOR_EDITMODE, inwf_change_mode},
	// terminator
	{-1}
};

void ewf_input()
{
	tick_make_cmd(&tick_local_cmd, thing_local_player);
	edit_handle_input(infunc_normal);
}

//
// mode

void ewf_mode_set()
{
	system_mouse_grab(1);
	input_clear();
	tick_make_cmd(&tick_local_cmd, thing_local_player);
	system_draw = ewf_draw;
	system_input = ewf_input;
	system_tick = ewf_tick;
}

//
// DRAW

void ewf_draw()
{
	link_entry_t *ent;
	float angle[2];
	kge_xyz_vec_t vpos;

	shader_mode_color();

	// extra mode check
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	// position
	angle[0] = ANGLE_TO_RAD(thing_local_camera->pos.angle);
	angle[1] = PITCH_TO_RAD(thing_local_camera->pos.pitch);

	vpos.x = thing_local_camera->pos.x;
	vpos.y = thing_local_camera->pos.y;
	vpos.z = thing_local_camera->pos.z + thing_local_camera->prop.viewheight;

	// update matrix
	matrix_rotate_x(r_work_matrix + 0, &mat_identity, angle[1] + M_PI * 0.5f);
	matrix_rotate_z(r_work_matrix + 1, r_work_matrix + 0, -angle[0]);
	matrix_translate(r_work_matrix + 0, r_work_matrix + 1, -vpos.x, -vpos.y, -vpos.z);
	matrix_mult((matrix3d_t*)shader_buffer.projection.matrix, &r_projection_3d, r_work_matrix + 0);

	// wireframe
	apply_4f(shader_buffer.shading.color, editor_color[EDITCOLOR_LINE_SOLID].color);

	shader_changed = 1;
	shader_update();

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

			v0 = line->vertex[0];
			v1 = line->vertex[1];

			gl_vertex_buf[0].x = v1->x;
			gl_vertex_buf[0].y = v1->y;
			gl_vertex_buf[1].x = v0->x;
			gl_vertex_buf[1].y = v0->y;
			gl_vertex_buf[2].x = v0->x;
			gl_vertex_buf[2].y = v0->y;
			gl_vertex_buf[3].x = v1->x;
			gl_vertex_buf[3].y = v1->y;

			if(line->backsector)
			{
				kge_sector_t *bs = line->backsector;
				float height[2];
				float beight[2];

				height[0] = sec->plane[PLANE_BOT].height;
				height[1] = sec->plane[PLANE_BOT].height;

				beight[0] = bs->plane[PLANE_BOT].height;
				beight[1] = bs->plane[PLANE_BOT].height;

				gl_vertex_buf[0].z = beight[1];
				gl_vertex_buf[1].z = beight[0];
				gl_vertex_buf[2].z = height[0];
				gl_vertex_buf[3].z = height[1];
				if(beight[1] >= height[0] || beight[1] >= height[0])
					glDrawArrays(GL_LINE_LOOP, 0, 4);

				height[0] = bs->plane[PLANE_TOP].height;
				height[1] = bs->plane[PLANE_TOP].height;

				beight[0] = sec->plane[PLANE_TOP].height;
				beight[1] = sec->plane[PLANE_TOP].height;

				gl_vertex_buf[0].z = beight[1];
				gl_vertex_buf[1].z = beight[0];
				gl_vertex_buf[2].z = height[0];
				gl_vertex_buf[3].z = height[1];
				if(beight[1] >= height[0] || beight[1] >= height[0])
					glDrawArrays(GL_LINE_LOOP, 0, 4);
			} else
			{
				gl_vertex_buf[0].z = sec->plane[PLANE_TOP].height;
				gl_vertex_buf[1].z = sec->plane[PLANE_TOP].height;
				gl_vertex_buf[2].z = sec->plane[PLANE_BOT].height;
				gl_vertex_buf[3].z = sec->plane[PLANE_BOT].height;
				glDrawArrays(GL_LINE_LOOP, 0, 4);
			}
		}

		ent = ent->next;
	}

	glEnable(GL_DEPTH_TEST);
}

//
// TICK

void ewf_tick(uint32_t count)
{
	kge_thing_t *thing = thing_local_player;
	kge_ticcmd_t *tick_move = thing->ticcmd;
	float speed;

	thing->prop.speed = input_shift ? 10.0f : 2.5f;

	// level time is not advanced in this mode
	for(uint32_t i = 0; i < count; i++)
	{
		// UI
		edit_ui_tick();

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

		if(thing->mom.x)
		{
			thing->pos.x += thing->mom.x;
			if(thing->mom.x < MIN_VELOCITY && thing->mom.x > -MIN_VELOCITY)
				thing->mom.x = 0;
			else
				thing->mom.x *= FRICTION_DEFAULT;
		}

		if(thing->mom.y)
		{
			thing->pos.y += thing->mom.y;
			if(thing->mom.y < MIN_VELOCITY && thing->mom.y > -MIN_VELOCITY)
				thing->mom.y = 0;
			else
				thing->mom.y *= FRICTION_DEFAULT;
		}

		if(thing->mom.z)
		{
			thing->pos.z += thing->mom.z;
			if(thing->mom.z < MIN_VELOCITY && thing->mom.z > -MIN_VELOCITY)
				thing->mom.z = 0;
			else
			if(thing->prop.gravity <= 0.0f)
				thing->mom.z *= FRICTION_DEFAULT;
		}
	}
}

