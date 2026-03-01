#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "defs.h"
#include "tick.h"
#include "things.h"
#include "hud.h"

#define SPRITE_IDX_TOP	104
#define SPRITE_IDX_HUD	(SPRITE_IDX_TOP - 12)
#define SPRITE_IDX_TXT	0

//

hud_export_t hud_info;
uint32_t hud_update;

static uint32_t idx_hud;
static uint32_t idx_txt;

static uint32_t color_hud;
static uint32_t color_txt;

//
// sprites

static void spr_hud(int32_t x, int32_t y, uint32_t vis)
{
	x16_sprite_t *spr = (x16_sprite_t*)&vram[0x1FC00];

	spr += idx_hud++;

	if(vis < 8)
		vis += 0x0F8;
	else
		vis += 0x170;

	spr->addr = vis << 1;
	spr->x = x;
	spr->y = y;
	spr->ia = 0b00001100;
	spr->ib = color_hud;
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
		x += hud_info.style.space;
	}
}

//
// conversion

static uint8_t *convert(uint32_t val, uint32_t skip)
{
	static uint8_t text[8];
	uint8_t *ptr = text;

	sprintf(text, "%04u", val);

	while(1)
	{
		uint8_t in = *ptr;

		if(!in)
			break;

		if(in != '0')
			break;

		*ptr = ':';

		ptr++;
	}

	if(!val)
		text[3] = '0';

	return text + !skip;
}

//
// API

void hud_draw()
{
	thing_t *th;

	if(!hud_update)
		return;

	hud_update = 0;

	if(player_thing != camera_thing)
	{
do_clear:
		for(uint32_t i = SPRITE_IDX_HUD; i < SPRITE_IDX_TOP; i++)
		{
			x16_sprite_t *spr = (x16_sprite_t*)&vram[0x1FC00];
			spr += i;
			spr->ia = 0;
		}
		return;
	}

	th = thing_ptr(player_thing);

	if(!th->health)
		goto do_clear;

	idx_hud = SPRITE_IDX_HUD;
	idx_txt = SPRITE_IDX_TXT;

	// health

	color_hud = hud_info.stat_color.hp;

	write_hud(hud_info.style.hp_x[0], hud_info.style.bar_y, convert(th->health, hud_info.style.digits & 0x80));

	if(hud_info.style.hp_x[2])
	{
		spr_hud(hud_info.style.hp_x[1], hud_info.style.bar_y, 11);
		spr_hud(hud_info.style.hp_x[2], hud_info.style.bar_y, 11);
	}

	// ammo

	color_hud = hud_info.stat_color.ammo;

	write_hud(hud_info.style.am_x[0], hud_info.style.bar_y, convert(0, hud_info.style.digits & 0x40));

	if(hud_info.style.am_x[2])
	{
		spr_hud(hud_info.style.am_x[1], hud_info.style.bar_y, 11);
		spr_hud(hud_info.style.am_x[2], hud_info.style.bar_y, 11);
	}
}
