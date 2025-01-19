#include "inc.h"
#include "defs.h"
#include "engine.h"
#include "input.h"
#include "list.h"
#include "tick.h"

uint32_t leveltime;

kge_ticcmd_t tick_local_cmd;

linked_list_t tick_list_normal;

//
// functions

static void tick_list(linked_list_t *list)
{
	// Enties are intentionaly processed in backward order.
	// This way, newly added tickables are not processed instatnly but in the next tick.
	link_entry_t *ent = list->cur;

	while(ent)
	{
		uint32_t ret;
		link_entry_t *old = ent;
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);
		ret = tickable->info.tick(tickable->data);
		ent = ent->prev;
		if(ret)
			list_del_entry(list, old);
	}
}

//
// API

void tick_make_cmd(kge_ticcmd_t *cmd, kge_thing_t *th)
{
	uint32_t angle;
	int32_t pitch;
	int32_t fmove = 0;
	int32_t smove = 0;

	memset(cmd, 0, sizeof(kge_ticcmd_t));

	angle = th->pos.angle;
	pitch = th->pos.pitch;

	// horizontal angle
	if(input_analog_h)
	{
		angle -= input_analog_h * 0x30;
		input_analog_h = 0;
	}

	// vertical angle
	if(input_analog_v)
	{
		int32_t vvv = (int32_t)pitch + input_analog_v * 0x60;

		if(vvv > 0x7FFF)
			pitch = 0x7FFF;
		else
		if(vvv < -0x7FFF)
			pitch = -0x7FFF;
		else
			pitch = vvv;

		input_analog_v = 0;
	}

	if(input_state[INPUT_FORWARD])
		fmove += 0x7FFF;

	if(input_state[INPUT_BACKWARD])
		fmove -= 0x7FFF;

	if(input_state[INPUT_S_LEFT])
		smove -= 0x7FFF;

	if(input_state[INPUT_S_RIGHT])
		smove += 0x7FFF;

	if(fmove > 0x7FFF)
		fmove = 0x7FFF;
	if(fmove < -0x7FFF)
		fmove = -0x7FFF;

	if(smove > 0x7FFF)
		smove = 0x7FFF;
	if(smove < -0x7FFF)
		smove = -0x7FFF;

	cmd->fmove = fmove;
	cmd->smove = smove;
	cmd->angle = angle;
	cmd->pitch = pitch;
}

kge_ticker_t *tick_create(uint32_t size, struct linked_list_s *list)
{
	void *ret;
	size += sizeof(kge_tickhead_t);
	ret = list_add_entry(list, size);
	memset(ret, 0, size);
	return ret;
}

void tick_cleanup()
{
	list_clear(&tick_list_normal);
}

void tick_go()
{
	tick_list(&tick_list_normal);
	leveltime++;
}

