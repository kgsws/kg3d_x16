
#define MAP_VERSION	17
#define MAP_MAGIC	0x36315870614D676B
#define MAX_LIGHTS	8
#define MAX_REMAPS	4
#define MAX_X16_VARIANTS	16
#define MAX_TEXTURES	128
#define MAX_PLAYER_STARTS	32

#define MARK_SPLIT	0x1000
#define MARK_PORTAL	0x2000
#define MARK_SCROLL	0x4000
#define MARK_LAST	0x8000
#define MARK_TYPE_BITS	(MARK_SCROLL | MARK_PORTAL | MARK_SPLIT)
#define MARK_MID_BITS	(MARK_PORTAL | MARK_SPLIT)
#define MARK_MASKED	MARK_MID_BITS

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
	int8_t x, y;
} wall_scroll_t;

typedef struct
{
	int16_t x, y;
} vertex_t;

typedef struct
{
	vertex_t vtx; // A
} wall_end_t;

typedef struct
{
	vertex_t vtx; // A
	vertex_t dist; // B
	uint16_t angle; // C
	uint16_t special; // D
	uint8_t tflags; // E
	uint8_t texture; // F
	uint8_t tex_ox; // G
	uint8_t tex_oy; // H
	// MARK_SCROLL
	wall_scroll_t scroll;
} __attribute__((packed)) wall_solid_t;

typedef struct
{
	vertex_t vtx; // A
	vertex_t dist; // B
	uint16_t angle; // C
	uint16_t special; // D
	uint8_t tflags; // E
	uint8_t texture_top; // F
	uint8_t tex_top_ox; // G
	uint8_t tex_top_oy; // H
	uint8_t texture_bot; // I
	uint8_t tex_bot_ox; // J
	uint8_t tex_bot_oy; // K
	// this order is forced by 6502 ASM
	uint8_t backsector; // L
	uint8_t blocking; // M
	// MARK_SCROLL
	wall_scroll_t scroll_top;
	wall_scroll_t scroll_bot;
} __attribute__((packed)) wall_portal_t;

typedef struct
{
	vertex_t vtx; // A
	vertex_t dist; // B
	uint16_t angle; // C
	uint16_t special; // D
	uint8_t tflags; // E
	uint8_t texture_top; // F
	uint8_t tex_top_ox; // G
	uint8_t tex_top_oy; // H
	uint8_t texture_bot; // I
	uint8_t tex_bot_ox; // J
	uint8_t tex_bot_oy; // K
	int16_t height_split; // L M
	// MARK_SCROLL
	wall_scroll_t scroll_top;
	wall_scroll_t scroll_bot;
} __attribute__((packed)) wall_split_t;

typedef struct
{
	vertex_t vtx; // A
	vertex_t dist; // B
	uint16_t angle; // C
	uint16_t special; // D
	uint8_t tflags; // E
	uint8_t texture_top; // F
	uint8_t tex_top_ox; // G
	uint8_t tex_top_oy; // H
	uint8_t texture_bot; // I
	uint8_t tex_bot_ox; // J
	uint8_t tex_bot_oy; // K
	uint8_t backsector; // L
	uint8_t blocking; // M
	uint8_t texture_mid;
	uint8_t tex_mid_ox;
	uint8_t tex_mid_oy;
	uint8_t blockmid;
	// MARK_SCROLL
	wall_scroll_t scroll_top;
	wall_scroll_t scroll_bot;
	wall_scroll_t scroll_mid;
} __attribute__((packed)) wall_masked_t;

typedef union
{
	wall_end_t end;
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

//

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
