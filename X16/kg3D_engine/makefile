compiled = code
loader = loader
program = KG3D.PRG
engine = KG3D.BIN
image = /tmp/card.img
file_tbl = ../../PC/x16_editor/data/export/KG3D.TBL
file_gfx = ../../PC/x16_editor/data/export/KG3D.GFX
file_thd = ../../PC/x16_editor/data/export/KG3D.THD
OBJ = main.o file.o math.o video.o render.o map.o tick.o things.o actions.o hitscan.o menu.o hud.o text.o input.o sound.o
CC = cc65
AS = ca65
LD = ld65
CL = cl65
ASFLAGS = --cpu 65SC02

build: ${program} ${loader} ${compiled} ${engine} ${image}

clean:
	rm -f *.o ${compiled} ${program} ${loader} ${engine}

%.o: %.s
	${AS} ${ASFLAGS} $<

%.o: %.S
	${AS} ${ASFLAGS} $<

${program}: program.o
	${CL} -t cx16 program.o -o ${program}

${loader}: loader.o
	${LD} -C loader.cfg loader.o -o ${loader}

${engine}: ${loader} ${compiled}
	cat ${loader} > ${engine}
	cat ${compiled} >> ${engine}

${image}: ${engine} ${program} ${file_tbl} ${file_thd} ${file_gfx}
	cp card.img ${image}
	dd conv=notrunc if=${file_tbl} of=${image} bs=512 seek=100495
	dd conv=notrunc if=${file_thd} of=${image} bs=512 seek=100624
	dd conv=notrunc if=${file_gfx} of=${image} bs=512 seek=2179
	dd conv=notrunc if=${program} of=${image} bs=512 seek=100483
	dd conv=notrunc if=${engine} of=${image} bs=512 seek=2051
	size=`stat -L -c %s ${file_gfx}`; printf "0: 00%.2X%.2X%.2X" $$(((size >> 8) & 255)) $$(((size >> 16) & 255)) $$(((size >> 24) & 255)) | xxd -r -g0 | dd conv=notrunc of=${image} bs=4 seek=262415

${compiled}: ${OBJ}
	${LD} -C engine.cfg ${OBJ} -o ${compiled}

run:
	../../../../x16-emulator/x16emu -sdcard ${image} -ram 2048 -debug -noemucmdkeys

run05m:
	../../../../x16-emulator/x16emu -sdcard ${image} -debug -noemucmdkeys

run10m:
	../../../../x16-emulator/x16emu -sdcard ${image} -ram 1024 -debug -noemucmdkeys

run15m:
	../../../../x16-emulator/x16emu -sdcard ${image} -ram 1536 -debug -noemucmdkeys

run10:
	../../../../x16-emulator/x16emu -sdcard ${image} -ram 2048 -mhz 10 -noemucmdkeys

run12:
	../../../../x16-emulator/x16emu -sdcard ${image} -ram 2048 -mhz 12 -noemucmdkeys

run16:
	../../../../x16-emulator/x16emu -sdcard ${image} -ram 2048 -mhz 16 -noemucmdkeys

