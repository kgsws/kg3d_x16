
#define IS_THING_TICKER(x)	((void*)(x)->info.tick == (void*)thing_ticker)

// internal thing types
#define THING_TYPE_UNKNOWN	0x010000

enum
{
	THING_TYPE_START_NORMAL	= 0x020000,
	THING_TYPE_START_COOP,
	THING_TYPE_START_DM,
};

#define THING_TYPE_MARKER	0x030000

#define THING_TYPE_EDIT_CAMERA	0xFFFFFFFF

//

extern kge_thing_t *thing_local_player;
extern kge_thing_t *thing_local_camera;

extern uint32_t vc_move;

//

uint32_t thing_ticker(kge_thing_t *thing);

kge_thing_t *thing_spawn(float x, float y, float z, kge_sector_t *sec);
void thing_remove(kge_thing_t*);

void thing_update_sector(kge_thing_t *thing, uint32_t forced);

void thing_thrust(kge_thing_t *th, float speed, uint32_t angle, int32_t vngle);
