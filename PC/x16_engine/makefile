program = test
OBJ = main.o things.o actions.o hitscan.o
LIBS = -lSDL2 -lGL -lGLU -lm -lpng
OPT=-O2 -g
CC=gcc
DEFS=
CFLAGS=${OPT} ${DEFS} -Werror -Wno-address-of-packed-member

build: ${program}

clean:
	rm -f ${program} ${OBJ}

${program}: ${OBJ}
	${CC} ${OBJ} ${LIBS} -o ${program} ${OPT}

