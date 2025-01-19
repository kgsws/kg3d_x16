#include "inc.h"
#include "image.h"

//
// API

image_t *img_png_load(const char *file, uint32_t paletted)
{
	FILE *f;
	png_structp png_ptr;
	png_infop info_ptr;
	int width, height, bitdepth, colortype, channels;
	png_uint_32 i, rowbytes;
	png_bytep *row_pointers;
	void *data;
	uint8_t temp[8];
	image_t *ret;
	int tx, ty;
	png_colorp palptr;
	int palcount;

	f = fopen(file, "rb");
	if(!f)
		return NULL;

	// PNG check
	fread(temp, 1, sizeof(temp), f);
	if(!png_check_sig(temp, sizeof(temp)))
	{
		fclose(f);
		return NULL;
	}

	// PNG init
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr)
	{
		fclose(f);
		return NULL;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(f);
		return NULL;
	}

	if(setjmp(png_jmpbuf(png_ptr)))
	{
error_exit:
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(f);
		return NULL;
        }

	png_init_io(png_ptr, f);
	png_set_sig_bytes(png_ptr, sizeof(temp));
	png_read_info(png_ptr, info_ptr);

	// get PNG info
	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	bitdepth = png_get_bit_depth(png_ptr, info_ptr);
	colortype = png_get_color_type(png_ptr, info_ptr);
	channels = png_get_channels(png_ptr, info_ptr);

	// check type
	if(colortype == PNG_COLOR_TYPE_PALETTE)
	{
		png_get_PLTE(png_ptr, info_ptr, &palptr, &palcount);
		if(!paletted)
			png_set_palette_to_rgb(png_ptr);
		else
		if(bitdepth < 4 || width & 1)
			goto error_exit;
	} else
	{
		if(paletted)
			goto error_exit;

		if(colortype == PNG_COLOR_TYPE_GRAY)
		{
			if(bitdepth < 8)
				png_set_expand_gray_1_2_4_to_8(png_ptr);
			png_set_gray_to_rgb(png_ptr);
		} else
		if(bitdepth > 8)
			goto error_exit;
		palcount = 0;
	}

	if(!paletted)
	{
		if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(png_ptr);
		else
			png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
		paletted = sizeof(uint32_t);
	} else
		paletted = 1;

	// allocate
	ret = malloc(sizeof(image_t) + width * height * paletted);
	ret->width = width;
	ret->height = height;
	ret->palcount = palcount;
	memcpy(ret->palette, palptr, 3 * palcount);

	// load data
	row_pointers = malloc(sizeof(png_bytep) * height);

	rowbytes = width * paletted;

	for(i = 0; i < height; i++)
		row_pointers[i] = ret->data + i * rowbytes;

	// read data
	png_read_image(png_ptr, row_pointers);

	// fix data
	if(paletted == 1 && bitdepth == 4)
	{
		uint8_t *src = ret->data + width / 2;
		uint8_t *dst = ret->data + width;

		for(uint32_t y = 0; y < height; y++)
		{
			for(uint32_t x = 0; x < width / 2; x++)
			{
				uint8_t pd;

				src--;
				pd = *src;

				dst--;
				*dst = pd & 15;
				dst--;
				*dst = pd >> 4;
			}

			src += width + width / 2;
			dst += width * 2;
		}
	}

	// free the memory
	free(row_pointers);

	// done
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(f);

	return ret;
}

uint32_t img_png_save(const char *file, image_t *img, uint32_t format)
{
	static uint8_t trns[1];
	FILE *f;
	uint32_t ctype, bytes;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte *row_pointers[img->height];

	switch(format & 0xFFFF)
	{
		case IMG_SF_U32:
			bytes = sizeof(uint32_t);
			ctype = PNG_COLOR_TYPE_RGBA;
		break;
		case IMG_SF_U8:
			bytes = sizeof(uint8_t);
			if(format & IMG_SF_PALETTE)
				ctype = PNG_COLOR_TYPE_PALETTE;
			else
				ctype = PNG_COLOR_TYPE_GRAY;
		break;
		default:
			return 1;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr)
		return 1;

	info_ptr = png_create_info_struct(png_ptr);
	if(info_ptr == NULL)
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return 1;
	}

	if(setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return 1;
	}

	f = fopen(file, "wb");
	if(!f)
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return 1;
	}

	if(format & IMG_SF_PALETTE)
		png_set_PLTE(png_ptr, info_ptr, (void*)img->palette, img->palcount);

	if(format & IMG_SF_TRNS)
		png_set_tRNS(png_ptr, info_ptr, trns, sizeof(trns), NULL);

	png_set_IHDR(png_ptr, info_ptr, img->width, img->height, 8, ctype, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	for(uint32_t y = 0; y < img->height; y++)
		row_pointers[y] = img->data + y * img->width * bytes;

	png_init_io(png_ptr, f);
	png_set_rows(png_ptr, info_ptr, row_pointers);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(f);

	return 0;
}

