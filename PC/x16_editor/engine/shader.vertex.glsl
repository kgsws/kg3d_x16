#version 140

layout(std140) uniform shader_stuff
{
	// projection
	mat4 matrix;
	mat4 extra;
	// shading
	vec4 color;
	vec4 identity;
	// texturing
	vec4 scaling;
	mat2 rotate;
	// clipping
	vec4 plane0;
	vec4 plane1;
	// remapping
	float cmap;
	float lmap;
	// mode
	int mode_vertex;
	int mode_fragment;
};

in vec4 vertex;
in vec4 coord;
out vec4 tex_coord;

out float gl_ClipDistance[2];

void main()
{
	switch(mode_vertex)
	{
		case 1: // model
		{
			vec4 vtx = extra * vertex;

			gl_Position = matrix * vertex;
			tex_coord = coord;

			gl_ClipDistance[0] = dot(plane0, vtx);
			gl_ClipDistance[1] = -dot(plane1, vtx);
		}
		break;
		case 2: // plane
			gl_Position = matrix * vertex;
			tex_coord = vec4((vertex.xy * rotate + scaling.zw) / scaling.xy, 0, 0);
			gl_ClipDistance[0] = dot(plane0, vertex);
			gl_ClipDistance[1] = -dot(plane1, vertex);
		break;
		default: // default
			gl_Position = matrix * vertex;
			tex_coord = coord;
			gl_ClipDistance[0] = dot(plane0, vertex);
			gl_ClipDistance[1] = -dot(plane1, vertex);
		break;
	}
}

