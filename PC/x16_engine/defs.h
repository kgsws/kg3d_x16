
#define MAP_VERSION	27
#define MAP_MAGIC	0x36315870614D676B
#define MAX_LIGHTS	8
#define MAX_X16_VARIANTS	16
#define MAX_TEXTURES	128
#define MAX_PLAYER_STARTS	256
#define MAX_SOBJ	8

#define WALL_BANK_COUNT	16
#define WALL_BANK_SIZE	4096

#define WALL_MARK_SWAP	0x8000
#define WALL_MARK_EXTENDED	0x4000
#define WALL_MARK_XORIGIN	0x2000
#define WALL_MARK_SKIP	0x1000

#define SECTOR_FLAG_WATER	0x80

typedef struct
{
	uint16_t addr;
	int16_t x, y;
	uint8_t ia, ib;
} x16_sprite_t;

typedef struct
{
	uint64_t magic;
	uint8_t version;
	uint8_t flags;
	//
	uint8_t count_lights;
	uint8_t count_ptex;
	uint8_t count_wtex;
	uint8_t count_textures;
	uint8_t count_starts_normal;
	uint8_t count_starts_coop;
	uint8_t count_starts_dm;
	uint8_t count_wall_banks;
	uint8_t count_extra_storage;
	uint8_t count_things;
	//
	uint8_t unused[8];
	//
	uint32_t hash_sky;
} map_head_t;

typedef struct
{
	int32_t x, y, z;
	uint8_t wh, wd;
	uint8_t sector;
	int16_t sin, cos;
	uint16_t a;
	uint8_t a8;
	uint8_t ycw, ycp;
	uint8_t x0, x1;
	uint8_t x0d, x1d;
	uint8_t ox, oy, wx, fix;
	uint8_t ta, tx;
	int32_t pl_x, pl_y;
	int16_t pl_sin, pl_cos;
	uint8_t *wtex;
	uint16_t *cols;
} projection_t;

typedef struct
{
	int16_t x, y;
	uint8_t a;
} p2a_t;

typedef struct
{
	int16_t x, y;
} __attribute__((packed)) vertex_t;

typedef struct
{
	uint8_t texture;
	uint8_t ox;
	uint8_t oy;
} __attribute__((packed)) tex_info_t;

typedef struct
{
	vertex_t vtx; // $00
	vertex_t dist; // $04
	uint16_t angle; // $08
	uint8_t next; // $0A
	uint8_t backsector; // $0B
	uint8_t tflags; // $0C
	tex_info_t top; // $0D
	tex_info_t bot; // $10
	tex_info_t mid; // $13
	uint8_t blocking; // $16
	uint8_t blockmid; // $17
	int16_t split; // $18
	uint8_t special; // $1A
	uint8_t padding[5]; // $1B
} __attribute__((packed)) wall_t;

typedef struct
{
	int16_t height;
	uint8_t texture;
	uint8_t ox, oy;
	uint8_t sx, sy;
	uint8_t angle;
	uint8_t link;
} __attribute__((packed)) sector_plane_t;

typedef struct
{
	// 32 bytes total
	sector_plane_t floor;
	sector_plane_t ceiling;
	struct
	{
		uint8_t bank;
		uint8_t first;
	} wall;
	uint8_t flags; // light, palette, underwater
	uint8_t midheight;
	uint16_t sobj_lo;
	uint8_t sobj_hi;
	//
	uint8_t filler[7];
} __attribute__((packed)) sector_t;

typedef struct
{
	uint8_t bank;
	uint8_t first;
	int16_t x, y;
} map_secobj_t;

typedef struct
{
	int16_t x, y, z;
	uint8_t sector;
	uint8_t angle;
	uint8_t pitch;
	uint8_t flags;
} player_start_t;

//

extern player_start_t player_starts[MAX_PLAYER_STARTS];

extern uint32_t frame_counter;

extern uint32_t level_tick;

extern p2a_t p2a_coord;

extern projection_t projection;

extern uint8_t wram[0x200000];
extern uint8_t vram[128 * 1024];

extern sector_t map_sectors[256];
extern wall_t map_walls[WALL_BANK_COUNT][256];

extern uint8_t sectorth[256][32]; // list of things which are in specific sector, last slot is a counter

extern int16_t tab_sin[256];
extern int16_t tab_cos[256];

extern int16_t *const inv_div;

extern int16_t tab_tan_hs[128];

extern vertex_t display_point;
extern vertex_t display_line[2];
extern vertex_t display_wall[2];

extern const uint32_t wall_size_tab[];

//

int32_t load_file(const char *name, void *data, uint32_t size);

uint16_t point_to_angle();
uint16_t point_to_dist();

uint8_t rng_get();
uint8_t rng_val(uint8_t);

