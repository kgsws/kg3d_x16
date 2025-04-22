
#define TICKRATE	120	// tics per second

// render
#define ENGINE_FOV	(M_PI * 0.5f)	// TODO: variable
#define MAX_RENDER_RECURSION	32

// movement
#define FRICTION_DEFAULT	0.95f
#define MIN_VELOCITY	0.001f
#define MAX_COLLISION_RECURSION	1024

// ticcmd
#define TICCMD_SPEED_MULT	(1.0f / 32767.0f)

// x16
#define MAX_X16_MAPSTRING	32

//
static inline void apply_4f(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
}

