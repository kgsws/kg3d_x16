
typedef union matrix3d_u
{
	float raw[4*4];
	float val[4][4];
} matrix3d_t;

//

extern const matrix3d_t mat_identity;

//

void matrix_perspective(matrix3d_t *mat, float fovy, float aspect, float zNear, float zFar);
void martix_ortho(matrix3d_t *mat, float left, float right, float bottom, float top, float nearVal, float farVal);

void matrix_identity(matrix3d_t *mat);
void matrix_translate(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float x, float y, float z);
void matrix_scale(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float x, float y, float z);
void matrix_rotate_x(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float rad);
void matrix_rotate_y(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float rad);
void matrix_rotate_z(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float rad);

void matrix_mult(matrix3d_t *restrict dst, const matrix3d_t *restrict m0, const matrix3d_t *restrict m1);

void matrix_mult_point(const matrix3d_t *mat, float *pt);

