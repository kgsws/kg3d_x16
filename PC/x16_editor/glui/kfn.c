#include "inc.h"
#include "kfn.h"

// kfn info
typedef struct
{
	uint32_t id;
	uint8_t format;
	uint8_t reserved;
	uint16_t line_height;
	uint16_t num_ranges;
} __attribute__((packed)) kfn_head_t;

// character info
typedef struct
{
	uint16_t w, h;
	int16_t x, y, s;
	uint32_t pixo;
} __attribute__((packed)) kfn_cinfo_t;

// ranges
typedef struct
{
	uint16_t first;
	uint16_t count;
} kfn_range_t;

// parsing
typedef struct
{
	int32_t sx, ex;
	int32_t sy, ey;
} exinfo_t;

//

static const const kfn_head_t *current_font;

//
// KFN

static const kfn_cinfo_t *kfn_get_char(const kfn_head_t *kfn, uint16_t code)
{
	const void *ptr = kfn + 1;

	for(int i = 0; i < kfn->num_ranges; i++)
	{
		if(((kfn_range_t*)ptr)->first > code)
			// character not present
			break;
		if(((kfn_range_t*)ptr)->first + ((kfn_range_t*)ptr)->count < code)
		{
			// character is not in this range
			ptr += sizeof(kfn_range_t) + sizeof(kfn_cinfo_t) * ((kfn_range_t*)ptr)->count;
			continue;
		}
		// character is in this range
		code -= ((kfn_range_t*)ptr)->first;
		return (const kfn_cinfo_t *)(ptr + sizeof(kfn_range_t) + sizeof(kfn_cinfo_t) * code);
	}

	return NULL;
}

static inline void *kfn_get_pixels(const kfn_head_t *kfn, const kfn_cinfo_t *info)
{
	union
	{
		void *ptr;
		uint32_t *u32;
		uint16_t *u16;
	} ptr;
	ptr.ptr = (void*)&info->pixo;
	return (void*)kfn + (ptr.u16[0] | ((uint32_t)ptr.u16[1] << 16));
}

static inline const uint8_t *UTF8(const uint8_t *in, uint16_t *code)
{
	int count;

	if(!(*in & 0x80))
	{
		*code = *in;
		return in+1;
	}

	if((*in & 0xE0) == 0xC0)
	{
		count = 1;
		*code = (*in & 0x1F);
	} else
	if((*in & 0xF0) == 0xE0)
	{
		count = 2;
		*code = (*in & 0x0F);
	} else {
		*code = '?';
		return in+1;
	}

	in++;

	while(count--)
	{
		if(!*in)
		{
			*code = 0;
			return in;
		}
		if((*in & 0xC0) != 0x80)
		{
			*code = '?';
			return in;
		}
		*code <<= 6;
		*code |= *in & 0x3F;
		in++;
	}

	return in;
}

//
// handlers

static void cb_draw(void *stuff, int32_t x, int32_t y, const kfn_cinfo_t *info)
{
	// NOTE: this rendering is not correct - alpha blending is missing
	kfn_extents_t *extents;
	const uint8_t *src;
	uint8_t *dst;
	uint32_t skip;

	if(!info->w || !info->h)
		return;

	extents = stuff;
	src = kfn_get_pixels(current_font, info);

	dst = extents->dest;
	dst += x + info->x;
	dst += (y + info->y) * extents->width;

	skip = extents->width - info->w;

	for(uint32_t y = 0; y < info->h; y++)
	{
		for(uint32_t x = 0; x < info->w; x++)
		{
			if(*src)
				*dst = *src;
			dst++;
			src++;
		}
		dst += skip;
	}
}

static void cb_extents(void *stuff, int32_t x, int32_t y, const kfn_cinfo_t *info)
{
	exinfo_t *exinfo = stuff;

	if(!info->w || !info->h)
		return;

	x += info->x;
	y += info->y;

	if(x < exinfo->sx)
		exinfo->sx = x;
	if(y < exinfo->sy)
		exinfo->sy = y;

	x += info->w;
	y += info->h;

	if(x > exinfo->ex)
		exinfo->ex = x;
	if(y > exinfo->ey)
		exinfo->ey = y;
}

//
// text loop

static int32_t handle_text(const uint8_t *text, const void *font, int32_t x, int32_t y, uint32_t a, void (*cb)(void*,int32_t,int32_t,const kfn_cinfo_t *), void *stuff)
{
	int32_t xx = x;
	const kfn_cinfo_t *info;
	uint16_t code;
	int32_t ret;

	current_font = font;
	ret = y - current_font->line_height;

	if(a & 3)
	{
		while(1)
		{
			const uint8_t *txt = text;
			int32_t w = 0;

			while(1)
			{
				text = UTF8(text, &code);
				if(!code)
					break;

				if(code == '\n' || code == 11)
					break;

				info = kfn_get_char(font, code);
				if(!info)
					continue;

				w += info->s;
			}

			if(a & 1)
				w /= 2;

			xx = x - w;

			text = txt;
			while(1)
			{
				text = UTF8(text, &code);
				if(!code)
					break;

				if(code == '\n' || code == 11)
					break;

				info = kfn_get_char(font, code);
				if(!info)
					continue;

				cb(stuff, xx, y, info);

				xx += info->s;
			}

			xx = x;

			if(code == '\n')
				y += current_font->line_height;
			else
			if(code == 11)
				y += current_font->line_height / 2;
			else
				break;
		}
	} else
	while(1)
	{
		text = UTF8(text, &code);
		if(!code)
			break;

		if(code == '\n' || code == 11)
		{
			xx = x;
			if(code == 11)
				y += current_font->line_height / 2;
			else
				y += current_font->line_height;
			continue;
		}

		info = kfn_get_char(font, code);
		if(!info)
			continue;

		cb(stuff, xx, y, info);

		xx += info->s;
	}

	return y - ret;
}

//
// API

void kfn_text_extents(kfn_extents_t *extents, const uint8_t *text, const void *font, uint32_t align)
{
	uint32_t height;
	exinfo_t exinfo;

	exinfo.sx = 65535;
	exinfo.ex = -65535;
	exinfo.sy = 65535;
	exinfo.ey = -65535;

	extents->width = 0;
	extents->height = 0;

	height = handle_text(text, font, 0, 0, align, cb_extents, &exinfo);

	if(exinfo.sx < exinfo.ex)
	{
		extents->width = exinfo.ex - exinfo.sx;
		extents->width += 7;
		extents->width &= ~7;
	}

	if(exinfo.sy < exinfo.ey)
	{
		extents->height = exinfo.ey - exinfo.sy;
		extents->height += 7;
		extents->height &= ~7;
	}

	extents->ox = exinfo.sx;
	extents->oy = exinfo.sy;

	switch(align & 0xC)
	{
		case 4:
			extents->oh = height / 2;
		break;
		case 8:
			extents->oh = height;
		break;
		default:
			extents->oh = 0;
		break;
	}
}

void kfn_text_render(kfn_extents_t *extents, const uint8_t *text, const void *font, uint32_t align)
{
	handle_text(text, font, -extents->ox, -extents->oy, align, cb_draw, extents);
}

