#include "inc.h"
#include "matrix.h"

// TODO: OPTIMIZE!

const matrix3d_t mat_identity =
{
	.raw =
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static matrix3d_t mat_translate =
{
	.raw =
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static matrix3d_t mat_scale =
{
	.raw =
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static matrix3d_t mat_rotate_x =
{
	.raw =
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static matrix3d_t mat_rotate_y =
{
	.raw =
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

static matrix3d_t mat_rotate_z =
{
	.raw =
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}
};

//
//

void matrix_perspective(matrix3d_t *mat, float fovy, float aspect, float zNear, float zFar)
{
	float f, z;

	for(uint32_t i = 0; i < 16; i++)
		mat->raw[i] = 0.0f;

	f = fovy * 0.5f;
	f = cosf(f) / sinf(f);
	z = zNear - zFar;

	mat->val[0][0] = f / aspect;
	mat->val[1][1] = f;
	mat->val[2][2] = (zFar + zNear) / z;
	mat->val[3][2] = (zFar * zNear * 2.0f) / z;
	mat->val[2][3] = -1.0f;
}

void martix_ortho(matrix3d_t *mat, float left, float right, float bottom, float top, float nearVal, float farVal)
{
	float a, b, c;

	for(uint32_t i = 0; i < 16; i++)
		mat->raw[i] = mat_identity.raw[i];

	a = right - left;
	b = top - bottom;
	c = farVal - nearVal;

	mat->val[0][0] = 2.0f / a;
	mat->val[1][1] = 2.0f / b;
	mat->val[2][2] = -2.0f / c;

	mat->val[3][0] = (right + left) / -a;
	mat->val[3][1] = (top + bottom) / -b;
	mat->val[3][2] = (farVal + nearVal) / -c;
}

//
//

void matrix_mult(matrix3d_t *restrict dst, const matrix3d_t *restrict m0, const matrix3d_t *restrict m1)
{
	dst->val[0][0] = m1->val[0][0] * m0->val[0][0] + m1->val[0][1] * m0->val[1][0] + m1->val[0][2] * m0->val[2][0] + m1->val[0][3] * m0->val[3][0];
	dst->val[0][1] = m1->val[0][0] * m0->val[0][1] + m1->val[0][1] * m0->val[1][1] + m1->val[0][2] * m0->val[2][1] + m1->val[0][3] * m0->val[3][1];
	dst->val[0][2] = m1->val[0][0] * m0->val[0][2] + m1->val[0][1] * m0->val[1][2] + m1->val[0][2] * m0->val[2][2] + m1->val[0][3] * m0->val[3][2];
	dst->val[0][3] = m1->val[0][0] * m0->val[0][3] + m1->val[0][1] * m0->val[1][3] + m1->val[0][2] * m0->val[2][3] + m1->val[0][3] * m0->val[3][3];

	dst->val[1][0] = m1->val[1][0] * m0->val[0][0] + m1->val[1][1] * m0->val[1][0] + m1->val[1][2] * m0->val[2][0] + m1->val[1][3] * m0->val[3][0];
	dst->val[1][1] = m1->val[1][0] * m0->val[0][1] + m1->val[1][1] * m0->val[1][1] + m1->val[1][2] * m0->val[2][1] + m1->val[1][3] * m0->val[3][1];
	dst->val[1][2] = m1->val[1][0] * m0->val[0][2] + m1->val[1][1] * m0->val[1][2] + m1->val[1][2] * m0->val[2][2] + m1->val[1][3] * m0->val[3][2];
	dst->val[1][3] = m1->val[1][0] * m0->val[0][3] + m1->val[1][1] * m0->val[1][3] + m1->val[1][2] * m0->val[2][3] + m1->val[1][3] * m0->val[3][3];

	dst->val[2][0] = m1->val[2][0] * m0->val[0][0] + m1->val[2][1] * m0->val[1][0] + m1->val[2][2] * m0->val[2][0] + m1->val[2][3] * m0->val[3][0];
	dst->val[2][1] = m1->val[2][0] * m0->val[0][1] + m1->val[2][1] * m0->val[1][1] + m1->val[2][2] * m0->val[2][1] + m1->val[2][3] * m0->val[3][1];
	dst->val[2][2] = m1->val[2][0] * m0->val[0][2] + m1->val[2][1] * m0->val[1][2] + m1->val[2][2] * m0->val[2][2] + m1->val[2][3] * m0->val[3][2];
	dst->val[2][3] = m1->val[2][0] * m0->val[0][3] + m1->val[2][1] * m0->val[1][3] + m1->val[2][2] * m0->val[2][3] + m1->val[2][3] * m0->val[3][3];

	dst->val[3][0] = m1->val[3][0] * m0->val[0][0] + m1->val[3][1] * m0->val[1][0] + m1->val[3][2] * m0->val[2][0] + m1->val[3][3] * m0->val[3][0];
	dst->val[3][1] = m1->val[3][0] * m0->val[0][1] + m1->val[3][1] * m0->val[1][1] + m1->val[3][2] * m0->val[2][1] + m1->val[3][3] * m0->val[3][1];
	dst->val[3][2] = m1->val[3][0] * m0->val[0][2] + m1->val[3][1] * m0->val[1][2] + m1->val[3][2] * m0->val[2][2] + m1->val[3][3] * m0->val[3][2];
	dst->val[3][3] = m1->val[3][0] * m0->val[0][3] + m1->val[3][1] * m0->val[1][3] + m1->val[3][2] * m0->val[2][3] + m1->val[3][3] * m0->val[3][3];
}

void matrix_identity(matrix3d_t *mat)
{
	for(uint32_t i = 0; i < 16; i++)
		mat->raw[i] = mat_identity.raw[i];
}

void matrix_translate(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float x, float y, float z)
{
	mat_translate.val[3][0] = x;
	mat_translate.val[3][1] = y;
	mat_translate.val[3][2] = z;

	matrix_mult(dst, src, &mat_translate);
}

void matrix_scale(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float x, float y, float z)
{
	mat_scale.val[0][0] = x;
	mat_scale.val[1][1] = y;
	mat_scale.val[2][2] = z;

	matrix_mult(dst, src, &mat_scale);
}

void matrix_rotate_x(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float rad)
{
	float cc, ss;

	cc = cosf(rad);
	ss = sinf(rad);

	mat_rotate_x.val[0][0] = 1;
	mat_rotate_x.val[0][1] = 0;
	mat_rotate_x.val[0][2] = 0;

	mat_rotate_x.val[1][0] = 0;
	mat_rotate_x.val[1][1] = cc;
	mat_rotate_x.val[1][2] = -ss;

	mat_rotate_x.val[2][0] = 0;
	mat_rotate_x.val[2][1] = ss;
	mat_rotate_x.val[2][2] = cc;

	matrix_mult(dst, src, &mat_rotate_x);
}

void matrix_rotate_y(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float rad)
{
	float cc, ss;

	cc = cosf(rad);
	ss = sinf(rad);

	mat_rotate_y.val[0][0] = cc;
	mat_rotate_y.val[0][1] = 0;
	mat_rotate_y.val[0][2] = ss;

	mat_rotate_y.val[1][0] = 0;
	mat_rotate_y.val[1][1] = 1;
	mat_rotate_y.val[1][2] = 0;

	mat_rotate_y.val[2][0] = -ss;
	mat_rotate_y.val[2][1] = 0;
	mat_rotate_y.val[2][2] = cc;

	matrix_mult(dst, src, &mat_rotate_y);
}

void matrix_rotate_z(matrix3d_t *restrict dst, const matrix3d_t *restrict src, float rad)
{
	float cc, ss;

	cc = cosf(rad);
	ss = sinf(rad);

	mat_rotate_z.val[0][0] = cc;
	mat_rotate_z.val[0][1] = ss;
	mat_rotate_z.val[0][2] = 0;

	mat_rotate_z.val[1][0] = -ss;
	mat_rotate_z.val[1][1] = cc;
	mat_rotate_z.val[1][2] = 0;

	mat_rotate_z.val[2][0] = 0;
	mat_rotate_z.val[2][1] = 0;
	mat_rotate_z.val[2][2] = 1;

	matrix_mult(dst, src, &mat_rotate_z);
}

void matrix_mult_point(const matrix3d_t *mat, float *pt)
{
	float tp[2] = {pt[0], pt[1]};

//	w = mat->val[0][3] * pt[0] + mat->val[1][3] * pt[1] + mat->val[2][3] * pt[2] + mat->val[3][3];
	pt[0] = mat->val[0][0] * tp[0] + mat->val[1][0] * tp[1] + mat->val[2][0] * pt[2] + mat->val[3][0];
	pt[1] = mat->val[0][1] * tp[0] + mat->val[1][1] * tp[1] + mat->val[2][1] * pt[2] + mat->val[3][1];
	pt[2] = mat->val[0][2] * tp[0] + mat->val[1][2] * tp[1] + mat->val[2][2] * pt[2] + mat->val[3][2];
}

