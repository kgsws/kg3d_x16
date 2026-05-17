program = kgedit
MOBJ = main sys_sdl input matrix list engine img_png tick things shader render
XOBJ = x16e x16g x16t x16r x16c
COBJ = kgcbor
EOBJ = editor e2d ewf e3d
UOBJ = glui kfn ui_def fonts
OBJ = $(UOBJ:%=glui/%.o) $(MOBJ:%=engine/%.o) $(EOBJ:%=editor/%.o) $(COBJ:%=kgcbor/%.o) $(XOBJ:%=x16e/%.o)
LIBS = -lSDL2 -lGLEW -lGL -lGLU -lm -lpng
OPT = -O2 -g
CC = gcc
DEFS =
CFLAGS = ${OPT} ${DEFS} -Iengine -Ieditor -Iglui -Ikgcbor -Ix16e -Werror -Wno-address-of-packed-member

build: x16e/x16tex.h glui/fonts.c glui/ui_def.c ${program}

x16e/x16tex.h:
	echo -n "const " > x16e/x16tex.h
	xxd -i x16e/tex_sky.data >> x16e/x16tex.h
	echo -n "const " >> x16e/x16tex.h
	xxd -i x16e/tex_unk.data >> x16e/x16tex.h
	echo -n "const " >> x16e/x16tex.h
	xxd -i x16e/tex_bad.data >> x16e/x16tex.h
	echo -n "const " >> x16e/x16tex.h
	xxd -i x16e/spr_none.data >> x16e/x16tex.h
	echo -n "const " >> x16e/x16tex.h
	xxd -i x16e/spr_point.data >> x16e/x16tex.h
	echo -n "const " >> x16e/x16tex.h
	xxd -i x16e/spr_bad.data >> x16e/x16tex.h
	echo -n "const " >> x16e/x16tex.h
	xxd -i x16e/spr_st_coop.data >> x16e/x16tex.h
	echo -n "const " >> x16e/x16tex.h
	xxd -i x16e/spr_st_dm.data >> x16e/x16tex.h
	xxd -i x16e/spr_start.data >> x16e/x16tex.h

glui/fonts.c:
	echo -n "const " > glui/fonts.c
	xxd -i glui/font_huge.kfn >> glui/fonts.c
	echo -n "const " >> glui/fonts.c
	xxd -i glui/font_medium.kfn >> glui/fonts.c
	echo -n "const " >> glui/fonts.c
	xxd -i glui/font_small.kfn >> glui/fonts.c
	echo -n "const " >> glui/fonts.c
	xxd -i glui/font_tiny.kfn >> glui/fonts.c

glui/ui_def.c: glui/ui_def.json
	./glui/glui_conv.py glui/ui_def.json glui/ui_def

clean:
	rm -f ${program} ${OBJ} shader_text.inc glui/ui_def.c glui/ui_def.h glui/fonts.c x16e/x16tex.h

shader_text.inc: engine/shader.fragment.glsl engine/shader.vertex.glsl
	xxd -i engine/shader.fragment.glsl > shader_text.inc
	xxd -i engine/shader.vertex.glsl >> shader_text.inc

${program}: shader_text.inc ${OBJ}
	${CC} ${OBJ} ${LIBS} -o ${program} ${OPT}

