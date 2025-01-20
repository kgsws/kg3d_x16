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

in vec4 tex_coord;
out vec4 pixelColor;

uniform sampler2D texture;
uniform sampler2D palette;
uniform sampler2D colormap;
uniform sampler2D light;

void main()
{
	float lidx, cidx, pidx;
	vec2 temp;

	if(	identity.x >= 0 &&
		gl_FragCoord.x == identity.x &&
		gl_FragCoord.y == identity.y
	)
		pixelColor = vec4(identity.z, identity.w, 0.0f, 1.0f);
	else
	switch(mode_fragment)
	{
		case 0: // texture RGB
			pixelColor = texture2D(texture, tex_coord.st);
		break;
		case 1: // texture palette
			pidx = texture2D(texture, tex_coord.st).r * (255.0f / 256.0f) + (1.0f / 1024.0f);
			pixelColor = texture2D(palette, vec2(pidx, 0.0f));
		break;
		case 2: // texture colormap
			cidx = texture2D(texture, tex_coord.st).r * (255.0f / 16.0f);
			pidx = texture2D(colormap, vec2(cidx, cmap)).r * (255.0f / 256.0f) + (1.0f / 1024.0f);
			pixelColor = texture2D(palette, vec2(pidx, 0.0f));
		break;
		case 3: // texture palette with light
			temp = texture2D(texture, tex_coord.st).ra;
			pidx = temp.r * (255.0f / 256.0f) + (1.0f / 1024.0f);
			if(temp.g < 0.5f)
				lidx = texture2D(light, vec2(pidx, lmap)).r * (255.0f / 256.0f) + (1.0f / 1024.0f);
			else
				lidx = pidx;
			pixelColor = texture2D(palette, vec2(lidx, 0.0f));
		break;
		case 4: // texture colormap with light
			cidx = texture2D(texture, tex_coord.st).r * (255.0f / 16.0f);
			temp = texture2D(colormap, vec2(cidx, cmap)).ra;
			pidx = temp.r * (255.0f / 256.0f) + (1.0f / 1024.0f);
			if(temp.g < 0.5f)
				lidx = texture2D(light, vec2(pidx, lmap)).r * (255.0f / 256.0f) + (1.0f / 1024.0f);
			else
				lidx = pidx;
			pixelColor = texture2D(palette, vec2(lidx, 0.0f));
		break;
		case 5: // plain color
			pixelColor = color;
		break;
		default: // colored alpha
			pixelColor = color * vec4(1.0f, 1.0f, 1.0f, texture2D(texture, tex_coord.st).r);
		break;
	}
}

