#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "defs.h"
#include "tick.h"
#include "things.h"
#include "hud.h"

#define SPRITE_HUD	104
#define SPRITE_TXT	0

//

hud_cfg_t hud_cfg;

static uint32_t idx_hud;
static uint32_t idx_txt;

static uint32_t color_hud;
static uint32_t color_txt;

//
// sprites

static void spr_hud(int32_t x, int32_t y, uint32_t vis)
{
	x16_sprite_t *spr = (x16_sprite_t*)&vram[0x1FC00];

	idx_hud--;
	spr += idx_hud;

	if(vis < 8)
		vis += 0x0F8;
	else
		vis += 0x170;

	spr->addr = vis << 1;
	spr->x = x;
	spr->y = y;
	spr->ia = 0b00001100;
	spr->ib = 0b01000000 | color_hud;
}

//
// write

static void write_hud(int32_t x, int32_t y, uint8_t *text)
{
	while(1)
	{
		uint8_t in = *text++;

		if(!in)
			break;

		in -= 0x30;

		spr_hud(x, y, in);
		x += font_info[128 + in];
	}
}

//
// API

void hud_draw()
{
	thing_t *th = thing_ptr(player_thing);
	uint8_t text[16];

	idx_hud = SPRITE_HUD;
	idx_txt = SPRITE_TXT;

	color_hud = hud_cfg.stat_color.hp;
	sprintf(text, "%03u", th->health);
	write_hud(hud_cfg.stat_cfg.bar_pos[0], hud_cfg.stat_cfg.bar_pos[1], text);
}
