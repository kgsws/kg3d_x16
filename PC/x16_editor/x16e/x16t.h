
#define MAX_X16_STATES	1024	// limited by design
#define MAX_X16_THING_TYPES	128	// limited by design; > 127 means "special or no thing"

#define THING_TYPE_PLAYER_N	127	// normal
#define THING_TYPE_PLAYER_C	126	// crouching
#define THING_TYPE_PLAYER_S	125	// swimming
#define THING_TYPE_PLAYER_F	124	// flying
#define X16_NUM_UNLISTED_THINGS	4	// all player types

#define THING_MAX_SPAWN_TYPES	4	// also update 'spawn' field in 'default_player_info' and 'default_thing_info'

#define THING_WEAPON_COUNT	12
#define THING_WEAPON_FIRST	112

#define MAX_STATES_IN_ANIMATION	32

#define LEN_X16_THING_NAME	16

#define THING_EFLAG_SLIDING	0x80
#define THING_EFLAG_CLIMBABLE	0x40
#define THING_EFLAG_NOCLIP	0x08
#define THING_EFLAG_PROJECTILE	0x04
#define THING_EFLAG_WATERSPEC	0x02
#define THING_EFLAG_PUSHABLE	0x01

#define THING_CHECK_WEAPON_SPRITE(a, t)	((a) != ANIM_SPAWN && (t) >= THING_WEAPON_FIRST && (t) < THING_WEAPON_FIRST + THING_WEAPON_COUNT)

enum
{
	ANIM_SPAWN,
	ANIM_PAIN,
	ANIM_DEATH,
	ANIM_MOVE,
	ANIM_FIRE,
	ANIM_MELEE,
	ANIM_ACTIVE,
	ANIM_INACTIVE,
	//
	NUM_THING_ANIMS // 8 is engine limit
};

typedef struct
{
	uint8_t spawn[THING_MAX_SPAWN_TYPES];
	//
	uint8_t radius;
	uint8_t height;
	uint8_t blocking;
	uint8_t blockedby;
	uint8_t mass;
	uint8_t gravity;
	uint8_t speed;
	uint8_t eflags;
	uint16_t health;
	uint8_t scale;
	uint8_t step_height;
	uint8_t water_height;
	uint8_t view_height;
	uint8_t atk_height;
	uint8_t alt_radius;
	//
	uint8_t jump_pwr;
} __attribute((packed)) export_type_t;

typedef struct thing_st_s
{
	uint32_t sprite;
	uint8_t arg[3];
	uint8_t next;
	uint8_t frame;
	uint8_t ticks;
	uint8_t action;
} thing_st_t;

typedef struct
{
	uint16_t gen_idx;
	union
	{
		struct
		{
			uint8_t thing;
			uint8_t anim;
		} link;
		int16_t count;
	};
	uint32_t index;
	union
	{
		thing_st_t state[MAX_STATES_IN_ANIMATION];
		uint8_t load_link[LEN_X16_THING_NAME + 16];
	};
} thing_anim_t;

typedef struct
{
	uint8_t text[LEN_X16_THING_NAME];
	uint32_t hash;
} thing_name_t;

typedef struct
{
	thing_name_t name;
	export_type_t info;
	thing_anim_t anim[NUM_THING_ANIMS];
} thing_def_t;

typedef struct
{
	uint32_t width, height, stride, bright;
	int32_t ox, oy;
	uint16_t *data;
	uint32_t *offs;
} thing_sprite_t;

typedef struct
{
	const char *name;
	uint8_t type;
	uint8_t def;
	int16_t lim[2];
} state_arg_def_t;

typedef struct
{
	const char *name;
	uint32_t flags;
	uint32_t (*validate)(thing_st_t*);
	state_arg_def_t arg[3];
} state_action_def_t;

//

extern uint32_t x16_thing_texture[MAX_X16_THING_TYPES];
extern thing_sprite_t x16_thing_sprite[MAX_X16_THING_TYPES];

extern thing_def_t thing_info[MAX_X16_THING_TYPES + 1];

extern const state_action_def_t state_action_def[];

//

uint32_t x16t_init();

void x16t_mode_set();

void x16t_thing_select(uint32_t);

void x16t_new_thg();

void x16t_load(const uint8_t *file);
const uint8_t *x16t_save(const uint8_t *file);

void x16t_generate();
void x16t_export();

void x16t_update_map();

//

void x16t_update_thing_view(uint32_t);
