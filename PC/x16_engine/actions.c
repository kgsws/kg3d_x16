#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "defs.h"
#include "tick.h"
#include "things.h"
#include "hitscan.h"
#include "actions.h"

//
// stuff

static uint8_t spread(uint8_t val, uint8_t rng)
{
	val -= rng;
	val += rng_val(rng * 2);
	return val;
}

static void aim_rng(thing_t *th, uint8_t *res, uint32_t arg)
{
	uint8_t tmp;

	res[0] = th->angle;
	if(arg & 15)
		res[0] = spread(res[0], arg & 15);

	res[1] = th->pitch >> 1;
	if(arg >> 4)
		res[1] = spread(res[1], arg >> 4);
}

void check_weapon_th(thing_t **ptr)
{
	if(*ptr == (thing_t*)ticker)
		*ptr = (thing_t*)&ticker[player_thing];
}

//
// HACK

static void slide_door_hack(sector_t *sec, map_secobj_t *so, int16_t pos)
{
	wall_t *wall = map_walls[so->bank] + so->first;

	wall = map_walls[so->bank] + wall->next;
	wall->vtx.y = so->y + pos;
	wall = map_walls[so->bank] + wall->next;
	wall->vtx.y = so->y + pos;
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
							th->ticker.type = cmd;
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
			if(th->height >= 52)
			{
				th->height = 52;
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

				ph->angle = aim[0];
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
				hitscan_attack(tdx, thing_type[th->ticker.type].atk_height, aim[0], aim[1], type, 0);
			}
		}
		break;
		case 6: // attack: melee
		{
			uint32_t tdx;
			uint32_t type = thing_type[th->ticker.type].spawn[st->arg[0]];
			uint8_t aim[2];

			check_weapon_th(&th);
			tdx = ticker_idx(th);

			aim_rng(th, aim, st->arg[1]);
			hitscan_attack(tdx, thing_type[th->ticker.type].atk_height, aim[0], aim[1], type, st->arg[2]);
		}
		break;
		case 7: // effect: blood splat
		{
			uint8_t tmp = rng_get();

			th->mz = 0;

			if(st->arg[0])
			{
				th->angle += 0x40;
				th->angle ^= tmp & 0x80;
				thing_launch_ang(tdx, th->angle, st->arg[0]);

				th->mz = st->arg[0] << 8;
			}

			if(st->arg[2] > (tmp & 0x7F))
				th->mz = st->arg[1] << 8;
		}
		break;
		case 8: // ticks: add
			th->ticks += rng_val(st->arg[0]);
		break;
		case 9: // death: simple
		case 10: // death: radius
		{
			th->gravity = st->arg[0];
			th->blocking = st->arg[1];
			th->blockedby = st->arg[2];

			if(act != 9)
				break;

			th->eflags |= THING_EFLAG_NORADIUS;
			th->iflags &= ~THING_IFLAG_HEIGHTCHECK;

			th->radius = thing_type[th->ticker.type].alt_radius;

			if(thing_check_pos(tdx, th->x / 256, th->y / 256, th->z / 256, 0))
				thing_apply_pos();
		}
		break;
	}

	return 0;
}

