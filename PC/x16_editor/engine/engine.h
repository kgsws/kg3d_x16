
// angle calculation
#define ANGLE_TO_RAD(x)	((float)(x) * ((M_PI*2) / 65536.0f))
#define PITCH_TO_RAD(x)	((float)(x) * (M_PI / 65535.0f))
#define RAD_TO_ANGLE(x)	((x) / ((M_PI*2) / 65536.0f))
#define RAD_TO_VNGLE(x)	((x) / (M_PI / 65536.0f))
#define ANGLE_TO_DEG(x)	((float)(x) * (360.0f / 65536.0f))
#define VNGLE_TO_DEG(x)	((float)(x) * (180.0f / 65536.0f))

// conversion stuff
#define float32bits(x)	*((uint32_t*)&(x))

// texture flags (walls)
#define TEXFLAG_PEG_Y	1	// forced by X16
#define TEXFLAG_MIRROR_X	2
#define TEXFLAG_MIRROR_Y_SWAP_XY	4

#define TEXFLAG_PEG_MID_BACK	TEXFLAG_MIRROR_Y_SWAP_XY

// wall flags
#define WALLFLAG_PEG_X	1

// sector flags
#define SECFLAG_WATER	0x80	// forced by X16

// pre-calculated sector flags
#define PSF_IS_CONVEX	1

//

enum
{
	BOX_XMIN,
	BOX_XMAX,
	BOX_YMIN,
	BOX_YMAX,
	//
	SECTOR_BOX_SIZE
};

enum
{
	PLANE_TOP,
	PLANE_BOT,
	PLANE_MASK = 1,
	PLANE_USE_COLOR = 0x40000000,
	PLANE_USE_Z = 0x80000000,
	PLANE_ZSHIFT = 8,
	PLANE_ZMASK = 0x00FFFF00,
};
#define PLANE_CUSTOMZ(z)	(PLANE_USE_Z | ((uint32_t)(z) << PLANE_ZSHIFT))

//

typedef struct gl_vertex_s
{
	float x, y, z;
	float s, t;
} gl_vertex_t;

typedef struct
{
	union
	{
		float val[5];
		struct
		{
			float a, b, c, d;
			float ic;
		} pl;
	};
} gl_plane_t;

typedef struct
{
	uint32_t idx;
	uint8_t name[MAX_X16_MAPSTRING];
	uint8_t ox, oy;
	uint8_t flags;
	uint8_t angle;
} kge_x16_tex_t;

typedef struct
{
	uint32_t idx;
	uint8_t name[MAX_X16_MAPSTRING];
} kge_light_t;

typedef struct
{
	uint8_t flags;
	uint8_t blocking;
	uint8_t blockmid;
} kge_line_info_t;

//

typedef union kge_xyz_vec_u
{
	float xyz[3];
	struct
	{
		float x, y, z;
	};
} kge_xyz_vec_t;

typedef union kge_xy_vec_u
{
	float xy[2];
	struct
	{
		float x, y;
	};
} kge_xy_vec_t;

//

typedef struct kge_ticcmd_s
{
	int16_t fmove, smove;
	uint16_t angle;
	int16_t pitch;
	uint32_t buttons;
} kge_ticcmd_t;

//

typedef struct kge_sector_validcount_s
{
	uint32_t editor;
	uint32_t move;
} kge_sector_validcount_t;

typedef struct kge_line_validcount_s
{
	uint32_t x16port;
	uint32_t editor;
	uint32_t move;
} kge_line_validcount_t;

typedef struct kge_thing_validcount_s
{
	uint32_t editor;
} kge_thing_validcount_t;

//

typedef struct
{
	uint32_t validcount;
	uint32_t id;
} kge_identity_t;

//

typedef struct kge_line_stuff_s
{
	kge_xy_vec_t center;
	kge_xy_vec_t normal;
	kge_xy_vec_t dist;
	float length;
	uint16_t x16angle;
} kge_line_stuff_t;

typedef struct kge_sector_stuff_s
{
	uint32_t flags;
	uint8_t group[2];
	uint8_t palette;
	uint8_t x16flags;
	float box[SECTOR_BOX_SIZE];
	struct
	{
		float x, y;
	} center;
} kge_sector_stuff_t;

//

typedef struct kge_secplane_s
{
	float height;
	kge_x16_tex_t texture;
	struct kge_sector_s *link;
} kge_secplane_t;

//

typedef struct kge_thing_pos_s
{
	struct kge_sector_s *sector;
	union
	{
		float xyz[3];
		struct
		{
			float x, y, z;
		};
	};
	uint16_t angle;
	int16_t pitch;
} kge_thing_pos_t;

typedef struct kge_thing_prop_s
{
	uint8_t name[MAX_X16_MAPSTRING];
	uint32_t type;
	uint32_t eflags;
	float radius;
	float height;
	float speed;
	float gravity;
	float viewheight;
	float viewz;
	float floorz;
	float ceilingz;
} kge_thing_prop_t;

typedef struct
{
	uint32_t gltex;
	uint32_t width, height, stride;
	uint32_t bright;
	int32_t ox, oy;
	float ws, scale;
	uint32_t shader;
	uint16_t *data;
	uint32_t *offs;
} kge_sprite_t;

//

typedef struct sector_link_s
{
	struct sector_link_s **prev_sector;
	struct sector_link_s *next_sector;
	struct sector_link_s **prev_thing;
	struct sector_link_s *next_thing;
	struct kge_thing_s *thing;
} sector_link_t;

//

typedef union kge_vertex_u
{
	float xy[2];
	struct
	{
		float x, y;
		uint32_t vc_move;
		uint32_t vc_editor;
	};
} kge_vertex_t;

typedef struct kge_line_s
{
	kge_vertex_t *vertex[2];
	kge_x16_tex_t texture[3];
	kge_line_info_t info;
	kge_identity_t tex_ident[3];
	float texture_split;
	void *object;
	kge_line_stuff_t stuff;
	kge_line_validcount_t vc;
	struct kge_sector_s *frontsector;
	struct kge_sector_s *backsector;
} kge_line_t;

typedef struct kge_sector_s
{
	kge_secplane_t plane[2];
	kge_light_t light;
	sector_link_t *thinglist;
	kge_identity_t pl_ident[2];
	kge_sector_stuff_t stuff;
	kge_sector_validcount_t vc;
	linked_list_t objects;
	uint32_t line_count;
	kge_line_t line[];
} kge_sector_t;

typedef struct kge_thing_s
{
	kge_thing_pos_t pos;
	kge_xyz_vec_t mom;
	kge_thing_prop_t prop;
	kge_sprite_t sprite;
	kge_thing_validcount_t vc;
	kge_identity_t ident;
	sector_link_t *thinglist;
	kge_ticcmd_t *ticcmd;
} kge_thing_t;

//

typedef struct kge_tickhead_s
{
	uint32_t (*tick)(void*);
} kge_tickhead_t;

typedef struct kge_ticker_s
{
	kge_tickhead_t info;
	union
	{
		uint8_t data[8];
		kge_thing_t thing;
	};
} kge_ticker_t;

//

extern sector_link_t **engine_link_ptr;

//

void engine_update_sector(kge_sector_t *sec);
void engine_update_line(kge_line_t *line);

//

void engine_add_sector_link(kge_thing_t *th, kge_sector_t *sector);

//
// stuff

static inline void copy_texture(kge_x16_tex_t *dst, kge_x16_tex_t *src)
{
	dst->idx = src->idx;
	strcpy(dst->name, src->name);
}

//
// math

static inline float kge_point_point_dist2(float *p0, float *p1)
{
	float x = p1[0] - p0[0];
	float y = p1[1] - p0[1];
	return x * x + y * y;
}

static inline float kge_line_point_distance(kge_line_t *ln, float x, float y)
{
	return ln->stuff.normal.y * (y - ln->vertex[0]->y) + ln->stuff.normal.x * (x - ln->vertex[0]->x);
}

static inline float kge_line_point_side(kge_line_t *ln, float x, float y)
{
	return ln->stuff.dist.x * (y - ln->vertex[0]->y) - ln->stuff.dist.y * (x - ln->vertex[0]->x);
}

static inline float kge_divline_point_side(float *xy, float *dist, float x, float y)
{
	return dist[0] * (y - xy[1]) - dist[1] * (x - xy[0]);
}

static inline float kge_divline_cross(float *xy0, float *d0, float *xy1, float *d1)
{
	float x, y, d, n;

	d = d0[1] * d1[0] - d0[0] * d1[1];
	if(fabs(d) < 0.0001f)
		return INFINITY;

	x = xy0[0] - xy1[0];
	y = xy1[1] - xy0[1];

	n = x * d0[1] + y * d0[0];

	return n / d;
}

