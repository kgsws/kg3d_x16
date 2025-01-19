
typedef struct
{
	uint16_t width, height;
	int16_t ox, oy, oh;
	uint8_t *dest;
} kfn_extents_t;

//

void kfn_text_extents(kfn_extents_t *extents, const uint8_t *text, const void *font, uint32_t align);
void kfn_text_render(kfn_extents_t *extents, const uint8_t *text, const void *font, uint32_t align);

