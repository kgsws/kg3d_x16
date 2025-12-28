#include "inc.h"
#include "defs.h"
#include "list.h"
#include "system.h"
#include "input.h"
#include "matrix.h"
#include "shader.h"
#include "engine.h"
#include "render.h"
#include "editor.h"
#include "things.h"
#include "tick.h"
#include "image.h"
#include "x16g.h"
#include "x16r.h"
#include "x16e.h"
#include "x16c.h"
#include "x16t.h"
#include "kgcbor.h"
#include "glui.h"
#include "ui_def.h"

#define NUM_SHOW_ATTRS	(sizeof(thing_attr) / sizeof(thing_edit_attr_t))
#define NUM_SHOW_FLAGS	(sizeof(thing_flag) / sizeof(thing_edit_flag_t))
#define NUM_SHOW_ANIMS	NUM_THING_ANIMS
#define NUM_STATE_COLS	9
#define NUM_STATE_ROWS	9

#define UI_ATTR_HEIGHT	17
#define UI_TITLE_HEIGHT	18
#define UI_SPACE_HEIGHT	10
#define UI_STATE_HEIGHT	18

#define MAX_ACTION_NAME	32

#define THING_CRC_XOR	0xF0E19B64

#define THING_ATTR(str,elm)	str, sizeof(str)-1, offsetof(export_type_t, elm)
#define FLAG_STR(str)	str, sizeof(str)-1

enum
{
	ATTR_TYPE_U8,
	ATTR_TYPE_U16,
	ATTR_TYPE_CHANCE,
	ATTR_TYPE_TT,
	ATTR_TYPE_BLOCK_FLAGS,
	ATTR_TYPE_SCALE,
	ATTR_TYPE_IMASS,
	ATTR_TYPE_U7F,
	ATTR_TYPE_VIEW_HEIGHT,
	ATTR_TYPE_ATK_HEIGHT,
	ATTR_TYPE_WATER_HEIGHT,
	ATTR_TYPE_ARADIUS,
};

enum
{
	AFLG_THING = 1,
	AFLG_WEAPON = 2,
};

enum
{
	ARGT_NONE,
	ARGT_U8,
	ARGT_S8,
	ARGT_CHANCE,
	ARGT_SPAWN_SLOT,
	ARGT_XY_SPREAD,
	ARGT_BLOCK_FLAGS,
};

enum
{
	CBOR_ROOT_THINGS,
	//
	NUM_CBOR_ROOT
};

enum
{
	CBOR_THING_NAME,
	//
	CBOR_THING_ATTRIBUTES,
	CBOR_THING_ANIMATIONS,
	CBOR_THING_FLAGS,
	//
	NUM_CBOR_THING
};

enum
{
	CBOR_STATE_SPRITE,
	CBOR_STATE_FRAME,
	CBOR_STATE_TICKS,
	CBOR_STATE_NEXT,
	CBOR_STATE_ACTION,
	CBOR_STATE_ARGS,
	//
	NUM_CBOR_STATE
};

typedef struct
{
	const uint8_t *text;
	void *func;
} arg_parse_t;

typedef union
{
	uint8_t raw[8];
	struct
	{
		uint8_t action;
		uint8_t next;
		uint8_t frm_nxt;
		uint8_t sprite;
		uint8_t ticks;
		uint8_t arg[3];
	};
	// state 0 contains extra game config
	struct
	{
		uint8_t num_sprlnk; // number of thing sprite names
		uint8_t menu_logo; // sprite for main menu logo
		uint8_t _frm_nxt;
		uint8_t _sprite;
		uint8_t _ticks;
		uint8_t plr_crouch; // player type change
		uint8_t plr_swim; // player type change
		uint8_t plr_fly; // player type change
	};
} export_state_t;

typedef struct
{
	uint8_t *const name;
	uint32_t nlen;
	uint16_t offs;
	uint16_t type;
} thing_edit_attr_t;

typedef struct
{
	uint8_t *const name;
	uint32_t nlen;
	uint32_t flag;
} thing_edit_flag_t;

typedef struct
{
	uint8_t *const name;
	uint8_t *const bold;
	uint32_t nlen;
} thing_edit_anim_t;

//

uint32_t x16_thing_texture[MAX_X16_THING_TYPES];
thing_sprite_t x16_thing_sprite[MAX_X16_THING_TYPES];

static uint8_t thing_path[EDIT_MAX_FILE_PATH];

thing_def_t thing_info[MAX_X16_THING_TYPES + 1];

static uint32_t hide_origin;

static uint32_t show_thing;
static uint32_t show_anim;
static uint32_t show_state;

static uint32_t show_angle;

static int32_t top_state;
static uint32_t set_attr;

static uint8_t *num_ptru8;
static uint32_t num_limit;
static uint8_t num_empty;

static glui_text_t *text_attribute;
static glui_text_t *text_flag;
static glui_text_t *text_animation;
static glui_text_t *text_states;

static uint8_t thing_data[8192 + MAX_X16_STATES * sizeof(export_state_t)];
static export_state_t state_data[MAX_X16_STATES];
static uint32_t state_idx;

static int32_t cbor_main_index;
static int32_t cbor_entry_index;
static int32_t cbor_sub_index;

static thing_def_t load_thing;

static uint8_t load_state_sprite[LEN_X16_TEXTURE_NAME];
static uint8_t load_action[MAX_ACTION_NAME];
static uint8_t load_args[3];
static thing_st_t load_state;

static uint8_t *spawn_dst;
static uint8_t *arg_dst;
static const int16_t *arg_lim;

// default player
static const export_type_t default_player_info =
{
	.radius = 31,
	.height = 144, // 0.5x for crouching / swimming
	.blocking = BLOCK_FLAG(BLOCKING_PLAYER) | BLOCK_FLAG(BLOCKING_ENEMY) | BLOCK_FLAG(BLOCKING_SOLID) | BLOCK_FLAG(BLOCKING_PROJECTILE) | BLOCK_FLAG(BLOCKING_HITSCAN),
	.blockedby = BLOCK_FLAG(BLOCKING_PLAYER) | BLOCK_FLAG(BLOCKING_SPECIAL),
	.imass = 64,
	.gravity = 128,
	.speed = 13, // 0.5x for crouching / swimming
	.eflags = THING_EFLAG_CANPUSH,
	.scale = 96,
	.step_height = 48, // 0.75x for crouching / swimming
	.view_height = 0,
	.water_height = 0, // 255 for crouching / swimming / flying
	.health = 100,
	.jump_pwr = 20, // 0.5x for crouching; 1 for swimming
	.spawn = {0xFF, 0xFF, 0xFF, 0xFF},
};

// default thing
static const export_type_t default_thing_info =
{
	.radius = 15,
	.height = 32,
	.gravity = 128,
	.scale = 96,
	.water_height = 255,
	.spawn = {0xFF, 0xFF, 0xFF, 0xFF},
};

// editable attributes
static const thing_edit_attr_t thing_attr[] =
{
	{THING_ATTR("height", height), ATTR_TYPE_U8},
	{THING_ATTR("radius", radius), ATTR_TYPE_U8},
	{THING_ATTR("alt radius", alt_radius), ATTR_TYPE_ARADIUS},
	{THING_ATTR("blocking", blocking), ATTR_TYPE_BLOCK_FLAGS},
	{THING_ATTR("blocked by", blockedby), ATTR_TYPE_BLOCK_FLAGS},
	{THING_ATTR("alt block", alt_block), ATTR_TYPE_BLOCK_FLAGS},
	{THING_ATTR("imass", imass), ATTR_TYPE_IMASS},
	{THING_ATTR("gravity", gravity), ATTR_TYPE_U8},
	{THING_ATTR("speed", speed), ATTR_TYPE_U8},
	{THING_ATTR("scale", scale), ATTR_TYPE_SCALE},
	{THING_ATTR("step height", step_height), ATTR_TYPE_U8},
	{THING_ATTR("view height", view_height), ATTR_TYPE_VIEW_HEIGHT},
	{THING_ATTR("attack height", atk_height), ATTR_TYPE_ATK_HEIGHT},
	{THING_ATTR("water height", water_height), ATTR_TYPE_WATER_HEIGHT},
	{THING_ATTR("health", health), ATTR_TYPE_U16},
	{THING_ATTR("pain chance", pain_chance), ATTR_TYPE_CHANCE},
	{THING_ATTR("jump power", jump_pwr), ATTR_TYPE_U8},
	{THING_ATTR("spawn A", spawn[0]), ATTR_TYPE_TT},
	{THING_ATTR("spawn B", spawn[1]), ATTR_TYPE_TT},
	{THING_ATTR("spawn C", spawn[2]), ATTR_TYPE_TT},
	{THING_ATTR("spawn D", spawn[3]), ATTR_TYPE_TT},
};

// editable flags
static const thing_edit_flag_t thing_flag[] =
{
	{FLAG_STR("projectile"), THING_EFLAG_PROJECTILE},
	{FLAG_STR("climbable"), THING_EFLAG_CLIMBABLE},
	{FLAG_STR("spriteclip"), THING_EFLAG_SPRCLIP},
	{FLAG_STR("noradius"), THING_EFLAG_NORADIUS},
	{FLAG_STR("waterspec"), THING_EFLAG_WATERSPEC},
	{FLAG_STR("canpush"), THING_EFLAG_CANPUSH},
	{FLAG_STR("pushable"), THING_EFLAG_PUSHABLE},
};

// editable animations
static const thing_edit_anim_t thing_anim[NUM_THING_ANIMS] =
{
	[ANIM_SPAWN] = {"spawn", "SPAWN", 5},
	[ANIM_PAIN] = {"pain", "PAIN", 4},
	[ANIM_DEATH] = {"death", "DEATH", 5},
	[ANIM_MOVE] = {"move", "MOVE", 4},
	[ANIM_FIRE] = {"ranged", "RANGED", 6},
	[ANIM_MELEE] = {"melee", "MELEE", 5},
	[ANIM_ACTIVE] = {"active", "ACTIVE", 6},
	[ANIM_INACTIVE] = {"inactive", "INACTIVE", 8},
};
static const thing_edit_anim_t weapon_anim[NUM_THING_ANIMS] =
{
	[ANIM_SPAWN] = {"spawn", "SPAWN", 5},
	[ANIM_PAIN] = {"user0", "USER0", 5},
	[ANIM_DEATH] = {"user1", "USER1", 5},
	[ANIM_MOVE] = {"ready", "READY", 5},
	[ANIM_FIRE] = {"atk", "ATK", 3},
	[ANIM_MELEE] = {"alt", "ALT", 3},
	[ANIM_ACTIVE] = {"raise", "RAISE", 5},
	[ANIM_INACTIVE] = {"lower", "LOWER", 5},
};

const state_action_def_t state_action_def[] =
{
	{
		.name = "\t", // no action
		.flags = AFLG_THING | AFLG_WEAPON
	},
	{
		.name = "weapon: ready",
		.flags = AFLG_WEAPON
	},
	{
		.name = "weapon: raise",
		.flags = AFLG_WEAPON,
		.arg[0] =
		{
			.name = "speed",
			.type = ARGT_U8,
			.def = 10,
			.lim = {1, 64}
		}
	},
	{
		.name = "weapon: lower",
		.flags = AFLG_WEAPON,
		.arg[0] =
		{
			.name = "speed",
			.type = ARGT_U8,
			.def = 10,
			.lim = {1, 64}
		}
	},
///
	{
		.name = "attack: projectile",
		.flags = AFLG_THING | AFLG_WEAPON,
		.arg[0] =
		{
			.name = "spawn",
			.type = ARGT_SPAWN_SLOT,
			.def = 0,
			.lim = {0, 3}
		},
		.arg[1] =
		{
			.name = "spread",
			.type = ARGT_XY_SPREAD,
			.def = 0,
			.lim = {0, 255}
		}
	},
	{
		.name = "attack: hitscan",
		.flags = AFLG_THING | AFLG_WEAPON,
		.arg[0] =
		{
			.name = "spawn",
			.type = ARGT_SPAWN_SLOT,
			.def = 0,
			.lim = {0, 3}
		},
		.arg[1] =
		{
			.name = "spread",
			.type = ARGT_XY_SPREAD,
			.def = 0,
			.lim = {0, 255}
		},
		.arg[2] =
		{
			.name = "count",
			.type = ARGT_U8,
			.def = 1,
			.lim = {1, 8}
		},
	},
///
	{
		.name = "effect: blood splat",
		.flags = AFLG_THING,
		.arg[0] =
		{
			.name = "power",
			.type = ARGT_U8,
			.def = 5,
			.lim = {0, 32}
		},
		.arg[1] =
		{
			.name = "boost power",
			.type = ARGT_U8,
			.def = 12,
			.lim = {0, 96}
		},
		.arg[2] =
		{
			.name = "boost chance",
			.type = ARGT_CHANCE,
			.def = 64,
			.lim = {0, 128}
		}
	},
///
	{
		.name = "ticks: add",
		.flags = AFLG_THING | AFLG_WEAPON,
		.arg[0] =
		{
			.name = "rng",
			.type = ARGT_U8,
			.def = 1,
			.lim = {1, 255}
		}
	},
///
	{
		.name = "death: simple",
		.flags = AFLG_THING,
		.arg[0] =
		{
			.name = "gravity",
			.type = ARGT_U8,
			.def = 128,
			.lim = {0, 255}
		},
		.arg[1] =
		{
			.name = "blocking",
			.type = ARGT_BLOCK_FLAGS,
			.def = 0,
			.lim = {0, 255}
		},
		.arg[2] =
		{
			.name = "blocked by",
			.type = ARGT_BLOCK_FLAGS,
			.def = 0,
			.lim = {0, 255}
		}
	},
	{
		.name = "death: radius",
		.flags = AFLG_THING,
		.arg[0] =
		{
			.name = "gravity",
			.type = ARGT_U8,
			.def = 128,
			.lim = {0, 255}
		},
		.arg[1] =
		{
			.name = "blocking",
			.type = ARGT_BLOCK_FLAGS,
			.def = 0,
			.lim = {0, 255}
		},
		.arg[2] =
		{
			.name = "blocked by",
			.type = ARGT_BLOCK_FLAGS,
			.def = 0,
			.lim = {0, 255}
		}
	},
	// terminator
	{}
};

//
static const uint8_t *block_req_text[] =
{
	"Bits which this thing will block.",
	"Bits which this thing will block. (alt)",
	"Bits by which this thing is blocked.",
	"Bits by which this thing is blocked. (alt)",
};

// state argument type parsers

static void te_arg_us8(uint8_t*);
static void te_arg_spawn(uint8_t*);
static void te_arg_spread(uint8_t*);
static void af_block_flags();

const arg_parse_t arg_parse[] =
{
	[ARGT_U8 - 1] = {"Enter a value (%d to %d).", te_arg_us8},
	[ARGT_S8 - 1] = {"Enter a value (%d to %d).", te_arg_us8},
	[ARGT_CHANCE - 1] = {"Enter a value (%d to %d).", te_arg_us8},
	[ARGT_SPAWN_SLOT - 1] = {"Enter spawn slot (A to D).", te_arg_spawn},
	[ARGT_XY_SPREAD - 1] = {"Enter two values (0 to 15).", te_arg_spread},
	[ARGT_BLOCK_FLAGS - 1] = {NULL, af_block_flags},
};

//
static uint32_t parse_animation_link(const uint8_t *name, uint32_t *tdx, uint32_t *adx, uint32_t is_weapon);

// state click functions
static int32_t uin_state_idx(glui_element_t*, int32_t, int32_t);
static int32_t uin_state_sprite(glui_element_t*, int32_t, int32_t);
static int32_t uin_state_frame(glui_element_t*, int32_t, int32_t);
static int32_t uin_state_ticks(glui_element_t*, int32_t, int32_t);
static int32_t uin_state_next(glui_element_t*, int32_t, int32_t);
static int32_t uin_state_action(glui_element_t*, int32_t, int32_t);
static int32_t uin_state_arg(glui_element_t*, int32_t, int32_t);

// CBOR
static int32_t cbor_cb_thing(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_cb_anims(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_cb_attrs(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_cb_flags(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_cb_args(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);

static const edit_cbor_obj_t cbor_root[] =
{
	[CBOR_ROOT_THINGS] =
	{
		.name = "things",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_cb_thing
	},
	// terminator
	[NUM_CBOR_ROOT] = {}
};

static const edit_cbor_obj_t cbor_thing[] =
{
	[CBOR_THING_NAME] =
	{
		.name = "name",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_STRING,
		.extra = LEN_X16_THING_NAME,
		.ptr = load_thing.name.text,
	},
	[CBOR_THING_ATTRIBUTES] =
	{
		.name = "attributes",
		.nlen = 10,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_cb_attrs
	},
	[CBOR_THING_ANIMATIONS] =
	{
		.name = "animations",
		.nlen = 10,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_cb_anims
	},
	[CBOR_THING_FLAGS] =
	{
		.name = "flags",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_cb_flags
	},
	// terminator
	[NUM_CBOR_THING] = {}
};

static edit_cbor_obj_t cbor_state[] =
{
	[CBOR_STATE_SPRITE] =
	{
		.name = "sprite",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_STRING,
		.extra = LEN_X16_TEXTURE_NAME,
		.ptr = load_state_sprite,
	},
	[CBOR_STATE_FRAME] =
	{
		.name = "frame",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_state.frame,
	},
	[CBOR_STATE_TICKS] =
	{
		.name = "ticks",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_state.ticks,
	},
	[CBOR_STATE_NEXT] =
	{
		.name = "next",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_state.next,
	},
	[CBOR_STATE_ACTION] =
	{
		.name = "action",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_STRING
	},
	[CBOR_STATE_ARGS] =
	{
		.name = "args",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_cb_args
	},
	// terminator
	[NUM_CBOR_STATE] = {}
};

//
// CBOR callbacks

static int32_t cbor_cb_args(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_VALUE)
		return 1;

	if(ctx->index >= 3)
		return 1;

	load_args[ctx->index] = value->u8;

	return 0;
}

static int32_t cbor_cb_state(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func;

	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		thing_anim_t *ta = load_thing.anim + cbor_entry_index;
		thing_st_t *st = ta->state + cbor_sub_index;
		const state_action_def_t *sa = state_action_def;
		int32_t link;

		ta->count = cbor_sub_index + 1;

		load_state.action = 0;
		while(sa->name)
		{
			if(!strcmp(load_action, sa->name))
			{
				load_state.action = sa - state_action_def;

				for(uint32_t i = 0; i < 3; i++)
				{
					const state_arg_def_t *arg = sa->arg + i;
					int32_t val = load_args[i];

					if(arg->type == ARGT_S8)
						val = (int8_t)load_args[i];
					else
						val = load_args[i];

					if(val < arg->lim[0])
						val = arg->lim[0];
					else
					if(val > arg->lim[1])
						val = arg->lim[1];

					load_state.arg[i] = val;
				}

				break;
			}
			sa++;
		}

		if(load_state.next & 0x80)
			load_state.next &= 0x87;

		load_state.frame &= 0b10011111;

		*st = load_state;

		link = x16g_spritelink_by_name(load_state_sprite, THING_CHECK_WEAPON_SPRITE(cbor_entry_index, cbor_main_index));
		if(link < 0)
			st->sprite = 0;
		else
			st->sprite = editor_sprlink[link].hash;

		return 1;
	}

	func = edit_cbor_branch(cbor_state, type, key, ctx->key_len, value, ctx->val_len);
	if(func)
	{
		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

static int32_t cbor_cb_states(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	if(ctx->index >= MAX_STATES_IN_ANIMATION)
		return 1;

	memset(&load_state, 0, sizeof(load_state));
	memset(&load_args, 0, sizeof(load_args));

	load_action[0] = '\t';
	cbor_state[CBOR_STATE_ACTION].ptr = load_action;
	cbor_state[CBOR_STATE_ACTION].extra = MAX_ACTION_NAME;

	cbor_sub_index = ctx->index;
	ctx->entry_cb = cbor_cb_state;

	return 0;
}

static int32_t cbor_cb_attrs(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_VALUE)
		return 1;

	for(uint32_t i = 0; i < NUM_SHOW_ATTRS; i++)
	{
		const thing_edit_attr_t *ta = thing_attr + i;

		if(	ctx->key_len == ta->nlen &&
			!memcmp(key, ta->name, ta->nlen)
		){
			union
			{
				uint8_t u8;
				uint16_t u16;
			} *dst = (void*)&load_thing.info + ta->offs;

			switch(ta->type)
			{
				case ATTR_TYPE_U16:
					dst->u16 = value->u16;
				break;
				default:
					dst->u8 = value->u8;
				break;
			}

			return 0;
		}
	}

	return 1;
}

static int32_t cbor_cb_anims(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	const thing_edit_anim_t *anim;

	if(	type != KGCBOR_TYPE_ARRAY &&
		type != KGCBOR_TYPE_STRING
	)
		return 1;

	cbor_entry_index = NUM_THING_ANIMS;
	anim = THING_CHECK_WEAPON_SPRITE(NUM_THING_ANIMS, cbor_main_index) ? weapon_anim : thing_anim;
	for(uint32_t i = 0; i < NUM_THING_ANIMS; i++, anim++)
	{
		if(	anim->nlen == ctx->key_len &&
			!memcmp(key, anim->name, anim->nlen)
		){
			cbor_entry_index = i;
			break;
		}
	}

	if(cbor_entry_index >= NUM_THING_ANIMS)
		return 1;

	if(type == KGCBOR_TYPE_STRING)
	{
		thing_anim_t *ta = load_thing.anim + cbor_entry_index;
		uint32_t tdx, adx;

		ta->count = 0;

		if(ctx->val_len && ctx->val_len < LEN_X16_THING_NAME + 12)
		{
			ta->count = -1;
			memcpy(ta->load_link, value->ptr, ctx->val_len);
			ta->load_link[ctx->val_len] = 0;
		}

		return 0;
	}

	ctx->entry_cb = cbor_cb_states;

	return 0;
}

static int32_t cbor_cb_flags(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_BOOLEAN)
		return 1;

	for(uint32_t i = 0; i < NUM_SHOW_FLAGS; i++)
	{
		const thing_edit_flag_t *tf = thing_flag + i;

		if(ctx->key_len != tf->nlen)
			continue;

		if(memcmp(key, tf->name, tf->nlen))
			continue;

		if(value->u8)
			load_thing.info.eflags |= tf->flag;
		else
			load_thing.info.eflags &= ~tf->flag;

		break;
	}

	return 0;
}

static int32_t cbor_cb_thing_single(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func;

	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		thing_def_t *ti = thing_info + cbor_main_index;

		*ti = load_thing;

		if(ti->name.text[0])
			ti->name.hash = x16c_crc(ti->name.text, -1, THING_CRC_XOR);

		for(uint32_t i = 0; i < NUM_THING_ANIMS; i++)
		{
			thing_anim_t *ta = ti->anim + i;
			thing_anim_t *tao;
			uint32_t tdx, adx;
			uint32_t is_weapon;

			if(ta->count >= 0)
				continue;

			ta->count = 0;

			is_weapon = THING_CHECK_WEAPON_SPRITE(i, cbor_main_index);

			if(	!parse_animation_link(ta->load_link, &tdx, &adx, is_weapon) &&
				is_weapon == THING_CHECK_WEAPON_SPRITE(adx, tdx)
			){
				tao = thing_info[tdx].anim + adx;
				if(tao->count >= 0 || tao == ta)
				{
					ta->link.thing = tdx;
					ta->link.anim = 0x80 | adx;
				}
			}
		}

		return 1;
	}

	func = edit_cbor_branch(cbor_thing, type, key, ctx->key_len, value, ctx->val_len);
	if(func)
	{
		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

static int32_t cbor_cb_thing(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	if(ctx->index >= MAX_X16_THING_TYPES)
		return 1;

	cbor_main_index = ctx->index;
	ctx->entry_cb = cbor_cb_thing_single;

	load_thing = thing_info[cbor_main_index];

	return 0;
}

static int32_t cbor_cb_root(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func = edit_cbor_branch(cbor_root, type, key, ctx->key_len, value, ctx->val_len);

	if(func)
	{
		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

//
// stuff

static void fill_sprite_name(uint8_t *dest, uint32_t hash)
{
	if(!hash)
	{
		// none
		strcpy(dest, "\t");
		return;
	}

	// find hash
	for(uint32_t i = 0; i < x16_num_sprlnk; i++)
	{
		editor_sprlink_t *sl = editor_sprlink + i;
		if(sl->hash == hash)
		{
			strcpy(dest, sl->name);
			return;
		}
	}

	// default to unknown
	strcpy(dest, "*\t*");
}

static uint32_t type_by_name(const uint8_t *name)
{
	uint32_t hash;

	hash = x16c_crc(name, -1, THING_CRC_XOR);

	for(uint32_t i = 0; i < MAX_X16_THING_TYPES; i++)
	{
		if(thing_info[i].name.hash == hash)
			return i;
	}

	return THING_TYPE_UNKNOWN;
}

static uint32_t parse_animation_link(const uint8_t *name, uint32_t *tdx, uint32_t *adx, uint32_t is_weapon)
{
	uint32_t nlen = strlen(name);
	uint32_t i = 0;
	uint32_t hash, alen, ai, ti;

	for( ; i < nlen && name[i] != '@'; i++);

	if(i >= nlen)
		return 3;

	alen = i++;

	if(is_weapon)
	{
		for(ai = 0; ai < NUM_THING_ANIMS; ai++)
		{
			if(weapon_anim[ai].nlen != alen)
				continue;
			if(!memcmp(weapon_anim[ai].name, name, alen))
				break;
		}
	} else
	{
		for(ai = 0; ai < NUM_THING_ANIMS; ai++)
		{
			if(thing_anim[ai].nlen != alen)
				continue;
			if(!memcmp(thing_anim[ai].name, name, alen))
				break;
		}
	}

	if(ai >= NUM_THING_ANIMS)
		return 1;

	nlen -= i;
	hash = x16c_crc(name + i, nlen, THING_CRC_XOR);

	for(ti = 0; ti < MAX_X16_THING_TYPES; ti++)
	{
		if(thing_info[ti].name.hash == hash)
			break;
	}

	if(!hash || ti >= MAX_X16_THING_TYPES)
		return 2;

	*tdx = ti;
	*adx = ai;

	return 0;
}

//
// update

static const uint8_t *make_arg_text(export_type_t *ti, thing_st_t *st, const state_action_def_t *sa, int32_t slot)
{
	static uint8_t text[32];
	const state_arg_def_t *sd;
	uint32_t temp;
	union
	{
		uint8_t u8;
		int8_t s8;
	} *val;

	if(slot < 0)
	{
		slot = -slot - 1;
		sd = sa->arg + slot;
		if(!sd->name)
		{
			sprintf(text, "arg[%u]", slot);
			return text;
		} else
			return sd->name;
	}

	if(sa == state_action_def)
		return "---";

	sd = sa->arg + slot;
	val = (void*)st->arg + slot;

	switch(sd->type)
	{
		case ARGT_NONE:
			return "---";
		case ARGT_U8:
			sprintf(text, "%u", (uint32_t)val->u8);
		break;
		case ARGT_S8:
			sprintf(text, "%d", (int32_t)val->s8);
		break;
		case ARGT_CHANCE:
			sprintf(text, "%u / 128", (uint32_t)val->u8);
		break;
		case ARGT_SPAWN_SLOT:
			if(val->u8 >= THING_MAX_SPAWN_TYPES)
				return "---";

			temp = ti->spawn[val->u8];

			if(temp >= MAX_X16_THING_TYPES)
				return "---";

			if(thing_info[temp].name.text[0])
				sprintf(text, "%s", thing_info[temp].name.text);
			else
				sprintf(text, "#%u", temp);
		break;
		case ARGT_XY_SPREAD:
			temp = val->u8;
			sprintf(text, "X(%u) Y(%u)", temp & 15, temp >> 4);
		break;
		case ARGT_BLOCK_FLAGS:
			edit_put_blockbits(text, val->u8)[0] = 0;
		break;
	}

	return text;
}

void x16t_update_thing_view(uint32_t force_show_state)
{
	thing_def_t *ti = thing_info + show_thing;
	thing_anim_t *ta = ti->anim + show_anim;
	const thing_edit_anim_t *anim_name_tab = thing_anim;
	uint32_t is_link = 0;
	glui_text_t *txt;
	uint8_t text[64];
	uint32_t i;

	anim_name_tab = THING_CHECK_WEAPON_SPRITE(NUM_THING_ANIMS, show_thing) ? weapon_anim : thing_anim;

	ui_thing_state_preview.base.disabled = 1;
	ui_thing_state_origin.base.disabled = 1;
	ui_thing_state_bbox.base.disabled = 1;

	if(force_show_state)
	{
		if(show_state >= top_state + NUM_STATE_ROWS)
		{
			top_state = show_state - NUM_STATE_ROWS + 1;
			if(top_state < 0)
				top_state = 0;
		} else
		if(show_state < top_state)
			top_state = show_state;
	}

	sprintf(text, "%u", show_thing);
	glui_set_text(&ui_thing_idx, text, glui_font_huge_kfn, GLUI_ALIGN_CENTER_CENTER);

	glui_set_text(&ui_thing_title, ti->name.text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	// animation
	if(ta->count < 0)
	{
		uint8_t *tn = thing_info[ta->link.thing].name.text;

		if(tn[0])
			sprintf(text, "Animation: %s; link to thing '%s' animation %s.", anim_name_tab[show_anim].bold, tn, anim_name_tab[ta->link.anim & 0x7F].bold);
		else
			sprintf(text, "Animation: %s; link to thing #%u animation %s.", anim_name_tab[show_anim].bold, ta->link.thing, anim_name_tab[ta->link.anim & 0x7F].bold);

		ta = thing_info[ta->link.thing].anim + (ta->link.anim & 0x7F);
		is_link = 1;
	} else
		sprintf(text, "Animation: %s; %u state%s", anim_name_tab[show_anim].bold, ta->count, ta->count == 1 ? "" : "s");

	glui_set_text(&ui_thing_state_anim, text, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);

	// attributes
	txt = text_attribute;
	for(i = 0; i < NUM_SHOW_ATTRS; i++)
	{
		const thing_edit_attr_t *ta = thing_attr + i;
		union
		{
			uint8_t u8;
			uint16_t u16;
		} *src = (void*)&ti->info + ta->offs;

		switch(ta->type)
		{
			case ATTR_TYPE_U16:
				sprintf(text, "%u", src->u16);
			break;
			case ATTR_TYPE_CHANCE:
				sprintf(text, "%u / 128", src->u8);
			break;
			case ATTR_TYPE_BLOCK_FLAGS:
				edit_put_blockbits(text, src->u8)[0] = 0;
			break;
			case ATTR_TYPE_TT:
				if(src->u8 < MAX_X16_THING_TYPES)
				{
					thing_def_t *to = thing_info + src->u8;
					if(to->name.text[0])
						sprintf(text, "%s", to->name.text);
					else
						sprintf(text, "#%u", src->u8);
				} else
					*(uint16_t*)text = '\t';
			break;
			case ATTR_TYPE_SCALE:
				sprintf(text, "%.3f", ((float)src->u8 * 2.0f + 64.0f) / 256.0f);
			break;
			case ATTR_TYPE_IMASS:
				sprintf(text, "%.3f", (float)src->u8 / 64.0f);
			break;
			case ATTR_TYPE_U7F:
				// special flag MSB
				if(!(src->u8 & 0x80))
					goto u8;
				sprintf(text, "*%u", src->u8 & 0x7F);
			break;
			case ATTR_TYPE_VIEW_HEIGHT:
				// based on height when zero
				if(src->u8)
					goto u8;
				sprintf(text, "[%u]", ti->info.height * 3 / 4); // also see 'x16t_export'
			break;
			case ATTR_TYPE_ATK_HEIGHT:
				// based on height when zero
				if(src->u8)
					goto u8;
				sprintf(text, "[%u]", ti->info.height * 8 / 12); // also see 'x16t_export'
			break;
			case ATTR_TYPE_WATER_HEIGHT:
				// based on height when zero, special when 0xFF
				if(src->u8)
					goto u8;
				if(src->u8 == 0xFF)
					sprintf(text, "---");
				else
					sprintf(text, "[%u]", ti->info.height * 5 / 8); // also see 'x16t_export'
			break;
			case ATTR_TYPE_ARADIUS:
				// equal to radius when zero
				if(src->u8)
					goto u8;
				sprintf(text, "[%u]", ti->info.radius);
			break;
			default: // ATTR_TYPE_U8
u8:
				sprintf(text, "%u", src->u8);
			break;
		}

		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_RIGHT);
		txt->x -= 8;

		txt += 2;
	}

	// flags
	txt = text_flag;
	for(i = 0; i < NUM_SHOW_FLAGS; i++)
	{
		const thing_edit_flag_t *tf = thing_flag + i;
		const uint8_t *fv = ti->info.eflags & tf->flag ? "ON" : "off";

		glui_set_text(txt, fv, glui_font_small_kfn, GLUI_ALIGN_BOT_RIGHT);
		txt->x -= 8;

		txt += 2;
	}

	// animations
	txt = text_animation;
	for(i = 0; i < NUM_THING_ANIMS; i++)
	{
		thing_anim_t *anim = ti->anim + i;
		uint8_t *out = text;

		txt->base.color[0] = show_anim == i ? 0x14FFFFFF : 0;

		if(anim->count < 0)
		{
			thing_def_t *tl = thing_info + anim->link.thing;
			if(tl->name.text[0])
				sprintf(text, "%s@%s", anim_name_tab[anim->link.anim & 0x7F].name, tl->name.text);
			else
				sprintf(text, "%s@#%u", anim_name_tab[anim->link.anim & 0x7F].name, anim->link.thing);
		} else
		if(anim->count)
			sprintf(text, "%u state%s", anim->count, anim->count == 1 ? "" : "s");
		else
			out = "\t";

		glui_set_text(txt, out, glui_font_small_kfn, GLUI_ALIGN_BOT_RIGHT);
		txt->x -= 8;
		txt++;

		glui_set_text(txt, anim_name_tab[i].name, glui_font_small_kfn, GLUI_ALIGN_BOT_LEFT);
		txt->x += 8;
		txt++;
	}

	// preview
	if(ta->count)
	{
		thing_st_t *st = ta->state + show_state;
		int32_t idx;

		idx = x16g_spritelink_by_hash(st->sprite);
		if(idx >= 0)
		{
			uint8_t frm = st->frame & 31;
			if(editor_sprlink[idx].vmsk & (1 << frm))
			{
				uint32_t scale = 3;

				ui_thing_state_origin.base.disabled = hide_origin;
				ui_thing_state_bbox.base.disabled = hide_origin;

				glBindTexture(GL_TEXTURE_2D, x16_thing_texture[0]);

				if(idx >= x16_num_sprlnk_thg)
				{
					if(!x16g_generate_weapon_texture(idx, frm, NULL))
					{
						ui_thing_state_preview.base.disabled = 0;
						ui_thing_state_origin.base.disabled = 1;
						ui_thing_state_bbox.base.disabled = 1;

						ui_thing_state_preview.base.x = ui_thing_state_origin.base.x;
						ui_thing_state_preview.base.y = 472;

						ui_thing_state_preview.base.width = 160 * 3;
						ui_thing_state_preview.base.height = 120 * 3;

						ui_thing_state_preview.coord.s[0] = 48.0f / 256.0f;
						ui_thing_state_preview.coord.s[1] = 208.0f / 256.0f;
						ui_thing_state_preview.coord.t[0] = 68.0f / 256.0f;
						ui_thing_state_preview.coord.t[1] = 188.0f / 256.0f;
					}
				} else
				{
					if(!x16g_generate_state_texture(idx, frm, show_angle | 0x80000000))
					{
						float fcale;

						ui_thing_state_preview.base.disabled = 0;
						ui_thing_state_origin.base.disabled = 0;
						ui_thing_state_bbox.base.disabled = 0;

						if(x16g_state_res[1] > 128)
							scale = 2;

						ui_thing_state_preview.base.x = ui_thing_state_origin.base.x - x16g_state_offs[0] * scale;
						ui_thing_state_preview.base.y = ui_thing_state_origin.elements[0]->base.y + ui_thing_state_origin.base.y - x16g_state_offs[1] * scale;

						ui_thing_state_preview.base.width = x16g_state_res[0] * scale;
						ui_thing_state_preview.base.height = x16g_state_res[1] * scale;

						ui_thing_state_bbox.base.x = ui_thing_state_origin.base.x;

						if(scale == 3)
						{
							if(!(x16g_state_res[0] & 1))
							{
								ui_thing_state_bbox.base.x--;
								ui_thing_state_preview.base.x--;
							}
							ui_thing_state_preview.base.y += 2;
						} else
							ui_thing_state_preview.base.y++;

						ui_thing_state_preview.coord.s[0] = 0.0f;
						ui_thing_state_preview.coord.s[1] = (float)x16g_state_res[0] / (float)x16g_state_res[2];
						ui_thing_state_preview.coord.t[0] = 0.0f;
						ui_thing_state_preview.coord.t[1] = 1.0f;

						fcale = 256.0f / ((float)ti->info.scale * 2.0f + 64.0f);

						ui_thing_state_bbox.base.width = 2 + (ti->info.radius * scale) * fcale;
						ui_thing_state_bbox.base.height = 2 + ((ti->info.height * scale) / 2) * fcale;
					}
				}
			}
		}
	}

	// states
	txt = text_states;
	for(i = 0; i < NUM_STATE_ROWS; i++)
	{
		uint32_t ii = i + top_state;
		thing_st_t *st = ta->state + ii;
		const state_action_def_t *sa = state_action_def + st->action;
		void *arg_func = is_link ? uin_state_idx : uin_state_arg;
		uint32_t color;

		if(ii >= ta->count)
			break;

		color = ii == show_state ? 0xFFFF0000 : 0xFF000000;

		// idx
		sprintf(text, "%u", ii);
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = uin_state_idx;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// sprite
		fill_sprite_name(text, st->sprite);
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = is_link ? uin_state_idx : uin_state_sprite;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// frame
		if(st->frame & 0x80)
			sprintf(text, "[%c]", (st->frame & 31) + 'A');
		else
			sprintf(text, "%c", (st->frame & 31) + 'A');
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = is_link ? uin_state_idx : uin_state_frame;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// ticks
		if(st->ticks)
			sprintf(text, "%u", st->ticks);
		else
			strcpy(text, "*");
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = is_link ? uin_state_idx : uin_state_ticks;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// next
		if(st->next & 0x80)
			sprintf(text, "%s", anim_name_tab[st->next & 7].name);
		else
		if(st->next >= ta->count)
			strcpy(text, "\t");
		else
			sprintf(text, "%u", st->next);
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = is_link ? uin_state_idx : uin_state_next;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// action
		glui_set_text(txt, sa->name, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = is_link ? uin_state_idx : uin_state_action;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// arg 0
		glui_set_text(txt, make_arg_text(&ti->info, st, sa, 0), glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = arg_func;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// arg 1
		glui_set_text(txt, make_arg_text(&ti->info, st, sa, 1), glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = arg_func;
		txt->base.custom |= 0x100;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// arg 2
		glui_set_text(txt, make_arg_text(&ti->info, st, sa, 2), glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->base.click = arg_func;
		txt->base.custom |= 0x200;
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;
	}

	// action args
	{
		thing_st_t *st = ta->state + show_state;
		const state_action_def_t *sa = state_action_def + st->action;

		glui_set_text(&ui_thing_state_arg0, make_arg_text(&ti->info, st, sa, -1), glui_font_small_kfn, GLUI_ALIGN_TOP_CENTER);
		glui_set_text(&ui_thing_state_arg1, make_arg_text(&ti->info, st, sa, -2), glui_font_small_kfn, GLUI_ALIGN_TOP_CENTER);
		glui_set_text(&ui_thing_state_arg2, make_arg_text(&ti->info, st, sa, -3), glui_font_small_kfn, GLUI_ALIGN_TOP_CENTER);
	}

	ui_thing_states.elements[0]->container.count = i * NUM_STATE_COLS;
}

//
// text entry

static void te_select(uint8_t *name)
{
	uint32_t value;

	if(!name)
		return;

	if(!name[0])
		return;

	value = x16c_crc(name, -1, THING_CRC_XOR);

	for(uint32_t i = 0; i < MAX_X16_THING_TYPES; i++)
	{
		if(thing_info[i].name.hash == value)
		{
			top_state = 0;
			show_state = 0;
			show_thing = i;
			x16t_update_thing_view(0);
			return;
		}
	}

	if(sscanf(name, "%u", &value) == 1 && value < MAX_X16_THING_TYPES)
	{
		top_state = 0;
		show_state = 0;
		show_thing = value;
		x16t_update_thing_view(0);
		return;
	}

	edit_status_printf("Invalid value!");
}

static void te_rename(uint8_t *name)
{
	uint32_t hash;

	if(!name)
		return;

	if(edit_check_name(name, ':'))
	{
		edit_status_printf("Invalid thing name!");
		return;
	}

	hash = x16c_crc(name, -1, THING_CRC_XOR);

	for(uint32_t i = 0; i < MAX_X16_THING_TYPES; i++)
	{
		if(thing_info[i].name.hash == hash)
		{
			edit_status_printf("Thing with this name already exists!");
			return;
		}
	}

	strcpy(thing_info[show_thing].name.text, name);
	thing_info[show_thing].name.hash = hash;

	x16t_update_thing_view(0);
}

static void te_set_attr(uint8_t *text)
{
	uint32_t orval = 0;
	uint32_t value, type;
	float falue;
	const thing_edit_attr_t *ta = thing_attr + set_attr;
	union
	{
		uint8_t u8;
		uint16_t u16;
	} *dst = (void*)&thing_info[show_thing].info + ta->offs;

	if(!text)
		return;

	type = ta->type;

	if(type == ATTR_TYPE_U7F)
	{
		if(text[0] < '0' || text[0] > '9')
		{
			text++;
			orval = 0x80;
		}

		type = ATTR_TYPE_U8;
		if(sscanf(text, "%u", &value) != 1 || value > 127)
			goto invalid;
	} else
	if(type == ATTR_TYPE_SCALE)
	{
		if(sscanf(text, "%f", &falue) != 1)
			goto invalid;
		falue *= 256.0f;
		falue -= 64.0f;
		falue /= 2.0f;
		if(falue < 0)
			value = 0;
		else
		if(falue > 255.9f)
			value = 255;
		else
			value = falue;
	} else
	if(type == ATTR_TYPE_IMASS)
	{
		if(sscanf(text, "%f", &falue) != 1)
			goto invalid;
		falue *= 64.0f;
		if(falue < 0)
			value = 0;
		else
		if(falue > 255.9f)
			value = 255;
		else
			value = falue;
	} else
	if(sscanf(text, "%u", &value) != 1)
		goto invalid;

	value |= orval;

	if(type == ATTR_TYPE_U16)
	{
		if(value >= 0x10000)
			goto invalid;
		dst->u16 = value;
	} else
	{
		if(value >= 0x100)
			goto invalid;
		dst->u8 = value;
	}

	x16t_update_thing_view(0);

	return;

invalid:
	edit_status_printf("Invalid value!");
	return;
}

//
// common text entry

static void te_set_u8(uint8_t *text)
{
	uint32_t value;

	if(!text)
		return;

	if(!text[0])
	{
		*num_ptru8 = num_empty;
		x16t_update_thing_view(0);
		return;
	}

	if(sscanf(text, "%u", &value) != 1 || value >= num_limit)
	{
		edit_status_printf("Invalid value!");
		return;
	}

	*num_ptru8 = value;
	x16t_update_thing_view(0);
}

//
// input

static void te_anim_link(uint8_t *name)
{
	uint32_t tdx, adx;
	thing_def_t *ti;
	thing_anim_t *ta;
	thing_anim_t *tax;
	uint32_t changed = 0;
	uint32_t is_weapon = THING_CHECK_WEAPON_SPRITE(show_anim, show_thing);

	if(!name)
		return;

	if(!name[0])
	{
		ti = thing_info + show_thing;
		ta = ti->anim + show_anim;
		ta->count = 0;
		x16t_update_thing_view(0);
		return;
	}

	switch(parse_animation_link(name, &tdx, &adx, is_weapon))
	{
		case 0:
		break;
		case 1:
			edit_status_printf("Unknown animation!");
			return;
		case 2:
			edit_status_printf("Unknown thing!");
			return;
		default:
			edit_status_printf("Invalid value!");
			return;
	}

	if(is_weapon != THING_CHECK_WEAPON_SPRITE(adx, tdx))
	{
		edit_status_printf("Invalid weapon-thing animation link!");
		return;
	}

	if(tdx == show_thing && adx == show_anim)
	{
		edit_status_printf("That's a bad idea!");
		return;
	}

	ti = thing_info + show_thing;
	ta = ti->anim + show_anim;

	ta->link.thing = tdx;
	ta->link.anim = 0x80 | adx;

	tax = thing_info[tdx].anim + adx;
	if(tax->count < 0)
	{
		tdx = tax->link.thing;
		adx = tax->link.anim & 0x7F;
	}

	if(tdx == show_thing && adx == show_anim)
	{
		edit_status_printf("No way!");
		return;
	}

	for(uint32_t i = 0; i < MAX_X16_THING_TYPES + 1; i++) // with clipboard
	{
		thing_def_t *tio = thing_info + i;

		for(uint32_t j = 0; j < NUM_THING_ANIMS; j++)
		{
			thing_anim_t *tao = tio->anim + j;
			thing_anim_t *tal;

			if(tao->count >= 0)
				continue;

			tal = thing_info[tao->link.thing].anim + (tao->link.anim & 0x7F);
			if(tal->count >= 0)
				continue;

			tao->link.thing = tdx;
			tao->link.anim = 0x80 | adx;

			if(i < MAX_X16_THING_TYPES)
				changed++;
		}
	}

	if(changed == 1)
		edit_status_printf("1 link was modified.");
	else
	if(changed)
		edit_status_printf("%u links were modified.", changed);

	x16t_update_thing_view(0);

	return;
}

int32_t uin_thing_left(glui_element_t *elm, int32_t x, int32_t y)
{
	top_state = 0;
	show_state = 0;

	if(!show_thing)
		show_thing = MAX_X16_THING_TYPES - 1;
	else
		show_thing--;

	x16t_update_thing_view(0);

	return 1;
}

int32_t uin_thing_right(glui_element_t *elm, int32_t x, int32_t y)
{
	top_state = 0;
	show_state = 0;
	show_thing++;

	if(show_thing >= MAX_X16_THING_TYPES)
		show_thing = 0;

	x16t_update_thing_view(0);

	return 1;
}

int32_t uin_thing_input(glui_element_t *elm, uint32_t magic)
{
	switch(magic)
	{
		case EDIT_INPUT_ESCAPE:
			ui_main_menu.base.disabled ^= 1;
		return 1;
		case EDIT_INPUT_LEFT:
			uin_thing_left(elm, 0, 0);
		return 1;
		case EDIT_INPUT_RIGHT:
			uin_thing_right(elm, 0, 0);
		return 1;
		case EDIT_INPUT_UP:
			if(input_alt)
			{
				show_angle--;
				show_angle &= 7;
				x16t_update_thing_view(0);
			} else
			if(top_state)
			{
				top_state--;
				x16t_update_thing_view(0);
			}
		return 1;
		case EDIT_INPUT_DOWN:
			if(input_alt)
			{
				show_angle++;
				show_angle &= 7;
				x16t_update_thing_view(0);
			} else
			{
				thing_def_t *ti = thing_info + show_thing;
				thing_anim_t *ta = ti->anim + show_anim;

				if(ta->count < 0)
				{
					ti = thing_info + ta->link.thing;
					ta = ti->anim + (ta->link.anim & 0x7F);
				}

				if(top_state < ta->count - NUM_STATE_ROWS)
				{
					top_state++;
					x16t_update_thing_view(0);
				}
			}
		return 1;
		case EDIT_INPUT_COPY:
		{
			thing_def_t *td = thing_info + show_thing;

			memcpy(thing_info + MAX_X16_THING_TYPES, td, sizeof(thing_def_t));

			if(!input_shift)
			{
				for(uint32_t i = 0; i < NUM_THING_ANIMS; i++)
				{
					thing_anim_t *ta = thing_info[MAX_X16_THING_TYPES].anim + i;

					if(ta->count <= 0)
						continue;

					ta->link.anim = 0x80 | i;
					ta->link.thing = show_thing;
				}

				edit_status_printf("Thing copied with links.");
			} else
				edit_status_printf("Thing fully copied.");
		}
		return 1;
		case EDIT_INPUT_PASTE:
		{
			thing_name_t tn = thing_info[show_thing].name;
			thing_info[show_thing] = thing_info[MAX_X16_THING_TYPES];
			thing_info[show_thing].name = tn;
			x16t_update_thing_view(0);
			edit_status_printf("Thing pasted.");
		}
		return 1;
	}

	return 0;
}

int32_t uin_thing_select(glui_element_t *elm, int32_t x, int32_t y)
{
	spawn_dst = NULL;
	x16t_generate();
	edit_ui_thing_select(NULL);
	return 1;
}

int32_t uin_thing_rename(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_textentry("Enter new thing name.", LEN_X16_THING_NAME, te_rename);
	return 1;
}

int32_t uin_thing_attr(glui_element_t *elm, int32_t x, int32_t y)
{
	uint8_t text[64];
	const thing_edit_attr_t *ta = thing_attr + elm->base.custom;
	void *src = (void*)&thing_info[show_thing].info + ta->offs;

	switch(ta->type)
	{
		case ATTR_TYPE_TT:
			spawn_dst = (uint8_t*)&thing_info[show_thing].info + ta->offs;
			if(!input_ctrl && !input_alt && !input_shift)
			{
				x16t_generate();
				edit_ui_thing_select(NULL);
				return 1;
			}
			*spawn_dst = 0xFF;
			x16t_update_thing_view(0);
		return 1;
		case ATTR_TYPE_BLOCK_FLAGS:
			edit_ui_blocking_select(block_req_text[ta->offs - offsetof(export_type_t, blocking)], src, 0);
		return 1;
	}

	set_attr = elm->base.custom;
	sprintf(text, "Enter new value for attribute \"%s\".", ta->name);
	edit_ui_textentry(text, 8, te_set_attr);

	return 1;
}

int32_t uin_thing_flag(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_info[show_thing].info.eflags ^= thing_flag[elm->base.custom].flag;
	x16t_update_thing_view(0);
	return 1;
}

int32_t uin_thing_anim(glui_element_t *elm, int32_t x, int32_t y)
{
	top_state = 0;
	show_state = 0;
	show_anim = elm->base.custom;
	x16t_update_thing_view(1);
	return 1;
}

int32_t uin_thing_state_new(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_def_t *ti = thing_info + show_thing;
	thing_anim_t *ta = ti->anim + show_anim;
	thing_st_t *ts;

	if(ta->count < 0)
	{
		edit_status_printf("You can't modify linked states!");
		return 1;
	}

	if(ta->count >= MAX_STATES_IN_ANIMATION)
	{
		edit_status_printf("Too many states in animation!");
		return 1;
	}

	ts = ta->state + ta->count;

	if(ta->count)
		memcpy(ts, ta->state + show_state, sizeof(thing_st_t));
	else
		memset(ts, 0, sizeof(thing_st_t));

	show_state = ta->count;
	ta->count++;
	ts->next = ta->count;

	x16t_update_thing_view(1);

	return 1;
}

int32_t uin_thing_state_ins(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_def_t *ti = thing_info + show_thing;
	thing_anim_t *ta = ti->anim + show_anim;
	thing_st_t *ts;

	if(!ta->count)
		return uin_thing_state_new(elm, x, y);

	if(ta->count < 0)
	{
		edit_status_printf("You can't modify linked states!");
		return 1;
	}

	if(ta->count >= MAX_STATES_IN_ANIMATION)
	{
		edit_status_printf("Too many states in animation!");
		return 1;
	}

	for(uint32_t i = ta->count; i > show_state; i--)
		ta->state[i] = ta->state[i - 1];

	ta->count++;

	for(uint32_t i = 0; i < ta->count; i++)
	{
		ts = ta->state + i;
		if(ts->next > show_state && ts->next < MAX_STATES_IN_ANIMATION)
			ts->next++;
	}

	ts = ta->state + show_state;
	ts->next = show_state + 1;

	x16t_update_thing_view(1);

	return 1;
}

int32_t uin_thing_state_del(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_def_t *ti = thing_info + show_thing;
	thing_anim_t *ta = ti->anim + show_anim;

	if(ta->count < 0)
	{
		edit_status_printf("You can't modify linked states!");
		return 1;
	}

	if(!ta->count)
		return 1;

	ta->count--;

	for(uint32_t i = show_state; i < ta->count; i++)
		ta->state[i] = ta->state[i + 1];

	for(uint32_t i = 0; i < ta->count; i++)
	{
		thing_st_t *ts = ta->state + i;
		if(ts->next < MAX_STATES_IN_ANIMATION)
		{
			if(ts->next > show_state)
				ts->next--;
			if(ts->next >= ta->count && i < ta->count - 1)
				ts->next = MAX_STATES_IN_ANIMATION;
		}
	}

	if(show_state >= ta->count && ta->count)
		show_state = ta->count - 1;

	x16t_update_thing_view(1);

	return 1;
}

int32_t uin_thing_state_lnk(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_textentry("Enter 'anim@thing' code or leave empty.", LEN_X16_THING_NAME + 12, te_anim_link);
	return 1;
}

int32_t uin_thing_state_origin(glui_element_t *elm, int32_t x, int32_t y)
{
	hide_origin = !hide_origin;
	x16t_update_thing_view(0);
	return 1;
}

//
// input for state table

static void te_set_nextstate(uint8_t *text)
{
	const thing_edit_anim_t *anim;
	uint32_t i;

	if(!text)
		return;

	if(text[0] != '@')
	{
		te_set_u8(text);
		return;
	}

	anim = THING_CHECK_WEAPON_SPRITE(NUM_THING_ANIMS, show_thing) ? weapon_anim : thing_anim;
	for(i = 0; i < NUM_THING_ANIMS; i++, anim++)
	{
		if(!strcmp(text + 1, anim->name))
			break;
	}

	if(i >= NUM_THING_ANIMS)
	{
		edit_status_printf("Invalid animation!");
		return;
	}

	*num_ptru8 = 0x80 | i;
	x16t_update_thing_view(0);
}

static void te_arg_us8(uint8_t *text)
{
	int32_t temp;

	if(!text)
		return;

	if(!text[0])
		return;

	if(sscanf(text, "%d", &temp) != 1 || temp < arg_lim[0] || temp > arg_lim[1])
	{
		edit_status_printf("Invalid value!");
		return;
	}

	*arg_dst = temp;
	x16t_update_thing_view(0);
}

static void te_arg_spawn(uint8_t *text)
{
	uint32_t temp;

	if(!text)
		return;

	if(!text[0])
		return;

	text[0] |= 0x20;
	if(text[0] >= 'a' && text[0] <= 'd')
		temp = text[0] - 'a';
	else
	{
		edit_status_printf("Invalid value!");
		return;
	}

	*arg_dst = temp;
	x16t_update_thing_view(0);
}

static uint32_t check_spread(uint32_t *val)
{
	return *val > 15;
}

static void te_arg_spread(uint8_t *text)
{
	uint32_t x, y;

	if(!text)
		return;

	if(!text[0])
		return;

	if(sscanf(text, "%u %u", &x, &y) != 2 || check_spread(&x) || check_spread(&y))
	{
		edit_status_printf("Invalid value!");
		return;
	}

	*arg_dst = x | (y << 4);
	x16t_update_thing_view(0);
}

static void af_block_flags()
{
	edit_ui_blocking_select("Block bits for argument.", arg_dst, 0);
}

static uint32_t select_state(uint32_t idx)
{
	idx += top_state;

	if(show_state == idx)
		return idx;

	show_state = idx;
	x16t_update_thing_view(1);

	return idx;
}

static int32_t uin_state_idx(glui_element_t *elm, int32_t x, int32_t y)
{
	select_state(elm->base.custom & 0xFF);
	return 1;
}

static int32_t uin_state_sprite(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_def_t *ti = thing_info + show_thing;
	int32_t si;

	si = select_state(elm->base.custom);

	if(THING_CHECK_WEAPON_SPRITE(show_anim, show_thing))
		edit_ui_wsprite_select(&ti->anim[show_anim].state[si].sprite);
	else
		edit_ui_tsprite_select(&ti->anim[show_anim].state[si].sprite);

	return 1;
}

static int32_t uin_state_frame(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_def_t *ti = thing_info + show_thing;
	int32_t si;

	si = select_state(elm->base.custom);

	if(!input_ctrl)
	{
		thing_st_t *st = ti->anim[show_anim].state + si;
		if(THING_CHECK_WEAPON_SPRITE(show_anim, show_thing))
			edit_ui_wframe_select(&st->frame, st->sprite);
		else
			edit_ui_tframe_select(&st->frame, st->sprite);
	} else
	{
		if(THING_CHECK_WEAPON_SPRITE(show_anim, show_thing))
			return 1;

		ti->anim[show_anim].state[si].frame ^= 0x80;
		x16t_update_thing_view(0);
	}

	return 1;
}

static int32_t uin_state_ticks(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_def_t *ti = thing_info + show_thing;
	int32_t si;

	si = select_state(elm->base.custom);

	num_empty = 0;
	num_limit = 256;
	num_ptru8 = &ti->anim[show_anim].state[si].ticks;
	edit_ui_textentry("Enter new ticks (0 to 255).", 8, te_set_u8);

	return 1;
}

static int32_t uin_state_next(glui_element_t *elm, int32_t x, int32_t y)
{
	uint8_t text[64];
	thing_def_t *ti = thing_info + show_thing;
	int32_t si;

	si = select_state(elm->base.custom);

	num_empty = MAX_STATES_IN_ANIMATION;
	num_limit = ti->anim[show_anim].count;
	num_ptru8 = &ti->anim[show_anim].state[si].next;

	if(num_limit)
	{
		sprintf(text, "Enter next state (max %u) or @anim or leave empty.", num_limit - 1);
		edit_ui_textentry(text, 16, te_set_nextstate);
	}

	return 1;
}

static int32_t uin_state_action(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_def_t *ti = thing_info + show_thing;
	int32_t si;

	si = select_state(elm->base.custom);

	edit_ui_action_select(ti->anim[show_anim].state + si, THING_CHECK_WEAPON_SPRITE(show_anim, show_thing));

	return 1;
}

static int32_t uin_state_arg(glui_element_t *elm, int32_t x, int32_t y)
{
	thing_def_t *ti = thing_info + show_thing;
	uint32_t arg = elm->base.custom >> 8;
	const state_action_def_t *sa;
	const state_arg_def_t *ar;
	const arg_parse_t *ap;
	uint8_t text[64];
	thing_anim_t *ta;
	thing_st_t *st;
	int32_t si;

	si = select_state(elm->base.custom & 0xFF);

	ta = ti->anim + show_anim;
	st = ta->state + si;
	sa = state_action_def + st->action;

	arg_dst = st->arg + arg;
	ar = sa->arg + arg;

	arg = ar->type;
	if(!arg)
		return 1;

	arg_lim = ar->lim;

	ap = arg_parse + arg - 1;

	if(ap->text)
	{
		sprintf(text, ap->text, (int32_t)arg_lim[0], (int32_t)arg_lim[1]);
		edit_ui_textentry(text, 16, ap->func);
	} else
	{
		void (*cb)() = ap->func;
		cb();
	}

	return 1;
}

//
// load

static void thing_cleanup()
{
	thing_def_t *ti;

	memset(thing_info, 0, sizeof(thing_info));

	for(uint32_t i = 0; i < MAX_X16_THING_TYPES + 1; i++) // with clipboard
		thing_info[i].info = default_thing_info;

	// player (normal)
	ti = thing_info + THING_TYPE_PLAYER_N;
	strcpy(ti->name.text, "Player (normal)");
	ti->name.hash = x16c_crc(ti->name.text, -1, THING_CRC_XOR);
	ti->info = default_player_info;

	// player (crouch)
	ti = thing_info + THING_TYPE_PLAYER_C;
	strcpy(ti->name.text, "Player (crouch)");
	ti->name.hash = x16c_crc(ti->name.text, -1, THING_CRC_XOR);
	ti->info = default_player_info;
	ti->info.height *= 0.5f;
	ti->info.speed *= 0.5f;
	ti->info.step_height *= 0.75f;
	ti->info.water_height = 255;
	ti->info.jump_pwr *= 0.5f;

	// player (swim)
	ti = thing_info + THING_TYPE_PLAYER_S;
	strcpy(ti->name.text, "Player (swim)");
	ti->name.hash = x16c_crc(ti->name.text, -1, THING_CRC_XOR);
	ti->info = default_player_info;
	ti->info.height *= 0.5f;
	ti->info.speed *= 0.5f;
	ti->info.step_height *= 0.75f;
	ti->info.water_height = 255;
	ti->info.jump_pwr = 8;

	// player (flying)
	ti = thing_info + THING_TYPE_PLAYER_F;
	strcpy(ti->name.text, "Player (fly)");
	ti->name.hash = x16c_crc(ti->name.text, -1, THING_CRC_XOR);
	ti->info = default_player_info;
	ti->info.water_height = 255;

	// weapons
	for(uint32_t i = 0; i < THING_WEAPON_COUNT; i++)
	{
		ti = thing_info + THING_WEAPON_FIRST + i;
		sprintf(ti->name.text, "Weapon #%X", i);
		ti->name.hash = x16c_crc(ti->name.text, -1, THING_CRC_XOR);
		ti->info.blocking = BLOCK_FLAG(BLOCKING_SPECIAL);
		ti->info.blockedby = BLOCK_FLAG(BLOCKING_SOLID);
	}
}

void x16t_new_thg()
{
	thing_cleanup();
	x16t_update_thing_view(1);
}

void x16t_load(const uint8_t *file)
{
	uint32_t size;
	int32_t ret;
	kgcbor_ctx_t ctx;

	if(file)
		strcpy(thing_path, file);

	size = edit_read_file(thing_path, edit_cbor_buffer, EDIT_EXPORT_BUFFER_SIZE);
	if(size)
	{
		edit_busy_window("Loading things ...");

		thing_cleanup();

		ctx.entry_cb = cbor_cb_root;
		ctx.max_recursion = 16;
		ret = kgcbor_parse_object(&ctx, edit_cbor_buffer, size);
		if(!ret)
			return;
	}

	thing_cleanup();
	edit_status_printf("Unable to load things file!");
}

//
// save

const uint8_t *x16t_save(const uint8_t *file)
{
	kgcbor_gen_t gen;

	edit_busy_window("Saving things ...");

	if(file)
		strcpy(thing_path, file);

	gen.ptr = edit_cbor_buffer;
	gen.end = edit_cbor_buffer + EDIT_EXPORT_BUFFER_SIZE;

	/// root
	edit_cbor_export(cbor_root, NUM_CBOR_ROOT, &gen);

	/// things
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_THINGS].name, cbor_root[CBOR_ROOT_THINGS].nlen);
	kgcbor_put_array(&gen, MAX_X16_THING_TYPES);

	for(uint32_t i = 0; i < MAX_X16_THING_TYPES; i++)
	{
		thing_def_t *ti = thing_info + i;

		load_thing = *ti;
		edit_cbor_export(cbor_thing, NUM_CBOR_THING, &gen);

		/// attributes
		kgcbor_put_string(&gen, cbor_thing[CBOR_THING_ATTRIBUTES].name, cbor_thing[CBOR_THING_ATTRIBUTES].nlen);
		kgcbor_put_object(&gen, NUM_SHOW_ATTRS);

		for(uint32_t j = 0; j < NUM_SHOW_ATTRS; j++)
		{
			const thing_edit_attr_t *ta = thing_attr + j;
			union
			{
				uint8_t u8;
				uint16_t u16;
			} *src = (void*)&ti->info + ta->offs;

			kgcbor_put_string(&gen, ta->name, ta->nlen);
			if(ta->type == ATTR_TYPE_U16)
				kgcbor_put_u32(&gen, src->u16);
			else
				kgcbor_put_u32(&gen, src->u8);
		}

		/// animations
		kgcbor_put_string(&gen, cbor_thing[CBOR_THING_ANIMATIONS].name, cbor_thing[CBOR_THING_ANIMATIONS].nlen);
		kgcbor_put_object(&gen, NUM_THING_ANIMS);

		for(uint32_t j = 0; j < NUM_THING_ANIMS; j++)
		{
			const thing_edit_anim_t *anim_name_tab = THING_CHECK_WEAPON_SPRITE(NUM_THING_ANIMS, i) ? weapon_anim : thing_anim;
			thing_anim_t *ta = ti->anim + j;

			kgcbor_put_string(&gen, anim_name_tab[j].name, -1);

			if(ta->count < 0)
			{
				uint8_t text[LEN_X16_THING_NAME + 12];
				uint32_t len;
				len = sprintf(text, "%s@%s", anim_name_tab[ta->link.anim & 0x7F].name, thing_info[ta->link.thing].name.text);
				kgcbor_put_string(&gen, text, len);
			} else
			{
				kgcbor_put_array(&gen, ta->count);

				for(uint32_t k = 0; k < ta->count; k++)
				{
					const state_arg_def_t *arg;
					uint32_t ac;

					load_state = ta->state[k];

					arg = state_action_def[load_state.action].arg;
					for(ac = 0; ac < 3; ac++)
						if(!arg[ac].name)
							break;

					if(load_state.action)
						cbor_state[CBOR_STATE_ACTION].ptr = (uint8_t*)state_action_def[load_state.action].name;
					else
						cbor_state[CBOR_STATE_ACTION].ptr = NULL;

					fill_sprite_name(load_state_sprite, ta->state[k].sprite);
					edit_cbor_export(cbor_state, NUM_CBOR_STATE, &gen);

					// args
					kgcbor_put_string(&gen, cbor_state[CBOR_STATE_ARGS].name, cbor_state[CBOR_STATE_ARGS].nlen);
					kgcbor_put_array(&gen, ac);

					for(uint32_t l = 0; l < ac; l++)
						kgcbor_put_u32(&gen, load_state.arg[l]);
				}
			}
		}

		/// flags
		kgcbor_put_string(&gen, cbor_thing[CBOR_THING_FLAGS].name, cbor_thing[CBOR_THING_FLAGS].nlen);
		kgcbor_put_object(&gen, NUM_SHOW_FLAGS);

		for(uint32_t j = 0; j < NUM_SHOW_FLAGS; j++)
		{
			const thing_edit_flag_t *tf = thing_flag + j;
			kgcbor_put_string(&gen, tf->name, tf->nlen);
			kgcbor_put_bool(&gen, ti->info.eflags & tf->flag);
		}
	}

	// save
	if(edit_save_file(thing_path, edit_cbor_buffer, (void*)gen.ptr - edit_cbor_buffer))
		return NULL;

	return edit_path_filename(thing_path);
}

//
// export state fix

static void fix_action_args(export_state_t *st)
{
#if 0
	const state_action_def_t *sd;
	uint32_t act = st->action & 0x7F;

	if(!act)
		return;

	sd = state_action_def + act;

	for(uint32_t i = 0; i < 3; i++)
	{
		const state_arg_def_t *ad = sd->arg + i;

		if(ad->type == )
			st->arg[i] = ;
	}
#endif
}

static void fix_action_anim(export_state_t *st)
{
}

//
// API

uint32_t x16t_init()
{
	void *ptr, *ttex;
	uint32_t *tmptex;
	glui_container_t *cont;
	glui_text_t *txt;
	uint32_t size;
	uint32_t tcnt;

	sprintf(thing_path, X16_PATH_DEFS PATH_SPLIT_STR "%s.thdef", X16_DEFAULT_NAME);

	// GL textures

	glGenTextures(MAX_X16_THING_TYPES, x16_thing_texture);

	for(uint32_t i = 0; i < MAX_X16_THING_TYPES; i++)
	{
		glBindTexture(GL_TEXTURE_2D, x16_thing_texture[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	// UI
	tcnt = (NUM_SHOW_ATTRS + NUM_SHOW_FLAGS + NUM_SHOW_ANIMS + 3) * 2;
	tcnt += NUM_STATE_COLS * NUM_STATE_ROWS;

	size = sizeof(glui_container_t) * 4; // attributes, flags, animations, states
	size += tcnt * (sizeof(glui_text_t*) + sizeof(glui_text_t)); // text elements and container slots

	ptr = calloc(size, 1);
	if(!ptr)
		return 1;

	tmptex = malloc(tcnt * sizeof(uint32_t));
	if(!tmptex)
		return 1;
	ttex = tmptex;

	glGenTextures(tcnt, tmptex);

	for(uint32_t i = 0; i < tcnt; i++)
	{
		glBindTexture(GL_TEXTURE_2D, tmptex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	// attributes
	cont = ptr;
	ui_thing_attrs.elements[0] = ptr;
	ptr += sizeof(glui_container_t) + (NUM_SHOW_ATTRS * 2 + 1) * sizeof(glui_text_t*);
	cont->base.draw = glui_df_container;
	cont->count = (NUM_SHOW_ATTRS * 2 + 1);

	text_attribute = ptr;

	for(uint32_t i = 0; i < NUM_SHOW_ATTRS * 2; i++)
	{
		uint32_t ii = i / 2;

		// value
		txt = ptr;
		ptr += sizeof(glui_text_t);

		cont->elements[i] = (glui_element_t*)txt;

		txt->base.draw = glui_df_text;
		txt->base.click = uin_thing_attr;
		txt->base.custom = ii;

		txt->base.y = ii * UI_ATTR_HEIGHT + UI_TITLE_HEIGHT;
		txt->base.width = ui_thing_attrs.base.width;
		txt->base.height = UI_ATTR_HEIGHT;

		txt->base.color[1] = 0x20FFFFFF;

		txt->color[0] = 0xFFCCCCCC;
		txt->color[1] = 0xFFFFFFFF;

		txt->gltex = *tmptex++;

		//
		i++;

		// name
		txt = ptr;
		ptr += sizeof(glui_text_t);

		cont->elements[i] = (glui_element_t*)txt;

		txt->base.draw = glui_df_text;

		txt->base.y = ii * UI_ATTR_HEIGHT + UI_TITLE_HEIGHT;
		txt->base.height = UI_ATTR_HEIGHT;

		txt->color[0] = 0xFFDDDDDD;
		txt->color[1] = 0xFFDDDDDD;

		txt->gltex = *tmptex++;

		glui_set_text(txt, thing_attr[ii].name, glui_font_small_kfn, GLUI_ALIGN_BOT_LEFT);
		txt->x += 8;
	}

	// attributes title
	txt = ptr;
	ptr += sizeof(glui_text_t);

	cont->elements[NUM_SHOW_ATTRS * 2] = (glui_element_t*)txt;

	txt->base.draw = glui_df_text;
	txt->base.width = ui_thing_attrs.base.width;
	txt->base.height = UI_TITLE_HEIGHT;
	txt->color[0] = 0xFFFFFFFF;
	txt->color[1] = 0xFFFFFFFF;
	txt->gltex = *tmptex++;
	glui_set_text(txt, "Attributes", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	// flags
	cont = ptr;
	ui_thing_flags.elements[0] = ptr;
	ptr += sizeof(glui_container_t) + (NUM_SHOW_FLAGS * 2 + 1) * sizeof(glui_text_t*);
	cont->base.draw = glui_df_container;
	cont->count = (NUM_SHOW_FLAGS * 2 + 1);

	text_flag = ptr;

	for(uint32_t i = 0; i < NUM_SHOW_FLAGS * 2; i++)
	{
		uint32_t ii = i / 2;

		// value
		txt = ptr;
		ptr += sizeof(glui_text_t);

		cont->elements[i] = (glui_element_t*)txt;

		txt->base.draw = glui_df_text;
		txt->base.click = uin_thing_flag;
		txt->base.custom = ii;

		txt->base.y = ii * UI_ATTR_HEIGHT + UI_TITLE_HEIGHT;
		txt->base.width = ui_thing_flags.base.width;
		txt->base.height = UI_ATTR_HEIGHT;

		txt->base.color[1] = 0x20FFFFFF;

		txt->color[0] = 0xFFCCCCCC;
		txt->color[1] = 0xFFFFFFFF;

		txt->gltex = *tmptex++;

		//
		i++;

		// name
		txt = ptr;
		ptr += sizeof(glui_text_t);

		cont->elements[i] = (glui_element_t*)txt;

		txt->base.draw = glui_df_text;

		txt->base.y = ii * UI_ATTR_HEIGHT + UI_TITLE_HEIGHT;
		txt->base.height = UI_ATTR_HEIGHT;

		txt->color[0] = 0xFFDDDDDD;
		txt->color[1] = 0xFFDDDDDD;

		txt->gltex = *tmptex++;

		glui_set_text(txt, thing_flag[ii].name, glui_font_small_kfn, GLUI_ALIGN_BOT_LEFT);
		txt->x += 8;
	}

	// flags title
	txt = ptr;
	ptr += sizeof(glui_text_t);

	cont->elements[NUM_SHOW_FLAGS * 2] = (glui_element_t*)txt;

	txt->base.draw = glui_df_text;
	txt->base.width = ui_thing_flags.base.width;
	txt->base.height = UI_TITLE_HEIGHT;
	txt->color[0] = 0xFFFFFFFF;
	txt->color[1] = 0xFFFFFFFF;
	txt->gltex = *tmptex++;
	glui_set_text(txt, "Flags", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	// animations
	cont = ptr;
	ui_thing_anims.elements[0] = ptr;
	ptr += sizeof(glui_container_t) + (NUM_SHOW_ANIMS * 2 + 1) * sizeof(glui_text_t*);
	cont->base.draw = glui_df_container;
	cont->count = (NUM_SHOW_ANIMS * 2 + 1);

	text_animation = ptr;

	for(uint32_t i = 0; i < NUM_SHOW_ANIMS * 2; i++)
	{
		uint32_t ii = i / 2;

		// value
		txt = ptr;
		ptr += sizeof(glui_text_t);

		cont->elements[i] = (glui_element_t*)txt;

		txt->base.draw = glui_df_text;
		txt->base.click = uin_thing_anim;
		txt->base.custom = ii;

		txt->base.y = ii * UI_ATTR_HEIGHT + UI_TITLE_HEIGHT;
		txt->base.width = ui_thing_anims.base.width;
		txt->base.height = UI_ATTR_HEIGHT;

		txt->base.color[1] = 0x20FFFFFF;

		txt->color[0] = 0xFFCCCCCC;
		txt->color[1] = 0xFFFFFFFF;

		txt->gltex = *tmptex++;

		//
		i++;

		// name
		txt = ptr;
		ptr += sizeof(glui_text_t);

		cont->elements[i] = (glui_element_t*)txt;

		txt->base.draw = glui_df_text;

		txt->base.y = ii * UI_ATTR_HEIGHT + UI_TITLE_HEIGHT;
		txt->base.height = UI_ATTR_HEIGHT;

		txt->color[0] = 0xFFDDDDDD;
		txt->color[1] = 0xFFDDDDDD;

		txt->gltex = *tmptex++;
	}

	// animations title
	txt = ptr;
	ptr += sizeof(glui_text_t);

	cont->elements[NUM_SHOW_ANIMS * 2] = (glui_element_t*)txt;

	txt->base.draw = glui_df_text;
	txt->base.width = ui_thing_anims.base.width;
	txt->base.height = UI_TITLE_HEIGHT;
	txt->color[0] = 0xFFFFFFFF;
	txt->color[1] = 0xFFFFFFFF;
	txt->gltex = *tmptex++;
	glui_set_text(txt, "Animations", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	// states
	cont = ptr;
	ui_thing_states.elements[0] = ptr;
	ptr += sizeof(glui_container_t) + NUM_STATE_COLS * NUM_STATE_ROWS * sizeof(glui_text_t*);
	cont->base.draw = glui_df_container;
	cont->count = NUM_STATE_COLS * NUM_STATE_ROWS;

	text_states = ptr;

	tcnt = 0;
	for(uint32_t i = 0; i < NUM_STATE_ROWS; i++)
	{
		for(uint32_t j = 0; j < NUM_STATE_COLS; j++)
		{
			glui_text_t *tt = &ui_thing_state_table.elements[j]->text;

			txt = ptr;
			ptr += sizeof(glui_text_t);

			cont->elements[tcnt++] = (glui_element_t*)txt;

			txt->base.draw = glui_df_text;
			txt->base.custom = i;

			txt->base.x = tt->base.x;
			txt->base.y = i * UI_STATE_HEIGHT;
			txt->base.width = tt->base.width;
			txt->base.height = UI_STATE_HEIGHT;

			txt->base.color[1] = 0x11000000;

			txt->color[0] = 0xFF000000;
			txt->color[1] = 0xFF000000;

			txt->gltex = *tmptex++;
		}
	}

	//

	free(ttex);

	ui_thing_attrs.base.height = NUM_SHOW_ATTRS * UI_ATTR_HEIGHT + (UI_ATTR_HEIGHT / 4) + UI_TITLE_HEIGHT;

	ui_thing_flags.base.y = ui_thing_attrs.base.y + ui_thing_attrs.base.height + UI_SPACE_HEIGHT;
	ui_thing_flags.base.height = NUM_SHOW_FLAGS * UI_ATTR_HEIGHT + (UI_ATTR_HEIGHT / 4) + UI_TITLE_HEIGHT;

	ui_thing_anims.base.y = ui_thing_flags.base.y + ui_thing_flags.base.height + UI_SPACE_HEIGHT;
	ui_thing_anims.base.height = NUM_SHOW_ANIMS * UI_ATTR_HEIGHT + (UI_ATTR_HEIGHT / 4) + UI_TITLE_HEIGHT;

	// load default
	x16t_load(NULL);

	// generate things
	x16t_generate();

	return 0;
}

void x16t_mode_set()
{
	system_mouse_grab(0);
	system_draw = edit_ui_draw;
	system_input = (void*)edit_ui_input;
	system_tick = x16g_tick;

	shader_buffer.colormap = COLORMAP_IDX(0);
	shader_buffer.lightmap = LIGHTMAP_IDX(0);

	x16t_update_thing_view(1);
}

void x16t_thing_select(uint32_t type)
{
	if(spawn_dst)
		*spawn_dst = type;
	else
		show_thing = type;
	x16t_update_thing_view(!spawn_dst);
}

void x16t_generate()
{
	link_entry_t *ent;

	edit_busy_window("Generating editor things ...");

	// generate textures from spawn state
	for(uint32_t i = 0; i < MAX_X16_THING_TYPES; i++)
	{
		thing_def_t *ti = thing_info + i;
		thing_anim_t *ta = ti->anim + ANIM_SPAWN;
		thing_sprite_t *spr = x16_thing_sprite + i;
		thing_st_t *st;
		int32_t idx;
		uint8_t frm;

		spr->width = 0;

		if(!ti->name.hash)
			continue;

		if(ta->count < 0)
			ta = thing_info[ta->link.thing].anim + (ta->link.anim & 0x7F);

		if(ta->count <= 0)
			continue;

		idx = x16g_spritelink_by_hash(ta->state[0].sprite);
		if(idx < 0)
			continue;

		st = ta->state + 0;

		frm = st->frame & 31;

		if(!(editor_sprlink[idx].vmsk & (1 << frm)))
			continue;

		glBindTexture(GL_TEXTURE_2D, x16_thing_texture[i]);
		if(x16g_generate_state_texture(idx, frm, 0))
			continue;

		spr->bright = st->frame & 0x80;

		spr->width = x16g_state_res[0];
		spr->height = x16g_state_res[1];
		spr->stride = x16g_state_res[2];

		spr->ox = x16g_state_offs[0];
		spr->oy = x16g_state_offs[1];

		spr->data = x16g_state_data_ptr;
		spr->offs = x16g_state_offs_ptr;
	}

	// update things
	x16t_update_map();
}

void x16t_export()
{
	uint8_t crouch_height = thing_info[THING_TYPE_PLAYER_C].info.height;
	uint8_t swim_height = thing_info[THING_TYPE_PLAYER_S].info.height;
	uint8_t fly_height = thing_info[THING_TYPE_PLAYER_F].info.height;
	uint8_t st_next[MAX_X16_STATES];

	// reset
	memset(state_data, 0, sizeof(state_data));

	// store extra info into dummy state zero
	state_data->num_sprlnk = x16_num_sprlnk_thg;
	state_data->menu_logo = 0xFF;
	state_data->plr_crouch = THING_TYPE_PLAYER_C;
	state_data->plr_swim = THING_TYPE_PLAYER_S;
	state_data->plr_fly = THING_TYPE_PLAYER_F;

	// checks

	if(thing_info[THING_TYPE_PLAYER_N].anim[ANIM_SPAWN].count == 0)
	{
		edit_status_printf("Player has no spawn animation! Unable to export!");
		return;
	}

	if(thing_info[THING_TYPE_PLAYER_C].anim[ANIM_SPAWN].count == 0)
	{
		edit_status_printf("Player crouching is disabled!");
		thing_info[THING_TYPE_PLAYER_C].info.height = 0;
		state_data->plr_crouch = THING_TYPE_PLAYER_N;
	}

	if(thing_info[THING_TYPE_PLAYER_S].anim[ANIM_SPAWN].count == 0)
	{
		edit_status_printf("Player swimming is not used!");
		thing_info[THING_TYPE_PLAYER_S].info.height = 0;
		state_data->plr_swim = THING_TYPE_PLAYER_N;
	}

	if(thing_info[THING_TYPE_PLAYER_F].anim[ANIM_SPAWN].count == 0)
	{
		edit_status_printf("Player flying is not used!");
		thing_info[THING_TYPE_PLAYER_F].info.height = 0;
		state_data->plr_fly = THING_TYPE_PLAYER_N;
	}

	edit_busy_window("Exporting things ...");

	state_idx = 1; // state 0 is 'STOP'

	memset(thing_data, 0, sizeof(thing_data));

	// sprite names
	for(uint32_t i = 0; i < x16_num_sprlnk; i++)
	{
		uint8_t *tdst = thing_data + i + 128 + 24 * 256; // 24 = offset for sprite names, stored in 'thing animation info' area; 0xB880
		uint32_t hash = editor_sprlink[i].hash;

		if(	i >= x16_num_sprlnk_thg &&
			hash == 0xF8845BD5
		)
			// logo sprite
			state_data->menu_logo = i;

		*tdst = hash;
		tdst += 256;
		*tdst = hash >> 8;
		tdst += 256;
		*tdst = hash >> 16;
		tdst += 256;
		*tdst = hash >> 24;
	}

	// things and animations
	for(uint32_t i = 0; i < MAX_X16_THING_TYPES; i++)
	{
		thing_def_t *ti = thing_info + i;
		export_type_t info = ti->info;
		uint8_t *thsh = thing_data + i + 128 + 28 * 256; // 28 = offset for thing hash, stored in 'thing animation info' area; 0xBC80
		uint8_t *tdst = thing_data + i; // thing attributes; 0xA000
		uint8_t *tan0 = thing_data + i + 128; // thing animations LO; 0xA080
		uint8_t *tan1 = thing_data + i + 128 + NUM_THING_ANIMS * 256; // thing animations HI; 0xA880
		uint8_t *tan2 = thing_data + i + 128 + NUM_THING_ANIMS * 256 * 2; // thing animation sizes; 0xB080
		uint8_t *tsrc = (uint8_t*)&info;
		uint32_t base_state;
		uint32_t base_bright;

		thsh[0 * 256] = ti->name.hash;
		thsh[1 * 256] = ti->name.hash >> 8;
		thsh[2 * 256] = ti->name.hash >> 16;
		thsh[3 * 256] = ti->name.hash >> 24;

		for(uint32_t j = 0; j < NUM_THING_ANIMS; j++)
		{
			thing_anim_t *ta = ti->anim + j;

			if(ta->count <= 0)
			{
				ta->index = 0;
				tan0[j * 256] = 0;
				tan1[j * 256] = 0;
				tan2[j * 256] = 0;
				continue;
			}

			base_state = state_idx;
			base_bright = ta->state[0].frame & 0x80;

			for(uint32_t k = 0; k < ta->count; k++)
			{
				uint32_t next, frame;
				export_state_t *dst;
				thing_st_t *st;
				int32_t sprite;

				if(state_idx >= MAX_X16_STATES)
				{
					edit_status_printf("Too many states! Unable to export!");
					goto restore;
				}

				st = ta->state + k;
				dst = state_data + state_idx;

				frame = st->frame & 0x1F;

				sprite = x16g_spritelink_by_hash(st->sprite);
				if(sprite < 0)
					sprite = 0xFF;

				st_next[state_idx] = st->next;
				if(st->next & 0x80)
				{
					dst->next = i;
					dst->frm_nxt = frame;
				} else
				{
					if(st->next < ta->count)
						next = base_state + st->next;
					else
						next = 0;

					dst->next = next;
					dst->frm_nxt = ((next >> 3) & 0xE0) | frame;
				}

				dst->sprite = sprite;
				dst->ticks = st->ticks;
				dst->action = st->action | (st->frame & 0x80);
				dst->arg[0] = st->arg[0];
				dst->arg[1] = st->arg[1];
				dst->arg[2] = st->arg[2];

				fix_action_args(dst);

				state_idx++;
			}

			ta->index = base_state;
			tan0[j * 256] = base_state;
			tan1[j * 256] = (base_state >> 8) | base_bright;
			tan2[j * 256] = ta->count;
		}

		if(!info.view_height)
			info.view_height = ti->info.height * 3 / 4; // this is also used in editor.c

		if(!info.atk_height)
			info.atk_height = ti->info.height * 8 / 12;

		if(!info.water_height)
			info.water_height = ti->info.height * 5 / 8;

		if(!info.alt_radius)
			info.alt_radius = info.radius;

		for(uint32_t j = 0; j < sizeof(export_type_t); j++)
			tdst[j * 256] = tsrc[j];
	}

	// fix animation jumps
	for(uint32_t i = 0; i < state_idx; i++)
	{
		export_state_t *dst = state_data + i;
		uint8_t in = st_next[i];

		fix_action_anim(dst);

		if(in & 0x80)
		{
			thing_anim_t *ta;
			uint32_t next;

			in &= 7;

			ta = thing_info[dst->next].anim + in;
			next = ta->index;

			dst->next = next;
			dst->frm_nxt |= ((next >> 3) & 0xE0);
		}
	}

	// fix animation links
	for(uint32_t i = 0; i < MAX_X16_THING_TYPES; i++)
	{
		thing_def_t *ti = thing_info + i;
		uint8_t *tan0 = thing_data + i + 128; // thing animations LO; 0xA080
		uint8_t *tan1 = thing_data + i + 128 + NUM_THING_ANIMS * 256; // thing animations HI; 0xA880
		uint8_t *tan2 = thing_data + i + 128 + NUM_THING_ANIMS * 256 * 2; // thing animation sizes; 0xB080

		for(uint32_t j = 0; j < NUM_THING_ANIMS; j++)
		{
			thing_anim_t *ta = ti->anim + j;
			uint32_t state, bright;

			if(ta->count >= 0)
				continue;

			ta = thing_info[ta->link.thing].anim + (ta->link.anim & 0x7F);

			state = ta->index;
			bright = ta->state[0].frame & 0x80;

			tan0[j * 256] = state;
			tan1[j * 256] = (state >> 8) | bright;
			tan2[j * 256] = ta->count;
		}
	}

	// convert states
	for(uint32_t i = 0; i < MAX_X16_STATES / 256; i++)
		for(uint32_t x = 0; x < sizeof(export_state_t); x++)
			for(uint32_t y = 0; y < 256; y++)
				thing_data[8192 + y + x * 256 + i * 2048] = state_data[i * 256 + y].raw[x];

	// save
	edit_save_file(X16_PATH_EXPORT PATH_SPLIT_STR "TABLES4.BIN", thing_data, sizeof(thing_data));

	// done
	edit_status_printf("Things exported, %u states.", state_idx);

	x16t_update_thing_view(0);

restore:
	thing_info[THING_TYPE_PLAYER_C].info.height = crouch_height;
	thing_info[THING_TYPE_PLAYER_S].info.height = swim_height;
	thing_info[THING_TYPE_PLAYER_F].info.height = fly_height;
}

void x16t_update_map()
{
	link_entry_t *ent;

	ent = tick_list_normal.top;
	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			kge_thing_t *th = (kge_thing_t*)tickable->data;

			if(th->prop.name[0])
			{
				uint32_t type = type_by_name(th->prop.name);

				if(type < MAX_X16_THING_TYPES)
					th->prop.type = type;
				else
					th->prop.type = edit_get_special_thing(th->prop.name);
			}

			edit_update_thing_type(th);
		}

		ent = ent->next;
	}
}

