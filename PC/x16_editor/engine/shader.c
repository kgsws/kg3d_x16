#include "inc.h"
#include "matrix.h"
#include "shader.h"

#include "../shader_text.inc"

static GLuint shared_uniform[1];

shader_buffer_t shader_buffer;
uint32_t shader_changed;

GLuint shader_attr_vertex;
GLuint shader_attr_coord;

//
// funcs

static uint32_t shader_load_compile(const char *buff, uint32_t size, int type)
{
	uint32_t shader, status;

	shader = glCreateShader(type);
	glShaderSource(shader, 1, (const GLchar**)&buff, &size);

	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if(!status)
	{
		char *text;
		printf("[SHADER] failed to compile\n");
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
		text = malloc(size);
		if(text)
		{
			glGetShaderInfoLog(shader, size, NULL, text);
			printf("-- error --\n%s\n", text);
			free(text);
		}
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

//
// API

uint32_t shader_init()
{
	uint32_t status;
	GLuint program;
	GLuint vtx;
	GLuint frag;
	union
	{
		GLint s;
		GLuint u;
	} idx;

	glGenBuffers(1, shared_uniform);
	glBindBuffer(GL_UNIFORM_BUFFER, shared_uniform[0]);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(shader_buffer_t), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferRange(GL_UNIFORM_BUFFER, 0, shared_uniform[0], 0, sizeof(shader_buffer_t));

	program = glCreateProgram();
	if(!program)
		return 1;

	vtx = shader_load_compile(engine_shader_vertex_glsl, engine_shader_vertex_glsl_len, GL_VERTEX_SHADER);
	frag = shader_load_compile(engine_shader_fragment_glsl, engine_shader_fragment_glsl_len, GL_FRAGMENT_SHADER);

	glAttachShader(program, vtx);
	glAttachShader(program, frag);
	glLinkProgram(program);

	glUseProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if(!status)
	{
		glDeleteProgram(program);
		printf("[SHADER] failed to link program\n");
		return 1;
	}

	// texture slots
	idx.s = glGetUniformLocation(program, "texture");
	if(idx.s >= 0)
		glUniform1i(idx.s, 0);
	idx.s = glGetUniformLocation(program, "palette");
	if(idx.s >= 0)
		glUniform1i(idx.s, 1);
	idx.s = glGetUniformLocation(program, "colormap");
	if(idx.s >= 0)
		glUniform1i(idx.s, 2);
	idx.s = glGetUniformLocation(program, "light");
	if(idx.s >= 0)
		glUniform1i(idx.s, 3);

	// inputs
	shader_attr_vertex = glGetAttribLocation(program, "vertex");
	shader_attr_coord = glGetAttribLocation(program, "coord");

	// info blocks
	idx.u = glGetUniformBlockIndex(program, "shader_stuff");
	if(idx.u != GL_INVALID_INDEX)
		glUniformBlockBinding(program, idx.u, 0);

	glDeleteShader(vtx);
	glDeleteShader(frag);

	shader_buffer.shading.identity[0] = -1.0f;

	return 0;
}

void shader_update()
{
	if(!shader_changed)
		return;

	glBindBuffer(GL_UNIFORM_BUFFER, shared_uniform[0]);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(shader_buffer_t), &shader_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	shader_changed = 0;
}

