#include "inc.h"
#include "defs.h"
#include "shader.h"
#include "list.h"
#include "engine.h"
#include "matrix.h"
#include "render.h"
#include "kfn.h"
#include "glui.h"
#include "ui_def.h"

#ifdef GLUI_INIT_TEXT
extern const glui_init_text_t glui_init_text[GLUI_INIT_TEXT];
#endif

static glui_container_t fake_container =
{
	.count = 1,
	.elements[0] = NULL
};

static int32_t mouse_pos[2];
static int32_t input_mouse_pos[2];
static uint32_t input_priority;
static glui_element_t *input_elm;

static uint_fast8_t dummy_forced;

//
// drawers

void glui_df_dummy(glui_element_t *elm, int32_t x, int32_t y)
{
	uint32_t do_solid, do_border;
	int32_t xx, yy;
	uint32_t bord, back;

	back = elm->base.color[!!elm->base.selected];
	bord = elm->base.border_color[!!elm->base.selected];

	do_solid = back & 0xFF000000 && elm->base.width && elm->base.height;
	do_border = bord & 0xFF000000 && elm->base.border_size > 0 && (elm->base.width || elm->base.height);

	if(!do_solid && !do_border && !dummy_forced)
		return;

	xx = x + (int32_t)elm->base.width;
	yy = y + (int32_t)elm->base.height;

	gl_vertex_buf[0].x = x;
	gl_vertex_buf[0].y = y;
	gl_vertex_buf[0].z = 0;

	gl_vertex_buf[1].x = x;
	gl_vertex_buf[1].y = yy;
	gl_vertex_buf[1].z = 0;

	gl_vertex_buf[2].x = xx;
	gl_vertex_buf[2].y = yy;
	gl_vertex_buf[2].z = 0;

	gl_vertex_buf[3].x = xx;
	gl_vertex_buf[3].y = y;
	gl_vertex_buf[3].z = 0;

	if(do_solid)
	{
		shader_color32(back);
		shader_mode_color();
		shader_update();
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	if(do_border)
	{
		glLineWidth(elm->base.border_size);
		shader_color32(bord);
		shader_mode_color();
		shader_update();

		gl_vertex_buf[0].x += 0.5f;
		gl_vertex_buf[0].y += 0.5f;
		gl_vertex_buf[1].x += 0.5f;
		gl_vertex_buf[1].y -= 0.5f;
		gl_vertex_buf[2].x -= 0.5f;
		gl_vertex_buf[2].y -= 0.5f;
		gl_vertex_buf[3].x -= 0.5f;
		gl_vertex_buf[3].y += 0.5f;

		glDrawArrays(GL_LINE_LOOP, 0, 4);
	}
}

void glui_df_text(glui_element_t *elm, int32_t x, int32_t y)
{
	int32_t xx, yy;
	uint32_t color;

	glui_df_dummy(elm, x, y);

	if(!elm->text.width)
		return;

	if(!elm->text.height)
		return;

	color = elm->text.color[!!elm->base.selected];

	if(!(color & 0xFF000000))
		return;

	x += elm->text.x;
	y += elm->text.y;

	xx = x + (int32_t)elm->text.width;
	yy = y + (int32_t)elm->text.height;

	gl_vertex_buf[0].x = x;
	gl_vertex_buf[0].y = y;
	gl_vertex_buf[0].z = 0;
	gl_vertex_buf[0].s = 0.0f;
	gl_vertex_buf[0].t = 0.0f;

	gl_vertex_buf[1].x = x;
	gl_vertex_buf[1].y = yy;
	gl_vertex_buf[1].z = 0;
	gl_vertex_buf[1].s = 0.0f;
	gl_vertex_buf[1].t = 1.0f;

	gl_vertex_buf[2].x = xx;
	gl_vertex_buf[2].y = yy;
	gl_vertex_buf[2].z = 0;
	gl_vertex_buf[2].s = 1.0f;
	gl_vertex_buf[2].t = 1.0f;

	gl_vertex_buf[3].x = xx;
	gl_vertex_buf[3].y = y;
	gl_vertex_buf[3].z = 0;
	gl_vertex_buf[3].s = 1.0f;
	gl_vertex_buf[3].t = 0.0f;

	glBindTexture(GL_TEXTURE_2D, elm->text.gltex);

	shader_color32(color);
	shader_mode_texture_alpha();
	shader_update();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void glui_df_image(glui_element_t *elm, int32_t x, int32_t y)
{
	dummy_forced = elm->image.gltex != NULL;
	glui_df_dummy(elm, x, y);
	dummy_forced = 0;

	if(!elm->image.gltex)
		return;

	gl_vertex_buf[0].s = elm->image.coord.s[0];
	gl_vertex_buf[0].t = elm->image.coord.t[0];

	gl_vertex_buf[1].s = elm->image.coord.s[0];
	gl_vertex_buf[1].t = elm->image.coord.t[1];

	gl_vertex_buf[2].s = elm->image.coord.s[1];
	gl_vertex_buf[2].t = elm->image.coord.t[1];

	gl_vertex_buf[3].s = elm->image.coord.s[1];
	gl_vertex_buf[3].t = elm->image.coord.t[0];

	glBindTexture(GL_TEXTURE_2D, *elm->image.gltex);

	shader_buffer.mode.vertex = 0;
	shader_buffer.mode.fragment = elm->image.shader;
	shader_buffer.colormap = elm->image.colormap;
	shader_buffer.lightmap = 0;
	shader_changed = 1;

	shader_update();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void glui_df_container(glui_element_t *elm, int32_t x, int32_t y)
{
	glui_df_dummy(elm, x, y);

	for(uint32_t i = 0; i < elm->container.count; i++)
	{
		glui_element_t *ent = elm->container.elements[i];
		int32_t xx, yy;

		if(ent->base.disabled > 0)
			continue;

		xx = x + ent->base.x;
		yy = y + ent->base.y;

		switch(ent->base.align & 0x3)
		{
			case 1:
				xx -= (int32_t)ent->base.width / 2;
			break;
			case 2:
				xx -= (int32_t)ent->base.width;
			break;
		}

		switch(ent->base.align & 0xC)
		{
			case 4:
				yy -= (int32_t)ent->base.height / 2;
			break;
			case 8:
				yy -= (int32_t)ent->base.height;
			break;
		}

		ent->base.draw(ent, xx, yy);
	}
}

//
// input

static void recursive_mouse(glui_container_t *cont, int32_t x, int32_t y, uint32_t priority)
{
	for(uint32_t i = 0; i < cont->count; i++)
	{
		glui_element_t *ent = cont->elements[i];
		int32_t xx, yy;
		uint32_t prio = priority;

		if(ent->base.disabled)
			continue;

		xx = x + ent->base.x;
		yy = y + ent->base.y;

		switch(ent->base.align & 0x3)
		{
			case 1:
				xx -= (int32_t)ent->base.width / 2;
			break;
			case 2:
				xx -= (int32_t)ent->base.width;
			break;
		}

		switch(ent->base.align & 0xC)
		{
			case 4:
				yy -= (int32_t)ent->base.height / 2;
			break;
			case 8:
				yy -= (int32_t)ent->base.height;
			break;
		}

		ent->base.selected = 0;

		if(	ent->base.draw == glui_df_container &&
			ent->container.priority
		){
			prio = ent->container.priority;
			if(prio > input_priority)
			{
				input_elm = NULL;
				input_priority = prio;
			}
		}

		if(	prio >= input_priority &&
			mouse_pos[0] >= xx &&
			mouse_pos[0] < xx + (int32_t)ent->base.width &&
			mouse_pos[1] >= yy &&
			mouse_pos[1] < yy + (int32_t)ent->base.height
		){
			input_elm = ent;
			input_mouse_pos[0] = mouse_pos[0] - xx;
			input_mouse_pos[1] = mouse_pos[1] - yy;
		}

		if(ent->base.draw == glui_df_container)
			recursive_mouse(&ent->container, xx, yy, prio);
	}
}

static void recursive_input(glui_container_t *cont, int32_t x, int32_t y, uint32_t priority)
{
	for(uint32_t i = 0; i < cont->count; i++)
	{
		glui_element_t *ent = cont->elements[i];
		int32_t xx, yy;
		uint32_t prio = priority;

		if(ent->base.disabled)
			continue;

		if(ent->base.draw != glui_df_container)
			continue;

		xx = x + ent->base.x;
		yy = y + ent->base.y;

		switch(ent->base.align & 0x3)
		{
			case 1:
				xx -= (int32_t)ent->base.width / 2;
			break;
			case 2:
				xx -= (int32_t)ent->base.width;
			break;
		}

		switch(ent->base.align & 0xC)
		{
			case 4:
				yy -= (int32_t)ent->base.height / 2;
			break;
			case 8:
				yy -= (int32_t)ent->base.height;
			break;
		}

		ent->base.selected = 0;

		if(ent->container.priority)
		{
			prio = ent->container.priority;
			if(prio > input_priority)
			{
				input_elm = NULL;
				input_priority = prio;
			}
		}

		if(	prio >= input_priority &&
			ent->container.input
		)
			input_elm = ent;

		recursive_input(&ent->container, xx, yy, prio);
	}
}

glui_element_t *element_from_mouse(glui_element_t *root, int32_t mousex, int32_t mousey)
{
	fake_container.elements[0] = root;

	mouse_pos[0] = mousex;
	mouse_pos[1] = mousey;
	input_priority = 0;
	input_elm = NULL;
	recursive_mouse(&fake_container, 0, 0, 0);

	return input_elm;
}

//
// API

uint32_t glui_init()
{
#ifdef GLUI_INIT_TEXT
	uint32_t *gltex;

	gltex = malloc(GLUI_INIT_TEXT * sizeof(uint32_t));
	if(!gltex)
		return 1;

	glGenTextures(GLUI_INIT_TEXT, gltex);

	for(uint32_t i = 0; i < GLUI_INIT_TEXT; i++)
	{
		const glui_init_text_t *it = glui_init_text + i;

		it->elm->gltex = gltex[i];

		glBindTexture(GL_TEXTURE_2D, gltex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		if(it->text && it->font)
			glui_set_text(it->elm, it->text, it->font, it->align);
	}

	free(gltex);
#endif
	return 0;
}

uint32_t glui_draw(glui_element_t *root, int32_t mousex, int32_t mousey)
{
	if(element_from_mouse(root, mousex, mousey))
		input_elm->base.selected = 1;

	glui_df_container((glui_element_t*)&fake_container, 0, 0);

	glLineWidth(1.0f);

	return input_priority;
}

int32_t glui_key_input(glui_element_t *root, uint32_t magic)
{
	fake_container.elements[0] = root;

	input_priority = 0;
	input_elm = NULL;
	recursive_input(&fake_container, 0, 0, 0);

	if(!input_elm)
		return 0;

	if(!input_elm->container.input)
		return 0;

	return input_elm->container.input(input_elm, magic);
}

int32_t glui_mouse_click(glui_element_t *root, int32_t mousex, int32_t mousey)
{
	if(!element_from_mouse(root, mousex, mousey))
		return 0;

	if(!input_elm->base.click)
		return 0;

	return input_elm->base.click(input_elm, input_mouse_pos[0], input_mouse_pos[1]);
}

void glui_set_text(glui_text_t *ui_text, const uint8_t *text, const void *font, uint32_t align)
{
	kfn_extents_t extents;

	kfn_text_extents(&extents, text, font, align);

	ui_text->width = extents.width;
	ui_text->height = extents.height;

	if(!extents.width)
		return;

	if(!extents.height)
		return;

	extents.dest = malloc(extents.width * extents.height);
	if(!extents.dest)
		return;

	memset(extents.dest, 0, extents.width * extents.height);

	kfn_text_render(&extents, text, font, align);

	glBindTexture(GL_TEXTURE_2D, ui_text->gltex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, extents.width, extents.height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, extents.dest);

	free(extents.dest);

	ui_text->x = extents.ox;
	ui_text->y = extents.oy - extents.oh;

	switch(align & 0x3)
	{
		case 1:
			ui_text->x += ui_text->base.width / 2;
		break;
		case 2:
			ui_text->x += ui_text->base.width;
		break;
	}

	switch(align & 0xC)
	{
		case 4:
			ui_text->y += ui_text->base.height / 2;
		break;
		case 8:
			ui_text->y += ui_text->base.height;
		break;
	}
}

