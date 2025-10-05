
#define MAP_VERSION	19
#define MAP_MAGIC	0x36315870614D676B
#define MAX_LIGHTS	8
#define MAX_X16_VARIANTS	16
#define MAX_TEXTURES	128
#define MAX_PLAYER_STARTS	32

#define WALL_FLAG_LAST	0x8000	// forced by code
#define WALL_FLAG_SWAP	0x4000
#define WALL_TYPE_MASK	0x3000

#define WALL_TYPE_SOLID	0x0000
#define WALL_TYPE_PORTAL	0x1000
#define WALL_TYPE_SPLIT	0x2000
#define WALL_TYPE_MASKED	0x3000

#define SECTOR_FLAG_WATER	0x80

typedef struct
{
	int32_t x, y, z;
	uint8_t viewheight;
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
} __attribute__((packed)) vertex_t;

typedef struct
{
	uint8_t texture;
	uint8_t flags;
	uint8_t ox;
	uint8_t oy;
} __attribute__((packed)) tex_info_t;

typedef struct
{
	uint16_t angle;
	uint8_t backsector;
	uint8_t special;
	vertex_t vtx;
	vertex_t dist;
	tex_info_t top;
} __attribute__((packed)) wall_solid_t;

typedef struct
{
	uint16_t angle;
	uint8_t backsector;
	uint8_t special;
	vertex_t vtx;
	vertex_t dist;
	tex_info_t top;
	tex_info_t bot;
	uint8_t blocking;
} __attribute__((packed)) wall_portal_t;

typedef struct
{
	uint16_t angle;
	uint8_t backsector;
	uint8_t special;
	vertex_t vtx;
	vertex_t dist;
	tex_info_t top;
	tex_info_t bot;
	uint16_t height;
} __attribute__((packed)) wall_split_t;

typedef struct
{
	uint16_t angle;
	uint8_t backsector;
	uint8_t special;
	vertex_t vtx;
	vertex_t dist;
	tex_info_t top;
	tex_info_t bot;
	uint8_t blocking;
	uint8_t blockmid;
	tex_info_t mid;
} __attribute__((packed)) wall_masked_t;

typedef union
{
	wall_solid_t solid;
	wall_portal_t portal;
	wall_split_t split;
	wall_masked_t masked;
} __attribute__((packed)) wall_combo_t;

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
	sector_plane_t floor;
	sector_plane_t ceiling;
	uint16_t walls;
	uint8_t flags;
	int8_t floordist;
	uint8_t floormasked;
} __attribute__((packed)) sector_t;

typedef struct
{
	uint8_t maskblock;
} sector_extra_t;

typedef struct
{
	int16_t x, y, z;
	uint8_t sector;
	uint8_t angle;
	uint8_t pitch;
	uint8_t flags;
} player_start_t;

//

extern player_start_t player_starts[MAX_PLAYER_STARTS * 3];

extern uint32_t frame_counter;

extern uint32_t level_tick;

extern vertex_t p2a_coord;

extern projection_t projection;

extern sector_t map_sectors[256];
extern sector_extra_t map_secext[256];

extern uint8_t sectorth[256][32]; // list of things which are in specific sector, last slot is a counter

extern int16_t tab_sin[256];
extern int16_t tab_cos[256];

extern int16_t *const inv_div;

extern int16_t tab_tan_hs[128];

extern uint8_t map_data[32 * 1024];

extern vertex_t display_point;
extern vertex_t display_line[2];
extern vertex_t display_wall[2];

extern const uint32_t wall_size_tab[];

//

int32_t load_file(const char *name, void *data, uint32_t size);

uint16_t point_to_angle();

uint8_t rng_get();

