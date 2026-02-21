
typedef struct
{
	int16_t x, y, z;
	uint8_t origin;
	uint8_t sector;
	uint8_t angle;
	uint8_t range;
	uint8_t type;
	uint8_t axis;
	uint8_t link;
	uint8_t radius;
	uint8_t height;
	uint8_t blockedby;
	uint8_t is_sobj;
	uint8_t ptop, pbot, tsec;
	uint16_t damage;
	int16_t sin;
	int16_t cos;
	int16_t idiv;
	int16_t wtan;
	int16_t ptan;
	//
	int16_t dist;
	int16_t ztmp;
	//
	uint8_t thing_pick;
	uint8_t thing_sdx;
	int16_t thing_zz;
} hitscan_t;

//

extern hitscan_t hitscan;

//

void hitscan_func(uint8_t tdx, uint8_t hang, uint32_t (*cb)(wall_t*));
void hitscan_attack(uint8_t tdx, uint8_t zadd, uint8_t hang, uint8_t halfpitch, uint8_t type, uint8_t range);

void hitscan_sight_check(uint8_t tdx, uint8_t odx);
void hitscan_sight_ex(uint8_t tdx, uint8_t odx, uint8_t ang, int32_t dist);

void hitscan_angles(uint8_t hang, uint8_t halfpitch);
int32_t hitscan_wall_pos(wall_t *wall, vertex_t *d0);
int32_t hitscan_wall_hitz(int32_t dist);
int32_t hitscan_thing_dd(thing_t *th, vertex_t *d1);
int32_t hitscan_thing_dt(thing_t *th, vertex_t *d1);
uint32_t hitscan_sobj_sort(sector_t *sec, map_secobj_t **sobjlist, int32_t x, int32_t y);
