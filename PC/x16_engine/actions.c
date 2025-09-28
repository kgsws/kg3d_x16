#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "defs.h"
#include "tick.h"
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

void check_weapon_th(thing_t **ptr)
{
	if(*ptr == (thing_t*)ticker)
		*ptr = (thing_t*)&ticker[player_thing];
}

//
// run actions

uint32_t action_func(uint8_t tdx, uint32_t act, thing_state_t *st)
{
	thing_t *th = thing_ptr(tdx);

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
					if(cmd != th->ticker.type)
					{
						state = thing_anim[cmd][ANIM_RAISE].state; // TODO: inventory check
						if(state)
						{
							th->next_state = thing_anim[th->ticker.type][ANIM_LOWER].state;
							th->counter = cmd;
							return 1;
						}
					}
				}
			}

			if(ticcmd.bits_l & TCMD_ATK && thing_anim[th->ticker.type][ANIM_ATK].state)
			{
				th->next_state = thing_anim[th->ticker.type][ANIM_ATK].state;
				th->iflags = 1;
				return 1;
			}
			if(ticcmd.bits_l & TCMD_ALT && thing_anim[th->ticker.type][ANIM_ALT].state)
			{
				th->next_state = thing_anim[th->ticker.type][ANIM_ALT].state;
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
				th->next_state = thing_anim[th->ticker.type][ANIM_READY].state;
				return 1;
			}
		break;
		case 3: // weapon: lower
			th->height += st->arg[0];
			if(th->height >= 64)
			{
				th->height = 64;
				th->ticker.type = th->counter;
				th->next_state = thing_anim[th->ticker.type][ANIM_RAISE].state;
				return 1;
			}
		break;
		case 4: // attack: projectile
		{
			uint32_t type = thing_type[th->ticker.type].spawn[st->arg[0]];
			uint8_t aim[2];
			thing_t *ph;
			uint32_t tdx, pdx;
			uint8_t diff;

			check_weapon_th(&th);
			tdx = ticker_idx(th);

			diff = thing_type[th->ticker.type].atk_height - thing_type[type].height / 2;

			pdx = thing_spawn(th->x, th->y, th->z + ((uint32_t)diff << 8), thingsec[tdx][0], type, tdx);
			if(pdx)
			{
				ph = thing_ptr(pdx);

				aim_rng(th, aim, st->arg[1]);

				ph->angle = aim[0] + st->arg[2];
				ph->pitch = aim[1] << 1;

				thing_launch(pdx, thing_type[ph->ticker.type].speed);
			}
		}
		break;
		case 5: // attack: hitscan
		{
			uint32_t tdx;
			uint32_t type = thing_type[th->ticker.type].spawn[st->arg[0]];
			uint8_t aim[2];

			check_weapon_th(&th);
			tdx = ticker_idx(th);

			for(uint32_t i = 0; i < st->arg[2]; i++)
			{
				aim_rng(th, aim, st->arg[1]);
				hitscan_attack(tdx, thing_type[th->ticker.type].atk_height, aim[0], aim[1], type);
			}
		}
		break;
		case 6: // effect: blood splat
		{
			uint8_t tmp = rng_get();

			th->mz = 0;

			if(st->arg[0])
			{
				th->angle += 0x40;
				th->angle ^= tmp & 0x80;
				thing_launch(tdx, st->arg[0]);

				th->mz = st->arg[0] << 8;
			}

			if(st->arg[2] > (tmp & 0x7F))
				th->mz = st->arg[1] << 8;
		}
		break;
		case 7: // ticks: add
			th->ticks += rng_get() & st->arg[0];
		break;
		case 8: // death: simple
		{
			th->gravity = st->arg[0];
			th->blocking = st->arg[1];
			th->blockedby = st->arg[2];
		}
		break;
	}

	return 0;
}

