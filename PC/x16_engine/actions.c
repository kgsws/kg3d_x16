#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "defs.h"
#include "things.h"
#include "hitscan.h"
#include "actions.h"

static const uint8_t rng_mask[] = {0, 0, 1, 3, 7, 15, 31, 63};

//
// stuff

static uint8_t randomize(uint32_t mdx)
{
	uint8_t rng = rng_get();
	uint8_t val = (rng & rng_mask[mdx]) + 1;
	if(rng & 0x80)
		return -val;
	return val;
}

static void aim_rng(thing_t *th, uint8_t *res, uint32_t arg)
{
	res[0] = th->angle;
	if(arg & 0x07)
		res[0] += randomize(arg & 7);

	res[1] = th->pitch >> 1;
	if(arg & 0x70)
		res[1] += randomize((arg & 0x70) >> 4);
}

//
// run actions

uint32_t action_func(uint8_t tdx, uint32_t act, thing_state_t *st)
{
	thing_t *th = things + tdx;

	switch(act)
	{
		case 1:	// weapon: ready
		{
			uint8_t cmd;

			th->iflags = 0;

			cmd = ticcmd.bits_h & 0x1F;
			if(cmd)
			{
				cmd--;
				if(cmd < 10)
				{
					uint32_t state;

					ticcmd.bits_h &= 0xE0;

					cmd += THING_WEAPON_FIRST;
					if(cmd != th->type)
					{
						state = thing_anim[cmd][ANIM_RAISE].state; // TODO: inventory check
						if(state)
						{
							th->next_state = thing_anim[th->type][ANIM_LOWER].state;
							th->counter = cmd;
							return 1;
						}
					}
				}
			}

			if(ticcmd.bits_l & TCMD_ATK && thing_anim[th->type][ANIM_ATK].state)
			{
				th->next_state = thing_anim[th->type][ANIM_ATK].state;
				th->iflags = 1;
				return 1;
			}
			if(ticcmd.bits_l & TCMD_ALT && thing_anim[th->type][ANIM_ALT].state)
			{
				th->next_state = thing_anim[th->type][ANIM_ALT].state;
				th->iflags = 2;
				return 1;
			}
		}
		break;
		case 2: // weapon: raise
			th->height -= st->arg[0];
			if(th->height & 0x80)
				th->height = 0;
			if(!th->height)
			{
				th->next_state = thing_anim[th->type][ANIM_READY].state;
				return 1;
			}
		break;
		case 3: // weapon: lower
			th->height += st->arg[0];
			if(th->height >= 64)
			{
				th->height = 64;
				th->type = th->counter;
				th->next_state = thing_anim[th->type][ANIM_RAISE].state;
				return 1;
			}
		break;
		case 4: // attack: projectile
		{
			uint32_t type = thing_type[th->type].spawn[st->arg[0]];
			uint8_t aim[2];
			thing_t *ph;
			uint32_t tdx, pdx;
			uint8_t diff;

			if(th == things)
				th += player_thing;

			tdx = th - things;

			diff = thing_type[th->type].atk_height - thing_type[type].height / 2;

			pdx = thing_spawn(th->x, th->y, th->z + ((uint32_t)diff << 8), thingsec[tdx][0], type, tdx);
			if(pdx)
			{
				ph = things + pdx;

				aim_rng(th, aim, st->arg[1]);

				ph->angle = aim[0];
				ph->pitch = aim[1] << 1;

				thing_launch(pdx, thing_type[ph->type].speed);
			}
		}
		break;
		case 5: // attack: hitscan
		{
			uint32_t tdx;
			uint32_t type = thing_type[th->type].spawn[st->arg[0]];
			uint8_t aim[2];

			if(th == things)
				th += player_thing;

			tdx = th - things;

			aim_rng(th, aim, st->arg[1]);

			hitscan_attack(tdx, thing_type[th->type].atk_height, aim[0], aim[1], type);
		}
		break;
	}

	return 0;
}

