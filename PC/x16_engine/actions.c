#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include "defs.h"
#include "tick.h"
#include "things.h"
#include "hitscan.h"
#include "actions.h"

static int32_t dist_calc;

static const uint8_t prop_offs[] =
{
	offsetof(thing_t, height),
	offsetof(thing_t, radius),
	offsetof(thing_t, view_height),
	offsetof(thing_t, step_height),
	offsetof(thing_t, water_height),
	offsetof(thing_t, gravity),
	offsetof(thing_t, angle),
	offsetof(thing_t, pitch),
	offsetof(thing_t, scale),
	offsetof(thing_t, blocking),
	offsetof(thing_t, blockedby),
};

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

static void check_weapon_th(thing_t **ptr)
{
	if(*ptr == (thing_t*)ticker)
		*ptr = (thing_t*)&ticker[player_thing];
}

static void skip_state(thing_t *th)
{
	thing_state_t *st = thing_state + (th->next_state & (MAX_X16_STATES-1));
	th->next_state = st->next;
	th->next_state |= (st->frm_nxt & 0xE0) << 3;
	th->next_state |= (st->action & 0x80) << 8;
}

static uint32_t vis_check(uint32_t s0, uint32_t s1)
{
	uint8_t *tab = map_vis_tab + ((s1 & 0x1F) * 256);
	return tab[s0] & (1 << (s1 >> 5));
}

static uint32_t ang_check(uint32_t t0, uint32_t t1)
{
	thing_t *th = thing_ptr(t0);
	thing_t *ht = thing_ptr(t1);
	p2a_coord.x = ht->x / 256 - th->x / 256;
	p2a_coord.y = ht->y / 256 - th->y / 256;
	return point_to_angle() >> 4;
}

static void get_dist(uint32_t t0, uint32_t t1)
{
	if(dist_calc >= 0)
		return;

	thing_t *th = thing_ptr(t0);
	thing_t *ht = thing_ptr(t1);

	p2a_coord.x = ht->x / 256 - th->x / 256;
	p2a_coord.y = ht->y / 256 - th->y / 256;

	dist_calc = point_to_dist();
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

				if(th->counter)
					return 0;

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
		case 9: // death: base
		case 10: // death: base (alt)
		{
			th->gravity = st->arg[0];
			th->eflags = st->arg[1];
			th->view_height = st->arg[2];

			if(act == 9)
				break;

			th->eflags |= THING_EFLAG_NORADIUS;
			th->iflags &= ~THING_IFLAG_HEIGHTCHECK;

			th->radius = thing_type[th->ticker.type].alt_radius;

			if(thing_check_pos(tdx, th->x / 256, th->y / 256, th->z / 256, 0))
				thing_apply_pos();
		}
		break;
		case 11: // death: heights
			if(st->arg[0])
				th->height = st->arg[0];
			if(st->arg[1])
				th->step_height = st->arg[1];
			if(st->arg[2])
			{
				th->water_height = st->arg[2];
				th->eflags |= THING_EFLAG_WATERSPEC;
			}
		break;
		case 12: // death: blocking
			th->blocking = st->arg[0];
			th->blockedby = st->arg[1];
		break;
		case 13: // death: type
		{
			uint32_t type = thing_type[th->ticker.type].spawn[st->arg[0]];
			thing_type_t *info = thing_type + type;

			th->ticker.type = type;
			th->blocking = info->blocking;
			th->blockedby = info->blockedby;
			th->health = info->health;
			th->height = info->height;
			th->water_height = info->water_height;
			th->step_height = info->step_height;
			th->eflags = info->eflags;
			th->gravity = info->gravity;
		}
		break;
		case 14: // explosion: origin
			thing_explode(tdx, st->arg[0], st->arg[1], st->arg[2], th->origin);
		break;
		case 15: // explosion: target
			thing_explode(tdx, st->arg[0], st->arg[1], st->arg[2], th->target);
		break;
		case 16: // explosion: damager
			thing_explode(tdx, st->arg[0], st->arg[1], st->arg[2], th->damager);
		break;
		case 17: // explosion: self
			thing_explode(tdx, st->arg[0], st->arg[1], st->arg[2], tdx);
		break;
		case 18: // set: property
			*((uint8_t*)th + prop_offs[st->arg[0]]) = st->arg[1];
		break;
		case 19: // set: flags
			th->eflags &= st->arg[0];
			th->eflags |= st->arg[1];
		break;
		case 20: // aim: angle
			if(th->target)
			{
				th->angle = ang_check(tdx, th->target);
				th->pitch = 0x80;
			}
			if(st->arg[0])
			{
				skip_state(th);
				return 1;
			}
		break;
		case 21: // aim: attack
			if(th->target)
				th->angle = ang_check(tdx, th->target); // TODO: sight check full aim
			if(st->arg[0])
			{
				skip_state(th);
				return 1;
			}
		break;
		case 22: // enemy: look
		{
			thing_t *ht;
			uint8_t ang;

			if(!vis_check(thingsec[tdx][0], thingsec[player_thing][0]))
				break;

			ht = thing_ptr(th->target);
			if(ht->iflags & THING_IFLAG_CORPSE)
				break;

			ang = ang_check(tdx, player_thing);
			if(	(ang + 0x40 - th->angle) & 0x80 &&
				!st->arg[0]
			)
				break;

			th->target = player_thing;
			th->chaseang = ang & 0xF0;
			th->counter = 15;

			th->next_state = thing_anim[th->ticker.type][ANIM_MOVE].state;
			return 1;
		}
		break;
		case 23: // enemy: chase
		{
			thing_t *ht;
			uint8_t ang;
			int32_t nx, ny;
			thing_type_t *info;

			if(!th->target)
			{
stop:
				th->next_state = thing_anim[th->ticker.type][ANIM_SPAWN].state;
				return 1;
			}

			ht = thing_ptr(th->target);
			if(ht->iflags & THING_IFLAG_CORPSE)
				goto stop;

			if(!vis_check(thingsec[tdx][0], thingsec[th->target][0]))
				break;

			dist_calc = -1;

			if(	st->arg[2] &&
				(rng_get() & 0x7F) < st->arg[2]
			){
				get_dist(tdx, th->target);
				th->dist = dist_calc;
				th->counter = 10;
				th->next_state = thing_anim[th->ticker.type][ANIM_FIRE].state;
				return 1;
			}

			if(st->arg[1])
			{
				get_dist(tdx, th->target);
				if(st->arg[1] * 4 - dist_calc >= 0)
				{
					th->dist = dist_calc;
					th->counter = 10;
					th->next_state = thing_anim[th->ticker.type][ANIM_MELEE].state;
					return 1;
				}
			}

			th->angle = th->chaseang;

			th->counter--;
			if(!th->counter)
			{
				get_dist(tdx, th->target);
				if(dist_calc - st->arg[0] * 4 < 0)
					p2a_coord.a ^= 0x80;

				th->counter = 10 + (rng_get() & 7);
				th->chaseang = p2a_coord.a;
				th->chaseang -= 0x0F;
				th->chaseang += rng_get() & 0x1F;
				th->angle = th->chaseang;
			}

			info = thing_type + th->ticker.type;
			nx = th->x + tab_sin[th->chaseang] * info->speed;
			ny = th->y + tab_cos[th->chaseang] * info->speed;
			if(thing_check_pos(tdx, nx / 256, ny / 256, th->z / 256, 0))
			{
				th->x = nx;
				th->y = ny;
				thing_apply_pos();
				break;
			}

			if(rng_get() < 80)
			{
				th->chaseang = poscheck.hitang + 0x40;
				th->angle = th->chaseang;
				break;
			}

			get_dist(tdx, th->target);
			if(dist_calc - st->arg[0] * 4 < 0)
				p2a_coord.a ^= 0x80;

			ang = p2a_coord.a;
			ang = (poscheck.hitang + 0x40) - ang;
			ang &= 0x80;
			th->chaseang = poscheck.hitang ^ ang;
			th->angle = th->chaseang;
		}
		break;
	}

	return 0;
}

