
enum
{
	SHADER_FRAGMENT_RGB,
	SHADER_FRAGMENT_PALETTE,
	SHADER_FRAGMENT_COLORMAP,
	SHADER_FRAGMENT_PALETTE_LIGHT,
	SHADER_FRAGMENT_COLORMAP_LIGHT,
	SHADER_FRAGMENT_COLOR,
	SHADER_FRAGMENT_COLOR_ALPHA,
};

enum
{
	SHADER_VERTEX_NORMAL,
	SHADER_VERTEX_SPRITE,
	SHADER_VERTEX_PLANE,
};

//

typedef struct
{
	float matrix[4*4];
	union
	{
		struct
		{
			float origin[4];
			float settings[4];
		};
		float extra[4*4];
	};
} shader_projection_info_t;

typedef struct
{
	float plane0[4];
	float plane1[4];
} shader_clip_info_t;

typedef struct
{
	float color[4];
	float identity[4];
} shader_shading_info_t;

typedef struct
{
	float vec[4];
	float rot[2*4];
} shader_scaling_info_t;

typedef struct
{
	uint32_t vertex;
	uint32_t fragment;
	uint32_t pad2[2];
} shader_mode_t;

typedef struct
{
	shader_projection_info_t projection;
	shader_shading_info_t shading;
	shader_scaling_info_t scaling;
	shader_clip_info_t clipping;
	float colormap;
	float lightmap;
	shader_mode_t mode;
} shader_buffer_t;

//

typedef struct
{
	float origin[4];
	float settings[4];
	float color[4];
	float fog[4];
} settings_and_shading_t;

//

union matrix3d_u;

//
//

extern shader_buffer_t shader_buffer;
extern uint32_t shader_changed;

extern GLuint shader_attr_vertex;
extern GLuint shader_attr_coord;

//
//

static inline void shader_mode_texture_rgb()
{
	shader_buffer.mode.vertex = 0;
	shader_buffer.mode.fragment = SHADER_FRAGMENT_RGB;
	shader_changed = 1;
}

static inline void shader_mode_color()
{
	shader_buffer.mode.vertex = 0;
	shader_buffer.mode.fragment = SHADER_FRAGMENT_COLOR;
	shader_changed = 1;
}

static inline void shader_mode_texture_alpha()
{
	shader_buffer.mode.vertex = 0;
	shader_buffer.mode.fragment = SHADER_FRAGMENT_COLOR_ALPHA;
	shader_changed = 1;
}

static inline void shader_mode_sprite()
{
	shader_buffer.mode.vertex = 0;
	shader_buffer.mode.fragment = SHADER_FRAGMENT_PALETTE_LIGHT;
	shader_changed = 1;
}

//
//

static inline void shader_color32(uint32_t color)
{
	shader_buffer.shading.color[0] = (float)(color & 0xFF) / 255.0f;
	shader_buffer.shading.color[1] = (float)((color >> 8) & 0xFF) / 255.0f;
	shader_buffer.shading.color[2] = (float)((color >> 16) & 0xFF) / 255.0f;
	shader_buffer.shading.color[3] = (float)((color >> 24) & 0xFF) / 255.0f;
}

//
// API

uint32_t shader_init();
void shader_update();

