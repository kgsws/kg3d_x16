#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "tick.h"
#include "things.h"

ticker_dummy_t ticker[NUM_TICKERS];
uint8_t tick_idx;

static uint8_t top;
static uint8_t cur;

//
// API

void tick_clear()
{
	for(uint32_t i = 1; i < NUM_TICKERS; i++)
		ticker[i].base.func = TFUNC_FREE_SLOT;

	top = 0;
	cur = 0;

	thing_clear();
}

void tick_run()
{
	// camera stuff

	if(camera_damage <= 3)
		camera_damage = 0;
	else
	if(camera_damage)
		camera_damage -= 3;


	// run tickers

	tick_idx = top;
	while(tick_idx)
	{
		uint32_t next = ticker[tick_idx].base.next;
		switch(ticker[tick_idx].base.func)
		{
			case TICKER_FUNC_ANIM:
				thing_tick_anim();
			break;
			case TICKER_FUNC_MOVE:
				thing_tick_move();
			break;
			case TICKER_FUNC_THING:
				thing_tick_full();
			break;
			case TICKER_FUNC_PLAYER:
				thing_tick_plyr();
			break;
		}
		tick_idx = next;
	}
}

uint32_t tick_add()
{
	uint32_t i;

	// find free slot; ZERO is local player weapon
	for(i = 1; i < NUM_TICKERS; i++)
	{
		if(ticker[i].base.func & 0x80)
			break;
	}
	if(i >= NUM_TICKERS)
		return 0;

	ticker[i].base.func = TICKER_FUNC_DUMMY;
	ticker[i].base.prev = cur;
	ticker[i].base.next = 0;
	ticker[cur].base.next = i; // writing to ZERO is ignored
	cur = i;

	if(!top)
		top = i;

	return i;
}

void tick_del(uint32_t idx)
{
	uint8_t next = ticker[idx].base.next;
	uint8_t prev = ticker[idx].base.prev;

	ticker[idx].base.func = TFUNC_FREE_SLOT;

	// writing to ZERO is ignored
	ticker[next].base.prev = prev;
	ticker[prev].base.next = next;

	if(idx == top)
		top = next;

	if(idx == cur)
		cur = prev;
}
