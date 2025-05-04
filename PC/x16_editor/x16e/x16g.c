#include "inc.h"
#include "defs.h"
#include "system.h"
#include "input.h"
#include "matrix.h"
#include "shader.h"
#include "engine.h"
#include "render.h"
#include "editor.h"
#include "things.h"
#include "list.h"
#include "image.h"
#include "x16g.h"
#include "x16r.h"
#include "x16e.h"
#include "x16c.h"
#include "x16t.h"
#include "kgcbor.h"
#include "glui.h"
#include "ui_def.h"

#include "x16tex.h"

#define STEX_PIXEL_LIMIT	0x10000	// 0x10000 is engine limit
#define FONT_FIRST_CHAR	' '
#define FONT_CHAR_COUNT	96
#define NUMS_CHAR_COUNT	16

#define NUM_RANGES_COLS	4
#define NUM_RANGES_ROWS	8
#define UI_RANGE_HEIGHT	18

#define UI_WPN_PART_HEIGHT	23

#define UI_WPN_IMG_SCALE	4

#define UI_WPN_PARTS	23

#define SWPN_MAX_DATA	4096

#define LIGHT_CRC_XOR	0xD17F114E
#define PLANE_CRC_XOR	0x50E3FB68
#define WALL_CRC_XOR	0x8ED0D2DA
#define SPR_CRC_XOR	0x97C1EB69
#define WPN_CRC_XOR	0xD560D38D
#define SKY_CRC_XOR	0x38673175
#define VAR_CRC_XOR	0x4B3F46E2

#define WPN_PART_FLAG_BRIGHT	0x80
#define WPN_PART_FLAG_DISABLED	0x40
#define WPN_PART_FLAG_MIRROR_X	1

enum
{
	GFX_MODE_PALETTE,
	GFX_MODE_LIGHTS,
	GFX_MODE_REMAPS,
	GFX_MODE_PLANES,
	GFX_MODE_WALLS,
	GFX_MODE_SPRITES,
	GFX_MODE_WEAPONS,
	GFX_MODE_SKIES,
	GFX_MODE_HUD,
	//
	GFX_NUM_MODES
};

enum
{
	VLIST_PLANE,
	VLIST_WALL,
	VLIST_SPRITE,
	VLIST_WEAPON,
	//
	NUM_VLIST_TYPES
};

enum
{
	HUDE_FONT,
	HUDE_NUMS,
	//
	NUM_HUD_ELM
};

enum
{
	CBOR_ROOT_PALETTES,
	CBOR_ROOT_LIGHTS,
	CBOR_ROOT_REMAPS,
	CBOR_ROOT_PLANES,
	CBOR_ROOT_WALLS,
	CBOR_ROOT_SPRITES,
	CBOR_ROOT_WEAPONS,
	CBOR_ROOT_SKIES,
	CBOR_ROOT_FONT,
	CBOR_ROOT_NUMS,
	CBOR_ROOT_FULLBRIGHT,
	//
	NUM_CBOR_ROOT
};

enum
{
	CBOR_LIGHT_D,
	CBOR_LIGHT_R,
	CBOR_LIGHT_G,
	CBOR_LIGHT_B,
	//
	NUM_CBOR_LIGHT
};

enum
{
	CBOR_PLANE4_WIDTH,
	CBOR_PLANE4_HEIGHT,
	CBOR_PLANE4_DATA,
	CBOR_PLANE4_VARIANTS,
	//
	NUM_CBOR_PLANE4
};

enum
{
	CBOR_PLANE8_WIDTH,
	CBOR_PLANE8_HEIGHT,
	CBOR_PLANE8_DATA,
	//
	NUM_CBOR_PLANE8
};

enum
{
	CBOR_PLANE_VARIANT_DATA,
	CBOR_PLANE_VARIANT_EFFECT,
	//
	NUM_CBOR_PLANE_VARIANT
};

enum
{
	CBOR_STEX_DATA,
	CBOR_STEX_VARIANTS,
	//
	NUM_CBOR_STEX
};

enum
{
	CBOR_VAR_WALL_WIDTH,
	CBOR_VAR_WALL_HEIGHT,
	CBOR_VAR_WALL_MASKED,
	CBOR_VAR_WALL_ANIMATION,
	CBOR_VAR_WALL_OFFSETS,
	//
	NUM_CBOR_VAR_WALL
};

enum
{
	CBOR_VAR_SPR_WIDTH,
	CBOR_VAR_SPR_HEIGHT,
	CBOR_VAR_SPR_OX,
	CBOR_VAR_SPR_OY,
	CBOR_VAR_SPR_OFFSETS,
	//
	NUM_CBOR_VAR_SPR
};

enum
{
	CBOR_WPN_VGRP_NDATA,
	CBOR_WPN_VGRP_BDATA,
	CBOR_WPN_VGRP_VARIANTS,
	//
	NUM_CBOR_WPN_VGRP
};

enum
{
	CBOR_WPN_PART_WIDTH,
	CBOR_WPN_PART_HEIGHT,
	CBOR_WPN_PART_OX,
	CBOR_WPN_PART_OY,
	CBOR_WPN_PART_OFFSET,
	CBOR_WPN_PART_BRIGHT,
	CBOR_WPN_PART_DISABLED,
	CBOR_WPN_PART_MIRROR_X,
	//
	NUM_CBOR_WPN_PART
};
enum
{
	CBOR_FONT_DATA,
	CBOR_FONT_SPACE,
	//
	NUM_CBOR_FONT
};

typedef struct
{
	uint32_t width, height;
	uint32_t format;
	const void *data;
} gltex_info_t;

typedef struct
{
	int32_t now;
	int32_t max;
} ui_idx_t;

typedef struct
{
	glui_container_t *cont;
	glui_text_t *text;
	const uint8_t *(*update)(ui_idx_t*);
	int32_t (*input)(glui_element_t*,uint32_t);
} ui_set_t;

typedef struct hud_element_s
{
	const uint8_t *name;
	int32_t width, height, scale;
	uint32_t shader;
	void (*import)(uint8_t*);
	void *(*texgen)(const struct hud_element_s *);
} hud_element_t;

typedef union
{
	uint8_t raw[9];
	struct
	{
		uint8_t t, s, e;
		union
		{
			struct
			{
				uint8_t rs, gs, bs;
				uint8_t re, ge, be;
			} rgb;
			struct
			{
				uint8_t s, e;
			} pal;
		};
	};
} remap_entry_t;

typedef struct
{
	uint32_t count;
	uint32_t now;
	remap_entry_t entry[NUM_RANGES_ROWS];
} color_remap_t;

typedef union
{
	uint32_t hash;
	struct
	{
		uint8_t frm;
		uint8_t rot;
		// for export only
		int8_t ox, oy;
	};
} sprite_hash_t;

typedef union
{
	uint32_t hash;
	struct
	{
		uint8_t frm;
		uint8_t count;
		uint8_t base;
	};
} wspr_hash_t;

typedef struct
{
	uint16_t width, height;
	int16_t x, y;
	uint16_t hack_height;
	uint8_t flags;
	uint32_t offset;
} wpnspr_part_t;

typedef struct
{
	uint8_t name[LEN_X16_VARIANT_NAME];
	union
	{
		uint32_t hash;
		sprite_hash_t spr;
		wspr_hash_t wpn;
	};
	union
	{
		struct
		{
			uint16_t bright;
			uint8_t data[16];
			uint8_t effect[4];
		} pl;
		struct
		{
			uint16_t width, height, stride;
			int8_t ox, oy;
			uint8_t masked;
			uint8_t anim[3];
			uint32_t offset[128];
			uint16_t length[128];
			uint16_t temp[128];
		} sw;
		struct
		{
			uint32_t now;
			int8_t ox, oy;
			uint32_t dstart, dsize, dbright;
			wpnspr_part_t part[UI_WPN_PARTS];
		} ws;
	};
} variant_info_t;

typedef struct
{
	uint8_t name[LEN_X16_TEXTURE_NAME];
	uint32_t hash;
	uint16_t max, now;
	union
	{
		struct
		{
			// planes
			uint32_t width, height;
		};
		struct
		{
			// walls, sprites
			uint32_t stex_total;
			uint32_t stex_used;
			uint32_t stex_fullbright;
		};
	};
	variant_info_t variant[MAX_X16_VARIANTS];
	uint8_t data[STEX_PIXEL_LIMIT * 2];
} variant_list_t;

typedef struct
{
	uint8_t *name;
	uint32_t exor;
} vlist_type_t;

typedef struct
{
	struct
	{
		int8_t des;
		uint8_t mr, mg, mb;
		uint8_t ar, ag, ab;
	} tint;
	struct
	{
		uint8_t des, add;
		uint8_t r, g, b;
	} dmg;
} palette_options_t;

typedef union
{
	int8_t *s8;
	uint8_t *u8;
} value_ptr_t;

typedef struct
{
	uint16_t height;
	uint8_t data[];
} st_column_t;

typedef struct
{
	uint8_t data[32];
	int8_t space;
} font_char_t;

typedef struct
{
	uint8_t data[64];
	int8_t space;
} nums_char_t;

//

typedef struct
{
	uint32_t hash;
	uint16_t bright;
	uint8_t data[16];
	uint8_t effect[4];
} __attribute__((packed)) export_plane_variant_t;

typedef struct
{
	uint8_t map_base;
	uint8_t num_variants;
	union
	{
		struct
		{
			uint8_t data[64 * 64 / 2];
			export_plane_variant_t variant[MAX_X16_VARIANTS_PLN];
		} bpp4;
		struct
		{
			uint8_t data[64 * 64];
			uint8_t effect[4];
		} bpp8;
	};
} export_plane_t;

//

static uint32_t gfx_mode;
static ui_idx_t gfx_idx[GFX_NUM_MODES];

uint32_t x16_editor_gt[MAX_EDITOR_TEXTURES];
uint32_t x16_gl_tex[NUM_X16G_GLTEX];

static value_ptr_t value_ptr;
static void (*value_cb)();

uint32_t x16_palette_data[256 * MAX_X16_PALETTE];
uint16_t x16_palette_bright[16];
static uint32_t palette_clipboard[256];

static uint8_t remap_temp_data[256];
static color_remap_t x16_color_remap[MAX_X16_REMAPS];
static glui_text_t *text_ranges;
static uint32_t remap_entry_dst;

uint8_t x16_light_data[256 * MAX_X16_LIGHTS];

uint8_t x16_colormap_data[32 * MAX_X16_GL_CMAPS];

editor_texture_t editor_texture[MAX_EDITOR_TEXTURES];
uint32_t editor_texture_count;

editor_light_t editor_light[MAX_X16_LIGHTS];
uint32_t x16_num_lights;

static variant_list_t x16_plane[MAX_X16_PLANES];
static uint_fast8_t plane_8bpp;
static uint_fast8_t plane_display;

static variant_list_t x16_wall[MAX_X16_WALLS];
static uint_fast8_t wall_display;

static variant_list_t x16_sprite[MAX_X16_THGSPR];
static uint_fast8_t sprite_display;
static uint_fast8_t sprite_origin = 2;

static variant_list_t x16_weapon[MAX_X16_WPNSPR];
static wpnspr_part_t wpn_part_info[UI_WPN_PARTS];
static glui_dummy_t *wpn_part_rect;
static int32_t wpn_part_copy = -1;

editor_sprlink_t editor_sprlink[MAX_X16_THGSPR + MAX_X16_WPNSPR];
uint32_t x16_num_sprlnk_thg;
uint32_t x16_num_sprlnk_wpn;
uint32_t x16_num_sprlnk;

uint32_t x16_num_skies;
editor_sky_t editor_sky[MAX_X16_SKIES];

static uint_fast8_t hud_display;

static font_char_t font_char[FONT_CHAR_COUNT];
static nums_char_t nums_char[NUMS_CHAR_COUNT];

static uint32_t base_height_cmaps;

static uint16_t stex_source[STEX_PIXEL_LIMIT * 2];
static uint16_t stex_data[STEX_PIXEL_LIMIT];
static uint16_t stex_space[STEX_PIXEL_LIMIT / 256];
static uint32_t stex_size;
static uint32_t stex_total;
static uint32_t stex_fullbright;
static uint32_t stex_import_pal;
static uint32_t stex_wpn_import;

static uint8_t *effect_dest;
static uint_fast8_t effect_idx;

uint32_t x16g_state_res[3];
int32_t x16g_state_offs[2];
uint16_t *x16g_state_data_ptr;
uint32_t *x16g_state_offs_ptr;

static uint8_t gfx_path[EDIT_MAX_FILE_PATH];

static variant_list_t *cbor_load_object;
static variant_info_t *cbor_load_entry;
static wpnspr_part_t *cbor_load_part;
static uint32_t *cbor_load_array;
static int32_t cbor_main_index;
static int32_t cbor_entry_index;
static uint_fast8_t cbor_stex_is_sprite;

uint8_t x16_sky_name[LEN_X16_SKY_NAME];

static const uint8_t vram_ranges[] =
{
#include "ranges.h"
};

static const uint8_t tm_128x32[] =
{
#include "tm_128x32.h"
};

static const uint8_t tm_256x16[] =
{
#include "tm_256x16.h"
};

static const uint8_t *update_gfx_palette(ui_idx_t *idx);
static int32_t input_gfx_palette(glui_element_t*,uint32_t);
static int32_t input_gfx_weapons(glui_element_t*,uint32_t);
static const uint8_t *update_gfx_lights(ui_idx_t *idx);
static const uint8_t *update_gfx_remaps(ui_idx_t *idx);
static const uint8_t *update_gfx_planes(ui_idx_t *idx);
static const uint8_t *update_gfx_walls(ui_idx_t *idx);
static const uint8_t *update_gfx_sprites(ui_idx_t *idx);
static const uint8_t *update_gfx_weapons(ui_idx_t *idx);
static const uint8_t *update_gfx_skies(ui_idx_t *idx);
static const uint8_t *update_gfx_hud(ui_idx_t *idx);

static palette_options_t palette_options =
{
	// tint
	.tint.des = 0,
	.tint.mr = 100,
	.tint.mg = 100,
	.tint.mb = 100,
	.tint.ar = 0,
	.tint.ag = 0,
	// damage
	.dmg.des = 33,
	.dmg.add = 5,
	.dmg.r = 100,
	.dmg.g = 0,
	.dmg.b = 0,
};

static const ui_set_t ui_set[GFX_NUM_MODES] =
{
	[GFX_MODE_PALETTE] =
	{
		&ui_gfx_palette,
		&ui_gfx_palette_txt,
		update_gfx_palette,
		input_gfx_palette
	},
	[GFX_MODE_LIGHTS] =
	{
		&ui_gfx_lights,
		&ui_gfx_lights_txt,
		update_gfx_lights,
	},
	[GFX_MODE_REMAPS] =
	{
		&ui_gfx_remaps,
		&ui_gfx_remaps_txt,
		update_gfx_remaps,
	},
	[GFX_MODE_PLANES] =
	{
		&ui_gfx_planes,
		&ui_gfx_planes_txt,
		update_gfx_planes
	},
	[GFX_MODE_WALLS] =
	{
		&ui_gfx_walls,
		&ui_gfx_walls_txt,
		update_gfx_walls
	},
	[GFX_MODE_SPRITES] =
	{
		&ui_gfx_sprites,
		&ui_gfx_sprites_txt,
		update_gfx_sprites
	},
	[GFX_MODE_WEAPONS] =
	{
		&ui_gfx_weapons,
		&ui_gfx_weapons_txt,
		update_gfx_weapons,
		input_gfx_weapons
	},
	[GFX_MODE_SKIES] =
	{
		&ui_gfx_skies,
		&ui_gfx_skies_txt,
		update_gfx_skies
	},
	[GFX_MODE_HUD] =
	{
		&ui_gfx_hud,
		&ui_gfx_hud_txt,
		update_gfx_hud
	},
};

const uint8_t *const x16g_palette_name[MAX_X16_PALETTE] =
{
	"normal palette",
	"normal damage 0",
	"normal damage 1",
	"normal damage 2",

	"custom A",
	"custom A damage 0",
	"custom A damage 1",
	"custom A damage 2",

	"custom B",
	"custom B damage 0",
	"custom B damage 1",
	"custom B damage 2",

	"custom C",
	"custom C damage 0",
	"custom C damage 1",
	"custom C damage 2",
};

static uint8_t remap_name[] = "Remap #";

static const uint8_t *const plane_effect_name[8] =
{
	[X16G_PL_EFFECT_NONE] = "\t",
	[X16G_PL_EFFECT_RANDOM] = "random",
	[X16G_PL_EFFECT_CIRCLE] = "the 'O'",
	[X16G_PL_EFFECT_EIGHT] = "the '8'",
	[X16G_PL_EFFECT_ANIMATE] = "animation",
	[5] = "INVALID",
	[6] = "INVALID",
	[7] = "INVALID",
};

static const uint8_t *const sprite_origin_txt[] =
{
	"Origin: hide",
	"Origin: show",
	"Origin: use"
};

static const vlist_type_t vlist_type[NUM_VLIST_TYPES] =
{
	[VLIST_PLANE] = {.name = "Plane", .exor = PLANE_CRC_XOR},
	[VLIST_WALL] = {.name = "Wall", .exor = WALL_CRC_XOR},
	[VLIST_SPRITE] = {.name = "Sprite", .exor = SPR_CRC_XOR},
	[VLIST_WEAPON] = {.name = "Wprite", .exor = WPN_CRC_XOR},
};

static const uint16_t swpn_data_size[] =
{
	4096, // 64 x 64
	2048, // 32 x 64 // 64 x 32
	1024, // 16 x 64 // 32 x 32 // 64 x 16
	512, // 8 x 64 // 16 x 32 // 32 x 16 // 64 x 8
	256, // 8 x 32 // 16 x 16 // 32 x 8
	128, // 8 x 16 // 16 x 8
	64, // 8 x 8
	//
	0x8000 | 4096,
	0x8000 | 2048,
	0x8000 | 1024,
	0x8000 | 512,
	0x8000 | 256,
	0x8000 | 128,
	0x8000 | 64,
	//
	0
};

static void hud_import_font(uint8_t*);
static void *hud_texgen_font(const hud_element_t*);
static void hud_import_nums(uint8_t*);
static void *hud_texgen_nums(const hud_element_t*);

static const hud_element_t hud_element[NUM_HUD_ELM] =
{
	[HUDE_FONT] =
	{
		.name = "text font",
		.width = 16 * 8,
		.height = 6 * 8,
		.scale = 4,
		.shader = SHADER_FRAGMENT_COLORMAP,
		.import = hud_import_font,
		.texgen = hud_texgen_font,
	},
	[HUDE_NUMS] =
	{
		"numeric font",
		.width = 8 * 8,
		.height = 2 * 16,
		.scale = 4,
		.shader = SHADER_FRAGMENT_COLORMAP,
		.import = hud_import_nums,
		.texgen = hud_texgen_nums,
	},
};

static gltex_info_t gltex_info[NUM_X16G_GLTEX] =
{
	[X16G_GLTEX_PALETTE] =
	{
		.width = 16,
		.height = 16 * MAX_X16_PALETTE,
		.format = GL_RGBA,
		.data = x16_palette_data
	},
	[X16G_GLTEX_LIGHTS] =
	{
		.width = 256,
		.height = MAX_X16_LIGHTS,
		.format = GL_LUMINANCE,
		.data = x16_light_data
	},
	[X16G_GLTEX_LIGHT_TEX] =
	{
		.width = 16,
		.height = MAX_X16_LIGHTS * 16,
		.format = GL_LUMINANCE,
		.data = x16_light_data
	},
	[X16G_GLTEX_COLORMAPS] =
	{
		.width = 16,
		.height = MAX_X16_GL_CMAPS,
		.format = GL_LUMINANCE_ALPHA,
		.data = x16_colormap_data
	},
	[X16G_GLTEX_NOW_PALETTE] =
	{
		.width = 256,
		.height = 1,
		.format = GL_RGBA,
		.data = x16_palette_data
	},
	[X16G_GLTEX_SHOW_LIGHT] =
	{
		.width = 16,
		.height = 16,
		.format = GL_LUMINANCE,
		.data = x16_light_data
	},
	[X16G_GLTEX_SHOW_TEXTURE] =
	{
		.format = GL_LUMINANCE,
	},
	[X16G_GLTEX_BRIGHT_COLORS] =
	{
		.format = GL_LUMINANCE,
	},
	[X16G_GLTEX_UNK_TEXTURE] =
	{
		.width = 64,
		.height = 64,
		.format = GL_RGBA,
		.data = x16e_tex_unk_data
	},
	[X16G_GLTEX_SPR_NONE] =
	{
		.width = 32,
		.height = 64,
		.format = GL_RGBA,
		.data = x16e_spr_none_data
	},
	[X16G_GLTEX_SPR_POINT] =
	{
		.width = 32,
		.height = 32,
		.format = GL_RGBA,
		.data = x16e_spr_point_data
	},
	[X16G_GLTEX_SPR_BAD] =
	{
		.width = 8,
		.height = 64,
		.format = GL_RGBA,
		.data = x16e_spr_bad_data
	},
	[X16G_GLTEX_SPR_START] =
	{
		.width = 32,
		.height = 96,
		.format = GL_RGBA,
		.data = x16e_spr_start_data
	},
	[X16G_GLTEX_SPR_START_COOP] =
	{
		.width = 32,
		.height = 96,
		.format = GL_RGBA,
		.data = x16e_spr_start_data
	},
	[X16G_GLTEX_SPR_START_DM] =
	{
		.width = 32,
		.height = 96,
		.format = GL_RGBA,
		.data = x16e_spr_start_data
	},
};

static int32_t cbor_gfx_palette(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_light(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_remap(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_plane(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_wall(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_sprite(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_weapon(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_wpn_variant(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_sky(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_font(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_nums(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_fullbright(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);
static int32_t cbor_gfx_stex_variant(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value);

static const edit_cbor_obj_t cbor_root[] =
{
	[CBOR_ROOT_PALETTES] =
	{
		.name = "palettes",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_gfx_palette
	},
	[CBOR_ROOT_LIGHTS] =
	{
		.name = "lights",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_gfx_light
	},
	[CBOR_ROOT_REMAPS] =
	{
		.name = "remaps",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_gfx_remap
	},
	[CBOR_ROOT_PLANES] =
	{
		.name = "planes",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_gfx_plane
	},
	[CBOR_ROOT_WALLS] =
	{
		.name = "walls",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_gfx_wall
	},
	[CBOR_ROOT_SPRITES] =
	{
		.name = "sprites",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_gfx_sprite
	},
	[CBOR_ROOT_WEAPONS] =
	{
		.name = "weapons",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_gfx_weapon
	},
	[CBOR_ROOT_SKIES] =
	{
		.name = "skies",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_gfx_sky
	},
	[CBOR_ROOT_FONT] =
	{
		.name = "font",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_gfx_font
	},
	[CBOR_ROOT_NUMS] =
	{
		.name = "nums",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_gfx_nums
	},
	[CBOR_ROOT_FULLBRIGHT] =
	{
		.name = "fullbright",
		.nlen = 10,
		.type = EDIT_CBOR_TYPE_ARRAY,
		.handler = cbor_gfx_fullbright
	},
	// terminator
	[NUM_CBOR_ROOT] = {}
};

static edit_cbor_obj_t cbor_light[] =
{
	[CBOR_LIGHT_D] =
	{
		.name = "D",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_S8,
	},
	[CBOR_LIGHT_R] =
	{
		.name = "R",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_U8,
	},
	[CBOR_LIGHT_G] =
	{
		.name = "G",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_U8,
	},
	[CBOR_LIGHT_B] =
	{
		.name = "B",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_U8,
	},
	// terminator
	[NUM_CBOR_LIGHT] = {}
};

static edit_cbor_obj_t cbor_plane4[] =
{
	[CBOR_PLANE4_WIDTH] =
	{
		.name = "width",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U32,
	},
	[CBOR_PLANE4_HEIGHT] =
	{
		.name = "height",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_U32,
	},
	[CBOR_PLANE4_DATA] =
	{
		.name = "data",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_BINARY,
	},
	[CBOR_PLANE4_VARIANTS] =
	{
		.name = "variants",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_OBJECT,
	},
	// terminator
	[NUM_CBOR_PLANE4] = {}
};

static edit_cbor_obj_t cbor_plane8[] =
{
	[CBOR_PLANE8_WIDTH] =
	{
		.name = "width",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U32,
	},
	[CBOR_PLANE8_HEIGHT] =
	{
		.name = "height",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_U32,
	},
	[CBOR_PLANE8_DATA] =
	{
		.name = "data",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_BINARY,
	},
	// terminator
	[NUM_CBOR_PLANE8] = {}
};

static edit_cbor_obj_t cbor_plane_var[] =
{
	[CBOR_PLANE_VARIANT_DATA] =
	{
		.name = "data",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_BINARY,
		.extra = 2 + 16
	},
	[CBOR_PLANE_VARIANT_EFFECT] =
	{
		.name = "effect",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_BINARY,
		.extra = 4
	},
	// terminator
	[NUM_CBOR_PLANE_VARIANT] = {}
};

static edit_cbor_obj_t cbor_stex[] =
{
	[CBOR_STEX_DATA] =
	{
		.name = "data",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_BINARY,
	},
	[CBOR_STEX_VARIANTS] =
	{
		.name = "variants",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_gfx_stex_variant
	},
	// terminator
	[NUM_CBOR_STEX] = {}
};

static edit_cbor_obj_t cbor_var_wall[] =
{
	[CBOR_VAR_WALL_WIDTH] =
	{
		.name = "width",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U16,
	},
	[CBOR_VAR_WALL_HEIGHT] =
	{
		.name = "height",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_U16,
	},
	[CBOR_VAR_WALL_MASKED] =
	{
		.name = "masked",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.extra = 1
	},
	[CBOR_VAR_WALL_ANIMATION] =
	{
		.name = "animation",
		.nlen = 9,
		.type = EDIT_CBOR_TYPE_BINARY,
		.extra = 3
	},
	[CBOR_VAR_WALL_OFFSETS] =
	{
		.name = "offsets",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_ARRAY
	},
	// terminator
	[NUM_CBOR_VAR_WALL] = {}
};

static edit_cbor_obj_t cbor_var_spr[] =
{
	[CBOR_VAR_SPR_WIDTH] =
	{
		.name = "width",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U16,
	},
	[CBOR_VAR_SPR_HEIGHT] =
	{
		.name = "height",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_U16,
	},
	[CBOR_VAR_SPR_OX] =
	{
		.name = "x",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_S8,
	},
	[CBOR_VAR_SPR_OY] =
	{
		.name = "y",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_S8,
	},
	[CBOR_VAR_SPR_OFFSETS] =
	{
		.name = "offsets",
		.nlen = 7,
		.type = EDIT_CBOR_TYPE_ARRAY
	},
	// terminator
	[NUM_CBOR_VAR_SPR] = {}
};

static uint32_t load_ndata, load_bdata;
static edit_cbor_obj_t cbor_wpn_vgrp[] =
{
	[CBOR_WPN_VGRP_NDATA] =
	{
		.name = "normal",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_BINARY,
		.readlen = &load_ndata
	},
	[CBOR_WPN_VGRP_BDATA] =
	{
		.name = "bright",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_BINARY,
		.readlen = &load_bdata
	},
	[CBOR_WPN_VGRP_VARIANTS] =
	{
		.name = "variants",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_OBJECT,
		.handler = cbor_gfx_wpn_variant
	},
	// terminator
	[NUM_CBOR_WPN_VGRP] = {}
};

static wpnspr_part_t load_part;
static edit_cbor_obj_t cbor_wpn_part[] =
{
	[CBOR_WPN_PART_WIDTH] =
	{
		.name = "width",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U16,
		.u16 = &load_part.width
	},
	[CBOR_WPN_PART_HEIGHT] =
	{
		.name = "height",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_U16,
		.u16 = &load_part.hack_height
	},
	[CBOR_WPN_PART_OX] =
	{
		.name = "x",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_S16,
		.s16 = &load_part.x
	},
	[CBOR_WPN_PART_OY] =
	{
		.name = "y",
		.nlen = 1,
		.type = EDIT_CBOR_TYPE_S16,
		.s16 = &load_part.y
	},
	[CBOR_WPN_PART_OFFSET] =
	{
		.name = "offset",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_U32,
		.u32 = &load_part.offset
	},
	[CBOR_WPN_PART_BRIGHT] =
	{
		.name = "bright",
		.nlen = 6,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.u8 = &load_part.flags,
		.extra = WPN_PART_FLAG_BRIGHT
	},
	[CBOR_WPN_PART_DISABLED] =
	{
		.name = "disabled",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.u8 = &load_part.flags,
		.extra = WPN_PART_FLAG_DISABLED
	},
	[CBOR_WPN_PART_MIRROR_X] =
	{
		.name = "mirror x",
		.nlen = 8,
		.type = EDIT_CBOR_TYPE_FLAG8,
		.u8 = &load_part.flags,
		.extra = WPN_PART_FLAG_MIRROR_X
	},
	// terminator
	[NUM_CBOR_WPN_PART] = {}
};

static edit_cbor_obj_t cbor_font[] =
{
	[CBOR_FONT_DATA] =
	{
		.name = "data",
		.nlen = 4,
		.type = EDIT_CBOR_TYPE_BINARY,
	},
	[CBOR_FONT_SPACE] =
	{
		.name = "space",
		.nlen = 5,
		.type = EDIT_CBOR_TYPE_U8,
	},
	// terminator
	[NUM_CBOR_FONT] = {}
};

//
// checks

static uint32_t check_plane_resolution(uint32_t width, uint32_t height)
{
	return	!(width == 64 && height == 64) &&
		!(width == 32 && height == 128) &&
		!(width == 16 && height == 256);
}

static uint32_t check_wall_resolution(uint32_t width, uint32_t height)
{
	return	(
			width != 8 &&
			width != 16 &&
			width != 32 &&
			width != 64 &&
			width != 128
		) ||
		(
			height != 8 &&
			height != 16 &&
			height != 32 &&
			height != 64 &&
			height != 128 &&
			height != 256
		);
}

static uint32_t check_masked_resolution(uint32_t width, uint32_t height)
{
	return	height < 1 ||
		height > 128 ||
		(
			width != 8 &&
			width != 16 &&
			width != 32 &&
			width != 64 &&
			width != 128
		);
}

static uint32_t check_sprite_resolution(uint32_t width, uint32_t height)
{
	return	width < 1 ||
		height < 1 ||
		width > 126 || // > 126 is not supported
		height > 250 // > 250 is not supported
	;
}

static uint32_t check_weapon_resolution(uint32_t width, uint32_t height)
{
	return	width & 7 ||
		width < 8 ||
		height < 8 ||
		width > 160 ||
		height > 120
	;
}

static uint32_t check_part_resolution(uint32_t res)
{
	return	res != 8 &&
		res != 16 &&
		res != 32 &&
		res != 64;
}

//
// sprite name

uint32_t sprite_name_hash(const uint8_t *name)
{
	sprite_hash_t ret;

	ret.hash = 0xFFFFFFFF;

	if(name[0] < 'A' || name[0] > 'Z')
		return 0xFFFFFFFF;

	if(name[1] < '0' || name[1] > '7')
		return 0xFFFFFFFF;

	ret.frm = name[0] - 'A';
	ret.rot = name[1] - '0';

	if(name[2])
		return 0xFFFFFFFF;

	return ret.hash;
}

//
// CBOR callbacks

static int32_t cbor_gfx_palette_entry(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	uint32_t color;
	uint32_t r, g, b;

	if(ctx->index >= 255)
		return 1;

	if(type != KGCBOR_TYPE_VALUE)
		return 1;

	color = value->u32;

	r = color & 15;
	r |= r << 4;

	g = (color >> 4) & 15;
	g |= g << 4;

	b = (color >> 8) & 15;
	b |= b << 4;

	x16_palette_data[cbor_main_index * 256 + ctx->index + 1] = r | (g << 8) | (b << 16) | 0xFF000000;

	return 0;
}

static int32_t cbor_gfx_palette(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(ctx->index >= MAX_X16_PALETTE)
		return 1;

	if(type == KGCBOR_TYPE_ARRAY)
	{
		cbor_main_index = ctx->index;
		ctx->entry_cb = cbor_gfx_palette_entry;
		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_light_entry(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	edit_cbor_branch(cbor_light, type, key, ctx->key_len, value, ctx->val_len);
	return 1;
}

static int32_t cbor_gfx_light(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		gfx_idx[GFX_MODE_LIGHTS].max = cbor_main_index;
		return 0;
	}

	if(type == KGCBOR_TYPE_OBJECT)
	{
		editor_light_t *el = editor_light + cbor_main_index;

		if(cbor_main_index >= MAX_X16_LIGHTS)
			return 1;

		if(ctx->key_len >= LEN_X16_LIGHT_NAME)
			return 1;

		memset(el, 0, sizeof(editor_light_t));
		memcpy(el->name, key, ctx->key_len);
		el->hash = x16c_crc(el->name, -1, LIGHT_CRC_XOR);

		if(edit_check_name(el->name, 0))
			return 1;

		cbor_light[CBOR_LIGHT_D].s8 = &el->des;
		cbor_light[CBOR_LIGHT_R].u8 = &el->r;
		cbor_light[CBOR_LIGHT_G].u8 = &el->g;
		cbor_light[CBOR_LIGHT_B].u8 = &el->b;
		cbor_main_index++;

		ctx->entry_cb = cbor_gfx_light_entry;

		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_remap_entry(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	remap_entry_t *ent;

	if(type != KGCBOR_TYPE_BINARY)
		return 1;

	if(cbor_entry_index >= NUM_RANGES_ROWS)
		return 1;

	if(ctx->val_len != 9/*sizeof(remap_entry_t)*/)
		return 1;

	ent = (remap_entry_t*)value->ptr;

	if(ent->s > ent->e)
		return 1;

	memcpy(x16_color_remap[cbor_main_index].entry + cbor_entry_index, value->ptr, ctx->val_len);
	cbor_entry_index++;

	x16_color_remap[cbor_main_index].count = cbor_entry_index;

	return 0;
}

static int32_t cbor_gfx_remap(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_ARRAY)
		return 1;

	if(ctx->index >= MAX_X16_REMAPS)
		return 1;

	cbor_main_index = ctx->index;
	cbor_entry_index = 0;

	ctx->entry_cb = cbor_gfx_remap_entry;
	return 0;
}

static int32_t cbor_gfx_plane_variant_stuff(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	edit_cbor_branch(cbor_plane_var, type, key, ctx->key_len, value, ctx->val_len);
	return 1;
}

static int32_t cbor_gfx_plane_variant(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	variant_list_t *pl = cbor_load_object;

	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	if(ctx->key_len >= LEN_X16_VARIANT_NAME)
		return 1;

	if(cbor_entry_index >= MAX_X16_VARIANTS_PLN)
		return 1;

	memset(pl->variant[cbor_entry_index].name, 0, LEN_X16_VARIANT_NAME);
	memcpy(pl->variant[cbor_entry_index].name, key, ctx->key_len);
	pl->variant[cbor_entry_index].hash = x16c_crc(pl->variant[cbor_entry_index].name, -1, VAR_CRC_XOR);

	if(edit_check_name(pl->variant[cbor_entry_index].name, 0))
		return 1;

	cbor_plane_var[CBOR_PLANE_VARIANT_DATA].ptr = (void*)&pl->variant[cbor_entry_index].pl.bright;
	cbor_plane_var[CBOR_PLANE_VARIANT_EFFECT].ptr = (void*)&pl->variant[cbor_entry_index].pl.effect;

	cbor_entry_index++;

	ctx->entry_cb = cbor_gfx_plane_variant_stuff;

	return 0;
}

static int32_t cbor_gfx_plane_entry(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_OBJECT)
	{
		// special check for 'variants' only present in 4bpp planes
		if(	ctx->key_len == cbor_plane4[CBOR_PLANE4_VARIANTS].nlen &&
			!memcmp(key, cbor_plane4[CBOR_PLANE4_VARIANTS].name, cbor_plane4[CBOR_PLANE4_VARIANTS].nlen)
		){
			// handle variant list
			cbor_entry_index = 0;
			ctx->entry_cb = cbor_gfx_plane_variant;
			return 0;
		}

		return 1;
	}

	edit_cbor_branch(cbor_plane8, type, key, ctx->key_len, value, ctx->val_len);

	return 1;
}

static int32_t cbor_gfx_plane(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		if(!cbor_load_object)
			return 0;

		if(!cbor_load_object->width && !cbor_load_object->height)
		{
			// empty plane; check bit depth
			if(cbor_entry_index >= 0)
				// save variant count
				cbor_load_object->max = cbor_entry_index;
			else
				// mark as 8bpp
				cbor_load_object->now = MAX_X16_VARIANTS_PLN;
			// save amount
			gfx_idx[GFX_MODE_PLANES].max = cbor_main_index;
		} else
		if(check_plane_resolution(cbor_load_object->width, cbor_load_object->height))
		{
			// invalid resolution; skip
			memset(cbor_load_object, 0, sizeof(variant_list_t));
			cbor_main_index--;
		} else
		{
			// save amount
			gfx_idx[GFX_MODE_PLANES].max = cbor_main_index;

			// check bit depth
			if(cbor_entry_index >= 0)
			{
				// expand 4bpp to 8bpp
				variant_list_t *pl = cbor_load_object;
				uint32_t size = pl->width * pl->height;
				uint8_t *dst = pl->data + size;
				uint8_t *src = pl->data + size / 2;

				// save variant count
				pl->max = cbor_entry_index;

				for(uint32_t i = 0; i < size / 2; i++)
				{
					uint8_t in;

					src--;
					in = *src;

					dst--;
					*dst = in >> 4;

					dst--;
					*dst = in & 15;
				}
			} else
				// mark as 8bpp
				cbor_load_object->now = MAX_X16_VARIANTS_PLN;
		}

		cbor_load_object = NULL;

		return 0;
	}

	if(type == KGCBOR_TYPE_OBJECT)
	{
		variant_list_t *pl = x16_plane + cbor_main_index;

		if(cbor_main_index >= MAX_X16_PLANES)
			return 1;

		if(ctx->key_len >= LEN_X16_TEXTURE_NAME)
			return 1;

		memset(pl, 0, sizeof(variant_list_t));
		memcpy(pl->name, key, ctx->key_len);
		pl->hash = x16c_crc(pl->name, -1, PLANE_CRC_XOR);

		if(edit_check_name(pl->name, 0))
			return 1;

		cbor_entry_index = -1;
		cbor_load_object = pl;

		cbor_plane8[CBOR_PLANE8_WIDTH].u32 = &pl->width;
		cbor_plane8[CBOR_PLANE8_HEIGHT].u32 = &pl->height;
		cbor_plane8[CBOR_PLANE8_DATA].ptr = pl->data;
		cbor_plane8[CBOR_PLANE8_DATA].extra = 64 * 64;
		cbor_main_index++;

		ctx->entry_cb = cbor_gfx_plane_entry;

		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_stex_column(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_VALUE)
		return 1;

	if(ctx->index >= 256)
		return 1;

	cbor_load_array[ctx->index] = value->u32;

	return 1;
}

static int32_t cbor_gfx_stex_varent(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	edit_cbor_obj_t *co = cbor_stex_is_sprite ? cbor_var_spr : cbor_var_wall;

	cbor_load_array = edit_cbor_branch(co, type, key, ctx->key_len, value, ctx->val_len);

	if(!cbor_load_array)
		return 1;

	ctx->entry_cb = cbor_gfx_stex_column;

	return 0;
}

static int32_t cbor_gfx_stex_variant(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		variant_info_t *vi = cbor_load_entry;

		if(!cbor_load_entry)
			return 0;

		cbor_load_entry = NULL;

		if(vi->sw.width || vi->sw.height)
		{
			if(cbor_stex_is_sprite)
			{
				// check resolution
				if(check_sprite_resolution(vi->sw.width, vi->sw.height))
					goto is_invalid;
			} else
			{
				// check resolution
				if(vi->sw.masked)
				{
					if(check_masked_resolution(vi->sw.width, vi->sw.height))
						goto is_invalid;
				} else
				{
					if(check_wall_resolution(vi->sw.width, vi->sw.height))
						goto is_invalid;
				}
			}
		}

		// save amount
		cbor_load_object->max = cbor_entry_index;

		// set stride
		vi->sw.stride = (vi->sw.width + 3) & ~3;

		return 0;

is_invalid:
		// invalid; skip
		memset(vi, 0, sizeof(variant_info_t));
		cbor_entry_index--;

		return 0;
	}

	if(type == KGCBOR_TYPE_OBJECT)
	{
		variant_info_t *vi = cbor_load_object->variant + cbor_entry_index;

		if(cbor_entry_index >= MAX_X16_VARIANTS)
			return 1;

		if(ctx->key_len >= LEN_X16_VARIANT_NAME)
			return 1;

		memset(vi, 0, sizeof(variant_info_t));
		memcpy(vi->name, key, ctx->key_len);

		if(cbor_stex_is_sprite)
		{
			vi->hash = sprite_name_hash(vi->name);
			if(vi->hash == 0xFFFFFFFF)
				return 1;

			cbor_var_spr[CBOR_VAR_SPR_WIDTH].u16 = &vi->sw.width;
			cbor_var_spr[CBOR_VAR_SPR_HEIGHT].u16 = &vi->sw.height;
			cbor_var_spr[CBOR_VAR_SPR_OX].s8 = &vi->sw.ox;
			cbor_var_spr[CBOR_VAR_SPR_OY].s8 = &vi->sw.oy;
			cbor_var_spr[CBOR_VAR_SPR_OFFSETS].u32 = vi->sw.offset;
		} else
		{
			if(edit_check_name(vi->name, 0))
				return 1;

			vi->hash = x16c_crc(vi->name, -1, VAR_CRC_XOR);

			cbor_var_wall[CBOR_VAR_WALL_WIDTH].u16 = &vi->sw.width;
			cbor_var_wall[CBOR_VAR_WALL_HEIGHT].u16 = &vi->sw.height;
			cbor_var_wall[CBOR_VAR_WALL_MASKED].u8 = &vi->sw.masked;
			cbor_var_wall[CBOR_VAR_WALL_ANIMATION].ptr = vi->sw.anim;
			cbor_var_wall[CBOR_VAR_WALL_OFFSETS].u32 = vi->sw.offset;
		}

		cbor_load_entry = vi;

		cbor_entry_index++;

		ctx->entry_cb = cbor_gfx_stex_varent;

		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_stex_entry(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func = edit_cbor_branch(cbor_stex, type, key, ctx->key_len, value, ctx->val_len);

	if(func)
	{
		cbor_entry_index = 0;
		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_wall(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		if(!cbor_load_object)
			return 0;

		gfx_idx[GFX_MODE_WALLS].max = cbor_main_index;

		cbor_load_object = NULL;

		return 0;
	}

	if(type == KGCBOR_TYPE_OBJECT)
	{
		variant_list_t *wa = x16_wall + cbor_main_index;

		if(cbor_main_index >= MAX_X16_WALLS)
			return 1;

		if(ctx->key_len >= LEN_X16_TEXTURE_NAME)
			return 1;

		memset(wa, 0, sizeof(variant_list_t));
		memcpy(wa->name, key, ctx->key_len);
		wa->hash = x16c_crc(wa->name, -1, WALL_CRC_XOR);
		wa->stex_used = STEX_PIXEL_LIMIT;

		if(edit_check_name(wa->name, 0))
			return 1;

		cbor_load_object = wa;

		cbor_stex[CBOR_STEX_DATA].ptr = wa->data;
		cbor_stex[CBOR_STEX_DATA].extra = STEX_PIXEL_LIMIT * 2;
		cbor_main_index++;

		cbor_stex_is_sprite = 0;
		ctx->entry_cb = cbor_gfx_stex_entry;

		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_sprite(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		if(!cbor_load_object)
			return 0;

		gfx_idx[GFX_MODE_SPRITES].max = cbor_main_index;

		cbor_load_object = NULL;

		return 0;
	}

	if(type == KGCBOR_TYPE_OBJECT)
	{
		variant_list_t *sp = x16_sprite + cbor_main_index;

		if(cbor_main_index >= MAX_X16_THGSPR)
			return 1;

		if(ctx->key_len >= LEN_X16_TEXTURE_NAME)
			return 1;

		memset(sp, 0, sizeof(variant_list_t));
		memcpy(sp->name, key, ctx->key_len);
		sp->hash = x16c_crc(sp->name, -1, SPR_CRC_XOR);
		sp->stex_used = STEX_PIXEL_LIMIT;

		if(edit_check_name(sp->name, 0))
			return 1;

		cbor_load_object = sp;

		cbor_stex[CBOR_STEX_DATA].ptr = sp->data;
		cbor_stex[CBOR_STEX_DATA].extra = STEX_PIXEL_LIMIT;
		cbor_main_index++;

		cbor_stex_is_sprite = 1;
		ctx->entry_cb = cbor_gfx_stex_entry;

		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_wpn_part(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		if(load_part.width)
		{
			if(!check_part_resolution(load_part.width))
			{
				uint32_t height = 8;

				while(height < load_part.hack_height)
					height *= 2;

				load_part.height = height;

				if(!check_part_resolution(height))
				{
					*cbor_load_part = load_part;
					return 0;
				}
			}
		}

		memset(&load_part, 0, sizeof(load_part));

		return 0;
	}

	return edit_cbor_branch(cbor_wpn_part, type, key, ctx->key_len, value, ctx->val_len) == NULL;
}

static int32_t cbor_gfx_wpn_parr(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	if(ctx->index >= UI_WPN_PARTS)
		return 1;

	cbor_load_part = cbor_load_object->variant[cbor_entry_index-1].ws.part + ctx->index;

	memset(&load_part, 0, sizeof(load_part));
	ctx->entry_cb = cbor_gfx_wpn_part;

	return 0;
}

static int32_t cbor_gfx_wpn_variant(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	uint8_t frm;
	variant_info_t *va;

	if(type != KGCBOR_TYPE_ARRAY)
		return 1;

	if(ctx->key_len != 1)
		return 1;

	if(cbor_entry_index >= MAX_X16_VARIANTS)
		return 1;

	frm = key[0] - 'A';
	if(frm >= MAX_X16_SPRITE_FRAMES)
		return 1;

	for(uint32_t i = 0; i < cbor_entry_index; i++)
	{
		va = cbor_load_object->variant + i;
		if(va->wpn.frm == frm)
			return 1;
	}

	va = cbor_load_object->variant + cbor_entry_index;
	va->wpn.frm = frm;

	if(!cbor_load_entry)
		cbor_load_entry = va;

	cbor_entry_index++;

	ctx->entry_cb = cbor_gfx_wpn_parr;

	return 0;
}

static int32_t cbor_gfx_wpn_group(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func;

	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		if(cbor_load_entry)
		{
			uint32_t base = cbor_load_entry - cbor_load_object->variant;
			uint8_t text[LEN_X16_VARIANT_NAME];
			uint8_t *txt = text;
			uint32_t count = cbor_entry_index - base;
			uint32_t dbase, dsize;

			load_ndata += 255;
			load_ndata &= ~255;

			load_bdata += 255;
			load_bdata &= ~255;

			if(	count >= MAX_X16_WPNGROUP ||
				load_ndata + load_bdata > SWPN_MAX_DATA
			)
			{
				memset(cbor_load_object->variant + base, 0, sizeof(variant_info_t) * count);
				cbor_entry_index = base;
				return 0;
			}

			dbase = cbor_load_object->stex_used;
			dsize = load_ndata + load_bdata;

			if(STEX_PIXEL_LIMIT * 2 - (cbor_load_object->stex_used + dsize) < SWPN_MAX_DATA)
				// out of pixels; fatal error
				return -1;

			memcpy(cbor_load_object->data + cbor_load_object->stex_used, stex_data, load_ndata);
			cbor_load_object->stex_used += load_ndata;

			memcpy(cbor_load_object->data + cbor_load_object->stex_used, (uint8_t*)stex_data + SWPN_MAX_DATA, load_bdata);
			cbor_load_object->stex_used += load_bdata;

			for(uint32_t i = base; i < cbor_entry_index; i++)
			{
				variant_info_t *va = cbor_load_object->variant + i;

				*txt++ = va->wpn.frm + 'A';

				va->wpn.base = base;
				va->wpn.count = 0xFF;

				va->ws.dstart = dbase;
				va->ws.dsize = dsize;
				va->ws.dbright = load_ndata;

				for(uint32_t j = 0; j < UI_WPN_PARTS; j++)
				{
					wpnspr_part_t *part = va->ws.part + j;

					if(part->flags & WPN_PART_FLAG_BRIGHT)
						part->offset += load_ndata;

					if(part->offset >= dsize - part->width)
						part->width = 0;
				}
			}
			cbor_load_entry->wpn.count = count;

			for(uint32_t i = base; i < cbor_entry_index; i++)
			{
				variant_info_t *va = cbor_load_object->variant + i;
				uint8_t *name = va->name;

				for(uint32_t j = 0; j < count; j++)
				{
					uint8_t in = text[j];

					if(in == va->wpn.frm + 'A')
					{
						*name++ = '[';
						*name++ = in;
						*name++ = ']';
					} else
						*name++ = in | 0x20;
				}
				*name = 0;
			}

			cbor_load_entry = NULL;
		}
		return 0;
	}

	func = edit_cbor_branch(cbor_wpn_vgrp, type, key, ctx->key_len, value, ctx->val_len);
	if(func)
	{
		cbor_load_entry = NULL;
		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_swpn_entry(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		cbor_load_object->max = cbor_entry_index;
		return 0;
	}

	if(type == KGCBOR_TYPE_OBJECT)
	{
		if(cbor_entry_index >= MAX_X16_VARIANTS)
			return 1;

		load_ndata = 0;
		load_bdata = 0;
		ctx->entry_cb = cbor_gfx_wpn_group;

		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_weapon(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	if(type == KGCBOR_TYPE_TERMINATOR)
	{
		if(!cbor_load_object)
			return 0;

		gfx_idx[GFX_MODE_WEAPONS].max = cbor_main_index;

		cbor_load_object = NULL;

		return 0;
	}

	if(type == KGCBOR_TYPE_ARRAY)
	{
		variant_list_t *ws = x16_weapon + cbor_main_index;

		if(cbor_main_index >= MAX_X16_WPNSPR)
			return 1;

		if(ctx->key_len >= LEN_X16_TEXTURE_NAME)
			return 1;

		memset(ws, 0, sizeof(variant_list_t));
		memcpy(ws->name, key, ctx->key_len);
		ws->hash = x16c_crc(ws->name, -1, WPN_CRC_XOR);

		if(edit_check_name(ws->name, 0))
			return 1;

		cbor_load_object = ws;

		cbor_wpn_vgrp[CBOR_WPN_VGRP_NDATA].ptr = (uint8_t*)stex_data;
		cbor_wpn_vgrp[CBOR_WPN_VGRP_NDATA].extra = SWPN_MAX_DATA;
		cbor_wpn_vgrp[CBOR_WPN_VGRP_BDATA].ptr = (uint8_t*)stex_data + SWPN_MAX_DATA;
		cbor_wpn_vgrp[CBOR_WPN_VGRP_BDATA].extra = SWPN_MAX_DATA;
		cbor_main_index++;

		cbor_entry_index = 0;

		ctx->entry_cb = cbor_gfx_swpn_entry;

		return 0;
	}

	return 1;
}

static int32_t cbor_gfx_sky(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	editor_sky_t *sky;

	if(type == KGCBOR_TYPE_TERMINATOR_CB)
	{
		gfx_idx[GFX_MODE_SKIES].max = cbor_main_index;
		return 1;
	}

	if(type != KGCBOR_TYPE_BINARY)
		return 1;

	if(ctx->key_len >= LEN_X16_SKY_NAME)
		return 1;

	if(ctx->val_len != X16_SKY_DATA_SIZE)
		return 1;

	sky = editor_sky + cbor_main_index;

	memcpy(sky->name, key, ctx->key_len);
	sky->name[ctx->key_len] = 0;
	sky->hash = x16c_crc(sky->name, -1, SKY_CRC_XOR);

	if(edit_check_name(sky->name, 0))
		return 1;

	memcpy(sky->data, value->ptr, X16_SKY_DATA_SIZE);

	cbor_main_index++;

	return 1;
}

static int32_t cbor_gfx_font_entry(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	edit_cbor_branch(cbor_font, type, key, ctx->key_len, value, ctx->val_len);
	return 1;
}

static int32_t cbor_gfx_font(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	font_char_t *fc;

	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	if(ctx->index >= FONT_CHAR_COUNT)
		return 1;

	fc = font_char + ctx->index;

	cbor_font[CBOR_FONT_DATA].ptr = fc->data;
	cbor_font[CBOR_FONT_DATA].extra = 8 * 8 / 2;
	cbor_font[CBOR_FONT_SPACE].u8 = (uint8_t*)&fc->space;

	ctx->entry_cb = cbor_gfx_font_entry;

	return 0;
}

static int32_t cbor_gfx_nums(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	nums_char_t *nc;

	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	if(ctx->index >= NUMS_CHAR_COUNT)
		return 1;

	nc = nums_char + ctx->index;

	cbor_font[CBOR_FONT_DATA].ptr = nc->data;
	cbor_font[CBOR_FONT_DATA].extra = 16 * 8 / 2;
	cbor_font[CBOR_FONT_SPACE].u8 = (uint8_t*)&nc->space;

	ctx->entry_cb = cbor_gfx_font_entry;

	return 0;
}

static int32_t cbor_gfx_fullbright(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	uint32_t i;

	if(type != KGCBOR_TYPE_VALUE)
		return 1;

	i = value->u32;
	if(i && i < 256)
		x16_palette_bright[i >> 4] |= (1 << (i & 15));

	return 0;
}

static int32_t cbor_gfx_root(kgcbor_ctx_t *ctx, uint8_t *key, uint8_t type, kgcbor_value_t *value)
{
	void *func = edit_cbor_branch(cbor_root, type, key, ctx->key_len, value, ctx->val_len);

	if(func)
	{
		cbor_main_index = 0;
		ctx->entry_cb = func;
		return 0;
	}

	return 1;
}

//
// color stuff

static void negpos_offset(float *val)
{
	if(*val > 0)
		*val += 1.0f;
	else
	if(*val < 0)
		*val -= 1.0f;
}

static void generate_remap(uint32_t idx)
{
	color_remap_t *cr;

	for(uint32_t i = 0; i < 256; i++)
		remap_temp_data[i] = i;

	if(idx >= MAX_X16_REMAPS)
		return;

	cr = x16_color_remap + idx;

	for(uint32_t i = 0; i < cr->count; i++)
	{
		remap_entry_t *ent = cr->entry + i;

		if(ent->t)
		{
			// RGB
			float div = 1.0f + (float)ent->e - (float)ent->s;
			float value[3] = {ent->rgb.rs, ent->rgb.gs, ent->rgb.bs};
			float step[3] =
			{
				(float)ent->rgb.re - (float)ent->rgb.rs,
				(float)ent->rgb.ge - (float)ent->rgb.gs,
				(float)ent->rgb.be - (float)ent->rgb.bs,
			};

			negpos_offset(step + 0);
			negpos_offset(step + 1);
			negpos_offset(step + 2);

			step[0] /= div;
			step[1] /= div;
			step[2] /= div;

			for(uint32_t j = ent->s; j <= ent->e; j++)
			{
				if(!(x16_palette_bright[j >> 4] & (1 << (j & 15))))
				{
					uint32_t color;

					color = value[0];
					color |= (uint32_t)value[1] << 8;
					color |= (uint32_t)value[2] << 16;

					remap_temp_data[j] = x16g_palette_match(color | 0xFF000000, 0);
				}

				value[0] += step[0];
				value[1] += step[1];
				value[2] += step[2];
			}
		} else
		{
			// palette
			float value = ent->pal.s;
			float step = (float)ent->pal.e - (float)ent->pal.s;

			negpos_offset(&step);

			step /= (1.0f + (float)ent->e - (float)ent->s);

			for(uint32_t j = ent->s; j <= ent->e; j++)
			{
				if(!(x16_palette_bright[j >> 4] & (1 << (j & 15))))
					remap_temp_data[j] = value;
				value += step;
			}
		}
	}
}

static void generate_colormap(uint32_t idx, uint8_t *data, uint16_t bright)
{
	uint8_t *dst;

	dst = x16_colormap_data;
	dst += idx * 32;

	for(uint32_t i = 0; i < 16; i++)
	{
		*dst++ = *data++;
		*dst++ = !!(bright & (1 << i)) * 255;
	}
}

static void rgb2hsv(float *restrict rgb, float *restrict hsv)
{
	float cmin, cmax, delta, hue, sat;

	cmin = rgb[0];
	if(rgb[1] < cmin)
		cmin = rgb[1];
	if(rgb[2] < cmin)
		cmin = rgb[2];

	cmax = rgb[0];
	if(rgb[1] > cmax)
		cmax = rgb[1];
	if(rgb[2] > cmax)
		cmax = rgb[2];

	delta = cmax - cmin;

	if(delta == 0.0f)
		hue = 0.0f;
	else
	if(cmax == rgb[0])
	{
		hue = (rgb[1] - rgb[2]) / delta;
		if(hue < 0)
			hue += 6.0f;
	} else
	if(cmax == rgb[1])
		hue = (rgb[2] - rgb[0]) / delta + 2.0f;
	else
		hue = (rgb[0] - rgb[1]) / delta + 4.0f;

	if(cmax == 0.0f)
		sat = 0.0f;
	else
		sat = delta / cmax;

	hsv[0] = hue;
	hsv[1] = sat;
	hsv[2] = cmax;
}

static void hsv2rgb(float *restrict hsv, float *restrict rgb)
{
	float c, x, m;

	c = hsv[2] * hsv[1];

	m = hsv[2] - c;

	x = 1.0f - fabs(fmod(hsv[0], 2) - 1.0f);
	x *= c;

	if(hsv[0] < 1.0f)
	{
		rgb[0] = c;
		rgb[1] = x;
		rgb[2] = 0;
	} else
	if(hsv[0] < 2.0f)
	{
		rgb[0] = x;
		rgb[1] = c;
		rgb[2] = 0;
	} else
	if(hsv[0] < 3.0f)
	{
		rgb[0] = 0;
		rgb[1] = c;
		rgb[2] = x;
	} else
	if(hsv[0] < 4.0f)
	{
		rgb[0] = 0;
		rgb[1] = x;
		rgb[2] = c;
	} else
	if(hsv[0] < 5.0f)
	{
		rgb[0] = x;
		rgb[1] = 0;
		rgb[2] = c;
	} else
	{
		rgb[0] = c;
		rgb[1] = 0;
		rgb[2] = x;
	}

	rgb[0] += m;
	rgb[1] += m;
	rgb[2] += m;
}

static uint32_t color_drgb_process(uint32_t src, float des, float *mul, float *add, float *dhsv, float *dclr)
{
	float color[3];
	float val;
	uint8_t r, g, b;

	color[0] = (float)(src & 0xFF) / 255.0f;
	color[1] = (float)((src >> 8) & 0xFF) / 255.0f;
	color[2] = (float)((src >> 16) & 0xFF) / 255.0f;

	if(des < 0)
	{
		float hsv[3];
		rgb2hsv(color, hsv);
		val = hsv[2];
		des = -des;
	} else
		val = color[0] * (1.0f / 3.0f) + color[1] * (1.0f / 3.0f) + color[2] * (1.0f / 3.0f);

	if(dhsv)
	{
		float hsv[3];
		float clr[3];
		float vvv;

		vvv = val * 2.0f;

		hsv[0] = dhsv[0];

		if(vvv > 1.0f)
		{
			hsv[1] = 2.0f - vvv;
			hsv[2] = 1.0f;
		} else
		{
			hsv[1] = 1.0f;
			hsv[2] = vvv;
		}

		hsv2rgb(hsv, clr);

		if(dhsv[2] < 1.0f && dclr)
		{
			float alp, inv;

			alp = dhsv[2] * 2.0f;
			alp -= 1.0f;
			if(alp < 0.0f)
				alp = 0.0f;

			inv = 1.0f - alp;

			clr[0] = clr[0] * alp + inv * val * dclr[0];
			clr[1] = clr[1] * alp + inv * val * dclr[1];
			clr[2] = clr[2] * alp + inv * val * dclr[2];
		}

		if(des > 0.0f)
		{
			float inv = 1.0f - des;

			color[0] = dclr[0] * val * inv + des * clr[0];
			color[1] = dclr[1] * val * inv + des * clr[1];
			color[2] = dclr[2] * val * inv + des * clr[2];
		} else
		{
			color[0] = clr[0];
			color[1] = clr[1];
			color[2] = clr[2];
		}
	} else
	if(des > 0.0f)
	{
		float inv = 1.0f - des;

		if(dclr)
		{
			color[0] = color[0] * inv + des * val * dclr[0];
			color[1] = color[1] * inv + des * val * dclr[1];
			color[2] = color[2] * inv + des * val * dclr[2];
		} else
		{
			color[0] = color[0] * inv + des * val;
			color[1] = color[1] * inv + des * val;
			color[2] = color[2] * inv + des * val;
		}
	}

	if(mul)
	{
		color[0] *= mul[0];
		color[1] *= mul[1];
		color[2] *= mul[2];
	}

	if(add)
	{
		color[0] += add[0];
		color[1] += add[1];
		color[2] += add[2];
	}

	if(color[0] > 1.0f)
		color[0] = 1.0f;
	if(color[1] > 1.0f)
		color[1] = 1.0f;
	if(color[2] > 1.0f)
		color[2] = 1.0f;

	r = color[0] * 255.0f;
	g = color[1] * 255.0f;
	b = color[2] * 255.0f;

	src = r;
	src |= g << 8;
	src |= b << 16;

	return src;
}

static void make_light_data(uint32_t idx, uint32_t rdx)
{
	editor_light_t *el = editor_light + idx;
	uint8_t *dst = x16_light_data + idx * 256;
	float des;
	float clr[3];
	float hsv[3];
	float rgb[3];
	float *dhsv = NULL;
	float *dmul = NULL;
	float *drgb = NULL;

	generate_remap(rdx - 1);

	if(!el->des && el->r == 100 && el->g == 100 && el->b == 100)
	{
		for(uint32_t i = 1; i < 256; i++)
			dst[i] = remap_temp_data[i];
		return;
	}

	des = fabs((float)el->des / 100.0f);
	clr[0] = (float)el->r / 100.0f;
	clr[1] = (float)el->g / 100.0f;
	clr[2] = (float)el->b / 100.0f;

	if(el->des < 0)
	{
		float tmp[3];

		rgb2hsv(clr, hsv);

		tmp[0] = hsv[0];
		tmp[1] = 1.0f;
		tmp[2] = 1.0f;
		hsv2rgb(tmp, rgb);

		dhsv = hsv;
		drgb = rgb;
	} else
		dmul = clr;

	for(uint32_t i = 1; i < 256; i++)
	{
		uint32_t tmp;

		if(!(x16_palette_bright[i >> 4] & (1 << (i & 15))))
		{
			tmp = x16_palette_data[remap_temp_data[i]];
			tmp = color_drgb_process(tmp, des, dmul, NULL, dhsv, drgb);
			dst[i] = x16g_palette_match(tmp | 0xFF000000, 0);
		} else
			dst[i] = i;
	}
}

static void pal_apply_tint(uint32_t dst, uint32_t src, float des, float *mul, float *add, float *dclr)
{
	uint32_t *psrc = x16_palette_data + src * 256;
	uint32_t *pdst = x16_palette_data + dst * 256;

	for(uint32_t i = 1; i < 256; i++)
	{
		uint32_t tmp;

		tmp = psrc[i];
		tmp = color_drgb_process(tmp, des, mul, add, NULL, dclr);

		tmp &= 0xF0F0F0;
		tmp |= tmp >> 4;

		pdst[i] = tmp | 0xFF000000;
	}
}

static void pal_tint()
{
	float des;
	float val[3];
	float add[3];
	float *m = NULL;
	float *d = NULL;

	des = (float)palette_options.tint.des / 100.0f;

	val[0] = (float)palette_options.tint.mr / 100.0f;
	val[1] = (float)palette_options.tint.mg / 100.0f;
	val[2] = (float)palette_options.tint.mb / 100.0f;

	add[0] = (float)palette_options.tint.ar / 100.0f;
	add[1] = (float)palette_options.tint.ag / 100.0f;
	add[2] = (float)palette_options.tint.ab / 100.0f;

	if(des < 0)
		d = val;
	else
		m = val;

	pal_apply_tint(gfx_idx[GFX_MODE_PALETTE].now, gfx_idx[GFX_MODE_PALETTE].now, des, m, add, d);

	x16g_update_texture(X16G_GLTEX_PALETTE);

	edit_status_printf("Tint applied.");
}

static void pal_dmg_single(uint32_t idx)
{
	float des = 0;
	float add[3] = {0, 0, 0};
	float color[3];
	float sa[3];
	float sd;
	float tmp;

	sd = (float)palette_options.dmg.des / 100.0f;

	tmp = (float)palette_options.dmg.add / 100.0f;
	color[0] = (float)palette_options.dmg.r / 100.0f;
	color[1] = (float)palette_options.dmg.g / 100.0f;
	color[2] = (float)palette_options.dmg.b / 100.0f;

	sa[0] = color[0] * tmp;
	sa[1] = color[1] * tmp;
	sa[2] = color[2] * tmp;

	idx &= ~3;

	for(uint32_t i = 1; i < 4; i++)
	{
		add[0] += sa[0];
		add[1] += sa[1];
		add[2] += sa[2];
		des += sd;

		if(des > 1.0f)
			des = 1.0f;

		pal_apply_tint(i + idx, idx, des, NULL, add, color);
	}
}

static void pal_dmg(uint32_t all)
{
	if(all)
	{
		for(uint32_t i = 0; i < 16; i += 4)
			pal_dmg_single(i);
	} else
		pal_dmg_single(gfx_idx[GFX_MODE_PALETTE].now);

	x16g_update_texture(X16G_GLTEX_PALETTE);

	edit_status_printf("Damage tint generated.");
}

static void light_recalc()
{
	make_light_data(gfx_idx[GFX_MODE_LIGHTS].now, 0);
	x16g_update_texture(X16G_GLTEX_LIGHTS);
}

static void recalc_lights()
{
	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_LIGHTS].max; i++)
		make_light_data(i, 0);
}

//
// plane export

static void place_plane_tile(uint8_t *dst, uint8_t *src, uint32_t tx, uint32_t ty)
{
	for(uint32_t y = 0; y < 8; y++)
	{
		uint8_t *ss = src;
		for(uint32_t x = 0; x < 8; x++)
		{
			*dst++ = *ss;
			ss += ty;
		}
		src++;
	}
}

static void make_plane_tiles(uint8_t *dst, uint8_t *src, uint32_t tx, uint32_t ty)
{
	for(uint32_t y = 0; y < ty; y += 8)
	{
		uint8_t *ss = src;
		for(uint32_t x = 0; x < tx; x += 8)
		{
			place_plane_tile(dst, ss, tx, ty);
			dst += 8 * 8;
			ss += ty * 8;
		}
		src += 8;
	}
}

//
// map item search

static uint32_t find_plane_texture(uint8_t *name)
{
	if(!name[0])
	{
		name[0] = '\t';
		name[1] = 0;
		return 0;
	}

	for(uint32_t i = 0; i < editor_texture_count; i++)
	{
		editor_texture_t *et = editor_texture + i;

		if(	et->type != X16G_TEX_TYPE_PLANE_8BPP &&
			et->type != X16G_TEX_TYPE_PLANE_4BPP &&
			et->type != X16G_TEX_TYPE_RGB
		)
			continue;

		if(!strcmp(et->name, name))
			return i;
	}

	return 0;
}

static uint32_t find_wall_texture(uint8_t *name)
{
	uint32_t fallback = 0;

	if(!name[0])
	{
		name[0] = '\t';
		name[1] = 0;
		return 0;
	}

	for(uint32_t i = 0; i < editor_texture_count; i++)
	{
		editor_texture_t *et = editor_texture + i;

		if(et->type == X16G_TEX_TYPE_WALL_MASKED)
			continue;

		if(!strcmp(et->name, name))
		{
			if(	et->type == X16G_TEX_TYPE_PLANE_8BPP ||
				et->type == X16G_TEX_TYPE_PLANE_4BPP
			)
				fallback = i;
			else
				return i;
		}
	}

	return fallback;
}

static uint32_t find_mask_texture(uint8_t *name)
{
	if(!name[0] || name[0] == '\t')
	{
		name[0] = '\t';
		name[1] = 0;
		return 0;
	}

	for(uint32_t i = 0; i < editor_texture_count; i++)
	{
		editor_texture_t *et = editor_texture + i;

		if(et->type != X16G_TEX_TYPE_WALL_MASKED)
			continue;

		if(!strcmp(et->name, name))
			return i;
	}

	return 0;
}

static uint32_t find_sector_light(uint8_t *name)
{
	for(uint32_t i = 0; i < x16_num_lights; i++)
	{
		if(!strcmp(editor_light[i].name, name))
			return i;
	}

	return 0;
}

//
// mode

static void update_gfx_mode(int32_t step)
{
	ui_idx_t *idx = gfx_idx + gfx_mode;
	uint8_t text[16];
	const uint8_t *tptr;

	ui_gfx_plane_texture.base.disabled = 1;

	if(step)
	{
		idx->now += step;
		if(idx->now >= idx->max)
			idx->now = 0;
		if(idx->now < 0)
			idx->now = idx->max - 1;
	}

	if(idx->max)
	{
		sprintf(text, "%u / %u", idx->now + 1, idx->max);
		glui_set_text(&ui_gfx_idx, text, glui_font_huge_kfn, GLUI_ALIGN_TOP_CENTER);

		tptr = ui_set[gfx_mode].update(idx);

		glui_set_text(&ui_gfx_title, tptr, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	} else
	{
		ui_set[gfx_mode].update(NULL);
		glui_set_text(&ui_gfx_idx, "---", glui_font_huge_kfn, GLUI_ALIGN_TOP_CENTER);
		glui_set_text(&ui_gfx_title, "[no entries]", glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);
	}
}

static void set_gfx_mode(uint32_t mode)
{
	stex_wpn_import = 0;
	wpn_part_copy = -1;

	gfx_mode = mode;

	for(uint32_t i = 0; i < GFX_NUM_MODES; i++)
	{
		ui_set[i].cont->base.disabled = 1;
		ui_set[i].text->color[0] = 0xFFC0C0C0;
	}

	ui_set[mode].cont->base.disabled = 0;
	ui_set[mode].text->color[0] = 0xFF0077FF;

	update_gfx_mode(0);
}

//
// striped texture management

static void stex_reset(uint8_t *source, uint32_t count)
{
	stex_size = 0;
	stex_total = 0;
	stex_fullbright = 1;

	memcpy(stex_source, source, count * 2);

	for(uint32_t i = 0; i < STEX_PIXEL_LIMIT / 256; i++)
		stex_space[i] = 256;

	memset(stex_data, 0, sizeof(stex_data));
}

static int32_t stex_insert(uint16_t *data, uint32_t len)
{
	uint16_t *ptr = stex_data;
	uint16_t *fitp = NULL;
	uint16_t *fits = NULL;
	uint32_t offs, used;

	stex_total += len;

	// go trough existing data
	for(uint32_t i = 0; i < STEX_PIXEL_LIMIT / 256; i++, ptr += 256)
	{
		int32_t used = 256 - (int32_t)stex_space[i];
		int32_t check = used - len;
		uint16_t *src = ptr;

		if(!fitp && stex_space[i] >= len)
		{
			fitp = ptr + used;
			fits = stex_space + i;
		}

		if(check < 0)
			continue;

		for(uint32_t j = 0; j <= check; j++)
		{
			if(!memcmp(src, data, len * 2))
			{
				// found matching data
				return src - stex_data;
			}
			src++;
		}
	}

	// check free space for new data
	if(!fitp)
		return -1;

	// mark used length
	*fits -= len;

	// copy new data and check fullbright
	for(uint32_t i = 0; i < len; i++)
	{
		fitp[i] = data[i];
		if(!(data[i] & 0xFF00))
			stex_fullbright = 0;
	}

	// done
	offs = fitp - stex_data;
	used = (offs + len + 255) & ~255;
	if(used > stex_size)
		stex_size = used;

	return offs;
}

static uint32_t stex_remake_columns(variant_list_t *vl)
{
	uint8_t ll[257];

	memset(ll, 0, sizeof(ll));

	// detect all column lengths
	for(uint32_t i = 0; i < vl->max; i++)
	{
		variant_info_t *vi = vl->variant + i;

		for(uint32_t j = 0; j < vi->sw.width; j++)
			ll[vi->sw.length[j]] = 1;
	}

	// reset buffers
	stex_reset(vl->data, vl->stex_used);

	// add all columns
	for(uint32_t l = 256; l > 0; l--)
	{
		if(!ll[l])
			continue;

		for(uint32_t i = 0; i < vl->max; i++)
		{
			variant_info_t *vi = vl->variant + i;

			for(uint32_t j = 0; j < vi->sw.width; j++)
			{
				uint32_t len = vi->sw.length[j];
				uint32_t offs = vi->sw.offset[j];
				int32_t ret;

				if(len != l)
					continue;

				ret = stex_insert(stex_source + offs, len);
				if(ret < 0)
					return 1;

				vi->sw.temp[j] = ret;
			}
		}
	}

	// apply new data
	memcpy(vl->data, stex_data, stex_size * 2);
	vl->stex_used = stex_size;
	vl->stex_total = stex_total;
	vl->stex_fullbright = stex_fullbright;

	for(uint32_t i = 0; i < vl->max; i++)
	{
		variant_info_t *vi = vl->variant + i;

		for(uint32_t j = 0; j < vi->sw.width; j++)
			vi->sw.offset[j] = vi->sw.temp[j];
	}

	return 0;
}

static uint16_t stex_read_color(uint32_t **src, uint32_t width)
{
	uint16_t color;
	uint32_t in;

	in = **src;
	*src = *src + width;

	if(!(in & 0xFF000000))
		return 0xFF00;

	if((in & 0xFF000000) == 0xFF000000)
		color = 0;
	else
		color = 0xFF00;

	color |= x16g_palette_match(in, 1);

	return color;
}

static uint32_t stex_wall_masked_row(uint16_t *dst, uint16_t *src, uint32_t height)
{
	uint16_t *px = src;
	uint16_t *last, *head, *top;
	uint32_t len;

	// add dummy transparent pixel
	src[height] = 0xFF00;

	// find first non-transparent pixel
	for(uint32_t y = 0; y < height && *px == 0xFF00; y++)
		px++;

	if(*px != 0xFF00)
	{
		// make full column
		top = px;

		head = dst;
		dst += 2;

		// find last non-transparent pixel
		while(px < src + height)
		{
			if(*px != 0xFF00)
				last = px;
			px++;
		}

		// pixel count
		len = 1 + last - top;
		head[0] = 0xFF00 | len;

		// bottom offset
		head[1] = 0xFF00 | (src + height - last - 1);

		// copy pixels
		for(uint32_t i = 0; i < len; i++)
			*dst++ = *last--;

		// append transparent pixels
		// this hides imprecise scaling
		*dst++ = 0xFF00;
		*dst++ = 0xFF00;
		*dst++ = 0xFF00;

		// get length
		len += 5;
	} else
	{
		// make generic "empty column"
		*dst++ = 0xFF00;
		len = 1;
	}

	return len;
}

static uint32_t stex_wall_texture_32(image_t *img, variant_list_t *wl)
{
	variant_info_t *vi = wl->variant + wl->now;
	uint32_t offset = wl->stex_used;
	uint32_t *src = (uint32_t*)img->data;
	uint16_t *dst = stex_source + offset;
	uint32_t is_masked = 0;
	uint16_t data[256];

	vi->sw.width = img->width;
	vi->sw.stride = (img->width + 3) & ~3;
	vi->sw.height = img->height;

	for(uint32_t i = 0; i < img->width * img->height; i++)
	{
		if(!(src[i] & 0xFF000000))
		{
			is_masked = 1;
			break;
		}
	}

	if(!is_masked)
	{
		if(check_wall_resolution(img->width, img->height))
			return 1;

		for(uint32_t x = 0; x < img->width; x++)
		{
			uint32_t *ss = src++;

			for(uint32_t y = 0; y < img->height; y++)
				*dst++ = stex_read_color(&ss, img->width);

			vi->sw.offset[x] = offset;
			vi->sw.length[x] = img->height;

			offset += img->height;
		}

		vi->sw.masked = 0;

		return 0;
	}

	if(check_masked_resolution(img->width, img->height))
		return 1;

	for(uint32_t x = 0; x < img->width; x++)
	{
		uint32_t *ss = src++;
		uint32_t len;

		for(uint32_t y = 0; y < img->height; y++)
			data[y] = stex_read_color(&ss, img->width);

		len = stex_wall_masked_row(dst, data, img->height);

		dst += len;

		vi->sw.offset[x] = offset;
		vi->sw.length[x] = len;

		offset += len;
	}

	vi->sw.masked = 1;

	return 0;
}

static uint32_t stex_wall_texture_8(image_t *img, variant_list_t *wl)
{
	variant_info_t *vi = wl->variant + wl->now;
	uint32_t offset = wl->stex_used;
	uint8_t *src = img->data;
	uint16_t *dst = stex_source + offset;
	uint32_t is_masked = 0;
	uint16_t data[256];

	vi->sw.width = img->width;
	vi->sw.stride = (img->width + 3) & ~3;
	vi->sw.height = img->height;

	for(uint32_t i = 0; i < img->width * img->height; i++)
	{
		if(!src[i])
		{
			is_masked = 1;
			break;
		}
	}

	if(!is_masked)
	{
		if(check_wall_resolution(img->width, img->height))
			return 1;

		for(uint32_t x = 0; x < img->width; x++)
		{
			uint8_t *ss = src++;

			for(uint32_t y = 0; y < img->height; y++)
			{
				*dst++ = *ss;
				ss += img->width;
			}

			vi->sw.offset[x] = offset;
			vi->sw.length[x] = img->height;

			offset += img->height;
		}

		vi->sw.masked = 0;

		return 0;
	}

	if(check_masked_resolution(img->width, img->height))
		return 1;

	for(uint32_t x = 0; x < img->width; x++)
	{
		uint8_t *ss = src++;
		uint32_t len;

		for(uint32_t y = 0; y < img->height; y++)
		{
			uint16_t in = *ss;
			if(!in)
				in = 0xFF00;
			data[y] = in;
			ss += img->width;
		}

		len = stex_wall_masked_row(dst, data, img->height);

		dst += len;

		vi->sw.offset[x] = offset;
		vi->sw.length[x] = len;

		offset += len;
	}

	vi->sw.masked = 1;

	return 0;
}

static uint32_t stex_sprite_texture(image_t *img, variant_list_t *sp)
{
	variant_info_t *vi = sp->variant + sp->now;
	uint32_t offset = sp->stex_used;
	void *src = img->data;
	uint16_t *dst = stex_source + offset;
	uint32_t last_used = 0;
	uint32_t xx = 0;
	uint16_t data[256];

	// save info
	vi->sw.width = img->width;
	vi->sw.stride = (img->width + 3) & ~3;
	vi->sw.height = img->height;

	// add dummy transparent pixel
	data[img->height] = 0xFF00;

	// go trough columns
	for(uint32_t x = 0; x < img->width; x++)
	{
		uint16_t *px = data;
		uint16_t *last, *head, *top;
		uint32_t len;

		// generate full column
		if(stex_import_pal)
		{
			uint8_t *ss = src;
			src++;
			for(uint32_t y = 0; y < img->height; y++)
			{
				data[y] = *ss;
				ss += img->width;
			}
		} else
		{
			uint32_t *ss = src;
			src += sizeof(uint32_t);
			for(uint32_t y = 0; y < img->height; y++)
				data[y] = stex_read_color(&ss, img->width) | 0xFF00;
		}

		px = data;

		// find first non-transparent pixel
		for(uint32_t y = 0; y < img->height && *px == 0xFF00; y++)
			px++;

		if(*px != 0xFF00)
		{
			last_used = x + 1;

			// make full column
			top = px;

			head = dst;
			dst += 2;

			// find last non-transparent pixel
			while(px < data + img->height)
			{
				if(*px != 0xFF00)
					last = px;
				px++;
			}

			// pixel count
			len = 1 + last - top;
			head[0] = 0xFF00 | len;

			// bottom offset
			head[1] = 0xFF00 | (data + img->height - last - 1);

			// copy pixels
			for(uint32_t i = 0; i < len; i++)
				*dst++ = *last--;

			// append transparent pixels
			// this hides imprecise scaling
			*dst++ = 0xFF00;
			*dst++ = 0xFF00;
			*dst++ = 0xFF00;

			// get length
			len += 5;
		} else
		if(last_used)
		{
			// make generic "empty column"
			*dst++ = 0xFF00;
			len = 1;
		} else
		{
			// don't add
			vi->sw.width--;
			continue;
		}

		vi->sw.offset[xx] = offset;
		vi->sw.length[xx] = len;
		xx++;

		offset += len;
	}

	if(!last_used)
	{
		// it's empty!
		vi->sw.width = 0;
		vi->sw.stride = 0;
		vi->sw.height = 0;
		return 1;
	}

	if(last_used < img->width)
	{
		vi->sw.width -= img->width - last_used;
		vi->sw.stride = (img->width + 3) & ~3;
	}

	return 0;
}

static void stex_parse_columns(variant_list_t *vl, variant_info_t *vi)
{
	for(uint32_t x = 0; x < vi->sw.width; x++)
	{
		uint16_t *ptr = (uint16_t*)vl->data + vi->sw.offset[x];
		uint8_t len, offs;

		if(ptr >= (uint16_t*)vl->data + STEX_PIXEL_LIMIT)
		{
is_invalid:
			// bad data; make empty
			vi->sw.width = 0;
			vi->sw.height = 0;
			vi->sw.stride = 0;
			break;
		}

		if((*ptr & 0xFF00) != 0xFF00)
			goto is_invalid;

		len = *ptr++;
		if(len)
		{
			if((*ptr & 0xFF00) != 0xFF00)
				goto is_invalid;
			offs = *ptr++;

			if(len + offs > vi->sw.height)
				goto is_invalid;

			ptr += len;
			if(ptr + 3 > (uint16_t*)vl->data + STEX_PIXEL_LIMIT)
				goto is_invalid;

			// check for transparent pixels
			if(*ptr != 0xFF00)
				goto is_invalid;

			ptr++;
			if(*ptr != 0xFF00)
				goto is_invalid;

			ptr++;
			if(*ptr != 0xFF00)
				goto is_invalid;

			vi->sw.length[x] = len + 5;
		} else
			vi->sw.length[x] = 1;
	}
}

static uint32_t stex_export_cbor(kgcbor_gen_t *gen, variant_list_t *vl, uint32_t count, uint32_t is_sprite)
{
	uint8_t temp[STEX_PIXEL_LIMIT];

	if(kgcbor_put_object(gen, count))
		return 1;

	while(count--)
	{
		if(kgcbor_put_string(gen, vl->name, -1))
			return 1;

		if(is_sprite)
		{
			uint8_t *dst = temp;
			uint16_t *src = (uint16_t*)vl->data;

			for(uint32_t i = 0; i < vl->stex_used; i++)
				*dst++ = *src++;

			cbor_stex[CBOR_STEX_DATA].ptr = temp;
			cbor_stex[CBOR_STEX_DATA].extra = vl->stex_used;
		} else
		{
			cbor_stex[CBOR_STEX_DATA].ptr = vl->data;
			cbor_stex[CBOR_STEX_DATA].extra = vl->stex_used * 2;
		}

		if(edit_cbor_export(cbor_stex, NUM_CBOR_STEX, gen))
			return 1;

		if(kgcbor_put_string(gen, cbor_stex[CBOR_STEX_VARIANTS].name, cbor_stex[CBOR_STEX_VARIANTS].nlen))
			return 1;

		if(kgcbor_put_object(gen, vl->max))
			return 1;

		for(uint32_t i = 0; i < vl->max; i++)
		{
			variant_info_t *vi = vl->variant + i;

			if(kgcbor_put_string(gen, vi->name, -1))
				return 1;

			if(is_sprite)
			{
				cbor_var_spr[CBOR_VAR_SPR_WIDTH].u16 = &vi->sw.width;
				cbor_var_spr[CBOR_VAR_SPR_HEIGHT].u16 = &vi->sw.height;
				cbor_var_spr[CBOR_VAR_SPR_OX].s8 = &vi->sw.ox;
				cbor_var_spr[CBOR_VAR_SPR_OY].s8 = &vi->sw.oy;
				if(edit_cbor_export(cbor_var_spr, NUM_CBOR_VAR_SPR, gen))
					return 1;
				if(kgcbor_put_string(gen, cbor_var_spr[CBOR_VAR_SPR_OFFSETS].name, cbor_var_spr[CBOR_VAR_SPR_OFFSETS].nlen))
					return 1;
			} else
			{
				cbor_var_wall[CBOR_VAR_WALL_WIDTH].u16 = &vi->sw.width;
				cbor_var_wall[CBOR_VAR_WALL_HEIGHT].u16 = &vi->sw.height;
				cbor_var_wall[CBOR_VAR_WALL_MASKED].u8 = &vi->sw.masked;
				cbor_var_wall[CBOR_VAR_WALL_ANIMATION].s8 = vi->sw.anim;
				if(edit_cbor_export(cbor_var_wall, NUM_CBOR_VAR_WALL, gen))
					return 1;
				if(kgcbor_put_string(gen, cbor_var_wall[CBOR_VAR_WALL_OFFSETS].name, cbor_var_wall[CBOR_VAR_WALL_OFFSETS].nlen))
					return 1;
			}

			if(kgcbor_put_array(gen, vi->sw.width))
				return 1;

			for(uint32_t x = 0; x < vi->sw.width; x++)
			{
				if(kgcbor_put_u32(gen, vi->sw.offset[x]))
					return 1;
			}
		}

		vl++;
	}

	return 0;
}

static void stex_generate_wall(variant_list_t *wa, variant_info_t *va, uint16_t *data)
{
	if(va->sw.masked)
	{
		memset(data, 0, va->sw.width * va->sw.height * sizeof(uint16_t));

		for(uint32_t x = 0; x < va->sw.width; x++)
		{
			uint16_t *src = (uint16_t*)wa->data + va->sw.offset[x];
			uint16_t *dst;
			uint8_t offs, len;

			len = *src++;
			if(len)
			{
				offs = *src++;

				dst = data + va->sw.height * va->sw.stride;
				dst -= offs * va->sw.stride;

				for(uint32_t i = 0; i < len; i++)
				{
					dst -= va->sw.stride;
					*dst = *src++;
				}
			}

			data++;
		}

		return;
	}

	for(uint32_t x = 0; x < va->sw.width; x++)
	{
		uint16_t *dst = data++;
		uint16_t *src = (uint16_t*)wa->data + va->sw.offset[x];

		for(uint32_t y = 0; y < va->sw.height; y++)
		{
			uint16_t in = *src++;
			uint8_t pal = in & 0xFF;

			if(x16_palette_bright[pal >> 4] & (1 << (pal & 15)))
				in |= 0xFF00;

			*dst = in;
			dst += va->sw.stride;
		}
	}
}

static void stex_generate_sprite(variant_list_t *wa, variant_info_t *va, uint16_t *data)
{
	for(uint32_t x = 0; x < va->sw.width; x++)
	{
		uint16_t *src = (uint16_t*)wa->data + va->sw.offset[x];
		uint16_t *dst;
		uint8_t offs, len;

		len = *src++;
		if(len)
		{
			offs = *src++;

			dst = data + va->sw.height * va->sw.stride;
			dst -= offs * va->sw.stride;

			for(uint32_t i = 0; i < len; i++)
			{
				uint16_t in = *src++;
				uint8_t pal = in & 0xFF;

				if(!(x16_palette_bright[pal >> 4] & (1 << (pal & 15))))
					in &= 0x00FF;

				dst -= va->sw.stride;
				*dst = in;
			}
		}

		data++;
	}
}

static void stex_x16_export_wall(uint8_t *buffer, uint8_t *txt)
{
	variant_list_t *vl = x16_wall;
	uint32_t count = gfx_idx[GFX_MODE_WALLS].max;

	for(uint32_t i = 0; i < count; i++, vl++)
	{
		uint8_t *ptr = buffer;
		uint16_t tmp;

		if(!vl->max)
			continue;

		if(!vl->stex_used)
			continue;

		*ptr++ = vl->stex_used >> 8;
		*ptr++ = vl->max | (vl->stex_fullbright * 0x80);

		memcpy(ptr, vl->data, vl->stex_used * 2);
		ptr += vl->stex_used * 2;

		for(uint32_t j = 0; j < vl->max; j++)
		{
			variant_info_t *vi = vl->variant + j;
			uint32_t width = vi->sw.width;
			uint8_t *ptl;
			uint16_t offset;

			*((uint32_t*)ptr) = vi->hash;
			ptr += sizeof(uint32_t);

			if(vi->sw.masked)
				*ptr = 0x40;
			else
			switch(vi->sw.height)
			{
				case 256:
					*ptr = 0;
				break;
				case 128:
					*ptr = 2;
				break;
				case 64:
					*ptr = 4;
				break;
				case 32:
					*ptr = 6;
				break;
				case 16:
					*ptr = 8;
				break;
				case 8:
					*ptr = 10;
				break;
				default:
					*ptr = 12; // invalid
				break;
			}
			ptr++;

			*ptr++ = vi->sw.anim[0];
			*ptr++ = vi->sw.anim[1];
			*ptr++ = vi->sw.anim[2];

			ptl = ptr;
			ptr += 128;

			for(uint32_t x = 0; x < 128; x++)
			{
				uint16_t offset = vi->sw.offset[x & (vi->sw.width-1)];
				*ptl++ = offset;
				*ptr++ = offset >> 8;
			}
		}

		sprintf(txt, X16_PATH_EXPORT PATH_SPLIT_STR "%08X.WAL", vl->hash);
		edit_save_file(txt, buffer, ptr - buffer);
	}
}

static void stex_x16_export_sprite(uint8_t *buffer, uint8_t *txt)
{
	variant_list_t *vl = x16_sprite;
	uint32_t count = gfx_idx[GFX_MODE_SPRITES].max;

	for(uint32_t i = 0; i < count; i++, vl++)
	{
		uint8_t *ptr = buffer;
		uint16_t *src = (uint16_t*)vl->data;
		uint16_t tmp;
		uint16_t stopoffs = 0;

		if(!vl->max)
			continue;

		if(!vl->stex_used)
			continue;

		*ptr++ = vl->stex_used >> 8;
		*ptr++ = vl->max;

		for(uint32_t i = 0; i < vl->stex_used; i++)
			*ptr++ = *src++;

		src = (uint16_t*)vl->data;
		for(uint32_t i = 0; i < vl->stex_used; i++)
		{
			if(*src == 0xFF00)
			{
				stopoffs = i;
				break;
			}
			src++;
		}

		for(uint32_t j = 0; j < vl->max; j++)
		{
			variant_info_t *vi = vl->variant + j;
			uint8_t *ptl;
			uint16_t offset;
			uint32_t x;

			// save offsets
			vi->spr.ox = vi->sw.width / 2 + vi->sw.ox;
			vi->spr.oy = vi->sw.oy;

			*((uint32_t*)ptr) = vi->hash;
			ptr += sizeof(uint32_t);

			*ptr++ = vi->sw.width * 2; // 'half pixel' width
			*ptr++ = vi->sw.height * 2; // 'half pixel' height

			ptl = ptr;
			ptr += 128;

			for(x = 0; x < vi->sw.width; x++)
			{
				uint16_t offset = vi->sw.offset[x];
				*ptl++ = offset;
				*ptr++ = offset >> 8;
			}

			for( ; x < 128; x++)
			{
				*ptl++ = stopoffs;
				*ptr++ = stopoffs >> 8;
			}
		}

		sprintf(txt, X16_PATH_EXPORT PATH_SPLIT_STR "%08X.THS", vl->hash);
		edit_save_file(txt, buffer, ptr - buffer);
	}
}

//
// weapon sprite management

static uint32_t swpn_add_part(wpnspr_part_t *part, uint8_t *data)
{
	int32_t size = part->width * part->hack_height;

	size += 31;
	size &= ~31;

	for(int32_t i = stex_fullbright; i <= (int32_t)stex_size - size; i += 32)
	{
		if(!memcmp(data, (uint8_t*)stex_data + i, size))
		{
			part->offset = i;
			return 0;
		}
	}

	if(stex_size + size > SWPN_MAX_DATA)
		return 1;

	part->offset = stex_size;

	memcpy((uint8_t*)stex_data + stex_size, data, size);
	stex_size += size;

	return 0;
}

static uint32_t swpn_add_pnew(wpnspr_part_t *part, int32_t width, int32_t height)
{
	uint8_t buff[64 * 64];
	uint8_t *dst = buff;
	int32_t diff;

	part->hack_height = part->height;

	diff = height - (part->y + part->height);
	if(diff < 0)
		part->hack_height += diff;

	for(int32_t y = part->y; y < part->y + part->hack_height; y++)
	{
		uint8_t *src;

		if(y < 0 || y >= height)
		{
			for(int32_t x = 0; x < part->width; x++)
				*dst++ = 0;
			continue;
		}

		src = (uint8_t*)stex_source + y * width + part->x;

		for(int32_t x = part->x; x < part->x + part->width; x++)
		{
			if(x < 0 || x >= width)
			{
				*dst++ = 0;
				src++;
				continue;
			}

			*dst++ = *src++;
		}
	}

	return swpn_add_part(part, buff);
}

static void swpn_clear_data()
{
	variant_list_t *vl = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	variant_info_t *va = vl->variant + vl->now;
	variant_info_t *vm = va;
	uint32_t dbase, dsize;
	uint32_t top, pot;

	top = va->wpn.base;
	va = vl->variant + top;

	if(!va->ws.dsize)
		return;

	pot = top + va->wpn.count;

	dbase = va->ws.dstart;
	dsize = va->ws.dsize;

	memcpy(vl->data + dbase, vl->data + dbase + dsize, vl->stex_used - (dbase + dsize));
	vl->stex_used -= dsize;

	va = vl->variant;
	for(uint32_t i = 0; i < vl->max; i++, va++)
	{
		if(i >= top && i < pot)
		{
			va->ws.dstart = 0;
			va->ws.dsize = 0;
			va->ws.dbright = 0;
			memset(va->ws.part, 0, sizeof(wpnspr_part_t) * UI_WPN_PARTS);
		} else
		{
			if(va->ws.dstart >= dbase)
				va->ws.dstart -= dsize;
		}
	}
}

static int32_t swpn_apply()
{
	variant_list_t *ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	variant_info_t *va = ws->variant + ws->now;
	uint32_t width = stex_wpn_import & 0xFFFF;
	uint32_t height = stex_wpn_import >> 16;
	uint32_t count;

	if(!stex_wpn_import)
		return -1;

	if(STEX_PIXEL_LIMIT * 2 - ws->stex_used < SWPN_MAX_DATA)
		return 2;

	swpn_clear_data();

	stex_size = 0;
	stex_fullbright = 0;
	memset(stex_data, 0, SWPN_MAX_DATA);

	for(uint32_t i = 0; i < UI_WPN_PARTS; i++)
	{
		wpnspr_part_t *part = wpn_part_info + i;
		int32_t tmp;

		if(!part->width)
			continue;

		if(part->flags & WPN_PART_FLAG_BRIGHT)
			continue;

		if(swpn_add_pnew(part, width, height))
			return 1;
	}

	stex_size += 255;
	stex_size &= ~255;
	stex_fullbright = stex_size;

	for(uint32_t i = 0; i < UI_WPN_PARTS; i++)
	{
		wpnspr_part_t *part = wpn_part_info + i;
		int32_t tmp;

		if(!part->width)
			continue;

		if(!(part->flags & WPN_PART_FLAG_BRIGHT))
			continue;

		if(swpn_add_pnew(part, width, height))
			return 1;
	}

	stex_size += 255;
	stex_size &= ~255;

	if(!stex_size)
		return -1;

	width = (160 - width) / 2;
	height = 120 - height;

	for(uint32_t i = 0; i < UI_WPN_PARTS; i++)
	{
		wpnspr_part_t *part = wpn_part_info + i;

		if(!part->width)
			continue;

		part->x += width;
		part->y += height;
	}

	memcpy(ws->data + ws->stex_used, stex_data, stex_size);

	va = ws->variant + va->wpn.base;
	count = va->wpn.count;

	for(uint32_t i = 0; i < count; i++, va++)
	{
		memcpy(va->ws.part, wpn_part_info, sizeof(wpn_part_info));
		va->ws.dstart = ws->stex_used;
		va->ws.dsize = stex_size;
		va->ws.dbright = stex_fullbright;
	}

	ws->stex_used += stex_size;

	stex_wpn_import = 0;
	update_gfx_mode(0);

	return 0;
}

static int32_t swpn_count_valid(variant_list_t *ws, uint32_t *bitmap, int32_t *invalid)
{
	uint32_t count = 0;
	uint32_t bits = 0;

	for(uint32_t i = 0; i < ws->max; i++)
	{
		variant_info_t *va = ws->variant + i;
		uint32_t valid = 0;

		for(uint32_t j = 0; j < UI_WPN_PARTS; j++)
		{
			wpnspr_part_t *part = va->ws.part + j;

			if(!part->width)
				continue;

			if(part->flags & WPN_PART_FLAG_DISABLED)
				continue;

			valid++;
		}

		if(valid > MAX_X16_WPNPARTS)
		{
			if(invalid)
				*invalid = i;
		} else
		if(valid)
		{
			bits |= 1 << i;
			count++;
		}
	}

	*bitmap = bits;

	return count;
}

//
// variant lists

static void vlist_new(uint32_t type, uint8_t *name, variant_list_t *vl, ui_idx_t *idx, uint32_t now)
{
	const vlist_type_t *vt = vlist_type + type;
	variant_list_t *ret;
	uint32_t hash;

	if(!name)
		return;

	if(edit_check_name(name, 0))
	{
		edit_status_printf("Invalid name!");
		return;
	}

	hash = x16c_crc(name, -1, vt->exor);

	for(uint32_t i = 0; i < idx->max; i++)
	{
		if(vl[i].hash == hash)
		{
			edit_status_printf("%s with this name already exists!", vt->name);
			return;
		}
	}

	ret = vl + idx->max;

	idx->now = idx->max;
	idx->max++;

	strcpy(ret->name, name);
	ret->hash = hash;
	ret->width = 0;
	ret->height = 0;
	ret->max = 0;
	ret->now = now;

	memset(ret->variant, 0, sizeof(variant_info_t) * MAX_X16_VARIANTS);

	update_gfx_mode(0);
}

static void vlist_rename(uint32_t type, uint8_t *name, variant_list_t *vl, ui_idx_t *idx)
{
	const vlist_type_t *vt = vlist_type + type;
	variant_list_t *ret;
	uint32_t hash;

	if(!name)
		return;

	if(edit_check_name(name, 0))
	{
		edit_status_printf("Invalid name!");
		return;
	}

	hash = x16c_crc(name, -1, vt->exor);

	for(uint32_t i = 0; i < idx->max; i++)
	{
		if(vl[i].hash == hash)
		{
			edit_status_printf("%s with this name already exists!", vt->name);
			return;
		}
	}

	ret = vl + idx->now;
	strcpy(ret->name, name);
	ret->hash = hash;

	update_gfx_mode(0);
}

static void vlist_var_new(uint8_t *name, variant_list_t *vl, ui_idx_t *idx)
{
	variant_info_t *va;
	uint32_t hash;
	uint32_t is_sprite = idx == gfx_idx + GFX_MODE_SPRITES;
	uint32_t is_weapon = idx == gfx_idx + GFX_MODE_WEAPONS;

	if(!name)
		return;

	if(is_sprite)
	{
		if(name[0] >= 'a' && name[0] <= 'z')
			name[0] &= ~32;
		hash = sprite_name_hash(name);
		if(hash == 0xFFFFFFFF)
		{
			edit_status_printf("Invalid name!");
			return;
		}
	} else
	if(is_weapon)
	{
		hash = name[0] - 'a';
	} else
	{
		if(edit_check_name(name, 0))
		{
			edit_status_printf("Invalid name!");
			return;
		}

		hash = x16c_crc(name, -1, VAR_CRC_XOR);
	}

	vl += idx->now;

	for(uint32_t i = 0; i < vl->max; i++)
	{
		if(vl->variant[i].hash == hash)
		{
			edit_status_printf("Variant with this name already exists!");
			return;
		}
	}

	vl->now = vl->max;
	vl->max++;

	va = vl->variant + vl->now;
	memset(va, 0, sizeof(variant_info_t));
	strcpy(va->name, name);
	va->hash = hash;

	if(!is_weapon)
		update_gfx_mode(0);
}

static void vlist_var_ren(uint8_t *name, variant_list_t *vl, ui_idx_t *idx)
{
	variant_info_t *va;
	uint32_t hash;
	uint32_t is_sprite = idx == gfx_idx + GFX_MODE_SPRITES;

	if(!name)
		return;

	if(is_sprite)
	{
		if(name[0] >= 'a' && name[0] <= 'z')
			name[0] &= ~32;

		hash = sprite_name_hash(name);
		if(hash == 0xFFFFFFFF)
		{
			edit_status_printf("Invalid name!");
			return;
		}
	} else
	{
		if(edit_check_name(name, 0))
		{
			edit_status_printf("Invalid name!");
			return;
		}

		hash = x16c_crc(name, -1, VAR_CRC_XOR);
	}

	vl += idx->now;

	for(uint32_t i = 0; i < vl->max; i++)
	{
		if(!strcmp(vl->variant[i].name, name))
		{
			edit_status_printf("Variant with this name already exists!");
			return;
		}
	}

	va = vl->variant + vl->now;
	strcpy(va->name, name);
	va->hash = hash;

	update_gfx_mode(0);
}

static void vlist_delete(variant_list_t *vl, ui_idx_t *idx)
{
	uint32_t n = idx->now;
	uint32_t m = idx->max - 1;

	idx->max = m;

	memcpy(vl + n, vl + n + 1, sizeof(variant_list_t) * (m - n));

	if(m && n >= m)
		idx->now = n - 1;

	update_gfx_mode(0);
}

static void vlist_var_del(variant_list_t *vl, ui_idx_t *idx)
{
	uint32_t is_stex = vl == x16_wall || vl == x16_sprite;

	vl += idx->now;
	vl->max--;

	memcpy(vl->variant + vl->now, vl->variant + vl->now + 1, sizeof(variant_info_t) * (vl->max - vl->now));

	if(vl->max && vl->now >= vl->max)
		vl->now--;

	if(is_stex)
		stex_remake_columns(vl);

	update_gfx_mode(0);
}

//
// load

static void gfx_cleanup()
{
	stex_wpn_import = 0;
	wpn_part_copy = -1;

	for(uint32_t i = 0; i < GFX_NUM_MODES; i++)
	{
		gfx_idx[i].now = 0;
		gfx_idx[i].max = 0;
	}

	gfx_idx[GFX_MODE_PALETTE].max = MAX_X16_PALETTE;
	gfx_idx[GFX_MODE_REMAPS].max = MAX_X16_REMAPS;
	gfx_idx[GFX_MODE_HUD].max = NUM_HUD_ELM;

	for(uint32_t i = 0; i < 256 * MAX_X16_PALETTE; i++)
	{
		uint8_t r, g, b;
		uint16_t tmp = rand();

		r = tmp & 15;
		g = (tmp >> 4) & 15;
		b = (tmp >> 8) & 15;

		r |= r << 4;
		g |= g << 4;
		b |= b << 4;

		x16_palette_data[i] = 0xFF000000 | r | (g << 8) | (b << 16);
	}

	memset(x16_palette_bright, 0, sizeof(x16_palette_bright));
	memset(x16_color_remap, 0, sizeof(x16_color_remap));
	memset(x16_plane, 0, sizeof(x16_plane));
	memset(x16_wall, 0, sizeof(x16_wall));
	memset(x16_sprite, 0, sizeof(x16_sprite));
	memset(x16_weapon, 0, sizeof(x16_weapon));
	memset(font_char, 0, sizeof(font_char));
	memset(nums_char, 0, sizeof(nums_char));
}

static void gfx_default_light()
{
	gfx_idx[GFX_MODE_LIGHTS].max = 1;

	for(uint32_t i = 0; i < 256; i++)
		x16_light_data[i] = i;

	strcpy(editor_light[0].name, "white 100%");
	editor_light[0].hash = x16c_crc(editor_light[0].name, -1, LIGHT_CRC_XOR);
	editor_light[0].des = 0;
	editor_light[0].r = 100;
	editor_light[0].g = 100;
	editor_light[0].b = 100;
}

static void gfx_load(const uint8_t *file)
{
	uint32_t size;
	int32_t ret;
	kgcbor_ctx_t ctx;

	if(file)
		strcpy(gfx_path, file);

	size = edit_read_file(gfx_path, edit_cbor_buffer, EDIT_EXPORT_BUFFER_SIZE);
	if(size)
	{
		edit_busy_window("Loading GFX ...");

		gfx_cleanup();

		ctx.entry_cb = cbor_gfx_root;
		ctx.max_recursion = 16;
		ret = kgcbor_parse_object(&ctx, edit_cbor_buffer, size);

		if(ret)
			goto do_fail;

		for(uint32_t i = 0; i < gfx_idx[GFX_MODE_WALLS].max; i++)
		{
			variant_list_t *vl = x16_wall + i;

			// check columns and calculate lengths
			for(uint32_t j = 0; j < vl->max; j++)
			{
				variant_info_t *vi = vl->variant + j;

				if(!vi->sw.masked)
				{
					for(uint32_t x = 0; x < vi->sw.width; x++)
					{
						if(vi->sw.offset[x] + vi->sw.height > STEX_PIXEL_LIMIT)
						{
							// bad data; make empty
							vi->sw.width = 0;
							vi->sw.height = 0;
							vi->sw.stride = 0;
							break;
						}
						vi->sw.length[x] = vi->sw.height;
					}
				} else
					stex_parse_columns(vl, vi);
			}

			// remake all columns
			if(stex_remake_columns(vl))
				goto do_fail;
		}

		for(uint32_t i = 0; i < gfx_idx[GFX_MODE_SPRITES].max; i++)
		{
			variant_list_t *vl = x16_sprite + i;
			uint8_t *src;
			uint16_t *dst;

			// expand data
			src = vl->data + STEX_PIXEL_LIMIT;
			dst = (uint16_t*)vl->data + STEX_PIXEL_LIMIT;
			for(uint32_t i = 0; i < STEX_PIXEL_LIMIT; i++)
			{
				dst--;
				src--;
				*dst = *src | 0xFF00;
			}

			// check columns and calculate lengths
			for(uint32_t j = 0; j < vl->max; j++)
				stex_parse_columns(vl, vl->variant + j);

			// remake all columns
			if(stex_remake_columns(vl))
				goto do_fail;
		}
	} else
	{
do_fail:
		gfx_cleanup();
		edit_status_printf("Unable to load GFX file!");
	}

	if(!gfx_idx[GFX_MODE_LIGHTS].max)
		gfx_default_light();

	editor_light[0].des = 0;
	editor_light[0].r = 100;
	editor_light[0].g = 100;
	editor_light[0].b = 100;

	x16_palette_bright[0] |= 1; // 'transparent pixel'

	recalc_lights();

	x16g_update_texture(X16G_GLTEX_PALETTE);
	x16g_update_texture(X16G_GLTEX_LIGHTS);

	update_gfx_mode(0);

	return;
}

//
// update: palette

static const uint8_t *update_gfx_palette(ui_idx_t *idx)
{
	uint8_t text[32];
	uint8_t *data, *ptr;
	uint32_t pidx;
	gltex_info_t *gi;

	if(!idx)
		return NULL;

	gltex_info[X16G_GLTEX_NOW_PALETTE].data = x16_palette_data + idx->now * 256;
	x16g_update_texture(X16G_GLTEX_NOW_PALETTE);

	sprintf(text, "%d%%", palette_options.tint.des);
	glui_set_text(&ui_gfx_palette_tint_des, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Red: %u%%", palette_options.tint.mr);
	glui_set_text(&ui_gfx_palette_tint_mr, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Green: %u%%", palette_options.tint.mg);
	glui_set_text(&ui_gfx_palette_tint_mg, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Blue: %u%%", palette_options.tint.mb);
	glui_set_text(&ui_gfx_palette_tint_mb, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Red: %u%%", palette_options.tint.ar);
	glui_set_text(&ui_gfx_palette_tint_ar, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Green: %u%%", palette_options.tint.ag);
	glui_set_text(&ui_gfx_palette_tint_ag, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Blue: %u%%", palette_options.tint.ab);
	glui_set_text(&ui_gfx_palette_tint_ab, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	sprintf(text, "%u%%", palette_options.dmg.des);
	glui_set_text(&ui_gfx_palette_dmg_des, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "%u%%", palette_options.dmg.add);
	glui_set_text(&ui_gfx_palette_dmg_add, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Red: %u%%", palette_options.dmg.r);
	glui_set_text(&ui_gfx_palette_dmg_r, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Green: %u%%", palette_options.dmg.g);
	glui_set_text(&ui_gfx_palette_dmg_g, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Blue: %u%%", palette_options.dmg.b);
	glui_set_text(&ui_gfx_palette_dmg_b, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	data = malloc(48 * 48);
	if(!data)
		return NULL;

	ptr = data;
	pidx = 0;
	for(uint32_t y = 0; y < 16; y++)
	{
		for(uint32_t i = 0; i < 3; i++)
		{
			for(uint32_t x = 0; x < 16; x++)
			{
				uint8_t ii = pidx + x;
				*ptr++ = ii;
				*ptr++ = ii;
				*ptr++ = ii;
				if(i == 1 && x16_palette_bright[y] & (1 << x))
				{
					ptr[-2] = 0;
					x16_light_data[ii] = ii;
				}
			}
		}
		pidx += 16;
	}

	gi = gltex_info + X16G_GLTEX_BRIGHT_COLORS;
	gi->width = 16 * 3;
	gi->height = 16 * 3;
	gi->data = data;
	x16g_update_texture(X16G_GLTEX_BRIGHT_COLORS);

	free(data);

	return x16g_palette_name[idx->now];
}

//
// update: lights

static const uint8_t *update_gfx_lights(ui_idx_t *idx)
{
	uint8_t text[32];
	editor_light_t *el;

	if(!idx)
		return NULL;

	make_light_data(idx->now, 0);

	el = editor_light + idx->now;

	sprintf(text, "Desat: %d%%", el->des);
	glui_set_text(&ui_gfx_light_des, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Red: %u%%", el->r);
	glui_set_text(&ui_gfx_light_r, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Green: %u%%", el->g);
	glui_set_text(&ui_gfx_light_g, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	sprintf(text, "Blue: %u%%", el->b);
	glui_set_text(&ui_gfx_light_b, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	x16g_update_texture(X16G_GLTEX_SHOW_LIGHT);

	return editor_light[idx->now].name;
}

//
// update: remaps

static const uint8_t *update_gfx_remaps(ui_idx_t *idx)
{
	uint8_t text[32];
	color_remap_t *cr;
	glui_text_t *txt;
	gltex_info_t *gi;

	if(!idx)
		return NULL;

	cr = x16_color_remap + idx->now;
	txt = text_ranges;
	for(uint32_t i = 0; i < cr->count; i++)
	{
		remap_entry_t *ce = cr->entry + i;
		uint32_t color = i == cr->now ? 0xFFFF0000 : 0xFF000000;

		// first
		sprintf(text, "%u", ce->s);
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		// last
		sprintf(text, "%u", ce->e);
		glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
		txt->color[0] = color;
		txt->color[1] = color;
		txt++;

		if(ce->t)
		{
			// start
			sprintf(text, "r: %u g: %u b:%u", ce->rgb.rs, ce->rgb.gs, ce->rgb.bs);
			glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
			txt->color[0] = color;
			txt->color[1] = color;
			txt++;

			// stop
			sprintf(text, "r: %u g: %u b:%u", ce->rgb.re, ce->rgb.ge, ce->rgb.be);
			glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
			txt->color[0] = color;
			txt->color[1] = color;
			txt++;
		} else
		{
			// start
			sprintf(text, "p: %u", ce->pal.s);
			glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
			txt->color[0] = color;
			txt->color[1] = color;
			txt++;

			// stop
			sprintf(text, "p: %u", ce->pal.e);
			glui_set_text(txt, text, glui_font_small_kfn, GLUI_ALIGN_BOT_CENTER);
			txt->color[0] = color;
			txt->color[1] = color;
			txt++;
		}
	}

	ui_gfx_remaps_ranges.elements[0]->container.count = cr->count * NUM_RANGES_COLS;

	generate_remap(idx->now);

	gi = gltex_info + X16G_GLTEX_SHOW_TEXTURE;
	gi->width = 16;
	gi->height = 16;
	gi->format = GL_LUMINANCE;
	gi->data = remap_temp_data;
	x16g_update_texture(X16G_GLTEX_SHOW_TEXTURE);

	remap_name[sizeof(remap_name)-2] = 'A' + idx->now;

	return remap_name;
}

//
// update: planes

static const uint8_t *update_gfx_planes(ui_idx_t *idx)
{
	variant_list_t *pl;
	gltex_info_t *gi;
	uint8_t *data, *ptr;
	uint8_t *effect = NULL;
	uint8_t text[32];

	glui_set_text(&ui_gfx_plane_display, plane_display ? "Show: only fullbright" : "Show: everything", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	for(uint32_t i = 0; i < ui_gfx_plane_effect.count; i++)
		ui_gfx_plane_effect.elements[i]->base.disabled = 1;

	if(!idx)
	{
		ui_gfx_plane_texture.base.disabled = 1;
		ui_gfx_plane_cmaps.base.disabled = 1;
		ui_gfx_plane_variant_name.base.disabled = 1;
		ui_gfx_plane_variant_pl.base.disabled = 1;
		ui_gfx_plane_variant_pr.base.disabled = 1;
		ui_gfx_plane_variant_box.base.disabled = 1;
		return NULL;
	}

	data = calloc(256, 256);
	if(!data)
		return NULL;

	if(plane_display)
		memset(x16_light_data, 0, 256);

	pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;

	gltex_info[X16G_GLTEX_SHOW_TEXTURE].data = pl->data;

	if(pl->now >= MAX_X16_VARIANTS_PLN)
	{
		// 8bpp
		uint8_t idx = 0;
		uint8_t *src;

		effect = pl->variant[0].pl.effect;

		ui_gfx_plane_import_raw.base.disabled = 0;
		ui_gfx_plane_variant.base.disabled = 1;
		ui_gfx_plane_renvar.base.disabled = 1;
		ui_gfx_plane_delvar.base.disabled = 1;

		ui_gfx_plane_variant_pl.base.disabled = 1;
		ui_gfx_plane_variant_pr.base.disabled = 1;
		ui_gfx_plane_variant_name.base.disabled = 1;
		ui_gfx_plane_variant_box.base.disabled = 1;
		ui_gfx_plane_cmaps.base.disabled = 1;
//		ui_gfx_plane_texture.base.x = 512;
		ui_gfx_plane_texture.shader = SHADER_FRAGMENT_PALETTE;

		if(plane_display)
		{
			src = pl->data;
			ptr = data;
			for(uint32_t i = 0; i < pl->width * pl->height; i++)
			{
				uint8_t in = *src++;
				if(x16_palette_bright[in >> 4] & (1 << (in & 15)))
					*ptr++ = in;
				else
					*ptr++ = 0;
			}

			gltex_info[X16G_GLTEX_SHOW_TEXTURE].data = data;
		}

		ui_gfx_plane_cmaps.base.height = base_height_cmaps * 16;
		ui_gfx_plane_cmaps.coord.t[0] = 0.0f;
		ui_gfx_plane_cmaps.coord.t[1] = 1.0f;
	} else
	{
		// 4bpp
		ui_gfx_plane_import_raw.base.disabled = 1;
		ui_gfx_plane_variant.base.disabled = 0;
		ui_gfx_plane_renvar.base.disabled = 0;
		ui_gfx_plane_delvar.base.disabled = 0;

		ui_gfx_plane_variant_box.base.disabled = 0;
//		ui_gfx_plane_texture.base.x = 256;
		ui_gfx_plane_texture.shader = plane_display ? SHADER_FRAGMENT_COLORMAP_LIGHT : SHADER_FRAGMENT_COLORMAP;

		if(pl->max)
		{
			ui_gfx_plane_variant_pl.base.disabled = 0;
			ui_gfx_plane_variant_pr.base.disabled = 0;
			ui_gfx_plane_variant_pl.base.y = 12 + base_height_cmaps * pl->now;
			ui_gfx_plane_variant_pr.base.y = 12 + base_height_cmaps * pl->now;
			ui_gfx_plane_cmaps.base.disabled = 0;
			ui_gfx_plane_variant_name.base.disabled = 0;
			generate_colormap(0, pl->variant[pl->now].pl.data, pl->variant[pl->now].pl.bright);
			x16g_update_texture(X16G_GLTEX_COLORMAPS);
			effect = pl->variant[pl->now].pl.effect;
		} else
		{
			ui_gfx_plane_variant_pl.base.disabled = 1;
			ui_gfx_plane_variant_pr.base.disabled = 1;
			ui_gfx_plane_cmaps.base.disabled = 1;
			ui_gfx_plane_variant_name.base.disabled = 1;
		}

		glui_set_text(&ui_gfx_plane_variant_name, pl->variant[pl->now].name, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);

		ptr = data;

		for(uint32_t i = 0; i < pl->max; i++)
		{
			uint8_t *tmp = ptr;

			for(uint32_t j = 0; j < 16 * 9; j++)
				*ptr++ = pl->variant[i].pl.data[(j / 3) & 15];

			if(pl->variant[i].pl.bright)
			{
				tmp += 16 * 3 + 1;
				for(uint32_t j = 0; j < 16; j++)
				{
					if(pl->variant[i].pl.bright & (1 << j))
					{
						uint8_t ii = tmp[j * 3];
						tmp[j * 3] = 0;
					}
				}
				tmp += 16 * 3 * 2;
			}
		}

		gi = gltex_info + X16G_GLTEX_BRIGHT_COLORS;
		gi->width = 16 * 3;
		gi->height = MAX_X16_VARIANTS_PLN * 3;
		gi->data = data;
		x16g_update_texture(X16G_GLTEX_BRIGHT_COLORS);

		ui_gfx_plane_cmaps.base.height = base_height_cmaps * pl->max;
		ui_gfx_plane_cmaps.coord.t[0] = 0.0f;
		ui_gfx_plane_cmaps.coord.t[1] = (float)pl->max / (float)MAX_X16_VARIANTS_PLN;
	}

	if(plane_display)
		x16g_update_texture(X16G_GLTEX_LIGHTS);

	if(pl->width && pl->height)
	{
		uint32_t scale;

		gi = gltex_info + X16G_GLTEX_SHOW_TEXTURE;
		gi->width = pl->width;
		gi->height = pl->height;
		gi->format = GL_LUMINANCE;
		x16g_update_texture(X16G_GLTEX_SHOW_TEXTURE);

		scale = pl->height > 128 ? 2 : 3;

		ui_gfx_plane_texture.base.disabled = 0;
		ui_gfx_plane_texture.base.width = (uint32_t)pl->width * scale;
		ui_gfx_plane_texture.base.height = (uint32_t)pl->height * scale;
	} else
		ui_gfx_plane_texture.base.disabled = 1;

	free(data);

	if(effect)
	{
		int16_t temp;
		float femp;

		ui_gfx_plane_effect.elements[0]->base.disabled = 0;

		if(effect[0])
			for(uint32_t i = 1; i < ui_gfx_plane_effect.count; i++)
				ui_gfx_plane_effect.elements[i]->base.disabled = 0;

		sprintf(text, "Effect: %s", plane_effect_name[effect[0] & X16G_MASK_PL_EFFECT]);
		glui_set_text((void*)ui_gfx_plane_effect.elements[0], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

		if(effect[1] & 0x80)
			femp = 1 << (effect[1] & 0x7F);
		else
			femp = 1.0f / (float)(1 << effect[1]);

		sprintf(text, "Speed: %.3f", femp);
		glui_set_text((void*)ui_gfx_plane_effect.elements[1], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

		switch(effect[0] & X16G_MASK_PL_EFFECT)
		{
			case X16G_PL_EFFECT_RANDOM:
				ptr = text;
				ptr += sprintf(text, "Mode: ");
				if(!(effect[2] & 0x80))
					*ptr++ = 'X';
				if(!(effect[2] & 0x40))
					*ptr++ = 'Y';
				if(!(effect[2] & 0x01))
					*ptr++ = 'A';
				*ptr = 0;
				glui_set_text((void*)ui_gfx_plane_effect.elements[2], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
				ui_gfx_plane_effect.elements[3]->base.disabled = 1;
			break;
			case X16G_PL_EFFECT_CIRCLE:
			case X16G_PL_EFFECT_EIGHT:
				temp = effect[2];
				if(temp == 0xFF)
					temp++;
				if(effect[0] & 0x80)
					temp = -temp;
				sprintf(text, "Scale X: %d", temp);
				glui_set_text((void*)ui_gfx_plane_effect.elements[2], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
				temp = effect[3];
				if(temp == 0xFF)
					temp++;
				if(effect[0] & 0x40)
					temp = -temp;
				sprintf(text, "Scale Y: %d", temp);
				glui_set_text((void*)ui_gfx_plane_effect.elements[3], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
			break;
			case X16G_PL_EFFECT_ANIMATE:
				if(!effect[2])
					effect[2]++;
				temp = effect[2] + 1;
				sprintf(text, "Frames: %d", temp);
				glui_set_text((void*)ui_gfx_plane_effect.elements[2], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
				sprintf(text, "Start: %u", effect[3]);
				glui_set_text((void*)ui_gfx_plane_effect.elements[3], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
			break;
		}
	}

	return pl->name;
}

//
// update: walls

static const uint8_t *update_gfx_walls(ui_idx_t *idx)
{
	variant_list_t *wa;
	variant_info_t *va;
	uint8_t text[64];

	ui_gfx_wall_variant_name.base.disabled = 1;
	ui_gfx_wall_texture.base.disabled = 1;
	ui_gfx_wall_variant_info.base.disabled = 1;
	ui_gfx_wall_display.base.disabled = 1;
	ui_gfx_wall_resolution.base.disabled = 1;
	ui_gfx_wall_info.base.disabled = 1;
	ui_gfx_wall_animation.base.disabled = 1;

	if(!idx)
		return NULL;

	wa = x16_wall + idx->now;

	if(!wa->stex_used || !wa->max)
	{
		ui_gfx_wall_info.base.disabled = 0;
		ui_gfx_wall_info.color[0] = 0xFF0000CC;
		ui_gfx_wall_info.color[1] = 0xFF0000CC;
		glui_set_text(&ui_gfx_wall_info, "This texture is empty!", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	} else
	if(!wa->stex_fullbright)
	{
		ui_gfx_wall_display.base.disabled = 0;
		glui_set_text(&ui_gfx_wall_display, wall_display ? "Show: only fullbright" : "Show: everything", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	} else
	{
		ui_gfx_wall_info.base.disabled = 0;
		ui_gfx_wall_info.color[0] = 0xFF00CCCC;
		ui_gfx_wall_info.color[1] = 0xFF00CCCC;
		glui_set_text(&ui_gfx_wall_info, "This texture is fullbright.", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	}

	if(wa->max)
	{
		uint8_t *data;

		data = malloc(256 * 256 * 2);
		if(data)
		{
			gltex_info_t *gi;
			uint32_t scale;
			uint8_t *ptr;
			uint8_t pi = 0;

			if(wall_display)
			{
				memset(x16_light_data, 0, 256);
				x16g_update_texture(X16G_GLTEX_LIGHTS);
			}

			va = wa->variant + wa->now;

			ui_gfx_wall_resolution.base.disabled = 0;
			sprintf(text, "%u x %u\nvariant: %u / %u", va->sw.width, va->sw.height, wa->now + 1, wa->max);
			glui_set_text(&ui_gfx_wall_resolution, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

			stex_generate_wall(wa, va, (uint16_t*)data);

			gi = gltex_info + X16G_GLTEX_SHOW_TEXTURE;
			gi->width = va->sw.width;
			gi->height = va->sw.height;
			gi->format = GL_LUMINANCE_ALPHA;
			gi->data = data;
			x16g_update_texture(X16G_GLTEX_SHOW_TEXTURE);

			scale = va->sw.height > 128 ? 1 : 3;

			ui_gfx_wall_texture.base.disabled = 0;
			ui_gfx_wall_texture.base.width = (uint32_t)va->sw.width * scale;
			ui_gfx_wall_texture.base.height = (uint32_t)va->sw.height * scale;
			ui_gfx_wall_texture.shader = wall_display ? SHADER_FRAGMENT_PALETTE_LIGHT : SHADER_FRAGMENT_PALETTE;

			ui_gfx_wall_variant_info.base.disabled = 0;
			sprintf(data, "Bytes used: %u\nBytes total: %u\nBytes saved: %d", wa->stex_used, wa->stex_total, wa->stex_total - wa->stex_used);
			glui_set_text(&ui_gfx_wall_variant_info, data, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);

			ui_gfx_wall_variant_name.base.disabled = 0;
			glui_set_text(&ui_gfx_wall_variant_name, va->name, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);

			free(data);

			ui_gfx_wall_animation.base.disabled = 0;

			if(va->sw.anim[0])
			{
				sprintf(text, "Speed: %.3f", 1.0f / (float)(1 << va->sw.anim[1]));
				glui_set_text((void*)ui_gfx_wall_animation.elements[1], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
				ui_gfx_wall_animation.elements[1]->base.disabled = 0;

				sprintf(text, "Start: %u", va->sw.anim[2]);
				glui_set_text((void*)ui_gfx_wall_animation.elements[2], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
				ui_gfx_wall_animation.elements[2]->base.disabled = 0;

				sprintf(text, "Animation: %u", va->sw.anim[0] + 1);
			} else
			{
				ui_gfx_wall_animation.elements[1]->base.disabled = 1;
				ui_gfx_wall_animation.elements[2]->base.disabled = 1;
				sprintf(text, "Animation: \t");
			}

			glui_set_text((void*)ui_gfx_wall_animation.elements[0], text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
		}
	}

	return wa->name;
}

//
// update: sprites

static const uint8_t *update_gfx_sprites(ui_idx_t *idx)
{
	variant_list_t *sp;
	variant_info_t *va;
	uint8_t text[64];

	ui_gfx_sprite_variant_name.base.disabled = 1;
	ui_gfx_sprite_texture.base.disabled = 1;
	ui_gfx_sprite_variant_info.base.disabled = 1;
	ui_gfx_sprite_display.base.disabled = 1;
	ui_gfx_sprite_resolution.base.disabled = 1;
	ui_gfx_sprite_info.base.disabled = 1;
	ui_gfx_sprite_offs_x.base.disabled = 1;
	ui_gfx_sprite_offs_y.base.disabled = 1;
	ui_gfx_sprite_origin.base.disabled = 1;
	ui_gfx_sprite_show_origin.base.disabled = 1;

	if(!idx)
		return NULL;

	sp = x16_sprite + idx->now;

	ui_gfx_sprite_show_origin.base.disabled = 0;
	glui_set_text(&ui_gfx_sprite_show_origin, sprite_origin_txt[sprite_origin], glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	if(!sp->stex_used || !sp->max)
	{
		ui_gfx_sprite_info.base.disabled = 0;
		ui_gfx_sprite_info.color[0] = 0xFF0000CC;
		ui_gfx_sprite_info.color[1] = 0xFF0000CC;
		glui_set_text(&ui_gfx_sprite_info, "This sprite is empty!", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	} else
	{
		ui_gfx_sprite_display.base.disabled = 0;
		glui_set_text(&ui_gfx_sprite_display, sprite_display ? "Show: only fullbright" : "Show: everything", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
	}

	if(sp->max)
	{
		uint8_t *data;

		data = calloc(256 * 256, 2);
		if(data)
		{
			gltex_info_t *gi;
			uint32_t scale;
			uint8_t *ptr;
			uint8_t pi = 0;

			if(sprite_display)
			{
				memset(x16_light_data, 0, 256);
				x16g_update_texture(X16G_GLTEX_LIGHTS);
			}

			va = sp->variant + sp->now;

			sprintf(text, "<        %d        >", va->sw.ox);
			ui_gfx_sprite_offs_x.base.disabled = 0;
			glui_set_text(&ui_gfx_sprite_offs_x, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

			sprintf(text, "<        %d        >", va->sw.oy);
			ui_gfx_sprite_offs_y.base.disabled = 0;
			glui_set_text(&ui_gfx_sprite_offs_y, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

			ui_gfx_sprite_resolution.base.disabled = 0;
			sprintf(text, "%u x %u", va->sw.width, va->sw.height);
			glui_set_text(&ui_gfx_sprite_resolution, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

			stex_generate_sprite(sp, va, (uint16_t*)data);

			gi = gltex_info + X16G_GLTEX_SHOW_TEXTURE;
			gi->width = va->sw.stride;
			gi->height = va->sw.height;
			gi->format = GL_LUMINANCE_ALPHA;
			gi->data = data;
			x16g_update_texture(X16G_GLTEX_SHOW_TEXTURE);

			if(va->sw.height > 193)
				scale = 1;
			else
			if(va->sw.height > 129)
				scale = 2;
			else
				scale = 3;

			ui_gfx_sprite_texture.base.disabled = 0;
			ui_gfx_sprite_texture.base.width = (uint32_t)va->sw.width * scale;
			ui_gfx_sprite_texture.base.height = (uint32_t)va->sw.height * scale;
			ui_gfx_sprite_texture.shader = sprite_display ? SHADER_FRAGMENT_PALETTE_LIGHT : SHADER_FRAGMENT_PALETTE;
			ui_gfx_sprite_texture.coord.s[1] = (float)va->sw.width / (float)va->sw.stride;

			if(sprite_origin < 2)
			{
				ui_gfx_sprite_texture.base.x = 512;
				ui_gfx_sprite_texture.base.y = 448;
				ui_gfx_sprite_texture.base.align = GLUI_ALIGN_CENTER_CENTER;

				ui_gfx_sprite_origin.base.disabled = !sprite_origin;
				ui_gfx_sprite_origin.base.x = ui_gfx_sprite_texture.base.x + va->sw.ox * scale;
				ui_gfx_sprite_origin.base.y = ui_gfx_sprite_texture.base.y + (ui_gfx_sprite_texture.base.height - 1) / 2 + va->sw.oy * scale;

				if(scale == 3)
				{
					if(!(va->sw.width & 1))
						ui_gfx_sprite_origin.base.x++;
					ui_gfx_sprite_origin.base.y--;
				}
			} else
			{
				ui_gfx_sprite_texture.base.x = 512 - va->sw.ox * scale;
				ui_gfx_sprite_texture.base.y = 608 - va->sw.oy * scale;
				ui_gfx_sprite_texture.base.align = GLUI_ALIGN_CENTER_BOT;

				ui_gfx_sprite_origin.base.disabled = 0;
				ui_gfx_sprite_origin.base.x = 512;
				ui_gfx_sprite_origin.base.y = 608;

				if(scale == 3)
				{
					if(!(va->sw.width & 1))
						ui_gfx_sprite_texture.base.x--;
					ui_gfx_sprite_texture.base.y += 2;
				} else
					ui_gfx_sprite_texture.base.y++;
			}

			ui_gfx_sprite_variant_info.base.disabled = 0;
			sprintf(data, "Bytes used: %u\nBytes total: %u\nBytes saved: %d", sp->stex_used, sp->stex_total, sp->stex_total - sp->stex_used);
			glui_set_text(&ui_gfx_sprite_variant_info, data, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);

			ui_gfx_sprite_variant_name.base.disabled = 0;
			glui_set_text(&ui_gfx_sprite_variant_name, va->name, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);

			free(data);
		}
	}

	return sp->name;
}

//
// update: weapons

static void generate_wpn_part_rect(wpnspr_part_t *pr, uint32_t now)
{
	glui_dummy_t *rect = wpn_part_rect;
	wpnspr_part_t *part_now = NULL;

	ui_gfx_wpn_part_rect_box.base.disabled = 0;

	for(uint32_t i = 0; i < UI_WPN_PARTS; i++)
		wpn_part_rect[i].base.disabled = 1;

	for(uint32_t i = 0; i < UI_WPN_PARTS; i++, pr++)
	{
		if(!pr->width)
			continue;

		if(pr->flags & WPN_PART_FLAG_DISABLED)
			continue;

		if(i != now)
		{
			rect->base.disabled = 0;
			rect->base.x = ui_gfx_wpn_texture.base.x + pr->x * UI_WPN_IMG_SCALE - 1;
			rect->base.y = ui_gfx_wpn_texture.base.y + pr->y * UI_WPN_IMG_SCALE - 1;
			rect->base.width = pr->width * UI_WPN_IMG_SCALE + 2;
			rect->base.height = pr->height * UI_WPN_IMG_SCALE + 2;
			rect->base.border_color[0] = 0xFF000000;
			rect->base.border_color[1] = 0xFF000000;
			rect++;
		} else
			part_now = pr;
	}

	if(part_now)
	{
		rect->base.disabled = 0;
		rect->base.x = ui_gfx_wpn_texture.base.x + part_now->x * UI_WPN_IMG_SCALE - 1;
		rect->base.y = ui_gfx_wpn_texture.base.y + part_now->y * UI_WPN_IMG_SCALE - 1;
		rect->base.width = part_now->width * UI_WPN_IMG_SCALE + 2;
		rect->base.height = part_now->height * UI_WPN_IMG_SCALE + 2;
		rect->base.border_color[0] = 0xFFFFFFFF;
		rect->base.border_color[1] = 0xFFFFFFFF;
	}
}

static uint16_t *generate_wpn_preview(variant_info_t *va, uint8_t *source, int32_t *box)
{
	uint16_t *data = (uint16_t*)edit_cbor_buffer;
	int32_t x0 = 65535;
	int32_t x1 = -65535;
	int32_t y0 = 65535;
	int32_t y1 = -65535;
	uint32_t valid = 0;

	memset(data, 0, 256 * 256 * 2);

	for(uint32_t i = 0; i < UI_WPN_PARTS; i++)
	{
		wpnspr_part_t *part = va->ws.part + i;
		uint16_t bright = 0;
		uint32_t base = va->ws.dstart + part->offset;
		int32_t xx, yy;

		if(!part->width)
			continue;

		if(part->flags & WPN_PART_FLAG_DISABLED)
			continue;

		if(part->flags & WPN_PART_FLAG_BRIGHT)
			bright = 0xFF00;

		xx = 48 + part->x;
		yy = 68 + part->y;

		if(xx < x0)
			x0 = xx;
		if(xx + part->width > x1)
			x1 = xx + part->width;

		if(yy < y0)
			y0 = yy;
		if(yy + part->hack_height > y1)
			y1 = yy + part->hack_height;

		valid++;

		if(part->flags & WPN_PART_FLAG_MIRROR_X)
		{
			for(uint32_t y = 0; y < part->height; y++)
			{
				uint16_t *dst = data + xx + (yy + y) * 256 + part->width;
				uint8_t *src = source + base + y * part->width;

				for(uint32_t x = 0; x < part->width; x++)
				{
					uint8_t in = *src++;
					dst--;
					if(in)
						*dst = bright | in;
				}
				dst += part->width * 2;
			}
		} else
		{
			for(uint32_t y = 0; y < part->height; y++)
			{
				uint16_t *dst = data + xx + (yy + y) * 256;
				uint8_t *src = source + base + y * part->width;

				for(uint32_t x = 0; x < part->width; x++)
				{
					uint8_t in = *src++;
					if(in)
						*dst = bright | in;
					dst++;
				}
			}
		}
	}

	if(!valid)
		return NULL;

	if(box)
	{
		box[0] = x0;
		box[1] = y0;
		box[2] = x1;
		box[3] = y1;
	}

	return edit_cbor_buffer;
}

static const uint8_t *update_gfx_weapons(ui_idx_t *idx)
{
	variant_list_t *ws;
	uint8_t text[1024];
	uint8_t *txt;

	ui_gfx_wpn_part_rect_box.base.disabled = 1;
	ui_gfx_wpn_part_box.base.disabled = 1;
	ui_gfx_wpn_texture.base.disabled = 1;
	ui_gfx_wpn_variant_name.base.disabled = 1;
	ui_gfx_wpn_info.base.disabled = 1;
	ui_gfx_wpn_part_apply.base.disabled = 1;
	ui_gfx_wpn_part_btn0.base.disabled = 1;
	ui_gfx_wpn_part_btn1.base.disabled = 1;
	ui_gfx_wpn_part_btn2.base.disabled = 1;

	ui_gfx_wpn_info.color[0] = 0xFF0000CC;
	ui_gfx_wpn_info.color[1] = 0xFF0000CC;

	if(!idx)
		return NULL;

	ws = x16_weapon + idx->now;

	if(ws->max)
	{
		variant_info_t *va = ws->variant + ws->now;
		uint32_t empty = 1;
		wpnspr_part_t *pr = stex_wpn_import ? wpn_part_info : va->ws.part;

		ui_gfx_wpn_part_box.base.disabled = 0;

		ui_gfx_wpn_part_pl.base.y = UI_WPN_PART_HEIGHT / 2 + va->ws.now * UI_WPN_PART_HEIGHT;
		ui_gfx_wpn_part_pr.base.y = UI_WPN_PART_HEIGHT / 2 + va->ws.now * UI_WPN_PART_HEIGHT;

		ui_gfx_wpn_variant_name.base.disabled = 0;
		glui_set_text(&ui_gfx_wpn_variant_name, va->name, glui_font_medium_kfn, GLUI_ALIGN_TOP_CENTER);

		txt = text;
		for(uint32_t i = 0; i < UI_WPN_PARTS; i++)
		{
			wpnspr_part_t *part = pr + i;

			if(part->width)
			{
				if(part->flags & WPN_PART_FLAG_DISABLED)
				{
					*txt++ = 'D';
					*txt++ = ' ';
				} else
					empty = 0;
				txt += sprintf(txt, "%ux%u @\n", part->width, part->height);
			} else
				txt += sprintf(txt, "[Unu\n");
		}
		glui_set_text(&ui_gfx_wpn_part_text_l, text, glui_font_medium_kfn, GLUI_ALIGN_TOP_RIGHT);

		txt = text;
		for(uint32_t i = 0; i < UI_WPN_PARTS; i++)
		{
			wpnspr_part_t *part = pr + i;

			if(part->width)
			{
				txt += sprintf(txt, " %dx%d ", part->x, part->y);
				if(part->flags & WPN_PART_FLAG_BRIGHT)
					*txt++ = 'B';
				if(part->flags & WPN_PART_FLAG_MIRROR_X)
					*txt++ = 'M';
				*txt++ = '\n';
			} else
				txt += sprintf(txt, "sed]\n");
			part++;
		}
		*txt = 0;
		glui_set_text(&ui_gfx_wpn_part_text_r, text, glui_font_medium_kfn, GLUI_ALIGN_TOP_LEFT);

		if(stex_wpn_import)
		{
			wpnspr_part_t *part = wpn_part_info + va->ws.now;

			generate_wpn_part_rect(wpn_part_info, va->ws.now);

			ui_gfx_wpn_texture.base.disabled = 0;

			ui_gfx_wpn_info.base.disabled = 0;
			ui_gfx_wpn_info.color[0] = 0xFF00CCFF;
			ui_gfx_wpn_info.color[1] = 0xFF00CCFF;
			glui_set_text(&ui_gfx_wpn_info, "IMPORT MODE", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

			ui_gfx_wpn_part_apply.base.disabled = 0;
			ui_gfx_wpn_part_btn0.base.disabled = 0;

			if(part->width)
			{
				sprintf(text, "Width: %u", part->width);
				glui_set_text(&ui_gfx_wpn_part_btn0, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

				ui_gfx_wpn_part_btn1.base.disabled = 0;
				sprintf(text, "Height: %u", part->height);
				glui_set_text(&ui_gfx_wpn_part_btn1, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

				ui_gfx_wpn_part_btn2.base.disabled = 0;
				sprintf(text, "Fullbright: %s", txt_yes_no[!(part->flags & WPN_PART_FLAG_BRIGHT)]);
				glui_set_text(&ui_gfx_wpn_part_btn2, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
			} else
				glui_set_text(&ui_gfx_wpn_part_btn0, "Width: \t", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

			return ws->name;
		} else
		{
			if(!empty)
			{
				gltex_info_t *gi;
				uint16_t *data;

				ui_gfx_wpn_info.base.disabled = 0;
				ui_gfx_wpn_info.color[0] = 0xFF44EE44;
				ui_gfx_wpn_info.color[1] = 0xFF44EE44;
				glui_set_text(&ui_gfx_wpn_info, "EDIT MODE", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

				data = generate_wpn_preview(va, ws->data, NULL);
				if(data)
				{
					wpnspr_part_t *part = va->ws.part + va->ws.now;

					ui_gfx_wpn_part_btn0.base.disabled = 0;
					sprintf(text, "Enabled: %s", txt_yes_no[!!(part->flags & WPN_PART_FLAG_DISABLED)]);
					glui_set_text(&ui_gfx_wpn_part_btn0, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

					ui_gfx_wpn_part_btn1.base.disabled = 0;
					sprintf(text, "Mirror: %s", txt_yes_no[!(part->flags & WPN_PART_FLAG_MIRROR_X)]);
					glui_set_text(&ui_gfx_wpn_part_btn1, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

					ui_gfx_wpn_texture.base.disabled = 0;
					ui_gfx_wpn_texture.base.width = 160 * UI_WPN_IMG_SCALE;
					ui_gfx_wpn_texture.base.height = 120 * UI_WPN_IMG_SCALE;
					ui_gfx_wpn_texture.base.x = 364 - ui_gfx_wpn_texture.base.width / 2;
					ui_gfx_wpn_texture.base.y = 433 - ui_gfx_wpn_texture.base.height / 2;
					ui_gfx_wpn_texture.coord.s[0] = 48.0f / 256.0f;
					ui_gfx_wpn_texture.coord.s[1] = 208.0f / 256.0f;
					ui_gfx_wpn_texture.coord.t[0] = 68.0f / 256.0f;
					ui_gfx_wpn_texture.coord.t[1] = 188.0f / 256.0f;

					generate_wpn_part_rect(pr, va->ws.now);

					gi = gltex_info + X16G_GLTEX_SHOW_TEXTURE;
					gi->width = 256;
					gi->height = 256;
					gi->format = GL_LUMINANCE_ALPHA;
					gi->data = data;
					x16g_update_texture(X16G_GLTEX_SHOW_TEXTURE);
				}

				return ws->name;
			} else
				glui_set_text(&ui_gfx_wpn_info, "This variant is empty!", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);
		}
	}

	ui_gfx_wpn_info.base.disabled = 0;
	glui_set_text(&ui_gfx_wpn_info, "This sprite is empty!", glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	return ws->name;
}

//
// update: skies

static const uint8_t *update_gfx_skies(ui_idx_t *idx)
{
	editor_sky_t *sky;
	gltex_info_t *gi;

	if(!idx)
	{
		ui_gfx_sky_texture.base.disabled = 1;
		return NULL;
	}

	ui_gfx_sky_texture.base.disabled = 0;

	sky = editor_sky + idx->now;

	gi = gltex_info + X16G_GLTEX_SHOW_TEXTURE;
	gi->width = 512;
	gi->height = 120;
	gi->format = GL_LUMINANCE;
	gi->data = sky->data;
	x16g_update_texture(X16G_GLTEX_SHOW_TEXTURE);

	return sky->name;
}

//
// update: HUD

static void *hud_texgen_font(const hud_element_t *elm)
{
	uint8_t *data;
	uint8_t *dst;
	uint32_t idx;

	data = malloc(elm->width * elm->height);
	if(!data)
		return NULL;

	dst = data;
	idx = 0;
	for(uint32_t y = 0; y < FONT_CHAR_COUNT / 16; y++)
	{
		for(uint32_t x = 0; x < 16; x++)
		{
			font_char_t *fc = font_char + idx;
			uint8_t *dd = dst;
			uint8_t *ss = fc->data;

			for(uint32_t yy = 0; yy < 8; yy++)
			{
				for(uint32_t xx = 0; xx < 4; xx++)
				{
					uint8_t in = *ss++;
					*dd++ = in & 15;
					*dd++ = in >> 4;
				}
				dd += 8 * 16 - 8;
			}

			dst += 8;
			idx++;
		}
		dst += 7 * 8 * 16;
	}

	return data;
}

static void *hud_texgen_nums(const hud_element_t *elm)
{
	uint8_t *data;
	uint8_t *dst;
	uint32_t idx;

	data = malloc(elm->width * elm->height);
	if(!data)
		return NULL;

	dst = data;
	idx = 0;
	for(uint32_t y = 0; y < NUMS_CHAR_COUNT / 8; y++)
	{
		for(uint32_t x = 0; x < 8; x++)
		{
			nums_char_t *nc = nums_char + idx;
			uint8_t *dd = dst;
			uint8_t *ss = nc->data;

			for(uint32_t yy = 0; yy < 16; yy++)
			{
				for(uint32_t xx = 0; xx < 4; xx++)
				{
					uint8_t in = *ss++;
					*dd++ = in & 15;
					*dd++ = in >> 4;
				}
				dd += 7 * 8;
			}

			dst += 8;
			idx++;
		}
		dst += 15 * 8 * 8;
	}

	return data;
}

static const uint8_t *update_gfx_hud(ui_idx_t *idx)
{
	const hud_element_t *elm = hud_element + idx->now;
	gltex_info_t *gi;
	void *data;
	uint16_t *cmap;
	uint8_t text[32];

	ui_gfx_hud_texture.base.disabled = 1;

	data = elm->texgen(elm);
	if(data)
	{
		// update button
		sprintf(text, "Color: %u", hud_display);
		glui_set_text(&ui_gfx_hud_color, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

		// update texture
		gi = gltex_info + X16G_GLTEX_SHOW_TEXTURE;
		gi->width = elm->width;
		gi->height = elm->height;
		gi->format = GL_LUMINANCE;
		gi->data = data;
		x16g_update_texture(X16G_GLTEX_SHOW_TEXTURE);

		ui_gfx_hud_texture.base.width = gi->width * elm->scale;
		ui_gfx_hud_texture.base.height = gi->height * elm->scale;
		ui_gfx_hud_texture.base.disabled = 0;
		ui_gfx_hud_texture.shader = elm->shader;

		free(data);

		// update colormap
		cmap = (uint16_t*)x16_colormap_data;
		*cmap++ = 0;
		for(uint32_t i = 1; i < 16; i++)
			*cmap++ = i + hud_display * 16;

		x16g_update_texture(X16G_GLTEX_COLORMAPS);
	}

	return hud_element[idx->now].name;
}

//
// input: common

int32_t uin_gfx_mode_click(glui_element_t *elm, int32_t x, int32_t y)
{
	set_gfx_mode(elm->base.custom);
	return 1;
}

int32_t uin_gfx_left(glui_element_t *elm, int32_t x, int32_t y)
{
	stex_wpn_import = 0;
	wpn_part_copy = -1;
	update_gfx_mode(-1);
	return 1;
}

int32_t uin_gfx_right(glui_element_t *elm, int32_t x, int32_t y)
{
	stex_wpn_import = 0;
	wpn_part_copy = -1;
	update_gfx_mode(1);
	return 1;
}

int32_t uin_gfx_input(glui_element_t *elm, uint32_t magic)
{
	switch(magic)
	{
		case EDIT_INPUT_ESCAPE:
			ui_main_menu.base.disabled ^= 1;
		return 1;
		case EDIT_INPUT_LEFT:
			uin_gfx_left(elm, 0, 0);
		return 1;
		case EDIT_INPUT_RIGHT:
			uin_gfx_right(elm, 0, 0);
		return 1;
		default:
			if(!ui_set[gfx_mode].input)
				break;
		return ui_set[gfx_mode].input(elm, magic);
	}

	return 0;
}

//
// input: numeric text

static void te_numeric_desat(uint8_t *text)
{
	int32_t value;

	if(!text)
		return;

	if(sscanf(text, "%d", &value) != 1 || value > 100 || value < -100)
	{
		edit_status_printf("Invalid value!");
		return;
	}

	*value_ptr.s8 = value;

	if(value_cb)
		value_cb();

	ui_set[gfx_mode].update(gfx_idx + gfx_mode);
}

static void te_numeric_normal(uint8_t *text)
{
	uint32_t value;

	if(!text)
		return;

	if(sscanf(text, "%u", &value) != 1 || value > 200)
	{
		edit_status_printf("Invalid value!");
		return;
	}

	*value_ptr.u8 = value;

	if(value_cb)
		value_cb();

	ui_set[gfx_mode].update(gfx_idx + gfx_mode);
}

//
// input: palette

static void fs_palette(uint8_t *file)
{
	image_t *img;
	uint8_t *ptr, *dst;

	img = img_png_load(file, 0);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(img->palcount != 256)
	{
		edit_status_printf("PNG file does not contain correct pallete!");
		return;
	}

	ptr = img->palette;
	dst = (uint8_t*)&x16_palette_data[gfx_idx[GFX_MODE_PALETTE].now * 256];

	for(uint32_t i = 0; i < 256; i++)
	{
		uint8_t r, g, b;

		r = *ptr++;
		g = *ptr++;
		b = *ptr++;

		r = (r & 0xF0) | (r >> 4);
		g = (g & 0xF0) | (g >> 4);
		b = (b & 0xF0) | (b >> 4);

		*dst++ = r;
		*dst++ = g;
		*dst++ = b;
		*dst++ = 0xFF;
	}

	free(img);

	x16g_update_texture(X16G_GLTEX_PALETTE);

	if(!gfx_idx[GFX_MODE_PALETTE].now)
		recalc_lights();
}

static void te_numeric_limited(uint8_t *text)
{
	uint32_t value;

	if(!text)
		return;

	if(sscanf(text, "%u", &value) != 1 || value > 100)
	{
		edit_status_printf("Invalid value!");
		return;
	}

	*value_ptr.u8 = value;

	if(value_cb)
		value_cb();

	ui_set[gfx_mode].update(gfx_idx + gfx_mode);
}

static int32_t input_gfx_palette(glui_element_t *elm, uint32_t magic)
{
	if(magic == EDIT_INPUT_COPY)
	{
		memcpy(palette_clipboard, x16_palette_data + gfx_idx[GFX_MODE_PALETTE].now * 256, 256 * sizeof(uint32_t));
		edit_status_printf("Palette copied.");
		return 1;
	}

	if(magic == EDIT_INPUT_PASTE)
	{
		memcpy(x16_palette_data + gfx_idx[GFX_MODE_PALETTE].now * 256, palette_clipboard, 256 * sizeof(uint32_t));

		if(!gfx_idx[GFX_MODE_PALETTE].now)
			recalc_lights();

		x16g_update_texture(X16G_GLTEX_PALETTE);

		edit_status_printf("Palette pasted.");
		return 1;
	}

	return 1;
}

int32_t uin_gfx_palette_import(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_file_select("Select PNG to load palette from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_palette);
	return 1;
}

int32_t uin_gfx_palette_tint_des(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.s8 = &palette_options.tint.des;
	edit_ui_textentry("Enter value between -100 and 100.", 5, te_numeric_desat);
	return 1;
}

int32_t uin_gfx_palette_tint_mr(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.tint.mr;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

int32_t uin_gfx_palette_tint_mg(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.tint.mg;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

int32_t uin_gfx_palette_tint_mb(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.tint.mb;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

int32_t uin_gfx_palette_tint_ar(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.tint.ar;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

int32_t uin_gfx_palette_tint_ag(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.tint.ag;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

int32_t uin_gfx_palette_tint_ab(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.tint.ab;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

int32_t uin_gfx_palette_tint(glui_element_t *elm, int32_t x, int32_t y)
{
	pal_tint();
	return 1;
}

int32_t uin_gfx_palette_dmg_des(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.dmg.des;
	edit_ui_textentry("Enter value between 0 and 100.", 4, te_numeric_limited);
	return 1;
}

int32_t uin_gfx_palette_dmg_add(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.dmg.add;
	edit_ui_textentry("Enter value between 0 and 100.", 4, te_numeric_limited);
	return 1;
}

int32_t uin_gfx_palette_dmg_r(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.dmg.r;
	edit_ui_textentry("Enter value between 0 and 100.", 4, te_numeric_limited);
	return 1;
}

int32_t uin_gfx_palette_dmg_g(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.dmg.g;
	edit_ui_textentry("Enter value between 0 and 100.", 4, te_numeric_limited);
	return 1;
}

int32_t uin_gfx_palette_dmg_b(glui_element_t *elm, int32_t x, int32_t y)
{
	value_cb = NULL;
	value_ptr.u8 = &palette_options.dmg.b;
	edit_ui_textentry("Enter value between 0 and 100.", 4, te_numeric_limited);
	return 1;
}

int32_t uin_gfx_palette_dmg_single(glui_element_t *elm, int32_t x, int32_t y)
{
	pal_dmg(0);
	return 1;
}

int32_t uin_gfx_palette_dmg_all(glui_element_t *elm, int32_t x, int32_t y)
{
	pal_dmg(1);
	return 1;
}

int32_t uin_gfx_palette_bright(glui_element_t *elm, int32_t x, int32_t y)
{
	x /= 32;
	y /= 32;

	if(!x && !y)
		return 1;

	x16_palette_bright[y] ^= 1 << x;

	recalc_lights();

	update_gfx_mode(0);

	return 1;
}

//
// input: lights

static void te_light_new(uint8_t *name)
{
	uint8_t *ptr;
	uint32_t idx;
	editor_light_t *el;
	uint32_t hash;

	if(!name)
		return;

	if(edit_check_name(name, 0))
	{
		edit_status_printf("Invalid light name!");
		return;
	}

	hash = x16c_crc(name, -1, LIGHT_CRC_XOR);

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_LIGHTS].max; i++)
	{
		if(editor_light[i].hash == hash)
		{
			edit_status_printf("Light with this name already exists!");
			return;
		}
	}

	idx = gfx_idx[GFX_MODE_LIGHTS].max++;
	gfx_idx[GFX_MODE_LIGHTS].now = idx;

	el = editor_light + idx;

	ptr = x16_light_data + idx * 256;
	for(uint32_t i = 0; i < 256; i++)
		ptr[i] = i;

	strcpy(el->name, name);
	el->hash = hash;
	el->des = 0;
	el->r = 100;
	el->g = 100;
	el->b = 100;

	x16g_update_texture(X16G_GLTEX_LIGHTS);
	update_gfx_mode(0);
}

static void te_light_rename(uint8_t *name)
{
	editor_light_t *el;
	uint32_t hash;

	if(!name)
		return;

	if(edit_check_name(name, 0))
	{
		edit_status_printf("Invalid light name!");
		return;
	}

	hash = x16c_crc(name, -1, LIGHT_CRC_XOR);

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_LIGHTS].max; i++)
	{
		if(i == gfx_idx[GFX_MODE_LIGHTS].now)
			continue;

		if(editor_light[i].hash == hash)
		{
			edit_status_printf("Light with this name already exists!");
			return;
		}
	}

	el = editor_light + gfx_idx[GFX_MODE_LIGHTS].now;
	strcpy(el->name, name);
	el->hash = hash;

	update_gfx_mode(0);
}

static void qe_delete_light(uint32_t res)
{
	uint32_t idx;

	if(!res)
		return;

	gfx_idx[GFX_MODE_LIGHTS].max--;

	for(uint32_t i = gfx_idx[GFX_MODE_LIGHTS].now; i < gfx_idx[GFX_MODE_LIGHTS].max; i++)
	{
		uint8_t *ptr = x16_light_data + i * 256;

		for(uint32_t i = 0; i < 256; i++)
			ptr[i] = ptr[i + 256];

		editor_light[i] = editor_light[i + 1];
	}

	if(gfx_idx[GFX_MODE_LIGHTS].now >= gfx_idx[GFX_MODE_LIGHTS].max)
		gfx_idx[GFX_MODE_LIGHTS].now--;

	x16g_update_texture(X16G_GLTEX_LIGHTS);

	update_gfx_mode(0);
}

int32_t uin_gfx_light_new(glui_element_t *elm, int32_t x, int32_t y)
{
	if(gfx_idx[GFX_MODE_LIGHTS].max >= MAX_X16_LIGHTS)
	{
		edit_status_printf("Too many lights already exists!");
		return 1;
	}

	edit_ui_textentry("Enter light name.", LEN_X16_LIGHT_NAME, te_light_new);

	return 1;
}

int32_t uin_gfx_light_rename(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_textentry("Enter light name.", LEN_X16_LIGHT_NAME, te_light_rename);
	return 1;
}

int32_t uin_gfx_light_delete(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_LIGHTS].now)
	{
		edit_status_printf("You can't delete white light!");
		return 1;
	}

	edit_ui_question("Delete light?", "Do you really want to delete this light?\nThis operation is irreversible!", qe_delete_light);

	return 1;
}

int32_t uin_gfx_light_des(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_LIGHTS].now)
	{
		edit_status_printf("You can't change white light!");
		return 1;
	}

	value_cb = light_recalc;
	value_ptr.s8 = &editor_light[gfx_idx[GFX_MODE_LIGHTS].now].des;
	edit_ui_textentry("Enter value between -100 and 100.", 5, te_numeric_desat);

	return 1;
}

int32_t uin_gfx_light_r(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_LIGHTS].now)
	{
		edit_status_printf("You can't change white light!");
		return 1;
	}

	value_cb = light_recalc;
	value_ptr.u8 = &editor_light[gfx_idx[GFX_MODE_LIGHTS].now].r;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

int32_t uin_gfx_light_g(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_LIGHTS].now)
	{
		edit_status_printf("You can't change white light!");
		return 1;
	}

	value_cb = light_recalc;
	value_ptr.u8 = &editor_light[gfx_idx[GFX_MODE_LIGHTS].now].g;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

int32_t uin_gfx_light_b(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_LIGHTS].now)
	{
		edit_status_printf("You can't change white light!");
		return 1;
	}

	value_cb = light_recalc;
	value_ptr.u8 = &editor_light[gfx_idx[GFX_MODE_LIGHTS].now].b;
	edit_ui_textentry("Enter value between 0 and 200.", 4, te_numeric_normal);
	return 1;
}

//
// input: remaps

static uint32_t decode_range(remap_entry_t *ent, uint8_t *text)
{
	uint32_t s, e, t;
	uint32_t rs, gs, bs;
	uint32_t re, ge, be;
	uint32_t nrd = 0;

	if(sscanf(text, "%u:%u=%n", &s, &e, &nrd) != 2 || !nrd || s > 255 || e > 255 || s > e)
		return 1;

	text += nrd;

	if(text[0] == '[')
	{
		// RGB
		if(sscanf(text, "[%u,%u,%u]:[%u,%u,%u]", &rs, &gs, &bs, &re, &ge, &be) != 6)
			return 1;

		if(rs > 255)
			return 1;

		if(re > 255)
			return 1;

		if(gs > 255)
			return 1;

		if(bs > 255)
			return 1;

		if(ge > 255)
			return 1;

		if(be > 255)
			return 1;

		ent->rgb.rs = rs;
		ent->rgb.gs = gs;
		ent->rgb.bs = bs;
		ent->rgb.re = re;
		ent->rgb.ge = ge;
		ent->rgb.be = be;

		t = 1;
	} else
	{
		// palette
		if(sscanf(text, "%u:%u", &rs, &re) != 2)
			return 1;

		if(rs > 255)
			return 1;

		if(re > 255)
			return 1;

		ent->pal.s = rs;
		ent->pal.e = re;

		t = 0;
	}

	ent->s = s;
	ent->e = e;
	ent->t = t;

	return 0;
}

static void te_range_new(uint8_t *text)
{
	color_remap_t *cr;
	remap_entry_t *ent;

	if(!text)
		return;

	cr = x16_color_remap + gfx_idx[GFX_MODE_REMAPS].now;
	ent = cr->entry + cr->count;

	if(text[0])
	{
		if(decode_range(ent, text))
		{
			edit_status_printf("Invalid remap range!");
			return;
		}
	} else
		memset(ent, 0, sizeof(remap_entry_t));

	cr->now = cr->count;
	cr->count++;

	update_gfx_mode(0);
}

static void te_range_src(uint8_t *text)
{
	color_remap_t *cr;
	remap_entry_t *ent;
	uint32_t value;

	if(!text)
		return;

	if(sscanf(text, "%u", &value) != 1 || value > 255)
	{
invalid:
		edit_status_printf("Invalid remap range!");
		return;
	}

	cr = x16_color_remap + gfx_idx[GFX_MODE_REMAPS].now;
	ent = cr->entry + cr->now;

	if(remap_entry_dst)
	{
		if(value < ent->s)
			goto invalid;
		ent->e = value;
	} else
	{
		if(value > ent->e)
			goto invalid;
		ent->s = value;
	}

	update_gfx_mode(0);
}

static void te_range_dst(uint8_t *text)
{
	color_remap_t *cr;
	remap_entry_t *ent;
	uint32_t vr, vg, vb;
	uint32_t set;

	if(!text)
		return;

	cr = x16_color_remap + gfx_idx[GFX_MODE_REMAPS].now;
	ent = cr->entry + cr->now;

	switch(sscanf(text, "%u,%u,%u", &vr, &vg, &vb))
	{
		case 1:
			if(vr > 255)
				goto invalid;

			if(ent->t)
				set = 3;
			else
				set = remap_entry_dst ? 2 : 1;

			ent->t = 0;

			if(set & 2)
				ent->pal.e = vr;
			if(set & 1)
				ent->pal.s = vr;
		break;
		case 3:
			if(vr > 255)
				goto invalid;
			if(vg > 255)
				goto invalid;
			if(vb > 255)
				goto invalid;

			if(ent->t)
				set = remap_entry_dst ? 2 : 1;
			else
				set = 3;

			ent->t = 1;

			if(set & 2)
			{
				ent->rgb.re = vr;
				ent->rgb.ge = vg;
				ent->rgb.be = vb;
			}

			if(set & 1)
			{
				ent->rgb.rs = vr;
				ent->rgb.gs = vg;
				ent->rgb.bs = vb;
			}
		break;
		default:
invalid:
			edit_status_printf("Invalid remap range!");
		return;
	}

	update_gfx_mode(0);
}

static void qe_delete_range(uint32_t res)
{
	color_remap_t *cr;
	remap_entry_t *ent;
	uint32_t count;

	if(!res)
		return;

	cr = x16_color_remap + gfx_idx[GFX_MODE_REMAPS].now;

	count = cr->count - cr->now - 1;
	if(!count)
	{
		if(cr->now)
			cr->now--;
	} else
		memcpy(cr->entry + cr->now, cr->entry + cr->now + 1, count * sizeof(remap_entry_t));

	cr->count--;

	update_gfx_mode(0);
}

int32_t uin_gfx_remap_color(glui_element_t *elm, int32_t x, int32_t y)
{
	uint8_t text[24];

	x /= 24;
	y /= 24;

	sprintf(text, "Color index: %u", x + y * 16);
	glui_set_text(&ui_gfx_remap_color, text, glui_font_medium_kfn, GLUI_ALIGN_CENTER_CENTER);

	return 1;
}

static int32_t uin_remap_range_click(glui_element_t *elm, int32_t x, int32_t y)
{
	color_remap_t *cr = x16_color_remap + gfx_idx[GFX_MODE_REMAPS].now;
	uint32_t row = elm->base.custom & 0xFF;

	if(row == cr->now)
	{
		uint32_t col = elm->base.custom >> 8;
		const uint8_t *text;

		remap_entry_dst = col & 1;

		if(col & 2)
			edit_ui_textentry("Enter remap color.", 14, te_range_dst);
		else
			edit_ui_textentry("Enter palette index.", 4, te_range_src);

		return 1;
	}

	cr->now = row;
	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_remaps_new(glui_element_t *elm, int32_t x, int32_t y)
{
	color_remap_t *cr = x16_color_remap + gfx_idx[GFX_MODE_REMAPS].now;

	if(cr->count >= NUM_RANGES_ROWS)
	{
		edit_status_printf("Too many ranges!");
		return 1;
	}

	edit_ui_textentry("Enter remap range.", 48, te_range_new);

	return 1;
}

int32_t uin_gfx_remaps_del(glui_element_t *elm, int32_t x, int32_t y)
{
	color_remap_t *cr = x16_color_remap + gfx_idx[GFX_MODE_REMAPS].now;

	if(cr->now >= cr->count)
		return 1;

	edit_ui_question("Delete range?", "Do you really want to delete this range?\nThis operation is irreversible!", qe_delete_range);

	return 1;
}

//
// input: planes

static void te_plane_new(uint8_t *text)
{
	vlist_new(VLIST_PLANE, text, x16_plane, gfx_idx + GFX_MODE_PLANES, plane_8bpp);
}

static void te_plane_rename(uint8_t *text)
{
	vlist_rename(VLIST_PLANE, text, x16_plane, gfx_idx + GFX_MODE_PLANES);
}

static void te_plane_variant_new(uint8_t *text)
{
	vlist_var_new(text, x16_plane, gfx_idx + GFX_MODE_PLANES);
}

static void te_plane_variant_rename(uint8_t *text)
{
	vlist_var_ren(text, x16_plane, gfx_idx + GFX_MODE_PLANES);
}

static void fs_plane(uint8_t *file)
{
	image_t *img;
	uint32_t *src;
	uint32_t size;
	variant_list_t *pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;
	uint32_t pal = pl->now < MAX_X16_VARIANTS_PLN;

	img = img_png_load(file, pal);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(check_plane_resolution(img->width, img->height))
	{
		free(img);
		edit_status_printf("Invalid image resolution!");
		return;
	}

	size = img->width * img->height;

	if(pl->now >= MAX_X16_VARIANTS_PLN)
	{
		uint32_t *src = (uint32_t*)img->data;
		uint8_t colors[16];
		uint32_t count = 0;

		for(uint32_t i = 0; i < size; i++)
			pl->data[i] = x16g_palette_match(*src++, 1);

		for(uint32_t i = 0; i < size; i++)
		{
			uint8_t cc = pl->data[i];
			uint32_t j;

			for(j = 0; j < count; j++)
				if(colors[j] == cc)
					break;

			if(j < count)
				continue;

			if(count < 16)
				colors[count] = cc;
			count++;
			if(count > 16)
				break;
		}

		if(count <= 16)
			edit_status_printf("This image uses %u colors and could be converted to 4bpp.", count);
	} else
	{
		variant_info_t *pv;
		uint8_t *color = img->palette;
		uint8_t *src = img->data;

		if(img->palcount > 16)
		{
			edit_status_printf("Image contains more than 16 colors!");
			free(img);
			return;
		}

		pv = pl->variant + pl->now;
		pv->pl.bright = 0;
		memset(pv->pl.data, 0, 16);

		for(uint32_t i = 0; i < 16; i++)
		{
			uint32_t cc;

			cc = *color++;
			cc |= *color++ << 8;
			cc |= *color++ << 16;
			cc |= 0xFF000000;

			pv->pl.data[i] = x16g_palette_match(cc, 1);
		}

		for(uint32_t i = 0; i < size; i++)
			pl->data[i] = *src++;
	}

	pl->width = img->width;
	pl->height = img->height;

	free(img);

	update_gfx_mode(0);
}

static void fs_plane_raw(uint8_t *file)
{
	image_t *img;
	uint32_t *src;
	uint32_t size;
	variant_list_t *pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;

	img = img_png_load(file, 1);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(check_plane_resolution(img->width, img->height))
	{
		free(img);
		edit_status_printf("Invalid image resolution!");
		return;
	}

	memcpy(pl->data, img->data, img->width * img->height);

	pl->width = img->width;
	pl->height = img->height;

	free(img);

	update_gfx_mode(0);
}

static void qe_delete_plane(uint32_t res)
{
	if(!res)
		return;
	vlist_delete(x16_plane, gfx_idx + GFX_MODE_PLANES);
}

static void qe_delete_plane_variant(uint32_t res)
{
	if(!res)
		return;
	vlist_var_del(x16_plane, gfx_idx + GFX_MODE_PLANES);
}

static void te_effect_scale(uint8_t *text)
{
	int32_t temp;
	uint8_t bit;

	if(!text)
		return;

	if(sscanf(text, "%d", &temp) != 1 || temp < -255 || temp > 255)
		edit_status_printf("Invalid value!");

	bit = 0x80 >> (effect_idx - 2);

	if(temp < 0)
	{
		temp = -temp;
		effect_dest[0] |= bit;
	} else
		effect_dest[0] &= ~bit;

	effect_dest[effect_idx] = temp;

	update_gfx_mode(0);
}

int32_t uin_gfx_plane_new4(glui_element_t *elm, int32_t x, int32_t y)
{
	if(gfx_idx[GFX_MODE_PLANES].max >= MAX_X16_PLANES)
	{
		edit_status_printf("Too many planes!");
		return 1;
	}

	plane_8bpp = 0;
	edit_ui_textentry("Enter plane name.", LEN_X16_TEXTURE_NAME, te_plane_new);

	return 1;
}

int32_t uin_gfx_plane_new8(glui_element_t *elm, int32_t x, int32_t y)
{
	if(gfx_idx[GFX_MODE_PLANES].max >= MAX_X16_PLANES)
	{
		edit_status_printf("Too many planes!");
		return 1;
	}

	plane_8bpp = MAX_X16_VARIANTS_PLN;
	edit_ui_textentry("Enter plane name.", LEN_X16_TEXTURE_NAME, te_plane_new);

	return 1;
}

int32_t uin_gfx_plane_rename(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;
	edit_ui_textentry("Enter plane name.", LEN_X16_TEXTURE_NAME, te_plane_rename);
	return 1;
}

int32_t uin_gfx_plane_delete(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;

	edit_ui_question("Delete plane?", "Do you really want to delete this plane?\nThis operation is irreversible!", qe_delete_plane);

	return 1;
}

int32_t uin_gfx_plane_import(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *pl;

	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;

	pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;

	if(pl->now < MAX_X16_VARIANTS_PLN && !pl->max)
		return 1;

	edit_ui_file_select("Select PNG to load plane from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_plane);

	return 1;
}

int32_t uin_gfx_plane_import_raw(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *pl;

	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;

	pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;

	if(pl->now < MAX_X16_VARIANTS_PLN && !pl->max)
		return 1;

	edit_ui_file_select("Select PNG to load plane from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_plane_raw);

	return 1;
}

int32_t uin_gfx_plane_variant(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *pl;

	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;

	pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;

	if(pl->max >= MAX_X16_VARIANTS_PLN)
	{
		edit_status_printf("Too many variants!");
		return 1;
	}

	edit_ui_textentry("Enter variant name.", LEN_X16_TEXTURE_NAME, te_plane_variant_new);

	return 1;
}

int32_t uin_gfx_plane_renvar(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *pl;

	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;

	pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;

	if(!pl->max)
		return 1;

	edit_ui_textentry("Enter variant name.", LEN_X16_TEXTURE_NAME, te_plane_variant_rename);

	return 1;
}

int32_t uin_gfx_plane_delvar(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *pl;

	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;

	pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;

	if(!pl->max)
		return 1;

	edit_ui_question("Delete variant?", "Do you really want to delete this variant?\nThis operation is irreversible!", qe_delete_plane_variant);

	return 1;
}

int32_t uin_gfx_plane_display(glui_element_t *elm, int32_t x, int32_t y)
{
	plane_display = !plane_display;
	update_gfx_mode(0);
	return 1;
}

int32_t uin_gfx_plane_effect_btn(glui_element_t *elm, int32_t x, int32_t y)
{
	uint32_t btn = elm->base.custom;
	variant_list_t *pl;
	uint8_t *effect;
	uint32_t temp;

	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;

	pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;

	if(pl->now < MAX_X16_VARIANTS_PLN)
	{
		if(!pl->max)
			return 1;
		effect = pl->variant[pl->now].pl.effect;
	} else
		effect = pl->variant[0].pl.effect;

	switch(btn)
	{
		case 0: // change mode
			temp = effect[0] & X16G_MASK_PL_EFFECT;
			temp++;
			if(temp >= X16G_NUM_PLANE_EFFECTS)
				temp = 0;
			effect[0] = temp;

			for(uint32_t i = 1; i < X16G_NUM_PLANE_EFFECTS; i++)
				effect[i] = 0;
		break;
		case 1: // delay
			if(effect[1] & 0x80)
			{
				effect[1]--;
				if(effect[1] == 0x80)
					effect[1] = 0;
			} else
			{
				effect[1]++;
				if(effect[1] > 6)
					effect[1] = 0x84;
			}
			if(	(effect[0] & X16G_MASK_PL_EFFECT) == X16G_PL_EFFECT_ANIMATE &&
				effect[1] & 0x80
			)
				effect[1] = 0;
		break;
		default:
			switch(effect[0] & X16G_MASK_PL_EFFECT)
			{
				case X16G_PL_EFFECT_RANDOM:
					temp = (effect[2] & 1) | (effect[2] >> 5);

					temp++;
					if(temp >= 7)
						temp = 0;

					effect[2] = (temp & 0x01) | ((temp << 5) & 0xC0);
				break;
				case X16G_PL_EFFECT_ANIMATE:
					if(btn == 3)
					{
						effect[3]++;
						if(effect[3] > MAX_X16_VARIANTS_PLN)
							effect[3] = 0;
					} else
					{
						temp = (effect[2] + 1) * 2;
						if(temp > MAX_X16_VARIANTS_PLN)
							temp = 2;
						effect[2] = temp - 1;
					}
				break;
				default:
					effect_dest = effect;
					effect_idx = btn;
					edit_ui_textentry("Enter value between -255 and 255.", 4, te_effect_scale);
				return 1;
			}
		break;
	}

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_plane_colormap(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *pl;
	int32_t idx;

	if(!gfx_idx[GFX_MODE_PLANES].max)
		return 1;

	pl = x16_plane + gfx_idx[GFX_MODE_PLANES].now;
	if(!pl->max)
		return 1;

	idx = y / base_height_cmaps;

	if(idx >= pl->max)
		return 1;

	if(pl->now == idx)
	{
		idx = x / base_height_cmaps;
		pl->variant[pl->now].pl.bright ^= 1 << idx;
	} else
		pl->now = idx;

	update_gfx_mode(0);

	return 1;
}

//
// input: walls

static void te_wall_new(uint8_t *text)
{
	vlist_new(VLIST_WALL, text, x16_wall, gfx_idx + GFX_MODE_WALLS, 0);
}

static void te_wall_rename(uint8_t *text)
{
	vlist_rename(VLIST_WALL, text, x16_wall, gfx_idx + GFX_MODE_WALLS);
}

static void fs_wall(uint8_t *file)
{
	image_t *img;
	uint32_t *src;
	uint32_t size;
	variant_list_t *wl;
	variant_info_t *vi;
	variant_info_t backup;

	img = img_png_load(file, stex_import_pal);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	wl = x16_wall + gfx_idx[GFX_MODE_WALLS].now;
	vi = wl->variant + wl->now;

	backup = *vi;

	if(stex_import_pal)
	{
		if(stex_wall_texture_8(img, wl))
			goto invalid;
	} else
	{
		if(stex_wall_texture_32(img, wl))
			goto invalid;
	}

	if(stex_remake_columns(wl))
	{
		*vi = backup;
		edit_status_printf("Too many used bytes!");
	}

	free(img);

	update_gfx_mode(0);
	return;

invalid:
	free(img);
	edit_status_printf("Invalid image resolution!");
	return;
}

static void te_wall_variant_new(uint8_t *text)
{
	vlist_var_new(text, x16_wall, gfx_idx + GFX_MODE_WALLS);
}

static void te_wall_variant_rename(uint8_t *text)
{
	vlist_var_ren(text, x16_wall, gfx_idx + GFX_MODE_WALLS);
}

static void qe_delete_wall(uint32_t res)
{
	if(!res)
		return;
	vlist_delete(x16_wall, gfx_idx + GFX_MODE_WALLS);
}

static void qe_delete_wall_variant(uint32_t res)
{
	if(!res)
		return;
	vlist_var_del(x16_wall, gfx_idx + GFX_MODE_WALLS);
}

static void qe_fullbright_wall_all(uint32_t res)
{
	uint16_t *ptr;
	variant_list_t *wa;

	if(!res)
		return;

	wa = x16_wall + gfx_idx[GFX_MODE_WALLS].now;
	ptr = (uint16_t*)wa->data;

	for(uint32_t i = 0; i < wa->stex_used; i++)
	{
		*ptr = *ptr | 0xFF00;
		ptr++;
	}

	stex_remake_columns(wa);
	update_gfx_mode(0);
}

int32_t uin_gfx_wall_new(glui_element_t *elm, int32_t x, int32_t y)
{
	if(gfx_idx[GFX_MODE_WALLS].max >= MAX_X16_WALLS)
	{
		edit_status_printf("Too many walls!");
		return 1;
	}

	edit_ui_textentry("Enter wall name.", LEN_X16_TEXTURE_NAME, te_wall_new);

	return 1;
}

int32_t uin_gfx_wall_delete(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	edit_ui_question("Delete wall?", "Do you really want to delete this wall with all its variants?\nThis operation is irreversible!", qe_delete_wall);

	return 1;
}

int32_t uin_gfx_wall_rename(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;
	edit_ui_textentry("Enter wall name.", LEN_X16_TEXTURE_NAME, te_wall_rename);
	return 1;
}

int32_t uin_gfx_wall_import(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *wa;

	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	wa = x16_wall + gfx_idx[GFX_MODE_WALLS].now;

	if(!wa->max)
		return 1;

	stex_import_pal = (void*)elm == (void*)&ui_gfx_wall_import_raw;
	edit_ui_file_select("Select PNG to load wall from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_wall);

	return 1;
}

int32_t uin_gfx_wall_variant(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *wa;

	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	wa = x16_wall + gfx_idx[GFX_MODE_WALLS].now;

	if(wa->max >= MAX_X16_VARIANTS)
	{
		edit_status_printf("Too many variants!");
		return 1;
	}

	edit_ui_textentry("Enter variant name.", LEN_X16_TEXTURE_NAME, te_wall_variant_new);

	return 1;
}

int32_t uin_gfx_wall_renvar(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *wa;

	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	wa = x16_wall + gfx_idx[GFX_MODE_WALLS].now;

	if(!wa->max)
		return 1;

	edit_ui_textentry("Enter variant name.", LEN_X16_TEXTURE_NAME, te_wall_variant_rename);

	return 1;
}

int32_t uin_gfx_wall_delvar(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *wa;

	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	wa = x16_wall + gfx_idx[GFX_MODE_WALLS].now;

	if(!wa->max)
		return 1;

	edit_ui_question("Delete variant?", "Do you really want to delete this variant?\nThis operation is irreversible!", qe_delete_wall_variant);

	return 1;
}

int32_t uin_gfx_wall_display(glui_element_t *elm, int32_t x, int32_t y)
{
	wall_display = !wall_display;
	update_gfx_mode(0);
	return 1;
}

int32_t uin_gfx_wall_fullbright_all(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	edit_ui_question("Make fullbright?", "Do you really want to make all variants fullbright?\nThis operation is irreversible!", qe_fullbright_wall_all);

	return 1;
}

int32_t uin_gfx_wall_animation_btn(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *wa;
	variant_info_t *vi;
	uint8_t *anim;
	uint32_t temp;

	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	wa = x16_wall + gfx_idx[GFX_MODE_WALLS].now;
	vi = wa->variant + wa->now;
	anim = vi->sw.anim;

	switch(elm->base.custom)
	{
		case 0:
			if(anim[0])
			{
				temp = (anim[0] + 1) * 2;
				if(temp > MAX_X16_VARIANTS)
					temp = 2;
				anim[0] = temp - 1;
			} else
				anim[0] = 1;
		break;
		case 1:
			anim[1]++;
			if(anim[1] > 6)
				anim[1] = 0;
		break;
		case 2:
			anim[2]++;
			if(anim[2] > MAX_X16_VARIANTS)
				anim[2] = 0;
		break;
	}

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_wall_left(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *wa;

	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	wa = x16_wall + gfx_idx[GFX_MODE_WALLS].now;
	if(wa->now)
		wa->now--;
	else
		wa->now = wa->max - 1;

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_wall_right(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *wa;

	if(!gfx_idx[GFX_MODE_WALLS].max)
		return 1;

	wa = x16_wall + gfx_idx[GFX_MODE_WALLS].now;
	wa->now++;
	if(wa->now >= wa->max)
		wa->now = 0;

	update_gfx_mode(0);

	return 1;
}

//
// input: sprites

static void te_sprite_new(uint8_t *text)
{
	vlist_new(VLIST_SPRITE, text, x16_sprite, gfx_idx + GFX_MODE_SPRITES, 0);
}

static void te_sprite_rename(uint8_t *text)
{
	vlist_rename(VLIST_SPRITE, text, x16_sprite, gfx_idx + GFX_MODE_SPRITES);
}

static void fs_sprite(uint8_t *file)
{
	image_t *img;
	uint32_t *src;
	uint32_t size;
	variant_list_t *sp;
	variant_info_t *vi;
	variant_info_t backup;

	img = img_png_load(file, stex_import_pal);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(check_sprite_resolution(img->width, img->height))
	{
		free(img);
		edit_status_printf("Invalid image resolution!");
		return;
	}

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;
	vi = sp->variant + sp->now;

	backup = *vi;

	if(stex_sprite_texture(img, sp))
	{
		*vi = backup;
		edit_status_printf("Bad sprite columns!");
	} else
	if(stex_remake_columns(sp))
	{
		*vi = backup;
		edit_status_printf("Too many used bytes!");
	}

	free(img);

	update_gfx_mode(0);
}

static void te_sprite_variant_new(uint8_t *text)
{
	vlist_var_new(text, x16_sprite, gfx_idx + GFX_MODE_SPRITES);
}

static void te_sprite_variant_rename(uint8_t *text)
{
	vlist_var_ren(text, x16_sprite, gfx_idx + GFX_MODE_SPRITES);
}

static void qe_delete_sprite(uint32_t res)
{
	if(!res)
		return;
	vlist_delete(x16_sprite, gfx_idx + GFX_MODE_SPRITES);
}

static void qe_delete_sprite_variant(uint32_t res)
{
	if(!res)
		return;
	vlist_var_del(x16_sprite, gfx_idx + GFX_MODE_SPRITES);
}

int32_t uin_gfx_sprite_new(glui_element_t *elm, int32_t x, int32_t y)
{
	if(gfx_idx[GFX_MODE_SPRITES].max >= MAX_X16_THGSPR)
	{
		edit_status_printf("Too many sprites!");
		return 1;
	}

	edit_ui_textentry("Enter sprite name.", LEN_X16_TEXTURE_NAME, te_sprite_new);

	return 1;
}

int32_t uin_gfx_sprite_delete(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	edit_ui_question("Delete sprite?", "Do you really want to delete this sprite?\nThis operation is irreversible!", qe_delete_sprite);

	return 1;
}

int32_t uin_gfx_sprite_rename(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;
	edit_ui_textentry("Enter sprite name.", LEN_X16_TEXTURE_NAME, te_sprite_rename);
	return 1;
}

int32_t uin_gfx_sprite_import(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;

	if(!sp->max)
		return 1;

	stex_import_pal = (void*)elm == (void*)&ui_gfx_sprite_import_raw;
	edit_ui_file_select("Select PNG to load sprite from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_sprite);

	return 1;
}

int32_t uin_gfx_sprite_variant(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;

	if(sp->max >= MAX_X16_VARIANTS)
	{
		edit_status_printf("Too many variants!");
		return 1;
	}

	edit_ui_textentry("Enter frame and amination code.", 3, te_sprite_variant_new);

	return 1;
}

int32_t uin_gfx_sprite_renvar(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;

	if(!sp->max)
		return 1;

	edit_ui_textentry("Enter variant name.", 3, te_sprite_variant_rename);

	return 1;
}

int32_t uin_gfx_sprite_delvar(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;

	if(!sp->max)
		return 1;

	edit_ui_question("Delete variant?", "Do you really want to delete this variant?\nThis operation is irreversible!", qe_delete_sprite_variant);

	return 1;
}

int32_t uin_gfx_sprite_display(glui_element_t *elm, int32_t x, int32_t y)
{
	sprite_display = !sprite_display;
	update_gfx_mode(0);
	return 1;
}

int32_t uin_gfx_sprite_show_origin(glui_element_t *elm, int32_t x, int32_t y)
{
	sprite_origin++;
	if(sprite_origin > 2)
		sprite_origin = 0;
	update_gfx_mode(0);
	return 1;
}

int32_t uin_gfx_sprite_offs_x(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;
	variant_info_t *va;
	int32_t third;
	int32_t value;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;
	va = sp->variant + sp->now;

	third = elm->base.width / 3;

	if(x < third)
		va->sw.ox--;
	else
	if(x > third * 2)
		va->sw.ox++;

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_sprite_offs_y(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;
	variant_info_t *va;
	int32_t third;
	int32_t value;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;
	va = sp->variant + sp->now;

	third = elm->base.width / 3;

	if(x < third)
		va->sw.oy--;
	else
	if(x > third * 2)
		va->sw.oy++;

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_sprite_mirror(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;
	variant_info_t *va;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;
	if(!sp->max)
		return 1;

	va = sp->variant + sp->now;

	if(!va->sw.width)
		return 1;

	for(uint32_t x = 0; x < va->sw.width; x++)
		va->sw.temp[x] = va->sw.offset[va->sw.width - x - 1];

	for(uint32_t x = 0; x < va->sw.width; x++)
		va->sw.offset[x] = va->sw.temp[x];

	for(uint32_t x = 0; x < va->sw.width; x++)
		va->sw.temp[x] = va->sw.length[va->sw.width - x - 1];

	for(uint32_t x = 0; x < va->sw.width; x++)
		va->sw.length[x] = va->sw.temp[x];

	va->sw.ox = -va->sw.ox;

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_sprite_left(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;
	if(sp->now)
		sp->now--;
	else
		sp->now = sp->max - 1;

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_sprite_right(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *sp;

	if(!gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	sp = x16_sprite + gfx_idx[GFX_MODE_SPRITES].now;
	sp->now++;
	if(sp->now >= sp->max)
		sp->now = 0;

	update_gfx_mode(0);

	return 1;
}

//
// input: weapons

static void te_weapon_new(uint8_t *text)
{
	vlist_new(VLIST_WEAPON, text, x16_weapon, gfx_idx + GFX_MODE_WEAPONS, 0);
}

static void te_weapon_rename(uint8_t *text)
{
	vlist_rename(VLIST_WEAPON, text, x16_weapon, gfx_idx + GFX_MODE_WEAPONS);
}

static void fs_weapon(uint8_t *file)
{
	image_t *img;
	uint8_t *dst;
	uint32_t *src;
	uint32_t size;
	gltex_info_t *gi;

	img = img_png_load(file, 0);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(check_weapon_resolution(img->width, img->height))
	{
		free(img);
		edit_status_printf("Invalid image resolution!");
		return;
	}

	wpn_part_copy = -1;
	stex_wpn_import = img->width | (img->height << 16);
	memset(wpn_part_info, 0, sizeof(wpn_part_info));

	size = img->width * img->height;
	src = (uint32_t*)img->data;
	dst = (uint8_t*)stex_source;

	for(uint32_t i = 0; i < size; i++)
		*dst++ = x16g_palette_match(*src++, 0);

	gi = gltex_info + X16G_GLTEX_SHOW_TEXTURE;
	gi->width = img->width;
	gi->height = img->height;
	gi->format = GL_LUMINANCE;
	gi->data = stex_source;
	x16g_update_texture(X16G_GLTEX_SHOW_TEXTURE);

	ui_gfx_wpn_texture.base.width = img->width * UI_WPN_IMG_SCALE;
	ui_gfx_wpn_texture.base.height = img->height * UI_WPN_IMG_SCALE;
	ui_gfx_wpn_texture.base.x = 364 - ui_gfx_wpn_texture.base.width / 2;
	ui_gfx_wpn_texture.base.y = 433 - ui_gfx_wpn_texture.base.height / 2;
	ui_gfx_wpn_texture.coord.s[0] = 0.0f;
	ui_gfx_wpn_texture.coord.s[1] = 1.0f;
	ui_gfx_wpn_texture.coord.t[0] = 0.0f;
	ui_gfx_wpn_texture.coord.t[1] = 1.0f;

	free(img);

	update_gfx_mode(0);
}

static void te_weapon_variant_new(uint8_t *text)
{
	uint32_t count = 0;
	uint8_t *ptr = text;
	variant_list_t *vl = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	variant_info_t *va;
	uint32_t base;

	if(!text)
		return;

	while(1)
	{
		uint8_t in = *ptr++;

		if(count >= MAX_X16_WPNGROUP)
		{
			edit_status_printf("Invalid name!");
			return;
		}

		if(!in)
			break;

		for(uint8_t *check = ptr - 2; check >= text; check--)
		{
			if(*check == in)
			{
				edit_status_printf("Invalid name!");
				return;
			}
		}

		if(in >= 'a' && in <= 'z')
			in -= 'a';
		else
		if(in >= 'A' && in <= 'Z')
			in -= 'A';
		else
		{
			edit_status_printf("Invalid name!");
			return;
		}

		for(uint32_t i = 0; i < vl->max; i++)
		{
			if(vl->variant[i].wpn.frm == in)
			{
				edit_status_printf("Variant with this name already exists!");
				return;
			}
		}

		count++;
	}

	if(vl->max + count > MAX_X16_VARIANTS)
	{
		edit_status_printf("Too many variants!");
		return;
	}

	base = vl->max;
	ptr = text;
	va = vl->variant + vl->max;
	for(uint32_t i = 0; i < count; i++, va++, ptr++)
	{
		uint8_t *name = va->name;

		vlist_var_new(ptr, x16_weapon, gfx_idx + GFX_MODE_WEAPONS);

		va->wpn.count = i ? 0xFF : count;
		va->wpn.base = base;

		for(uint32_t j = 0; j < count; j++)
		{
			uint8_t in = text[j];

			if(j == i)
			{
				*name++ = '[';
				*name++ = in & 0xDF;
				*name++ = ']';
			} else
				*name++ = in | 0x20;
		}
		*name = 0;
	}

	wpn_part_copy = -1;

	update_gfx_mode(0);
}

static void qe_delete_weapon(uint32_t res)
{
	if(!res)
		return;
	wpn_part_copy = -1;
	vlist_delete(x16_weapon, gfx_idx + GFX_MODE_WEAPONS);
}

static void qe_delete_weapon_variant(uint32_t res)
{
	variant_info_t *va;
	variant_list_t *vl;
	uint32_t count, top;

	if(!res)
		return;

	vl = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	va = vl->variant + vl->now;

	va = vl->variant + va->wpn.base;
	vl->now = va->wpn.base;

	top = vl->now + va->wpn.count;
	count = va->wpn.count;

	swpn_clear_data();

	memcpy(vl->variant + vl->now, vl->variant + top, sizeof(variant_info_t) * (vl->max - top));

	vl->max -= count;
	if(vl->max && vl->now >= vl->max)
		vl->now--;

	va = vl->variant;
	for(uint32_t i = 0; i < vl->max; )
	{
		uint32_t base = i;
		count = va->wpn.count;
		for( ; i < base + count; i++, va++)
			va->wpn.base = base;
	}

	wpn_part_copy = -1;

	update_gfx_mode(0);
}

static int32_t input_gfx_weapons(glui_element_t *elm, uint32_t magic)
{
	variant_list_t *ws;
	variant_info_t *va;
	wpnspr_part_t *src, *dst;

	if(stex_wpn_import)
		return 1;

	if(!gfx_idx[GFX_MODE_WEAPONS].max)
		return 1;

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;

	if(!ws->max)
		return 1;

	va = ws->variant + ws->now;

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	va = ws->variant + ws->now;

	if(magic == EDIT_INPUT_COPY)
	{
		wpn_part_copy = va->ws.now;
		edit_status_printf("Part copied.");
		return 1;
	}

	if(magic == EDIT_INPUT_PASTE)
	{
		if(wpn_part_copy < 0)
			return 1;

		if(va->ws.now == wpn_part_copy)
			return 1;

		dst = va->ws.part + va->ws.now;
		src = va->ws.part + wpn_part_copy;

		*dst = *src;
		update_gfx_mode(0);

		edit_status_printf("Part pasted.");

		return 1;
	}

	return 1;
}

int32_t uin_gfx_wpn_import(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws;

	if(!gfx_idx[GFX_MODE_WEAPONS].max)
		return 1;

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;

	if(!ws->max)
		return 1;

	if(stex_wpn_import)
	{
		stex_wpn_import = 0;
		update_gfx_mode(0);
	}

	edit_ui_file_select("Select PNG to load sprite from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_weapon);

	return 1;
}

int32_t uin_gfx_wpn_new(glui_element_t *elm, int32_t x, int32_t y)
{
	if(stex_wpn_import)
	{
		stex_wpn_import = 0;
		update_gfx_mode(0);
	}

	if(gfx_idx[GFX_MODE_WEAPONS].max >= MAX_X16_WPNSPR)
	{
		edit_status_printf("Too many weapon sprites already exists!");
		return 1;
	}

	edit_ui_textentry("Enter sprite name.", LEN_X16_LIGHT_NAME, te_weapon_new);

	return 1;
}

int32_t uin_gfx_wpn_rename(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_WEAPONS].max)
		return 1;
	edit_ui_textentry("Enter sprite name.", LEN_X16_TEXTURE_NAME, te_weapon_rename);
	return 1;
}

int32_t uin_gfx_wpn_delete(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_WEAPONS].max)
		return 1;

	if(stex_wpn_import)
	{
		stex_wpn_import = 0;
		update_gfx_mode(0);
	}

	edit_ui_question("Delete sprite?", "Do you really want to delete this sprite?\nThis operation is irreversible!", qe_delete_weapon);

	return 1;
}

int32_t uin_gfx_wpn_variant(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws;

	if(!gfx_idx[GFX_MODE_WEAPONS].max)
		return 1;

	if(stex_wpn_import)
	{
		stex_wpn_import = 0;
		update_gfx_mode(0);
	}

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;

	if(ws->max >= MAX_X16_VARIANTS)
	{
		edit_status_printf("Too many variants!");
		return 1;
	}

	edit_ui_textentry("Enter frame code.", MAX_X16_WPNGROUP + 1, te_weapon_variant_new);

	return 1;
}

int32_t uin_gfx_wpn_delvar(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws;

	if(!gfx_idx[GFX_MODE_WEAPONS].max)
		return 1;

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;

	if(!ws->max)
		return 1;

	if(stex_wpn_import)
	{
		stex_wpn_import = 0;
		update_gfx_mode(0);
	}

	edit_ui_question("Delete variant?", "Do you really want to delete this variant?\nThis operation is irreversible!", qe_delete_weapon_variant);

	return 1;
}

int32_t uin_gfx_wpn_left(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws;

	stex_wpn_import = 0;
	wpn_part_copy = -1;

	if(!gfx_idx[GFX_MODE_WEAPONS].max)
		return 1;

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	if(ws->now)
		ws->now--;
	else
		ws->now = ws->max - 1;

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_wpn_right(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws;

	stex_wpn_import = 0;
	wpn_part_copy = -1;

	if(!gfx_idx[GFX_MODE_WEAPONS].max)
		return 1;

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	ws->now++;
	if(ws->now >= ws->max)
		ws->now = 0;

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_wpn_part_sel(glui_element_t *elm, int32_t x, int32_t y)
{
	int32_t idx = y / UI_WPN_PART_HEIGHT;
	variant_list_t *ws;
	variant_info_t *va;

	if(idx >= UI_WPN_PARTS)
		return 1;

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	va = ws->variant + ws->now;

	va->ws.now = idx;

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_wpn_part_pos(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws;
	variant_info_t *va;
	wpnspr_part_t *part;

	ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	va = ws->variant + ws->now;

	if(stex_wpn_import)
		part = wpn_part_info + va->ws.now;
	else
		part = va->ws.part + va->ws.now;

	if(!part->width)
		return 1;
#if 0
	part->x = x / UI_WPN_IMG_SCALE;
	part->x -= part->width / 2;

	part->y = y / UI_WPN_IMG_SCALE;
	part->y -= part->height / 2;
#else
	part->x = x / UI_WPN_IMG_SCALE;
	part->y = y / UI_WPN_IMG_SCALE;
#endif

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_wpn_part_apply(glui_element_t *elm, int32_t x, int32_t y)
{
	int32_t ret = swpn_apply();

	if(ret < 0)
		return 1;

	if(!ret)
	{
		edit_status_printf("Weapon sprite generated.");
		return 1;
	}

	switch(ret)
	{
		case 1:
			edit_status_printf("Too many pixels!");
		break;
		case 2:
			edit_status_printf("Out of pixels!");
		break;
	}

	return 1;
}

int32_t uin_gfx_wpn_part_btn0(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	variant_info_t *va = ws->variant + ws->now;
	wpnspr_part_t *part;

	if(stex_wpn_import)
	{
		part = wpn_part_info + va->ws.now;

		if(part->width)
		{
			part->width *= 2;
			if(part->width > 64)
				part->width = 0;
		} else
			part->width = 8;

		if(!part->height)
			part->height = 8;
	} else
	{
		part = va->ws.part + va->ws.now;

		if(part->width)
			part->flags ^= WPN_PART_FLAG_DISABLED;
	}

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_wpn_part_btn1(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	variant_info_t *va = ws->variant + ws->now;
	wpnspr_part_t *part;

	if(stex_wpn_import)
	{
		part = wpn_part_info + va->ws.now;

		part->height *= 2;
		if(part->height > 64)
			part->height = 8;
	} else
	{
		part = va->ws.part + va->ws.now;

		if(part->width)
			part->flags ^= WPN_PART_FLAG_MIRROR_X;
	}

	update_gfx_mode(0);

	return 1;
}

int32_t uin_gfx_wpn_part_btn2(glui_element_t *elm, int32_t x, int32_t y)
{
	variant_list_t *ws = x16_weapon + gfx_idx[GFX_MODE_WEAPONS].now;
	variant_info_t *va = ws->variant + ws->now;
	wpnspr_part_t *part = wpn_part_info + va->ws.now;

	part->flags ^= WPN_PART_FLAG_BRIGHT;

	update_gfx_mode(0);

	return 1;
}

//
// input: skies

static void te_sky_new(uint8_t *name)
{
	editor_sky_t *sky;
	uint32_t count;
	uint32_t hash;

	if(!name)
		return;

	if(edit_check_name(name, 0))
	{
		edit_status_printf("Invalid name!");
		return;
	}

	hash = x16c_crc(name, -1, SKY_CRC_XOR);

	count = gfx_idx[GFX_MODE_SKIES].max;

	for(uint32_t i = 0; i < count; i++)
	{
		if(editor_sky[i].hash == hash)
		{
			edit_status_printf("Sky with this name already exists!");
			return;
		}
	}

	sky = editor_sky + count;
	strcpy(sky->name, name);
	sky->hash = hash;
	memset(sky->data, 0, X16_SKY_DATA_SIZE);

	gfx_idx[GFX_MODE_SKIES].now = count;
	gfx_idx[GFX_MODE_SKIES].max = count + 1;

	update_gfx_mode(0);
}

static void te_sky_rename(uint8_t *name)
{
	editor_sky_t *sky;
	uint32_t hash;

	if(!name)
		return;

	if(edit_check_name(name, 0))
	{
		edit_status_printf("Invalid name!");
		return;
	}

	hash = x16c_crc(name, -1, SKY_CRC_XOR);

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_SKIES].max; i++)
	{
		if(editor_sky[i].hash == hash)
		{
			edit_status_printf("Sky with this name already exists!");
			return;
		}
	}

	sky = editor_sky + gfx_idx[GFX_MODE_SKIES].now;

	strcpy(sky->name, name);
	sky->hash = hash;

	update_gfx_mode(0);
}

static void qe_delete_sky(uint32_t res)
{
	if(!res)
		return;

	gfx_idx[GFX_MODE_SKIES].max--;

	for(uint32_t i = gfx_idx[GFX_MODE_SKIES].now; i < gfx_idx[GFX_MODE_SKIES].max; i++)
		editor_sky[i] = editor_sky[i + 1];

	if(gfx_idx[GFX_MODE_SKIES].now >= gfx_idx[GFX_MODE_SKIES].max)
		gfx_idx[GFX_MODE_SKIES].now--;

	update_gfx_mode(0);
}

static void fs_sky(uint8_t *file)
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

	if(img->width != 512 || img->height != 120)
	{
		free(img);
		edit_status_printf("Invalid image resolution!");
		return;
	}

	src = (uint32_t*)img->data;
	dst = editor_sky[gfx_idx[GFX_MODE_SKIES].now].data;

	for(uint32_t i = 0; i < X16_SKY_DATA_SIZE; i++)
		*dst++ = x16g_palette_match(*src++ | 0xFF000000, 0);

	free(img);

	update_gfx_mode(0);
}

int32_t uin_gfx_sky_import(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_SKIES].max)
		return 1;

	edit_ui_file_select("Select PNG to load sky from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_sky);

	return 1;
}

int32_t uin_gfx_sky_new(glui_element_t *elm, int32_t x, int32_t y)
{
	if(gfx_idx[GFX_MODE_SKIES].max >= MAX_X16_SKIES)
	{
		edit_status_printf("Too many skies!");
		return 1;
	}

	edit_ui_textentry("Enter sky name.", LEN_X16_SKY_NAME, te_sky_new);

	return 1;
}

int32_t uin_gfx_sky_rename(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_SKIES].max)
		return 1;

	edit_ui_textentry("Enter sky name.", LEN_X16_SKY_NAME, te_sky_rename);

	return 1;
}

int32_t uin_gfx_sky_delete(glui_element_t *elm, int32_t x, int32_t y)
{
	if(!gfx_idx[GFX_MODE_SKIES].max)
		return 1;

	edit_ui_question("Delete sky?", "Do you really want to delete this sky?\nThis operation is irreversible!", qe_delete_sky);

	return 1;
}

//
// input: hud

static void hud_import_font(uint8_t *file)
{
	image_t *img;
	uint8_t *src;

	img = img_png_load(file, 1);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(img->width != 8 || img->height != FONT_CHAR_COUNT * 8)
	{
		free(img);
		edit_status_printf("Invalid image resolution!");
		return;
	}

	src = img->data;

	for(uint32_t i = 0; i < FONT_CHAR_COUNT; i++)
	{
		font_char_t *fc = font_char + i;
		uint8_t *dst = fc->data;

		fc->space = -9;

		for(uint32_t x = 0; x < 64; x += 2)
		{
			uint8_t in, out;

			in = *src++;
			if(in >= 16)
			{
				in = 0;
				if(fc->space < 0)
					fc->space = (x & 7) + (in - 16);
			}
			out = in;

			in = *src++;
			if(in >= 16)
			{
				in = 0;
				if(fc->space < 0)
					fc->space = (x & 7) + (in - 15);
			}
			out |= in << 4;

			*dst++ = out;
		}

		if(fc->space < 0)
			fc->space = -fc->space;
	}

	free(img);

	update_gfx_mode(0);
}

static void hud_import_nums(uint8_t *file)
{
	image_t *img;
	uint8_t *src;

	img = img_png_load(file, 1);
	if(!img)
	{
		edit_status_printf("Error loading PNG file!");
		return;
	}

	if(img->width != 8 || img->height != NUMS_CHAR_COUNT * 16)
	{
		free(img);
		edit_status_printf("Invalid image resolution!");
		return;
	}

	src = img->data;

	for(uint32_t i = 0; i < NUMS_CHAR_COUNT; i++)
	{
		nums_char_t *nc = nums_char + i;
		uint8_t *dst = nc->data;

		nc->space = -9;

		for(uint32_t x = 0; x < 128; x += 2)
		{
			uint8_t in, out;

			in = *src++;
			if(in >= 16)
			{
				in = 0;
				if(nc->space < 0)
					nc->space = (x & 7) + (in - 16);
			}
			out = in;

			in = *src++;
			if(in >= 16)
			{
				in = 0;
				if(nc->space < 0)
					nc->space = (x & 7) + (in - 15);
			}
			out |= in << 4;

			*dst++ = out;
		}

		if(nc->space < 0)
			nc->space = -nc->space;
	}

	free(img);

	update_gfx_mode(0);
}

static void fs_hud(uint8_t *file)
{
	hud_element[gfx_idx[GFX_MODE_HUD].now].import(file);
}

int32_t uin_gfx_hud_import(glui_element_t *elm, int32_t x, int32_t y)
{
	edit_ui_file_select("Select PNG to load HUD element from.", X16_DIR_DATA PATH_SPLIT_STR, ".png", fs_hud);

	return 1;
}

int32_t uin_gfx_hud_color(glui_element_t *elm, int32_t x, int32_t y)
{
	hud_display = (hud_display + 1) & 15;
	update_gfx_mode(0);
	return 1;
}

//
// tick

void x16g_tick(uint32_t count)
{
	static uint32_t last_gt;

	// level time is not advanced in this mode
	for(uint32_t i = 0; i < count; i++)
		edit_ui_tick();

	if(gametick - last_gt < 16)
		return;

	last_gt = gametick & ~15;

	for(uint32_t i = 0; i < MAX_X16_PALETTE; i++)
	{
		uint8_t color = gametick >> 4;

		if(gametick & 256)
			color = ~color;

		color &= 15;
		color |= color << 4;

		x16_palette_data[i * 256] = color | (color << 8) | (color << 16) | 0xFF000000;
	}

	x16g_update_texture(X16G_GLTEX_PALETTE);
}

//
// API

uint32_t x16g_init()
{
	void *ptr, *ttex;
	uint32_t size, tcnt;
	uint32_t *tmptex;
	glui_container_t *cont;

	sprintf(gfx_path, X16_PATH_DEFS PATH_SPLIT_STR "%s.gfx", X16_DEFAULT_NAME);

	for(uint32_t i = 0; i < GFX_NUM_MODES; i++)
		ui_set[i].text->base.custom = i;

	// UI stuff
	tcnt = NUM_RANGES_COLS * NUM_RANGES_ROWS;
	size = sizeof(glui_container_t); // ranges
	size += tcnt * (sizeof(glui_text_t*) + sizeof(glui_text_t)); // text elements and container slots

	size += sizeof(glui_container_t); // weapon part container
	size += (sizeof(glui_dummy_t*) + sizeof(glui_dummy_t)) * UI_WPN_PARTS; // weapon part container

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

	// remap ranges list
	cont = ptr;
	ui_gfx_remaps_ranges.elements[0] = ptr;
	ptr += sizeof(glui_container_t) + NUM_RANGES_COLS * NUM_RANGES_ROWS * sizeof(glui_text_t*);
	cont->base.draw = glui_df_container;
	cont->count = NUM_RANGES_COLS * NUM_RANGES_ROWS;

	text_ranges = ptr;

	tcnt = 0;
	for(uint32_t i = 0; i < NUM_RANGES_ROWS; i++)
	{
		for(uint32_t j = 0; j < NUM_RANGES_COLS; j++)
		{
			glui_text_t *tt = &ui_gfx_remap_table.elements[j]->text;
			glui_text_t *txt = ptr;

			ptr += sizeof(glui_text_t);

			cont->elements[tcnt++] = (glui_element_t*)txt;

			txt->base.draw = glui_df_text;
			txt->base.click = uin_remap_range_click;
			txt->base.custom = i | (j << 8);

			txt->base.x = tt->base.x;
			txt->base.y = i * UI_RANGE_HEIGHT;
			txt->base.width = tt->base.width;
			txt->base.height = UI_RANGE_HEIGHT;

			txt->base.color[1] = 0x11000000;

			txt->color[0] = 0xFF000000;
			txt->color[1] = 0xFF000000;

			txt->gltex = *tmptex++;
		}
	}

	// weapon parts

	cont = ptr;
	ui_gfx_wpn_part_rect_box.elements[0] = ptr;
	cont->base.draw = glui_df_container;
	cont->count = UI_WPN_PARTS;
	ptr += sizeof(glui_container_t) + UI_WPN_PARTS * sizeof(glui_dummy_t*);

	wpn_part_rect = ptr;

	for(uint32_t i = 0; i < UI_WPN_PARTS; i++)
	{
		glui_dummy_t *rect = ptr;

		ptr += sizeof(glui_dummy_t);

		cont->elements[i] = (glui_element_t*)rect;

		rect->base.draw = glui_df_dummy;
		rect->base.border_size = 1.0f;
	}

	// GL stuff

	glGenTextures(NUM_X16G_GLTEX, x16_gl_tex);
	glGenTextures(MAX_EDITOR_TEXTURES, x16_editor_gt);

	set_gfx_mode(GFX_MODE_PALETTE);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, x16_gl_tex[X16G_GLTEX_NOW_PALETTE]);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, x16_gl_tex[X16G_GLTEX_COLORMAPS]);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, x16_gl_tex[X16G_GLTEX_LIGHTS]);

	glActiveTexture(GL_TEXTURE0);

	base_height_cmaps = ui_gfx_plane_cmaps.base.height / MAX_X16_VARIANTS_PLN;

	for(uint32_t i = 0; i < NUM_X16G_GLTEX; i++)
	{
		glBindTexture(GL_TEXTURE_2D, x16_gl_tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if(i >= X16G_GLTEX_SPR_NONE)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}

	for(uint32_t i = 0; i < MAX_EDITOR_TEXTURES; i++)
	{
		glBindTexture(GL_TEXTURE_2D, x16_editor_gt[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glBindTexture(GL_TEXTURE_2D, x16_editor_gt[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, x16e_tex_unk_data);

	glBindTexture(GL_TEXTURE_2D, x16_editor_gt[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, x16e_tex_sky_data);

	// 'NONE'
	editor_texture[0].name[0] = '\t';
	editor_texture[0].type = X16G_TEX_TYPE_RGB;
	editor_texture[0].width = 64;
	editor_texture[0].height = 64;
	editor_texture[0].gltex = x16_editor_gt[0];
	editor_texture[0].data = x16e_tex_bad_data; // hack for software preview; resolution is 16x16

	// 'SKY'
	editor_texture[1].name[0] = 7;
	editor_texture[1].type = X16G_TEX_TYPE_RGB;
	editor_texture[1].width = 64;
	editor_texture[1].height = 64;
	editor_texture[1].gltex = x16_editor_gt[1];
	editor_texture[1].data = x16e_tex_sky_data;

	x16g_update_texture(X16G_GLTEX_UNK_TEXTURE);
	x16g_update_texture(X16G_GLTEX_SPR_NONE);
	x16g_update_texture(X16G_GLTEX_SPR_POINT);
	x16g_update_texture(X16G_GLTEX_SPR_BAD);

	x16g_update_texture(X16G_GLTEX_SPR_START);

	memcpy(x16e_spr_start_data, x16e_spr_st_coop_data, sizeof(x16e_spr_st_coop_data));
	x16g_update_texture(X16G_GLTEX_SPR_START_COOP);

	memcpy(x16e_spr_start_data, x16e_spr_st_dm_data, sizeof(x16e_spr_st_dm_data));
	x16g_update_texture(X16G_GLTEX_SPR_START_DM);

	// load GFX
	gfx_load(NULL);

	// generate GFX
	x16g_generate();

	return 0;
}

void x16g_mode_set()
{
	stex_wpn_import = 0;

	system_mouse_grab(0);
	system_draw = edit_ui_draw;
	system_input = (void*)edit_ui_input;
	system_tick = x16g_tick;

	shader_buffer.colormap = COLORMAP_IDX(0);
	shader_buffer.lightmap = LIGHTMAP_IDX(0);

	update_gfx_mode(0);
}

void x16g_new_gfx()
{
	gfx_cleanup();
	gfx_default_light();

	recalc_lights();

	x16g_update_texture(X16G_GLTEX_PALETTE);
	x16g_update_texture(X16G_GLTEX_LIGHTS);

	update_gfx_mode(0);
}

void x16g_generate()
{
	uint32_t gi = 2;
	uint32_t ci = 0;
	uint8_t data[256 * 256 * 2];

	edit_busy_window("Generating editor graphics ...");

	editor_texture_count = 2;

	// reset palette
	gltex_info[X16G_GLTEX_NOW_PALETTE].data = x16_palette_data;
	x16g_update_texture(X16G_GLTEX_NOW_PALETTE);

	// generate planes
	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_PLANES].max; i++)
	{
		variant_list_t *pl = x16_plane + i;

		if(gi >= MAX_EDITOR_TEXTURES)
			break;

		if(!pl->width)
			continue;

		if(!pl->height)
			continue;

		if(pl->now >= MAX_X16_VARIANTS_PLN)
		{
			// 8bpp
			editor_texture_t *et = editor_texture + editor_texture_count;
			uint16_t *dst = (uint16_t*)data;
			uint8_t *src = pl->data;

			for(uint32_t i = 0; i < pl->width * pl->height; i++)
			{
				uint16_t color = *src++;

				if(x16_palette_bright[color >> 4] & (1 << (color & 15)))
					color |= 0xFF00;

				*dst++ = color;
			}

			glBindTexture(GL_TEXTURE_2D, x16_editor_gt[gi]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, pl->width, pl->height, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);

			strcpy(et->name, pl->name);
			et->nhash = pl->hash;
			et->vhash = 0;
			et->type = X16G_TEX_TYPE_PLANE_8BPP;
			et->width = pl->width;
			et->height = pl->height;
			et->gltex = x16_editor_gt[gi];
			et->data = pl->data;
			et->effect = pl->variant[0].pl.effect;
			et->animate = NULL;

			editor_texture_count++;
			if(editor_texture_count >= MAX_EDITOR_TEXTURES)
				break;
		} else
		for(uint32_t j = 0; j < pl->max; j++)
		{
			// 4bpp
			variant_info_t *pi = pl->variant + j;
			editor_texture_t *et = editor_texture + editor_texture_count;

			glBindTexture(GL_TEXTURE_2D, x16_editor_gt[gi]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, pl->width, pl->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pl->data);

			sprintf(et->name, "%s\n%s", pl->name, pi->name);
			et->nhash = pl->hash;
			et->vhash = pi->hash;
			et->cmap = ci;
			et->type = X16G_TEX_TYPE_PLANE_4BPP;
			et->width = pl->width;
			et->height = pl->height;
			et->gltex = x16_editor_gt[gi];
			et->data = pl->data;
			et->effect = pi->pl.effect;
			et->animate = NULL;

			generate_colormap(ci, pi->pl.data, pi->pl.bright);

			ci++;
			if(ci >= MAX_X16_GL_CMAPS)
				break;

			editor_texture_count++;
			if(editor_texture_count >= MAX_EDITOR_TEXTURES)
				break;
		}

		gi++;
	}

	// generate walls
	if(editor_texture_count < MAX_EDITOR_TEXTURES)
	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_WALLS].max; i++)
	{
		variant_list_t *wa = x16_wall + i;

		for(uint32_t j = 0; j < wa->max; j++)
		{
			variant_info_t *wv = wa->variant + j;
			editor_texture_t *et = editor_texture + editor_texture_count;

			if(!wv->sw.width || !wv->sw.height)
				continue;

			sprintf(et->name, "%s\n%s", wa->name, wv->name);
			et->nhash = wa->hash;
			et->vhash = wv->hash;
			et->cmap = -1;
			et->type = wv->sw.masked ? X16G_TEX_TYPE_WALL_MASKED : X16G_TEX_TYPE_WALL;
			et->width = wv->sw.width;
			et->height = wv->sw.height;
			et->gltex = x16_editor_gt[gi];
			et->offs = wv->sw.offset;
			et->data = wa->data;
			et->effect = NULL;
			et->animate = wv->sw.anim;

			stex_generate_wall(wa, wv, (uint16_t*)data);

			glBindTexture(GL_TEXTURE_2D, x16_editor_gt[gi]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, wv->sw.width, wv->sw.height, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);

			gi++;
			if(gi >= MAX_EDITOR_TEXTURES)
				break;

			editor_texture_count++;
			if(editor_texture_count >= MAX_EDITOR_TEXTURES)
				break;
		}
	}

	// generate thing sprite links
	x16_num_sprlnk = 0;
	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_SPRITES].max; i++)
	{
		editor_sprlink_t *sl = editor_sprlink + x16_num_sprlnk;
		variant_list_t *sp = x16_sprite + i;
		uint32_t mask = 0;

		for(uint32_t j = 0; j < sp->max; j++)
		{
			variant_info_t *sv = sp->variant + j;

			if(!sv->sw.width)
				continue;

			if(!sv->sw.height)
				continue;

			mask |= 1 << sv->spr.frm;
		}

		if(!mask)
			continue;

		sl->name = sp->name;
		sl->hash = sp->hash;
		sl->vmsk = mask;
		sl->idx = i;
		x16_num_sprlnk++;
	}
	x16_num_sprlnk_thg = x16_num_sprlnk;

	// generate weapon sprite links
	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_WEAPONS].max; i++)
	{
		editor_sprlink_t *sl = editor_sprlink + x16_num_sprlnk;
		variant_list_t *ws = x16_weapon + i;
		uint32_t mask = 0;

		for(uint32_t j = 0; j < ws->max; j++)
		{
			variant_info_t *va = ws->variant + j;

			for(uint32_t k = 0; k < UI_WPN_PARTS; k++)
			{
				wpnspr_part_t *part = va->ws.part + k;

				if(!part->width)
					continue;

				if(part->flags & WPN_PART_FLAG_DISABLED)
					continue;

				mask |= 1 << va->wpn.frm;
				break;
			}
		}

		if(!mask)
			continue;

		sl->name = ws->name;
		sl->hash = ws->hash;
		sl->vmsk = mask;
		sl->idx = i;
		x16_num_sprlnk++;
	}
	x16_num_sprlnk_wpn = x16_num_sprlnk - x16_num_sprlnk_thg;

	// update colormaps
	x16g_update_texture(X16G_GLTEX_COLORMAPS);

	// update lights
	recalc_lights();
	x16_num_lights = gfx_idx[GFX_MODE_LIGHTS].max;
	x16g_update_texture(X16G_GLTEX_LIGHTS);
	x16g_update_texture(X16G_GLTEX_LIGHT_TEX);

	// update skies
	x16_num_skies = gfx_idx[GFX_MODE_SKIES].max;

	// update palette
	x16_palette_data[0] = 0;
	x16g_update_texture(X16G_GLTEX_PALETTE);

	// update map
	x16g_update_map();
}

void x16g_export()
{
	int32_t fd;
	uint8_t txt[64];

	union
	{
		uint8_t raw[STEX_PIXEL_LIMIT * 2 + sizeof(uint32_t) * 256];
		uint8_t sky[512 * 128];
		export_plane_t ep;
	} stuff;

	edit_busy_window("Exporting graphics ...");

	/// TABLES2.BIN (with HUD)

	fd = open(X16_PATH_EXPORT PATH_SPLIT_STR "TABLES2.BIN", O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if(fd >= 0)
	{
		uint32_t i;
		uint8_t ttmp[256];

		// HUD
		for(i = 0; i < NUMS_CHAR_COUNT; i++)
			write(fd, nums_char[i].data, 64);

		// font, 32 characters
		for(i = 0; i < 32; i++)
			write(fd, font_char[i].data, 32);

		// 256x16 / 64x64 tile map
		write(fd, tm_256x16, sizeof(tm_256x16));

		// font, next 32 characters
		for( ; i < 64; i++)
			write(fd, font_char[i].data, 32);

		// 128x32 tile map
		write(fd, tm_128x32, sizeof(tm_128x32));

		// font, last 32 characters
		for( ; i < 96; i++)
			write(fd, font_char[i].data, 32);

		// ranges
		write(fd, vram_ranges, sizeof(vram_ranges));

		// invalid texture
		for(i = 0; i < 256; i++)
			ttmp[i] = x16g_palette_match(((uint32_t*)x16e_tex_bad_data)[i], 0);

		make_plane_tiles(stuff.raw, ttmp, 16, 16);

		write(fd, stuff.raw, 256);

		close(fd);
	}

	/// TABLES3.BIN (palette)

	fd = open(X16_PATH_EXPORT PATH_SPLIT_STR "TABLES3.BIN", O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if(fd >= 0)
	{
		uint32_t *src = x16_palette_data;

		for(uint32_t i = 0; i < MAX_X16_PALETTE; i++)
		{
			uint16_t *dst = (uint16_t*)stuff.raw;

			*dst++ = 0;
			src++;

			for(uint32_t j = 1; j < 256; j++)
			{
				uint32_t pc = *src++;
				uint16_t color;

				color = (pc >> 20) & 0x00F;
				color |= (pc >> 8) & 0x0F0;
				color |= (pc << 4) & 0xF00;

				*dst++ = color;
			}

			write(fd, stuff.raw, 256 * 2);
		}

		close(fd);
	}

	/// lights

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_LIGHTS].max; i++)
	{
		uint8_t *src = x16_light_data + i * 256;
		uint8_t dst[256 * (MAX_X16_REMAPS+1)];

		for(uint32_t j = 0; j <= MAX_X16_REMAPS; j++)
		{
			make_light_data(i, j);
			memcpy(dst + j * 256, src, 256);
		}

		sprintf(txt, X16_PATH_EXPORT PATH_SPLIT_STR "%08X.LIT", editor_light[i].hash);

		edit_save_file(txt, dst, sizeof(dst));
	}

	/// planes

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_PLANES].max; i++)
	{
		variant_list_t *pl = x16_plane + i;
		uint32_t size;

		switch(pl->height)
		{
			case 256:
				stuff.ep.map_base = 0b11101010;
				make_plane_tiles(stuff.ep.bpp8.data, pl->data, 256, 16);
			break;
			case 128:
				stuff.ep.map_base = 0b11101110;
				make_plane_tiles(stuff.ep.bpp8.data, pl->data, 128, 32);
			break;
			case 64:
				stuff.ep.map_base = 0b11101001;
				make_plane_tiles(stuff.ep.bpp8.data, pl->data, 64, 64);
			break;
			default:
			continue;
		}

		if(pl->now >= MAX_X16_VARIANTS_PLN)
		{
			// 8bpp
			size = 2 + 64 * 64 + 4;
			stuff.ep.num_variants = 0xF0;
			memcpy(stuff.ep.bpp8.effect, pl->variant[0].pl.effect, 4);
		} else
		{
			// 4bpp
			uint8_t *src = stuff.ep.bpp8.data;

			if(!pl->max)
				continue;

			stuff.ep.num_variants = pl->max;

			for(uint32_t k = 0; k < 64 * 64 / 2; k++)
			{
				uint8_t out;

				out = *src++;
				out |= *src++ << 4;

				stuff.ep.bpp4.data[k] = out;
			}

			size = 2 + 64 * 64 / 2;
			size += pl->max * sizeof(export_plane_variant_t);

			for(uint32_t j = 0; j < pl->max; j++)
			{
				variant_info_t *pv = pl->variant + j;
				export_plane_variant_t *ev = stuff.ep.bpp4.variant + j;

				ev->hash = pv->hash;
				ev->bright = pv->pl.bright;
				memcpy(ev->data, pv->pl.data, 16);
				memcpy(ev->effect, pv->pl.effect, 4);
			}
		}

		sprintf(txt, X16_PATH_EXPORT PATH_SPLIT_STR "%08X.PLN", pl->hash);
		edit_save_file(txt, &stuff.ep, size);
	}

	/// walls

	stex_x16_export_wall(stuff.raw, txt);

	/// sprites

	stex_x16_export_sprite(stuff.raw, txt);

	/// weapons

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_WEAPONS].max; i++)
	{
		variant_list_t *ws = x16_weapon + i;
		uint8_t *ptr = stuff.raw;
		uint32_t count, bits;
		int32_t inv = -1;

		count = swpn_count_valid(ws, &bits, &inv);

		if(inv >= 0)
			edit_status_printf("Weapon '%s' frame %c has too many parts!", ws->name, 'A' + inv);

		if(!count)
			continue;

		*ptr++ = ws->stex_used >> 8;
		*ptr++ = count;

		memcpy(ptr, ws->data, ws->stex_used);
		ptr += ws->stex_used;

		for(uint32_t i = 0; i < ws->max; i++)
		{
			variant_info_t *va = ws->variant + i;
			uint32_t valid = 0;
			uint32_t nsz, tsz;
			uint8_t *pcnt;

			if(!(bits & (1 << i)))
				continue;

			nsz = va->ws.dbright >> 8;
			tsz = va->ws.dsize >> 8;

			pcnt = ptr;
			*ptr++ = 0; // will be filled later

			*ptr++ = va->ws.dstart >> 8;
			*ptr++ = va->wpn.frm;
			*ptr++ = nsz;
			*ptr++ = tsz - nsz;

			for(int32_t j = UI_WPN_PARTS-1; j >= 0; j--)
			{
				wpnspr_part_t *part = va->ws.part + j;
				uint32_t flags = 0x0C;

				if(!part->width)
					continue;

				if(part->flags & WPN_PART_FLAG_DISABLED)
					continue;

				switch(part->width)
				{
					case 8:
						flags |= 0 << 4;
					break;
					case 16:
						flags |= 1 << 4;
					break;
					case 32:
						flags |= 2 << 4;
					break;
					default:
						flags |= 3 << 4;
					break;
				}

				switch(part->height)
				{
					case 8:
						flags |= 0 << 6;
					break;
					case 16:
						flags |= 1 << 6;
					break;
					case 32:
						flags |= 2 << 6;
					break;
					default:
						flags |= 3 << 6;
					break;
				}

				flags |= part->flags & 3; // XY mirror

				*ptr++ = part->offset / 32;
				*ptr++ = 48 + part->x;
				*ptr++ = part->y;
				*ptr++ = flags;

				valid++;
			}

			*pcnt = valid;
		}

		sprintf(txt, X16_PATH_EXPORT PATH_SPLIT_STR "%08X.WPS", ws->hash);
		edit_save_file(txt, stuff.raw, ptr - stuff.raw);
	}

	/// skies

	memset(stuff.sky, 0, X16_SKY_DATA_RAW);

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_SKIES].max; i++)
	{
		editor_sky_t *sky = editor_sky + i;
		uint8_t *dst;

		for(uint32_t x = 0; x < X16_SKY_WIDTH / 2; x++)
		{
			uint8_t *src0 = sky->data + (X16_SKY_WIDTH / 2 - x - 1) + X16_SKY_HEIGHT * X16_SKY_WIDTH;
			uint8_t *dst0 = stuff.sky + x * X16_SKY_HEIGHT_RAW * 2;
			uint8_t *src1 = src0 + (X16_SKY_WIDTH / 2);
			uint8_t *dst1 = stuff.sky + x * X16_SKY_HEIGHT_RAW * 2 + X16_SKY_HEIGHT_RAW;

			for(uint32_t y = 0; y < X16_SKY_HEIGHT; y++)
			{
				src0 -= X16_SKY_WIDTH;
				*dst0++ = *src0;
				src1 -= X16_SKY_WIDTH;
				*dst1++ = *src1;
			}

			dst0 += X16_SKY_WIDTH * (X16_SKY_HEIGHT_RAW - X16_SKY_HEIGHT);
			dst1 += X16_SKY_WIDTH * (X16_SKY_HEIGHT_RAW - X16_SKY_HEIGHT);
		}

		sprintf(txt, X16_PATH_EXPORT PATH_SPLIT_STR "%08X.SKY", sky->hash);
		edit_save_file(txt, stuff.sky, X16_SKY_DATA_RAW);
	}

	// done
	edit_status_printf("GFX exported.");
}

void x16g_update_map()
{
	link_entry_t *ent;

	// sky
	edit_sky_num = -1;
	if(x16_sky_name[0] != '\t')
	{
		for(uint32_t i = 0; i < x16_num_skies; i++)
		{
			if(!strcmp(editor_sky[i].name, x16_sky_name))
			{
				edit_sky_num = i;
				break;
			}
		}
	}

	// textures
	ent = edit_list_sector.top;
	while(ent)
	{
		kge_sector_t *sec = (kge_sector_t*)(ent + 1);
		int32_t tex;

		sec->plane[PLANE_TOP].texture.idx = find_plane_texture(sec->plane[PLANE_TOP].texture.name);
		sec->plane[PLANE_BOT].texture.idx = find_plane_texture(sec->plane[PLANE_BOT].texture.name);

		sec->light.idx = find_sector_light(sec->light.name);

		for(uint32_t i = 0; i < sec->line_count; i++)
		{
			kge_line_t *line = sec->line + i;

			line->texture[0].idx = find_wall_texture(line->texture[0].name);
			line->texture[1].idx = find_wall_texture(line->texture[1].name);
			line->texture[2].idx = find_mask_texture(line->texture[2].name);
		}

		ent = ent->next;
	}
}

void x16g_update_texture(uint32_t tex)
{
	const gltex_info_t *ti = gltex_info + tex;
	const void *data = ti->data;
	uint32_t width = ti->width;
	uint32_t height = ti->height;

	if(tex == X16G_GLTEX_PALETTE)
		for(uint32_t i = 1; i < MAX_X16_PALETTE; i++)
			x16_palette_data[i * 256] = x16_palette_data[0];

	if(tex == X16G_GLTEX_SHOW_LIGHT)
		data += 256 * gfx_idx[GFX_MODE_LIGHTS].now;

	glBindTexture(GL_TEXTURE_2D, x16_gl_tex[tex]);
	glTexImage2D(GL_TEXTURE_2D, 0, ti->format, width, height, 0, ti->format, GL_UNSIGNED_BYTE, data);

	if(tex == X16G_GLTEX_PALETTE)
		x16g_update_texture(X16G_GLTEX_NOW_PALETTE);
}

void x16g_set_texture(uint32_t slot, uint32_t width, uint32_t height, uint32_t format, void *data)
{
	gltex_info_t *gi;

	gi = gltex_info + slot;
	gi->width = width;
	gi->height = height;
	gi->format = format;
	gi->data = data;

	x16g_update_texture(slot);
}

uint32_t x16g_generate_state_texture(uint32_t idx, uint32_t frm, uint32_t rot)
{
	variant_list_t *sp = x16_sprite + editor_sprlink[idx].idx;
	variant_info_t *match_perfect = NULL;
	variant_info_t *match_top_rot = NULL;
	variant_info_t *match_bad_rot = NULL;
	variant_info_t *match_bad_frm = NULL;
	uint32_t exact = rot & 0x80000000; // frame and rotation must match (with exception for non-rotated sprites)
	uint32_t exfrm = rot & 0x40000000; // only frame must match
	uint8_t *data;

	if(idx >= gfx_idx[GFX_MODE_SPRITES].max)
		return 1;

	rot &= 7;

	for(uint32_t i = 0; i < sp->max; i++)
	{
		variant_info_t *sv = sp->variant + i;

		if(!sv->sw.width)
			continue;

		if(!sv->sw.height)
			continue;

		if(sv->spr.frm == frm)
		{
			if(sv->spr.rot == rot)
			{
				match_perfect = sv;
				break;
			}
			if(!match_bad_rot)
				match_bad_rot = sv;
			if(!sv->spr.rot)
				match_top_rot = sv;
		}

		if(!match_bad_frm)
			match_bad_frm = sv;
	}

	if(!match_perfect)
	{
		if(!match_top_rot)
		{
			if(exact)
				return 1;

			if(!match_bad_rot)
			{
				if(exfrm)
					return 1;

				match_perfect = match_bad_frm;
			} else
				match_perfect = match_bad_rot;
		} else
			match_perfect = match_top_rot;
	}

	if(!match_perfect)
		return 1;

	data = calloc(256 * 256, 2);
	if(data)
	{
		x16g_state_res[0] = match_perfect->sw.width;
		x16g_state_res[1] = match_perfect->sw.height;
		x16g_state_res[2] = match_perfect->sw.stride;

		x16g_state_offs[0] = match_perfect->sw.ox;
		x16g_state_offs[1] = match_perfect->sw.oy;

		stex_generate_sprite(sp, match_perfect, (uint16_t*)data);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, match_perfect->sw.stride, match_perfect->sw.height, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);

		free(data);

		x16g_state_data_ptr = (uint16_t*)sp->data;
		x16g_state_offs_ptr = match_perfect->sw.offset;

		return 0;
	}

	return 1;
}

uint32_t x16g_generate_weapon_texture(uint32_t idx, uint32_t frm, int32_t *box)
{
	variant_list_t *ws = x16_weapon + editor_sprlink[idx].idx;
	int32_t pick = -1;
	uint16_t *data;

	if(frm >= MAX_X16_SPRITE_FRAMES)
	{
		for(uint32_t i = 0; i < ws->max && pick < 0; i++)
		{
			variant_info_t *va = ws->variant + i;

			for(uint32_t j = 0; j < UI_WPN_PARTS; j++)
			{
				wpnspr_part_t *part = va->ws.part + i;

				if(!part->width)
					continue;

				if(part->flags & WPN_PART_FLAG_DISABLED)
					continue;

				pick = i;
				break;
			}
		}
	} else
	{
		for(uint32_t i = 0; i < ws->max && pick < 0; i++)
		{
			if(ws->variant[i].wpn.frm == frm)
				pick = i;
		}
	}

	if(pick < 0)
		return 1;

	data = generate_wpn_preview(ws->variant + pick, ws->data, box);
	if(!data)
		return 1;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 256, 256, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);

	return 0;
}

uint8_t x16g_palette_match(uint32_t color, uint32_t skip_bright)
{
	int32_t dist = 0x0FFFFFFF;
	uint32_t match = 0;

	if(!(color & 0xFF000000))
		return 0;

	for(uint32_t i = 1; i < 256; i++)
	{
		if(	!skip_bright ||
			!(x16_palette_bright[i >> 4] & (1 << (i & 15)))
		){
			uint32_t rgb = x16_palette_data[i];
			int32_t cd[4];
			int32_t tmp;

			tmp = color & 0xFF;
			tmp -= rgb & 0xFF;
			cd[0] = tmp * tmp;

			tmp = (color >> 8) & 0xFF;
			tmp -= (rgb >> 8) & 0xFF;
			cd[1] = tmp * tmp;

			tmp = (color >> 16) & 0xFF;
			tmp -= (rgb >> 16) & 0xFF;
			cd[2] = tmp * tmp;

			cd[3] = abs(cd[0] + cd[1] + cd[2]);
			if(!cd[3])
				return i;
			if(cd[3] < dist)
			{
				dist = cd[3];
				match = i;
			}
		}
	}

	return match;
}

void x16g_img_pal_conv(image_t *img, uint8_t *dst)
{
	uint32_t *src = (void*)img->data;
	uint32_t size = img->width * img->height;

	for(uint32_t i = 0; i < size; i++)
		*dst++ = x16g_palette_match(*src++, 0);
}

void x16g_update_palette(int32_t idx)
{
	gltex_info_t *gi;
	uint32_t *pal = x16_palette_data + idx * 256;

	*pal = 0;

	gi = gltex_info + X16G_GLTEX_NOW_PALETTE;
	gi->data = pal;

	x16g_update_texture(X16G_GLTEX_NOW_PALETTE);

	gi->data = x16_palette_data;
}

const uint8_t *x16g_save(const uint8_t *file)
{
	kgcbor_gen_t gen;
	uint8_t *src, *dst;
	uint8_t wbuf[256 * 256];
	uint32_t size;

	edit_busy_window("Saving GFX ...");

	if(file)
		strcpy(gfx_path, file);

	gen.ptr = edit_cbor_buffer;
	gen.end = edit_cbor_buffer + EDIT_EXPORT_BUFFER_SIZE;

	/// root
	edit_cbor_export(cbor_root, NUM_CBOR_ROOT, &gen);

	/// palettes
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_PALETTES].name, cbor_root[CBOR_ROOT_PALETTES].nlen);
	kgcbor_put_array(&gen, MAX_X16_PALETTE);

	src = (uint8_t*)x16_palette_data;
	for(uint32_t i = 0; i < MAX_X16_PALETTE; i++)
	{
		kgcbor_put_array(&gen, 255);

		src += 4;

		for(uint32_t j = 0; j < 255; j++)
		{
			uint32_t color;

			color = src[0] >> 4;
			color |= src[1] & 0x0F0;
			color |= (src[2] << 4) & 0xF00;
			kgcbor_put_u32(&gen, color);

			src += 4;
		}
	}

	/// lights
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_LIGHTS].name, cbor_root[CBOR_ROOT_LIGHTS].nlen);
	kgcbor_put_object(&gen, gfx_idx[GFX_MODE_LIGHTS].max);

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_LIGHTS].max; i++)
	{
		cbor_light[CBOR_LIGHT_D].s8 = &editor_light[i].des;
		cbor_light[CBOR_LIGHT_R].u8 = &editor_light[i].r;
		cbor_light[CBOR_LIGHT_G].u8 = &editor_light[i].g;
		cbor_light[CBOR_LIGHT_B].u8 = &editor_light[i].b;

		kgcbor_put_string(&gen, editor_light[i].name, -1);
		edit_cbor_export(cbor_light, NUM_CBOR_LIGHT, &gen);
	}

	/// remaps
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_REMAPS].name, cbor_root[CBOR_ROOT_REMAPS].nlen);
	kgcbor_put_array(&gen, MAX_X16_REMAPS);

	for(uint32_t i = 0; i < MAX_X16_REMAPS; i++)
	{
		color_remap_t *cr = x16_color_remap + i;

		kgcbor_put_array(&gen, cr->count);

		for(uint32_t j = 0; j < cr->count; j++)
		{
			remap_entry_t *ent = cr->entry + j;
			kgcbor_put_binary(&gen, ent->raw, 9/*sizeof(remap_entry_t)*/);
		}
	}

	/// planes
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_PLANES].name, cbor_root[CBOR_ROOT_PLANES].nlen);
	kgcbor_put_object(&gen, gfx_idx[GFX_MODE_PLANES].max);

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_PLANES].max; i++)
	{
		variant_list_t *pl = x16_plane + i;

		kgcbor_put_string(&gen, pl->name, -1);

		if(pl->now < MAX_X16_VARIANTS_PLN)
		{
			// 4bpp
			size = pl->width * pl->height / 2;

			src = pl->data;
			dst = wbuf;

			for(uint32_t i = 0; i < size; i++)
			{
				uint8_t temp;
				temp = *src++;
				temp |= *src++ << 4;
				*dst++ = temp;
			}

			cbor_plane4[CBOR_PLANE4_WIDTH].u32 = &pl->width;
			cbor_plane4[CBOR_PLANE4_HEIGHT].u32 = &pl->height;
			cbor_plane4[CBOR_PLANE4_DATA].ptr = wbuf;
			cbor_plane4[CBOR_PLANE4_DATA].extra = size;

			edit_cbor_export(cbor_plane4, NUM_CBOR_PLANE4, &gen);

			// variants
			kgcbor_put_string(&gen, cbor_plane4[CBOR_PLANE4_VARIANTS].name, cbor_plane4[CBOR_PLANE4_VARIANTS].nlen);
			kgcbor_put_object(&gen, pl->max);

			for(uint32_t j = 0; j < pl->max; j++)
			{
				kgcbor_put_string(&gen, pl->variant[j].name, -1);

				cbor_plane_var[CBOR_PLANE_VARIANT_DATA].ptr = (void*)&pl->variant[j].pl.bright;
				cbor_plane_var[CBOR_PLANE_VARIANT_EFFECT].ptr = (void*)&pl->variant[j].pl.effect;

				edit_cbor_export(cbor_plane_var, NUM_CBOR_PLANE_VARIANT, &gen);
			}
		} else
		{
			// 8bpp
			cbor_plane8[CBOR_PLANE8_WIDTH].u32 = &pl->width;
			cbor_plane8[CBOR_PLANE8_HEIGHT].u32 = &pl->height;
			cbor_plane8[CBOR_PLANE8_DATA].ptr = pl->data;
			cbor_plane8[CBOR_PLANE8_DATA].extra = pl->width * pl->height;

			edit_cbor_export(cbor_plane8, NUM_CBOR_PLANE8, &gen);
		}
	}

	/// walls
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_WALLS].name, cbor_root[CBOR_ROOT_WALLS].nlen);
	stex_export_cbor(&gen, x16_wall, gfx_idx[CBOR_ROOT_WALLS].max, 0);

	/// sprites
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_SPRITES].name, cbor_root[CBOR_ROOT_SPRITES].nlen);
	stex_export_cbor(&gen, x16_sprite, gfx_idx[CBOR_ROOT_SPRITES].max, 1);

	/// weapons
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_WEAPONS].name, cbor_root[CBOR_ROOT_WEAPONS].nlen);
	kgcbor_put_object(&gen, gfx_idx[CBOR_ROOT_WEAPONS].max);

	for(uint32_t i = 0; i < gfx_idx[CBOR_ROOT_WEAPONS].max; i++)
	{
		variant_list_t *vl = x16_weapon + i;
		uint32_t count = 0;

		for(uint32_t j = 0; j < vl->max; )
		{
			count++;
			j += vl->variant[j].wpn.count;
		}

		kgcbor_put_string(&gen, vl->name, -1);
		kgcbor_put_array(&gen, count);

		if(!count)
			continue;

		/// one variant group

		for(uint32_t j = 0; j < vl->max; )
		{
			variant_info_t *vi = vl->variant + j;
			uint32_t top = j + vi->wpn.count;

			cbor_wpn_vgrp[CBOR_WPN_VGRP_NDATA].ptr = vl->data + vi->ws.dstart;
			cbor_wpn_vgrp[CBOR_WPN_VGRP_NDATA].extra = vi->ws.dbright;

			cbor_wpn_vgrp[CBOR_WPN_VGRP_BDATA].ptr = vl->data + vi->ws.dstart + vi->ws.dbright;
			cbor_wpn_vgrp[CBOR_WPN_VGRP_BDATA].extra = vi->ws.dsize - vi->ws.dbright;

			edit_cbor_export(cbor_wpn_vgrp, NUM_CBOR_WPN_VGRP, &gen);

			kgcbor_put_string(&gen, cbor_wpn_vgrp[CBOR_WPN_VGRP_VARIANTS].name, cbor_wpn_vgrp[CBOR_WPN_VGRP_VARIANTS].nlen);
			kgcbor_put_object(&gen, vi->wpn.count);

			for( ; j < top; j++, vi++)
			{
				uint8_t frm = 'A' + vi->wpn.frm;

				kgcbor_put_string(&gen, &frm, 1);
				kgcbor_put_array(&gen, UI_WPN_PARTS);

				for(uint32_t k = 0; k < UI_WPN_PARTS; k++)
				{
					wpnspr_part_t *part = vi->ws.part + k;

					if(part->width)
					{
						load_part = *part;
						if(load_part.flags & WPN_PART_FLAG_BRIGHT)
							load_part.offset -= vi->ws.dbright;
						edit_cbor_export(cbor_wpn_part, NUM_CBOR_WPN_PART, &gen);
					} else
						kgcbor_put_null(&gen);
				}
			}
		}
	}

	/// skies
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_SKIES].name, cbor_root[CBOR_ROOT_SKIES].nlen);
	kgcbor_put_object(&gen, gfx_idx[GFX_MODE_SKIES].max);

	for(uint32_t i = 0; i < gfx_idx[GFX_MODE_SKIES].max; i++)
	{
		editor_sky_t *sky = editor_sky + i;

		kgcbor_put_string(&gen, sky->name, -1);
		kgcbor_put_binary(&gen, sky->data, X16_SKY_DATA_SIZE);
	}

	/// font
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_FONT].name, cbor_root[CBOR_ROOT_FONT].nlen);
	kgcbor_put_array(&gen, FONT_CHAR_COUNT);

	cbor_font[CBOR_FONT_DATA].extra = 8 * 8 / 2;

	for(uint32_t i = 0; i < FONT_CHAR_COUNT; i++)
	{
		font_char_t *fc = font_char + i;

		cbor_font[CBOR_FONT_DATA].ptr = fc->data;
		cbor_font[CBOR_FONT_SPACE].u8 = (uint8_t*)&fc->space;

		edit_cbor_export(cbor_font, NUM_CBOR_FONT, &gen);
	}

	/// numbers
	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_NUMS].name, cbor_root[CBOR_ROOT_NUMS].nlen);
	kgcbor_put_array(&gen, NUMS_CHAR_COUNT);

	cbor_font[CBOR_FONT_DATA].extra = 16 * 8 / 2;

	for(uint32_t i = 0; i < NUMS_CHAR_COUNT; i++)
	{
		nums_char_t *nc = nums_char + i;

		cbor_font[CBOR_FONT_DATA].ptr = nc->data;
		cbor_font[CBOR_FONT_SPACE].u8 = (uint8_t*)&nc->space;

		edit_cbor_export(cbor_font, NUM_CBOR_FONT, &gen);
	}

	/// fullbright colors
	size = 0;
	for(uint32_t i = 0; i < 256; i++)
	{
		if(x16_palette_bright[i >> 4] & (1 << (i & 15)))
			wbuf[size++] = i;
	}

	kgcbor_put_string(&gen, cbor_root[CBOR_ROOT_FULLBRIGHT].name, cbor_root[CBOR_ROOT_FULLBRIGHT].nlen);
	kgcbor_put_array(&gen, size);

	for(uint32_t i = 0; i < size; i++)
		kgcbor_put_u32(&gen, wbuf[i]);

	// save
	if(edit_save_file(gfx_path, edit_cbor_buffer, (void*)gen.ptr - edit_cbor_buffer))
		return NULL;

	return edit_path_filename(gfx_path);
}

void x16g_set_sprite_texture(kge_thing_t *th)
{
	uint32_t tex = X16G_GLTEX_SPR_NONE;
	gltex_info_t *gi;
	thing_sprite_t *spr;

	switch(th->prop.type & 0xFF0000)
	{
		case 0x000000:
			spr = x16_thing_sprite + th->prop.type;

			if(spr->width)
			{
				th->sprite.gltex = x16_thing_texture[th->prop.type];
				th->sprite.width = spr->width;
				th->sprite.height = spr->height * 2;
				th->sprite.stride = spr->stride;
				th->sprite.ws = (float)spr->width / (float)spr->stride;
				th->sprite.shader = SHADER_FRAGMENT_PALETTE_LIGHT;
				th->sprite.ox = spr->ox;
				th->sprite.oy = spr->oy;
				th->sprite.data = spr->data;
				th->sprite.offs = spr->offs;
				th->sprite.bright = spr->bright;
				th->sprite.scale = ((float)thing_info[th->prop.type].info.scale * 2.0f + 64.0f) / 256.0f;

				return;
			}

			tex = X16G_GLTEX_SPR_BAD;
		break;
		case 0x020000:
			tex = X16G_GLTEX_SPR_START + (th->prop.type & 0xFFFF);
		break;
	}

	gi = gltex_info + tex;

	th->sprite.gltex = x16_gl_tex[tex];
	th->sprite.width = gi->width;
	th->sprite.height = gi->height * 2;
	th->sprite.stride = gi->width;
	th->sprite.ws = 1.0f;
	th->sprite.shader = SHADER_FRAGMENT_RGB;
	th->sprite.ox = 0;
	th->sprite.oy = 0;
	th->sprite.data = NULL;
	th->sprite.offs = NULL;
	th->sprite.bright = 1;
	th->sprite.scale = 1.0f;
}

void x16g_set_thingsel_texture(glui_image_t *img, uint32_t idx)
{
	gltex_info_t *gi = gltex_info + idx;

	img->shader = SHADER_FRAGMENT_RGB;
	img->gltex = x16_gl_tex + idx;

	img->base.width = gi->width;
	img->base.height = gi->height;
}

int32_t x16g_spritelink_by_hash(uint32_t hash)
{
	if(!hash)
		return -1;

	for(uint32_t i = 0; i < x16_num_sprlnk; i++)
	{
		if(editor_sprlink[i].hash == hash)
			return i;
	}

	return -1;
}

int32_t x16g_spritelink_by_name(const uint8_t *name, uint32_t is_wpn)
{
	is_wpn = is_wpn ? WPN_CRC_XOR : SPR_CRC_XOR;
	return x16g_spritelink_by_hash(x16c_crc(name, -1, is_wpn));
}

