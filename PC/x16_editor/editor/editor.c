#include "inc.h"
#include "defs.h"
#include "list.h"
#include "engine.h"
#include "system.h"
#include "shader.h"
#include "matrix.h"
#include "render.h"
#include "input.h"
#include "tick.h"
#include "things.h"
#include "x16g.h"
#include "x16t.h"
#include "x16r.h"
#include "x16e.h"
#include "image.h"
#include "kgcbor.h"
#include "editor.h"
#include "glui.h"
#include "ui_def.h"

#define STATUS_LINE_HEIGHT	24
#define STATUS_LINE_TICK	(2 * TICKRATE - TICKRATE / 4)

#define FILE_ENTRY_W	160
#define FILE_ENTRY_H	32
#define FILE_LIST_W	5
#define FILE_LIST_H	17
#define FILE_LIST_MAX	(FILE_LIST_W * FILE_LIST_H)
#define MAX_FILE_NAME	32

#define TEXTURE_ENTRY_W	148
#define TEXTURE_ENTRY_H	172
#define TEXTURE_ENTRY_GAP	10
#define TEXTURE_LIST_W	5
#define TEXTURE_LIST_H	3
#define TEXTURE_LIST_MAX	(TEXTURE_LIST_W * TEXTURE_LIST_H)

#define MAX_TEXT_ENTRY	48
#define TEXT_ENTRY_BLINK_CHAR	'_'
#define TEXT_ENTRY_BLINK_RATE	(TICKRATE / 4)

enum
{
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTER,
	ALIGN_TOP = ALIGN_LEFT,
	ALIGN_BOT = ALIGN_RIGHT,
};

enum
{
	CBOR_ROOT_LOGO,
	CBOR_ROOT_SKY,
	CBOR_ROOT_GROUPS,
	CBOR_ROOT_CAMERA,
	CBOR_ROOT_VERTEXES,
	CBOR_ROOT_SECTORS,
	CBOR_ROOT_THINGS,
	//
	NUM_CBOR_ROOT
};

enum
{
	CBOR_CAMERA_X,
	CBOR_CAMERA_Y,
	CBOR_CAMERA_Z,
	CBOR_CAMERA_SECTOR,
	CBOR_CAMERA_ANGLE,
	CBOR_CAMERA_PITCH,
	//
	NUM_CBOR_CAMERA
};

enum
{
	CBOR_THING_TYPE,
	CBOR_THING_X,
	CBOR_THING_Y,
	CBOR_THING_Z,
	CBOR_THING_SECTOR,
	CBOR_THING_ANGLE,
	//
	NUM_CBOR_THING
};

enum
{
	CBOR_VTX_X,
	CBOR_VTX_Y,
	//
	NUM_CBOR_VTX
};

enum
{
	CBOR_SECTOR_LIGHT,
	CBOR_SECTOR_PALETTE,
	CBOR_SECTOR_IS_WATER,
	CBOR_SECTOR_GROUP,
	CBOR_SECTOR_CEILING,
	CBOR_SECTOR_FLOOR,
	CBOR_SECTOR_LINES,
	CBOR_SECTOR_OBJECTS,
	//
	NUM_CBOR_SECTOR
};

enum
{
	CBOR_SECPLANE_HEIGHT,
	CBOR_SECPLANE_TEXTURE,
	CBOR_SECPLANE_OFFS_X,
	CBOR_SECPLANE_OFFS_Y,
	CBOR_SECPLANE_ANGLE,
	CBOR_SECPLANE_LINK,
	//
	NUM_CBOR_SECPLANE
};

enum
{
	CBOR_LINE_BACKSECTOR,
	CBOR_LINE_SPLIT,
	CBOR_LINE_BLOCKING,
	CBOR_LINE_BLOCKMID,
	CBOR_LINE_PEG_X,
	CBOR_LINE_SKIP,
	CBOR_LINE_VTX,
	CBOR_LINE_TOP,
	CBOR_LINE_BOT,
	CBOR_LINE_MID,
	//
	NUM_CBOR_LINE
};

enum
{
	CBOR_OBJLINE_PEG_X,
	CBOR_OBJLINE_SKIP,
	CBOR_OBJLINE_X,
	CBOR_OBJLINE_Y,
	CBOR_OBJLINE_TOP,
	//
	NUM_CBOR_OBJLINE
};

enum
{
	CBOR_LINETEX_TEXTURE,
	CBOR_LINETEX_OFFS_X,
	CBOR_LINETEX_OFFS_Y,
	CBOR_LINETEX_MIRROR_X,
	CBOR_LINETEX_MIRROR_Y,
	CBOR_LINETEX_PEG_Y,
	//
	NUM_CBOR_LINETEX
};

typedef struct
{
	uint8_t name[MAX_FILE_NAME];
	uint16_t nlen;
	uint16_t isdir;
} file_list_t;

typedef struct
{
	const uint8_t *name;
	uint32_t type;
	uint32_t tidx;
} internal_thing_t;

// colors
edit_color_t editor_color[EDITCOLOR__COUNT] =
{
	[EDITCOLOR_GRID] = {0.15f, 0.15f, 0.15f, 1.0f},
	[EDITCOLOR_ORIGIN] = {0.3f, 0.3f, 0.3f, 1.0f},
	[EDITCOLOR_CAMERA] = {0.5f, 0.1f, 1.0f, 1.0f},
	[EDITCOLOR_CAMERA_BAD] = {1.0f, 0.2f, 0.2f, 1.0f},
	[EDITCOLOR_THING] = {0.1f, 1.0f, 1.0f, 1.0f},
	[EDITCOLOR_THING_BAD] = {1.0f, 0.2f, 0.2f, 1.0f},
	[EDITCOLOR_OBJ_ORG] = {1.0f, 0.75f, 0.0f, 0.25f},
	[EDITCOLOR_LINE_SOLID] = {1.0f, 1.0f, 1.0f, 1.0f},
	[EDITCOLOR_LINE_PORTAL] = {0.0f, 0.8f, 0.5f, 1.0f},
	[EDITCOLOR_LINE_NEW] = {1.0f, 1.0f, 1.0f, 1.0f},
	[EDITCOLOR_LINE_OBJ] = {1.0f, 1.0f, 0.0f, 1.0f},
	[EDITCOLOR_LINE_BAD] = {1.0f, 0.0f, 0.0f, 1.0f},
	[EDITCOLOR_VERTEX] = {0.0f, 0.8f, 0.0f, 1.0f},
	[EDITCOLOR_VERTEX_NEW] = {0.8f, 0.8f, 0.0f, 1.0f},
	[EDITCOLOR_SECTOR_HIGHLIGHT] = {0.0f, 0.8f, 0.0f, 1.0f},
	[EDITCOLOR_SECTOR_HIGHLIGHT_BAD] = {0.9f, 0.0f, 0.0f, 1.0f},
	[EDITCOLOR_SECTOR_HIGHLIGHT_LINK] = {0.6f, 0.8f, 0.0f, 1.0f},
	[EDITCOLOR_CROSSHAIR] = {1.0f, 1.0f, 1.0f, 1.0f},
};

// blocking

static const uint8_t *const blocking_name[NUM_BLOCKBITS] =
{
	[BLOCKING_PLAYER] = "player",
	[BLOCKING_ENEMY] = "enemy",
	[BLOCKING_SOLID] = "solid",
	[BLOCKING_PROJECTILE] = "projectile",
	[BLOCKING_HITSCAN] = "hitscan",
	[BLOCKING_EXTRA_A] = "extra A",
	[BLOCKING_EXTRA_B] = "extra B",
	[BLOCKING_SPECIAL] = "special",
};

// other text

const uint8_t *txt_right_left[] =
{
	"right",
	"left"
};

const uint8_t *txt_yes_no[] =
{
	"yes",
	"no"
};

const uint8_t *txt_top_bot[] =
{
	"top",
	"bottom"
};

const uint8_t *txt_back_front[] =
{
	"back",
	"front"
};

//

uint32_t vc_editor;

linked_list_t edit_list_draw_new;
linked_list_t edit_list_vertex;
linked_list_t edit_list_sector;

int32_t edit_sky_num = -1;

float edit_highlight_alpha = 0.5f;

edit_hit_t edit_hit;

gl_vertex_t edit_circle_vtx[EDIT_THING_CIRCLE_VTX+4];

edit_group_t edit_group[EDIT_MAX_GROUPS];
uint32_t edit_num_groups;
uint32_t edit_grp_show;
uint32_t edit_subgrp_show;
uint32_t edit_grp_last;
uint32_t edit_subgrp_last;

static int32_t mode_2d3d;
kge_thing_pos_t edit_wf_pos;

kge_thing_t *edit_camera_thing;

void *edit_cbor_buffer;

// mode
uint32_t edit_x16_mode = EDIT_XMODE_MAP;

// save / load
static uint8_t map_path[EDIT_MAX_FILE_PATH];

// menu
uint32_t edit_ui_busy;

// common selector
static void (*common_sel_fill_func)();
static int32_t selector_sprite;
static union
{
	kge_x16_tex_t *texture;
	kge_thing_t *thing;
	kge_light_t *light;
	thing_st_t *state;
	uint32_t *hash;
	uint8_t *frame;
	uint8_t *palidx;
	uint8_t *blocking;
	void *ptr;
} selector_dest;

// texture selector
static uint32_t texsel_need_more;
static uint32_t texsel_start;
static uint32_t texsel_mode;
static uint16_t texsel_slot[TEXTURE_LIST_MAX];

// blocking selector
static uint32_t blocksel_bits;
static int32_t blocksel_is_wall;

// text entry
static void (*textentry_cb)(uint8_t*);
static uint32_t textentry_limit;
static uint32_t textentry_idx;
static uint32_t textentry_blink;
static uint8_t text_entry[MAX_TEXT_ENTRY + 4];

// question
static void (*question_cb)(uint32_t);

// file select
static file_list_t file_names[FILE_LIST_MAX];

static uint8_t filesel_path[EDIT_MAX_FILE_PATH];
static uint8_t filesel_suffix[8];
static uint32_t filesel_pos, filesel_root, filesel_slen;
static void (*filesel_cb)(uint8_t*);

// group select
static void (*grpsel_cb)(uint32_t);
static uint32_t grpsel_flags;
static uint32_t grpsel_grp;
static uint32_t grpsel_sub;

// info box
uint8_t edit_info_box_text[1024];

// x16 mode stuff
static uint32_t mode_xslot[3];

static const uint8_t *const mode_name[NUM_X16_EDIT_MODES] =
{
	[EDIT_XMODE_MAP] = "Map editor",
	[EDIT_XMODE_GFX] = "GFX editor",
	[EDIT_XMODE_THG] = "Thing editor",
};

// internal thing types
static internal_thing_t internal_thing[] =
{
	{
		.name = "----\nPlayer Start",
		.type = THING_TYPE_START_NORMAL,
		.tidx = X16G_GLTEX_SPR_START
	},
	{
		.name = "----\nCoop Start",
		.type = THING_TYPE_START_COOP,
		.tidx = X16G_GLTEX_SPR_START_COOP
	},
	{
		.name = "----\nDeathmatch Start",
		.type = THING_TYPE_START_DM,
		.tidx = X16G_GLTEX_SPR_START_DM
	}
};

//
static int32_t texture_select_click(glui_element_t *elm, int32_t x, int32_t y);
static int32_t light_select_click(glui_element_t *elm, int32_t x, int32_t y);
static int32_t palette_select_click(glui_element_t *elm, int32_t x, int32_t y);
static int32_t sprite_select_click(glui_element_t *elm, int32_t x, int32_t y);
static int32_t frame_select_click(glui_element_t *elm, int32_t x, int32_t y);
static int32_t thing_select_click(glui_element_t *elm, int32_t x, int32_t y);
static int32_t action_select_click(glui_element_t *elm, int32_t x, int32_t y);
static int32_t file_select_click(glui_element_t *elm, int32_t x, int32_t y);

static int32_t group_select_show_all(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_show_current(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_create_main(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_group(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_delete(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_rename(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_create_sub(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_return(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_subgroup(glui_element_t *elm, int32_t x, int32_t y);
static int32_t group_select_subgroup_now(glui_element_t *elm, int32_t x, int32_t y);

// CBOR type map
static const uint32_t cbor_type_map[] =
{
	[EDIT_CBOR_TYPE_FLAG8] = KGCBOR_TYPE_BOOLEAN,
	[EDIT_CBOR_TYPE_U8] = KGCBOR_TYPE_VALUE,
	[EDIT_CBOR_TYPE_S8] = KGCBOR_TYPE_VALUE,
	[EDIT_CBOR_TYPE_U16] = KGCBOR_TYPE_VALUE,
	[EDIT_CBOR_TYPE_S16] = KGCBOR_TYPE_VALUE,
	[EDIT_CBOR_TYPE_U32] = KGCBOR_TYPE_VALUE,
	[EDIT_CBOR_TYPE_S32] = KGCBOR_TYPE_VALUE,
	[EDIT_CBOR_TYPE_ANGLE] = KGCBOR_TYPE_VALUE,
	[EDIT_CBOR_TYPE_FLOAT] = KGCBOR_TYPE_VALUE,
	[EDIT_CBOR_TYPE_STRING] = KGCBOR_TYPE_STRING,
	[EDIT_CBOR_TYPE_BINARY] = KGCBOR_TYPE_BINARY,
	[EDIT_CBOR_TYPE_OBJECT] = KGCBOR_TYPE_OBJECT,
	[EDIT_CBOR_TYPE_ARRAY] = KGCBOR_TYPE_ARRAY,
};

// map CBOR

static edit_group_t *load_group;
static int32_t cbor_map_groups(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_map_camera(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_map_vertexes(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_map_sectors(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_map_things(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static const edit_cbor_obj_t cbor_root[] =
{
	[CBOR_ROOT_LOGO] =
	{
		.name = "logo",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_BINARY,
		.extra = sizeof(x16e_logo_data),
		.valid = &x16e_enable_logo,
		.ptr = x16e_logo_data
	},
	[CBOR_ROOT_SKY] =
	{
		.name = "sky",
		.nlen = 3,
		.type = EDIT_CBOR_TYPE_STRING,
		.extra = LEN_X16_SKY_NAME,
		.ptr = x16_sky_name
	},
	[CBOR_ROOT_GROUPS] =
	{
		.name = "groups",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_map_groups
	},
	[CBOR_ROOT_CAMERA] =
	{
		.name = "camera",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_map_camera
	},
	[CBOR_ROOT_VERTEXES] =
	{
		.name = "vertexes",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_map_vertexes
	},
	[CBOR_ROOT_SECTORS] =
	{
		.name = "sectors",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_map_sectors
	},
	[CBOR_ROOT_THINGS] =
	{
		.name = "things",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_map_things
	},
	// terminator
	[NUM_CBOR_ROOT] = {}
};

static kge_thing_pos_t load_camera;
static const edit_cbor_obj_t cbor_camera[] =
{
	[CBOR_CAMERA_X] =
	{
		.name = "x",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_camera.x,
	},
	[CBOR_CAMERA_Y] =
	{
		.name = "y",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_camera.y,
	},
	[CBOR_CAMERA_Z] =
	{
		.name = "z",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_camera.z,
	},
	[CBOR_CAMERA_SECTOR] =
	{
		.name = "sector",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_S32,
		.s32 = (uint32_t*)&load_camera.sector,
	},
	[CBOR_CAMERA_ANGLE] =
	{
		.name = "angle",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_ANGLE,
		.u16 = &load_camera.angle,
	},
	[CBOR_CAMERA_PITCH] =
	{
		.name = "pitch",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U16,
		.s16 = &load_camera.pitch,
	},
	// terminator
	[NUM_CBOR_CAMERA] = {}
};

static kge_thing_t *new_thing;
static kge_thing_t load_thing;
static const edit_cbor_obj_t cbor_thing[] =
{
	[CBOR_THING_TYPE] =
	{
		.name = "type",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_STRING,
		.extra = MAX_X16_MAPSTRING,
		.ptr = load_thing.prop.name,
	},
	[CBOR_THING_X] =
	{
		.name = "x",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_thing.pos.x,
	},
	[CBOR_THING_Y] =
	{
		.name = "y",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_thing.pos.y,
	},
	[CBOR_THING_Z] =
	{
		.name = "z",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_thing.pos.z,
	},
	[CBOR_THING_SECTOR] =
	{
		.name = "sector",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_S32,
		.s32 = (uint32_t*)&load_thing.pos.sector,
	},
	[CBOR_THING_ANGLE] =
	{
		.name = "angle",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_ANGLE,
		.u16 = &load_thing.pos.angle,
	},
	// terminator
	[NUM_CBOR_CAMERA] = {}
};

static kge_vertex_t load_vtx;
static const edit_cbor_obj_t cbor_vtx[] =
{
	[CBOR_VTX_X] =
	{
		.name = "x",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_vtx.x,
	},
	[CBOR_VTX_Y] =
	{
		.name = "y",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_vtx.y,
	},
	// terminator
	[NUM_CBOR_VTX] = {}
};

static kge_sector_t *load_sector;
static uint8_t load_secpalidx;
static uint8_t load_sector_flags;
static uint8_t load_seclight[MAX_X16_MAPSTRING];
static int32_t cbor_sector_group(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_sector_ceiling(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_sector_floor(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_sector_lines(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_sector_objects(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static const edit_cbor_obj_t cbor_sector[] =
{
	[CBOR_SECTOR_LIGHT] =
	{
		.name = "light",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_STRING,
		.extra = MAX_X16_MAPSTRING,
		.ptr = load_seclight,
	},
	[CBOR_SECTOR_PALETTE] =
	{
		.name = "palette",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_U8,
		.ptr = &load_secpalidx,
	},
	[CBOR_SECTOR_IS_WATER] =
	{
		.name = "water",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.ptr = &load_sector_flags,
		.extra = SECFLAG_WATER
	},
	[CBOR_SECTOR_GROUP] =
	{
		.name = "group",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_sector_group
	},
	[CBOR_SECTOR_CEILING] =
	{
		.name = "ceiling",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_sector_ceiling
	},
	[CBOR_SECTOR_FLOOR] =
	{
		.name = "floor",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_sector_floor
	},
	[CBOR_SECTOR_LINES] =
	{
		.name = "lines",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_sector_lines
	},
	[CBOR_SECTOR_OBJECTS] =
	{
		.name = "objects",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_sector_objects
	},
	// terminator
	[NUM_CBOR_SECTOR] = {}
};

static kge_secplane_t load_secplane;
static const edit_cbor_obj_t cbor_secplane[] =
{
	[CBOR_SECPLANE_TEXTURE] =
	{
		.name = "texture",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_STRING,
		.extra = MAX_X16_MAPSTRING,
		.ptr = load_secplane.texture.name,
	},
	[CBOR_SECPLANE_HEIGHT] =
	{
		.name = "height",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_secplane.height,
	},
	[CBOR_SECPLANE_OFFS_X] =
	{
		.name = "x",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_secplane.texture.ox,
	},
	[CBOR_SECPLANE_OFFS_Y] =
	{
		.name = "y",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_secplane.texture.oy,
	},
	[CBOR_SECPLANE_ANGLE] =
	{
		.name = "angle",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_secplane.texture.angle,
	},
	[CBOR_SECPLANE_LINK] =
	{
		.name = "link",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_S32,
		.ptr = (void*)&load_secplane.link,
	},
	// terminator
	[NUM_CBOR_SECPLANE] = {}
};

static kge_line_t *load_line;
static int32_t load_backsector;
static float load_split;
static uint8_t load_lblock;
static uint8_t load_mblock;
static uint8_t load_lnflags;
static int32_t cbor_line_vtx(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_line_top(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_line_bot(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_line_mid(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static const edit_cbor_obj_t cbor_line[] =
{
	[CBOR_LINE_BACKSECTOR] =
	{
		.name = "backsector",
		.nlen = 10,
		.type = EDIT_CBOR_TYPE_S32,
		.s32 = &load_backsector
	},
	[CBOR_LINE_SPLIT] =
	{
		.name = "split",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_FLOAT,
		.f32 = &load_split
	},
	[CBOR_LINE_BLOCKING] =
	{
		.name = "blocking",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_lblock
	},
	[CBOR_LINE_BLOCKMID] =
	{
		.name = "blockmid",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_mblock
	},
	[CBOR_LINE_PEG_X] =
	{
		.name = "peg x",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.extra = WALLFLAG_PEG_X,
		.u8 = &load_lnflags
	},
	[CBOR_LINE_SKIP] =
	{
		.name = "skip",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.extra = WALLFLAG_SKIP,
		.u8 = &load_lnflags
	},
	[CBOR_LINE_VTX] =
	{
		.name = "vtx",
		.nlen = 3,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_line_vtx
	},
	[CBOR_LINE_TOP] =
	{
		.name = "top",
		.nlen = 3,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_line_top
	},
	[CBOR_LINE_BOT] =
	{
		.name = "bot",
		.nlen = 3,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_line_bot
	},
	[CBOR_LINE_MID] =
	{
		.name = "mid",
		.nlen = 3,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_line_mid
	},
	// terminator
	[NUM_CBOR_LINE] = {}
};

static edit_sec_obj_t *load_object;
static edit_cbor_obj_t cbor_objline[] =
{
	[CBOR_OBJLINE_PEG_X] =
	{
		.name = "peg x",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.extra = WALLFLAG_PEG_X,
		.u8 = &load_lnflags
	},
	[CBOR_OBJLINE_SKIP] =
	{
		.name = "skip",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.extra = WALLFLAG_SKIP,
		.u8 = &load_lnflags
	},
	[CBOR_OBJLINE_X] =
	{
		.name = "x",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
	},
	[CBOR_OBJLINE_Y] =
	{
		.name = "y",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_FLOAT,
	},
	[CBOR_OBJLINE_TOP] =
	{
		.name = "top",
		.nlen = 3,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_line_top
	},
	// terminator
	[NUM_CBOR_OBJLINE] = {}
};

static kge_x16_tex_t load_linetex;
static const edit_cbor_obj_t cbor_linetex[] =
{
	[CBOR_LINETEX_TEXTURE] =
	{
		.name = "texture",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_STRING,
		.extra = MAX_X16_MAPSTRING,
		.ptr = load_linetex.name,
	},
	[CBOR_LINETEX_OFFS_X] =
	{
		.name = "x",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_linetex.ox,
	},
	[CBOR_LINETEX_OFFS_Y] =
	{
		.name = "y",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_U8,
		.u8 = &load_linetex.oy,
	},
	[CBOR_LINETEX_MIRROR_X] =
	{
		.name = "mirror x",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.extra = TEXFLAG_MIRROR_X,
		.u8 = &load_linetex.flags,
	},
	[CBOR_LINETEX_MIRROR_Y] =
	{
		.name = "mirror y",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.extra = TEXFLAG_MIRROR_Y_SWAP_XY,
		.u8 = &load_linetex.flags,
	},
	[CBOR_LINETEX_PEG_Y] =
	{
		.name = "peg y",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.extra = TEXFLAG_PEG_Y,
		.u8 = &load_linetex.flags,
	},
	// terminator
	[NUM_CBOR_LINETEX] = {}
};

// shape for thing highlight
static const gl_vertex_t edit_shape_arrow[] =
{
	{0.0f, 1.25f, 0.0f},
	{0.5f, 0.0f, 0.0f},
	{-0.5f, 0.0f, 0.0f},
	{0.0f, 1.25f, 0.0f}
};

// map defaults
kge_secplane_t edit_ceiling_plane =
{
	.texture = {.name = "\t"},
	.height = 256.0f
};

kge_secplane_t edit_floor_plane =
{
	.texture = {.name = "\t"},
	.height = 0.0f
};

kge_x16_tex_t edit_line_default;

const kge_x16_tex_t edit_empty_texture =
{
	.name = "\t"
};

//
//

static int32_t infunc_edit_menu_click();
static int32_t infunc_edit_menu();
static int32_t infunc_edit_menu_left();
static int32_t infunc_edit_menu_right();
static int32_t infunc_edit_menu_up();
static int32_t infunc_edit_menu_down();
static int32_t infunc_edit_menu_copy();
static int32_t infunc_edit_menu_paste();

static const edit_input_func_t infunc_edit_ui[] =
{
	{INPUT_EDITOR_MENU_CLICK, infunc_edit_menu_click},
	{INPUT_EDITOR_MENU, infunc_edit_menu},
	{INPUT_EDITOR_MENU_LEFT, infunc_edit_menu_left},
	{INPUT_EDITOR_MENU_RIGHT, infunc_edit_menu_right},
	{INPUT_EDITOR_INCREASE, infunc_edit_menu_up},
	{INPUT_EDITOR_DECREASE, infunc_edit_menu_down},
	{INPUT_EDITOR_COPY, infunc_edit_menu_copy},
	{INPUT_EDITOR_PASTE, infunc_edit_menu_paste},
	// terminator
	{-1}
};

//
//

static void texture_select_fill()
{
	glui_element_t **elm = ui_file_select.elements[1]->container.elements;
	uint32_t idx = texsel_start;
	uint32_t i;

	for(i = 0; i < TEXTURE_LIST_MAX && idx < editor_texture_count; idx++)
	{
		editor_texture_t *et = editor_texture + idx;
		glui_image_t *img;
		glui_text_t *txt;
		uint32_t shader;
		float colormap = 0;

		switch(et->type)
		{
			case X16G_TEX_TYPE_WALL_MASKED:
				if(texsel_mode <= 1)
					continue;
				shader = SHADER_FRAGMENT_PALETTE;
			break;
			case X16G_TEX_TYPE_WALL:
				if(texsel_mode)
					continue;
			case X16G_TEX_TYPE_PLANE_8BPP:
				if(texsel_mode > 1)
					continue;
				shader = SHADER_FRAGMENT_PALETTE;
			break;
			case X16G_TEX_TYPE_PLANE_4BPP:
				if(texsel_mode > 1)
					continue;
				colormap = COLORMAP_IDX(et->cmap);
				shader = SHADER_FRAGMENT_COLORMAP;
			break;
			default:
				shader = SHADER_FRAGMENT_RGB;
			break;
		}

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = idx;
		txt->base.click = texture_select_click;
		glui_set_text(txt, et->name, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		img->shader = shader;
		img->colormap = colormap;
		img->gltex = &et->gltex;

		if(et->width > et->height)
		{
			img->base.width = 128;
			img->base.height = 128 * ((float)et->height / (float)et->width);
		} else
		{
			img->base.width = 128 * ((float)et->width / (float)et->height);
			img->base.height = 128;
		}

		img->base.x = txt->base.x + (TEXTURE_ENTRY_W - img->base.width) / 2;
		img->base.y = txt->base.y + (TEXTURE_ENTRY_W - img->base.height) / 2;

		img->coord.s[0] = 0.0f;
		img->coord.s[1] = 1.0f;
		img->coord.t[0] = 0.0f;
		img->coord.t[1] = 1.0f;

		texsel_slot[i++] = idx;
	}

	texsel_need_more = i >= TEXTURE_LIST_MAX;

	ui_file_select.elements[1]->container.count = i * 2;
}

static void light_select_fill()
{
	glui_element_t **elm = ui_file_select.elements[1]->container.elements;
	uint32_t idx = texsel_start;
	uint32_t i;
	float t = 0.0f;

	for(i = 0; i < TEXTURE_LIST_MAX && idx < x16_num_lights; i++, idx++)
	{
		editor_light_t *el = editor_light + i;
		glui_image_t *img;
		glui_text_t *txt;

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = idx;
		txt->base.click = light_select_click;
		glui_set_text(txt, el->name, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		img->base.width = 128;
		img->base.height = 128;
		img->base.x = txt->base.x + (TEXTURE_ENTRY_W - 128) / 2;
		img->base.y = txt->base.y + (TEXTURE_ENTRY_W - 128) / 2;
		img->shader = SHADER_FRAGMENT_PALETTE;
		img->gltex = &x16_gl_tex[X16G_GLTEX_LIGHT_TEX];

		img->coord.s[0] = 0.0f;
		img->coord.s[1] = 1.0f;
		img->coord.t[0] = t;
		t += 1.0f / (float)MAX_X16_LIGHTS;
		img->coord.t[1] = t;
	}

	texsel_need_more = idx < x16_num_lights;

	ui_file_select.elements[1]->container.count = i * 2;
}

static void palette_select_fill()
{
	glui_element_t **elm = ui_file_select.elements[1]->container.elements;
	float t = 0.0f;

	for(uint32_t i = 0; i < MAX_X16_PALETTE / 4; i++)
	{
		glui_image_t *img;
		glui_text_t *txt;

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = i;
		txt->base.click = palette_select_click;
		glui_set_text(txt, x16g_palette_name[i * 4], glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		img->base.width = 128;
		img->base.height = 128;
		img->base.x = txt->base.x + (TEXTURE_ENTRY_W - 128) / 2;
		img->base.y = txt->base.y + (TEXTURE_ENTRY_W - 128) / 2;
		img->shader = SHADER_FRAGMENT_RGB;
		img->gltex = &x16_gl_tex[X16G_GLTEX_PALETTE];

		img->coord.s[0] = 0.0f;
		img->coord.s[1] = 1.0f;
		img->coord.t[0] = t;
		img->coord.t[1] = t + 1.0f / (float)MAX_X16_PALETTE;
		t += 0.25f;
	}

	texsel_need_more = 0;

	ui_file_select.elements[1]->container.count = MAX_X16_PALETTE / 2;
}

static void wframe_select_fill()
{
	glui_element_t **elm = ui_file_select.elements[1]->container.elements;
	uint32_t idx = texsel_start;
	uint8_t text[4] = {};
	uint32_t i;

	for(i = 0; i < TEXTURE_LIST_MAX && idx < MAX_X16_SPRITE_FRAMES; i++, idx++)
	{
		glui_image_t *img;
		glui_text_t *txt;
		int32_t box[4];

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = idx;
		txt->base.click = frame_select_click;
		text[0] = 'A' + idx;
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		glBindTexture(GL_TEXTURE_2D, x16_thing_texture[i+1]);
		if(!x16g_generate_weapon_texture(selector_sprite, idx, box))
		{
			int32_t width = box[2] - box[0];
			int32_t height = box[3] - box[1];

			img->shader = SHADER_FRAGMENT_PALETTE;
			img->gltex = x16_thing_texture + i + 1;

			if(width > height)
			{
				img->base.width = 128;
				img->base.height = 128 * ((float)height / (float)width);
			} else
			{
				img->base.width = 128 * ((float)width / (float)height);
				img->base.height = 128;
			}

			img->base.x = txt->base.x + (TEXTURE_ENTRY_W - img->base.width) / 2;
			img->base.y = txt->base.y + (TEXTURE_ENTRY_W - img->base.height) / 2;

			img->coord.s[0] = (float)box[0] / 256.0f;
			img->coord.s[1] = (float)box[2] / 256.0f;
			img->coord.t[0] = (float)box[1] / 256.0f;
			img->coord.t[1] = (float)box[3] / 256.0f;
		} else
			img->gltex = NULL;
	}

	texsel_need_more = idx < MAX_X16_SPRITE_FRAMES;

	ui_file_select.elements[1]->container.count = i * 2;
}

static void tframe_select_fill()
{
	glui_element_t **elm = ui_file_select.elements[1]->container.elements;
	uint32_t idx = texsel_start;
	uint8_t text[4] = {};
	uint32_t i;

	for(i = 0; i < TEXTURE_LIST_MAX && idx < MAX_X16_SPRITE_FRAMES; i++, idx++)
	{
		glui_image_t *img;
		glui_text_t *txt;

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = idx;
		txt->base.click = frame_select_click;
		text[0] = 'A' + idx;
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		glBindTexture(GL_TEXTURE_2D, x16_thing_texture[i+1]);
		if(!x16g_generate_state_texture(selector_sprite, idx, 0x40000000))
		{
			img->shader = SHADER_FRAGMENT_PALETTE;
			img->gltex = x16_thing_texture + i + 1;

			if(x16g_state_res[0] > x16g_state_res[1])
			{
				img->base.width = 128;
				img->base.height = 128 * ((float)x16g_state_res[1] / (float)x16g_state_res[0]);
			} else
			{
				img->base.width = 128 * ((float)x16g_state_res[0] / (float)x16g_state_res[1]);
				img->base.height = 128;
			}

			img->base.x = txt->base.x + (TEXTURE_ENTRY_W - img->base.width) / 2;
			img->base.y = txt->base.y + (TEXTURE_ENTRY_W - img->base.height) / 2;

			img->coord.s[0] = 0.0f;
			img->coord.s[1] = (float)x16g_state_res[0] / (float)x16g_state_res[2];

			img->coord.t[0] = 0.0f;
			img->coord.t[1] = 1.0f;
		} else
			img->gltex = NULL;
	}

	texsel_need_more = idx < MAX_X16_SPRITE_FRAMES;

	ui_file_select.elements[1]->container.count = i * 2;
}

static void wsprite_select_fill()
{
	glui_element_t **elm = ui_file_select.elements[1]->container.elements;
	uint32_t idx = texsel_start;
	uint32_t i = 0;

	if(!idx)
	{
		glui_image_t *img;
		glui_text_t *txt;

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = -1;
		txt->base.click = sprite_select_click;
		glui_set_text(txt, "\t", glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		img->gltex = NULL;

		idx++;
		i++;
	}

	for( ; i < TEXTURE_LIST_MAX && idx <= x16_num_sprlnk_wpn; i++, idx++)
	{
		editor_sprlink_t *sl = editor_sprlink + x16_num_sprlnk_thg + idx - 1;
		glui_image_t *img;
		glui_text_t *txt;
		int32_t box[4];

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = x16_num_sprlnk_thg + idx - 1;
		txt->base.click = sprite_select_click;
		glui_set_text(txt, sl->name, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		glBindTexture(GL_TEXTURE_2D, x16_thing_texture[i+1]);
		if(!x16g_generate_weapon_texture(x16_num_sprlnk_thg + idx - 1, MAX_X16_SPRITE_FRAMES, box))
		{
			int32_t width = box[2] - box[0];
			int32_t height = box[3] - box[1];

			img->shader = SHADER_FRAGMENT_PALETTE;
			img->gltex = x16_thing_texture + i + 1;

			if(width > height)
			{
				img->base.width = 128;
				img->base.height = 128 * ((float)height / (float)width);
			} else
			{
				img->base.width = 128 * ((float)width / (float)height);
				img->base.height = 128;
			}

			img->base.x = txt->base.x + (TEXTURE_ENTRY_W - img->base.width) / 2;
			img->base.y = txt->base.y + (TEXTURE_ENTRY_W - img->base.height) / 2;

			img->coord.s[0] = (float)box[0] / 256.0f;
			img->coord.s[1] = (float)box[2] / 256.0f;
			img->coord.t[0] = (float)box[1] / 256.0f;
			img->coord.t[1] = (float)box[3] / 256.0f;
		} else
			img->gltex = NULL;
	}

	texsel_need_more = idx <= x16_num_sprlnk_thg;

	ui_file_select.elements[1]->container.count = i * 2;
}

static void tsprite_select_fill()
{
	glui_element_t **elm = ui_file_select.elements[1]->container.elements;
	uint32_t idx = texsel_start;
	uint32_t i = 0;

	if(!idx)
	{
		glui_image_t *img;
		glui_text_t *txt;

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = -1;
		txt->base.click = sprite_select_click;
		glui_set_text(txt, "\t", glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		img->gltex = NULL;

		idx++;
		i++;
	}

	for( ; i < TEXTURE_LIST_MAX && idx <= x16_num_sprlnk_thg; i++, idx++)
	{
		editor_sprlink_t *sl = editor_sprlink + idx - 1;
		glui_image_t *img;
		glui_text_t *txt;

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = idx - 1;
		txt->base.click = sprite_select_click;
		glui_set_text(txt, sl->name, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		glBindTexture(GL_TEXTURE_2D, x16_thing_texture[i+1]);
		if(!x16g_generate_state_texture(idx - 1, 0, 0))
		{
			img->shader = SHADER_FRAGMENT_PALETTE;
			img->gltex = x16_thing_texture + i + 1;

			if(x16g_state_res[0] > x16g_state_res[1])
			{
				img->base.width = 128;
				img->base.height = 128 * ((float)x16g_state_res[1] / (float)x16g_state_res[0]);
			} else
			{
				img->base.width = 128 * ((float)x16g_state_res[0] / (float)x16g_state_res[1]);
				img->base.height = 128;
			}

			img->base.x = txt->base.x + (TEXTURE_ENTRY_W - img->base.width) / 2;
			img->base.y = txt->base.y + (TEXTURE_ENTRY_W - img->base.height) / 2;

			img->coord.s[0] = 0.0f;
			img->coord.s[1] = (float)x16g_state_res[0] / (float)x16g_state_res[2];

			img->coord.t[0] = 0.0f;
			img->coord.t[1] = 1.0f;
		} else
			img->gltex = NULL;
	}

	texsel_need_more = idx <= x16_num_sprlnk_thg;

	ui_file_select.elements[1]->container.count = i * 2;
}

static void thing_select_fill()
{
	glui_element_t **elm = ui_file_select.elements[1]->container.elements;
	uint32_t idx = texsel_start;
	uint32_t tdx = texsel_start;
	uint32_t i = 0;
	uint32_t top = MAX_X16_THING_TYPES;
	uint8_t tnam[16];

	if(selector_dest.thing)
	{
		for( ; i < TEXTURE_LIST_MAX && idx < sizeof(internal_thing) / sizeof(internal_thing_t); idx++, i++)
		{
			internal_thing_t *it = internal_thing + idx;
			glui_image_t *img;
			glui_text_t *txt;

			txt = (glui_text_t*)*elm;
			elm++;

			txt->base.custom = it->type;
			txt->base.click = thing_select_click;
			glui_set_text(txt, it->name, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

			img = (glui_image_t*)*elm;
			elm++;

			x16g_set_thingsel_texture(img, it->tidx);

			img->base.x = txt->base.x + (TEXTURE_ENTRY_W - img->base.width) / 2;
			img->base.y = txt->base.y + (TEXTURE_ENTRY_W - img->base.height) / 2;

			img->coord.s[0] = 0.0f;
			img->coord.s[1] = 1.0f;

			img->coord.t[0] = 0.0f;
			img->coord.t[1] = 1.0f;
		}

		tdx = idx - sizeof(internal_thing) / sizeof(internal_thing_t);
		top = MAX_X16_THING_TYPES - X16_NUM_UNLISTED_THINGS;
	}

	for( ; i < TEXTURE_LIST_MAX && tdx < top; tdx++)
	{
		thing_def_t *ti = thing_info + tdx;
		thing_sprite_t *spr = x16_thing_sprite + tdx;
		glui_image_t *img;
		glui_text_t *txt;

		if(!spr->width)
			continue;

		txt = (glui_text_t*)*elm;
		elm++;

		txt->base.custom = tdx;
		txt->base.click = thing_select_click;

		if(!ti->name.text[0])
		{
			sprintf(tnam, "#%u", tdx);
			glui_set_text(txt, tnam, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);
		} else
			glui_set_text(txt, ti->name.text, glui_font_small_kfn, GLUI_ALIGN_CENTER_BOT);

		img = (glui_image_t*)*elm;
		elm++;

		img->shader = SHADER_FRAGMENT_PALETTE;
		img->gltex = x16_thing_texture + tdx;

		if(spr->width > spr->height)
		{
			img->base.width = 128;
			img->base.height = 128 * ((float)spr->height / (float)spr->width);
		} else
		{
			img->base.width = 128 * ((float)spr->width / (float)spr->height);
			img->base.height = 128;
		}

		img->base.x = txt->base.x + (TEXTURE_ENTRY_W - img->base.width) / 2;
		img->base.y = txt->base.y + (TEXTURE_ENTRY_W - img->base.height) / 2;

		img->coord.s[0] = 0.0f;
		img->coord.s[1] = (float)spr->width / (float)spr->stride;

		img->coord.t[0] = 0.0f;
		img->coord.t[1] = 1.0f;

		i++;
	}

	texsel_need_more = 0;

	if(i >= TEXTURE_LIST_MAX)
	{
		for(; tdx < top; tdx++)
		{
			thing_def_t *ti = thing_info + tdx;
			thing_sprite_t *spr = x16_thing_sprite + tdx;
			glui_image_t *img;
			glui_text_t *txt;

			if(spr->width)
			{
				texsel_need_more = 1;
				break;
			}
		}
	}

	ui_file_select.elements[1]->container.count = i * 2;
}

static void blocking_select_fill()
{
	uint8_t text[32];
	uint32_t len = NUM_BLOCKBITS;

	if(blocksel_is_wall < 0)
		len--;

	ui_block_bit_btns.elements[NUM_BLOCKBITS-1]->base.disabled = blocksel_is_wall < 0;

	for(uint32_t i = 0; i < NUM_BLOCKBITS; i++)
	{
		const uint8_t *name = blocking_name[i];

		if(blocksel_is_wall && i >= NUM_BLOCKBITS - 1)
			name = "use";

		sprintf(text, "%s: %s", name, blocksel_bits & (1 << i) ? "ON" : "off");
		text[0] &= ~0x20;
		glui_set_text(&ui_block_bit_btns.elements[i]->text, text, glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
	}
}

static void file_select_fill(uint32_t is_root)
{
	uint32_t count = 0;
	glui_text_t *text = &ui_file_select.elements[0]->container.elements[0]->text;
	DIR *d;

	if(!is_root)
	{
		file_names[0].name[0] = '.';
		file_names[0].name[1] = '.';
		file_names[0].name[2] = 0;
		file_names[0].nlen = 2;
		file_names[0].isdir = 2;

		count = 1;
	}

	d = opendir(filesel_path);
	if(d)
	{
		struct dirent *ent;

		while(count < FILE_LIST_MAX)
		{
			uint32_t len;
			uint32_t sort;

			ent = readdir(d);
			if(!ent)
				break;

			if(ent->d_name[0] == '.')
			{
				if(ent->d_name[1] == 0)
					continue;
				if(ent->d_name[1] == '.' && ent->d_name[2] == 0)
					continue;
			}

			len = strlen(ent->d_name);
			if(len >= MAX_FILE_NAME)
				continue;

			if(filesel_pos + len >= EDIT_MAX_FILE_PATH - 1)
				continue;

			if(ent->d_type != DT_DIR)
			{
				if(len + 1 < filesel_slen)
					continue;

				if(strcmp(ent->d_name + len - filesel_slen, filesel_suffix))
					continue;

				for(sort = !is_root; sort < count; sort++)
				{
					if(!file_names[sort].isdir)
						break;
				}

				for( ; sort < count; sort++)
				{
					if(strcmp(ent->d_name, file_names[sort].name) < 0)
						break;
				}
			} else
			for(sort = !is_root ; sort < count; sort++)
			{
				if(!file_names[sort].isdir)
					break;
				if(strcmp(ent->d_name, file_names[sort].name) < 0)
					break;
			}

			if(sort < count)
			{
				for(uint32_t i = count; i > sort; i--)
					file_names[i] = file_names[i-1];
			} else
				sort = count;

			strcpy(file_names[sort].name, ent->d_name);
			file_names[sort].nlen = len;
			file_names[sort].isdir = ent->d_type == DT_DIR;

			count++;
		}
	}

	for(uint32_t i = 0; i < count; i++)
	{
		text->base.custom = i;
		text->base.click = file_select_click;
		glui_set_text(text, file_names[i].name, glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);

		if(file_names[i].isdir)
		{
			text->color[0] = 0xFF00BBBB;
			text->color[1] = 0xFF00FFFF;
		} else
		{
			text->color[0] = 0xFFAAAAAA;
			text->color[1] = 0xFFFFFFFF;
		}

		text++;
	}

	ui_file_select.elements[0]->container.count = count;
}

static void action_select_fill(uint32_t is_root)
{
	glui_text_t *text = &ui_file_select.elements[0]->container.elements[0]->text;
	const state_action_def_t *sa = state_action_def;

	while(1)
	{
		if(!sa->name)
			break;

		if(sa->flags & filesel_root)
		{
			glui_set_text(text, sa->name, glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = action_select_click;
			text->base.custom = sa - state_action_def;
			text->color[0] = 0xFFAAAAAA;
			text->color[1] = 0xFFFFFFFF;
			text++;
		}

		sa++;
	}

	ui_file_select.elements[0]->container.count = text - &ui_file_select.elements[0]->container.elements[0]->text;
}

static void group_select_fill(uint32_t grp)
{
	glui_text_t *text = &ui_file_select.elements[0]->container.elements[0]->text;

	if(!grp)
	{
		grpsel_grp = 0;

		glui_set_text(&ui_file_select_path, "Select main group.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

		if(grpsel_flags & EDIT_GRPSEL_FLAG_ALLOW_MODIFY)
		{
			glui_set_text(text, "Create Group", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_create_main;
			text->color[0] = 0xFF00BB00;
			text->color[1] = 0xFF00FF11;
			text++;
		}

		if(grpsel_flags & EDIT_GRPSEL_FLAG_ENABLE_SPECIAL)
		{
			glui_set_text(text, "[ everything ]", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_show_all;
			text->color[0] = 0xFF00BBBB;
			text->color[1] = 0xFF00FFFF;
			text++;

			glui_set_text(text, "[ camera sector ]", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_show_current;
			text->color[0] = 0xFF00BBBB;
			text->color[1] = 0xFF00FFFF;
			text++;
		}

		if(grpsel_flags & EDIT_GRPSEL_FLAG_ENABLE_NONE)
		{
			glui_set_text(text, "[ none ]", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_subgroup_now;
			text->base.custom = 0;
			text->color[0] = 0xFF00BBBB;
			text->color[1] = 0xFF00FFFF;
			text++;
		}

		for(uint32_t i = 1; i < edit_num_groups; i++)
		{
			glui_set_text(text, edit_group[i].name, glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_group;
			text->base.custom = i;
			text->color[0] = 0xFFAAAAAA;
			text->color[1] = 0xFFFFFFFF;
			text++;
		}
	} else
	{
		glui_set_text(&ui_file_select_path, "Select subgroup.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

		if(grpsel_flags & EDIT_GRPSEL_FLAG_ALLOW_MODIFY)
		{
			glui_set_text(text, "Delete Group", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_delete;
			text->color[0] = 0xFF0000BB;
			text->color[1] = 0xFF0011FF;
			text++;

			glui_set_text(text, "Rename Group", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_rename;
			text->color[0] = 0xFFBBAA00;
			text->color[1] = 0xFFFFEE11;
			text++;

			glui_set_text(text, "Create Subgroup", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_create_sub;
			text->color[0] = 0xFF00BB00;
			text->color[1] = 0xFF00FF11;
			text++;
		}

		glui_set_text(text, "..", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
		text->base.click = group_select_return;
		text->color[0] = 0xFF00BBBB;
		text->color[1] = 0xFF00FFFF;
		text++;

		for(uint32_t i = 0; i < edit_group[grp].count; i++)
		{
			glui_set_text(text, edit_group[grp].sub[i].name, glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
			text->base.click = group_select_subgroup;
			text->base.custom = i;
			text->color[0] = 0xFFAAAAAA;
			text->color[1] = 0xFFFFFFFF;
			text++;
		}
	}

	ui_file_select.elements[0]->container.count = text - &ui_file_select.elements[0]->container.elements[0]->text;
}

static void edit_text_input(uint8_t *text, uint32_t control)
{
	if(textentry_cb)
	{
		if(!text)
		{
			switch(control)
			{
				case 0:
					uin_textentry_ok(NULL, 0, 0);
				return;
				case 1:
					uin_textentry_ko(NULL, 0, 0);
				return;
				case 2:
					if(textentry_idx)
					{
						textentry_idx--;
						text_entry[textentry_idx] = TEXT_ENTRY_BLINK_CHAR;
						text_entry[textentry_idx+1] = 0;
					}
				break;
			}
		} else
		{
			while(*text)
			{
				if(!(*text & 0x80) && textentry_idx < textentry_limit - 1)
				{
					text_entry[textentry_idx] = *text;
					textentry_idx++;
					text_entry[textentry_idx] = TEXT_ENTRY_BLINK_CHAR;
					text_entry[textentry_idx+1] = 0;
				}
				text++;
			}
		}

		textentry_blink = TEXT_ENTRY_BLINK_RATE;
		glui_set_text(&ui_textentry_text, text_entry, glui_font_medium_kfn, GLUI_ALIGN_TOP_LEFT);
	}
}

static int32_t common_select_input(glui_element_t *elm, uint32_t magic)
{
	switch(magic)
	{
		case EDIT_INPUT_ESCAPE:
			ui_file_select.base.disabled = 1;
			if(	edit_x16_mode == EDIT_XMODE_MAP &&
				mode_2d3d == EDIT_MODE_3D
			){
				input_analog_h = 0;
				input_analog_v = 0;
				system_mouse_grab(1);
			}
			if(edit_x16_mode == EDIT_XMODE_THG)
				x16t_update_thing_view(0);
		return 1;
		case EDIT_INPUT_UP:
			if(texsel_start)
				texsel_start -= TEXTURE_LIST_W;
			common_sel_fill_func();
		return 1;
		case EDIT_INPUT_DOWN:
			if(texsel_need_more)
			{
				texsel_start += TEXTURE_LIST_W;
				common_sel_fill_func();
			}
		return 1;
	}

	return 1;
}

int32_t uin_block_bits_save(glui_element_t *elm, int32_t x, int32_t y)
{
	*selector_dest.blocking = blocksel_bits;

	if(blocksel_is_wall)
	{
		kge_line_t *line;
		line = e2d_find_other_line(edit_hit.line->backsector, edit_hit.extra_sector, edit_hit.line);
		if(line)
		{
			if(blocksel_is_wall < 0)
				line->info.blockmid = edit_hit.line->info.blockmid;
			else
				line->info.blocking = edit_hit.line->info.blocking;
		}
	}

	uin_block_bits_input(elm, EDIT_INPUT_ESCAPE);
	return 1;
}

int32_t uin_block_bit_tgl(glui_element_t *elm, int32_t x, int32_t y)
{
	uint8_t bit = 1 << elm->base.custom;
	blocksel_bits ^= bit;
	blocking_select_fill();
	return 1;
}

static int32_t thing_select_click(glui_element_t *elm, int32_t x, int32_t y)
{
	ui_file_select.base.disabled = 1;

	if(edit_x16_mode == EDIT_XMODE_THG)
	{
		x16t_thing_select(elm->base.custom);
		return 1;
	}

	selector_dest.thing->prop.type = elm->base.custom;

	edit_update_thing_type(selector_dest.thing);

	if(mode_2d3d == EDIT_MODE_3D)
	{
		input_analog_h = 0;
		input_analog_v = 0;
		system_mouse_grab(1);
	}

	return 1;
}

static int32_t frame_select_click(glui_element_t *elm, int32_t x, int32_t y)
{
	uint8_t frame = *selector_dest.frame;

	frame &= 0x80;
	frame |= elm->base.custom;

	*selector_dest.frame = frame;

	ui_file_select.base.disabled = 1;
	x16t_update_thing_view(0);

	return 1;
}

static int32_t sprite_select_click(glui_element_t *elm, int32_t x, int32_t y)
{
	editor_sprlink_t *sl;

	if(elm->base.custom >= 0)
	{
		sl = editor_sprlink + elm->base.custom;
		*selector_dest.hash = sl->hash;
	} else
		*selector_dest.hash = 0;

	ui_file_select.base.disabled = 1;
	x16t_update_thing_view(0);

	return 1;
}

static int32_t light_select_click(glui_element_t *elm, int32_t x, int32_t y)
{
	editor_light_t *el = editor_light + elm->base.custom;

	selector_dest.light->idx = elm->base.custom;
	strcpy(selector_dest.light->name, el->name);

	ui_file_select.base.disabled = 1;

	if(mode_2d3d == EDIT_MODE_3D)
	{
		input_analog_h = 0;
		input_analog_v = 0;
		system_mouse_grab(1);
	}

	return 1;
}

static int32_t palette_select_click(glui_element_t *elm, int32_t x, int32_t y)
{
	*selector_dest.palidx = elm->base.custom;

	ui_file_select.base.disabled = 1;

	if(mode_2d3d == EDIT_MODE_3D)
	{
		input_analog_h = 0;
		input_analog_v = 0;
		system_mouse_grab(1);
	}

	return 1;
}

static int32_t texture_select_click(glui_element_t *elm, int32_t x, int32_t y)
{
	editor_texture_t *et = editor_texture + elm->base.custom;

	selector_dest.texture->idx = elm->base.custom;
	strcpy(selector_dest.texture->name, et->name);

	ui_file_select.base.disabled = 1;

	if(mode_2d3d == EDIT_MODE_3D)
	{
		input_analog_h = 0;
		input_analog_v = 0;
		system_mouse_grab(1);
	}

	return 1;
}

static int32_t file_select_input(glui_element_t *elm, uint32_t magic)
{
	if(magic != EDIT_INPUT_ESCAPE)
		return 1;

	ui_file_select.base.disabled = 1;

	return 1;
}

static int32_t action_select_click(glui_element_t *elm, int32_t x, int32_t y)
{
	uint32_t idx = elm->base.custom;

	if(selector_dest.state->action != idx)
	{
		const state_action_def_t *sd = state_action_def + idx;
		selector_dest.state->action = idx;
		selector_dest.state->arg[0] = sd->arg[0].def;
		selector_dest.state->arg[1] = sd->arg[1].def;
		selector_dest.state->arg[2] = sd->arg[2].def;
	}

	ui_file_select.base.disabled = 1;

	x16t_update_thing_view(0);

	return 1;
}

static int32_t file_select_click(glui_element_t *elm, int32_t x, int32_t y)
{
	uint32_t idx = elm->base.custom;

	if(file_names[idx].isdir)
	{
		if(file_names[idx].isdir > 1)
		{
			while(1)
			{
				filesel_pos--;
				if(filesel_path[filesel_pos-1] == PATH_SPLIT_CHAR)
					break;
			}
			filesel_path[filesel_pos] = 0;
		} else
			filesel_pos += sprintf(filesel_path + filesel_pos, PATH_DIR_FMTSTR, file_names[idx].name);

		glui_set_text(&ui_file_select_path, filesel_path, glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

		file_select_fill(filesel_pos <= filesel_root);
	} else
	{
		strcpy(filesel_path + filesel_pos, file_names[idx].name);
		ui_file_select.base.disabled = 1;
		filesel_cb(filesel_path);
	}

	return 1;
}

static int32_t group_select_show_all(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_grp_show = 0;
	edit_subgrp_show = 0;
	ui_file_select.base.disabled = 1;
	return 1;
}

static int32_t group_select_show_current(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_grp_show = 0;
	edit_subgrp_show = 1;
	ui_file_select.base.disabled = 1;
	return 1;
}

static void te_group_create(uint8_t *text)
{
	int32_t value;
	edit_group_t *eg;

	if(!text)
		return;

	if(!text[0])
		return;

	for(uint32_t i = 0; i < edit_num_groups; i++)
	{
		if(!strcmp(text, edit_group[i].name))
		{
			edit_status_printf("This group already exists!");
			return;
		}
	}

	eg = edit_group + edit_num_groups;
	strcpy(eg->name, text);
	eg->count = 1;

	edit_num_groups++;

	group_select_fill(0);
}

static void te_group_rename(uint8_t *text)
{
	int32_t value;
	edit_group_t *eg;

	if(!text)
		return;

	if(!text[0])
		return;

	for(uint32_t i = 0; i < edit_num_groups; i++)
	{
		if(!strcmp(text, edit_group[i].name))
		{
			edit_status_printf("This name is already used!");
			return;
		}
	}

	eg = edit_group + grpsel_grp;
	strcpy(eg->name, text);

	group_select_fill(grpsel_grp);
}

static void te_group_create_sub(uint8_t *text)
{
	int32_t value;
	edit_group_t *eg;

	if(!text)
		return;

	if(!text[0])
		return;

	eg = edit_group + grpsel_grp;

	for(uint32_t i = 0; i < eg->count; i++)
	{
		if(!strcmp(text, eg->sub[i].name))
		{
			edit_status_printf("This subgroup already exists!");
			return;
		}
	}

	strcpy(eg->sub[eg->count].name, text);
	eg->count++;

	group_select_fill(grpsel_grp);
}

static void te_group_rename_sub(uint8_t *text)
{
	int32_t value;
	edit_group_t *eg;

	if(!text)
		return;

	if(!text[0])
		return;

	eg = edit_group + grpsel_grp;

	for(uint32_t i = 0; i < eg->count; i++)
	{
		if(!strcmp(text, eg->sub[i].name))
		{
			edit_status_printf("This name is already used!");
			return;
		}
	}

	strcpy(eg->sub[grpsel_sub].name, text);

	group_select_fill(grpsel_grp);
}

static void qe_delete_group(uint32_t res)
{
	link_entry_t *ent;

	if(!res)
		return;

	edit_num_groups--;

	memcpy(edit_group + grpsel_grp, edit_group + grpsel_grp + 1, edit_num_groups * sizeof(edit_group_t));

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(sec->stuff.group[0] == grpsel_grp)
			sec->stuff.group[0] = 0;
		else
		if(sec->stuff.group[0] > grpsel_grp)
			sec->stuff.group[0]--;

		ent = ent->next;
	}

	edit_grp_show = 0;
	edit_subgrp_show = 0;
	edit_grp_last = 0;
	edit_subgrp_last = 0;

	group_select_fill(0);
}

static void qe_delete_group_sub(uint32_t res)
{
	link_entry_t *ent;
	edit_group_t *eg = edit_group + grpsel_grp;

	if(!res)
		return;

	eg->count--;

	memcpy(eg->sub + grpsel_sub, eg->sub + grpsel_sub + 1, edit_num_groups * sizeof(edit_subgroup_t));

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(sec->stuff.group[0] == grpsel_grp)
		{
			if(sec->stuff.group[1] == grpsel_sub)
				sec->stuff.group[1] = 0;
			else
			if(sec->stuff.group[1] > grpsel_sub)
				sec->stuff.group[1]--;
		}

		ent = ent->next;
	}

	edit_grp_show = 0;
	edit_subgrp_show = 0;
	edit_grp_last = 0;
	edit_subgrp_last = 0;

	group_select_fill(grpsel_grp);
}

static int32_t group_select_create_main(glui_element_t *elm, int32_t x, int32_t y)
{
	if(edit_num_groups >= EDIT_MAX_GROUPS)
	{
		edit_status_printf("Too many groups!");
		return 1;
	}

	edit_ui_textentry("Enter group name.", EDIT_MAX_GROUP_NAME, te_group_create);

	return 1;
}

static int32_t group_select_group(glui_element_t *elm, int32_t x, int32_t y)
{
	grpsel_grp = elm->base.custom;
	group_select_fill(grpsel_grp);
	return 1;
}

static int32_t group_select_delete(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_question("Delete group?", "Do you really want to delete this group?\nAll subgroups will be lost!\nThis operation is irreversible!", qe_delete_group);
	return 1;
}

static int32_t group_select_rename(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_textentry("Enter group name.", EDIT_MAX_GROUP_NAME, te_group_rename);
	return 1;
}

static int32_t group_select_create_sub(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_group_t *eg = edit_group + grpsel_grp;

	if(eg->count >= EDIT_MAX_SUBGROUPS)
	{
		edit_status_printf("Too many subgroups!");
		return 1;
	}

	edit_ui_textentry("Enter subgroup name.", EDIT_MAX_GROUP_NAME, te_group_create_sub);

	return 1;
}

static int32_t group_select_return(glui_element_t *elm, int32_t x, int32_t y)
{
	group_select_fill(0);
	return 1;
}

static int32_t group_select_delete_sub(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_question("Delete subgroup?", "Do you really want to delete this subgroup?\nThis operation is irreversible!", qe_delete_group_sub);
	return 1;
}

static int32_t group_select_rename_sub(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_textentry("Enter subgroup name.", EDIT_MAX_GROUP_NAME, te_group_rename_sub);
	return 1;
}

static int32_t group_select_return_sub(glui_element_t *elm, int32_t x, int32_t y)
{
	group_select_fill(grpsel_grp);
	return 1;
}

static int32_t group_select_subgroup_now(glui_element_t *elm, int32_t x, int32_t y)
{
	ui_file_select.base.disabled = 1;
	grpsel_cb(elm->base.custom | (grpsel_grp << 8));
	return 1;
}

static int32_t group_select_subgroup(glui_element_t *elm, int32_t x, int32_t y)
{
	if(	elm->base.custom &&
		!input_shift &&
		grpsel_flags & EDIT_GRPSEL_FLAG_ALLOW_MODIFY
	){
		glui_text_t *text = &ui_file_select.elements[0]->container.elements[0]->text;

		glui_set_text(text, "Delete Subgroup", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
		text->base.click = group_select_delete_sub;
		text->color[0] = 0xFF0000BB;
		text->color[1] = 0xFF0011FF;
		text++;

		glui_set_text(text, "Rename Subgroup", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
		text->base.click = group_select_rename_sub;
		text->color[0] = 0xFFBBAA00;
		text->color[1] = 0xFFFFEE11;
		text++;

		glui_set_text(text, "Select Subgroup", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
		text->base.click = group_select_subgroup_now;
		text->base.custom = elm->base.custom;
		text->color[0] = 0xFF00BB00;
		text->color[1] = 0xFF00FF11;
		text++;

		glui_set_text(text, "..", glui_font_small_kfn, GLUI_ALIGN_CENTER_CENTER);
		text->base.click = group_select_return_sub;
		text->color[0] = 0xFF00BBBB;
		text->color[1] = 0xFF00FFFF;
		text++;

		ui_file_select.elements[0]->container.count = text - &ui_file_select.elements[0]->container.elements[0]->text;

		grpsel_sub = elm->base.custom;

		return 1;
	}

	return group_select_subgroup_now(elm, x, y);
}

static void clear_sectors()
{
	link_entry_t *ent = edit_list_sector.top;

	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);
		list_clear(&sec->objects);
		ent = ent->next;
	}

	list_clear(&edit_list_sector);
}

//
// map loading

static int32_t cbor_line_vtx(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(ctx->index >= 2)
		return 1;

	if(type != KGCBOR_TYPE_VALUE)
		return 1;

	load_line->vertex[!ctx->index] = (kge_vertex_t*)(intptr_t)value->u32;

	return 1;
}

static int32_t cbor_line_top(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		load_line->texture[0] = load_linetex;
		return 1;
	}
	return edit_cbor_branch(cbor_linetex, type, key, ctx->key_len, value, ctx->val_len) == NULL;
}

static int32_t cbor_line_bot(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		load_line->texture[1] = load_linetex;
		return 1;
	}
	return edit_cbor_branch(cbor_linetex, type, key, ctx->key_len, value, ctx->val_len) == NULL;
}

static int32_t cbor_line_mid(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		load_line->texture[2] = load_linetex;
		return 1;
	}
	return edit_cbor_branch(cbor_linetex, type, key, ctx->key_len, value, ctx->val_len) == NULL;
}

static int32_t cbor_sector_group(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(ctx->index >= 2)
		return 1;

	if(type != KGCBOR_TYPE_VALUE)
		return 1;

	load_sector->stuff.group[ctx->index] = value->u8;

	return 1;
}

static int32_t cbor_sector_ceiling(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		load_sector->plane[PLANE_TOP] = load_secplane;
		return 1;
	}
	return edit_cbor_branch(cbor_secplane, type, key, ctx->key_len, value, ctx->val_len) == NULL;
}

static int32_t cbor_sector_floor(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		load_sector->plane[PLANE_BOT] = load_secplane;
		return 1;
	}
	return edit_cbor_branch(cbor_secplane, type, key, ctx->key_len, value, ctx->val_len) == NULL;
}

static int32_t cbor_sector_line_add(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func = edit_cbor_branch(cbor_line, type, key, ctx->key_len, value, ctx->val_len);

	if(func)
	{
		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

static int32_t cbor_sector_lines(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
		return 1;

	if(ctx->count > EDIT_MAX_SECTOR_LINES)
		return -1;

	if(ctx->index >= EDIT_MAX_SECTOR_LINES)
		return -1;

	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		load_sector->line_count = ctx->count;

		if(load_backsector < 0)
		{
			load_lblock = 0;
			load_mblock = 0;
		}

		load_line->backsector = (kge_sector_t*)(intptr_t)load_backsector;
		load_line->texture_split = load_split;
		load_line->info.blocking = load_lblock;
		load_line->info.blockmid = load_mblock;
		load_line->info.flags = load_lnflags;

		return 1;
	}

	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	load_line = load_sector->line + ctx->count - ctx->index - 1; // reverse order

	load_backsector = -1;
	load_split = INFINITY;
	load_lnflags = 0;
	load_lblock = 0;
	load_mblock = 0;

	ctx->entry_cb = cbor_sector_line_add;

	return 0;
}

static int32_t cbor_object_line(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func;

	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		load_line->info.flags = load_lnflags;
		load_line->texture[0] = load_linetex;
		load_line->frontsector = load_sector;
		load_line->object = load_object;
		load_line->texture_split = INFINITY;

		return 1;
	}

	func = edit_cbor_branch(cbor_objline, type, key, ctx->key_len, value, ctx->val_len);
	if(func)
	{
		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

static int32_t cbor_sector_object(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	if(ctx->count > EDIT_MAX_SOBJ_LINES)
		return -1;

	if(ctx->index >= EDIT_MAX_SOBJ_LINES)
		return -1;

	load_lnflags = 0;

	cbor_objline[CBOR_OBJLINE_X].f32 = &load_object->vtx[ctx->index].x;
	cbor_objline[CBOR_OBJLINE_Y].f32 = &load_object->vtx[ctx->index].y;

	load_line = load_object->line + ctx->index;

	load_object->count = ctx->index + 1;

	ctx->entry_cb = cbor_object_line;

	return 0;
}

static int32_t cbor_sector_objects(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
		return 1;

	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		if(!load_object->count)
			return -1;

		load_object = NULL;

		return 1;
	}

	if(type == KGCBOR_TYPE_ARRAY)
	{
		if(load_sector->objects.count > MAX_SECTOR_OBJECTS)
			return -1;

		load_object = list_add_entry(&load_sector->objects, sizeof(edit_sec_obj_t));
		memset(load_object, 0, sizeof(edit_sec_obj_t));

		ctx->entry_cb = cbor_sector_object;
	}

	return 0;
}

static int32_t cbor_map_subgroup(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	uint32_t idx = ctx->index + 1;

	if(type != KGCBOR_TYPE_STRING)
		return 1;

	if(idx >= EDIT_MAX_SUBGROUPS)
		return 1;

	load_group->count = idx + 1;

	if(ctx->val_len >= EDIT_MAX_GROUP_NAME)
		return 1;

	memcpy(load_group->sub[idx].name, value->ptr, ctx->val_len);
	load_group->sub[idx].name[ctx->val_len] = 0;

	return 0;
}

static int32_t cbor_map_groups(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	uint32_t idx = ctx->index + 1;

	if(type != KGCBOR_TYPE_ARRAY)
		return 1;

	if(idx >= EDIT_MAX_GROUPS)
		return 1;

	edit_num_groups = idx + 1;

	if(ctx->key_len >= EDIT_MAX_GROUP_NAME)
		return 1;

	load_group = edit_group + idx;

	memcpy(load_group->name, key, ctx->key_len);
	load_group->name[ctx->key_len] = 0;

	ctx->entry_cb = cbor_map_subgroup;

	return 0;
}

static int32_t cbor_map_camera(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	return edit_cbor_branch(cbor_camera, type, key, ctx->key_len, value, ctx->val_len) == NULL;
}

static int32_t cbor_map_vtx_add(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	return edit_cbor_branch(cbor_vtx, type, key, ctx->key_len, value, ctx->val_len) == NULL;
}

static int32_t cbor_map_vertexes(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(	!ctx->index &&
		type != KGCBOR_TYPE_TERMINATOR &&
		type != KGCBOR_TYPE_TERMINATOR_CB
	)
		list_clear(&edit_list_vertex);

	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		kge_vertex_t *vtx;

		vtx = list_add_entry(&edit_list_vertex, sizeof(kge_vertex_t));
		*vtx = load_vtx;
		vtx->vc_editor = 0;

		return 1;
	}

	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	memset(&load_vtx, 0, sizeof(load_vtx));
	ctx->entry_cb = cbor_map_vtx_add;

	return 0;
}

static int32_t cbor_map_sec_add(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func;

	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		memcpy(load_sector->light.name, load_seclight, MAX_X16_MAPSTRING);
		load_sector->stuff.palette = load_secpalidx;
		load_sector->stuff.x16flags = load_sector_flags;
		return 1;
	}

	func = edit_cbor_branch(cbor_sector, type, key, ctx->key_len, value, ctx->val_len);
	if(func)
	{
		if(func == cbor_sector_ceiling)
			load_secplane = edit_ceiling_plane;
		else
		if(func == cbor_sector_floor)
			load_secplane = edit_floor_plane;
		load_secplane.link = (void*)-1;

		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

static int32_t cbor_map_sectors(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(	!ctx->index &&
		type != KGCBOR_TYPE_TERMINATOR &&
		type != KGCBOR_TYPE_TERMINATOR_CB
	)
		clear_sectors();

	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	if(edit_list_sector.count >= 255)
		return -1;

	load_sector = list_add_entry(&edit_list_sector, sizeof(kge_sector_t) + sizeof(kge_line_t) * EDIT_MAX_SECTOR_LINES);

	memset(load_sector, 0, sizeof(kge_sector_t) + sizeof(kge_line_t) * EDIT_MAX_SECTOR_LINES);

	strcpy(load_sector->light.name, editor_light[0].name);
	load_sector->plane[PLANE_TOP] = edit_ceiling_plane;
	load_sector->plane[PLANE_BOT] = edit_floor_plane;

	ctx->entry_cb = cbor_map_sec_add;

	return 0;
}

static int32_t cbor_map_thing_add(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		strcpy(new_thing->prop.name, load_thing.prop.name);
		new_thing->pos = load_thing.pos;
		return 1;
	}

	edit_cbor_branch(cbor_thing, type, key, ctx->key_len, value, ctx->val_len);

	return 1;
}

static int32_t cbor_map_things(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(	!ctx->index &&
		type != KGCBOR_TYPE_TERMINATOR &&
		type != KGCBOR_TYPE_TERMINATOR_CB
	)
		tick_cleanup();

	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	new_thing = thing_spawn(0, 0, 0, NULL);
	if(!new_thing)
		return 1;

	x16g_set_sprite_texture(new_thing);

	memset(&load_thing, 0, sizeof(load_thing));

	ctx->entry_cb = cbor_map_thing_add;

	return 0;
}


static int32_t cbor_map_root(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
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
// map menu

static void update_map_options(uint32_t update_sky_name)
{
	uint8_t text[64];

	if(edit_sky_num >= 0)
	{
		editor_sky_t *es = editor_sky + edit_sky_num;

		x16g_set_texture(X16G_GLTEX_SHOW_TEXTURE, X16_SKY_WIDTH, X16_SKY_HEIGHT, GL_LUMINANCE, es->data);
		ui_map_opt_sky_texture.gltex = &x16_gl_tex[X16G_GLTEX_SHOW_TEXTURE];

		if(update_sky_name)
			strcpy(x16_sky_name, es->name);

		sprintf(text, "Sky: %s", es->name);
	} else
	{
		ui_map_opt_sky_texture.gltex = NULL;

		if(update_sky_name)
			x16_sky_name[0] = '\t';

		if(x16_sky_name[0] != '\t')
			sprintf(text, "Sky: %s (%s)", "\t", x16_sky_name);
		else
			sprintf(text, "Sky: %s", "\t");
	}
	glui_set_text(&ui_map_opt_sky_name, text, glui_font_small_kfn, GLUI_ALIGN_CENTER);

	if(x16e_enable_logo)
	{
		x16g_set_texture(X16G_GLTEX_SHOW_TEXTURE_ALT, 160, 120, GL_LUMINANCE, x16e_logo_data);
		ui_map_opt_load_image.gltex = &x16_gl_tex[X16G_GLTEX_SHOW_TEXTURE_ALT];
	} else
		ui_map_opt_load_image.gltex = NULL;
}

//
// inputs

int32_t uin_textentry_ok(glui_element_t *elm, int32_t x, int32_t y)
{
	void (*cb)(uint8_t*);

	system_text = NULL;

	cb = textentry_cb;
	textentry_cb = NULL;

	ui_textentry.base.disabled = 1;

	text_entry[textentry_idx] = 0;
	cb(text_entry);

	return 1;
}

int32_t uin_textentry_ko(glui_element_t *elm, int32_t x, int32_t y)
{
	void (*cb)(uint8_t*);

	system_text = NULL;

	cb = textentry_cb;
	textentry_cb = NULL;

	ui_textentry.base.disabled = 1;

	cb(NULL);

	return 1;
}

int32_t uin_question_yes(glui_element_t *elm, int32_t x, int32_t y)
{
	ui_question.base.disabled = 1;
	question_cb(1);
	return 1;
}

int32_t uin_question_no(glui_element_t *elm, int32_t x, int32_t y)
{
	ui_question.base.disabled = 1;
	question_cb(0);
	return 1;
}

void qe_new_map(uint32_t res)
{
	if(!res)
		return;

	edit_new_map();
	edit_spawn_camera();

	ui_main_menu.base.disabled = 1;
}

void qe_new_gfx(uint32_t res)
{
	if(!res)
		return;
	x16g_new_gfx();
	ui_main_menu.base.disabled = 1;
}

void qe_new_thg(uint32_t res)
{
	if(!res)
		return;
	x16t_new_thg();
	ui_main_menu.base.disabled = 1;
}

void qe_exit(uint32_t res)
{
	if(!res)
		return;
	ui_main_menu.base.disabled = 1;
	system_stop(0);
}

static void fs_load_image(uint8_t *file)
{
	image_t *img;
	uint32_t *src;
	uint8_t *dst;

	img = img_png_load(file, 0);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(img->width != 160 || img->height != 120)
	{
		free(img);
		edit_status_printf("Invalid image resolution!");
		return;
	}

	src = (uint32_t*)img->data;
	dst = x16e_logo_data;

	for(uint32_t i = 0; i < 160 * 120; i++)
		*dst++ = x16g_palette_match(*src++ | 0xFF000000, 0);

	free(img);

	x16e_enable_logo = 1;
	update_map_options(0);
}

void group_select_apply_filter(uint32_t magic)
{
	edit_grp_show = magic >> 8;
	edit_subgrp_show = magic & 0xFF;
}

int32_t uin_menu_new(glui_element_t *elm, int32_t x, int32_t y)
{
	switch(edit_x16_mode)
	{
		case EDIT_XMODE_MAP:
			edit_ui_question("Start a new map?", "Do you really want to start a new map?\nAny unsaved progress will be lost!", qe_new_map);
		break;
		case EDIT_XMODE_GFX:
			edit_ui_question("Clear all graphics?", "Do you really want to clear all graphics?\nAny unsaved progress will be lost!", qe_new_gfx);
		break;
		case EDIT_XMODE_THG:
			edit_ui_question("Clear all things?", "Do you really want to clear all things?\nAny unsaved progress will be lost!", qe_new_thg);
		break;
	}
	return 1;
}

int32_t uin_menu_load(glui_element_t *elm, int32_t x, int32_t y)
{
	return 1;
}

int32_t uin_menu_save(glui_element_t *elm, int32_t x, int32_t y)
{
	const uint8_t *what;
	const uint8_t *where;

	switch(edit_x16_mode)
	{
		case EDIT_XMODE_MAP:
			what = "Map";
			where = edit_map_save(NULL);
		break;
		case EDIT_XMODE_GFX:
			what = "GFX";
			where = x16g_save(NULL);
		break;
		case EDIT_XMODE_THG:
			what = "Things";
			where = x16t_save(NULL);
		break;
	}

	ui_main_menu.base.disabled = 1;

	if(!where)
		edit_status_printf("%s saving failed!", what);
	else
		edit_status_printf("%s saved as %s.", what, where);

	return 1;
}

int32_t uin_menu_save_as(glui_element_t *elm, int32_t x, int32_t y)
{
	return 1;
}

int32_t uin_menu_export(glui_element_t *elm, int32_t x, int32_t y)
{
	ui_main_menu.base.disabled = 1;

	switch(edit_x16_mode)
	{
		case EDIT_XMODE_MAP:
			x16_export_map();
		break;
		case EDIT_XMODE_GFX:
			x16g_export();
		break;
		case EDIT_XMODE_THG:
			x16t_export();
		break;
	}

	return 1;
}

int32_t uin_menu_mode(glui_element_t *elm, int32_t x, int32_t y)
{
	uint32_t slot = elm->base.custom;
	uint32_t newmode = mode_xslot[slot];

	mode_xslot[slot] = edit_x16_mode;
	glui_set_text(&elm->text, mode_name[edit_x16_mode], glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	ui_main_menu.base.disabled = 1;

	switch(edit_x16_mode)
	{
		case EDIT_XMODE_GFX:
			x16g_generate();
		case EDIT_XMODE_THG:
			x16t_generate();
		break;
	}

	x16_palette_data[0] = 0;
	x16g_update_texture(X16G_GLTEX_PALETTE);

	editor_x16_mode(newmode);

	return 1;
}

int32_t uin_menu_generate_tables(glui_element_t *elm, int32_t x, int32_t y)
{
	ui_main_menu.base.disabled = 1;
	x16r_generate();
	return 1;
}

int32_t uin_menu_exit(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_question("Exit editor?", "Do you really want to exit?\nAny unsaved progress will be lost!", qe_exit);
	return 1;
}

int32_t uin_map_options(glui_element_t *elm, int32_t x, int32_t y)
{
	update_map_options(0);
	ui_main_menu.base.disabled = 1;
	ui_map_menu.base.disabled = 0;
	return 1;
}

int32_t uin_map_opt_load_image(glui_element_t *elm, int32_t x, int32_t y)
{
	if(input_shift)
	{
		if(x16e_enable_logo)
		{
			x16e_enable_logo = 0;
			update_map_options(0);
			edit_status_printf("Map loading screen removed.");
		}
	} else
		edit_ui_file_select("Select PNG to load image from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_load_image);

	return 1;
}

int32_t uin_map_opt_grpsel(glui_element_t *elm, int32_t x, int32_t y)
{
	ui_map_menu.base.disabled = 1;
	edit_ui_group_select("Group visiblity filter.", EDIT_GRPSEL_FLAG_ENABLE_SPECIAL, group_select_apply_filter);
	return 1;
}

int32_t uin_map_opt_input(glui_element_t *elm, uint32_t magic)
{
	switch(magic)
	{
		case EDIT_INPUT_ESCAPE:
			ui_map_menu.base.disabled ^= 1;
		return 1;
		case EDIT_INPUT_UP:
			if(edit_sky_num < 0)
				edit_sky_num = x16_num_skies - 1;
			else
				edit_sky_num--;
			update_map_options(1);
		return 1;
		case EDIT_INPUT_DOWN:
			edit_sky_num++;
			if(edit_sky_num >= x16_num_skies)
				edit_sky_num = -1;
			update_map_options(1);
		return 1;
	}

	return 1;
}

int32_t uin_main_menu_input(glui_element_t *elm, uint32_t magic)
{
	if(magic != EDIT_INPUT_ESCAPE)
		return 0;

	ui_main_menu.base.disabled ^= 1;

	return 1;
}

static int32_t infunc_edit_menu_click()
{
	return glui_mouse_click((void*)&ui_editor, mouse_x, mouse_y);
}

static int32_t infunc_edit_menu()
{
	return glui_key_input((void*)&ui_editor, EDIT_INPUT_ESCAPE);
}

static int32_t infunc_edit_menu_left()
{
	return glui_key_input((void*)&ui_editor, EDIT_INPUT_LEFT);
}

static int32_t infunc_edit_menu_right()
{
	return glui_key_input((void*)&ui_editor, EDIT_INPUT_RIGHT);
}

static int32_t infunc_edit_menu_up()
{
	return glui_key_input((void*)&ui_editor, EDIT_INPUT_UP);
}

static int32_t infunc_edit_menu_down()
{
	return glui_key_input((void*)&ui_editor, EDIT_INPUT_DOWN);
}

static int32_t infunc_edit_menu_copy()
{
	return glui_key_input((void*)&ui_editor, EDIT_INPUT_COPY);
}

static int32_t infunc_edit_menu_paste()
{
	return glui_key_input((void*)&ui_editor, EDIT_INPUT_PASTE);
}

int32_t edit_infunc_menu()
{
	ui_main_menu.base.disabled = 0;
	return 1;
}

int32_t uin_block_bits_input(glui_element_t *elm, uint32_t magic)
{
	if(magic != EDIT_INPUT_ESCAPE)
		return 0;

	ui_block_bits.base.disabled = 1;

	if(edit_x16_mode == EDIT_XMODE_THG)
		x16t_update_thing_view(0);
	else
	if(mode_2d3d == EDIT_MODE_3D)
	{
		input_analog_h = 0;
		input_analog_v = 0;
		system_mouse_grab(1);
	}

	return 1;
}

//
// API

uint32_t editor_init()
{
	float angle;
	uint32_t size;
	uint32_t *tmptex;
	glui_text_t *uitext;
	void *ptr;

	edit_busy_window("Initializing editor ...");

	//
	ui_edit_highlight.base.y = screen_height - 1;

	// default mode input
	for(uint32_t i = 0; i < ui_menu_mode_btns.count; i++)
	{
		mode_xslot[i] = i + 1;
		ui_menu_mode_btns.elements[i]->base.custom = i;
		glui_set_text(&ui_menu_mode_btns.elements[i]->text, mode_name[mode_xslot[i]], glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	}

	// default map file
	sprintf(map_path, X16_PATH_MAPS PATH_SPLIT_STR "%s.map", "default");

	// empty texture stuff
	edit_ceiling_plane.texture = edit_empty_texture;
	edit_floor_plane.texture = edit_empty_texture;
	edit_line_default = edit_empty_texture;

	// prepare circle highlight
	angle = 2.0f * M_PI / (float)(EDIT_THING_CIRCLE_VTX-1);
	for(uint32_t i = 0; i < EDIT_THING_CIRCLE_VTX; i++)
	{
		float aa = M_PI + (float)i * angle;
		edit_circle_vtx[i+4].x = sinf(aa);
		edit_circle_vtx[i+4].y = cosf(aa);
	}
	memcpy(edit_circle_vtx, edit_shape_arrow, sizeof(edit_shape_arrow));

	// setup status text
	uitext = calloc(ui_edit_status.count, sizeof(glui_text_t));
	if(!uitext)
		return 1;

	tmptex = malloc(ui_edit_status.count * sizeof(uint32_t));
	if(!tmptex)
		return 1;

	glGenTextures(ui_edit_status.count, tmptex);

	for(uint32_t i = 0; i < ui_edit_status.count; i++)
	{
		glui_text_t *txt = uitext + i;

		txt->base.draw = glui_df_text;
		txt->base.x = 4;
		txt->base.y = i * STATUS_LINE_HEIGHT;
		txt->base.width = screen_width - 4;
		txt->base.height = STATUS_LINE_HEIGHT;
		txt->color[0] = 0xDDFFFFFF;
		txt->color[1] = 0xDDFFFFFF;
		txt->gltex = tmptex[i];
		ui_edit_status.elements[i] = (glui_element_t*)txt;

		glBindTexture(GL_TEXTURE_2D, tmptex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	ui_edit_status.base.y = -STATUS_LINE_HEIGHT * (int32_t)ui_edit_status.count;
	ui_edit_status.base.color[0] = 0x77000000;
	ui_edit_status.base.color[1] = 0x77000000;
	ui_edit_status.base.width = screen_width;
	ui_edit_status.base.height = STATUS_LINE_HEIGHT * ui_edit_status.count;

	free(tmptex);

	// file selector
	ptr = calloc(sizeof(glui_container_t) + FILE_LIST_MAX * (sizeof(glui_text_t*) + sizeof(glui_text_t)), 1);
	if(!ptr)
		return 1;

	tmptex = malloc(FILE_LIST_MAX * sizeof(uint32_t));
	if(!tmptex)
		return 1;

	glGenTextures(FILE_LIST_MAX, tmptex);

	ui_file_select.elements[0] = ptr;
	ui_file_select.elements[0]->base.draw = glui_df_container;
	ui_file_select.elements[0]->base.x = 1;
	ui_file_select.elements[0]->base.y = 32;
	ui_file_select.elements[0]->container.input = file_select_input;
	ptr += sizeof(glui_container_t);
	ptr += FILE_LIST_MAX * sizeof(glui_text_t*);

	for(uint32_t i = 0; i < FILE_LIST_MAX; i++)
	{
		glui_text_t *txt = ptr;
		uint32_t x = i / FILE_LIST_H;
		uint32_t y = i % FILE_LIST_H;

		txt->base.custom = i;

		txt->base.draw = glui_df_text;

		txt->base.x = x * (FILE_ENTRY_W + 1);
		txt->base.y = y * (FILE_ENTRY_H + 1);
		txt->base.width = FILE_ENTRY_W;
		txt->base.height = FILE_ENTRY_H;

		txt->base.color[0] = 0xFF000000;
		txt->base.color[1] = 0xFF303030;

		txt->gltex = tmptex[i];

		glBindTexture(GL_TEXTURE_2D, tmptex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		ui_file_select.elements[0]->container.elements[i] = (glui_element_t*)txt;

		ptr += sizeof(glui_text_t);
	}

	free(tmptex);

	// texture / sprite / light / sky selector

	ptr = calloc(sizeof(glui_container_t) + TEXTURE_LIST_MAX * (sizeof(glui_text_t*) + sizeof(glui_image_t*) + sizeof(glui_text_t) + sizeof(glui_image_t)), 1);
	if(!ptr)
		return 1;

	tmptex = malloc(TEXTURE_LIST_MAX * sizeof(uint32_t));
	if(!tmptex)
		return 1;

	glGenTextures(TEXTURE_LIST_MAX, tmptex);

	ui_file_select.elements[1] = ptr;
	ui_file_select.elements[1]->base.draw = glui_df_container;
	ui_file_select.elements[1]->base.x = 13;
	ui_file_select.elements[1]->base.y = 43;
	ptr += sizeof(glui_container_t);
	ptr += TEXTURE_LIST_MAX * (sizeof(glui_text_t*) + sizeof(glui_image_t*));

	for(uint32_t i = 0; i < TEXTURE_LIST_MAX; i++)
	{
		uint32_t x = i % TEXTURE_LIST_W;
		uint32_t y = i / TEXTURE_LIST_W;
		glui_text_t *txt;
		glui_image_t *img;

		txt = ptr;
		ptr += sizeof(glui_text_t);

		img = ptr;
		ptr += sizeof(glui_image_t);

		txt->base.draw = glui_df_text;

		txt->base.x = x * (TEXTURE_ENTRY_W + TEXTURE_ENTRY_GAP);
		txt->base.y = y * (TEXTURE_ENTRY_H + TEXTURE_ENTRY_GAP);
		txt->base.width = TEXTURE_ENTRY_W;
		txt->base.height = TEXTURE_ENTRY_H;

		txt->base.color[0] = 0xFF000000;
		txt->base.color[1] = 0xFF808080;
		txt->color[0] = 0xFFC0C0C0;
		txt->color[1] = 0xFFFFFFFF;

		txt->gltex = tmptex[i];

		glBindTexture(GL_TEXTURE_2D, tmptex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		img->base.draw = glui_df_image;
		img->base.disabled = -1;

		img->base.x = x * (TEXTURE_ENTRY_W + TEXTURE_ENTRY_GAP);
		img->base.y = y * (TEXTURE_ENTRY_H + TEXTURE_ENTRY_GAP);

		img->coord.s[0] = 0.0f;
		img->coord.s[1] = 1.0f;

		ui_file_select.elements[1]->container.elements[i * 2 + 0] = (glui_element_t*)txt;
		ui_file_select.elements[1]->container.elements[i * 2 + 1] = (glui_element_t*)img;
	}

	free(tmptex);

	// reset / default map
	edit_map_load(NULL);

	// start
	editor_x16_mode(EDIT_XMODE_MAP);

	return 0;
}

uint8_t *edit_put_blockbits(uint8_t *dst, uint8_t bits)
{
#if 0
	for(int32_t i = NUM_BLOCKBITS - 1; i >= 0; i--)
	{
		uint8_t bb;
		if(bits & (1 << i))
			bb = '1';
		else
			bb = '0';
		*dst++ = bb;
	}
#else
	*dst++ = '|';
	for(int32_t i = NUM_BLOCKBITS - 1; i >= 0; i--)
	{
		uint8_t bb = blocking_name[i][0];

		if(bits & (1 << i))
			bb &= ~0x20;
		*dst++ = bb;
		*dst++ = '|';
	}
#endif
	return dst;
}

uint32_t edit_ui_input()
{
	static uint32_t was_busy;

	if(!edit_ui_busy)
	{
		if(was_busy)
		{
			input_clear();
			was_busy = 0;
		}
		return 0;
	}

	was_busy = 1;
	edit_handle_input(infunc_edit_ui);

	return 1;
}

void edit_ui_draw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// UI elements
	memcpy(shader_buffer.projection.matrix, r_projection_ui.raw, sizeof(matrix3d_t));
	edit_draw_ui();
}

void edit_ui_tick()
{
	if(!ui_edit_status.base.disabled)
	{
		for(uint32_t i = 0; i < ui_edit_status.count; i++)
		{
			glui_text_t *txt = &ui_edit_status.elements[i]->text;

			if(!txt->base.border_color[0])
				continue;

			txt->base.border_color[0]--;
			if(txt->base.border_color[0])
				continue;

			ui_edit_status.base.y -= STATUS_LINE_HEIGHT;
			if(ui_edit_status.base.y <= -STATUS_LINE_HEIGHT * (int32_t)ui_edit_status.count)
				ui_edit_status.base.disabled = 1;
		}
	}

	if(!ui_textentry.base.disabled)
	{
		textentry_blink--;
		if(!textentry_blink)
		{
			textentry_blink = TEXT_ENTRY_BLINK_RATE;
			text_entry[textentry_idx] ^= TEXT_ENTRY_BLINK_CHAR;
			glui_set_text(&ui_textentry_text, text_entry, glui_font_medium_kfn, GLUI_ALIGN_TOP_LEFT);
		}
	}
}

void edit_ui_blocking_select(const uint8_t *title, uint8_t *dest, int32_t is_wall)
{
	selector_dest.blocking = dest;
	blocksel_bits = *dest;
	blocksel_is_wall = is_wall;
	blocking_select_fill();
	glui_set_text(&ui_block_bits_title, title, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	ui_block_bits.base.disabled = 0;
	system_mouse_grab(0);
}

void edit_ui_thing_select(kge_thing_t *dest)
{
	texsel_start = 0;
	selector_dest.thing = dest;

	ui_file_select.elements[1]->container.input = common_select_input;
	thing_select_fill();
	common_sel_fill_func = thing_select_fill;

	glui_set_text(&ui_file_select_title, "Select thing type.", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, "Use scroll wheel to browse the list.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	ui_file_select.elements[0]->base.disabled = 1;
	ui_file_select.elements[1]->base.disabled = 0;
	ui_file_select.base.disabled = 0;

	system_mouse_grab(0);
}

void edit_ui_tframe_select(uint8_t *dest, uint32_t sprite)
{
	texsel_start = 0;
	selector_sprite = x16g_spritelink_by_hash(sprite);
	selector_dest.frame = dest;

	if(selector_sprite < 0)
		return;

	ui_file_select.elements[1]->container.input = common_select_input;
	tframe_select_fill();
	common_sel_fill_func = tframe_select_fill;

	glui_set_text(&ui_file_select_title, "Select state frame.", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, "Use scroll wheel to browse the list.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	ui_file_select.elements[0]->base.disabled = 1;
	ui_file_select.elements[1]->base.disabled = 0;
	ui_file_select.base.disabled = 0;

	system_mouse_grab(0);
}

void edit_ui_wframe_select(uint8_t *dest, uint32_t sprite)
{
	texsel_start = 0;
	selector_sprite = x16g_spritelink_by_hash(sprite);
	selector_dest.frame = dest;

	if(selector_sprite < 0)
		return;

	ui_file_select.elements[1]->container.input = common_select_input;
	wframe_select_fill();
	common_sel_fill_func = wframe_select_fill;

	glui_set_text(&ui_file_select_title, "Select state frame.", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, "Use scroll wheel to browse the list.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	ui_file_select.elements[0]->base.disabled = 1;
	ui_file_select.elements[1]->base.disabled = 0;
	ui_file_select.base.disabled = 0;

	system_mouse_grab(0);
}

void edit_ui_tsprite_select(uint32_t *dest)
{
	texsel_start = 0;
	selector_dest.hash = dest;

	ui_file_select.elements[1]->container.input = common_select_input;
	tsprite_select_fill();
	common_sel_fill_func = tsprite_select_fill;

	glui_set_text(&ui_file_select_title, "Select state sprite.", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, "Use scroll wheel to browse the list.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	ui_file_select.elements[0]->base.disabled = 1;
	ui_file_select.elements[1]->base.disabled = 0;
	ui_file_select.base.disabled = 0;

	system_mouse_grab(0);
}

void edit_ui_wsprite_select(uint32_t *dest)
{
	texsel_start = 0;
	selector_dest.hash = dest;

	ui_file_select.elements[1]->container.input = common_select_input;
	wsprite_select_fill();
	common_sel_fill_func = wsprite_select_fill;

	glui_set_text(&ui_file_select_title, "Select state sprite.", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, "Use scroll wheel to browse the list.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	ui_file_select.elements[0]->base.disabled = 1;
	ui_file_select.elements[1]->base.disabled = 0;
	ui_file_select.base.disabled = 0;

	system_mouse_grab(0);
}

void edit_ui_light_select(kge_light_t *dest)
{
	texsel_start = 0;
	selector_dest.light = dest;

	ui_file_select.elements[1]->container.input = common_select_input;
	light_select_fill();
	common_sel_fill_func = light_select_fill;

	glui_set_text(&ui_file_select_title, "Select sector light.", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, "Use scroll wheel to browse the list.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	ui_file_select.elements[0]->base.disabled = 1;
	ui_file_select.elements[1]->base.disabled = 0;
	ui_file_select.base.disabled = 0;

	system_mouse_grab(0);
}

void edit_ui_palette_select(uint8_t *dest)
{
	texsel_start = 0;
	selector_dest.palidx = dest;

	ui_file_select.elements[1]->container.input = common_select_input;
	palette_select_fill();
	common_sel_fill_func = palette_select_fill;

	glui_set_text(&ui_file_select_title, "Select sector palette.", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, "Use scroll wheel to browse the list.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	ui_file_select.elements[0]->base.disabled = 1;
	ui_file_select.elements[1]->base.disabled = 0;
	ui_file_select.base.disabled = 0;

	system_mouse_grab(0);
}

void edit_ui_texture_select(const uint8_t *title, kge_x16_tex_t *dest, uint32_t mode)
{
	texsel_start = 0;
	selector_dest.texture = dest;
	texsel_mode = mode;

	ui_file_select.elements[1]->container.input = common_select_input;
	texture_select_fill();
	common_sel_fill_func = texture_select_fill;

	glui_set_text(&ui_file_select_title, title, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, "Use scroll wheel to browse the list.", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	ui_file_select.elements[0]->base.disabled = 1;
	ui_file_select.elements[1]->base.disabled = 0;
	ui_file_select.base.disabled = 0;

	system_mouse_grab(0);
}

void edit_ui_file_select(const uint8_t *title, const uint8_t *path, const uint8_t *suffix, void (*cb)(uint8_t *file))
{
	strcpy(filesel_path, path);
	strncpy(filesel_suffix, suffix, sizeof(filesel_suffix)-1);

	filesel_pos = strlen(filesel_path);
	filesel_slen = strlen(filesel_suffix);

	filesel_root = filesel_pos;
	filesel_cb = cb;

	glui_set_text(&ui_file_select_title, title, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, filesel_path, glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	file_select_fill(1);

	ui_file_select.elements[0]->base.disabled = 0;
	ui_file_select.elements[1]->base.disabled = 1;
	ui_file_select.base.disabled = 0;
}

void edit_ui_action_select(thing_st_t *st, uint32_t is_wpn)
{
	selector_dest.state = st;
	filesel_root = 1 + is_wpn;

	glui_set_text(&ui_file_select_title, "Select state action.", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_file_select_path, is_wpn ? "[weapon mode]" : "[thing mode]", glui_font_tiny_kfn, GLUI_ALIGN_TOP_CENTER);

	action_select_fill(1);

	ui_file_select.elements[0]->base.disabled = 0;
	ui_file_select.elements[1]->base.disabled = 1;
	ui_file_select.base.disabled = 0;
}

void edit_ui_group_select(const uint8_t *title, uint32_t flags, void (*cb)(uint32_t magic))
{
	grpsel_flags = flags;
	grpsel_cb = cb;

	glui_set_text(&ui_file_select_title, title, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);

	group_select_fill(0);

	ui_file_select.elements[0]->base.disabled = 0;
	ui_file_select.elements[1]->base.disabled = 1;
	ui_file_select.base.disabled = 0;
}

void edit_ui_textentry(const uint8_t *title, uint32_t limit, void (*cb)(uint8_t *text))
{
	textentry_idx = 0;
	textentry_limit = limit;
	if(textentry_limit > MAX_TEXT_ENTRY)
		textentry_limit = MAX_TEXT_ENTRY;

	textentry_blink = TEXT_ENTRY_BLINK_RATE;

	text_entry[0] = TEXT_ENTRY_BLINK_CHAR;
	text_entry[1] = 0;

	system_text = edit_text_input;
	textentry_cb = cb;

	glui_set_text(&ui_textentry_title, title, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	ui_textentry_text.width = 0;

	ui_textentry.base.disabled = 0;
}

void edit_ui_question(const uint8_t *title, const uint8_t *text, void (*cb)(uint32_t))
{
	if(input_shift)
	{
		cb(1);
		return;
	}

	question_cb = cb;

	ui_question.base.disabled = 0;

	glui_set_text(&ui_question_title, title, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	glui_set_text(&ui_question_text, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
}

void edit_ui_info_box(const uint8_t *title, uint32_t textcolor)
{
//	kguir_text_set(&ui_info_title, title, NULL);
//	kguir_text_set(&ui_info_text, edit_info_box_text, NULL);
//	ui_info_text.color = textcolor;

//	edit_enable_ui(UI_INFO_BOX, -1);
}

void edit_busy_window(const uint8_t *tile)
{
	system_was_busy = 1;
	memcpy(shader_buffer.projection.matrix, r_projection_ui.raw, sizeof(matrix3d_t));
	glui_set_text(&ui_busy_text, tile, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	glui_draw((void*)&ui_busy_window, -1, -1);
	system_finish_draw();
}

void edit_draw_ui()
{
	edit_ui_busy = glui_draw((void*)&ui_editor, mouse_x, mouse_y);
}

uint32_t edit_check_name(const uint8_t *text, uint8_t reject)
{
	if(!text[0])
		return 1;

	if(text[0] == ' ')
		return 1;

	while(*text)
	{
		uint8_t txt = *text++;
		if(txt < ' ')
			return 1;
		if(txt == reject)
			return 1;
	}

	return 0;
}

void edit_place_thing_in_sector(kge_thing_t *th, kge_sector_t *sec)
{
	if(!sec)
	{
		link_entry_t *ent;

		// go trough sectors
		ent = edit_list_sector.top;
		while(ent)
		{
			sec = (kge_sector_t*)(ent + 1);

			if(edit_is_point_in_sector(th->pos.x, th->pos.y, sec, 0))
			{
				th->pos.sector = sec;
				thing_update_sector(th, 1);
				edit_fix_thing_z(th, 0);
				break;
			}

			ent = ent->next;
		}

		return;
	}

	if(	!th->pos.sector &&
		edit_is_point_in_sector(th->pos.x, th->pos.y, sec, 0)
	){
		th->pos.sector = sec;
		thing_update_sector(th, 1);
		edit_fix_thing_z(th, 0);
	}
}

void edit_mark_sector_things(kge_sector_t *sec)
{
	sector_link_t *link = sec->thinglist;
	while(link)
	{
		kge_thing_t *th = link->thing;
		th->vc.editor = vc_editor;
		th->prop.eflags = (th->pos.z <= th->prop.floorz);
		th->prop.eflags |= (th->pos.z + th->prop.height >= th->prop.ceilingz) << 1;
		link = link->next_thing;
	}
}

void edit_fix_thing_z(kge_thing_t *thing, uint32_t eflags)
{
	if(eflags & 2 || thing->pos.z > thing->prop.ceilingz - thing->prop.height)
		thing->pos.z = thing->prop.ceilingz - thing->prop.height;

	if(eflags & 1 || thing->pos.z < thing->prop.floorz)
		thing->pos.z = thing->prop.floorz;

	if(thing_check_links(thing))
		thing_update_sector(thing, 0);
}

void edit_fix_marked_things_z(uint32_t keep_on_planes)
{
	link_entry_t *ent = tick_list_normal.top;

	keep_on_planes = !!keep_on_planes;

	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			kge_thing_t *th = (kge_thing_t*)tickable->data;

			if(th->vc.editor == vc_editor)
			{
				thing_update_sector(th, 1);
				edit_fix_thing_z(th, th->prop.eflags * keep_on_planes);
			}
		}

		ent = ent->next;
	}
}

void edit_update_thing_type(kge_thing_t *th)
{
	thing_def_t *ti;
	export_type_t *info;

	th->prop.name[0] = 0;

	switch(th->prop.type & 0xFF0000)
	{
		case 0x000000:
			ti = thing_info + th->prop.type;
			info = &ti->info;
			th->prop.radius = info->radius;
			th->prop.height = info->height;
			th->prop.viewheight = info->view_height ? info->view_height : info->height * 3 / 4;
			strcpy(th->prop.name, ti->name.text);
		break;
		case 0x020000:
			ti = thing_info + THING_TYPE_PLAYER_N;
			info = &ti->info;
			th->prop.radius = info->radius;
			th->prop.height = info->height;
			th->prop.viewheight = info->view_height ? info->view_height : info->height * 3 / 4;
			strcpy(th->prop.name, internal_thing[th->prop.type & 0xFFFF].name);
		break;
		default:
			th->prop.radius = 16;
			th->prop.height = 32;
			th->prop.viewheight = 16;
		break;
	}

	if(th->prop.radius < 1)
		th->prop.radius = 1;

	thing_update_sector(th, 1);

	if(th->pos.sector)
		edit_fix_thing_z(th, 0);

	x16g_set_sprite_texture(th);
}

static void obj_box_vtx(int32_t *box, kge_vertex_t *vtx)
{
	int32_t tmp;

	tmp = vtx->x;
	if(box[0] > tmp)
		box[0] = tmp;
	if(box[1] < tmp)
		box[1] = tmp;

	tmp = vtx->y;
	if(box[2] > tmp)
		box[2] = tmp;
	if(box[3] < tmp)
		box[3] = tmp;
}

void edit_update_object(edit_sec_obj_t *obj)
{
	int32_t box[4] = {32767, -32767, 32767, -32767};
	int32_t tmp;

	for(uint32_t i = 0; i < obj->count; i++)
	{
		kge_line_t *line = obj->line + i;
		engine_update_line(line);
		obj_box_vtx(box, line->vertex[0]);
		obj_box_vtx(box, line->vertex[1]);
	}

	obj->origin.x = box[0] + (box[1] - box[0]) / 2;
	obj->origin.y = box[2] + (box[3] - box[2]) / 2;
}

void edit_fix_object(edit_sec_obj_t *obj)
{
	kge_line_t *line = obj->line;
	kge_vertex_t *vtx = obj->vtx;

	for(uint32_t i = 0; i < obj->count; i++, line++, vtx++)
	{
		line->vertex[0] = vtx + 1;
		line->vertex[1] = vtx + 0;
	}

	line--;
	line->vertex[0] = obj->vtx;
}

uint32_t edit_get_special_thing(const uint8_t *name)
{
	internal_thing_t *it = internal_thing;

	for(uint32_t i; i < sizeof(internal_thing) / sizeof(internal_thing_t); i++)
	{
		if(!strcmp(it->name, name))
			return it->type;
		it++;
	}

	return THING_TYPE_UNKNOWN;
}

uint32_t edit_is_sector_hidden(kge_sector_t *sector)
{
	if(edit_grp_show)
	{
		if(!sector)
			return 1;
		if(sector->stuff.group[0] != edit_grp_show)
			return 1;
		if(edit_subgrp_show && sector->stuff.group[1] != edit_subgrp_show)
			return 1;
	} else
	if(edit_subgrp_show)
	{
		if(!sector)
			return 1;
		if(sector != thing_local_player->pos.sector)
			return 1;
	}

	return 0;
}

void edit_clear_hit(uint32_t forced)
{
	memset(&edit_hit, 0, sizeof(edit_hit));
	if(forced)
		edit_highlight_changed();
}

static void set_hl_texture(glui_image_t *img, kge_x16_tex_t *tex)
{
	editor_texture_t *et = editor_texture + tex->idx;
	uint32_t shader;
	float colormap = 0;

	switch(et->type)
	{
		case X16G_TEX_TYPE_WALL_MASKED:
			shader = SHADER_FRAGMENT_PALETTE;
		break;
		case X16G_TEX_TYPE_WALL:
		case X16G_TEX_TYPE_PLANE_8BPP:
			shader = SHADER_FRAGMENT_PALETTE;
		break;
		case X16G_TEX_TYPE_PLANE_4BPP:
			colormap = COLORMAP_IDX(et->cmap);
			shader = SHADER_FRAGMENT_COLORMAP;
		break;
		default:
			shader = SHADER_FRAGMENT_RGB;
		break;
	}

	img->shader = shader;
	img->colormap = colormap;
	img->gltex = &et->gltex;

	img->coord.s[0] = 0.0f;
	img->coord.s[1] = 1.0f;
	img->coord.t[0] = 0.0f;
	img->coord.t[1] = 1.0f;

	if(et->width > et->height)
	{
		img->base.width = 64;
		img->base.height = 64.0f * ((float)et->height / (float)et->width);
	} else
	{
		img->base.width = 64.0f * ((float)et->width / (float)et->height);
		img->base.height = 64;
	}
}

static void make_wall_info(glui_text_t *dest, uint8_t *temp, const uint8_t *name, kge_x16_tex_t *tex)
{
	uint8_t mirror[3] = "no";
	uint8_t *dst = mirror;

	if(tex->flags & TEXFLAG_MIRROR_X)
		*dst++ = 'X';

	if(tex->flags & TEXFLAG_MIRROR_Y_SWAP_XY)
		*dst++ = 'Y';

	if(dst != mirror)
		*dst = 0;

	sprintf(temp, "%s\x0B\n\n\n\n\nyPeg: %s\nMirror: %s\nx: %u y: %u", name, txt_top_bot[!(tex->flags & TEXFLAG_PEG_Y)], mirror, tex->ox, tex->oy);
	glui_set_text(dest, temp, glui_font_tiny_kfn, GLUI_ALIGN_CENTER_TOP);
}

void edit_highlight_changed()
{
	static kge_sector_t last_sector;
	static kge_line_t last_line;
	static kge_thing_t last_thing;
	uint8_t text[1024];
	uint32_t noAimg = 0;
	uint32_t count = 1;
	uint32_t ypos = 16;

	if(!e3d_highlight && mode_2d3d == EDIT_MODE_3D)
	{
		ui_edit_highlight.base.disabled = 1;
		return;
	}

	if(edit_hit.sector)
	{
		last_sector.vc = edit_hit.sector->vc;
		last_sector.thinglist = edit_hit.sector->thinglist;
		last_sector.pl_ident[0] = edit_hit.sector->pl_ident[0];
		last_sector.pl_ident[1] = edit_hit.sector->pl_ident[1];

		if(memcmp(&last_sector, edit_hit.sector, sizeof(kge_sector_t)))
		{
			float t;
			uint8_t *dst, *tmp;
			kge_secplane_t *sp;

			memcpy(&last_sector, edit_hit.sector, sizeof(kge_sector_t));
			memset(&last_line, 0, sizeof(kge_line_t));
			memset(&last_thing, 0, sizeof(kge_thing_t));

			ui_edit_highlight.base.disabled = 0;

			sprintf(text, "Sector #%u", list_get_idx(&edit_list_sector, (link_entry_t*)edit_hit.sector - 1) + 1);
			glui_set_text(&ui_edit_highlight_title, text, glui_font_small_kfn, GLUI_ALIGN_CENTER_TOP);

			// floor

			sp = edit_hit.sector->plane + PLANE_BOT;
			dst = text;
			tmp = sp->link ? "FLOOR" : "Floor";
			dst += sprintf(text, "%s\x0B\n\n\n\n\nZ: %.0f", tmp, sp->height);
			sprintf(dst, "\nRot: %u\nx: %u y: %u", sp->texture.angle, sp->texture.ox, sp->texture.oy);
			glui_set_text(&ui_edit_highlight_textA, text, glui_font_tiny_kfn, GLUI_ALIGN_CENTER_TOP);

			set_hl_texture(&ui_edit_highlight_imgA, &sp->texture);

			// ceiling

			sp = edit_hit.sector->plane + PLANE_TOP;

			sprintf(text, "%s\x0B\n\n\n\n\nZ: %.0f\nRot: %u\nx: %u y: %u", sp->link ? "CEILING" : "Ceiling", sp->height, sp->texture.angle, sp->texture.ox, sp->texture.oy);
			glui_set_text(&ui_edit_highlight_textB, text, glui_font_tiny_kfn, GLUI_ALIGN_CENTER_TOP);

			set_hl_texture(&ui_edit_highlight_imgB, &sp->texture);

			// other

			glui_set_text(&ui_edit_highlight_textC, "Light", glui_font_tiny_kfn, GLUI_ALIGN_CENTER_TOP);
			glui_set_text(&ui_edit_highlight_textD, "Palette", glui_font_tiny_kfn, GLUI_ALIGN_CENTER_TOP);

			t = edit_hit.sector->light.idx * (1.0f / (float)MAX_X16_LIGHTS);
			ui_edit_highlight_imgC.base.width = 64;
			ui_edit_highlight_imgC.base.height = 64;
			ui_edit_highlight_imgC.shader = SHADER_FRAGMENT_PALETTE;
			ui_edit_highlight_imgC.gltex = &x16_gl_tex[X16G_GLTEX_LIGHT_TEX];
			ui_edit_highlight_imgC.coord.s[1] = 1.0f;
			ui_edit_highlight_imgC.coord.t[0] = t;
			ui_edit_highlight_imgC.coord.t[1] = t + 1.0f / (float)MAX_X16_LIGHTS;

			t = edit_hit.sector->stuff.palette * 4 * (1.0f / (float)MAX_X16_PALETTE);
			ui_edit_highlight_imgD.base.width = 64;
			ui_edit_highlight_imgD.base.height = 64;
			ui_edit_highlight_imgD.shader = SHADER_FRAGMENT_RGB;
			ui_edit_highlight_imgD.gltex = &x16_gl_tex[X16G_GLTEX_PALETTE];
			ui_edit_highlight_imgD.coord.s[1] = 1.0f;
			ui_edit_highlight_imgD.coord.t[0] = t;
			ui_edit_highlight_imgD.coord.t[1] = t + 1.0f / (float)MAX_X16_PALETTE;

			count = 4;
		} else
			return;
	} else
	if(edit_hit.line)
	{
		last_line.vc = edit_hit.line->vc;
		last_line.tex_ident[0] = edit_hit.line->tex_ident[0];
		last_line.tex_ident[1] = edit_hit.line->tex_ident[1];
		last_line.tex_ident[2] = edit_hit.line->tex_ident[2];

		if(memcmp(&last_line, edit_hit.line, sizeof(kge_line_t)))
		{
			uint8_t txet[1024];
			kge_x16_tex_t *tex;
			uint8_t *dst = txet;

			memset(&last_sector, 0, sizeof(kge_sector_t));
			memcpy(&last_line, edit_hit.line, sizeof(kge_line_t));
			memset(&last_thing, 0, sizeof(kge_thing_t));

			ui_edit_highlight.base.disabled = 0;

			if(edit_hit.line->object)
				sprintf(text, "Line-object of Sector #%u", list_get_idx(&edit_list_sector, (link_entry_t*)edit_hit.extra_sector - 1) + 1);
			else
				sprintf(text, "Line #%u of Sector #%u", edit_hit.line - edit_hit.extra_sector->line, list_get_idx(&edit_list_sector, (link_entry_t*)edit_hit.extra_sector - 1) + 1);
			glui_set_text(&ui_edit_highlight_title, text, glui_font_small_kfn, GLUI_ALIGN_CENTER_TOP);

			ypos = 22;

			// info

			dst += sprintf(dst, "Len: %.2f\nxPeg: %s\n", edit_hit.line->stuff.length, txt_right_left[!(edit_hit.line->info.flags & WALLFLAG_PEG_X)]);

			// top

			tex = edit_hit.line->texture + 0;

			make_wall_info(&ui_edit_highlight_textB, text, "Top", tex);
			set_hl_texture(&ui_edit_highlight_imgB, tex);

			count = 2;

			if(edit_hit.line->backsector)
			{
				// info

				dst += sprintf(dst, "Blocking\n");
				dst = edit_put_blockbits(dst, edit_hit.line->info.blocking);
				*dst++ = '\n';
				*dst = 0;

				// bot

				tex = edit_hit.line->texture + 1;

				make_wall_info(&ui_edit_highlight_textC, text, "Bot", tex);
				set_hl_texture(&ui_edit_highlight_imgC, tex);

				count = 3;

				if(edit_hit.line->texture[2].idx)
				{
					// info

					dst += sprintf(dst, "Blockmid\n");
					dst = edit_put_blockbits(dst, edit_hit.line->info.blockmid);
					*dst++ = '\n';
					*dst = 0;

					// mid

					tex = edit_hit.line->texture + 2;

					make_wall_info(&ui_edit_highlight_textD, text, "Mid", tex);
					set_hl_texture(&ui_edit_highlight_imgD, tex);

					count = 4;
				}
			} else
			if(edit_hit.line->texture_split != INFINITY)
			{
				// bot

				tex = edit_hit.line->texture + 1;

				make_wall_info(&ui_edit_highlight_textC, text, "Bot", tex);
				set_hl_texture(&ui_edit_highlight_imgC, tex);

				count = 3;
			} else
			if(edit_hit.line->texture[2].idx)
			{
				// mid

				tex = edit_hit.line->texture + 2;

				make_wall_info(&ui_edit_highlight_textC, text, "Mid", tex);
				set_hl_texture(&ui_edit_highlight_imgC, tex);

				count = 3;
			}

			// info
			glui_set_text(&ui_edit_highlight_textA, txet, glui_font_tiny_kfn, GLUI_ALIGN_CENTER_TOP);
			noAimg = 1;
		} else
			return;
	} else
	if(edit_hit.thing)
	{
		last_thing.vc = edit_hit.thing->vc;
		last_thing.ident = edit_hit.thing->ident;

		if(memcmp(&last_thing, edit_hit.thing, sizeof(kge_thing_t)))
		{
			uint8_t *txt;
			kge_thing_t *th = edit_hit.thing;
			glui_image_t *img = &ui_edit_highlight_imgA;
			uint32_t height = th->sprite.height / 2;

			memset(&last_sector, 0, sizeof(kge_sector_t));
			memset(&last_line, 0, sizeof(kge_line_t));
			memcpy(&last_thing, th, sizeof(kge_thing_t));

			ui_edit_highlight.base.disabled = 0;

			txt = th->prop.name;
			if(*txt != '\t')
			{
				if(th == thing_local_player)
					txt = "editor camera";
				else
				if(*txt)
				{
					while(*txt && *txt != '\n')
						txt++;

					if(*txt != '\n')
					{
						sprintf(text, "Thing \"%s\"", th->prop.name);
						txt = NULL;
					} else
						txt++;
				} else
					txt = "unknown";

				if(txt)
					sprintf(text, "Thing [%s]", txt);
			} else
				sprintf(text, "Thing \t");

			glui_set_text(&ui_edit_highlight_title, text, glui_font_small_kfn, GLUI_ALIGN_CENTER_TOP);

			// text
			sprintf(text, "\n\n\n\n\n\n\nX: %.0f Y: %.0f Z: %.0f", th->pos.x, th->pos.y, th->pos.z);
			glui_set_text(&ui_edit_highlight_textA, text, glui_font_tiny_kfn, GLUI_ALIGN_CENTER_TOP);

			// image
			th->sprite.shader;

			img->shader = th->sprite.shader;
			img->colormap = 0;
			img->gltex = &th->sprite.gltex;

			img->coord.s[0] = 0.0f;
			img->coord.s[1] = 1.0f;
			img->coord.t[0] = 0.0f;
			img->coord.t[1] = 1.0f;

			if(th->sprite.stride > height)
			{
				uint32_t width = th->sprite.stride;
				img->base.width = width > 96 ? 96 : width < 64 ? 64 : width;
				img->base.height = (float)img->base.width * ((float)height / (float)th->sprite.stride);
			} else
			{
				img->base.height = height > 96 ? 96 : height < 64 ? 64 : height;
				img->base.width = (float)img->base.height * ((float)th->sprite.stride / (float)height);
			}

			ypos = 24;
		} else
			return;
	} else
	{
		memset(&last_sector, 0, sizeof(kge_sector_t));
		memset(&last_line, 0, sizeof(kge_line_t));
		memset(&last_thing, 0, sizeof(kge_thing_t));
		ui_edit_highlight.base.disabled = 1;
		return;
	}

	ui_edit_highlight.base.width = 96 * (count < 2 ? 2 : count);
	ui_edit_highlight_title.base.x = ui_edit_highlight.base.width / 2;

	{
		uint32_t i;
		for(i = 0; i < count; i++)
		{
			glui_element_t **elm = ui_edit_highlight.elements + 1 + i;
			int32_t x = ui_edit_highlight.base.width * (1 + i * 2) / (count * 2);

			elm[0]->base.disabled = 0;
			elm[0]->base.x = x;
			elm[0]->base.y = ypos;

			elm[4]->base.disabled = 0;
			elm[4]->base.x = x;
			elm[4]->base.y = ypos + 17 + 32;
		}
		for( ; i < 4; i++)
		{
			glui_element_t **elm = ui_edit_highlight.elements + 1 + i;
			elm[0]->base.disabled = 1;
			elm[4]->base.disabled = 1;
		}
	}

	if(noAimg)
		ui_edit_highlight_imgA.base.disabled = 1;
}

void edit_spawn_camera()
{
	edit_camera_thing = thing_spawn(0, 0, 0, NULL);
	edit_camera_thing->prop.type = THING_TYPE_EDIT_CAMERA;
	edit_camera_thing->prop.name[0] = 0;
	edit_camera_thing->ticcmd = &tick_local_cmd;

	x16g_set_sprite_texture(edit_camera_thing);

	edit_camera_thing->prop.height = 16;
	edit_camera_thing->prop.radius = 8;
	edit_camera_thing->prop.viewheight = 8;
	edit_camera_thing->pos.z = 32.0f;

	thing_local_player = edit_camera_thing;
	thing_local_camera = edit_camera_thing;
}

void edit_enter_thing(kge_thing_t *th)
{
	tick_local_cmd.angle = th->pos.angle;
	tick_local_cmd.pitch = th->pos.pitch;
	thing_local_player->ticcmd = NULL;
	th->ticcmd = &tick_local_cmd;
	thing_local_player = th;
	thing_local_camera = th;
}

void edit_new_map()
{
	link_entry_t *ent;

	x16e_enable_logo = 0;

	x16_sky_name[0] = '\t';

	edit_clear_hit(1);

	ent = tick_list_normal.top;
	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			kge_thing_t *th = (kge_thing_t*)tickable->data;
			th->pos.sector = NULL;
			thing_update_sector(th, 1);
		}

		ent = ent->next;
	}

	tick_cleanup();
	list_clear(&edit_list_draw_new);
	list_clear(&edit_list_vertex);
	clear_sectors();
	edit_sky_num = -1;

	edit_num_groups = 1; // group 0 is dummy
	edit_grp_show = 0;
	edit_subgrp_show = 0;
	edit_grp_last = 0;
	edit_subgrp_last = 0;

	memset(edit_group, 0, sizeof(edit_group));

	for(uint32_t i = 1; i < EDIT_MAX_GROUPS; i++)
	{
		edit_group_t *eg = edit_group + i;
		eg->count = 1;
		eg->sub[0].name[0] = '\t';
		eg->sub[0].name[1] = 0;
	}
}

uint32_t edit_check_map()
{
	link_entry_t *ent;

	if(!edit_camera_thing->pos.sector)
	{
		edit_status_printf("Camera is not in any sector!");
		return 1;
	}

	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sector = (kge_sector_t*)(ent + 1);
		uint32_t i;

		for(i = 0; i < sector->line_count - 1; i++)
		{
			kge_line_t *line = sector->line + i;

			if(line->backsector)
			{
				uint32_t j;

				for(j = i + 1; j < sector->line_count; j++)
				{
					kge_line_t *ln = sector->line + j;

					if(	ln->backsector &&
						(kge_line_point_side(line, ln->vertex[0]->x, ln->vertex[0]->y) < 0.0f ||
						kge_line_point_side(line, ln->vertex[1]->x, ln->vertex[1]->y) < 0.0f)
					)
						break;
				}

				if(j != sector->line_count)
					break;
			}
		}

		if(i != sector->line_count - 1)
		{
			edit_status_printf("There are portal lines with potential recursion!");
			return 1;
		}

		ent = ent->next;
	}

	return 0;
}

void edit_status_printf(const uint8_t *text, ...)
{
	uint8_t *ptr;
	uint32_t idx, tmp;
	va_list arg;
	glui_text_t *last;
	uint8_t buff[256];

	if(!ui_edit_status.elements[0])
		return;

	tmp = ui_edit_status.elements[0]->text.gltex;

	for(idx = 0; idx < ui_edit_status.count - 1; idx++)
	{
		glui_text_t *t0 = &ui_edit_status.elements[idx]->text;
		glui_text_t *t1 = &ui_edit_status.elements[idx + 1]->text;

		t0->base.border_color[0] = t1->base.border_color[0];
		t0->width = t1->width;
		t0->height = t1->height;
		t0->x = t1->x;
		t0->y = t1->y;
		t0->gltex = t1->gltex;
	}

	last = &ui_edit_status.elements[idx]->text;

	last->base.border_color[0] = STATUS_LINE_TICK;
	last->gltex = tmp;

	va_start(arg, text);
	vsprintf(buff, text, arg);
	va_end(arg);

	glui_set_text(last, buff, glui_font_medium_kfn, GLUI_ALIGN_LEFT_CENTER);

	if(ui_edit_status.base.y < 0)
		ui_edit_status.base.y += STATUS_LINE_HEIGHT;

	ui_edit_status.base.disabled = 0;
}

uint32_t edit_handle_input(const edit_input_func_t *ifunc)
{
	while(ifunc->inidx >= 0)
	{
		if(input_state[ifunc->inidx])
		{
			int32_t ret;
			ret = ifunc->func();
			if(ret > 0)
				input_state[ifunc->inidx] = 0;
			if(ret)
				return 1;
		}
		ifunc++;
	}

	return 0;
}

void editor_change_mode(int32_t mode)
{
	ui_edit_mode.base.disabled = 1;

	edit_camera_thing->mom.x = 0;
	edit_camera_thing->mom.y = 0;
	edit_camera_thing->mom.z = 0;

	input_analog_h = 0;
	input_analog_v = 0;

	edit_clear_hit(1);
	input_clear();

	ui_edit_highlight.base.disabled = 1;

	if(mode < 0)
		mode = mode_2d3d;

	switch(mode)
	{
		case EDIT_MODE_2D:
set2d:
			mode_2d3d = EDIT_MODE_2D;
			edit_enter_thing(edit_camera_thing);
			e2d_mode_set();
		break;
		case EDIT_MODE_3D:
			if(!edit_camera_thing->pos.sector)
				goto set2d;
			mode_2d3d = EDIT_MODE_3D;
			e3d_mode_set();
		break;
		default:
			edit_wf_pos = thing_local_player->pos;
			ewf_mode_set();
		break;
	}
}

void editor_x16_mode(uint32_t mode)
{
	const uint8_t *text[4];

	ui_edit_highlight.base.disabled = 1;

	ui_edit_gfx.base.disabled = 1;
	ui_edit_thg.base.disabled = 1;
	ui_map_options.base.disabled = 1;

	edit_x16_mode = mode;

	switch(mode)
	{
		case EDIT_XMODE_MAP:
			ui_map_options.base.disabled = 0;
			text[0] = "New map";
			text[1] = "Load map";
			text[2] = "Save map";
			text[3] = "Export map";
			editor_change_mode(EDIT_MODE_2D_OR_3D);
		break;
		case EDIT_XMODE_GFX:
			ui_edit_gfx.base.disabled = 0;
			text[0] = "New GFX";
			text[1] = "Load GFX";
			text[2] = "Save GFX";
			text[3] = "Export GFX";
			x16g_mode_set();
		break;
		case EDIT_XMODE_THG:
			ui_edit_thg.base.disabled = 0;
			text[0] = "New things";
			text[1] = "Load things";
			text[2] = "Save things";
			text[3] = "Export things";
			x16t_mode_set();
		break;
	}

	glui_set_text(&ui_menu_new, text[0], glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	glui_set_text(&ui_menu_load, text[1], glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	glui_set_text(&ui_menu_save, text[2], glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	glui_set_text(&ui_menu_export, text[3], glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
}

uint32_t edit_is_point_in_sector(float x, float y, kge_sector_t *sec, uint32_t only_visible)
{
	/// check if XY point is in this sector
	uint32_t i;
	uint32_t cross = 0;

	if(!sec)
		return 0;

	if(only_visible && edit_is_sector_hidden(sec))
		return 0;

	// go trough lines
	for(i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = sec->line + i;

		float y0 = line->vertex[0]->y - y;
		float y1 = line->vertex[1]->y - y;

		if((float32bits(y0) ^ float32bits(y1)) & 0x80000000)
		{
			float x0 = line->vertex[0]->x - x;
			float x1 = line->vertex[1]->x - x;

			if((float32bits(x0) ^ float32bits(x1)) & 0x80000000)
			{
				x0 = x0 * y1 - x1 * y0;
				cross ^= float32bits(x0) ^ float32bits(y1);
			} else
				cross ^= float32bits(x0);
		}
	}

	return cross & 0x80000000;
}

void *edit_cbor_branch(const edit_cbor_obj_t *cbor_obj, uint32_t type, const uint8_t *key, uint32_t key_len, kgcbor_value_t *value, uint32_t val_len)
{
	while(cbor_obj->name)
	{
		if(	type == cbor_type_map[cbor_obj->type] &&
			key_len == cbor_obj->nlen &&
			!memcmp(key, cbor_obj->name, cbor_obj->nlen)
		){
			if(cbor_obj->valid)
				*cbor_obj->valid = 1;

			switch(cbor_obj->type)
			{
				case EDIT_CBOR_TYPE_FLAG8:
					if(value->u32)
						*cbor_obj->u8 |= cbor_obj->extra;
					else
						*cbor_obj->u8 &= ~cbor_obj->extra;
				return NULL;
				case EDIT_CBOR_TYPE_U8:
					*cbor_obj->u8 = value->u8;
				return NULL;
				case EDIT_CBOR_TYPE_S8:
					*cbor_obj->s8 = value->s8;
				return NULL;
				case EDIT_CBOR_TYPE_U16:
					*cbor_obj->u16 = value->u16;
				return NULL;
				case EDIT_CBOR_TYPE_S16:
					*cbor_obj->s16 = value->s16;
				return NULL;
				case EDIT_CBOR_TYPE_U32:
					*cbor_obj->s32 = value->s32;
				return NULL;
				case EDIT_CBOR_TYPE_S32:
					*cbor_obj->u32 = value->u32;
				return NULL;
				case EDIT_CBOR_TYPE_ANGLE:
					*cbor_obj->u16 = 0 - value->u16;
				return NULL;
				case EDIT_CBOR_TYPE_FLOAT:
					*cbor_obj->f32 = value->s32;
				return NULL;
				case EDIT_CBOR_TYPE_STRING:
					if(val_len >= cbor_obj->extra)
						val_len = cbor_obj->extra - 1;
					memcpy(cbor_obj->ptr, value->ptr, val_len);
					cbor_obj->ptr[val_len] = 0;
				return NULL;
				case EDIT_CBOR_TYPE_BINARY:
					if(val_len <= cbor_obj->extra)
					{
						if(cbor_obj->readlen)
							*cbor_obj->readlen = val_len;
						memcpy(cbor_obj->ptr, value->ptr, val_len);
					} else
					if(cbor_obj->readlen)
						*cbor_obj->readlen = 0;
				return NULL;
			}

			return cbor_obj->handler;
		}
		cbor_obj++;
	}

	return NULL;
}

uint32_t edit_cbor_export(const edit_cbor_obj_t *cbor_obj, uint32_t count, kgcbor_gen_t *gen)
{
	uint32_t ret;

	kgcbor_put_object(gen, count);

	for(uint32_t i = 0; i < count; i++)
	{
		if(cbor_obj->type >= EDIT_CBOR_TYPE_OBJECT)
			return 0;

		if(kgcbor_put_string(gen, cbor_obj->name, cbor_obj->nlen))
			return 1;

		if(cbor_obj->valid && !*cbor_obj->valid)
		{
			ret = kgcbor_put_null(gen);
			goto skip_value;
		}

		switch(cbor_obj->type)
		{
			case EDIT_CBOR_TYPE_FLAG8:
				ret = kgcbor_put_bool(gen, *cbor_obj->u8 & cbor_obj->extra);
			break;
			case EDIT_CBOR_TYPE_U8:
				ret = kgcbor_put_u32(gen, *cbor_obj->u8);
			break;
			case EDIT_CBOR_TYPE_S8:
				ret = kgcbor_put_s32(gen, *cbor_obj->s8);
			break;
			case EDIT_CBOR_TYPE_U16:
				ret = kgcbor_put_u32(gen, *cbor_obj->u16);
			break;
			case EDIT_CBOR_TYPE_S16:
				ret = kgcbor_put_s32(gen, *cbor_obj->s16);
			break;
			case EDIT_CBOR_TYPE_U32:
				ret = kgcbor_put_u32(gen, *cbor_obj->u32);
			break;
			case EDIT_CBOR_TYPE_S32:
				ret = kgcbor_put_s32(gen, *cbor_obj->s32);
			break;
			case EDIT_CBOR_TYPE_ANGLE:
				ret = kgcbor_put_u32(gen, (0 - *cbor_obj->u16) & 0xFFFF);
			break;
			case EDIT_CBOR_TYPE_FLOAT:
				if(*cbor_obj->f32 == INFINITY)
					ret = kgcbor_put_null(gen);
				else
					ret = kgcbor_put_s32(gen, *cbor_obj->f32);
			break;
			case EDIT_CBOR_TYPE_STRING:
				if(cbor_obj->ptr)
					ret = kgcbor_put_string(gen, cbor_obj->ptr, -1);
				else
					ret = kgcbor_put_null(gen);
			break;
			case EDIT_CBOR_TYPE_BINARY:
				ret = kgcbor_put_binary(gen, cbor_obj->ptr, cbor_obj->extra);
			break;
		}

skip_value:
		if(ret)
			return 1;

		cbor_obj++;
	}

	return 0;
}

kge_sector_t *edit_find_sector_by_point(float x, float y, kge_sector_t *ignore, uint32_t only_visible)
{
	/// find sector by XY location
	link_entry_t *ent;

	// go trough sectors
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		if(sec != ignore)
		{
			if(edit_is_point_in_sector(x, y, sec, only_visible))
				return sec;
		}

		ent = ent->next;
	}

	return NULL;
}

uint32_t edit_check_sector_overlap(kge_sector_t *sec, kge_sector_t *sector)
{
	uint32_t i, j;

	if(sec->line_count != sector->line_count)
		return 0;

	if(sec->plane[PLANE_TOP].height == sector->plane[PLANE_BOT].height)
	{
		if(sec->plane[PLANE_TOP].link)
			return 0;
		if(sector->plane[PLANE_BOT].link)
			return 0;
	} else
	if(sec->plane[PLANE_BOT].height == sector->plane[PLANE_TOP].height)
	{
		if(sec->plane[PLANE_BOT].link)
			return 0;
		if(sector->plane[PLANE_TOP].link)
			return 0;
	} else
		return 0;

	// find first matching vertex
	for(i = 0; i < sec->line_count; i++)
	{
		kge_line_t *lo = sector->line;
		kge_line_t *lt = sec->line + i;

		if(lo->vertex[0]->x != lt->vertex[0]->x)
			continue;

		if(lo->vertex[0]->y != lt->vertex[0]->y)
			continue;

		if(lo->vertex[1]->x != lt->vertex[1]->x)
			continue;

		if(lo->vertex[1]->y != lt->vertex[1]->y)
			continue;

		break;
	}

	if(i >= sec->line_count)
		return 0;

	// check all vertexes
	for(j = 0; j < sec->line_count; j++)
	{
		kge_line_t *lo = sector->line + j;
		kge_line_t *lt = sec->line + i;

		if(lo->vertex[0]->x != lt->vertex[0]->x)
			break;

		if(lo->vertex[0]->y != lt->vertex[0]->y)
			break;

		if(lo->vertex[1]->x != lt->vertex[1]->x)
			break;

		if(lo->vertex[1]->y != lt->vertex[1]->y)
			break;

		i++;
		if(i >= sec->line_count)
			i = 0;
	}

	return j >= sec->line_count;
}

void *edit_load_file(const uint8_t *path, uint32_t *size)
{
	int fd;
	struct stat stat;
	void *buff;

	fd = open(path, O_RDONLY);
	if(fd < 0)
		return NULL;

	if(fstat(fd, &stat))
		goto close_fail;

	if(!stat.st_size || stat.st_size > 64 * 1024 * 1024)
		goto close_fail;

	buff = malloc(stat.st_size);
	if(!buff)
		goto close_fail;

	read(fd, buff, stat.st_size);
	close(fd);

	if(size)
		*size = stat.st_size;

	return buff;

close_fail:
	close(fd);
	return NULL;
}

uint32_t edit_read_file(const uint8_t *path, void *buff, uint32_t size)
{
	int fd;
	struct stat stat;

	fd = open(path, O_RDONLY);
	if(fd < 0)
		return 0;

	if(fstat(fd, &stat))
		goto close_fail;

	if(!stat.st_size || stat.st_size > size)
		goto close_fail;

	read(fd, buff, stat.st_size);
	close(fd);

	return stat.st_size;

close_fail:
	close(fd);
	return 0;
}

uint32_t edit_save_file(const uint8_t *path, void *data, uint32_t size)
{
	int fd;

	fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if(fd < 0)
		return 1;

	write(fd, data, size);
	close(fd);

	return 0;
}

const uint8_t *edit_path_filename(const uint8_t *path)
{
	const uint8_t *ptr = path;

	while(*ptr)
		ptr++;

	while(ptr > path)
	{
		if(*ptr == PATH_SPLIT_CHAR)
			return ptr + 1;
		ptr--;
	}

	return ptr;
}

const uint8_t *edit_map_save(const uint8_t *file)
{
	link_entry_t *ent;
	kgcbor_gen_t gen;
	uint32_t size;

	if(file)
		strcpy(map_path, file);

	gen.ptr = edit_cbor_buffer;
	gen.end = edit_cbor_buffer + EDIT_EXPORT_BUFFER_SIZE;

	/// root
	edit_cbor_export(cbor_root, NUM_CBOR_ROOT, &gen);

	/// groups
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_GROUPS].name, cbor_root[CBOR_ROOT_GROUPS].nlen);
	kgcbor_put_object(&gen, edit_num_groups - 1); // group zero is not present

	for(uint32_t i = 1; i < edit_num_groups; i++)
	{
		edit_group_t *eg = edit_group + i;

		kgcbor_put_string(&gen, eg->name, -1);
		kgcbor_put_array(&gen, eg->count - 1); // subgroup zero is not present

		for(uint32_t j = 1; j < eg->count; j++)
			kgcbor_put_string(&gen, eg->sub[j].name, -1);
	}

	/// camera
	load_camera = edit_camera_thing->pos;
	if(thing_local_camera->pos.sector)
		load_camera.sector = (kge_sector_t*)(intptr_t)list_get_idx(&edit_list_sector, (link_entry_t*)thing_local_camera->pos.sector - 1);
	else
		load_camera.sector = (kge_sector_t*)(intptr_t)-1;
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_CAMERA].name, cbor_root[CBOR_ROOT_CAMERA].nlen);
	edit_cbor_export(cbor_camera, NUM_CBOR_CAMERA, &gen);

	/// vertexes
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_VERTEXES].name, cbor_root[CBOR_ROOT_VERTEXES].nlen);
	kgcbor_put_array(&gen, edit_list_vertex.count);

	ent = edit_list_vertex.top;
	while(ent)
	{
		kge_vertex_t *vtx = (kge_vertex_t*)(ent + 1);

		load_vtx = *vtx;
		edit_cbor_export(cbor_vtx, NUM_CBOR_VTX, &gen);

		ent = ent->next;
	}

	/// sectors
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_SECTORS].name, cbor_root[CBOR_ROOT_SECTORS].nlen);
	kgcbor_put_array(&gen, edit_list_sector.count);

	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);

		load_secpalidx = sec->stuff.palette;
		load_sector_flags = sec->stuff.x16flags;
		strcpy(load_seclight, sec->light.name);
		edit_cbor_export(cbor_sector, NUM_CBOR_SECTOR, &gen);

		// group
		kgcbor_put_string(&gen, cbor_sector[CBOR_SECTOR_GROUP].name, cbor_sector[CBOR_SECTOR_GROUP].nlen);
		kgcbor_put_array(&gen, 2);
		kgcbor_put_u32(&gen, sec->stuff.group[0]);
		kgcbor_put_u32(&gen, sec->stuff.group[1]);

		// ceiling
		kgcbor_put_string(&gen, cbor_sector[CBOR_SECTOR_CEILING].name, cbor_sector[CBOR_SECTOR_CEILING].nlen);
		load_secplane = sec->plane[PLANE_TOP];
		if(load_secplane.link)
			load_secplane.link = (void*)(uintptr_t)list_get_idx(&edit_list_sector, (link_entry_t*)load_secplane.link - 1);
		else
			load_secplane.link = (void*)-1;
		edit_cbor_export(cbor_secplane, NUM_CBOR_SECPLANE, &gen);

		// floor
		kgcbor_put_string(&gen, cbor_sector[CBOR_SECTOR_FLOOR].name, cbor_sector[CBOR_SECTOR_FLOOR].nlen);
		load_secplane = sec->plane[PLANE_BOT];
		if(load_secplane.link)
			load_secplane.link = (void*)(uintptr_t)list_get_idx(&edit_list_sector, (link_entry_t*)load_secplane.link - 1);
		else
			load_secplane.link = (void*)-1;
		edit_cbor_export(cbor_secplane, NUM_CBOR_SECPLANE, &gen);

		// walls
		kgcbor_put_string(&gen, cbor_sector[CBOR_SECTOR_LINES].name, cbor_sector[CBOR_SECTOR_LINES].nlen);
		kgcbor_put_array(&gen, sec->line_count);

		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = &sec->line[sec->line_count - i - 1]; // reverse order

			load_backsector = -1;
			if(line->backsector)
				load_backsector = list_get_idx(&edit_list_sector, (link_entry_t*)line->backsector - 1);

			load_split = line->texture_split;
			load_lblock = line->info.blocking;
			load_mblock = line->info.blockmid;
			load_lnflags = line->info.flags;

			edit_cbor_export(cbor_line, NUM_CBOR_LINE, &gen);

			// vtx
			kgcbor_put_string(&gen, cbor_line[CBOR_LINE_VTX].name, cbor_line[CBOR_LINE_VTX].nlen);
			kgcbor_put_array(&gen, 2);
			kgcbor_put_u32(&gen, list_get_idx(&edit_list_vertex, (link_entry_t*)line->vertex[1] - 1));
			kgcbor_put_u32(&gen, list_get_idx(&edit_list_vertex, (link_entry_t*)line->vertex[0] - 1));

			// top
			kgcbor_put_string(&gen, cbor_line[CBOR_LINE_TOP].name, cbor_line[CBOR_LINE_TOP].nlen);
			load_linetex = line->texture[0];
			edit_cbor_export(cbor_linetex, NUM_CBOR_LINETEX, &gen);

			// bot
			kgcbor_put_string(&gen, cbor_line[CBOR_LINE_BOT].name, cbor_line[CBOR_LINE_BOT].nlen);
			load_linetex = line->texture[1];
			edit_cbor_export(cbor_linetex, NUM_CBOR_LINETEX, &gen);

			// mid
			kgcbor_put_string(&gen, cbor_line[CBOR_LINE_MID].name, cbor_line[CBOR_LINE_MID].nlen);
			load_linetex = line->texture[2];
			edit_cbor_export(cbor_linetex, NUM_CBOR_LINETEX, &gen);
		}

		// objects
		{
			link_entry_t *ent = sec->objects.top; // another

			kgcbor_put_string(&gen, cbor_sector[CBOR_SECTOR_OBJECTS].name, cbor_sector[CBOR_SECTOR_OBJECTS].nlen);
			kgcbor_put_array(&gen, sec->objects.count);

			while(ent)
			{
				edit_sec_obj_t *obj = (edit_sec_obj_t*)(ent + 1);
				kge_line_t *line = obj->line;

				// object
				kgcbor_put_array(&gen, obj->count);

				for(uint32_t i = 0; i < obj->count; i++, line++)
				{
					load_lnflags = line->info.flags;
					cbor_objline[CBOR_OBJLINE_X].f32 = &line->vertex[1]->x;
					cbor_objline[CBOR_OBJLINE_Y].f32 = &line->vertex[1]->y;
					edit_cbor_export(cbor_objline, NUM_CBOR_OBJLINE, &gen);

					// top
					kgcbor_put_string(&gen, cbor_objline[CBOR_OBJLINE_TOP].name, cbor_objline[CBOR_OBJLINE_TOP].nlen);
					load_linetex = line->texture[0];
					edit_cbor_export(cbor_linetex, NUM_CBOR_LINETEX, &gen);
				}

				ent = ent->next;
			}
		}

		ent = ent->next;
	}

	/// things

	// count valid things first
	size = 0;
	ent = tick_list_normal.top;
	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			kge_thing_t *th = (kge_thing_t*)tickable->data;

			if(th->prop.name[0])
				size++;
		}

		ent = ent->next;
	}

	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_THINGS].name, cbor_root[CBOR_ROOT_THINGS].nlen);
	kgcbor_put_array(&gen, size);

	// export valid things
	ent = tick_list_normal.top;
	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			kge_thing_t *th = (kge_thing_t*)tickable->data;

			if(th->prop.name[0])
			{
				load_thing = *th;

				if(th->pos.sector)
					load_thing.pos.sector = (kge_sector_t*)(intptr_t)list_get_idx(&edit_list_sector, (link_entry_t*)th->pos.sector - 1);
				else
					load_thing.pos.sector = (kge_sector_t*)(intptr_t)-1;

				edit_cbor_export(cbor_thing, NUM_CBOR_THING, &gen);
			}
		}

		ent = ent->next;
	}

	// save
	if(edit_save_file(map_path, edit_cbor_buffer, (void*)gen.ptr - edit_cbor_buffer))
		return NULL;

	return edit_path_filename(map_path);
}

const uint8_t *edit_map_load(const uint8_t *file)
{
	link_entry_t *ent, *ont;
	uint32_t size;
	kgcbor_ctx_t gctx;
	kge_sector_t *sec;

	if(file)
		strcpy(map_path, file);

	size = edit_read_file(map_path, edit_cbor_buffer, EDIT_EXPORT_BUFFER_SIZE);
	if(!size)
		goto do_fail;

	memset(&load_camera, 0, sizeof(load_camera));

	edit_new_map();

	gctx.entry_cb = cbor_map_root;
	gctx.max_recursion = 16;
	if(kgcbor_parse_object(&gctx, edit_cbor_buffer, size))
		goto do_fail;

	ent = edit_list_sector.top;
	while(ent)
	{
		int32_t pdx;

		sec = (kge_sector_t*)(ent + 1);

		if(sec->stuff.group[0] >= edit_num_groups)
			goto do_fail;

		if(sec->stuff.group[1] && sec->stuff.group[1] >= edit_group[sec->stuff.group[0]].count)
			goto do_fail;

		pdx = (intptr_t)sec->plane[PLANE_TOP].link;
		if(pdx >= 0)
		{
			sec->plane[PLANE_TOP].link = list_find_idx(&edit_list_sector, pdx);
			if(!sec->plane[PLANE_TOP].link)
				goto do_fail;
		} else
			sec->plane[PLANE_TOP].link = NULL;

		pdx = (intptr_t)sec->plane[PLANE_BOT].link;
		if(pdx >= 0)
		{
			sec->plane[PLANE_BOT].link = list_find_idx(&edit_list_sector, pdx);
			if(!sec->plane[PLANE_BOT].link)
				goto do_fail;
		} else
			sec->plane[PLANE_BOT].link = NULL;

		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = sec->line + i;

			pdx = (intptr_t)line->backsector;

			if(pdx >= 0)
			{
				line->backsector = list_find_idx(&edit_list_sector, pdx);
				if(!line->backsector)
					goto do_fail;
			} else
				line->backsector = NULL;

			line->vertex[0] = list_find_idx(&edit_list_vertex, (uintptr_t)line->vertex[0]);
			if(!line->vertex[0])
				goto do_fail;

			line->vertex[1] = list_find_idx(&edit_list_vertex, (uintptr_t)line->vertex[1]);
			if(!line->vertex[1])
				goto do_fail;

			engine_update_line(line);
		}

		ont = sec->objects.top;
		while(ont)
		{
			edit_sec_obj_t *obj = (edit_sec_obj_t*)(ont + 1);
			kge_line_t *line = obj->line;

			for(uint32_t i = 0; i < obj->count; i++, line++)
				if(!line->object)
					goto do_fail;

			edit_fix_object(obj);
			edit_update_object(obj);

			ont = ont->next;
		}

		engine_update_sector(sec);

		ent = ent->next;
	}

	// editor camera
	edit_spawn_camera();
	edit_camera_thing->pos = load_camera;

	// check group names
	for(uint32_t i = 1; i < edit_num_groups; i++)
	{
		edit_group_t *eg = edit_group + i;

		if(!eg->name[0])
			sprintf(eg->name, "grp %u", i);

		for(uint32_t j = 1; j < eg->count - 1; j++)
		{
			if(!eg->sub[j].name[0])
				sprintf(eg->sub[j].name, "sub %u", j);
		}
	}

	// reslove thing sectors
	ent = tick_list_normal.top;
	while(ent)
	{
		kge_ticker_t *tickable = (kge_ticker_t*)(ent + 1);

		if(IS_THING_TICKER(tickable))
		{
			kge_thing_t *th = (kge_thing_t*)tickable->data;
			sec = list_find_idx(&edit_list_sector, (uintptr_t)th->pos.sector);
			th->pos.sector = NULL;
			edit_place_thing_in_sector(th, sec);
		}

		ent = ent->next;
	}

	x16g_update_map();
	x16t_update_map();

	return edit_path_filename(map_path);

do_fail:
	edit_new_map();
	edit_spawn_camera();
	return NULL;
}

