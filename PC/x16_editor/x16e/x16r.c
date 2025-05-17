#include "inc.h"
#include "defs.h"
#include "engine.h"
#include "editor.h"
#include "things.h"
#include "list.h"
#include "system.h"
#include "matrix.h"
#include "shader.h"
#include "render.h"
#include "tick.h"
#include "x16g.h"
#include "x16e.h"
#include "x16t.h"
#include "x16r.h"

#define USE_SPECDIV_CLIPPING

#define H_FOV	0x0200

#define PLX_CODE_BASE	(offsetof(tables_1100_t, pxloop) + 0x1100 + 0x0000)
#define PHX_CODE_BASE	(offsetof(tables_1100_t, pxloop) + 0x1100 + 0x0220)
#define PVX_CODE_BASE	(offsetof(tables_1100_t, pxloop) + 0x1100 + 0x0640)
#define PSX_CODE_BASE	(offsetof(tables_1100_t, pxloop) + 0x1100 + 0x0A01)
#define PTX_CODE_BASE	(offsetof(tables_1100_t, pxloop) + 0x1100 + 0x0FD2)
#define COLORMAP_ZP	0x22
#define LIGHTMAP_ZP	0x24
#define VIDEO_PAGE_L	0x50
#define VIDEO_PAGE_H	0x51

#define MAX_PORTALS	64

#define MAX_SPRITES	32
#define MAX_MASKED	16

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
		uint8_t sin_l[320];	// @ 0x1F00
		uint8_t sin_h[320];	// @ 0x2040
		uint8_t pxloop[0x1553];	// @ 0x2180
		// 0x36D3
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
		uint8_t random[2048];	// @ 0xB000 (bank 63)
		uint8_t planex_l[256];	// @ 0xB800 (bank 63) [plane texture stuff]
		uint8_t planex_h[256];	// @ 0xB900 (bank 63) [plane texture stuff]
		uint8_t pitch2yc[256];	// @ 0xBA00 (bank 63)
		// 0xBD00 contains portals (512 bytes)
		// 0xBF00 contains keyboard input table (256 bytes)
	};
} tables_A000_t;

typedef struct
{
	int16_t x, y;
} vertex_t;

typedef struct
{
	int32_t x, y, z;
	uint8_t viewheight;
	int16_t sin, cos;
	int32_t pl_x, pl_y;
	int16_t pl_sin, pl_cos;
	uint16_t a;
	uint16_t lca;
	uint8_t a8;
	uint8_t ycw, ycp;
	uint8_t x0, x1;
	uint8_t x0d, x1d;
	uint8_t ox, oy;
	uint8_t ta, tx;
} projection_t;

typedef struct
{
	kge_sector_t *sector;
	uint8_t x0, x1;
	uint8_t first_sprite;
	uint8_t masked;
} portal_t;

typedef struct
{
	uint8_t clip_top[80];
	uint8_t clip_bot[80];
	uint16_t *data;
	uint32_t *offs;
	uint8_t x0, x1;
	uint8_t next;
	int16_t bot;
	int16_t depth, tex_scale;
	int16_t tex_now, tex_step;
	int16_t dist, scale;
	uint8_t light;
} proj_spr_t;

typedef struct
{
	editor_texture_t *texture;
	int16_t tmap_scale[80];
	uint8_t clip_top[80];
	uint8_t tmap_coord[160];
	uint8_t clip_bot[80];
	int32_t scale_now, scale_step;
	int16_t bz;
	uint8_t x0, x1;
	uint8_t xor, ox;
	uint8_t light;
} proj_msk_t;

//

static kge_thing_t *render_camera;

static uint32_t *palette_data;

// sin / cos
static int16_t tab_sin[256];
static int16_t tab_cos[256];

// inverse division
static int16_t inv_div_raw[65536];
static int16_t *const inv_div = inv_div_raw + 32768;

// texture Z scale
static int16_t tex_scale[4096];

// Y depth projection
static int16_t tab_depth[4096];

// arc tan
static uint16_t atan_tab[4096];

// tan
static int16_t tab_tan[2048];

// angle to X and reverse
static int16_t angle2x[2048];
static uint16_t x2angle[160];

// plane scale
static int16_t tab_planex[256];

// pitch to Y center
static uint16_t pitch2yc[256];

// framebuffer
static uint32_t texture;
uint32_t framebuffer[160 * 120];

// rendering stuff
static vertex_t p2a_coord;
static projection_t projection;
static portal_t portals[MAX_PORTALS];
static portal_t *portal_wr;
static portal_t *portal_rd;
static uint8_t portal_top, portal_bot;
static uint8_t clip_top[80];
static uint8_t clip_bot[80];
static int16_t tmap_scale[80];
static int8_t tmap_coord[160];
static uint8_t planex0[120];
static uint8_t plf_top[80];
static uint8_t plf_bot[80];
static uint8_t plc_top[80];
static uint8_t plc_bot[80];

// sprites
static proj_spr_t proj_spr[MAX_SPRITES];
static uint32_t proj_spr_idx;
static uint8_t proj_spr_first;

// masked
static proj_msk_t proj_msk[MAX_MASKED];
static uint32_t proj_msk_idx;

// texturing
static const uint32_t *tex_d32;
static const void *tex_data;
static uint32_t *tex_offs;
static uint16_t *tex_cmap;
static uint16_t *tex_bright;
static uint8_t *tex_light;
static uint32_t tex_x_start;
static uint32_t tex_y_start;
static uint32_t tex_width;
static uint32_t tex_height;
static int32_t tex_offs_x;
static int32_t tex_offs_y;
static int32_t tex_step_x;
static int32_t tex_step_y;
static uint16_t tex_bmsk;
static uint32_t tex_y_mirror;
static uint32_t tex_swap_xy;
static uint32_t tex_is_sky;

// export
static tables_1100_t tables_1100;
static tables_A000_t tables_A000;

static const uint8_t pxloop[] =
{
	0xAC, 0x24, 0x9F,	// ldy	VERA_DATA1
	0xB1, COLORMAP_ZP,	// lda	(COLORMAP_L),y
	0x8D, 0x23, 0x9F	// sta	VERA_DATA0
};

static const uint8_t pxaddA[] =
{
	0xAD, 0x20, 0x9F,	// lda	VERA_ADDRx_L
	0x69, 0xC0,	// adc	#$C0
	0x8D, 0x20, 0x9F,	// sta	VERA_ADDRx_L
	0xAD, 0x21, 0x9F,	// lda	VERA_ADDRx_M
	0x69, 0x1F,	// adc	#$1F
	0x8D, 0x21, 0x9F,	// sta	VERA_ADDRx_M
};

static const uint8_t pxaddB[] =
{
	0xAD, 0x20, 0x9F,	// lda	VERA_ADDRx_L
	0x65, VIDEO_PAGE_L,	// adc	VIDEO_PAGE_L
	0x8D, 0x20, 0x9F,	// sta	VERA_ADDRx_L
	0xAD, 0x21, 0x9F,	// lda	VERA_ADDRx_M
	0x65, VIDEO_PAGE_H,	// adc	VIDEO_PAGE_H
	0x8D, 0x21, 0x9F,	// sta	VERA_ADDRx_M
};

static const uint8_t pxsky[] =
{
	0xB1, COLORMAP_ZP,	// lda	(COLORMAP_L),y
	0x8D, 0x23, 0x9F,	// sta	VERA_DATA0
	0x88,			// dey
};

static const uint8_t pxspr[] =
{
	0xAC, 0x24, 0x9F,	// ldy	VERA_DATA1
	0xB1, COLORMAP_ZP,	// lda	(COLORMAP_L),y
	0xA8,	// tay
	0xB1, LIGHTMAP_ZP,	// lda	(LIGHTMAP_L),y
	0x8D, 0x23, 0x9F	// sta	VERA_DATA0
};

//
// export

static void *put_code(uint8_t *dst, const uint8_t *src, uint32_t size)
{
	for(uint32_t i = 0; i < size; i++)
		*dst++ = *src++;
	return dst;
}

//
// math

static void sim24bit(int32_t *in)
{
#if 1
	int32_t val = *in & 0x00FFFFFF;

	if(*in < 0)
		val |= 0xFF000000;

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
			} else
			{
				int32_t res;
				res = p2a_coord.x * inv_div[p2a_coord.y];
				res >>= 3;
				ret = 2048 + atan_tab[res];
			}
		} else
		{
			if(p2a_coord.x > p2a_coord.y)
			{
				int32_t res;
				res = p2a_coord.y * inv_div[p2a_coord.x];
				res >>= 3;
				ret = atan_tab[res] + 1024;
			} else
			{
				int32_t res;
				res = p2a_coord.x * inv_div[p2a_coord.y];
				res >>= 3;
				ret = 2048 - atan_tab[res];
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
			} else
			{
				int32_t res;
				res = p2a_coord.x * inv_div[p2a_coord.y];
				res >>= 3;
				ret = 0 - atan_tab[res];
			}
		} else
		{
			if(p2a_coord.x > p2a_coord.y)
			{
				int32_t res;
				res = p2a_coord.y * inv_div[p2a_coord.x];
				res >>= 3;
				ret = 1024 - atan_tab[res];
			} else
			{
				int32_t res;
				res = p2a_coord.x * inv_div[p2a_coord.y];
				res >>= 3;
				ret = atan_tab[res];
			}
		}
	}

	return ret & 4095;
}

//
// texturing

static editor_texture_t *tex_set(kge_x16_tex_t *x16tex, uint8_t flags)
{
	uint8_t anim[3] = {0, 0, 0};
	uint32_t idx = x16tex->idx;
	editor_texture_t *et;
	uint32_t tw;

	tex_bmsk = 0xFFFF;

	tex_is_sky = edit_sky_num >= 0 && (idx == 1);

	et = editor_texture + idx;

	tex_x_start = 0;
	tex_y_mirror = 0;
	tex_swap_xy = 0;

	if(	et->effect &&
		(et->effect[0] & X16G_MASK_PL_EFFECT) == X16G_PL_EFFECT_ANIMATE
	){
		anim[0] = et->effect[2];
		anim[1] = et->effect[1];
		anim[2] = et->effect[3];
	}

	if(et->animate)
	{
		anim[0] = et->animate[0];
		anim[1] = et->animate[1];
		anim[2] = et->animate[2];
	}

	if(anim[0])
	{
		uint32_t etime = gametick / 8;

		etime = etime >> anim[1];
		etime += anim[2];

		idx -= anim[2];
		idx += etime & anim[0];

		if(idx >= editor_texture_count)
			idx = 0;

		et = editor_texture + idx;
	}

	if(flags & 0x80)
	{
		tex_x_start = x16tex->ox;
	} else
	{
		projection.ox = x16tex->ox;
		if(x16tex->flags & TEXFLAG_MIRROR_Y_SWAP_XY)
		{
			tex_y_mirror = et->type == X16G_TEX_TYPE_WALL;
			tex_swap_xy = et->type != X16G_TEX_TYPE_WALL;
		}
	}

	tex_y_start = x16tex->oy;
	if(!(flags & 0x80))
		tex_y_start -= projection.oy;

	if(tex_swap_xy)
	{
		tex_x_start = tex_y_start;
		tex_y_start = 0;
		projection.ta = et->height - 1;
	} else
		projection.ta = et->width - 1;

	projection.tx = 0;

	if(!(x16tex->flags & TEXFLAG_MIRROR_X))
		projection.tx ^= projection.ta;

	tex_cmap = NULL;
	tex_offs = NULL;
	tex_d32 = NULL;
	tex_data = et->data;

	switch(et->type)
	{
		case X16G_TEX_TYPE_PLANE_4BPP:
			tex_cmap = (uint16_t*)x16_colormap_data + et->cmap * 16;
		break;
		case X16G_TEX_TYPE_WALL:
			tex_offs = et->offs;
		break;
		case X16G_TEX_TYPE_PLANE_8BPP:
			tex_bright = x16_palette_bright;
		break;
		default:
			tex_d32 = (uint32_t*)tex_data;
		break;
	}

	if(!idx)
	{
		tex_width = 16;
		tex_height = 16;
	} else
	{
		tex_width = et->width;
		tex_height = et->height;
	}

	return et;
}

static uint32_t tex_read()
{
	uint32_t tx, ty;
	int16_t tile;
	uint8_t sample;

	tx = tex_offs_x >> 9;
	ty = tex_offs_y >> 9;

	tx += tex_x_start;
	ty += tex_y_start;

	tx &= tex_width - 1;
	ty &= tex_height - 1;

	if(tex_y_mirror)
		ty ^= tex_height - 1;

	tex_offs_x += tex_step_x;
	tex_offs_y += tex_step_y;

	if(tex_d32)
		return tex_d32[tx + ty * tex_width];

	if(tex_cmap)
	{
		uint16_t tmp;
		const uint8_t *data = tex_data;
		sample = data[tx + ty * tex_width];
		tmp = tex_cmap[sample];
		sample = tmp & 0xFF;
		if(!(tmp & 0xFF00))
			sample = tex_light[sample];
	} else
	if(tex_offs)
	{
		uint16_t tmp;
		const uint16_t *data = tex_data;
		data += tex_offs[tx];
		tmp = data[ty] & tex_bmsk;
		sample = tmp & 0xFF;
		if(!(tmp & 0xFF00))
			sample = tex_light[sample];
	} else
	{
		const uint8_t *data = tex_data;
		sample = data[tx + ty * tex_width];
		if(!(tex_bright[sample >> 4] & (1 << (sample & 15))))
			sample = tex_light[sample];
	}

	if(!sample)
		return 0;

	return palette_data[sample];
}

static void tex_vline(uint32_t x, int32_t y0, int32_t y1, int32_t tx, int32_t tnow, int32_t step)
{
	uint32_t *dst;

	if(tex_swap_xy)
	{
		tex_offs_y = tx << 9;
		tex_offs_x = tnow << 1;
		tex_step_y = 0;
		tex_step_x = step << 1;
	} else
	{
		tex_offs_x = tx << 9;
		tex_offs_y = tnow << 1;
		tex_step_x = 0;
		tex_step_y = step << 1;
	}

	dst = framebuffer + x;
	dst += y1 * 160;

	for( ; y1 > y0; y1--)
	{
		uint32_t col = tex_read();
		dst -= 160;
		if(col & 0xFF000000)
			*dst = col;
	}
}

static void tex_hline(int16_t height, uint8_t y, uint8_t x0, uint8_t x1)
{
	uint32_t *dst;
	int32_t xnow, xstep;
	int32_t ynow, ystep;
	vertex_t now, part;
	int16_t step;
	int32_t sx;

	y--;
	x0 *= 2;
	x1 *= 2;

	if(y >= 120)
		return;

	sx = (int32_t)x0 - 80;

	step = (height * tab_planex[y + projection.ycp]) >> 7;

	xstep = (step * projection.pl_cos) >> 7;
	ystep = (step * projection.pl_sin) >> 7;

	xnow = xstep * sx + 80 * ystep;
	ynow = ystep * sx - 80 * xstep;

	xnow += projection.pl_x * 256;
	ynow -= projection.pl_y * 256;

	dst = framebuffer + x0;
	dst += y * 160;

	tex_offs_x = xnow;
	tex_offs_y = ynow;
	tex_step_x = xstep;
	tex_step_y = ystep;

	for( ; x0 < x1; x0++)
	{
		uint32_t col = tex_read();
		if(col & 0xFF000000)
			*dst = col;
		dst++;
	}
}

static void tex_sline(uint8_t x, uint8_t y0, uint8_t y1)
{
	uint32_t *dst;
	uint8_t *src;
	uint32_t ys;
	int16_t xx;

	y0--;

	xx = (((x2angle[x] - projection.a) >> 3) + 256) & 0x1FF;

	ys = projection.ycw - y0;
	ys *= 512;

	src = editor_sky[edit_sky_num].data + (511 - xx);

	dst = framebuffer + x;
	dst += y0 * 160;

	for( ; y0 < y1; y0++)
	{
		*dst = palette_data[src[60928 - abs(ys)]];
		dst += 160;
		ys -= 512;
	}
}

//
// sprites

static void prepare_sprite(kge_thing_t *th, uint8_t light)
{
	proj_spr_t *spr;
	vertex_t d0;
	int16_t dist, dscl, xdiff, zdiff;
	uint16_t a0;
	int16_t xc, x0, x1;
	int32_t top, bot;
	int16_t tex_now, tex_step;

	int32_t spr_width = th->sprite.width * 2;
	int32_t spr_height = th->sprite.height;
	int32_t spr_xoffs = th->sprite.width + th->sprite.ox * 2;
	int32_t spr_yoffs = th->sprite.oy * -2;
	int32_t spr_scale = thing_info[th->prop.type].info.scale * 2 + 64;

	if(!th->sprite.data)
		return;

	// limit
	if(proj_spr_idx >= MAX_SPRITES)
		return;

	// diff
	d0.x = (int32_t)th->pos.x - projection.x;
	d0.y = (int32_t)th->pos.y - projection.y;

	// get distance Y
	dist = (d0.x * projection.sin + d0.y * projection.cos) >> 8;

	// reject
	if(dist <= 0)
		return;

	// center angle
	p2a_coord.x = d0.x;
	p2a_coord.y = d0.y;
	a0 = point_to_angle();

	a0 -= projection.a;
	a0 += H_FOV;
	a0 &= 0x07FF;

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
		return;

	// X reject
	if(x1 <= projection.x0d)
		return;

	if(x0 >= projection.x1d)
		return;

	// scale
	spr_yoffs = (spr_yoffs * spr_scale) >> 8;

	// bot
	zdiff = projection.z - ((int32_t)th->pos.z - spr_yoffs);
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

	// signed 8 bit
	if(top < 0)
		top = -1;

	// texturing
	tex_step = (spr_width * (int32_t)inv_div[xdiff]) >> 8;
	tex_now = 0;

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
	spr->bot = bot;
	spr->tex_now = tex_now;
	spr->tex_step = tex_step;
	spr->depth = tex_scale[dist];
	spr->tex_scale = tex_scale[dscl];
	spr->dist = dist;
	spr->scale = spr_scale * 2;
	spr->next = 0xFF;

	// texture
	spr->data = th->sprite.data;
	spr->offs = th->sprite.offs;
	spr->light = th->sprite.bright ? 0 : light;

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
	x1 = spr->x1 / 2;

	for( ; x0 < x1; x0++)
	{
		if(spr->depth < tmap_scale[x0])
			spr->clip_top[x0] = 120;
	}
}

//
// renderer

static void dr_textured_strip(uint8_t x0, uint8_t x1, int32_t top_now, int32_t top_step, int32_t bot_now, int32_t bot_step, uint32_t flags)
{
	uint8_t xx = x0 * 2;

	for( ; x0 < x1; x0++)
	{
		int32_t y0, y1;
		int32_t tnow;
		int16_t clipdiff;

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

		if(tex_is_sky)
		{
			tex_sline(xx, y0 + 1, y1);
			xx++;
			tex_sline(xx, y0 + 1, y1);
			xx++;
		} else
		{
			tex_vline(xx, y0, y1, ((tmap_coord[xx] - projection.ox) & projection.ta) ^ projection.tx, tnow, tmap_scale[x0] >> 2);
			xx++;
			tex_vline(xx, y0, y1, ((tmap_coord[xx] - projection.ox) & projection.ta) ^ projection.tx, tnow, tmap_scale[x0] >> 2);
			xx++;
		}
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

		top_now += top_step;
		bot_now += bot_step;
	}
}

static void dr_mark_top(uint8_t x0, uint8_t x1, int32_t now, int32_t step)
{
	for( ; x0 < x1; x0++)
	{
		int32_t y0;

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

		now += step;
	}
}

static void dr_mark_bot(uint8_t x0, uint8_t x1, int32_t now, int32_t step)
{
	for( ; x0 < x1; x0++)
	{
		int32_t y1;

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

		now += step;
	}
}

static void dr_wall(kge_sector_t *sec, kge_line_t *line, vertex_t *ld, uint32_t x0, uint32_t x1, int16_t s0, int16_t s1)
{
	int32_t scale_now, top_now, bot_now;
	int32_t scale_step, top_step, bot_step;
	int16_t ydiff, xdiff, zdiff;
	uint8_t oldx;
	uint8_t xx = x0;
	uint8_t masked_idx = 0xFF;

	xdiff = x1 - x0;
	ydiff = s1 - s0;

	if(x1 > projection.x1)
		x1 = projection.x1;

	// scale
	scale_step = ((int32_t)ydiff * (int32_t)inv_div[xdiff]) >> 7;
	scale_now = s0 << 8;

	if(scale_step > 0x7FFF)
		scale_step = 0x7FFF;
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
		tmap_scale[x0] = tex_scale[top_now >> 8];
		top_now += top_step;
	}

	for(x0 = xx * 2; x0 < x1 * 2; x0++)
	{
		uint16_t ang;
		uint8_t xoffs;

		ang = (projection.lca + x2angle[x0]) & 2047;
		xoffs = (-1 * ld->x + ld->y * tab_tan[ang]) >> 8;

		tmap_coord[x0] = xoffs;
	}

	projection.oy = 0;

	if(	line->texture[2].idx &&
		proj_msk_idx < MAX_MASKED
	){
		proj_msk_t *mt = proj_msk + proj_msk_idx;
		kge_sector_t *ss = sec;
		uint32_t pli = PLANE_BOT;

		mt->bz = line->texture[2].oy * 2;

		if(line->texture[2].flags & TEXFLAG_PEG_MID_BACK)
			ss = line->backsector;

		mt->scale_now = scale_now;
		mt->scale_step = scale_step;
		mt->x0 = xx;
		mt->x1 = x1;
		mt->texture = editor_texture + line->texture[2].idx;
		mt->bz += ss->plane[PLANE_BOT].height;
		mt->xor = line->texture[2].flags & TEXFLAG_MIRROR_X ? 0x00 : 0xFF;
		mt->ox = line->texture[2].ox;
		mt->light = sec->light.idx;

		memcpy(mt->tmap_scale, tmap_scale, sizeof(tmap_scale));
		memcpy(mt->tmap_coord, tmap_coord, sizeof(tmap_coord));

		masked_idx = proj_msk_idx;

		proj_msk_idx++;
	}

	if(line->backsector || line->texture_split != INFINITY)
	{
		kge_sector_t *bs = line->backsector;
		int16_t diff;

		if(!line->backsector && line->texture_split != INFINITY)
		{
			static kge_sector_t fake_bs;

			fake_bs.plane[PLANE_TOP].height = line->texture_split;
			fake_bs.plane[PLANE_BOT].height = line->texture_split;

			bs = &fake_bs;
		}

		diff = bs->plane[PLANE_BOT].height - sec->plane[PLANE_TOP].height;
		if(diff >= 0)
		{
			if(line->texture[1].flags & TEXFLAG_PEG_Y)
				projection.oy = (int16_t)(sec->plane[PLANE_BOT].height - bs->plane[PLANE_BOT].height) >> 1;
			tex_set(line->texture + 1, 0x40);
			goto do_solid_bot;
		}

		diff = sec->plane[PLANE_BOT].height - bs->plane[PLANE_TOP].height;
		if(diff >= 0)
		{
			projection.oy = diff >> 1;
			if(line->texture[0].flags & TEXFLAG_PEG_Y)
				projection.oy += (int16_t)(bs->plane[PLANE_TOP].height - sec->plane[PLANE_TOP].height) >> 1;
			goto do_solid_top;
		}

		portal_top = 1;
		portal_bot = 1;

		// top
		zdiff = projection.z - sec->plane[PLANE_TOP].height;
		top_step = (scale_step * zdiff) >> 8;
		top_now = (scale_now >> 8) * zdiff;
		top_now += projection.ycw << 8;
		sim24bit(&top_now);
		sim24bit(&top_step);

		if(bs->plane[PLANE_TOP].height < sec->plane[PLANE_TOP].height)
		{
			// masked clip
			if(!(masked_idx & 0x80))
				memcpy(proj_msk[masked_idx].clip_top, clip_top, sizeof(clip_top));

			// bot
			zdiff = projection.z - bs->plane[PLANE_TOP].height;
			bot_step = (scale_step * zdiff) >> 8;
			bot_now = (scale_now >> 8) * zdiff;
			bot_now += projection.ycw << 8;
			sim24bit(&bot_now);
			sim24bit(&bot_step);

			if(line->texture[0].flags & TEXFLAG_PEG_Y)
				projection.oy = (int16_t)(bs->plane[PLANE_TOP].height - sec->plane[PLANE_TOP].height) >> 1;

			// draw
			tex_set(line->texture + 0, 0x00);
			dr_textured_strip(xx, x1, top_now, top_step, bot_now, bot_step, 1);
		} else
		{
			dr_mark_top(xx, x1, top_now, top_step);

			// masked clip
			if(!(masked_idx & 0x80))
				memcpy(proj_msk[masked_idx].clip_top, clip_top, sizeof(clip_top));
		}

		// bot
		zdiff = projection.z - sec->plane[PLANE_BOT].height;
		bot_step = (scale_step * zdiff) >> 8;
		bot_now = (scale_now >> 8) * zdiff;
		bot_now += projection.ycw << 8;
		sim24bit(&bot_now);
		sim24bit(&bot_step);

		if(bs->plane[PLANE_BOT].height > sec->plane[PLANE_BOT].height)
		{
			// masked clip
			if(!(masked_idx & 0x80))
				memcpy(proj_msk[masked_idx].clip_bot, clip_bot, sizeof(clip_bot));

			// top
			zdiff = projection.z - bs->plane[PLANE_BOT].height;
			top_step = (scale_step * zdiff) >> 8;
			top_now = (scale_now >> 8) * zdiff;
			top_now += projection.ycw << 8;
			sim24bit(&top_now);
			sim24bit(&top_step);

			if(line->texture[1].flags & TEXFLAG_PEG_Y)
				projection.oy = (int16_t)(sec->plane[PLANE_BOT].height - bs->plane[PLANE_BOT].height) >> 1;
			else
				projection.oy = 0;

			// draw
			tex_set(line->texture + 1, 0x40);
			dr_textured_strip(xx, x1, top_now, top_step, bot_now, bot_step, 2);
		} else
		{
			dr_mark_bot(xx, x1, bot_now, bot_step);

			// masked clip
			if(!(masked_idx & 0x80))
				memcpy(proj_msk[masked_idx].clip_bot, clip_bot, sizeof(clip_bot));
		}

		// portal
		if(portal_wr - portals < MAX_PORTALS)
		{
			if(	!portal_top && !portal_bot &&
				bs->plane[PLANE_TOP].height > bs->plane[PLANE_BOT].height
			){
				// NOTE: portal merging is missing
				portal_wr->sector = line->backsector;
				portal_wr->x0 = xx;
				portal_wr->x1 = x1;
				portal_wr->masked = masked_idx;
				portal_wr++;

				return;
			}
		}
	} else
	{
		if(line->texture[0].flags & TEXFLAG_PEG_Y)
			projection.oy = (int16_t)(sec->plane[PLANE_BOT].height - sec->plane[PLANE_TOP].height) >> 1;
do_solid_top:
		tex_set(line->texture + 0, 0x00);
do_solid_bot:
		// masked clip
		if(!(masked_idx & 0x80))
		{
			memcpy(proj_msk[masked_idx].clip_top, clip_top, sizeof(clip_top));
			memcpy(proj_msk[masked_idx].clip_bot, clip_bot, sizeof(clip_bot));
		}

		// top
		zdiff = projection.z - sec->plane[PLANE_TOP].height;
		top_step = (scale_step * zdiff) >> 8;
		top_now = (scale_now >> 8) * zdiff;
		top_now += projection.ycw << 8;
		sim24bit(&top_now);
		sim24bit(&top_step);

		// bot
		zdiff = projection.z - sec->plane[PLANE_BOT].height;
		bot_step = (scale_step * zdiff) >> 8;
		bot_now = (scale_now >> 8) * zdiff;
		bot_now += projection.ycw << 8;
		sim24bit(&bot_now);
		sim24bit(&bot_step);

		// draw
		dr_textured_strip(xx, x1, top_now, top_step, bot_now, bot_step, 7);
	}

	// masked portal
	if(	!(masked_idx & 0x80) &&
		portal_wr - portals < MAX_PORTALS
	){
		// dummy portal for masked
		portal_wr->sector = 0;
		portal_wr->first_sprite = 0xFF;
		portal_wr->masked = masked_idx;
		portal_wr++;
	}
}

static void dr_plane(int16_t height, uint8_t *top, uint8_t *bot)
{
	uint8_t x0 = projection.x0;
	uint8_t x1 = projection.x1;
	uint8_t xx = x0 - 1;

	top[xx] = 0xFF;
	top[x1] = 0xFF;

	for( ; x0 <= x1; x0++, xx++)
	{
		uint8_t t0 = top[xx];
		uint8_t b0 = bot[xx];
		uint8_t t1 = top[x0];
		uint8_t b1 = bot[x0];

		while(t0 < t1 && t0 <= b0)
		{
			tex_hline(height, t0, planex0[t0], x0);
			t0++;
		}

		while(b0 > b1 && b0 >= t0)
		{
			tex_hline(height, b0, planex0[b0], x0);
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

static void dr_sky(uint8_t *top, uint8_t *bot)
{
	uint8_t x0 = projection.x0;
	uint8_t x1 = projection.x1;

	for( ; x0 < x1; x0++)
	{
		if(bot[x0] < top[x0])
			continue;

		tex_sline(x0 * 2 + 0, top[x0], bot[x0]);
		tex_sline(x0 * 2 + 1, top[x0], bot[x0]);
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

	tex_bmsk = 0x00FF;
	tex_width = 256;
	tex_height = 256;
	tex_x_start = 0;
	tex_y_start = 0;
	tex_y_mirror = 0;
	tex_cmap = NULL;
	tex_offs = spr->offs;
	tex_d32 = NULL;
	tex_data = spr->data + 2;
	tex_light = x16_light_data + 256 * spr->light;

	for( ; x0 < x1; x0++, tex_now += tex_step)
	{
		uint8_t xx = x0 / 2;
		int32_t tnow = 0;
		int16_t diff;
		uint8_t y0, y1;
		int16_t top;
		int16_t bot;
		uint16_t *src;
		uint8_t len, offs;

		src = spr->data + spr->offs[tex_now >> 8];

		len = *src++;
		if(!len)
			continue;

		offs = *src++;

		bot = spr->bot;
		diff = (offs * spr->scale) >> 8;
		bot -= (spr->dist * diff) >> 8;

		top = bot;
		diff = (len * spr->scale) >> 8;
		top -= (spr->dist * diff) >> 8;
		top--;

		if(top >= spr->clip_bot[xx])
			continue;

		if(bot < spr->clip_top[xx])
			continue;

		if(top < spr->clip_top[xx])
			y0 = spr->clip_top[xx];
		else
			y0 = top;

		diff = spr->clip_bot[xx] - bot;
		if(diff < 0)
		{
			y1 = spr->clip_bot[xx];
			tnow = spr->tex_scale * diff;
			tnow >>= 2;
		} else
			y1 = bot;

		tex_vline(x0, y0, y1, tex_now >> 8, tnow, spr->tex_scale / -4);
	}
}

static void dr_masked(uint32_t idx)
{
	proj_msk_t *mt = proj_msk + idx;
	editor_texture_t *et = mt->texture;
	uint8_t x0, x1;
	const uint16_t *src_data;
	uint32_t *src_offs;
	int32_t scale_now, scale_step;
	int16_t zdiff;

	if(et->type != X16G_TEX_TYPE_WALL_MASKED)
		return;

	x0 = mt->x0 * 2;
	x1 = mt->x1 * 2;

	src_data = et->data;
	src_offs = et->offs;

	tex_bmsk = 0xFFFF;
	tex_width = 256;
	tex_height = 256;
	tex_x_start = 0;
	tex_y_start = 0;
	tex_y_mirror = 0;
	tex_cmap = NULL;
	tex_offs = src_offs;
	tex_d32 = NULL;
	tex_data = src_data + 2;
	tex_light = x16_light_data + 256 * mt->light;

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
		const uint16_t *src;
		uint8_t len, offs;
		uint8_t tx;

		tx = ((mt->tmap_coord[x0] - mt->ox) ^ mt->xor) & (et->width - 1);
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

		tex_vline(x0, y0, y1, tx, tnow, mt->tmap_scale[xx] / -4);

next:
		if(x0 & 1)
			scale_now += scale_step;
	}
}

#ifdef USE_SPECDIV_CLIPPING
int32_t spec_inv_div(int16_t n, int16_t d)
{
	int32_t ret;
	uint32_t count = 0;

	while(d >= 256)
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
#endif

static int16_t fix_effect_value(uint8_t val, uint8_t flip)
{
	int16_t temp = val;
	if(flip & 0x80)
		return -temp;
	return temp;
}

static uint8_t apply_plane_effect(editor_texture_t *et, uint8_t ang)
{
	const uint8_t *effect;
	uint16_t etime;
	int16_t temp;
	uint32_t level_tick;

	if(!et)
		return ang;

	effect = et->effect;
	if(!effect)
		return ang;

	if(!effect[0])
		return ang;

	level_tick = gametick / 8;

	if(effect[1] & 0x80)
		etime = level_tick << (effect[1] & 0x7F);
	else
		etime = level_tick >> effect[1];

	switch(effect[0] & 3)
	{
		case 1: // random
			etime &= 1023;
			if(!(effect[2] & 0x80))
				tex_x_start += tables_A000.random[etime + 0];
			if(!(effect[2] & 0x40))
				tex_y_start += tables_A000.random[etime + 4];
			if(!(effect[2] & 0x01))
				ang += tables_A000.random[etime + 8];
		break;
		case 2: // circle
		case 3: // eight
			temp = fix_effect_value(effect[3], effect[0] << 1);
			tex_y_start += (tab_cos[etime & 0xFF] * temp) >> 8;

			if((effect[0] & 3) == 3)
				etime <<= 1;

			temp = fix_effect_value(effect[2], effect[0]);
			tex_x_start += (tab_sin[etime & 0xFF] * temp) >> 8;

		break;
	}

	return ang;
}

static void do_sector(kge_sector_t *sec, kge_sector_t *origin)
{
	sector_link_t *link;

	// fix plane clip
	plc_top[projection.x0] = 0xF0;
	plf_top[projection.x0] = 0xF0;
	plc_top[projection.x1-1] = 0xF0;
	plf_top[projection.x1-1] = 0xF0;

	// setup light map
	tex_light = x16_light_data + 256 * sec->light.idx;

	// prepare things
	proj_spr_first = 0xFF;
	link = sec->thinglist;
	while(link)
	{
		kge_thing_t *th = link->thing;

		if(th != render_camera)
			prepare_sprite(th, sec->light.idx);

		link = link->next_thing;
	}

	// walls
	for(uint32_t i = 0; i < sec->line_count; i++)
	{
		kge_line_t *line = &sec->line[sec->line_count - i - 1]; // reverse order
		uint16_t a0, a1, ad;
		vertex_t wall_dist;
		vertex_t v0, v1;
		vertex_t d0, d1, dc;
		vertex_t p0, p1;
		vertex_t ld;
		uint8_t x0, x1;
		int16_t s0, s1;
		uint8_t do_left_clip = 0;
		uint8_t do_right_clip = 0;
		uint8_t inside = 0;

		wall_dist.x = line->stuff.normal.y * 256;
		wall_dist.y = -line->stuff.normal.x * 256;
		v0.x = line->vertex[1]->x;
		v0.y = line->vertex[1]->y;
		v1.x = line->vertex[0]->x;
		v1.y = line->vertex[0]->y;

		// V0 diff
		d0.x = v0.x - projection.x;
		d0.y = v0.y - projection.y;

		// get distance Y
		ld.y = (d0.x * wall_dist.y - d0.y * wall_dist.x) >> 8;
		if(ld.y < 0)
			continue;
		inside = ld.y == 0 || ld.y == 1;
		if(!ld.y && sec != origin)
			continue;

		// V0 angle
		p2a_coord.x = d0.x;
		p2a_coord.y = d0.y;
		a0 = point_to_angle();

		// V1 diff
		d1.x = v1.x - projection.x;
		d1.y = v1.y - projection.y;

		// V1 angle
		p2a_coord.x = d1.x;
		p2a_coord.y = d1.y;
		a1 = point_to_angle();

		// side check
		ad = a1 - a0;
		if(ad & 2048)
		{
			if(!inside || !((a0 ^ a1) & 0x800))
				continue;
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
			continue;

		// V1 rejection and clipping checks
		a1 -= projection.a;
		a1 += H_FOV;
		a1 &= 4095;
		if(a1 >= 0xC00)
			continue;
		else
		if(a1 >= 0x400)
			do_right_clip = 1;

		// extra reject
		if(a0 & a1 & 0x800)
			continue;

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
			continue;

		if(x1 <= projection.x0)
			continue;

		if(x0 >= projection.x1)
			continue;

		// project angle
		projection.lca = (line->stuff.x16angle - projection.a) & 4095;

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
#ifndef USE_SPECDIV_CLIPPING
			r = n * inv_div[d];
#else
			r = spec_inv_div(n, d);
#endif
			if(r > 0 && r < 32768)
				p0.y += (dc.y * r) >> 15;
		}

		// right clip
		if(do_right_clip)
		{
			int32_t d, n, r;

			d = dc.x - dc.y;
			n = p1.x - p1.y;
#ifndef USE_SPECDIV_CLIPPING
			r = n * inv_div[d];
#else
			r = spec_inv_div(n, d);
#endif
			if(r > 0 && r < 32768)
				p1.y -= (dc.y * r) >> 15;
		}

		// extra step on PC
		if(p0.y > 4095)
			p0.y = 4095;
		if(p1.y > 4095)
			p1.y = 4095;

		// depth projection
		s0 = tab_depth[p0.y];
		s1 = tab_depth[p1.y];

		// get distance X
		if(line->info.flags & WALLFLAG_PEG_X)
			ld.x = (d1.x * wall_dist.x + d1.y * wall_dist.y) >> 1;
		else
			ld.x = (d0.x * wall_dist.x + d0.y * wall_dist.y) >> 1;

		// draw
		dr_wall(sec, line, &ld, x0, x1, s0, s1);
	}

	// floor
	if(edit_sky_num >= 0 && sec->plane[PLANE_BOT].texture.idx == 1)
		dr_sky(plf_top, plf_bot);
	else
	if(projection.z > (int16_t)sec->plane[PLANE_BOT].height)
	{
		uint8_t ang;
		editor_texture_t *et;

		et = tex_set(&sec->plane[PLANE_BOT].texture, 0x80);
		ang = apply_plane_effect(et, sec->plane[PLANE_BOT].texture.angle);

		projection.pl_x = (projection.x * tab_cos[ang] + projection.y * tab_sin[ang]) >> 8;
		projection.pl_y = (projection.y * tab_cos[ang] - projection.x * tab_sin[ang]) >> 8;

		ang += projection.a8;

		projection.pl_sin = tab_sin[ang];
		projection.pl_cos = tab_cos[ang];

		dr_plane(projection.z - (int16_t)sec->plane[PLANE_BOT].height, plf_top, plf_bot);
	}

	// ceiling
	if(edit_sky_num >= 0 && sec->plane[PLANE_TOP].texture.idx == 1)
		dr_sky(plc_top, plc_bot);
	else
	if(projection.z < (int16_t)sec->plane[PLANE_TOP].height)
	{
		uint8_t ang;
		editor_texture_t *et;

		et = tex_set(&sec->plane[PLANE_TOP].texture, 0x80);
		ang = apply_plane_effect(et, sec->plane[PLANE_TOP].texture.angle);

		projection.pl_x = (projection.x * tab_cos[ang] + projection.y * tab_sin[ang]) >> 8;
		projection.pl_y = (projection.y * tab_cos[ang] - projection.x * tab_sin[ang]) >> 8;

		ang += projection.a8;

		projection.pl_sin = tab_sin[ang];
		projection.pl_cos = tab_cos[ang];

		dr_plane(projection.z - (int16_t)sec->plane[PLANE_TOP].height, plc_top, plc_bot);
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

//
// API

uint32_t x16r_init()
{
	int32_t last;

	//// framebuffer

	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	//// generate tables

	// sin / cos
	for(uint32_t i = 0; i < 256; i++)
	{
		float a = (float)i * ((M_PI*2) / 256.0f);
		tab_sin[i] = sinf(a) * 256.0f;
		tab_cos[i] = cosf(a) * 256.0f;
	}

	// inverse division
	for(int32_t i = -32768; i < 32768; i++)
	{
		int32_t idx = i + 32768;
		if(i)
			inv_div_raw[idx] = 32767 / i;
		else
			inv_div_raw[idx] = 32767;
	}

	// texture scale
	for(uint32_t i = 0; i < 4096; i++)
	{
		float ff = i ? i : 1;

		ff = 131072 / ff;
		ff += 0.5f;

		if(ff > 32768)
			tex_scale[i] = -32768;
		else
			tex_scale[i] = -ff;
	}

	// Y depth projection
	for(uint32_t i = 0; i < 4096; i++)
	{
		uint32_t ii = i > 6 ? i : 6;
		tab_depth[i] = (20500 * (32768 / ii)) >> 15;
	}

	// arc tan
	for(uint32_t i = 0; i < 4096; i++)
		atan_tab[i] = (atanbin(4096, i, -4096) - 1024) & 4095;

	// tan
	for(int32_t i = 0; i < 1024; i++)
	{
		float rad = (float)(i - 1024) * (M_PI / 2048.0f);
		float val = tanf(rad);
		tab_tan[i] = val * 128.0f;
	}
	for(int32_t i = 0; i < 1024; i++)
		tab_tan[i + 1024] = -tab_tan[1023 - i];

	// angle to X
	for(uint32_t i = 0; i < 2048; i++)
	{
		uint32_t iii = i < 1536 ? i + 512 : i - 1536;
		uint32_t ii = iii > 3 ? (iii < 2045 ? iii : 2045) : 3;
		float tf = (float)tab_tan[ii] / -(float)tab_tan[512];
		float xf = tf * 80.0f;
		int16_t xx = 80.0f + xf;
		angle2x[i] = xx;
	}

	// X to angle
#if 0
	last = 0;
	for(uint32_t i = 0; i < 512; i++)
	{
		uint8_t xx = angle2x[i];

		if(xx != angle2x[last])
		{
			float diff = i - last;
			uint16_t an = last + diff * 0.75f;
			x2angle[xx-1] = (512 - an) & 4095;
			last = i;
		}
	}
	for(uint32_t i = 0; i < 80; i++)
		x2angle[80 + i] = 4095 - x2angle[79 - i];
#else
	for(int32_t i = 0; i < 160; i++)
	{
		uint16_t ang = (atanbin(80, 80 - i, -4096) - 1035) & 4095;
		x2angle[i] = ang;
	}
#endif

	// plane X scale
	for(int32_t i = 0; i < 255; i++)
	{
		int32_t ii = -127 + i;
		if(!ii)
			ii = 1;
		tab_planex[i] = 16384 / ii;
	}

	// pitch to Y center
	for(int32_t i = 0; i < 256; i++)
	{
		float rad = (float)(i - 128) * (M_PI / 390.0f);
		float val = tanf(rad) * 128.0f + 60.0f;
		uint32_t ii = (i + 128) & 255;

		if(val < 0)
			val = 0;
		else
		if(val > 119)
			val = 119;

		pitch2yc[ii] = val;
	}

	// random
	for(uint32_t i = 0; i < 2048; i++)
		tables_A000.random[i] = rand(); // TODO: check quality

	return 0;
}

uint16_t x16r_line_angle(kge_line_t *ln)
{
	int16_t dx, dy;

	dx = ln->stuff.normal.y * 256;
	dy = -ln->stuff.normal.x * 256;

	ln->stuff.x16angle = (atanbin(dy, dx, -4096) + 1024) & 4095;
}

void x16r_render(kge_thing_t *camera, float x, float y, float scale)
{
	uint8_t angle = 0 - (camera->pos.angle >> 8);
	uint8_t pitch = camera->pos.pitch >> 8;
	float xx = x + 160 * scale;
	float yy = y + 120 * scale;
	uint32_t clrcol = 0xFF000000 | ((gametick << 1) & 0xFF) * 65793;

	palette_data = x16_palette_data + camera->pos.sector->stuff.palette * 4 * 256;

	render_camera = camera;

	if((pitch & 0x80))
	{
		if(pitch < 0xCA)
			pitch = 0xCA;
	} else
	if(pitch > 0x36)
		pitch = 0x36;

	if(gametick & 0x80)
		clrcol ^= 0x00FFFFFF;
	for(uint32_t i = 0; i < 160 * 120; i++)
		framebuffer[i] = clrcol;

	memset(clip_top, 0, sizeof(clip_top));
	memset(clip_bot, 120, sizeof(clip_bot));
	memset(tmap_scale, 0, sizeof(tmap_scale));

	projection.x = camera->pos.x;
	projection.y = camera->pos.y;
	projection.z = camera->prop.viewz;
	projection.sin = tab_sin[angle];
	projection.cos = tab_cos[angle];
	projection.a = (uint16_t)angle << 4;
	projection.a8 = angle;
	projection.ycw = pitch2yc[pitch];
	projection.ycp = 128 - projection.ycw;

	proj_spr_idx = 0;
	proj_msk_idx = 0;

	portal_rd = portals;
	portal_wr = portals;

	portal_wr->sector = camera->pos.sector;
	portal_wr->x0 = 0;
	portal_wr->x1 = 80;
	portal_wr->masked = 0xFF;
	portal_wr++;

	while(portal_rd < portal_wr)
	{
		if(portal_rd->sector)
		{
			projection.x0 = portal_rd->x0;
			projection.x0d = projection.x0 * 2;
			projection.x1 = portal_rd->x1;
			projection.x1d = projection.x1 * 2;
			do_sector(portal_rd->sector, camera->pos.sector);
		}
		portal_rd++;
	}

//	printf("portals: %u\n", portal_rd - portals);

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

	if(scale <= 0)
		return;

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 160, 120, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);

	gl_vertex_buf[0].x = x;
	gl_vertex_buf[0].y = y;
	gl_vertex_buf[0].z = 0;
	gl_vertex_buf[0].s = 0;
	gl_vertex_buf[0].t = 1;

	gl_vertex_buf[1].x = xx;
	gl_vertex_buf[1].y = y;
	gl_vertex_buf[1].z = 0;
	gl_vertex_buf[1].s = 1;
	gl_vertex_buf[1].t = 1;

	gl_vertex_buf[2].x = x;
	gl_vertex_buf[2].y = yy;
	gl_vertex_buf[2].z = 0;
	gl_vertex_buf[2].s = 0;
	gl_vertex_buf[2].t = 0;

	gl_vertex_buf[3].x = xx;
	gl_vertex_buf[3].y = yy;
	gl_vertex_buf[3].z = 0;
	gl_vertex_buf[3].s = 1;
	gl_vertex_buf[3].t = 0;

	shader_mode_texture_rgb();
	shader_update();

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void x16r_render_8bpp(kge_thing_t *camera, uint8_t *dst)
{
	uint32_t *src = framebuffer;

	x16r_render(camera, 0, 0, 0);

	for(uint32_t i = 0; i < 160 * 120; i++)
		*dst++ = x16g_palette_match(*src++, 0);
}

void x16r_generate()
{
	uint8_t *ptr;

	edit_busy_window("Generating tables ...");

	// sin / cos
	for(uint32_t i = 0; i < 256; i++)
	{
		int16_t val = tab_sin[i];
		tables_1100.sin_l[i] = val;
		tables_1100.sin_h[i] = val >> 8;
	}
	for(uint32_t i = 0; i < 64; i++)
	{
		uint32_t ii = i + 256;
		int16_t val = tab_sin[i];
		tables_1100.sin_l[ii] = val;
		tables_1100.sin_h[ii] = val >> 8;
	}

	// inverse division (hi)
	for(uint32_t i = 0; i < 256; i++)
	{
		int16_t val = inv_div[i];
		tables_1100.idiv_h[i] = val >> 8;
	}

	// Y depth projection (hi)
	for(uint32_t i = 0; i < 256; i++)
	{
		int16_t val = tab_depth[i];
		tables_1100.ydepth_h[i] = val >> 8;
	}

	// X to angle
	for(uint32_t i = 0; i < 160; i++)
	{
		uint16_t val = x2angle[i];
		tables_1100.x2a_l[i] = val;
		tables_1100.x2a_h[i] = val >> 8;
	}

	// nibble swap
	for(uint32_t i = 0; i < 256; i++)
	{
		uint8_t val = (i << 4) | (i >> 4);
		tables_1100.swap[i] = val;
	}

	// 8k bank lookup
	for(uint32_t i = 0; i < 256; i++)
	{
		uint8_t val = i / 32;
		tables_1100.bank[i] = val;
	}

	// Y lookup
	for(uint32_t i = 0; i < 128; i++)
	{
		uint16_t offs = i * 64;
		tables_1100.yoffs_l[i] = offs;
		tables_1100.yoffs_h[i] = offs >> 8;
	}

	// hitscan tan
	for(int32_t i = 1; i < 128; i++)
	{
		float rad = (float)(i - 64) * (M_PI / 128.0f);
		int16_t val = tanf(rad) * 256.0f;
		tables_1100.htan_l[i] = val;
		tables_1100.htan_h[i] = val >> 8;
	}
	tables_1100.htan_l[0] = tables_1100.htan_l[1];
	tables_1100.htan_h[0] = tables_1100.htan_h[1];

	// X lookup
	for(uint32_t i = 0; i < 160; i++)
		tables_1100.xoffs_h[i] = (i & 0xC0) >> 1;

	// pixel jump offsets (vertical)
	// this uses line length as offset
	for(uint32_t i = 0; i < 128; i++)
	{
		uint16_t jmp;
		uint32_t idx = i <= 120 ? i : 0;

		jmp = PVX_CODE_BASE + (120 - idx) * sizeof(pxloop);
		tables_1100.pvxjmp_l[i] = jmp;
		tables_1100.pvxjmp_h[i] = jmp >> 8;
	}

	// pixel jump offsets (horizontal, full detail)
	// this uses pixel X as offset
	for(uint32_t i = 0; i <= 80; i++)
	{
		uint16_t jmp, extra;
		uint32_t idx = i * 2;

		if(idx < 64)
			extra = 0;
		else
		if(idx < 128)
			extra = sizeof(pxaddA);
		else
			extra = sizeof(pxaddA) + sizeof(pxaddB);

		jmp = PHX_CODE_BASE + idx * sizeof(pxloop) + extra;
		tables_1100.phxjmp_l[i] = jmp;
		tables_1100.phxjmp_h[i] = jmp >> 8;
	}

	// pixel jump offsets (horizontal, low detail)
	// this uses pixel X as offset
	for(uint32_t i = 0; i <= 80; i++)
	{
		uint16_t jmp, extra;

		if(i < 32)
			extra = 0;
		else
		if(i < 64)
			extra = sizeof(pxaddA);
		else
			extra = sizeof(pxaddA) + sizeof(pxaddB);

		jmp = PLX_CODE_BASE + i * sizeof(pxloop) + extra;
		tables_1100.plxjmp_l[i] = jmp;
		tables_1100.plxjmp_h[i] = jmp >> 8;
	}

	// pixel jump offsets (sky)
	// this uses pixel X as offset
	for(uint32_t i = 0; i < 256; i++)
	{
		uint16_t jmp;
		uint32_t idx = (119 - i) & 0xFF;

		jmp = PSX_CODE_BASE + idx * sizeof(pxsky);
		tables_1100.psxjmp_l[i] = jmp;
		tables_1100.psxjmp_h[i] = jmp >> 8;
	}

	// pixel jump offsets (thing sprites)
	// this uses line length as offset
	for(uint32_t i = 0; i < 127; i++)
	{
		uint16_t jmp;
		uint32_t idx = i <= 120 ? i : 0;

		jmp = PTX_CODE_BASE + ((120 - idx) + 8) * sizeof(pxspr);
		tables_1100.ptxjmp_l[i] = jmp;
		tables_1100.ptxjmp_h[i] = jmp >> 8;
	}
	tables_1100.ptxjmp_l[127] = PTX_CODE_BASE & 0xFF;
	tables_1100.ptxjmp_h[127] = PTX_CODE_BASE >> 8;

	// pixel loop (horizontal low detail)
	ptr = tables_1100.pxloop;
	for(uint32_t i = 0; i < 64; i++)
	{
		if(i == 32)
			ptr = put_code(ptr, pxaddA, sizeof(pxaddA));

		ptr = put_code(ptr, pxloop, sizeof(pxloop));
	}
	ptr = put_code(ptr, pxaddB, sizeof(pxaddB));
//	printf("plloop end 0x%04X\n", ptr - tables_1100.pxloop);

	// pixel loop (horizontal and vertical)
	for(uint32_t i = 0; i < 248; i++)
	{
		if(i == 64)
			ptr = put_code(ptr, pxaddA, sizeof(pxaddA));
		else
		if(i == 128)
			ptr = put_code(ptr, pxaddB, sizeof(pxaddB));

//		if(i == 128)
//			printf("px split 0x%04X\n", ptr - tables_1100.pxloop);

		ptr = put_code(ptr, pxloop, sizeof(pxloop));
	}
	*ptr++ = 0x60; // RTS
//	printf("pxloop end 0x%04X\n", ptr - tables_1100.pxloop);

	// pixel loop (sky)
	for(uint32_t i = 0; i < 248; i++)
	{
		ptr = put_code(ptr, pxsky, sizeof(pxsky));
		if(i >= 119)
			ptr[-1] = 0xC8; // INY
	}
	*ptr++ = 0x60; // RTS
//	printf("psky end 0x%04X\n", ptr - tables_1100.pxloop);

	// pixel loop (thing sprite)
	for(uint32_t i = 0; i < 128; i++)
		ptr = put_code(ptr, pxspr, sizeof(pxspr));
	*ptr++ = 0x60; // RTS
//	printf("pthg end 0x%04X\n", ptr - tables_1100.pxloop);

	// EXPORT
	edit_save_file(X16_PATH_EXPORT PATH_SPLIT_STR "TABLES0.BIN", tables_1100.raw, sizeof(tables_1100));

	// inverse division (lo)
	for(uint32_t i = 0; i < 8192; i++)
	{
		int16_t val = inv_div[i];
		tables_A000.idiv_l[i] = val;
	}

	// texture scale
	for(uint32_t i = 0; i < 4096; i++)
	{
		int16_t val = tex_scale[i];
		tables_A000.tscale_l[i] = val;
		tables_A000.tscale_h[i] = val >> 8;
	}

	// Y depth projection (lo)
	for(uint32_t i = 0; i < 4096; i++)
	{
		int16_t val = tab_depth[i];
		tables_A000.ydepth_l[i] = val;
	}

	// arc tan
	for(uint32_t i = 0; i < 4096; i++)
	{
		int16_t val = atan_tab[i];
		tables_A000.atan_l[i] = val;
		tables_A000.atan_h[i] = val >> 8;
	}

	// tan
	for(uint32_t i = 0; i < 2048; i++)
	{
		int16_t val = tab_tan[i];
		tables_A000.tan_l[i] = val;
		tables_A000.tan_h[i] = val >> 8;
	}

	// angle to X
	for(uint32_t i = 0; i < 2048; i++)
	{
		int16_t x = angle2x[i];
		tables_A000.a2x_l[i] = x;
		tables_A000.a2x_h[i] = x >> 8;
	}

	// random
	// only generated once

	// plane X
	for(uint32_t i = 0; i < 256; i++)
	{
		tables_A000.planex_l[i] = tab_planex[i];
		tables_A000.planex_h[i] = tab_planex[i] >> 8;
	}

	// pitch to Y center
	for(uint32_t i = 0; i < 256; i++)
		tables_A000.pitch2yc[i] = pitch2yc[i];

	// EXPORT
	edit_save_file(X16_PATH_EXPORT PATH_SPLIT_STR "TABLES1.BIN", tables_A000.raw, sizeof(tables_A000));

	edit_status_printf("Tables generated.");
}

