
#define GLUI_ALIGN_TOP_LEFT	0x00
#define GLUI_ALIGN_LEFT_TOP	0x00
#define GLUI_ALIGN_TOP_CENTER	0x01
#define GLUI_ALIGN_CENTER_TOP	0x01
#define GLUI_ALIGN_TOP_RIGHT	0x02
#define GLUI_ALIGN_RIGHT_TOP	0x02
#define GLUI_ALIGN_CENTER_LEFT	0x04
#define GLUI_ALIGN_LEFT_CENTER	0x04
#define GLUI_ALIGN_CENTER_CENTER	0x05
#define GLUI_ALIGN_CENTER_RIGHT	0x06
#define GLUI_ALIGN_RIGHT_CENTER	0x06
#define GLUI_ALIGN_BOT_LEFT	0x08
#define GLUI_ALIGN_LEFT_BOT	0x08
#define GLUI_ALIGN_BOT_CENTER	0x09
#define GLUI_ALIGN_CENTER_BOT	0x09
#define GLUI_ALIGN_BOT_RIGHT	0x0A
#define GLUI_ALIGN_RIGHT_BOT	0x0A
#define GLUI_ALIGN_LEFT	0x00
#define GLUI_ALIGN_CENTER	0x01
#define GLUI_ALIGN_RIGHT	0x02
#define GLUI_ALIGN_TOP	0x00
#define GLUI_ALIGN_BOT	0x08

union glui_element_u;

typedef void (*glui_draw_func_t)(union glui_element_u*,int32_t,int32_t);
typedef int32_t (*glui_input_func_t)(union glui_element_u*,uint32_t);
typedef int32_t (*glui_click_func_t)(union glui_element_u*,int32_t,int32_t);

typedef struct glui_base_s
{
	glui_draw_func_t draw;
	glui_click_func_t click;
	int16_t x, y;
	uint16_t width, height;
	uint32_t color[2], border_color[2];
	float border_size;
	int32_t custom;
	uint8_t align;
	int8_t disabled;
	uint8_t selected;
} glui_base_t;

typedef struct glui_dummy_s
{
	glui_base_t base;
} glui_dummy_t;

typedef struct glui_text_s
{
	glui_base_t base;
	uint16_t width, height;
	int16_t x, y;
	uint32_t color[2];
	uint32_t gltex;
} glui_text_t;

typedef struct glui_image_s
{
	glui_base_t base;
	uint32_t shader;
	float colormap;
	uint32_t *gltex;
	struct
	{
		float s[2];
		float t[2];
	} coord;
} glui_image_t;

typedef struct glui_container_s
{
	glui_base_t base;
	glui_input_func_t input;
	uint8_t priority;
	uint32_t count;
	union glui_element_u *elements[];
} glui_container_t;

typedef union glui_element_u
{
	glui_base_t base;
	glui_dummy_t dummy;
	glui_text_t text;
	glui_image_t image;
	glui_container_t container;
} glui_element_t;

typedef struct
{
	const void *text;
	const void *font;
	uint32_t align;
	glui_text_t *elm;
} glui_init_text_t;

//

uint32_t glui_init();
uint32_t glui_draw(glui_element_t *root, int32_t mousex, int32_t mousey);
int32_t glui_key_input(glui_element_t *root, uint32_t magic);
int32_t glui_mouse_click(glui_element_t *root, int32_t mousex, int32_t mousey);

void glui_set_text(glui_text_t *ui_text, const uint8_t *text, const void *font, uint32_t align);

void glui_df_dummy(glui_element_t*,int32_t,int32_t);
void glui_df_text(glui_element_t*,int32_t,int32_t);
void glui_df_image(glui_element_t*,int32_t,int32_t);
void glui_df_container(glui_element_t*,int32_t,int32_t);

