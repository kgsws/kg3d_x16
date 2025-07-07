program = ENGINE.BIN
loader = KG3D.PRG
OBJ = main.o file.o math.o video.o render.o map.o things.o hitscan.o actions.o menu.o text.o input.o sound.o
CC = cc65
AS = ca65
LD = ld65
CL = cl65
ASFLAGS = --cpu 65SC02

build: ${loader} ${program}

clean:
	rm -f *.o ${program} ${loader}

%.o: %.s
	${AS} ${ASFLAGS} $<

%.o: %.S
	${AS} ${ASFLAGS} $<

${loader}: loader.o
	${CL} -t cx16 loader.o -o ${loader}

${program}: ${OBJ}
	${LD} -C linker.cfg ${OBJ} -o ${program}

run:
	../../../../x16-emulator/x16emu -prg ${loader} -ram 2048 -run -debug -noemucmdkeys

run05m:
	../../../../x16-emulator/x16emu -prg ${loader} -run -debug -noemucmdkeys

run10m:
	../../../../x16-emulator/x16emu -prg ${loader} -ram 1024 -run -debug -noemucmdkeys

run15m:
	../../../../x16-emulator/x16emu -prg ${loader} -ram 1536 -run -debug -noemucmdkeys

run10:
	../../../../x16-emulator/x16emu -prg ${loader} -ram 2048 -run -mhz 10 -noemucmdkeys

run12:
	../../../../x16-emulator/x16emu -prg ${loader} -ram 2048 -run -mhz 12 -noemucmdkeys

run16:
	../../../../x16-emulator/x16emu -prg ${loader} -ram 2048 -run -mhz 16 -noemucmdkeys

load:
	../../../../x16-emulator/x16emu -prg ${loader} -ram 2048 -debug -noemucmdkeys
