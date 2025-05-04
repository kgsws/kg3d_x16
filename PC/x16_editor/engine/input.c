#include "inc.h"
#include "system.h"
#include "input.h"
#include <SDL2/SDL.h>

//
//

input_t input_list[INPUT__COUNT] =
{
//
// GAME
//
	[INPUT_FORWARD] =
	{
		.key = 'w',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_BACKWARD] =
	{
		.key = 's',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_S_LEFT] =
	{
		.key = 'a',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_S_RIGHT] =
	{
		.key = 'd',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_JUMP] =
	{
		.key = ' ',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_CROUCH] =
	{
		.key = SDLK_RCTRL,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
//
// EDITOR: common
//
	[INPUT_EDITOR_MENU] =
	{
		.key = SDLK_ESCAPE,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_MENU_CLICK] =
	{
		.key = SDL_BUTTON_LEFT,
		.type = INTYPE_MOUSE_BUTTON,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_MENU_LEFT] =
	{
		.key = SDLK_LEFT,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_MENU_RIGHT] =
	{
		.key = SDLK_RIGHT,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_EDITMODE] =
	{
		.key = SDLK_KP_ENTER,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_INCREASE] =
	{
		.key = MOUSE_WHEEL_Y_UP,
		.type = INTYPE_MOUSE_WHEEL,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_DECREASE] =
	{
		.key = MOUSE_WHEEL_Y_DOWN,
		.type = INTYPE_MOUSE_WHEEL,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_INSERT] =
	{
		.key = SDLK_INSERT,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_DELETE] =
	{
		.key = SDLK_DELETE,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_EDITOR_COPY] =
	{
		.key = 'c',
		.type = INTYPE_KEY,
		.extra = INEXTRA_CTRL,
		.ignore = INEXTRA_SHIFT
	},
	[INPUT_EDITOR_PASTE] =
	{
		.key = 'v',
		.type = INTYPE_KEY,
		.extra = INEXTRA_CTRL,
		.ignore = INEXTRA_SHIFT
	},
//
// EDITOR: 2D
//
	[INPUT_E2D_DRAG_MAP] =
	{
		.key = SDL_BUTTON_LEFT,
		.type = INTYPE_MOUSE_BUTTON
	},
	[INPUT_E2D_DRAG_SEL] =
	{
		.key = SDL_BUTTON_RIGHT,
		.type = INTYPE_MOUSE_BUTTON
	},
	[INPUT_E2D_DRAG_SOLO] =
	{
		.key = SDL_BUTTON_RIGHT,
		.type = INTYPE_MOUSE_BUTTON,
		.extra = INEXTRA_CTRL
	},
	[INPUT_E2D_GRID_MORE] =
	{
		.key = SDLK_KP_PLUS,
		.type = INTYPE_KEY
	},
	[INPUT_E2D_GRID_LESS] =
	{
		.key = SDLK_KP_MINUS,
		.type = INTYPE_KEY
	},
	[INPUT_E2D_ZOOM_IN] =
	{
		.key = MOUSE_WHEEL_Y_UP,
		.type = INTYPE_MOUSE_WHEEL
	},
	[INPUT_E2D_ZOOM_OUT] =
	{
		.key = MOUSE_WHEEL_Y_DOWN,
		.type = INTYPE_MOUSE_WHEEL
	},
	[INPUT_E2D_DRAW_POINT] =
	{
		.key = ' ',
		.type = INTYPE_KEY
	},
	[INPUT_E2D_DRAW_BACK] =
	{
		.key = SDLK_BACKSPACE,
		.type = INTYPE_KEY
	},
	[INPUT_E2D_MODE_LINES] =
	{
		.key = 'l',
		.type = INTYPE_KEY
	},
	[INPUT_E2D_MODE_SECTOR] =
	{
		.key = 's',
		.type = INTYPE_KEY
	},
	[INPUT_E2D_MODE_THINGS] =
	{
		.key = 't',
		.type = INTYPE_KEY
	},
	[INPUT_E2D_OPTIONS] =
	{
		.key = 'o',
		.type = INTYPE_KEY
	},
	[INPUT_E2D_GROUPS] =
	{
		.key = 'g',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E2D_SECTOR_CONNECT] =
	{
		.key = 'c',
		.type = INTYPE_KEY
	},
	[INPUT_E2D_SECTOR_DISCONNECT] =
	{
		.key = 'd',
		.type = INTYPE_KEY
	},
	[INPUT_E2D_BLOCKING] =
	{
		.key = 'b',
		.type = INTYPE_KEY
	},
	[INPUT_E2D_MASKED_LINE] =
	{
		.key = 'm',
		.type = INTYPE_KEY
	},
//
// EDITOR: 3D
//
	[INPUT_E3D_CLICK_LOCK] =
	{
		.key = SDL_BUTTON_LEFT,
		.type = INTYPE_MOUSE_BUTTON,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_CLICK_PICK] =
	{
		.key = SDL_BUTTON_RIGHT,
		.type = INTYPE_MOUSE_BUTTON,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_COPY] =
	{
		.key = 'c',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_PASTE] =
	{
		.key = 'v',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_HIGHLIGHT] =
	{
		.key = 'h',
		.type = INTYPE_KEY
	},
	[INPUT_E3D_PREVIEW] =
	{
		.key = 'p',
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_OFFS_RESET] =
	{
		.key = SDLK_KP_5,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_OFFS_X_INC] =
	{
		.key = SDLK_KP_4,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_OFFS_X_DEC] =
	{
		.key = SDLK_KP_6,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_OFFS_Y_INC] =
	{
		.key = SDLK_KP_8,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_OFFS_Y_DEC] =
	{
		.key = SDLK_KP_2,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_PLANE_A_INC] =
	{
		.key = SDLK_KP_1,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_PLANE_A_DEC] =
	{
		.key = SDLK_KP_3,
		.type = INTYPE_KEY,
		.extra = INEXTRA_IGNORE
	},
	[INPUT_E3D_WALL_PEG_X] =
	{
		.key = SDLK_KP_7,
		.type = INTYPE_KEY,
		.extra = INEXTRA_SHIFT
	},
	[INPUT_E3D_WALL_PEG_Y] =
	{
		.key = SDLK_KP_7,
		.type = INTYPE_KEY,
	},
	[INPUT_E3D_WALL_MIRROR_X] =
	{
		.key = SDLK_KP_1,
		.type = INTYPE_KEY
	},
	[INPUT_E3D_WALL_MIRROR_Y] =
	{
		.key = SDLK_KP_1,
		.type = INTYPE_KEY,
		.extra = INEXTRA_SHIFT
	},
	[INPUT_E3D_WALL_SPLIT] =
	{
		.key = SDLK_KP_3,
		.type = INTYPE_KEY
	},
	[INPUT_E3D_SECTOR_WATER] =
	{
		.key = SDLK_KP_7,
		.type = INTYPE_KEY
	},
};

uint8_t input_state[INPUT__COUNT];

int32_t input_analog_h;
int32_t input_analog_v;

int32_t mouse_wheel_x;
int32_t mouse_wheel_y;

int32_t mouse_x;
int32_t mouse_y, mouse_yy;
uint32_t mouse_btn;

uint32_t input_shift;
uint32_t input_ctrl;
uint32_t input_alt;

//
//

void input_init()
{
	// TODO: load config
}

void input_process(uint32_t key, uint16_t type, uint16_t state)
{
	for(uint32_t i = 0; i < INPUT__COUNT; i++)
	{
		input_t *in = input_list + i;

		if(in->type == type && in->key == key)
		{
			uint32_t st = state;
			if(st && !(in->extra & INEXTRA_IGNORE))
			{
				uint32_t extra;
				extra = !!input_shift;
				extra |= !!input_ctrl << 1;
				extra |= !!input_alt << 2;
				extra &= ~in->ignore;
				st = in->extra == extra;
			}
			input_state[i] = st;
		}
	}
}

void input_release()
{
	// release 'keys' that have no keyup event
	for(uint32_t i = 0; i < INPUT__COUNT; i++)
	{
		if(input_list[i].type == INTYPE_MOUSE_WHEEL)
			input_state[i] = 0;
	}
}

void input_clear()
{
	// release all the 'keys'
	for(uint32_t i = 0; i < INPUT__COUNT; i++)
		input_state[i] = 0;
}

