
enum
{
	IMG_SF_U32,
	IMG_SF_U8,
};

#define IMG_SF_PALETTE	0x40000000
#define IMG_SF_TRNS	0x80000000

typedef struct image_s
{
	uint16_t width, height;
	uint16_t palcount;
	uint8_t palette[768];
	uint8_t data[];
} image_t;

//

image_t *img_png_load(const char *file, uint32_t paletted);
uint32_t img_png_save(const char *file, image_t *img, uint32_t format);

