
#define MAX_EDITOR_TEXTURES	2048

#define MAX_X16_PALETTE	16	// 4 palettes with 4 damage levels
#define MAX_X16_LIGHTS	32	// forced by LIGHTMAP_IDX macro, limited by exporter
#define MAX_X16_REMAPS	3	// engine limit
#define MAX_X16_PLANES	256
#define MAX_X16_WALLS	256
#define MAX_X16_THGSPR	112	// engine limit; (128 total, shared with weapons)
#define MAX_X16_WPNSPR	16	// engine limit; (128 total, shared with things)
#define MAX_X16_SKIES	32

#define MAX_X16_VARIANTS	64	// limited to < 128 by sprite links
#define MAX_X16_VARIANTS_PLN	16	// limited by UI design
#define MAX_X16_GL_CMAPS	256	// forced by COLORMAP_IDX macro
#define MAX_X16_WPNPARTS	15	// engine limit
#define MAX_X16_WPNGROUP	8

#define MAX_X16_SPRITE_FRAMES	26	// full uppercase alphabet

#define LEN_X16_TEXTURE_NAME	16
#define LEN_X16_VARIANT_NAME	16
#define LEN_X16_LIGHT_NAME	16
#define LEN_X16_SKY_NAME	16

#define X16_SKY_WIDTH	512
#define X16_SKY_HEIGHT	120
#define X16_SKY_HEIGHT_RAW	128
#define X16_SKY_DATA_SIZE	(X16_SKY_WIDTH * X16_SKY_HEIGHT)
#define X16_SKY_DATA_RAW	(X16_SKY_WIDTH * X16_SKY_HEIGHT_RAW)

#define COLORMAP_IDX(x)	((1.0f / 2048.0f) + (1.0f / 255.0f) * (float)(x))
#define LIGHTMAP_IDX(x)	((1.0f / 2048.0f) + (1.0f / 31.0f) * (float)(x))

#define X16G_GLTEX_SHOW_TEXTURE_ALT	X16G_GLTEX_BRIGHT_COLORS

enum
{
	X16G_GLTEX_PALETTE,
	X16G_GLTEX_LIGHTS,
	X16G_GLTEX_LIGHT_TEX,
	X16G_GLTEX_COLORMAPS,
	//
	X16G_GLTEX_NOW_PALETTE,
	X16G_GLTEX_SHOW_LIGHT,
	X16G_GLTEX_SHOW_TEXTURE,
	X16G_GLTEX_BRIGHT_COLORS,
	//
	X16G_GLTEX_UNK_TEXTURE,
	// sprite textures have 'wrap' disabled
	X16G_GLTEX_SPR_NONE,
	X16G_GLTEX_SPR_POINT,
	X16G_GLTEX_SPR_BAD,
	X16G_GLTEX_SPR_START,
	X16G_GLTEX_SPR_START_COOP,
	X16G_GLTEX_SPR_START_DM,
	//
	NUM_X16G_GLTEX
};

enum
{
	X16G_TEX_TYPE_WALL,
	X16G_TEX_TYPE_WALL_MASKED,
	X16G_TEX_TYPE_PLANE_8BPP,
	X16G_TEX_TYPE_PLANE_4BPP,
	//
	X16G_TEX_TYPE_RGB,
};

enum
{
	X16G_PL_EFFECT_NONE,
	X16G_PL_EFFECT_RANDOM,
	X16G_PL_EFFECT_CIRCLE,
	X16G_PL_EFFECT_EIGHT,
	X16G_PL_EFFECT_ANIMATE,
	//
	X16G_NUM_PLANE_EFFECTS,
	//
	X16G_MASK_PL_EFFECT = 7
};

typedef struct
{
	uint8_t name[LEN_X16_SKY_NAME];
	uint32_t hash;
	uint8_t data[X16_SKY_DATA_SIZE];
} editor_sky_t;

typedef struct
{
	uint8_t *name;
	uint32_t hash;
	uint32_t vmsk;
	uint32_t idx;
} editor_sprlink_t;

typedef struct
{
	uint8_t name[MAX_X16_MAPSTRING];
	uint32_t nhash;
	uint32_t vhash;
	int16_t cmap;
	uint8_t type;
	uint16_t width, height;
	uint32_t gltex;
	uint32_t *offs;
	const void *data;
	const uint8_t *effect;
	const uint8_t *animate;
} editor_texture_t;

typedef struct
{
	uint8_t name[LEN_X16_LIGHT_NAME];
	uint32_t hash;
	int8_t des;
	uint8_t r, g, b;
} editor_light_t;

struct image_s;
struct kge_thing_s;
struct glui_image_s;

//

extern uint32_t x16_gl_tex[NUM_X16G_GLTEX];

extern const uint8_t *const x16g_palette_name[MAX_X16_PALETTE];

extern uint32_t x16_palette_data[256 * MAX_X16_PALETTE];
extern uint16_t x16_palette_bright[16];

extern uint8_t x16_light_data[256 * MAX_X16_LIGHTS];

extern uint8_t x16_colormap_data[32 * MAX_X16_GL_CMAPS];

extern editor_texture_t editor_texture[MAX_EDITOR_TEXTURES + 1];
extern uint32_t editor_texture_count;

extern editor_light_t editor_light[MAX_X16_LIGHTS];
extern uint32_t x16_num_lights;

extern editor_sprlink_t editor_sprlink[MAX_X16_THGSPR + MAX_X16_WPNSPR];
extern uint32_t x16_num_sprlnk_thg;
extern uint32_t x16_num_sprlnk_wpn;
extern uint32_t x16_num_sprlnk;

extern uint32_t x16_num_skies;
extern editor_sky_t editor_sky[MAX_X16_SKIES];

extern uint32_t x16g_state_res[3];
extern int32_t x16g_state_offs[2];
extern uint16_t *x16g_state_data_ptr;
extern uint32_t *x16g_state_offs_ptr;

extern uint8_t x16_sky_name[LEN_X16_SKY_NAME];

//

uint32_t x16g_init();

void x16g_tick(uint32_t count);

const uint8_t *x16g_save(const uint8_t *file);

void x16g_new_gfx();

void x16g_mode_set();

void x16g_generate();
void x16g_export();

void x16g_update_map();

void x16g_update_texture(uint32_t);
void x16g_set_texture(uint32_t slot, uint32_t width, uint32_t height, uint32_t format, void *data);

uint32_t x16g_generate_state_texture(uint32_t idx, uint32_t frm, uint32_t rot);
uint32_t x16g_generate_weapon_texture(uint32_t idx, uint32_t frm, int32_t *box);

void x16g_set_sprite_texture(struct kge_thing_s *);
void x16g_set_thingsel_texture(struct glui_image_s *, uint32_t);

uint8_t x16g_palette_match(uint32_t color, uint32_t skip_bright);
void x16g_img_pal_conv(struct image_s *img, uint8_t *dst);

void x16g_update_palette(int32_t);

int32_t x16g_spritelink_by_hash(uint32_t hash);
int32_t x16g_spritelink_by_name(const uint8_t *name, uint32_t is_wpn);

