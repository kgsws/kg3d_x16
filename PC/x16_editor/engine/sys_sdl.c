#include "inc.h"
#include "defs.h"
#include "engine.h"
#include "input.h"
#include "shader.h"
#include "editor.h"
#include "matrix.h"
#include "render.h"
#include "system.h"
#include "glui.h"
#include "x16e.h"
#include <SDL2/SDL.h>

#define FRAME_RATE_AVG	8
#define GAME_TICK_LIMIT	2

//

static SDL_Window *sdl_win;
static SDL_GLContext sdl_context;
static uint32_t stopped;

static uint32_t last_tick;
static uint32_t first_tick;

uint32_t screen_width;
uint32_t screen_height;
float screen_aspect;
float screen_scale[2];

uint32_t gametick;
uint_fast8_t system_was_busy;

void (*system_draw)();
void (*system_input)();
void (*system_tick)(uint32_t);
void (*system_text)(uint8_t*,uint32_t);

//
// time measurement (debug)

uint64_t get_app_tick()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_nsec + (uint64_t)ts.tv_sec * 1000000000;
}

//
// input handling

static inline void sys_input()
{
	SDL_Event event;

	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_QUIT:
				stopped = 0x80000000;
			break;
			case SDL_TEXTINPUT:
				if(system_text && event.text.text[0])
					system_text(event.text.text, 0);
			break;
			case SDL_KEYDOWN:
				if(system_text)
				{
					if(event.key.keysym.sym == SDLK_BACKSPACE)
					{
						system_text(NULL, 2);
						break;
					} else
					if(event.key.keysym.sym == SDLK_ESCAPE)
					{
						system_text(NULL, 1);
						break;
					} else
					if(event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
					{
						system_text(NULL, 0);
						break;
					}
				}
				input_process(event.key.keysym.sym, INTYPE_KEY, 1);
				// SHIFT key
				if(event.key.keysym.sym == SDLK_RSHIFT)
					input_shift |= 1;
				if(event.key.keysym.sym == SDLK_LSHIFT)
					input_shift |= 2;
				// CTRL key
				if(event.key.keysym.sym == SDLK_RCTRL)
					input_ctrl |= 1;
				if(event.key.keysym.sym == SDLK_LCTRL)
					input_ctrl |= 2;
				// ALT key
				if(event.key.keysym.sym == SDLK_RALT)
					input_alt |= 1;
				if(event.key.keysym.sym == SDLK_LALT)
					input_alt |= 2;
			break;
			case SDL_KEYUP:
				input_process(event.key.keysym.sym, INTYPE_KEY, 0);
				// SHIFT key
				if(event.key.keysym.sym == SDLK_RSHIFT)
					input_shift &= ~1;
				if(event.key.keysym.sym == SDLK_LSHIFT)
					input_shift &= ~2;
				// CTRL key
				if(event.key.keysym.sym == SDLK_RCTRL)
					input_ctrl &= ~1;
				if(event.key.keysym.sym == SDLK_LCTRL)
					input_ctrl &= ~2;
				// ALT key
				if(event.key.keysym.sym == SDLK_RALT)
					input_alt &= ~1;
				if(event.key.keysym.sym == SDLK_LALT)
					input_alt &= ~2;
			break;
			case SDL_MOUSEBUTTONDOWN:
				input_process(event.button.button, INTYPE_MOUSE_BUTTON, 1);
				mouse_btn |= 1 << event.button.button;
			break;
			case SDL_MOUSEBUTTONUP:
				input_process(event.button.button, INTYPE_MOUSE_BUTTON, 0);
				mouse_btn &= ~(1 << event.button.button);
			break;
			case SDL_MOUSEMOTION:
				input_analog_h += event.motion.xrel;
				input_analog_v -= event.motion.yrel;
				mouse_x = event.motion.x;
				mouse_y = event.motion.y;
				mouse_yy = screen_height - event.motion.y;
			break;
			case SDL_MOUSEWHEEL:
				mouse_wheel_x = event.wheel.x;
				mouse_wheel_y = event.wheel.y;
				// fake keypress
				if(event.wheel.x < 0)
					input_process(MOUSE_WHEEL_X_DOWN, INTYPE_MOUSE_WHEEL, 1);
				if(event.wheel.x > 0)
					input_process(MOUSE_WHEEL_X_UP, INTYPE_MOUSE_WHEEL, 1);
				if(event.wheel.y < 0)
					input_process(MOUSE_WHEEL_Y_DOWN, INTYPE_MOUSE_WHEEL, 1);
				if(event.wheel.y > 0)
					input_process(MOUSE_WHEEL_Y_UP, INTYPE_MOUSE_WHEEL, 1);
			break;
		}
	}
}

//
// time base

static inline void sys_time()
{
	// this is a mess; but it works
	uint32_t ms_now = SDL_GetTicks() - first_tick;
	uint32_t tick_now = ((uint64_t)ms_now * TICKRATE) / 1000;
	uint32_t ms_diff = (ms_now - last_tick) * TICKRATE;
	uint32_t steps;

	static uint32_t avg_buf[FRAME_RATE_AVG];
	static uint32_t avg_total;
	static uint32_t avg_idx;

	last_tick = ms_now;

	if(system_was_busy)
	{
		system_was_busy = 0;
		return;
	}

	// average framerate
	avg_total -= avg_buf[avg_idx];
	avg_buf[avg_idx] = ms_diff;
	avg_total += ms_diff;
	avg_idx++;
	if(avg_idx == FRAME_RATE_AVG)
		avg_idx = 0;
	steps = (avg_total + (500 * FRAME_RATE_AVG)) / (1000 * FRAME_RATE_AVG);

	if(tick_now > gametick)
	{
		// compensate slower frame rate
		if(tick_now - gametick >= GAME_TICK_LIMIT)
			steps += GAME_TICK_LIMIT / 2;
	}

	if(!steps)
		return;

	if(steps >= TICKRATE / 4)
		steps = TICKRATE / 4;

	system_input();
	input_release();

	gametick += steps;
	system_tick(steps);
}

//
// API

int system_init(int argc, void **argv)
{
	//
	// VIDEO
	screen_width = HAX_VIDEO_WIDTH;
	screen_height = HAX_VIDEO_HEIGHT;
	screen_aspect = (float)screen_width / (float)screen_height;

	if(screen_aspect > 0)
	{
		screen_scale[0] = (1.0f / screen_aspect) * (float)screen_width;
		screen_scale[1] = (float)screen_height;
	} else
	{
		screen_scale[0] = (float)screen_width;
		screen_scale[1] = (float)screen_height * screen_aspect;
	}

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0)
		return 1;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	sdl_win = SDL_CreateWindow("kgEdit X16", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, SDL_WINDOW_OPENGL);

	sdl_context = SDL_GL_CreateContext(sdl_win);
	if(!sdl_context)
		return 1;

	if(glewInit() != GLEW_OK)
		return 1;

	SDL_GL_SetSwapInterval(1);

	glViewport(0, 0, screen_width, screen_height);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

	// 3D view
	matrix_perspective(&r_projection_3d, ENGINE_FOV, screen_aspect, 1.0f, FAR_PLANE);

	// 2D view
	martix_ortho(&r_projection_2d, 0.0f, screen_width, 0.0f, screen_height, 0.0f, -16.0f);
	martix_ortho(&r_projection_ui, 0.0f, screen_width, screen_height, 0.0f, 0.0f, -16.0f);

	glClearColor(0.1f, 0.04f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(sdl_win);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(FAR_PLANE);
	glEnable(GL_DEPTH_CLAMP);

	glDisable(GL_STENCIL_TEST);
	glStencilMask(GL_FALSE);

	glAlphaFunc(GL_GREATER, 0.0f);

	glDepthFunc(GL_LEQUAL);

	glCullFace(GL_BACK);
//	glEnable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//
	// loading stuff

	input_init();

	if(shader_init())
		return 1;

	if(render_init())
		return 1;

	if(glui_init())
		return 1;

	if(x16_init())
		return 1;

	if(editor_init())
		return 1;

	system_clear_events();

	glEnable(GL_DEPTH_TEST);

	return 0;
}

void system_deinit()
{
	SDL_Quit();
}

void system_finish_draw()
{
	glFlush();
	SDL_GL_SwapWindow(sdl_win);
}

int system_loop()
{
	first_tick = SDL_GetTicks();
	while(!stopped)
	{
		// process input
		sys_input();
		// process time base
		sys_time();
		// draw
		system_draw();
		// render
		system_finish_draw();
	}

	return stopped & 0x7FFFFFFF;
}

void system_mouse_grab(int grab)
{
	if(grab)
	{
		SDL_SetWindowGrab(sdl_win, 1);
		SDL_SetRelativeMouseMode(1);
	} else
	{
		SDL_SetWindowGrab(sdl_win, 0);
		SDL_SetRelativeMouseMode(0);
	}
}

void system_clear_events()
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT)
			stopped = 0x80000000;
	}
}

void system_stop(uint32_t code)
{
	stopped = 0x80000000 | code;
}

