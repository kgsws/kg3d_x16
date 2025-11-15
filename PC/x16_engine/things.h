
#define MAX_X16_STATES	2048
#define MAX_X16_THING_TYPES	128
#define THING_MAX_SPAWN_TYPES	4

#define TCMD_MOVING	0x80
#define TCMD_GO_UP	0x40
#define TCMD_USE	0x20
#define TCMD_ALT	0x04
#define TCMD_ATK	0x02
#define TCMD_GO_DOWN	0x01

#define THING_IFLAG_NOJUMP	0x01
#define THING_IFLAG_GOTHIT	0x02
#define THING_IFLAG_CORPSE	0x04
#define THING_IFLAG_USED	0x20
#define THING_IFLAG_JUMPED	0x40
#define THING_IFLAG_HEIGHTCHECK	0x80

#define THING_EFLAG_PROJECTILE	0x80
#define THING_EFLAG_CLIMBABLE	0x40
#define THING_EFLAG_SPRCLIP	0x20
#define THING_EFLAG_NORADIUS	0x10
#define THING_EFLAG_WATERSPEC	0x04
#define THING_EFLAG_NOPUSH	0x02
#define THING_EFLAG_PUSHABLE	0x01

#define THING_TYPE_PLAYER_N	127	// normal
#define THING_TYPE_PLAYER_C	126	// crouching
#define THING_TYPE_PLAYER_S	125	// swimming
#define THING_TYPE_PLAYER_F	124	// flying

#define THING_WEAPON_COUNT	12
#define THING_WEAPON_FIRST	112

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
	NUM_THING_ANIMS,
	// weapon aliases
	ANIM_READY = ANIM_MOVE,
	ANIM_ATK = ANIM_FIRE,
	ANIM_ALT = ANIM_MELEE,
	ANIM_RAISE = ANIM_ACTIVE,
	ANIM_LOWER = ANIM_INACTIVE,
};

typedef struct
{
	uint8_t spawn[THING_MAX_SPAWN_TYPES];
	//
	uint8_t radius;
	uint8_t height;
	uint8_t blocking;
	uint8_t death_bling;
	uint8_t blockedby;
	uint8_t death_blby;
	uint8_t imass;
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
	uint8_t pain_chance;
	//
	uint8_t jump_pwr;
} __attribute((packed)) thing_type_t;

typedef struct
{
	ticker_base_t ticker;
	//
	int32_t x, y, z; // 24 bits
	int16_t mx, my, mz;
	uint8_t view_height;
	//
	int16_t floorz, ceilingz;
	uint8_t floors, ceilings;
	//
	uint8_t angle;
	uint8_t pitch;
	//
	uint8_t iflags;
	uint8_t eflags;
	//
	uint8_t radius;
	uint8_t height;
	uint8_t gravity;
	//
	uint16_t health;
	//
	uint8_t scale;
	uint8_t counter;
	//
	uint8_t blocking;
	uint8_t blockedby;
	//
	uint8_t origin;
	uint8_t target;
	//
	uint8_t sprite;
	uint8_t ticks;
	uint16_t next_state;
} thing_t;

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
} thing_state_t;

typedef struct
{
	uint32_t state;
	uint32_t count;
} thing_anim_t;

typedef struct
{
	uint8_t bits_l;
	uint8_t bits_h;
	uint8_t angle;
	uint8_t pitch;
} ticcmd_t;

//

extern thing_type_t thing_type[MAX_X16_THING_TYPES];
extern thing_anim_t thing_anim[MAX_X16_THING_TYPES][NUM_THING_ANIMS];
extern uint32_t thing_hash[MAX_X16_THING_TYPES];
extern uint32_t sprite_hash[128];
extern uint8_t sprite_remap[128];
extern thing_state_t thing_state[MAX_X16_STATES];
extern uint32_t num_sprlnk_thg;
extern uint32_t logo_spr_idx;

extern uint8_t thingsec[128][16]; // list of sectors which specific thing is in, slot 0 is main sector
extern uint8_t thingces[128][16]; // slot (index) in sector this specific thing is at for sector given by 'thingsec'

extern ticcmd_t ticcmd;

// player
extern uint8_t player_thing;

// camera
extern uint8_t camera_thing;
extern uint8_t camera_damage;

//

uint32_t thing_init(const char *file);

int32_t thing_find_type(uint32_t hash);

void thing_clear();
void thing_tick();
void thing_tick_plr();

uint8_t thing_spawn(int32_t x, int32_t y, int32_t z, uint8_t sector, uint8_t type, uint8_t origin);
void thing_spawn_player();
void thing_remove(uint8_t);

void thing_launch(uint8_t tdx, uint8_t speed);
void thing_launch_ang(uint8_t tdx, uint8_t ang, uint8_t speed);

void thing_damage(uint8_t tdx, uint8_t odx, uint8_t angle, uint16_t damage);

uint32_t thing_check_pos(uint8_t tdx, int16_t nx, int16_t ny, int16_t nz, uint8_t sdx);
void thing_apply_pos();

//

static inline thing_t *thing_ptr(uint32_t idx)
{
	return (thing_t*)&ticker[idx];
}
