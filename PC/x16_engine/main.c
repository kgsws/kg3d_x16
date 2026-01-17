#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include "defs.h"
#include "tick.h"
#include "things.h"

#define VRAM_TEXTURE_START	0x0A000
#define VRAM_TEXTURE_END	0x1C000

#define LIGHTMAP_SIZE	256

#define MAX_SPRITES	255
#define MAX_SPRITE_FRAMES	512

#define MAX_WEAPON_FRAMES	128

#define MAX_DRAW_SPRITES	32
#define MAX_DRAW_MASKED	16
#define MAX_DRAW_PORTALS	64

#define MAX_EXTRA_STORAGE	255

#define TEXFLAG_PEG_Y	1
#define TEXFLAG_MIRROR_X	2
#define TEXFLAG_MIRROR_Y_SWAP_XY	4

#define SECTOR_LIGHT(x)	(((x)->flags >> 4) & 7)

//#define SHOW_OCTANT

#define WIDTH	1024
#define HEIGHT	768

#define _BV(x)	(1 << (x))

// angle range: 0 to 4095
#define ANGLE_TO_DEG(x)	((float)(x) * (360.0f / 4096.0f))
#define ANGLE_TO_RAD(x)	((float)(x) * ((M_PI*2) / 4096.0f))
// angle range: 0 to 255
#define ANG8_TO_DEG(x)	((float)(x) * (360.0f / 256.0f))
#define ANG8_TO_RAD(x)	((float)(x) * ((M_PI*2) / 256.0f))

#define H_FOV	0x0200 // 90; don't change

enum
{
	MKEY_FORWARD,
	MKEY_BACKWARD,
	MKEY_STRAFE_LEFT,
	MKEY_STRAFE_RIGHT,
	//
	MKEY_ROTATE_LEFT,
	MKEY_ROTATE_RIGHT,
	//
	MKEY_GO_UP,
	MKEY_GO_DOWN,
	//
	MKEY_USE,
	//
	NUM_KEYS
};

typedef union
{
	uint8_t raw[256];
	struct
	{
		uint8_t idiv_h[256];	// @ 0x1100
		uint8_t ydepth_h[256];	// @ 0x1200
		uint8_t x2a_l[256];	// @ 0x1300
		uint8_t x2a_h[256];	// @ 0x1400
		uint8_t swap[256];	// @ 0x1500
		uint8_t bank[256];	// @ 0x1600
		uint8_t pvxjmp_l[128];	// @ 0x1700
		uint8_t pvxjmp_h[128];	// @ 0x1780
		uint8_t phxjmp_l[84];	// @ 0x1800
		uint8_t plxjmp_l[88];	// @ 0x1854
		uint8_t phxjmp_h[84];	// @ 0x18AC
		uint8_t plxjmp_h[88];	// @ 0x1900
		uint8_t xoffs_h[168];	// @ 0x1958
		uint8_t psxjmp_l[256];	// @ 0x1A00
		uint8_t psxjmp_h[256];	// @ 0x1B00
		uint8_t ptxjmp_l[128];	// @ 0x1C00
		uint8_t ptxjmp_h[128];	// @ 0x1C80
		uint8_t yoffs_l[128];	// @ 0x1D00
		uint8_t yoffs_h[128];	// @ 0x1D80
		uint8_t htan_l[128];	// @ 0x1E00
		uint8_t htan_h[128];	// @ 0x1E80
		uint8_t sign[256];	// @ 0x1F00
		uint8_t sin_l[320];	// @ 0x2000
		uint8_t sin_h[320];	// @ 0x2140
		uint8_t pxloop[0x1553];	// @ 0x2280
		// 0x37D3
	};
} tables_1100_t;

typedef union
{
	uint8_t raw[256];
	struct
	{
		uint8_t idiv_l[8192];	// @ 0xA000 (bank 59)
		uint8_t tscale_l[4096];	// @ 0xA000 (bank 60) [texture scale]
		uint8_t tscale_h[4096];	// @ 0xB000 (bank 60) [texture scale]
		uint8_t tan_l[2048];	// @ 0xA000 (bank 61)
		uint8_t tan_h[2048];	// @ 0xA800 (bank 61)
		uint8_t ydepth_l[4096];	// @ 0xB000 (bank 61)
		uint8_t atan_l[4096];	// @ 0xA000 (bank 62)
		uint8_t atan_h[4096];	// @ 0xB000 (bank 62)
		uint8_t a2x_l[2048];	// @ 0xA000 (bank 63)
		uint8_t a2x_h[2048];	// @ 0xA800 (bank 63)
		uint8_t random[2048];	// @ 0xB000 (bank 63) [rng stuff]
		uint8_t rng_mask[256];	// @ 0xB800 (bank 63) [rng stuff]
		uint8_t planex_l[256];	// @ 0xB900 (bank 63) [plane texture stuff]
		uint8_t planex_h[256];	// @ 0xBA00 (bank 63) [plane texture stuff]
		uint8_t pitch2yc[256];	// @ 0xBB00 (bank 63)
		uint8_t vidoffs_x[128];	// @ 0xBC00 (bank 63)
		uint8_t vidoffs_y[128];	// @ 0xBC80 (bank 63)
		uint8_t printint[256];	// @ 0xBD00 (bank 63)
		// 0xBEC0 contains portals
	};
} tables_A000_t;

typedef struct
{
	uint8_t sector;
	uint8_t x0, x1;
	uint8_t first_sprite;
	uint8_t masked;
} portal_t;

typedef struct
{
	uint8_t clip_top[80];
	uint8_t clip_bot[80];
	uint8_t *data;
	uint16_t *cols;
	uint8_t x0, x1;
	uint8_t y0, y1;
	uint8_t light;
	uint8_t next;
	int16_t bot;
	int16_t depth, tex_scale;
	int16_t tex_now, tex_step;
	int16_t dist, scale;
} proj_spr_t;

typedef struct
{
	int16_t tmap_scale[80];
	uint8_t clip_top[80];
	uint8_t tmap_coord[160];
	uint8_t clip_bot[80];
	int32_t scale_now, scale_step;
	int16_t bz;
	uint8_t texture;
	uint8_t x0, x1;
	uint8_t xor, ox;
	uint8_t sflags;
} proj_msk_t;

typedef struct
{
	uint32_t nhash;
	uint32_t vhash;
	uint8_t type;
	uint8_t effect[4];
	union
	{
		struct
		{
			uint8_t vera_tile_base;
			uint8_t vera_map_base;
			uint32_t colormap[MAX_LIGHTS];
		} plane;
		struct
		{
			uint8_t math_and;
			uint32_t offs_cols;
			uint32_t offs_data[MAX_LIGHTS];
		} wall;
	};
} texture_info_t;

typedef struct
{
	uint16_t addr;
	int16_t x, y;
	uint8_t ia, ib;
} x16_sprite_t;

typedef struct
{
	uint16_t start;
	uint8_t nsz;
	uint8_t bsz;
} weapon_info_t;

typedef struct
{
	uint8_t addr;
	uint8_t x, y;
	uint8_t info;
} weapon_part_t;

typedef struct
{
	weapon_info_t info;
	weapon_part_t part[15];
} weapon_frame_t;

typedef struct
{
	uint8_t width;
	uint8_t height;
	int8_t ox, oy;
	uint32_t offs_cols;
	uint32_t offs_data;
} sprite_frame_t;

typedef struct
{
	uint16_t rot[8];
	uint8_t rotate;
} sprite_info_t;

typedef struct
{
	uint32_t hash;
	int16_t x, y, z;
	uint8_t sector;
	uint8_t angle;
	uint8_t flags;
	uint8_t extra;
} __attribute__((packed)) map_thing_t;

typedef struct
{
	uint32_t hash;
	uint8_t lights;
} __attribute__((packed)) marked_texture_t;

typedef struct
{
	uint32_t nhash;
	uint32_t vhash;
} marked_variant_t;

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
			export_plane_variant_t variant[MAX_X16_VARIANTS];
		} bpp4;
		struct
		{
			uint8_t data[64 * 64];
			uint8_t effect[4];
		} bpp8;
	};
} export_plane_t;

typedef struct
{
	uint16_t offs;
	uint8_t light;
	uint8_t idx;
	int32_t avg;
} show_wpn_t;

//

static SDL_Window *sdl_win;
static SDL_GLContext sdl_context;

uint32_t frame_counter;
static uint32_t render_flags;

static uint32_t input_action;

uint32_t level_tick;
static uint8_t anim_tick[10];

vertex_t display_point;
vertex_t display_line[2];
vertex_t display_wall[2];

static uint32_t texture[2];
static uint8_t framebuffer[160 * 120];
static uint8_t sprbuffer[160 * 120];

static uint8_t *tex_data;
static uint8_t *tex_map;
static uint32_t tex_res_mask;
static uint32_t tex_map_stride;
static uint8_t tex_x_start, tex_y_start;

static int32_t tex_offs_x;
static int32_t tex_offs_y;
static int32_t tex_step_x;
static int32_t tex_step_y;
static uint8_t tex_type;
static uint8_t tex_swap;

static uint8_t palette_src[256 * 3 * 16];
static uint8_t *palette;

static uint8_t clip_top[80];
static uint8_t clip_bot[80];
static int16_t tmap_scale[80];
static int8_t tmap_coord[160];
static uint8_t planex0[120];

static uint8_t plf_top[80];
static uint8_t plf_bot[80];
static uint8_t plc_top[80];
static uint8_t plc_bot[80];

static uint32_t stopped;
static uint32_t keybits;

static float gl_fov_x, gl_fov_y;

static show_wpn_t show_wpn_now;
static uint8_t show_wpn_slot = 0x8E;

p2a_t p2a_coord;

projection_t projection;

static proj_spr_t proj_spr[MAX_DRAW_SPRITES];
static uint32_t proj_spr_idx;
static uint8_t proj_spr_first;

static uint8_t sprite_remap_pc[sizeof(sprite_remap)];

// masked
static proj_msk_t proj_msk[MAX_DRAW_MASKED];
static uint32_t proj_msk_idx;

uint8_t sectorth[256][32];

player_start_t player_starts[MAX_PLAYER_STARTS];

//// tables

static const uint8_t move_angle[16] =
{
	0x00, // -
	0x80, // MKEY_FORWARD
	0xC0, // MKEY_BACKWARD
	0x00, // MKEY_BACKWARD | MKEY_FORWARD

	0xE0, // MKEY_STRAFE_LEFT
	0xF0, // MKEY_STRAFE_LEFT | MKEY_FORWARD
	0xD0, // MKEY_STRAFE_LEFT | MKEY_BACKWARD
	0xE0, // MKEY_STRAFE_LEFT | MKEY_BACKWARD | MKEY_FORWARD

	0xA0, // MKEY_STRAFE_RIGHT
	0x90, // MKEY_STRAFE_RIGHT | MKEY_FORWARD
	0xB0, // MKEY_STRAFE_RIGHT | MKEY_BACKWARD
	0xA0, // MKEY_STRAFE_RIGHT | MKEY_BACKWARD | MKEY_FORWARD

	0x00, // MKEY_STRAFE_RIGHT | MKEY_STRAFE_LEFT
	0x80, // MKEY_STRAFE_RIGHT | MKEY_STRAFE_LEFT | MKEY_FORWARD
	0xC0, // MKEY_STRAFE_RIGHT | MKEY_STRAFE_LEFT | MKEY_BACKWARD
	0x00, // MKEY_STRAFE_RIGHT | MKEY_STRAFE_LEFT | MKEY_BACKWARD | MKEY_FORWARD
};

// random
static uint32_t rng_idx;
static uint8_t rng_tab[2048];
static uint8_t rng_mask[256];

// sin / cos
int16_t tab_sin[256];
int16_t tab_cos[256];

// inverse division
static int16_t inv_div_raw[65536];
int16_t *const inv_div = inv_div_raw + 32768;

// texture Z scale
static int16_t tex_scale[4096];

// Y depth projection
static int16_t tab_depth[4096];

// arc tan
static uint16_t atan_tab[4096];

// tan
static int16_t tab_tan[2048];
int16_t tab_tan_hs[128];

// angle to X and reverse
static int16_t angle2x[2048];
static uint16_t x2angle[160];

// plane scale
static int16_t tab_planex[256];

// pitch to Y center
static uint16_t pitch2yc[256];

// portals
static portal_t portals[MAX_DRAW_PORTALS];
static portal_t *portal_wr;
static portal_t *portal_rd;
static portal_t *portal_last;
static uint8_t portal_top, portal_bot;

// map buffer
static uint8_t map_block_data[8192];
map_head_t map_head;
sector_t map_sectors[256];
wall_t map_walls[WALL_BANK_COUNT][256];

// font stuff
static uint8_t font_info[512];

// texture stuff
static uint8_t vram[128 * 1024];
static uint8_t vram_4bpp;
static uint8_t vram_8bpp;

static uint8_t lightmaps[MAX_LIGHTS * 256];
static uint8_t *lightmap;

static uint8_t *colormap;

static uint32_t wram_used;
static uint32_t wram_used_pc;
uint8_t wram[0x200000];

static uint32_t sky_base;

static uint32_t num_textures;
static uint32_t texture_remap[MAX_TEXTURES];
static texture_info_t map_texture_info[MAX_TEXTURES + 1];

static uint32_t num_sprites;
static uint32_t num_sprites_pc;
static sprite_info_t sprite_info[MAX_SPRITES];

static uint32_t num_sframes;
static uint32_t num_sframes_pc;
static sprite_frame_t sprite_frame[MAX_SPRITE_FRAMES];

static uint32_t num_wframes;
static uint32_t num_wframes_pc;
static weapon_frame_t weapon_frame[MAX_WEAPON_FRAMES];

//
static const uint32_t mkey_sdl[] =
{
	[MKEY_ROTATE_LEFT] = SDLK_LEFT,
	[MKEY_ROTATE_RIGHT] = SDLK_RIGHT,
	[MKEY_FORWARD] = 'w',
	[MKEY_BACKWARD] = 's',
	[MKEY_STRAFE_LEFT] = 'a',
	[MKEY_STRAFE_RIGHT] = 'd',
	[MKEY_GO_UP] = ' ',
	[MKEY_GO_DOWN] = SDLK_LCTRL,
	[MKEY_USE] = 'e',
};

//
// debug

static void hexdump(uint8_t *src)
{
	for(uint32_t i = 0; i < 256; i++)
	{
		if((i & 15) == 0)
			printf("\r");

		if((i & 15) == 15)
			printf("0x%02X\n", *src);
		else
			printf("0x%02X, ", *src);

		src++;
	}
}

//
// textures

static void vera_tex_data(uint32_t tile_base, uint32_t map_base)
{
	uint32_t offset;

	// MAP

	switch(map_base & 3)
	{
		case 0:
			tex_map_stride = 2;
			tex_res_mask = 15;
		break;
		case 1:
			tex_map_stride = 8;
			tex_res_mask = 63;
		break;
		case 2:
			tex_map_stride = 32;
			tex_res_mask = 255;
		break;
		case 3:
			tex_map_stride = 128;
			tex_res_mask = 1023;
		break;
	}

	map_base &= 0xFC;
	offset = (uint32_t)map_base << 9;

	tex_map = vram + offset;

	// TILES

	tile_base &= 0xFC;
	offset = (uint32_t)tile_base << 9;

	tex_data = vram + offset;
}

static texture_info_t *tex_set(uint8_t idx, uint8_t ox, uint8_t oy, uint8_t light, uint8_t flags)
{
	uint32_t offset;
	texture_info_t *mt;
	uint8_t vera_tile_base;
	uint8_t vera_map_base;

	tex_swap = 0;

	if(idx & 0x80)
	{
		if(idx & 0x40)
		{
			// sky
			tex_type = 0x00;
			return NULL;
		}
		// invalid texture
		mt = map_texture_info + MAX_TEXTURES;
	} else
	{
		idx = texture_remap[idx];
		mt = map_texture_info + idx;
	}

	// animation
	if(mt->effect[0] & 4)
	{
		uint32_t etime = etime = anim_tick[mt->effect[1]];
		etime += mt->effect[3];
		idx -= mt->effect[3];
		idx += etime & mt->effect[2]; // TODO: overflow check
		mt = map_texture_info + idx;
	}

	// type
	if(mt->type & 0x80)
	{
		// plane
		tex_type = 0x01;
		tex_swap = flags & TEXFLAG_MIRROR_Y_SWAP_XY;

		vera_tile_base = mt->plane.vera_tile_base;
		vera_map_base = mt->plane.vera_map_base;
		if(mt->type & 1)
			colormap = lightmaps + light * LIGHTMAP_SIZE;
		else
			colormap = wram + mt->plane.colormap[light];
		projection.ta = 255;
	} else
	{
		// wall / sprite
		tex_type = 0xFF;
		vera_tile_base = 0xF8;
		vera_map_base = 0xFE;
		projection.wx = mt->type;
		projection.cols = (uint16_t*)(wram + mt->wall.offs_cols);
		projection.wtex = wram + mt->wall.offs_data[light];
		projection.ta = 127;
		if(flags & TEXFLAG_MIRROR_Y_SWAP_XY)
			projection.wx++;
	}

	// offset

	projection.ox = ox;
	projection.oy = oy - projection.fix;

	// math

	projection.tx = flags & TEXFLAG_MIRROR_X ? 0x00 : 0xFF;

	// VERA

	vera_tex_data(vera_tile_base, vera_map_base);

	//
	return mt;
}

static uint8_t tex_read()
{
	uint32_t tx, ty;
	uint32_t mx, my;
	uint32_t tile;
	uint8_t *src;

	tx = tex_offs_x >> 9;
	ty = tex_offs_y >> 9;

	tx += tex_x_start;
	ty += tex_y_start;

	tx &= tex_res_mask;
	ty &= tex_res_mask;

	tex_offs_x += tex_step_x;
	tex_offs_y += tex_step_y;

	mx = tx / 8;
	my = ty / 8;

	tx &= 7;
	ty &= 7;

	tile = tex_map[mx + my * tex_map_stride];
	src = tex_data + tile * 8 * 8;

	return colormap[src[tx + ty * 8]];
}

//
// math

static void sim24bit(int32_t *in)
{
#if 1
	int32_t val = *in & 0x007FFFFF;

	if(*in < 0)
		val |= 0xFF800000;

	*in = val;
#endif
}

static uint16_t atanbin(float y, float x, float scale)
{
	uint16_t angle;
	angle = (M_PI + atan2f(y, x)) * (scale / (M_PI*2));
	return angle;
}

uint16_t point_to_angle()
{
	uint16_t ret;

	if(p2a_coord.y < 0)
	{
		p2a_coord.y = -p2a_coord.y;
		if(p2a_coord.x < 0)
		{
			p2a_coord.x = -p2a_coord.x;
			if(p2a_coord.x > p2a_coord.y)
			{
				int32_t res;
				res = p2a_coord.y * inv_div[p2a_coord.x];
				res >>= 3;
				ret = 3072 - atan_tab[res];
#ifdef SHOW_OCTANT
printf("O: 5; 0x%03X\n", atan_tab[res]);
#endif
			} else
			{
				int32_t res;
				res = p2a_coord.x * inv_div[p2a_coord.y];
				res >>= 3;
				ret = 2048 + atan_tab[res];
#ifdef SHOW_OCTANT
printf("O: 4; 0x%03X\n", atan_tab[res]);
#endif
			}
		} else
		{
			if(p2a_coord.x > p2a_coord.y)
			{
				int32_t res;
				res = p2a_coord.y * inv_div[p2a_coord.x];
				res >>= 3;
				ret = atan_tab[res] + 1024;
#ifdef SHOW_OCTANT
printf("O: 1; 0x%03X\n", atan_tab[res]);
#endif
			} else
			{
				int32_t res;
				res = p2a_coord.x * inv_div[p2a_coord.y];
				res >>= 3;
				ret = 2048 - atan_tab[res];
#ifdef SHOW_OCTANT
printf("O: 2; 0x%03X\n", atan_tab[res]);
#endif
			}
		}
	} else
	{
		if(p2a_coord.x < 0)
		{
			p2a_coord.x = -p2a_coord.x;
			if(p2a_coord.x > p2a_coord.y)
			{
				int32_t res;
				res = p2a_coord.y * inv_div[p2a_coord.x];
				res >>= 3;
				ret = atan_tab[res] + 3072;
#ifdef SHOW_OCTANT
printf("O: 6; 0x%03X\n", atan_tab[res]);
#endif
			} else
			{
				int32_t res;
				res = p2a_coord.x * inv_div[p2a_coord.y];
				res >>= 3;
				ret = 0 - atan_tab[res];
#ifdef SHOW_OCTANT
printf("O: 7; 0x%03X\n", atan_tab[res]);
#endif
			}
		} else
		{
			if(p2a_coord.x > p2a_coord.y)
			{
				int32_t res;
				res = p2a_coord.y * inv_div[p2a_coord.x];
				res >>= 3;
				ret = 1024 - atan_tab[res];
#ifdef SHOW_OCTANT
printf("O: 1; 0x%03X\n", atan_tab[res]);
#endif
			} else
			{
				int32_t res;
				res = p2a_coord.x * inv_div[p2a_coord.y];
				res >>= 3;
				ret = atan_tab[res];
#ifdef SHOW_OCTANT
printf("O: 0; 0x%03X\n", atan_tab[res]);
#endif
			}
		}
	}

	return ret & 4095;
}

uint16_t point_to_dist()
{
	vertex_t vtx;
	uint8_t ang;

	vtx.x = p2a_coord.x;
	vtx.y = p2a_coord.y;

	ang = point_to_angle() >> 4;
	p2a_coord.a = ang;

	return (vtx.x * tab_sin[ang] + vtx.y * tab_cos[ang]) >> 8;
}

int32_t spec_inv_div(int16_t n, int16_t d)
{
	int32_t ret;
	uint32_t count = 0;

	while(d >= 128)
	{
		d >>= 1;
		count++;
	}

	ret = n * inv_div[d];

	while(count)
	{
		ret >>= 1;
		count--;
	}

	return ret;
}

//
// texture

static uint32_t color_lookup(uint32_t col)
{
	uint32_t color = 0xFF000000;
	uint8_t *pal = palette + col * 3;
	color |= *pal++ << 16;
	color |= *pal++ << 8;
	color |= *pal++;
	return color;
}

static void update_texture(float x0, float y0, float x1, float y1)
{
	uint32_t data[160 * 120];
	uint32_t *dst = data;
	uint8_t *src = framebuffer;

	for(uint32_t i = 0; i < 160 * 120; i++)
		*dst++ = color_lookup(*src++);

	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 160, 120, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(x0, y0, 0);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(x1, y0, 0);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(x0, y1, 0);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(x1, y1, 0);
	glEnd();
}

//
// X16 sprites

static void update_sprites(float x0, float y0, float x1, float y1)
{
	uint32_t data[256 * 384];
	x16_sprite_t *spr = (x16_sprite_t*)&vram[sizeof(vram)];
	const float s0 = 48.0f / 256.0f;
	const float s1 = 208.0f / 256.0f;
	const float t0 = 68.0f / 384.0f;
	const float t1 = 188.0f / 384.0f;

	memset(data, 0, sizeof(data));

	for(uint32_t i = 0; i < 128; i++)
	{
		uint32_t width;
		uint32_t height;
		uint8_t *src;

		spr--;

		if(!(spr->ia & 0b1100))
			continue;

		width = 8 << ((spr->ib >> 4) & 3);
		height = 8 << (spr->ib >> 6);

		src = vram + ((spr->addr * 32) & 0x1FFFF);

		if(spr->ia & 1)
		{
			for(uint32_t y = 0; y < height; y++)
			{
				uint32_t *dst = data + (spr->y + y + 68) * 256 + 48 + spr->x;
				dst += width;

				for(uint32_t x = 0; x < width; x++)
				{
					uint8_t in = *src++;
					dst--;
					if(in)
						*dst = color_lookup(in);
				}
				dst += width * 2;
			}
		} else
		{
			for(uint32_t y = 0; y < height; y++)
			{
				uint32_t *dst = data + (spr->y + y + 68) * 256 + 48 + spr->x;

				for(uint32_t x = 0; x < width; x++)
				{
					uint8_t in = *src++;
					if(in)
						*dst = color_lookup(in);
					dst++;
				}
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, texture[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 384, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(s0, t1);
		glVertex3f(x0, y0, 0);
		glTexCoord2f(s1, t1);
		glVertex3f(x1, y0, 0);
		glTexCoord2f(s0, t0);
		glVertex3f(x0, y1, 0);
		glTexCoord2f(s1, t0);
		glVertex3f(x1, y1, 0);
	glEnd();
}

//
// draw

static void draw_rect(float x0, float y0, float x1, float y1)
{
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(x0, y0, 0);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(x1, y0, 0);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(x0, y1, 0);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(x1, y1, 0);
	glEnd();
}

static void draw_line(float x0, float y0, float x1, float y1)
{
	glBegin(GL_LINES);
		glVertex3f(x0, y0, 0);
		glVertex3f(x1, y1, 0);
	glEnd();
}
/*
static void draw_wall(wall_t *wall)
{
	float cx, cy;
	float nx, ny;
	float len;
	vertex_t *v0, *v1;

	v0 = &wall->vtx;
	v1 = &wall[1].vtx;

	cx = v1->x - v0->x;
	cy = v1->y - v0->y;

	len = sqrtf(cx * cx + cy * cy) * 0.1f;

	nx = cy / len;
	ny = -cx / len;

	cx = cx * 0.5f + v0->x;
	cy = cy * 0.5f + v0->y;

	if(wall->backsector)
		glColor3f(0.0f, 0.5f, 0.0f);
	else
		glColor3f(0.4f, 0.4f, 0.4f);
	draw_line(cx, cy, cx + nx, cy + ny);

	if(wall->backsector)
		glColor3f(0.0f, 1.0f, 0.0f);
	else
		glColor3f(1.0f, 1.0f, 1.0f);
	draw_line(v0->x, v0->y, v1->x, v1->y);
}
*/
static void draw_aline(uint16_t angle, uint8_t type)
{
	float aa = ANGLE_TO_RAD(angle);
	float x, y, xe, ye;

	switch(type)
	{
		case 0:
			glColor3f(0.2f, 0.2f, 0.2f);
		break;
		case 1:
			glColor3f(0.3f, 0.3f, 0.0f);
		break;
		default:
			glColor3f(0.0f, 0.0f, 0.4f);
		break;
	}

	x = projection.x >> 8;
	y = projection.y >> 8;

	xe = sinf(aa) * 4*1024.0f;
	ye = cosf(aa) * 4*1024.0f;

	glBegin(GL_LINE_STRIP);
		glVertex3f(x, y, 0);
		glVertex3f(x + xe, y + ye, 0);
	glEnd();
}

static void draw_fov()
{
	// FOV
	glColor3f(0.3f, 0.15f, 0.0f);
	glBegin(GL_LINE_STRIP);
		glVertex3f(gl_fov_x, gl_fov_y, 0);
		glVertex3f(0, 0, 0);
		glVertex3f(-gl_fov_x, gl_fov_y, 0);
	glEnd();
}

static void draw_camera()
{
	glColor3f(1.0f, 1.0f, 0.0f);
	glBegin(GL_LINE_STRIP);
		glVertex3f(-8, 0, 0);
		glVertex3f(0, 8, 0);
		glVertex3f(0, -8, 0);
		glVertex3f(0, 8, 0);
		glVertex3f(8, 0, 0);
	glEnd();
}

//
// draw to texture

static void dr_sline(uint8_t x, uint8_t y0, uint8_t y1)
{
	uint8_t *dst;
	uint8_t *src;
	int32_t ys;
	int16_t xx;

	y0--;

	xx = ((x2angle[x] - projection.a) >> 3) & 0x1FF;

	ys = projection.ycw - y0;

	src = wram + sky_base * 256 + (xx & 0xFF) * 256 + ((xx & 0x100) >> 1);

	dst = framebuffer + x;
	dst += y0 * 160;

	for( ; ys >= 0; y0++, ys--)
	{
		if(y0 >= y1)
			return;
		*dst = src[ys];
		dst += 160;
	}

	ys = abs(ys);

	for( ; y0 < y1; y0++, ys++)
	{
		*dst = src[ys];
		dst += 160;
	}
}

static void dr_sky(uint8_t *top, uint8_t *bot)
{
	uint8_t x0 = projection.x0;
	uint8_t x1 = projection.x1;

	for( ; x0 < x1; x0++)
	{
		if(bot[x0] < top[x0])
			continue;

		dr_sline(x0 * 2 + 0, top[x0], bot[x0]);
		dr_sline(x0 * 2 + 1, top[x0], bot[x0]);
	}
}

static void dr_hline(int16_t height, uint8_t y, uint8_t x0, uint8_t x1)
{
	uint8_t *dst;
	int32_t xnow, xstep;
	int32_t ynow, ystep;
	vertex_t now, part;
	int16_t step;
	int32_t sx;

	if(render_flags & 2)
		return;

	y--;
	x0 *= 2;
	x1 *= 2;

	if(y >= 120)
	{
		printf("dr_hline y %d\n", y);
//		*(uint32_t*)1 = 1;
		return;
	}

	sx = (int32_t)x0 - 80;

	step = (height * tab_planex[y + projection.ycp]) >> 7;

	xstep = (step * projection.pl_cos) >> 7;
	ystep = (step * projection.pl_sin) >> 7;

	xnow = xstep * sx + 80 * ystep;
	ynow = ystep * sx - 80 * xstep;

	xnow += projection.pl_x;
	ynow -= projection.pl_y;

	dst = framebuffer + x0;
	dst += y * 160;

	tex_offs_y = xnow;
	tex_offs_x = ynow;
	tex_step_y = xstep;
	tex_step_x = ystep;

	for( ; x0 < x1; x0++)
	{
		uint8_t col = tex_read();
		if(col)
			*dst = col;
		dst++;
	}
}

static void dr_plane(int16_t height, uint8_t *top, uint8_t *bot)
{
	uint8_t x0 = projection.x0;
	uint8_t x1 = projection.x1;
	uint8_t xx = x0 - 1;

	top[xx] = 0xFF;
	top[x1] = 0xFF;

	tex_y_start = projection.ox;
	tex_x_start = projection.oy;

	for( ; x0 <= x1; x0++, xx++)
	{
		uint8_t t0 = top[xx];
		uint8_t b0 = bot[xx];
		uint8_t t1 = top[x0];
		uint8_t b1 = bot[x0];

		while(t0 < t1 && t0 <= b0)
		{
			dr_hline(height, t0, planex0[t0], x0);
			t0++;
		}

		while(b0 > b1 && b0 >= t0)
		{
			dr_hline(height, b0, planex0[b0], x0);
			b0--;
		}

		while(t1 < t0 && t1 <= b1)
		{
			planex0[t1] = x0;
			t1++;
		}

		while(b1 > b0 && b1 >= t1)
		{
			planex0[b1] = x0;
			b1--;
		}
	}
}

static void dr_vline(uint32_t x, int32_t y0, int32_t y1, int32_t tx, int32_t tnow, int32_t step)
{
	uint8_t *dst;

	if(render_flags & 1)
		return;

	if(!tex_type)
	{
		dr_sline(x, y0 + 1, y1);
		return;
	}

	if(tex_type & 0x80)
	{
		// wall / sprite
		tex_offs_y = projection.wx << 9;
		colormap = projection.wtex + projection.cols[tx];
	} else
		// plane
		tex_offs_y = tx << 9;

	if(tex_swap)
	{
		tex_offs_x = tex_offs_y;
		tex_offs_y = tnow << 1;
		tex_step_x = 0;
		tex_step_y = step << 1;

		tex_y_start = projection.oy;
		tex_x_start = 0;
	} else
	{
		tex_offs_x = tnow << 1;
		tex_step_y = 0;
		tex_step_x = step << 1;

		tex_y_start = 0;
		tex_x_start = projection.oy;
	}

	dst = framebuffer + x;
	dst += y1 * 160;

	for( ; y1 > y0; y1--)
	{
		uint8_t col = tex_read();
		dst -= 160;
		if(col)
			*dst = col;
	}
}

static void dr_vspr(uint32_t x, int32_t y0, int32_t y1, int32_t tx, int32_t tnow, int32_t step)
{
	uint8_t *dst;

	if(render_flags & 4)
		return;

	tex_y_start = 0;
	tex_x_start = 0;

	tex_offs_y = projection.wx << 9;

	tex_offs_x = tnow << 1;
	tex_step_y = 0;
	tex_step_x = step << 1;

	dst = framebuffer + x;
	dst += y1 * 160;

	for( ; y1 > y0; y1--)
	{
		uint8_t col = lightmap[tex_read()];
		dst -= 160;
		if(col)
			*dst = col;
	}
}

static void dr_textured_strip(uint8_t x0, uint8_t x1, int32_t top_now, int32_t top_step, int32_t bot_now, int32_t bot_step, uint32_t flags)
{
	uint8_t xx = x0 * 2;

	for( ; x0 < x1; x0++)
	{
		int32_t y0, y1;
		int32_t tnow;
		int16_t clipdiff;

		if(clip_top[x0] & 0x80)
		{
			xx += 2;
			goto skip;
		}

		sim24bit(&top_now);
		sim24bit(&bot_now);

		y0 = top_now >> 8;
		y1 = bot_now >> 8;

		if(y0 >= clip_bot[x0])
		{
			y0 = clip_bot[x0];
			y1 = 120;
			portal_bot = 0;
			xx += 2;
			goto go_next;
		}

		if(y1 <= clip_top[x0])
		{
			y1 = clip_top[x0];
			y0 = 0;
			portal_top = 0;
			xx += 2;
			goto go_next;
		}

		if(y0 < clip_top[x0])
			y0 = clip_top[x0];
		else
			portal_bot = 0;

		tnow = -1;
		clipdiff = clip_bot[x0] - y1;

		if(clipdiff < 0)
		{
			tnow = -clipdiff * (int32_t)tmap_scale[x0];
			tnow >>= 2;
			y1 = clip_bot[x0];
		} else
			portal_top = 0;

		dr_vline(xx, y0, y1, ((tmap_coord[xx] - projection.ox) ^ projection.tx) & projection.ta, tnow, tmap_scale[x0] >> 2);
		xx++;
		dr_vline(xx, y0, y1, ((tmap_coord[xx] - projection.ox) ^ projection.tx) & projection.ta, tnow, tmap_scale[x0] >> 2);
		xx++;

go_next:
		if(flags & 1)
		{
			plc_top[x0] = clip_top[x0] + 1;
			plc_bot[x0] = y0;
			clip_top[x0] = y1;
		}

		if(flags & 2)
		{
			plf_top[x0] = y1 + 1;
			plf_bot[x0] = clip_bot[x0];
			clip_bot[x0] = y0;
		}

		if(flags & 4)
			clip_top[x0] = 0xF0;

skip:
		top_now += top_step;
		bot_now += bot_step;
	}
}

static void dr_mark_top(uint8_t x0, uint8_t x1, int32_t now, int32_t step)
{
	for( ; x0 < x1; x0++)
	{
		int32_t y0;

		if(clip_top[x0] & 0x80)
			goto skip;

		sim24bit(&now);

		y0 = now >> 8;

		if(y0 >= clip_bot[x0])
		{
			y0 = clip_bot[x0];
			goto go_next;
		}

		portal_top = 0;

		if(y0 < clip_top[x0])
			y0 = clip_top[x0];

go_next:
		plc_top[x0] = clip_top[x0] + 1;
		plc_bot[x0] = y0;

		clip_top[x0] = y0;

skip:
		now += step;
	}
}

static void dr_mark_bot(uint8_t x0, uint8_t x1, int32_t now, int32_t step)
{
	for( ; x0 < x1; x0++)
	{
		int32_t y1;

		if(clip_top[x0] & 0x80)
			goto skip;

		sim24bit(&now);

		y1 = now >> 8;

		if(y1 <= clip_top[x0])
		{
			y1 = clip_top[x0];
			goto go_next;
		}

		portal_bot = 0;

		if(y1 > clip_bot[x0])
			y1 = clip_bot[x0];

go_next:
		plf_top[x0] = y1 + 1;
		plf_bot[x0] = clip_bot[x0];

		clip_bot[x0] = y1;

skip:
		now += step;
	}
}

static void dr_wall(sector_t *sec, wall_t *wall, vertex_t *ld, uint32_t x0, uint32_t x1, int16_t s0, int16_t s1)
{
	int32_t scale_now, top_now, bot_now;
	int32_t scale_step, top_step, bot_step;
	int16_t ydiff, xdiff, zdiff;
	uint16_t lca;
	uint8_t oldx;
	uint8_t xx = x0;
	uint8_t masked_idx = 0xFF;

	lca = wall->angle - projection.a;

	xdiff = x1 - x0;
	ydiff = s1 - s0;

	if(x1 > projection.x1)
		x1 = projection.x1;

	// scale
	scale_step = ((int32_t)ydiff * (int32_t)inv_div[xdiff]) >> 7;
	scale_now = s0 << 8;

	sim24bit(&scale_step);

	if(x0 < projection.x0)
	{
		int16_t diff = projection.x0 - x0;
		scale_now += scale_step * diff;
		x0 = projection.x0;
		xx = projection.x0;
	}

	// texture mapping
	top_step = scale_step;
	top_now = scale_now;

	for( ; x0 < x1; x0++)
	{
		if(!(clip_top[x0] & 0x80))
			tmap_scale[x0] = tex_scale[top_now >> 8];
		top_now += top_step;
	}

	for(x0 = xx * 2; x0 < x1 * 2; x0++)
	{
		uint16_t ang;
		uint8_t xoffs;

		ang = (lca + x2angle[x0]) & 2047;
		// xoffs = (-1 * ld->x + ld->y * tab_tan[ang]) >> 8;
		xoffs = ((ld->y * tab_tan[ang]) >> 8) - (ld->x >> 8);

		tmap_coord[x0] = xoffs;
	}

	projection.fix = 0;

	if(	proj_msk_idx < MAX_DRAW_MASKED &&
		wall->angle & WALL_MARK_EXTENDED &&
		!(wall->mid.texture & 0x80) &&
		!(texture_remap[wall->mid.texture] & 0x80)
	){
		proj_msk_t *mt = proj_msk + proj_msk_idx;
		sector_t *ss = sec;

		mt->bz = wall->mid.oy * 2;

		if(	wall->backsector &&
			wall->tflags & 0b10000000
		)
			ss = map_sectors + wall->backsector;

		mt->scale_now = scale_now;
		mt->scale_step = scale_step;
		mt->x0 = xx;
		mt->x1 = x1;
		mt->texture = wall->mid.texture;
		mt->bz += ss->floor.height;
		mt->xor = wall->tflags & 0b00001000 ? 0x00 : 0xFF;
		mt->ox = wall->mid.ox;
		mt->sflags = sec->flags;

		memcpy(mt->tmap_scale, tmap_scale, sizeof(tmap_scale));
		memcpy(mt->tmap_coord, tmap_coord, sizeof(tmap_coord));

		masked_idx = proj_msk_idx;

		proj_msk_idx++;
	}

	if(	wall->backsector ||
		(
			wall->angle & WALL_MARK_EXTENDED &&
			wall->split != -32768
		)
	){
		sector_t *bs;
		int16_t diff;

		if(!wall->backsector)
		{
			static sector_t fake_bs;

			fake_bs.floor.height = wall->split;
			fake_bs.ceiling.height = wall->split;

			bs = &fake_bs;
		} else
			bs = map_sectors + wall->backsector;

		diff = bs->floor.height - sec->ceiling.height;
		if(diff >= 0)
		{
			uint32_t tflags = wall->tflags >> 4;
			if(tflags & TEXFLAG_PEG_Y)
				projection.fix = (int16_t)(sec->floor.height - bs->floor.height) >> 1;
			tex_set(wall->bot.texture, wall->bot.ox, wall->bot.oy, SECTOR_LIGHT(sec), tflags);
			goto do_solid_bot;
		}

		diff = sec->floor.height - bs->ceiling.height;
		if(diff >= 0)
		{
			projection.fix = diff >> 1;
			if(wall->tflags & TEXFLAG_PEG_Y)
				projection.fix += (int16_t)(bs->ceiling.height - sec->ceiling.height) >> 1;
			goto do_solid_top;
		}

		portal_top = 1;
		portal_bot = 1;

		// top
		zdiff = projection.z - sec->ceiling.height;
		top_step = (scale_step * zdiff) >> 8;
		top_now = (scale_now >> 8) * zdiff;
		top_now += projection.ycw << 8;
		sim24bit(&top_now);
		sim24bit(&top_step);

		if(bs->ceiling.height < sec->ceiling.height)
		{
			// masked clip
			if(!(masked_idx & 0x80))
				memcpy(proj_msk[masked_idx].clip_top, clip_top, sizeof(clip_top));

			// bot
			zdiff = projection.z - bs->ceiling.height;
			bot_step = (scale_step * zdiff) >> 8;
			bot_now = (scale_now >> 8) * zdiff;
			bot_now += projection.ycw << 8;
			sim24bit(&bot_now);
			sim24bit(&bot_step);

			if(wall->tflags & TEXFLAG_PEG_Y)
				projection.fix = (int16_t)(bs->ceiling.height - sec->ceiling.height) >> 1;

			// draw
			tex_set(wall->top.texture, wall->top.ox, wall->top.oy, SECTOR_LIGHT(sec), wall->tflags);
			dr_textured_strip(xx, x1, top_now, top_step, bot_now, bot_step, 1);
		} else
		{
			dr_mark_top(xx, x1, top_now, top_step);

			// masked clip
			if(!(masked_idx & 0x80))
				memcpy(proj_msk[masked_idx].clip_top, clip_top, sizeof(clip_top));
		}

		// bot
		zdiff = projection.z - sec->floor.height;
		bot_step = (scale_step * zdiff) >> 8;
		bot_now = (scale_now >> 8) * zdiff;
		bot_now += projection.ycw << 8;
		sim24bit(&bot_now);
		sim24bit(&bot_step);

		if(bs->floor.height > sec->floor.height)
		{
			uint32_t tflags = wall->tflags >> 4;

			// masked clip
			if(!(masked_idx & 0x80))
				memcpy(proj_msk[masked_idx].clip_bot, clip_bot, sizeof(clip_bot));

			// top
			zdiff = projection.z - bs->floor.height;
			top_step = (scale_step * zdiff) >> 8;
			top_now = (scale_now >> 8) * zdiff;
			top_now += projection.ycw << 8;
			sim24bit(&top_now);
			sim24bit(&top_step);

			if(tflags & TEXFLAG_PEG_Y)
				projection.fix = (int16_t)(sec->floor.height - bs->floor.height) >> 1;
			else
				projection.fix = 0;

			// draw
			tex_set(wall->bot.texture, wall->bot.ox, wall->bot.oy, SECTOR_LIGHT(sec), tflags);
			dr_textured_strip(xx, x1, top_now, top_step, bot_now, bot_step, 2);
		} else
		{
			dr_mark_bot(xx, x1, bot_now, bot_step);

			// masked clip
			if(!(masked_idx & 0x80))
				memcpy(proj_msk[masked_idx].clip_bot, clip_bot, sizeof(clip_bot));
		}

		// portal
		if(	!portal_top && !portal_bot &&
			bs->ceiling.height > bs->floor.height
		){
			if(portal_last->sector != wall->backsector)
			{
				if(portal_wr - portals < MAX_DRAW_PORTALS)
				{
					portal_wr->sector = wall->backsector;
					portal_wr->x0 = xx;
					portal_wr->x1 = x1;
					portal_wr->masked = masked_idx;

					portal_last = portal_wr;
					portal_wr++;

					return;
				}
			} else
			{
				portal_last->x1 = x1;

				if(	!(masked_idx & 0x80) &&
					portal_wr - portals < MAX_DRAW_PORTALS
				){
					*portal_wr = *portal_last;

					portal_last->sector = 0;
					portal_last->first_sprite = 0xFF;
					portal_last->masked = masked_idx;

					portal_last = portal_wr;
					portal_wr++;

					return;
				}
			}
		}
	} else
	{
		if(wall->tflags & TEXFLAG_PEG_Y)
			projection.fix = (int16_t)(sec->floor.height - sec->ceiling.height) >> 1;
do_solid_top:
		tex_set(wall->top.texture, wall->top.ox, wall->top.oy, SECTOR_LIGHT(sec), wall->tflags);
do_solid_bot:
		// masked clip
		if(!(masked_idx & 0x80))
		{
			memcpy(proj_msk[masked_idx].clip_top, clip_top, sizeof(clip_top));
			memcpy(proj_msk[masked_idx].clip_bot, clip_bot, sizeof(clip_bot));
		}

		// top
		zdiff = projection.z - sec->ceiling.height;
		top_step = (scale_step * zdiff) >> 8;
		top_now = (scale_now >> 8) * zdiff;
		top_now += projection.ycw << 8;
		sim24bit(&top_now);
		sim24bit(&top_step);

		// bot
		zdiff = projection.z - sec->floor.height;
		bot_step = (scale_step * zdiff) >> 8;
		bot_now = (scale_now >> 8) * zdiff;
		bot_now += projection.ycw << 8;
		sim24bit(&bot_now);
		sim24bit(&bot_step);

		// draw
		dr_textured_strip(xx, x1, top_now, top_step, bot_now, bot_step, 7);

		// invalidate last portal
		portal_last = portal_rd;
	}

	// masked portal
	if(	!(masked_idx & 0x80) &&
		portal_wr - portals < MAX_DRAW_PORTALS
	){
		// dummy portal for masked
		portal_wr->sector = 0;
		portal_wr->first_sprite = 0xFF;
		portal_wr->masked = masked_idx;
		portal_wr++;

		return;
	}
}

static void dr_sprite(uint32_t idx)
{
	proj_spr_t *spr = proj_spr + idx;
	uint8_t x0, x1;
	int16_t tex_step, tex_now;

	x0 = spr->x0;
	x1 = spr->x1;

	tex_now = spr->tex_now;
	tex_step = spr->tex_step;

	vera_tex_data(0xF8, 0xFE);
	projection.wx = 0; // 256px

	lightmap = lightmaps + spr->light * LIGHTMAP_SIZE;

	for( ; x0 < x1; x0++, tex_now += tex_step)
	{
		uint8_t xx = x0 / 2;
		int32_t tnow = 0;
		int16_t diff;
		uint8_t y0, y1;
		int16_t top;
		int16_t bot;
		uint8_t *src;
		uint8_t len, offs;
		uint8_t clip_bot, clip_top;

		src = spr->data + spr->cols[tex_now >> 8];

		len = *src++;
		if(!len)
			continue;

		offs = *src++;

		colormap = src;

		bot = spr->bot;
		diff = (offs * spr->scale) >> 8;
		bot -= (spr->dist * diff) >> 8;

		top = bot;
		diff = (len * spr->scale) >> 8;
		top -= (spr->dist * diff) >> 8;
		top--;

		clip_bot = spr->clip_bot[xx];
		if(spr->y1 < clip_bot)
			clip_bot = spr->y1;

		if(top >= clip_bot)
			continue;

		clip_top = spr->clip_top[xx];
		if(spr->y0 > clip_top)
			clip_top = spr->y0;

		if(bot < clip_top)
			continue;

		if(top < clip_top)
			y0 = clip_top;
		else
			y0 = top;

		diff = clip_bot - bot;
		if(diff < 0)
		{
			y1 = clip_bot;
			tnow = spr->tex_scale * diff;
			tnow >>= 2;
		} else
			y1 = bot;

		dr_vspr(x0, y0, y1, tex_now >> 8, tnow, spr->tex_scale / -4);
	}
}

static void dr_masked(uint32_t idx)
{
	proj_msk_t *mt = proj_msk + idx;
	uint8_t x0, x1;
	uint8_t *src_data;
	uint16_t *src_offs;
	int32_t scale_now, scale_step;
	int16_t zdiff;

	x0 = mt->x0 * 2;
	x1 = mt->x1 * 2;

	tex_set(mt->texture, 0, 2, ((mt->sflags >> 4) & 7), 0);

	src_data = projection.wtex;
	src_offs = projection.cols;

	scale_now = mt->scale_now;
	scale_step = mt->scale_step;
	zdiff = projection.z - mt->bz;

	for( ; x0 < x1; x0++)
	{
		uint8_t xx = x0 / 2;
		int32_t tnow = 0;
		int16_t diff;
		uint8_t y0, y1;
		int16_t top;
		int16_t bot;
		uint8_t *src;
		uint8_t len, offs;
		uint8_t tx;

		tx = ((mt->tmap_coord[x0] - mt->ox) ^ mt->xor) & 0x7F;
		src = src_data + src_offs[tx];

		len = *src++;
		if(!len)
			goto next;
		len *= 2;

		offs = *src++;
		offs *= 2;

		bot = ((scale_now >> 8) * (zdiff - offs)) >> 8;
		bot += projection.ycw;

		top = ((scale_now >> 8) * (zdiff - offs - len)) >> 8;
		top += projection.ycw;

		if(top >= mt->clip_bot[xx])
			goto next;

		if(bot < mt->clip_top[xx])
			goto next;

		if(top < mt->clip_top[xx])
			y0 = mt->clip_top[xx];
		else
			y0 = top;

		diff = mt->clip_bot[xx] - bot;
		if(diff < 0)
		{
			y1 = mt->clip_bot[xx];
			tnow = mt->tmap_scale[xx] * diff;
			tnow >>= 2;
		} else
			y1 = bot;

		dr_vline(x0, y0, y1, tx, tnow, mt->tmap_scale[xx] / -4);

next:
		if(x0 & 1)
			scale_now += scale_step;
	}
}

//
// 3D

static void do_walls(uint8_t sdx, wall_t *walb, uint8_t wdx, uint8_t is_obj)
{
	sector_t *sec = map_sectors + sdx;
	wall_t *wall = walb + wdx;
	wall_t *walf = wall;
	uint16_t last_angle = 0x8000;

	portal_last = portal_rd;

	do
	{
		wall_t *waln = walb + wall->next;
		uint16_t a0, a1, ad;
		vertex_t *v0, *v1;
		vertex_t d0, d1, dc;
		vertex_t p0, p1;
		vertex_t ld;
		uint8_t x0, x1;
		int16_t s0, s1;
		uint8_t do_left_clip = 0;
		uint8_t do_right_clip = 0;
		uint8_t inside = 0;

		// skip check
		if(wall->angle & WALL_MARK_SKIP)
		{
			last_angle = 0x8000;
			goto do_next;
		}

		v0 = &wall->vtx;
		v1 = &waln->vtx;

		// flip check
		if(wall->angle & WALL_MARK_SWAP)
		{
			// V1 diff
			d0.x = v1->x - (projection.x >> 8);
			d0.y = v1->y - (projection.y >> 8);
		} else
		{
			// V0 diff
			d0.x = v0->x - (projection.x >> 8);
			d0.y = v0->y - (projection.y >> 8);
		}

		// get distance Y
		ld.y = (d0.x * wall->dist.y - d0.y * wall->dist.x) >> 8;
		if(	ld.y < 0 ||
			(!ld.y && sdx != projection.sector)
		){
			last_angle = 0x8000;
			goto do_next;
		}

		// V0 diff
		d0.x = v0->x - (projection.x >> 8);
		d0.y = v0->y - (projection.y >> 8);

		// V0 angle
		a0 = last_angle;
		if(last_angle & 0x8000)
		{
			p2a_coord.x = d0.x;
			p2a_coord.y = d0.y;
			a0 = point_to_angle();
			draw_aline(a0, 2 * is_obj);
		}

		// V1 diff
		d1.x = v1->x - (projection.x >> 8);
		d1.y = v1->y - (projection.y >> 8);

		// V1 angle
		p2a_coord.x = d1.x;
		p2a_coord.y = d1.y;
		a1 = point_to_angle();
		draw_aline(a1, 2 * is_obj);
		last_angle = a1;

		// side check
		ad = a1 - a0;
		if(	ad & 2048 &&
			!((a0 ^ a1) & 2048)
		){
//			printf("behind 0x%04X 0x%04X\n", a0, a1);
			goto do_next;
		}

		// V0 rejection and clipping checks
		// 0x000 -> 0x3FF: normal wall
		// 0xD00 -> 0xFFF: left clip
		a0 -= projection.a;
		a0 += H_FOV;
		a0 &= 4095;
		if(a0 & 0x800)
		{
			do_left_clip = 1;
		} else
		if(a0 >= 0x400)
		{
//			printf("too right 0x%04X\n", a0);
			goto do_next;
		}

		// V1 rejection and clipping checks
		a1 -= projection.a;
		a1 += H_FOV;
		a1 &= 4095;
		if(a1 >= 0xC00)
		{
//			printf("too left 0x%04X\n", a1);
			goto do_next;
		} else
		if(a1 >= 0x400)
		{
			do_right_clip = 1;
		}

		// extra reject
		if(a0 & a1 & 0x800)
		{
//			printf("extra reject 0x%04X 0x%04X\n", a0, a1);
			goto do_next;
		}

		// angle clipping and X projection
		if(do_left_clip)
			x0 = 0;
		else
			x0 = angle2x[a0] / 2;

		if(do_right_clip)
			x1 = 80;
		else
			x1 = angle2x[a1] / 2;

		if(x0 >= x1)
			goto do_next;

		if(x1 <= projection.x0)
			goto do_next;

		if(x0 >= projection.x1)
			goto do_next;

		// project Y0
		p0.y = (d0.x * projection.sin + d0.y * projection.cos) >> 8;

		// project Y1
		p1.y = (d1.x * projection.sin + d1.y * projection.cos) >> 8;

		// clipping
		if(do_left_clip || do_right_clip)
		{
			if(ld.y < 6)
			{
				p0.y = 0;
				p1.y = 0;
				do_left_clip = 0;
				do_right_clip = 0;
			} else
			{
				// project X0
				p0.x = (d0.x * projection.cos - d0.y * projection.sin) >> 8;

				// project X1
				p1.x = (d1.x * projection.cos - d1.y * projection.sin) >> 8;

				dc.x = p1.x - p0.x;
				dc.y = p1.y - p0.y;
			}
		}

		// left clip
		if(do_left_clip)
		{
			int32_t d, n, r;

			d = dc.x + dc.y;
			n = - p0.x - p0.y;
//			r = n * inv_div[d];
			r = spec_inv_div(n, d);

			if(r > 0 && r < 32768)
				p0.y += (dc.y * r) >> 15;
		}

		// right clip
		if(do_right_clip)
		{
			int32_t d, n, r;

			d = dc.x - dc.y;
			n = p1.x - p1.y;
//			r = n * inv_div[d];
			r = spec_inv_div(n, d);

			if(r > 0 && r < 32768)
				p1.y -= (dc.y * r) >> 15;
		}

		// TODO: Y should be limited to 4095
		s0 = tab_depth[p0.y];
		s1 = tab_depth[p1.y];

		// get distance X
		if(wall->angle & WALL_MARK_XORIGIN)
			ld.x = (d1.x * wall->dist.x + d1.y * wall->dist.y) >> 1;
		else
			ld.x = (d0.x * wall->dist.x + d0.y * wall->dist.y) >> 1;
/*
printf("A0 0x%03X A1 0x%03X\n", a0, a1);
printf("x0 %d x1 %d\n", x0, x1);
printf("dy %d dx %d\n", ld.y, ld.x);
printf("s0 %d y0 %d\n", s0, p0.y);
printf("s1 %d y1 %d\n", s1, p1.y);
*/
		// draw
		dr_wall(sec, wall, &ld, x0, x1, s0, s1);

		// next
do_next:
		wall = waln;
	} while(wall != walf);
}

static void prepare_sprite(uint8_t tdx, sector_t *sec)
{
	thing_t *th = thing_ptr(tdx);
	uint8_t light = SECTOR_LIGHT(sec);
	proj_spr_t *spr;
	vertex_t d0;
	int16_t dist, dscl, xdiff, zdiff;
	uint16_t a0;
	int16_t xc, x0, x1;
	int32_t top, bot;
	int32_t y0, y1;
	int16_t tex_now, tex_step;
	sprite_info_t *si = sprite_info + th->sprite;
	sprite_frame_t *frm;
	int32_t spr_width;
	int32_t spr_height;
	int32_t spr_xoffs;
	int32_t spr_yoffs;
	int32_t spr_scale;

	if(th->sprite & 0x80)
		return;

	// limit
	if(proj_spr_idx >= MAX_DRAW_SPRITES)
		return;

	// diff
	d0.x = (th->x >> 8) - (projection.x >> 8);
	d0.y = (th->y >> 8) - (projection.y >> 8);

	// get distance Y
	dist = (d0.x * projection.sin + d0.y * projection.cos) >> 8;

	// reject
	if(dist <= 16)
		return;

	// center angle
	p2a_coord.x = d0.x;
	p2a_coord.y = d0.y;
	a0 = point_to_angle();
	draw_aline(a0, 1);

	// rotation
	if(si->rotate)
	{
		uint8_t ang;
		ang = th->angle;
		ang -= a0 >> 4;
		ang += 0x90;
		ang >>= 5;
		frm = sprite_frame + si->rot[ang];
	} else
		frm = sprite_frame + si->rot[0];

	if(!frm->offs_data)
		return;

	a0 -= projection.a;
	a0 += H_FOV;
	a0 &= 0x07FF;

	// texture
	spr_width = frm->width;
	spr_height = frm->height * 2;
	spr_xoffs = frm->ox * 2;
	spr_yoffs = frm->oy * 2;
	spr_scale = th->scale * 2 + 64;

	// center X
	xc = angle2x[a0];

	// depth
	dist = tab_depth[dist];
	dscl = (dist * spr_scale) >> 8;

	// X scale
	zdiff = (spr_xoffs * dscl) >> 8;
	x0 = xc - zdiff;
	zdiff = (spr_width * dscl) >> 8;
	x1 = x0 + zdiff;

	xdiff = x1 - x0;
	if(xdiff <= 0)
		// always show thin sprite
		x1 = x0 + 1;

	// X reject
	if(x1 <= projection.x0d)
		return;

	if(x0 >= projection.x1d)
		return;

	// scale
	spr_yoffs = (spr_yoffs * spr_scale) >> 8;

	// bot
	zdiff = projection.z - ((th->z / 256) + spr_yoffs);
	bot = (dist * zdiff) >> 8;
	bot += projection.ycw;

	// Y1 reject
	if(bot <= 0)
		return;

	// top
	zdiff -= (spr_height * spr_scale) >> 8;
	top = (dist * zdiff) >> 8;
	top += projection.ycw;

	// Y0 reject
	if(top >= 120)
		return;

	// bot clip
	if(	th->eflags & THING_EFLAG_SPRCLIP ||
		sec->floor.link
	){
		zdiff = projection.z - sec->floor.height;
		y1 = (dist * zdiff) >> 8;
		y1 += projection.ycw;
		if(y1 > 255)
			y1 = 255;
	} else
		y1 = 255;

	// top clip
	if(	th->eflags & THING_EFLAG_SPRCLIP ||
		sec->ceiling.link
	){
		zdiff = projection.z - sec->ceiling.height;
		y0 = (dist * zdiff) >> 8;
		y0 += projection.ycw;
		if(y0 < 0)
			y0 = 0;
	} else
		y0 = 0;

	// texturing
	tex_step = (spr_width * (int32_t)inv_div[xdiff]) >> 8;
	tex_now = 0;

	// special centering
	if(x1 == x0 + 1)
		tex_now = (spr_width << 6) & 0xFF00;

	// X limit
	if(x1 > projection.x1d)
		x1 = projection.x1d;

	// X clip
	xdiff = x0 - projection.x0d;
	if(xdiff < 0)
	{
		x0 = projection.x0d;
		tex_now = tex_step * -xdiff;
	}

	// new projected sprite
	spr = proj_spr + proj_spr_idx;

	// save stuff
	spr->x0 = x0;
	spr->x1 = x1;
	spr->y0 = y0;
	spr->y1 = y1;
	spr->bot = bot;
	spr->tex_now = tex_now;
	spr->tex_step = tex_step;
	spr->depth = tex_scale[dist];
	spr->tex_scale = tex_scale[dscl];
	spr->dist = dist;
	spr->scale = spr_scale * 2;
	spr->light = th->next_state & 0x8000 ? 0 : light;
	spr->next = 0xFF;

	// texture
	spr->data = wram + frm->offs_data;
	spr->cols = (uint16_t*)(wram + frm->offs_cols);

	// clipping copy & depth check
	for(uint8_t xx = x0 / 2; xx < (x1+1) / 2; xx++)
	{
		spr->clip_top[xx] = clip_top[xx];
		spr->clip_bot[xx] = clip_bot[xx];

		if(spr->depth > tmap_scale[xx])
			spr->clip_top[xx] = 120;
	}

	// depth sorting
	{
		proj_spr_t *prev = NULL;
		proj_spr_t *check;

		if(proj_spr_first == 0xFF)
			check = NULL;
		else
			check = proj_spr + proj_spr_first;

		while(check)
		{
			if(spr->depth < check->depth)
				break;

			prev = check;

			if(check->next == 0xFF)
				check = NULL;
			else
				check = proj_spr + check->next;
		}

		if(check)
			spr->next = check - proj_spr;

		if(prev)
			prev->next = proj_spr_idx;
		else
			proj_spr_first = proj_spr_idx;
	}

	proj_spr_idx++;
}

static void finish_sprite(uint8_t idx)
{
	proj_spr_t *spr = proj_spr + idx;
	uint8_t x0, x1;

	x0 = spr->x0 / 2;
	x1 = (spr->x1 + 1) / 2;

	for( ; x0 < x1; x0++)
	{
		if(spr->depth < tmap_scale[x0])
			spr->clip_top[x0] = 120;
	}
}

static int16_t fix_effect_value(uint8_t val, uint8_t flip)
{
	int16_t temp = val;
	if(flip & 0x80)
		return -temp;
	return temp;
}

static uint8_t handle_plane_effect(texture_info_t *ti, uint8_t ang)
{
	uint8_t *effect;
	uint8_t etime;
	int16_t temp;

	if(!ti)
		return ang;

	effect = ti->effect;

	if(!effect[0])
		return ang;

	etime = anim_tick[effect[1]];

	switch(effect[0] & 3)
	{
		case 1: // random
			if(!(effect[2] & 0x80))
				projection.ox += rng_tab[etime + 0];
			if(!(effect[2] & 0x40))
				projection.oy += rng_tab[etime + 256];
			if(!(effect[2] & 0x01))
				ang += rng_tab[etime + 512];
		break;
		case 2: // circle
		case 3: // eight
			temp = fix_effect_value(effect[3], effect[0] << 1);
			projection.oy += (tab_cos[etime] * temp) >> 8;

			if((effect[0] & 3) == 3)
				etime <<= 1;

			temp = fix_effect_value(effect[2], effect[0]);
			projection.ox += (tab_sin[etime] * temp) >> 8;

		break;
	}

	return ang;
}

static void do_sector(uint8_t idx)
{
	sector_t *sec = map_sectors + idx;
	uint32_t did_object = 0;
	map_secobj_t *sobjlist[MAX_SOBJ];
	uint32_t sobjdist[MAX_SOBJ];

	// fix plane clip
	plc_top[projection.x0] = 0xF0;
	plf_top[projection.x0] = 0xF0;
	plc_top[projection.x1-1] = 0xF0;
	plf_top[projection.x1-1] = 0xF0;

	// prepare things
	proj_spr_first = 0xFF;

	for(uint32_t i = 0; i < 31; i++)
	{
		uint8_t tdx = sectorth[idx][i];

		if(tdx && tdx != camera_thing)
			prepare_sprite(tdx, sec);
	}

	// objects
	if(sec->sobj_hi)
	{
		map_secobj_t *sobj = (map_secobj_t*)(wram + sec->sobj_lo + (sec->sobj_hi & 0x7F) * 65536);

//		printf("addr 0x%02X%04X\n", sec->sobj_hi & 0x7F, sec->sobj_lo);
		for(uint32_t i = 0; i < MAX_SOBJ && !(sobj->bank & 0x80); i++, sobj++)
		{
			uint32_t dist;
			uint32_t pick;

			p2a_coord.x = sobj->x - (projection.x >> 8);
			p2a_coord.y = sobj->y - (projection.y >> 8);
			dist = point_to_dist();

			for(pick = 0; pick < did_object; pick++)
				if(sobjdist[pick] < dist)
					break;

			for(int32_t j = did_object; j > pick; j--)
			{
				sobjlist[j] = sobjlist[j-1];
				sobjdist[j] = sobjdist[j-1];
			}

			sobjlist[pick] = sobj;
			sobjdist[pick] = dist;

			did_object++;
		}

		for(int32_t i = did_object-1; i >= 0; i--)
		{
			map_secobj_t *sobj = sobjlist[i];
			do_walls(idx, map_walls[sobj->bank], sobj->first, 1);
//			printf("obj 0x%06X; bank %u first %u; %d x %d\n", (uint8_t*)sobj - wram, sobj->bank, sobj->first, sobj->x, sobj->y);
		}
	}

	// walls
	do_walls(idx, map_walls[sec->wall.bank], sec->wall.first, 0);

	projection.fix = 0;

	// floor
	if(sec->floor.texture == 0xFF)
		dr_sky(plf_top, plf_bot);
	else
	if(projection.z > sec->floor.height)
	{
		uint8_t ang;
		texture_info_t *ti;

		ti = tex_set(sec->floor.texture, sec->floor.ox, sec->floor.oy, SECTOR_LIGHT(sec), 0x80);
		ang = handle_plane_effect(ti, sec->floor.angle);

		projection.pl_x = (projection.x * tab_cos[ang] + projection.y * tab_sin[ang]) >> 8;
		projection.pl_y = (projection.y * tab_cos[ang] + projection.x * tab_sin[ang ^ 0x80]) >> 8;

		ang += projection.a8;

		projection.pl_sin = tab_sin[ang];
		projection.pl_cos = tab_cos[ang];

		dr_plane(projection.z - sec->floor.height, plf_top, plf_bot);
	}

	// ceiling
	if(sec->ceiling.texture == 0xFF)
		dr_sky(plc_top, plc_bot);
	else
	if(projection.z < sec->ceiling.height)
	{
		uint8_t ang;
		texture_info_t *ti;

		ti = tex_set(sec->ceiling.texture, sec->ceiling.ox, sec->ceiling.oy, SECTOR_LIGHT(sec), 0x80);
		ang = handle_plane_effect(ti, sec->ceiling.angle);

		projection.pl_x = (projection.x * tab_cos[ang] + projection.y * tab_sin[ang]) >> 8;
		projection.pl_y = (projection.y * tab_cos[ang] + projection.x * tab_sin[ang ^ 0x80]) >> 8;

		ang += projection.a8;

		projection.pl_sin = tab_sin[ang];
		projection.pl_cos = tab_cos[ang];

		dr_plane(projection.z - sec->ceiling.height, plc_top, plc_bot);
	}

	// object cleanup
	if(did_object)
	{
		for(uint32_t x0 = projection.x0; x0 < projection.x1; x0++)
		{
			if(clip_top[x0] & 0x80)
			{
				plc_top[x0] = 0xF0;
				plf_top[x0] = 0xF0;
			}
		}
	}

	// finish sprites
	{
		uint8_t idx = proj_spr_first;

		while(idx != 0xFF)
		{
			finish_sprite(idx);
			idx = proj_spr[idx].next;
		}
	}

	// save sprite list
	portal_rd->first_sprite = proj_spr_first;
}

static void do_3d()
{
	thing_t *th = thing_ptr(camera_thing);
	show_wpn_t show_wpn;
	sector_t *sec;
	uint32_t pidx;

	for(int32_t i = 0; i < 4; i++)
		anim_tick[i] = level_tick << (4-i);
	for(int32_t i = 0; i < 6; i++)
		anim_tick[i+4] = level_tick >> i;

	projection.x = th->x & ~0xFF;
	projection.y = th->y & ~0xFF;
	projection.z = (th->z >> 8) + projection.wh;
	projection.sin = tab_sin[th->angle];
	projection.cos = tab_cos[th->angle];
	projection.a = (uint16_t)th->angle << 4;
	projection.a8 = th->angle;
	projection.ycw = pitch2yc[th->pitch ^ 0x80];
	projection.ycp = 128 - projection.ycw;
	projection.sector = thingsec[camera_thing][0];

	sec = map_sectors + projection.sector;

	pidx = (15 + camera_damage) >> 4;
	if(pidx & 0xFC)
		pidx = 3;
	pidx += sec->flags & 0x0C;

	palette = palette_src;
	palette += pidx * 256 * 3;

	if(projection.z > sec->ceiling.height - 2)
		projection.z = sec->ceiling.height - 2;

	if(projection.z < sec->floor.height + 2)
		projection.z = sec->floor.height + 2;

	memset(framebuffer, frame_counter, sizeof(framebuffer));

	memset(clip_top, 0, sizeof(clip_top));
	memset(clip_bot, 120, sizeof(clip_bot));
	memset(tmap_scale, 0, sizeof(tmap_scale));

	proj_spr_idx = 0;
	proj_msk_idx = 0;

	portal_rd = portals;
	portal_wr = portals;

	portal_wr->sector = projection.sector;
	portal_wr->x0 = 0;
	portal_wr->x1 = 80;
	portal_wr->masked = 0xFF;
	portal_wr++;

	// sectors
	while(portal_rd < portal_wr)
	{
		uint8_t idx;

		idx = portal_rd->sector;
		if(idx)
		{
			projection.x0 = portal_rd->x0;
			projection.x0d = projection.x0 * 2;
			projection.x1 = portal_rd->x1;
			projection.x1d = projection.x1 * 2;
			do_sector(idx);
		}

		portal_rd++;
	}

	// sprites and masked
	while(1)
	{
		uint8_t idx;

		portal_rd--;

		idx = portal_rd->first_sprite;
		while(idx != 0xFF)
		{
			dr_sprite(idx);
			idx = proj_spr[idx].next;
		}

		if(!(portal_rd->masked & 0x80))
			dr_masked(portal_rd->masked);

		if(portal_rd == portals)
			break;
	}

	// weapon
	show_wpn.idx = 0xFF;

	if(camera_thing == player_thing)
		show_wpn.idx = thing_ptr(0)->sprite;

	if(show_wpn.idx >= 128)
	{
		show_wpn.light = 0;
		show_wpn_now.light = 0;
		show_wpn.offs = 0;
		show_wpn_now.offs = 0;
	} else
	{
		show_wpn.light = SECTOR_LIGHT(&map_sectors[thingsec[camera_thing][0]]);
		show_wpn.offs = weapon_frame[show_wpn.idx].info.start;
	}

	if(show_wpn.offs != show_wpn_now.offs)
	{
		// update all pixels, swap buffers
		weapon_frame_t *wfrm = weapon_frame + show_wpn.idx;
		uint8_t *dst;

		show_wpn_now.offs = show_wpn.offs;
		show_wpn_now.light = 0xFF; // force normal pixel update

		show_wpn_slot ^= 1;
		dst = vram + ((show_wpn_slot * 0x2000) & 0x1FFFF);

		// copy only fullbright pixels here
		memcpy(dst + wfrm->info.nsz * 256, wram + (show_wpn.offs + wfrm->info.nsz) * 256, wfrm->info.bsz * 256);
	}

	if(show_wpn.light != show_wpn_now.light)
	{
		// update unlit pixels
		weapon_frame_t *wfrm = weapon_frame + show_wpn.idx;
		uint8_t *dst = vram + ((show_wpn_slot * 0x2000) & 0x1FFFF);
		uint8_t *src = wram + show_wpn.offs * 256;
		uint32_t count = wfrm->info.nsz * 256;
		uint8_t *light = lightmaps + show_wpn.light * LIGHTMAP_SIZE;

		show_wpn_now.light = show_wpn.light;
		for(uint32_t i = 0; i < count; i++)
			*dst++ = light[*src++];
	}

	// update parts, always
	{
		x16_sprite_t *spr = (x16_sprite_t*)&vram[sizeof(vram) - (12 + 15) * sizeof(x16_sprite_t)];
		uint32_t i = 0;

		show_wpn_now.idx = show_wpn.idx;

		if(show_wpn.idx < 128)
		{
			thing_t *th = thing_ptr(player_thing);
			thing_t *tw = thing_ptr(0);
			weapon_part_t *part = weapon_frame[show_wpn.idx].part;
			int32_t ox, oy;
			int32_t dist = 0;

			ox = abs(th->mx) >> 8;
			oy = abs(th->my) >> 8;
			dist = ox > oy ? ox + oy / 2 : oy + ox / 2;
			dist *= inv_div[thing_type[th->ticker.type].speed];
			dist >>= 8;

			if(dist > 127)
				dist = 127;

			if(tw->iflags)
				dist >>= 2;

			show_wpn_now.avg >>= 1;
			show_wpn_now.avg += dist;

			dist = show_wpn_now.avg >> 1;
			if(dist)
			{
				dist <<= 4;
				ox = ((tab_sin[(level_tick << 4) & 0xFF] * dist) >> 16) - 48;
				oy = ((tab_cos[(level_tick << 4) & 0xFF] * dist) >> 16);
				if(oy < 0)
					oy = -oy;
			} else
			{
				ox = -48;
				oy = 0;
			}

			oy += tw->height;

			if(th->pitch >= 148)
			{
				int32_t diff = (int32_t)th->pitch - 148;
				oy += diff >> 2;
			}

			for( ; i < 15 && part->addr < 128; i++, spr++, part++)
			{
				spr->addr = (show_wpn_slot << 8) | part->addr;
				spr->x = ox + (int16_t)part->x;
				spr->y = oy + part->y;
				spr->ia = part->info;
				spr->ib = part->info & 0xF0;
			}
		}

		for( ; i < 15; i++, spr++)
			spr->ia = 0;
	}
}

//
// render

static void render()
{
	thing_t *th = thing_ptr(player_thing);
	float angle;

	frame_counter++;
#if 1
	if(!(frame_counter % 4))
#else
	if(!(frame_counter % 32))
#endif
	{
		level_tick++;
		tick_run(); // 15 TPS
		input_action = 0;
#if 0
		thing_t *th = things + player_thing;
		printf("fs %u cs %u\n", th->floors, th->ceilings);
#endif
#if 0
		for(uint32_t i = 0; i < 16; i++)
		{
			uint8_t s = thingsec[player_thing][i];
			if(!s)
				break;
			printf("secslot[%u] = %u\n", i, s);
		}
#endif
	}

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	//
	// relative view

	glViewport(0, 0, WIDTH / 2, HEIGHT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
//	glOrtho(WIDTH / -4, WIDTH / 4, HEIGHT / -2, HEIGHT / 2, 1.0f, -1.0f);
	glOrtho(WIDTH / -2, WIDTH / 2, HEIGHT / -1, HEIGHT / 1, 1.0f, -1.0f);

	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();

	glTranslatef(-th->x >> 8, -th->y >> 8, 0);

	//
	// special mark

	draw_line(display_point.x - 8, display_point.y - 8, display_point.x + 8, display_point.y + 8);
	draw_line(display_point.x - 8, display_point.y + 8, display_point.x + 8, display_point.y - 8);

	if(display_wall[0].x != display_wall[1].x || display_wall[0].y != display_wall[1].y)
		draw_line(display_wall[0].x, display_wall[0].y, display_wall[1].x, display_wall[1].y);

	if(display_line[0].x != display_line[1].x || display_line[0].y != display_line[1].y)
		draw_line(display_line[0].x, display_line[0].y, display_line[1].x, display_line[1].y);

	//
	// do 3D now

	do_3d();

	//
	// camera

	glLoadIdentity();
	angle = ANG8_TO_DEG(th->angle);
	glRotatef(-angle, 0, 0, 1);

	// FOV
	draw_fov();

	// camera
	draw_camera();

	//
	// projection

	glViewport(WIDTH / 2 + 15, HEIGHT / 2 + 15, 480, 360);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, 480, 0.0f, 360, 1.0f, -1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glColor3f(1.0f, 1.0f, 1.0f);

	update_texture(0, 0, 480, 360);
	update_sprites(0, 0, 480, 360);

	glDisable(GL_TEXTURE_2D);

	//
	// split

	glViewport(0, 0, WIDTH, HEIGHT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, WIDTH, 0.0f, HEIGHT, 1.0f, -1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glColor3f(0.4f, 0.4f, 0.4f);

	glBegin(GL_LINES);
		glVertex3f(WIDTH / 2, 0, 0);
		glVertex3f(WIDTH / 2, HEIGHT, 0);
	glEnd();

	//
	// done

	SDL_GL_SwapWindow(sdl_win);
}

//
// input

static void input()
{
	SDL_Event event;
	static int32_t mousex;
	static int32_t mousey;
	static uint32_t mbtn;

	while(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT)
			stopped = 1;
		else
		switch(event.type)
		{
			case SDL_KEYDOWN:
				for(uint32_t i = 0; i < NUM_KEYS; i++)
					if(event.key.keysym.sym == mkey_sdl[i])
						keybits |= 1 << i;
				switch(event.key.keysym.sym)
				{
					case SDLK_F1:
						render_flags ^= 1;
					break;
					case SDLK_F2:
						render_flags ^= 2;
					break;
					case SDLK_F3:
						render_flags ^= 4;
					break;
				}
			break;
			case SDL_KEYUP:
				for(uint32_t i = 0; i < NUM_KEYS; i++)
					if(event.key.keysym.sym == mkey_sdl[i])
						keybits &= ~(1 << i);
				if(event.key.keysym.sym >= '1' && event.key.keysym.sym <= '9')
					input_action = event.key.keysym.sym - 48;
				else
				if(event.key.keysym.sym == '0')
					input_action = 10;
			break;
			case SDL_MOUSEMOTION:
				mousex += event.motion.xrel;
				mousey += event.motion.yrel;
			break;
			case SDL_MOUSEBUTTONDOWN:
				mbtn |= 1 << event.button.button;
			break;
			case SDL_MOUSEBUTTONUP:
				mbtn &= ~(1 << event.button.button);
			break;
		}
	}

	// turning
	if(keybits & _BV(MKEY_ROTATE_LEFT))
		ticcmd.angle -= 1;

	if(keybits & _BV(MKEY_ROTATE_RIGHT))
		ticcmd.angle += 1;

	ticcmd.angle += mousex >> 1;
	mousex -= mousex >> 1;

	// looking

	ticcmd.pitch -= mousey;
	mousey = 0;

	if(ticcmd.pitch < 0x4A)
		ticcmd.pitch = 0x4A;
	else
	if(ticcmd.pitch > 0xB6)
		ticcmd.pitch = 0xB6;

	// moving
	ticcmd.bits_l = move_angle[keybits & 0x0F];
	ticcmd.bits_h = ticcmd.bits_l << 1;
	ticcmd.bits_l &= TCMD_MOVING;

	if(keybits & _BV(MKEY_GO_UP))
		ticcmd.bits_l |= TCMD_GO_UP;

	if(keybits & _BV(MKEY_GO_DOWN))
		ticcmd.bits_l |= TCMD_GO_DOWN;

	// use
	if(keybits & _BV(MKEY_USE))
		ticcmd.bits_l |= TCMD_USE;

	// attack
	if(mbtn & 2)
		ticcmd.bits_l |= TCMD_ATK;
	if(mbtn & 8)
		ticcmd.bits_l |= TCMD_ALT;

	// action
	ticcmd.bits_h |= input_action;
}

//
// stuff

static int save_file(const char *name, void *data, uint32_t size)
{
	int fd;

	fd = open(name, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if(fd < 0)
		return 1;

	write(fd, data, size);
	close(fd);

	return 0;
}

int32_t load_file(const char *name, void *data, uint32_t size)
{
	int32_t fd, rd;

	fd = open(name, O_RDONLY);
	if(fd < 0)
		return -1;

	rd = read(fd, data, size);
	close(fd);

	return rd;
}

static void prepare_stuff()
{
	float angle = ANGLE_TO_RAD(H_FOV);
	uint8_t *pal;

	gl_fov_x = sinf(angle) * 1024.0f;
	gl_fov_y = cosf(angle) * 1024.0f;
}

static uint32_t load_tables()
{
	int32_t fd;
	tables_1100_t tables_1100;
	tables_A000_t tables_A000;
	uint16_t pal12bpp[256 * 16];
	uint8_t *dst;

	if(load_file("DATA/TABLES3.BIN", pal12bpp, sizeof(pal12bpp)) != sizeof(pal12bpp))
	{
		printf("Unable to load TABLES3.BIN!\n");
		return 1;
	}

	if(load_file("DATA/TABLES0.BIN", &tables_1100, sizeof(tables_1100_t)) != sizeof(tables_1100_t))
	{
		printf("Unable to load TABLES0.BIN!\n");
		return 1;
	}

	if(load_file("DATA/TABLES1.BIN", &tables_A000, sizeof(tables_A000_t)) != sizeof(tables_A000_t))
	{
		printf("Unable to load TABLES1.BIN!\n");
		return 1;
	}

	/// VRAM tables

	fd = open("DATA/TABLES2.BIN", O_RDONLY);
	if(fd < 0)
	{
		printf("Unable to load TABLES2.BIN!\n");
		return 1;
	}

	// font info
	read(fd, font_info, sizeof(font_info));

	// HUD
	read(fd, vram + 62 * 256, 2 * 256);
	read(fd, vram + 94 * 256, 2 * 256);

	// font, 32 characters
	read(fd, vram + 126 * 256, 2 * 256);
	read(fd, vram + 158 * 256, 2 * 256);

	// 256x16 / 64x64 tile map
	// font, next 32 characters
	// 128x32 tile map
	// font, last 32 characters
	read(fd, vram + 464 * 256, 16 * 256);

	// ranges
	// invalid texture
	read(fd, vram + 496 * 256, 9 * 256 + 192);

	close(fd);

	/// palette

	dst = palette_src;
	for(uint32_t i = 0; i < 256 * 16; i++)
	{
		uint16_t c = pal12bpp[i];
		uint8_t t;

		t = c & 0x000F;
		*dst++ = (t << 4) + t;

		t = (c & 0x00F0) >> 4;
		*dst++ = (t << 4) + t;

		t = (c & 0x0F00) >> 8;
		*dst++ = (t << 4) + t;
	}

	/// T0

	// hitscan tan
	for(uint32_t i = 0; i < 128; i++)
		tab_tan_hs[i] = (tables_1100.htan_h[i] << 8) | tables_1100.htan_l[i];

	// sin / cos
	for(uint32_t i = 0; i < 256; i++)
		tab_sin[i] = (tables_1100.sin_h[i] << 8) | tables_1100.sin_l[i];

	// inverse division (hi)
	for(uint32_t i = 0; i < 256; i++)
		inv_div[i] = tables_1100.idiv_h[i] << 8;

	// Y depth projection (hi)
	for(uint32_t i = 0; i < 256; i++)
		tab_depth[i] = tables_1100.ydepth_h[i] << 8;

	// X to angle
	for(uint32_t i = 0; i < 160; i++)
		x2angle[i] = (tables_1100.x2a_h[i] << 8) | tables_1100.x2a_l[i];

	// nibble swap
	// NOT USED

	// Y lookup
	// NOT USED

	// X lookup
	// NOT USED

	// pixel jump offsets (vertical)
	// NOT USED

	// pixel jump offsets (horizontal)
	// NOT USED

	// pixel jump offsets (sky)
	// NOT USED

	// pixel loop (horizontal and vertical)
	// NOT USED

	// pixel loop (sky)
	// NOT USED

	/// T1

	// inverse division (lo)
	for(uint32_t i = 0; i < 8192; i++)
		inv_div[i] |= tables_A000.idiv_l[i];

	// texture scale
	for(uint32_t i = 0; i < 4096; i++)
		tex_scale[i] = tables_A000.tscale_l[i] | (tables_A000.tscale_h[i] << 8);

	// Y depth projection (lo)
	for(uint32_t i = 0; i < 4096; i++)
		tab_depth[i] |= tables_A000.ydepth_l[i];

	// arc tan
	for(uint32_t i = 0; i < 4096; i++)
		atan_tab[i] = (tables_A000.atan_h[i] << 8) | tables_A000.atan_l[i];

	// tan
	for(uint32_t i = 0; i < 2048; i++)
		tab_tan[i] = (tables_A000.tan_h[i] << 8) | tables_A000.tan_l[i];

	// angle to X
	for(uint32_t i = 0; i < 2048; i++)
		angle2x[i] = (tables_A000.a2x_h[i] << 8) | tables_A000.a2x_l[i];

	// plane X
	for(uint32_t i = 0; i < 256; i++)
		tab_planex[i] = (tables_A000.planex_h[i] << 8) | tables_A000.planex_l[i];

	// pitch to Y center
	for(uint32_t i = 0; i < 256; i++)
		pitch2yc[i] = tables_A000.pitch2yc[i];

	// random
	memcpy(rng_tab, tables_A000.random, sizeof(rng_tab));
	memcpy(rng_mask, tables_A000.rng_mask, sizeof(rng_mask));

	/// finishing touch

	for(uint32_t i = 8192; i < 11008; i++)
		inv_div[i] = 3;

	for(uint32_t i = 11008; i < 16384; i++)
		inv_div[i] = 2;

	for(uint32_t i = 16384; i < 32768; i++)
		inv_div[i] = 1;

	for(uint32_t i = 1; i < 32768; i++)
		inv_div_raw[i] = -inv_div[32768 - i];
	inv_div_raw[0] = -1;

	for(uint32_t i = 0; i < 256; i++)
		tab_cos[(i - 64) & 0xFF] = tab_sin[i];

	// thing types
	if(thing_init("DATA/TABLES4.BIN"))
		return 1;

	return 0;
}

//
// map loading

static uint32_t add_texture(texture_info_t *ti)
{
	if(num_textures >= MAX_TEXTURES)
		return 1;

	map_texture_info[num_textures] = *ti;

	num_textures++;

	return 0;
}

static void generate_cmap_16a(uint8_t *dst, uint32_t light, uint8_t *cmap, uint16_t bright)
{
	uint8_t *color = lightmaps + light * LIGHTMAP_SIZE;

	for(uint32_t i = 0; i < 256; i++)
	{
		uint32_t ii = i & 15;
		uint8_t pc = cmap[ii];

		if(bright & (1 << ii))
			dst[i] = pc;
		else
			dst[i] = color[pc];
	}
}

static void generate_cmap_16b(uint8_t *dst, uint32_t light, uint8_t *cmap, uint16_t bright)
{
	uint8_t *color = lightmaps + light * LIGHTMAP_SIZE;

	for(uint32_t i = 0; i < 256; i++)
	{
		uint32_t ii = i >> 4;
		uint8_t pc = cmap[ii];

		if(bright & (1 << ii))
			dst[i] = pc;
		else
			dst[i] = color[pc];
	}
}

static void convert_pixels(uint8_t *dst, uint32_t light, uint16_t *src, uint32_t size)
{
	uint8_t *color = lightmaps + light * LIGHTMAP_SIZE;

	for(uint32_t i = 0; i < size; i++)
	{
		uint16_t in = *src++;

		if(in & 0xFF00)
			in &= 0xFF;
		else
			in = color[in];

		*dst++ = in;
	}
}

static void *get_wram(uint32_t size)
{
	void *ret;

	size += 0xFF;
	size &= ~0xFF;

	if(wram_used + size > sizeof(wram))
		return NULL;

	ret = wram + wram_used;
	wram_used += size;

	return ret;
}

static uint32_t find_texture(uint32_t nhash, uint32_t vhash)
{
	for(uint32_t i = 0; i < num_textures; i++)
	{
		texture_info_t *ti = map_texture_info + i;

		if(ti->nhash == nhash && ti->vhash == vhash)
			return i;
	}

	return MAX_TEXTURES;
}

static uint32_t load_sky(uint8_t *path)
{
	uint32_t size;
	uint8_t *dst;

	sky_base = wram_used / 256;

	dst = get_wram(512 * 128);

	size = load_file(path, dst, 512 * 128);
	if(size != 512 * 128)
		return 1;

	return 0;
}

static uint32_t load_plane(uint8_t *path, uint32_t hash, uint32_t lights)
{
	int32_t fd;
	int32_t size, ret;
	uint8_t *dst;
	uint32_t offs;
	export_plane_t ep;
	texture_info_t ti;
	void (*cgen)(uint8_t*, uint32_t, uint8_t*, uint16_t);

	if(vram_8bpp == (vram_4bpp & 0xFE))
		return 1;

	size = load_file(path, &ep, sizeof(ep));
	if(size < 2)
	{
		printf("Unable to load %s!\n", path);
		return 1;
	}

	ti.nhash = hash;
	ti.plane.vera_map_base = ep.map_base;

	if(ep.num_variants & 0x80)
	{
		// 8bpp
		ti.type = 0x81;

		vram_8bpp -= 2;

		offs = VRAM_TEXTURE_START + vram_8bpp * 2048;
		dst = vram + offs;

		memcpy(dst, ep.bpp8.data, 4096);

		memcpy(ti.effect, ep.bpp8.effect, 4);

		ti.vhash = 0;
		ti.plane.vera_tile_base = offs >> 9;

		if(add_texture(&ti))
			return 1;

		return 0;
	}

	// 4bpp
	ti.type = 0x80;
	offs = VRAM_TEXTURE_START + (vram_4bpp >> 1) * 4096;
	dst = vram + offs;

	ti.plane.vera_tile_base = offs >> 9;

	if(vram_4bpp & 1)
	{
		for(uint32_t i = 0; i < 2048; i++)
		{
			*dst++ |= ep.bpp4.data[i] << 4;
			*dst++ |= ep.bpp4.data[i] & 0xF0;
		}
		cgen = generate_cmap_16b;
	} else
	{
		for(uint32_t i = 0; i < 2048; i++)
		{
			*dst++ = ep.bpp4.data[i] & 0x0F;
			*dst++ = ep.bpp4.data[i] >> 4;
		}
		cgen = generate_cmap_16a;
	}
	vram_4bpp++;

	// variants
	for(uint32_t i = 0; i < ep.num_variants; i++)
	{
		export_plane_variant_t *vi = ep.bpp4.variant + i;
		uint32_t def_cmap = wram_used;
		uint32_t ll = lights;

		ti.vhash = vi->hash;
		memcpy(ti.effect, vi->effect, 4);

		if(vi->bright == 0xFFFF)
			// only fullbright
			ll = 1;

		for(uint32_t j = 0; j < map_head.count_lights; j++)
		{
			if(ll & (1 << j))
			{
				uint8_t *cmap;

				ret = wram_used;

				cmap = get_wram(256);
				if(!dst)
					return 1;

				cgen(cmap, j, vi->data, vi->bright);
			} else
				ret = def_cmap;

			ti.plane.colormap[j] = ret;
		}

		if(add_texture(&ti))
			return 1;
	}

	return 0;
}

static uint32_t load_wall(uint8_t *path, uint32_t hash, uint32_t lights)
{
	int32_t size;
	uint8_t data[65536 * 3];
	uint8_t *src = data;
	uint32_t datal, count;
	uint32_t offset;
	uint8_t *dst;
	uint8_t anim[3];
	texture_info_t ti;

	size = load_file(path, data, sizeof(data));
	if(size < 2)
	{
		printf("Unable to load %s!\n", path);
		return 1;
	}

	ti.nhash = hash;

	datal = *src++ << 8;
	count = *src++;

	if(count & 0x80)
		lights = 1;

	count &= MAX_X16_VARIANTS - 1;

	if(!datal)
		datal = 0x10000;

	offset = wram_used;

	for(uint32_t i = 0; i < map_head.count_lights; i++)
	{
		if(lights & (1 << i))
		{
			dst = get_wram(datal);
			if(!dst)
				return 1;

			convert_pixels(dst, i, (uint16_t*)src, datal);
		}
	}

	src += datal * 2;

	for(uint32_t i = 0; i < count; i++)
	{
		uint8_t *cols;
		uint32_t loffs;
		uint8_t *srl;

		ti.vhash = *(uint32_t*)src;
		src += sizeof(uint32_t);

		ti.type = *src++ & 15;
		ti.wall.offs_cols = wram_used;

		anim[0] = *src++;
		anim[1] = *src++;
		anim[2] = *src++;

		if(anim[0])
		{
			ti.effect[0] = 4;
			ti.effect[1] = anim[1];
			ti.effect[2] = anim[0];
			ti.effect[3] = anim[2];
		} else
			ti.effect[0] = 0;

		cols = get_wram(256);
		if(!cols)
			return 1;

		srl = src;
		src += 128;
		for(uint32_t x = 0; x < 128; x++)
		{
			cols[x*2+0] = *srl++;
			cols[x*2+1] = *src++;
		}

		loffs = offset;
		for(uint32_t j = 0; j < map_head.count_lights; j++)
		{
			ti.wall.offs_data[j] = offset;
			if(lights & (1 << j))
			{
				ti.wall.offs_data[j] = loffs;
				loffs += datal;
			}
		}

		if(add_texture(&ti))
			return 1;
	}

	return 0;
}

static uint32_t load_tspr(uint8_t *path)
{
	uint32_t top_frame = 0;
	int32_t size;
	uint8_t data[65536 * 3];
	uint8_t *src = data;
	uint32_t datal, count;
	uint32_t offset, offs_cols;
	uint8_t *dst;
	struct
	{
		uint8_t frm;
		uint8_t rot;
		int8_t ox, oy;
		uint8_t width, height;
	} *info;

	size = load_file(path, data, sizeof(data));
	if(size < 2)
	{
		printf("Unable to load %s!\n", path);
		return 1;
	}

	datal = *src++ << 8;
	count = *src++;

	if(!datal)
		datal = 0x10000;

	offset = wram_used;

	dst = get_wram(datal);
	if(!dst)
		return 1;

	memcpy(dst, src, datal);
	src += datal;

	for(uint32_t i = 0; i < count; i++)
	{
		uint8_t *cols;
		uint8_t *srl;
		sprite_info_t *si;
		sprite_frame_t *frm;

		if(num_sframes >= MAX_SPRITE_FRAMES)
			return 1;

		info = (void*)src;
		src += 6;

		if(info->frm >= 32)
			return 1;

		if(info->rot >= 8)
			return 1;

		if(info->frm > top_frame)
		{
			if(num_sprites + top_frame + 1 > MAX_SPRITES)
				return 1;
			top_frame = info->frm;
		}

		offs_cols = wram_used;
		cols = get_wram(256);
		if(!cols)
			return 1;

		srl = src;
		src += 128;
		for(uint32_t x = 0; x < 128; x++)
		{
			cols[x*2+0] = *srl++;
			cols[x*2+1] = *src++;
		}

		si = sprite_info + num_sprites + info->frm;

		if(info->rot)
			si->rotate = 1;

		si->rot[info->rot] = num_sframes;
		frm = sprite_frame + num_sframes;
		num_sframes++;

		frm->width = info->width;
		frm->height = info->height;
		frm->ox = info->ox;
		frm->oy = info->oy;
		frm->offs_cols = offs_cols;
		frm->offs_data = offset;
	}

	num_sprites += top_frame + 1;

	return 0;
}

static uint32_t load_wspr(uint8_t *path)
{
	uint32_t top_frame = 0;
	int32_t size;
	uint8_t data[65536 * 3];
	uint8_t *src = data;
	uint32_t datal, count;
	uint32_t offset;
	uint8_t *dst;
	weapon_frame_t *base;

	size = load_file(path, data, sizeof(data));
	if(size < 2)
	{
		printf("Unable to load %s!\n", path);
		return 1;
	}

	datal = *src++ << 8;
	count = *src++;

	count &= 0x1F;

	if(!datal)
		datal = 0x10000;

	offset = wram_used / 256;

	dst = get_wram(datal);
	if(!dst)
		return 1;

	memcpy(dst, src, datal);
	src += datal;

	if(num_wframes + count >= MAX_WEAPON_FRAMES)
		return 1;

	base = weapon_frame + num_wframes;

	for(uint32_t i = 0; i < count; i++)
	{
		uint32_t frame, start, nsz, bsz, pcnt;
		weapon_frame_t *wfrm;

		pcnt = *src++;
		start = *src++;
		frame = *src++;
		nsz = *src++;
		bsz = *src++;

		wfrm = base + frame;

		if(frame > top_frame)
			top_frame = frame;

		wfrm->info.start = offset + start;
		wfrm->info.nsz = nsz;
		wfrm->info.bsz = bsz;

		memcpy(wfrm->part, src, pcnt * sizeof(weapon_part_t));
		src += pcnt * sizeof(weapon_part_t);

		if(pcnt < 15)
			wfrm->part[pcnt].addr = 0xFF;
	}

	num_wframes += top_frame + 1;

	return 0;
}

static uint32_t load_thing_sprites(uint32_t type, uint32_t recursion)
{
	thing_type_t *info = thing_type + type;
	uint8_t text[64];

	for(int32_t j = NUM_THING_ANIMS-1; j >= 0; j--)
	{
		thing_anim_t *anim = thing_anim[type] + j;
		thing_state_t *st = thing_state + (anim->state & (MAX_X16_STATES-1));

		for(uint32_t k = 0; k < anim->count; k++, st++)
		{
			if(st->sprite & 0x80)
				continue;

			if(!(sprite_remap[st->sprite] & 0x80))
				continue;

			if(st->sprite >= thing_state->num_sprlnk)
			{
				sprite_remap[st->sprite] = num_wframes;
				sprintf(text, "DATA/%08X.WPS", sprite_hash[st->sprite]);
				if(load_wspr(text))
					return 1;
			} else
			{
				sprite_remap[st->sprite] = num_sprites;
				sprintf(text, "DATA/%08X.THS", sprite_hash[st->sprite]);
				if(load_tspr(text))
					return 1;
			}
		}
	}

	if(recursion)
		return 0;

	for(uint32_t i = 0; i < THING_MAX_SPAWN_TYPES; i++)
	{
		uint32_t tt = info->spawn[i];
		if(tt < MAX_X16_THING_TYPES && load_thing_sprites(tt, recursion + 1))
			return 1;
	}

	return 0;
}

static void expand_array(void *dest, uint32_t rs, uint32_t ss)
{
	uint8_t *dst = dest;
	uint8_t *src = map_block_data;

	if(!ss)
		ss = rs;

	for(uint32_t j = 0; j < rs; j++)
	{
		for(uint32_t i = 0; i < 256; i++)
			dst[j + i * ss] = *src++;
	}
}

static void expand_walls(uint32_t idx)
{
	uint8_t *dst = (uint8_t*)&map_walls[idx][0];
	uint8_t *src = (uint8_t*)&map_walls[idx][1];

	dst += 16;

	for(uint32_t i = 0; i < 255; i++)
	{
		memcpy(dst, src, 16);
		src += 32;
		dst += 32;
	}
}

static uint32_t load_map()
{
	void *dst;
	int32_t fd;
	uint32_t temp, esbase;
	uint8_t text[64];
	marked_texture_t mt;
	marked_variant_t mv;

	memset(&show_wpn_now, 0xFF, sizeof(show_wpn_t));
	show_wpn_now.avg = 0;

	tick_clear();

	fd = open("DATA/DEFAULT.MAP", O_RDONLY);
	if(fd < 0)
		return 1;

	thing_clear();

	wram_used = wram_used_pc;

	num_textures = 0;
	num_sprites = num_sprites_pc;
	num_sframes = num_sframes_pc;
	num_wframes = num_wframes_pc;

	vram_4bpp = 0;
	vram_8bpp = (VRAM_TEXTURE_END - VRAM_TEXTURE_START) / 2048;

	memcpy(sprite_remap, sprite_remap_pc, sizeof(sprite_remap));

	if(read(fd, &map_head, sizeof(map_head)) != sizeof(map_head))
		goto error;

	if(map_head.magic != MAP_MAGIC)
		goto error;

	if(map_head.version != MAP_VERSION)
		goto error;

	map_head.count_lights++;
	if(map_head.count_lights >= MAX_LIGHTS)
		goto error;

	temp = map_head.count_starts_normal;
	temp += map_head.count_starts_coop;
	temp += map_head.count_starts_dm;

	if(temp >= MAX_PLAYER_STARTS)
		goto error;

	if(	!map_head.count_wall_banks ||
		map_head.count_wall_banks > WALL_BANK_COUNT
	)
		goto error;

	if(!map_head.count_things)
		goto error;

	if(map_head.count_extra_storage > MAX_EXTRA_STORAGE)
		goto error;

	// logo
	if(map_head.flags & 0x80)
		lseek(fd, 160 * 120, SEEK_CUR);

	// sky
	if(map_head.hash_sky)
	{
		sprintf(text, "DATA/%08X.SKY", map_head.hash_sky);
		if(load_sky(text))
			goto error;
	}

	// lights
	for(uint32_t i = 0; i < 256; i++)
		lightmaps[i] = i;

	for(uint32_t i = 1; i < map_head.count_lights; i++)
	{
		if(read(fd, &temp, sizeof(temp)) != sizeof(temp))
			goto error;

		sprintf(text, "DATA/%08X.LIT", temp);
		if(load_file(text, lightmaps + i * LIGHTMAP_SIZE, LIGHTMAP_SIZE) != LIGHTMAP_SIZE)
		{
			printf("Unable to load %s!\n", text);
			goto error;
		}
	}

	// planes
	for(uint32_t i = 0; i < map_head.count_ptex; i++)
	{
		if(read(fd, &mt, sizeof(mt)) != sizeof(mt))
			goto error;

		sprintf(text, "DATA/%08X.PLN", mt.hash);
		if(load_plane(text, mt.hash, mt.lights))
			goto error;
	}

	// walls
	for(uint32_t i = 0; i < map_head.count_wtex; i++)
	{
		if(read(fd, &mt, sizeof(mt)) != sizeof(mt))
			goto error;

		sprintf(text, "DATA/%08X.WAL", mt.hash);
		if(load_wall(text, mt.hash, mt.lights))
			goto error;
	}

	// textures
	for(uint32_t i = 0; i < map_head.count_textures; i++)
	{
		if(read(fd, &mv, sizeof(mv)) != sizeof(mv))
			goto error;

		texture_remap[i] = find_texture(mv.nhash, mv.vhash);
	}

	// map sectors
	if(read(fd, map_block_data, 256 * 32) != 256 * 32)
		goto error;
	expand_array(map_sectors, sizeof(sector_t), 0);

	// player starts
	if(read(fd, map_block_data, 4096) != 4096)
		goto error;
	expand_array(player_starts, sizeof(player_start_t), 0);

	// wall banks
	for(uint32_t i = 0; i < map_head.count_wall_banks; i++)
	{
		if(read(fd, map_block_data, WALL_BANK_SIZE) != WALL_BANK_SIZE)
			goto error;
		expand_array(map_walls[i], 16, sizeof(wall_t));
	}

	// expand walls
	for(uint32_t i = 0; i < map_head.count_wall_banks; i++)
		expand_walls(i);

	// extra storage
	esbase = wram_used;
	temp = map_head.count_extra_storage * 256;
	dst = get_wram(temp);
	if(!dst)
		goto error;

	if(temp && read(fd, dst, temp) != temp)
		goto error;

	// fix objects
	for(uint32_t i = 1; i < 256; i++)
	{
		sector_t *sec = map_sectors + i;
		uint32_t base;

		if(!sec->sobj_hi)
			continue;

		base = esbase + sec->sobj_lo;
		sec->sobj_lo = base;
		sec->sobj_hi = (base >> 16) | 0x80;
	}

	// things
	for(uint32_t i = 0; i < map_head.count_things; i++)
	{
		map_thing_t th;
		int32_t type;
		uint8_t ti;

		if(read(fd, &th, sizeof(th)) != sizeof(th))
			goto error;

		type = thing_find_type(th.hash);
		if(type < 0)
			goto error;

		if(load_thing_sprites(type, 0))
			goto error;

		ti = thing_spawn((int32_t)th.x << 8, (int32_t)th.y << 8, (int32_t)th.z << 8, (int32_t)th.sector, type, 0);
		thing_ptr(ti)->angle = th.angle;
	}

	// done
	close(fd);
	return 0;

error:
	close(fd);
	return 1;
}

//
// precache

static uint32_t precache()
{
	wram_used = 37 * 8192;
	num_sprites = 0;
	num_sframes = 0;
	num_wframes = 0;

	memset(sprite_remap, 0xFF, sizeof(sprite_remap));

	if(	!(thing_state->menu_logo & 0x80) &&
		load_wspr("DATA/F8845BD5.WPS")
	)
		return 1;

	for(uint32_t i = THING_WEAPON_FIRST; i < 128; i++)
	{
		if(load_thing_sprites(i, 0))
			return 1;
	}

	memcpy(sprite_remap_pc, sprite_remap, sizeof(sprite_remap));

	wram_used_pc = wram_used;
	num_sprites_pc = num_sprites;
	num_sframes_pc = num_sframes;
	num_wframes_pc = num_wframes;

	printf("precache: %u sinfo; %u sfrm; %u wfrm; %u RAM\n", num_sprites, num_sframes, num_wframes, wram_used);

	return 0;
}

//
// random

uint8_t rng_get()
{
	uint32_t ret = rng_idx;
	rng_idx++;
	rng_idx &= 2047;
	return rng_tab[ret];
}

uint8_t rng_val(uint8_t val)
{
	uint8_t mask = rng_mask[val];
	while(1)
	{
		uint8_t rng = rng_get() & mask;
		if(rng <= val)
			return rng;
	}
}

//
// MAIN

int main(int argc, void **argv)
{
	map_texture_info[MAX_TEXTURES].type = 0x81;
	map_texture_info[MAX_TEXTURES].plane.vera_tile_base = 0xFC;
	map_texture_info[MAX_TEXTURES].plane.vera_map_base = 0xFC;

	if(load_tables())
		return 1;

	if(precache())
		return 1;

	if(load_map())
	{
		printf("Unable to load map file!\n");
		return 1;
	}

	prepare_stuff();

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("SDL INIT error\n");
		return 1;
	}

	sdl_win = SDL_CreateWindow("TEST", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_OPENGL);
	sdl_context = SDL_GL_CreateContext(sdl_win);

	SDL_GL_SetSwapInterval(1);

	glViewport(0, 0, WIDTH, HEIGHT);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(0.0f);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);

	glGenTextures(2, texture);
	glBindTexture(GL_TEXTURE_2D, texture[0]);

	thing_spawn_player();

	while(!stopped)
	{
		input();
		render();
	}

	return 0;
}
