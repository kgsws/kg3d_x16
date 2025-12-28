#include "inc.h"
#include "defs.h"
#include "list.h"
#include "engine.h"
#include "system.h"
#include "shader.h"
#include "matrix.h"
#include "things.h"
#include "render.h"
#include "input.h"
#include "tick.h"
#include "editor.h"
#include "x16e.h"
#include "x16r.h"
#include "x16g.h"
#include "x16t.h"

typedef struct
{
	float x, y, s;
} e3d_preview_pos_t;

//

uint32_t e3d_highlight = 3;
static uint32_t e3d_preview = 1;

static int32_t thing_clipboard = -1;
static kge_x16_tex_t tex_clipboard_wall;
static kge_x16_tex_t tex_clipboard_plane;
static kge_x16_tex_t tex_clipboard_masked;
static float height_clipboard[2] = {256, 0};

//

static const uint8_t *txt_floor_ceiling[] =
{
	"Ceiling",
	"Floor",
};

static const uint8_t *txt_highlight[] =
{
	"none",
	"tooltip",
	"outline",
	"full",
};

static const uint8_t *txt_preview[] =
{
	"none",
	"small",
	"medium",
	"large",
};

static const e3d_preview_pos_t e3d_preview_pos[] =
{
	// corner
	{},
	{860, 644, 1},
	{700, 524, 2},
	{540, 404, 3},
	// center
	{},
	{352, 264, 2},
	{192, 144, 4},
	{32, 24, 6},
};

//
// changes

static const uint8_t *change_sector_light(kge_sector_t *sec, int32_t change)
{
	int32_t lidx = sec->light.idx;

	lidx += change;

	if(lidx < 0)
		lidx = x16_num_lights - 1;
	else
	if(lidx >= x16_num_lights)
		lidx = 0;

	sec->light.idx = lidx;
	strcpy(sec->light.name, editor_light[lidx].name);

	return sec->light.name;
}

static const uint8_t *change_sector_palette(kge_sector_t *sec, int32_t change)
{
	int32_t pidx = sec->stuff.palette;

	pidx += change;

	if(pidx < 0)
		pidx = (MAX_X16_PALETTE / 4) - 1;
	else
	if(pidx >= MAX_X16_PALETTE / 4)
		pidx = 0;

	sec->stuff.palette = pidx;

	return x16g_palette_name[pidx * 4];
}

static float change_plane_height(kge_sector_t *sec, float change, uint32_t idx)
{
	float height = sec->plane[idx].height + change;

	if(height < -8192)
		height = -8192;
	else
	if(height > 8192)
		height = 8192;

	sec->plane[idx].height = height;

	vc_editor++;

	if(sec->plane[idx].link)
	{
		edit_mark_sector_things(sec->plane[idx].link);
		sec->plane[idx].link->plane[idx ^ 1].height = height;
	}

	edit_mark_sector_things(sec);
	edit_fix_marked_things_z(1);

	return height;
}

//
// INPUT

static int32_t in3d_change_mode()
{
	if(input_ctrl)
		editor_change_mode(EDIT_MODE_WIREFRAME);
	else
		editor_change_mode(EDIT_MODE_2D);
	return 1;
}

static int32_t in3d_increase()
{
	float tmp, change;

	if(input_shift)
		change = 1.0f;
	else
		change = 8.0f;

	if(edit_hit.sector)
	{
		if(input_ctrl)
		{
			edit_status_printf("Sector light: %s", change_sector_light(edit_hit.sector, 1));
			return 1;
		}

		if(input_alt)
		{
			edit_status_printf("Sector palette: %s", change_sector_palette(edit_hit.sector, 1));
			return 1;
		}

		tmp = change_plane_height(edit_hit.sector, change, edit_hit.idx);
		edit_status_printf("%s height: %.0f", txt_floor_ceiling[edit_hit.idx], tmp);

		return 1;
	}

	if(edit_hit.line)
	{
		if(input_ctrl)
		{
			edit_status_printf("Sector light: %s", change_sector_light(edit_hit.extra_sector, 1));
			return 1;
		}

		if(input_alt)
		{
			edit_status_printf("Sector palette: %s", change_sector_palette(edit_hit.extra_sector, 1));
			return 1;
		}

		if(edit_hit.line->texture_split == INFINITY)
			return 1;

		edit_hit.line->texture_split += change;
		edit_status_printf("%s height: %.0f", "Split", edit_hit.line->texture_split);

		return 1;
	}

	if(edit_hit.thing)
	{
		if(input_alt)
		{
			edit_hit.thing->pos.angle += (uint32_t)change * 1024;
			edit_status_printf("Thing angle: %u", (0x100 - (edit_hit.thing->pos.angle >> 8)) & 0xFF);
			return 1;
		} else
		if(!input_ctrl)
		{
			edit_hit.thing->pos.z += change;
			edit_fix_thing_z(edit_hit.thing, 0);
		} else
			edit_hit.thing->pos.z = edit_hit.thing->prop.ceilingz - edit_hit.thing->prop.height;

		edit_status_printf("%s height: %.0f", "Thing", edit_hit.thing->pos.z);

		return 1;
	}

	return 0;
}

static int32_t in3d_decrease()
{
	float tmp, change;

	if(input_shift)
		change = -1.0f;
	else
		change = -8.0f;

	if(edit_hit.sector)
	{
		if(input_ctrl)
		{
			edit_status_printf("Sector light: %s", change_sector_light(edit_hit.sector, -1));
			return 1;
		}

		if(input_alt)
		{
			edit_status_printf("Sector palette: %s", change_sector_palette(edit_hit.sector, -1));
			return 1;
		}

		tmp = change_plane_height(edit_hit.sector, change, edit_hit.idx);
		edit_status_printf("%s height: %.0f", txt_floor_ceiling[edit_hit.idx], tmp);
		return 1;
	}

	if(edit_hit.line)
	{
		if(input_ctrl)
		{
			edit_status_printf("Sector light: %s", change_sector_light(edit_hit.extra_sector, -1));
			return 1;
		}

		if(input_alt)
		{
			edit_status_printf("Sector palette: %s", change_sector_palette(edit_hit.extra_sector, -1));
			return 1;
		}

		if(edit_hit.line->texture_split == INFINITY)
			return 1;

		edit_hit.line->texture_split += change;
		edit_status_printf("%s height: %.0f", "Split", edit_hit.line->texture_split);

		return 1;
	}

	if(edit_hit.thing)
	{
		if(input_alt)
		{
			change = -change;
			edit_hit.thing->pos.angle -= (uint32_t)change * 1024;
			edit_status_printf("Thing angle: %u", (0x100 - (edit_hit.thing->pos.angle >> 8)) & 0xFF);
			return 1;
		} else
		if(!input_ctrl)
		{
			edit_hit.thing->pos.z += change;
			edit_fix_thing_z(edit_hit.thing, 0);
		} else
			edit_hit.thing->pos.z = edit_hit.thing->prop.floorz;

		edit_status_printf("%s height: %.0f", "Thing", edit_hit.thing->pos.z);

		return 1;
	}

	return 0;
}

static int32_t in3d_delete()
{
	if(edit_hit.thing)
	{
		if(edit_hit.thing != thing_local_player)
		{
			thing_remove(edit_hit.thing);
			edit_clear_hit(1);
		}

		return 1;
	}

	return 1;
}

static int32_t in3d_lock()
{
	if(thing_local_player != edit_camera_thing)
		return 1;

	if(edit_hit.locked)
		edit_hit.locked = 0;
	else
	if(edit_hit.sector || edit_hit.line || edit_hit.thing)
		edit_hit.locked = 1;

	return 1;
}

static int32_t in3d_pick()
{
	if(edit_camera_thing != thing_local_player)
	{
		edit_enter_thing(edit_camera_thing);
		return 1;
	}

	if(input_alt)
	{
		kge_sector_t *sec;

		sec = edit_hit.sector;
		if(!sec)
			sec = edit_hit.extra_sector;

		if(!sec)
			return 1;

		edit_ui_palette_select(&sec->stuff.palette);

		return 1;
	}

	if(input_ctrl)
	{
		kge_sector_t *sec;

		if(edit_hit.thing)
		{
			edit_enter_thing(edit_hit.thing);
			return 1;
		}

		if(edit_hit.line)
		{
			if(edit_hit.idx > 1)
				edit_ui_blocking_select("Select mid blocking bits.", &edit_hit.line->info.blockmid, -1);
			return 1;
		}

		sec = edit_hit.sector;
		if(!sec)
			sec = edit_hit.extra_sector;

		if(!sec)
			return 1;

		edit_ui_light_select(&sec->light);

		return 1;
	}

	if(edit_hit.thing)
	{
		edit_ui_thing_select(edit_hit.thing);
		return 1;
	}

	if(edit_hit.sector)
	{
		edit_ui_texture_select("Select plane texture.", &edit_hit.sector->plane[edit_hit.idx].texture, 1);
		return 1;
	}

	if(edit_hit.line)
	{
		edit_ui_texture_select(edit_hit.idx & 2 ? "Select masked texture." : "Select wall texture.", &edit_hit.line->texture[edit_hit.idx], edit_hit.idx & 2);
		return 1;
	}

	return 1;
}

static int32_t in3d_paste()
{
	kge_x16_tex_t *dest = NULL;

	if(edit_hit.thing)
	{
		if(thing_clipboard >= 0)
		{
			edit_hit.thing->prop.type = thing_clipboard;
			edit_update_thing_type(edit_hit.thing);
			edit_status_printf("Thing type pasted.");
			return 1;
		}
	} else
	if(edit_hit.sector)
		dest = &edit_hit.sector->plane[edit_hit.idx].texture;
	else
	if(edit_hit.line)
		dest = &edit_hit.line->texture[edit_hit.idx];

	if(!dest)
		return 1;

	if(edit_hit.sector)
	{
		if(!input_ctrl)
		{
			copy_texture(dest, &tex_clipboard_plane);
			edit_status_printf("Plane texture pasted.");
			return 1;
		}

		*dest = tex_clipboard_plane;

		if(input_shift)
		{
			edit_hit.sector->plane[edit_hit.idx].height = height_clipboard[edit_hit.idx];
			change_plane_height(edit_hit.sector, 0, edit_hit.idx);
			edit_status_printf("Full plane info pasted.");
			return 1;
		}

		edit_status_printf("Plane texture info pasted.");

		return 1;
	}

	if(edit_hit.line)
	{
		const uint8_t *txt;
		kge_x16_tex_t *src;

		if(edit_hit.idx > 1)
		{
			txt = "Masked";
			src = &tex_clipboard_masked;
		} else
		{
			txt = "Wall";
			src = &tex_clipboard_wall;
		}

		if(!input_ctrl)
		{
			copy_texture(dest, src);
			edit_status_printf("%s texture pasted.", txt);
			return 1;
		}

		edit_status_printf("%s texture info pasted.", txt);
		*dest = *src;

		return 1;
	}

	return 1;
}

static int32_t in3d_copy()
{
	kge_x16_tex_t *source = NULL;

	if(edit_hit.thing)
	{
		if(edit_hit.thing->prop.type < MAX_X16_THING_TYPES)
		{
			thing_clipboard = edit_hit.thing->prop.type;
			edit_status_printf("Thing type copied.");
			return 1;
		}
	} else
	if(edit_hit.sector)
		source = &edit_hit.sector->plane[edit_hit.idx].texture;
	else
	if(edit_hit.line)
		source = &edit_hit.line->texture[edit_hit.idx];

	if(!source)
		return 1;

	if(edit_hit.sector)
	{
		if(!input_ctrl)
		{
			copy_texture(&tex_clipboard_plane, source);
			edit_status_printf("Plane texture copied.");
			return 1;
		}

		tex_clipboard_plane = *source;

		if(input_shift)
		{
			height_clipboard[edit_hit.idx] = edit_hit.sector->plane[edit_hit.idx].height;

			edit_status_printf("Full plane info copied.");
			return 1;
		}

		edit_status_printf("Plane texture info copied.");

		return 1;
	}

	if(edit_hit.line)
	{
		const uint8_t *txt;
		kge_x16_tex_t *dst;

		if(edit_hit.idx > 1)
		{
			txt = "Masked";
			dst = &tex_clipboard_masked;
		} else
		{
			txt = "Wall";
			dst = &tex_clipboard_wall;
		}

		if(!input_ctrl)
		{
			copy_texture(dst, source);
			edit_status_printf("%s texture copied.", txt);
			return 1;
		}

		edit_status_printf("%s texture info copied.", txt);
		*dst = *source;
		return 1;
	}

	return 1;
}

static int32_t in3d_highlight()
{
	e3d_highlight++;
	if(e3d_highlight > 3)
		e3d_highlight = 0;

	edit_highlight_changed();

	edit_status_printf("Highlight: %s", txt_highlight[e3d_highlight]);

	return 1;
}

static int32_t in3d_preview()
{
	uint32_t tmp = e3d_preview & 4;

	if(input_ctrl)
	{
		if(e3d_preview & 3)
		{
			e3d_preview ^= 4;
			edit_status_printf("Preview: %s", e3d_preview & 4 ? "center" : "corner");
		}
		return 1;
	}

	e3d_preview &= 3;

	e3d_preview++;
	if(e3d_preview > 3)
		e3d_preview = 0;

	edit_status_printf("Preview: %s", txt_preview[e3d_preview]);

	e3d_preview |= tmp;

	return 1;
}

static int32_t in3d_offs_reset()
{
	kge_x16_tex_t *dest = NULL;

	if(edit_hit.sector)
		dest = &edit_hit.sector->plane[edit_hit.idx].texture;
	else
	if(edit_hit.line)
		dest = &edit_hit.line->texture[edit_hit.idx];

	if(dest)
	{
		dest->ox = 0;
		dest->oy = 0;
		dest->flags = 0;
		dest->angle = 0;
		edit_status_printf("Texture offsets and rotation reset.");

		return 1;
	}

	return 0;
}

static int32_t in3d_offs_x_inc()
{
	kge_x16_tex_t *dest = NULL;

	if(edit_hit.sector)
		dest = &edit_hit.sector->plane[edit_hit.idx].texture;
	else
	if(edit_hit.line)
		dest = &edit_hit.line->texture[edit_hit.idx];

	if(dest)
	{
		if(input_shift)
			dest->ox++;
		else
			dest->ox += 4;
		edit_status_printf("Offset X: %u", dest->ox);

		return 1;
	}

	return 0;
}

static int32_t in3d_offs_x_dec()
{
	kge_x16_tex_t *dest = NULL;

	if(edit_hit.sector)
		dest = &edit_hit.sector->plane[edit_hit.idx].texture;
	else
	if(edit_hit.line)
		dest = &edit_hit.line->texture[edit_hit.idx];

	if(dest)
	{
		if(input_shift)
			dest->ox--;
		else
			dest->ox -= 4;
		edit_status_printf("Offset X: %u", dest->ox);

		return 1;
	}

	return 0;
}

static int32_t in3d_offs_y_inc()
{
	kge_x16_tex_t *dest = NULL;

	if(edit_hit.sector)
		dest = &edit_hit.sector->plane[edit_hit.idx].texture;
	else
	if(edit_hit.line)
		dest = &edit_hit.line->texture[edit_hit.idx];

	if(dest)
	{
		if(input_shift)
			dest->oy++;
		else
			dest->oy += 4;
		edit_status_printf("Offset Y: %u", dest->oy);

		return 1;
	}

	return 0;
}

static int32_t in3d_offs_y_dec()
{
	kge_x16_tex_t *dest = NULL;

	if(edit_hit.sector)
		dest = &edit_hit.sector->plane[edit_hit.idx].texture;
	else
	if(edit_hit.line)
		dest = &edit_hit.line->texture[edit_hit.idx];

	if(dest)
	{
		if(input_shift)
			dest->oy--;
		else
			dest->oy -= 4;
		edit_status_printf("Offset Y: %u", dest->oy);

		return 1;
	}

	return 0;
}

static int32_t in3d_plane_a_inc()
{
	kge_x16_tex_t *dest = NULL;

	if(edit_hit.sector)
	{
		dest = &edit_hit.sector->plane[edit_hit.idx].texture;

		if(input_shift)
			dest->angle++;
		else
			dest->angle += 8;

		edit_status_printf("Texture rotation: %u", dest->angle);

		return 1;
	}

	return 0;
}

static int32_t in3d_plane_a_dec()
{
	kge_x16_tex_t *dest = NULL;

	if(edit_hit.sector)
	{
		dest = &edit_hit.sector->plane[edit_hit.idx].texture;

		if(input_shift)
			dest->angle--;
		else
			dest->angle -= 8;

		edit_status_printf("Texture rotation: %u", dest->angle);

		return 1;
	}

	return 0;
}

static int32_t in3d_wall_peg_x()
{
	if(edit_hit.line)
	{
		edit_hit.line->info.flags ^= WALLFLAG_PEG_X;
		edit_status_printf("Wall X peg: %s", txt_right_left[!(edit_hit.line->info.flags & WALLFLAG_PEG_X)]);
		return 1;
	}

	return 0;
}

static int32_t in3d_wall_peg_y()
{
	if(edit_hit.line)
	{
		kge_x16_tex_t *dest;
		dest = &edit_hit.line->texture[edit_hit.idx];
		dest->flags ^= TEXFLAG_PEG_Y;
		edit_status_printf("Texture Y peg: %s", txt_top_bot[!(dest->flags & TEXFLAG_PEG_Y)]);
		return 1;
	}

	return 0;
}

static int32_t in3d_wall_mirror_x()
{
	if(edit_hit.line)
	{
		kge_x16_tex_t *dest;
		dest = &edit_hit.line->texture[edit_hit.idx];
		dest->flags ^= TEXFLAG_MIRROR_X;
		edit_status_printf("Texture X mirror: %s", txt_yes_no[!(dest->flags & TEXFLAG_MIRROR_X)]);
		return 1;
	}	

	return 0;
}

static int32_t in3d_wall_mirror_y()
{
	if(edit_hit.line)
	{
		kge_x16_tex_t *dest;

		dest = &edit_hit.line->texture[edit_hit.idx];
		dest->flags ^= TEXFLAG_MIRROR_Y_SWAP_XY; // shared with TEXFLAG_PEG_MID_BACK

		if(editor_texture[dest->idx].type != X16G_TEX_TYPE_WALL_MASKED)
		{
			if(editor_texture[dest->idx].type != X16G_TEX_TYPE_WALL)
				edit_status_printf("Texture rotate: %s", txt_yes_no[!(dest->flags & TEXFLAG_MIRROR_Y_SWAP_XY)]);
			else
				edit_status_printf("Texture Y mirror: %s", txt_yes_no[!(dest->flags & TEXFLAG_MIRROR_Y_SWAP_XY)]);
		} else
			edit_status_printf("Masked side peg: %s", txt_back_front[!(dest->flags & TEXFLAG_PEG_MID_BACK)]);

		return 1;
	}	

	return 0;
}

static int32_t in3d_wall_split()
{
	if(edit_hit.line)
	{
		if(edit_hit.line->backsector)
			return 1;

		if(edit_hit.line->texture_split == INFINITY)
		{
			int32_t split;

			split = (edit_hit.extra_sector->plane[PLANE_TOP].height - edit_hit.extra_sector->plane[PLANE_BOT].height) * 0.5f;
			split += edit_hit.extra_sector->plane[PLANE_BOT].height;
			edit_hit.line->texture_split = split;

			edit_status_printf("Wall texture split created.");
			return 1;
		}

		edit_hit.line->texture_split = INFINITY;
		edit_status_printf("Wall texture split removed.");

		return 1;
	}

	return 0;
}

static int32_t in3d_sector_water()
{
	if(!edit_hit.sector)
		return 0;

	edit_hit.sector->stuff.x16flags ^= SECFLAG_WATER;

	edit_status_printf("Sector water: %s", txt_yes_no[!(edit_hit.sector->stuff.x16flags & SECFLAG_WATER)]);

	return 1;
}

// lists
static const edit_input_func_t infunc_normal[] =
{
	{INPUT_EDITOR_EDITMODE, in3d_change_mode},
	{INPUT_EDITOR_INCREASE, in3d_increase},
	{INPUT_EDITOR_DECREASE, in3d_decrease},
	{INPUT_EDITOR_DELETE, in3d_delete},
	{INPUT_E3D_CLICK_LOCK, in3d_lock},
	{INPUT_E3D_CLICK_PICK, in3d_pick},
	{INPUT_E3D_PASTE, in3d_paste},
	{INPUT_E3D_COPY, in3d_copy},
	{INPUT_E3D_HIGHLIGHT, in3d_highlight},
	{INPUT_E3D_PREVIEW, in3d_preview},
	{INPUT_E3D_OFFS_RESET, in3d_offs_reset},
	{INPUT_E3D_OFFS_X_INC, in3d_offs_x_inc},
	{INPUT_E3D_OFFS_X_DEC, in3d_offs_x_dec},
	{INPUT_E3D_OFFS_Y_INC, in3d_offs_y_inc},
	{INPUT_E3D_OFFS_Y_DEC, in3d_offs_y_dec},
	{INPUT_E3D_PLANE_A_INC, in3d_plane_a_inc},
	{INPUT_E3D_PLANE_A_DEC, in3d_plane_a_dec},
	{INPUT_E3D_WALL_PEG_X, in3d_wall_peg_x},
	{INPUT_E3D_WALL_PEG_Y, in3d_wall_peg_y},
	{INPUT_E3D_WALL_MIRROR_X, in3d_wall_mirror_x},
	{INPUT_E3D_WALL_MIRROR_Y, in3d_wall_mirror_y},
	{INPUT_E3D_WALL_SPLIT, in3d_wall_split},
	{INPUT_E3D_SECTOR_WATER, in3d_sector_water},
	// terminator
	{-1}
};

void e3d_input()
{
	if(edit_ui_input())
		return;

	tick_make_cmd(&tick_local_cmd, thing_local_player);
	edit_handle_input(infunc_normal);
}

//
// mode

void e3d_mode_set()
{
	if(edit_camera_thing != thing_local_player)
		edit_hit.thing = thing_local_player;

	system_mouse_grab(1);
	input_clear();
	tick_make_cmd(&tick_local_cmd, thing_local_player);
	system_draw = e3d_draw;
	system_input = e3d_input;
	system_tick = e3d_tick;
}

//
// DRAW

void e3d_draw()
{
	uint32_t ident = 0;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	if(!edit_hit.locked && !edit_ui_busy && thing_local_player == edit_camera_thing)
	{
		shader_buffer.shading.identity[0] = (screen_width / 2) + 0.5f;
		shader_buffer.shading.identity[1] = (screen_height / 2) + 0.5f;
	}

	render_view(thing_local_camera);

	if(!edit_hit.locked && !edit_ui_busy && thing_local_player == edit_camera_thing)
	{
		glReadPixels(screen_width / 2, screen_height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &ident);
		ident &= 0xFFFFFF;

		shader_buffer.shading.identity[0] = -1.0f;

		edit_clear_hit(0);

		if(ident && ident < ident_idx)
		{
			ident_slot_t *is = ident_slot + ident;

			switch(is->type)
			{
				case IDENT_WALL_TOP:
					edit_hit.idx = 0;
					edit_hit.line = is->line;
					edit_hit.extra_sector = is->sector;
				break;
				case IDENT_WALL_BOT:
					edit_hit.idx = 1;
					edit_hit.line = is->line;
					edit_hit.extra_sector = is->sector;
				break;
				case IDENT_WALL_MID:
					edit_hit.idx = 2;
					edit_hit.line = is->line;
					edit_hit.extra_sector = is->sector;
				break;
				case IDENT_PLANE_TOP:
					edit_hit.idx = 0;
					edit_hit.sector = is->sector;
				break;
				case IDENT_PLANE_BOT:
					edit_hit.idx = 1;
					edit_hit.sector = is->sector;
				break;
				case IDENT_THING:
					edit_hit.thing = is->thing;
					edit_hit.extra_sector = is->sector;
				break;
			}
		}
	}

	edit_highlight_changed();

	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if(e3d_highlight > 1)
	{
		glDisable(GL_DEPTH_TEST);

		// highlight: sector
		if(edit_hit.sector)
		{
			float temp;

			apply_4f(shader_buffer.shading.color, editor_color[EDITCOLOR_SECTOR_HIGHLIGHT].color);

			if(e3d_highlight > 2)
			{
				shader_buffer.shading.color[3] = edit_highlight_alpha;
				r_draw_plane(edit_hit.sector, edit_hit.idx | PLANE_USE_COLOR);
			}

			shader_buffer.shading.color[3] = editor_color[EDITCOLOR_SECTOR_HIGHLIGHT].color[3];
			shader_mode_color();
			shader_update();

			temp = edit_hit.sector->plane[edit_hit.idx].height;

			for(uint32_t i = 0; i < edit_hit.sector->line_count; i++)
			{
				kge_line_t *line = edit_hit.sector->line + i;

				gl_vertex_buf[i].x = line->vertex[0]->x;
				gl_vertex_buf[i].y = line->vertex[0]->y;
				gl_vertex_buf[i].z = temp;
			}

			glDrawArrays(GL_LINE_LOOP, 0, edit_hit.sector->line_count);
		}

		// highlight: wall
		if(edit_hit.line)
		{
			float top, bot;
			kge_vertex_t *v0 = edit_hit.line->vertex[0];
			kge_vertex_t *v1 = edit_hit.line->vertex[1];

			apply_4f(shader_buffer.shading.color, editor_color[EDITCOLOR_SECTOR_HIGHLIGHT].color);
			shader_mode_color();

			if(edit_hit.line->backsector)
			{
				if(edit_hit.idx > 1)
				{
					kge_x16_tex_t *ti = edit_hit.line->texture + 2;
					editor_texture_t *et = editor_texture + ti->idx;
					kge_sector_t *sec = edit_hit.extra_sector;

					if(ti->flags & TEXFLAG_PEG_MID_BACK)
						sec = edit_hit.line->backsector;

					bot = sec->plane[PLANE_BOT].height + ti->oy * 2;
					top = bot + et->height * 2;
				} else
				if(edit_hit.idx)
				{
					top = edit_hit.line->backsector->plane[PLANE_BOT].height;
					bot = edit_hit.extra_sector->plane[PLANE_BOT].height;
				} else
				{
					top = edit_hit.extra_sector->plane[PLANE_TOP].height;
					bot = edit_hit.line->backsector->plane[PLANE_TOP].height;
				}
			} else
			{
				if(edit_hit.idx > 1)
				{
					kge_x16_tex_t *ti = edit_hit.line->texture + 2;
					editor_texture_t *et = editor_texture + ti->idx;

					bot = edit_hit.extra_sector->plane[PLANE_BOT].height + ti->oy * 2;
					top = bot + et->height * 2;
				} else
				if(edit_hit.line->texture_split != INFINITY)
				{
					if(edit_hit.idx)
					{
						top = edit_hit.line->texture_split;
						bot = edit_hit.extra_sector->plane[PLANE_BOT].height;
					} else
					{
						top = edit_hit.extra_sector->plane[PLANE_TOP].height;
						bot = edit_hit.line->texture_split;
					}
				} else
				{
					top = edit_hit.extra_sector->plane[PLANE_TOP].height;
					bot = edit_hit.extra_sector->plane[PLANE_BOT].height;
				}
			}

			gl_vertex_buf[0].x = v0->x;
			gl_vertex_buf[0].y = v0->y;
			gl_vertex_buf[0].z = bot;
			gl_vertex_buf[1].x = v0->x;
			gl_vertex_buf[1].y = v0->y;
			gl_vertex_buf[1].z = top;
			gl_vertex_buf[2].x = v1->x;
			gl_vertex_buf[2].y = v1->y;
			gl_vertex_buf[2].z = top;
			gl_vertex_buf[3].x = v1->x;
			gl_vertex_buf[3].y = v1->y;
			gl_vertex_buf[3].z = bot;

			if(e3d_highlight > 2)
			{
				shader_buffer.shading.color[3] = edit_highlight_alpha;
				shader_update();
				glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
				shader_buffer.shading.color[3] = editor_color[EDITCOLOR_SECTOR_HIGHLIGHT].color[3];
			}

			shader_changed = 1;
			shader_update();

			glDrawArrays(GL_LINE_LOOP, 0, 4);
		}

		// highlight: thing
		if(edit_hit.thing)
		{
			kge_thing_t *th = edit_hit.thing;
			float pA[2];
			float pB[2];
			float angle;

			pA[0] = th->pos.x - view_sprite_vec[0] * th->prop.radius;
			pA[1] = th->pos.y - view_sprite_vec[1] * th->prop.radius;
			pB[0] = th->pos.x + view_sprite_vec[0] * th->prop.radius;
			pB[1] = th->pos.y + view_sprite_vec[1] * th->prop.radius;

			apply_4f(shader_buffer.shading.color, editor_color[EDITCOLOR_SECTOR_HIGHLIGHT].color);
			shader_mode_color();
			shader_update();

			gl_vertex_buf[0].x = pA[0];
			gl_vertex_buf[0].y = pA[1];
			gl_vertex_buf[0].z = th->pos.z;

			gl_vertex_buf[1].x = pA[0];
			gl_vertex_buf[1].y = pA[1];
			gl_vertex_buf[1].z = th->pos.z + th->prop.height;

			gl_vertex_buf[2].x = pB[0];
			gl_vertex_buf[2].y = pB[1];
			gl_vertex_buf[2].z = th->pos.z + th->prop.height;

			gl_vertex_buf[3].x = pB[0];
			gl_vertex_buf[3].y = pB[1];
			gl_vertex_buf[3].z = th->pos.z;

			glDrawArrays(GL_LINES, 0, 4); // sides

			// circle + arrow
			angle = ANGLE_TO_RAD(th->pos.angle);
			r_work_matrix[2] = *((matrix3d_t*)shader_buffer.projection.matrix);
			memcpy(gl_vertex_buf, edit_circle_vtx, sizeof(edit_circle_vtx));

			matrix_translate(r_work_matrix + 0, &mat_identity, th->pos.x, th->pos.y, th->pos.z);
			matrix_rotate_z(r_work_matrix + 1, r_work_matrix + 0, angle);
			matrix_scale((matrix3d_t*)shader_buffer.projection.extra, r_work_matrix + 1, th->prop.radius, th->prop.radius, th->prop.radius);
			matrix_mult((matrix3d_t*)shader_buffer.projection.matrix, r_work_matrix + 2, (matrix3d_t*)shader_buffer.projection.extra);

			// bottom
			shader_changed = 1;
			shader_update();
			glDrawArrays(GL_LINE_LOOP, 0, EDIT_THING_CIRCLE_VTX + 4);

			matrix_translate(r_work_matrix + 0, &mat_identity, th->pos.x, th->pos.y, th->pos.z + th->prop.height);
			matrix_rotate_z(r_work_matrix + 1, r_work_matrix + 0, ANGLE_TO_RAD(th->pos.angle));
			matrix_scale((matrix3d_t*)shader_buffer.projection.extra, r_work_matrix + 1, th->prop.radius, th->prop.radius, th->prop.radius);
			matrix_mult((matrix3d_t*)shader_buffer.projection.matrix, r_work_matrix + 2, (matrix3d_t*)shader_buffer.projection.extra);

			// top
			shader_changed = 1;
			shader_update();
			glVertexAttribPointer(shader_attr_vertex, 3, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_t), &gl_vertex_buf[4].x);
			glDrawArrays(GL_LINE_LOOP, 0, EDIT_THING_CIRCLE_VTX);
			glVertexAttribPointer(shader_attr_vertex, 3, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_t), &gl_vertex_buf->x);

			*((matrix3d_t*)shader_buffer.projection.matrix) = r_work_matrix[2];
			shader_changed = 1;
		}

		glEnable(GL_DEPTH_TEST);
	}

	memcpy(shader_buffer.projection.matrix, r_projection_2d.raw, sizeof(matrix3d_t));

	// crosshair
	if(!edit_hit.locked && !edit_ui_busy && thing_local_player == edit_camera_thing)
	{
		apply_4f(shader_buffer.shading.color, editor_color[EDITCOLOR_CROSSHAIR].color);
		shader_mode_color();
		shader_update();
		gl_vertex_buf[0].x = screen_width / 2 - 3;
		gl_vertex_buf[0].y = screen_height / 2 + 1;
		gl_vertex_buf[0].z = 0.0f;
		gl_vertex_buf[1].x = screen_width / 2 + 4;
		gl_vertex_buf[1].y = screen_height / 2 + 1;
		gl_vertex_buf[1].z = 0.0f;
		gl_vertex_buf[2].x = screen_width / 2 + 1;
		gl_vertex_buf[2].y = screen_height / 2 - 3;
		gl_vertex_buf[2].z = 0.0f;
		gl_vertex_buf[3].x = screen_width / 2 + 1;
		gl_vertex_buf[3].y = screen_height / 2 + 4;
		gl_vertex_buf[3].z = 0.0f;
		glDrawArrays(GL_LINES, 0, 4);
	}

	// software 3D preview
	if(e3d_preview)
		x16r_render(thing_local_camera, e3d_preview_pos[e3d_preview].x, e3d_preview_pos[e3d_preview].y, e3d_preview_pos[e3d_preview].s);

	// UI elements
	memcpy(shader_buffer.projection.matrix, r_projection_ui.raw, sizeof(matrix3d_t));
	edit_draw_ui();
}

//
// TICK

void e3d_tick(uint32_t count)
{
	edit_highlight_alpha = 0.1f + 0.04f * sinf((float)gametick * 0.06f);

	thing_local_player->prop.speed = input_shift ? 10.0f : 2.5f;

	for(uint32_t i = 0; i < count; i++)
	{
		edit_ui_tick();
		if(!edit_ui_busy)
			tick_go();
	}
}

