.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "main.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "tables.inc"

.export file_load_wram
.export file_open_rd
.export file_close
.export file_read_8_wram
.export file_read_16_wram
.export file_restore_handle
.export file_read_sector
.export file_read_sectors

;;;
; CODE
;;;

.segment "CODE_MAP_FS"

;;;
; load file to WRAM
;;;
; TAB_FILE_DEST is destination pointer
; X and Y is file name (lo/hi)
; A is file name length

file_load_wram:
	jsr	k_SETNAM
	lda	#FILE_LOAD_HANDLE
	ldx	#FILE_DEVICE_ID
	ldy	#2
	jsr	k_SETLFS
	lda	#0
	ldx	TAB_FILE_DEST+0
	ldy	TAB_FILE_DEST+1
	jmp	k_LOAD

;;;
; open file (read)
;;;
; TAB_FILE_HANDLE is file handle
; X and Y is file name (lo/hi)
; A is file name length

file_open_rd:
	; set file name
	jsr	k_SETNAM

	; set open mode
	lda	TAB_FILE_HANDLE
	ldx	#FILE_DEVICE_ID
	tay
	jsr	k_SETLFS

	; open file
	jsr	k_OPEN

	; redirect output
file_restore_handle:
	ldx	TAB_FILE_HANDLE
	jmp	k_CHKIN

;;;
; close file
;;;

file_close:
	lda	TAB_FILE_HANDLE
	jmp	k_CLOSE

;;;
; read from file (255 bytes max)
;;;
; A is read size
; X and Y is destination (lo/hi)
; C is set on error

file_read_8_wram:
	clc
	jmp	k_MACPTR

;;;
; read from file (65536 bytes max)
;;;
; TAB_FILE_BUF_SIZE is size to read; 0 is 65536; this value is decremented
; TAB_FILE_DEST is destination pointer, assumend to be in banked area; this value is advanced, with banking support
; C is set on error

file_read_16_wram:
	; first block size
	lda	#FILE_BLOCK_READ
	sta	FILE_LOAD_RDSIZE
	lda	TAB_FILE_BUF_SIZE+1
	bne	:+
	lda	TAB_FILE_BUF_SIZE+0
	beq	:+
	cmp	#FILE_BLOCK_READ
	bcs	:+
	sta	FILE_LOAD_RDSIZE
:
	; the loop
read16_loop:
	; read
	clc
	lda	FILE_LOAD_RDSIZE
	ldx	TAB_FILE_DEST+0
	ldy	TAB_FILE_DEST+1
	jsr	k_MACPTR
	bcc	:+
	rts
:
	; increment address
	lda	TAB_FILE_DEST+0
	adc	FILE_LOAD_RDSIZE
	sta	TAB_FILE_DEST+0
	bcc	:+
	inc	TAB_FILE_DEST+1
	lda	TAB_FILE_DEST+1
	cmp	#$C0
	bne	:+
	lda	#$A0
	sta	TAB_FILE_DEST+1
:
	; decrement size
	sec
	lda	TAB_FILE_BUF_SIZE+0
	sbc	FILE_LOAD_RDSIZE
	sta	TAB_FILE_BUF_SIZE+0
	bcs	:+
	dec	TAB_FILE_BUF_SIZE+1
:
	; check size
	lda	TAB_FILE_BUF_SIZE+1
	bne	read16_loop
	lda	TAB_FILE_BUF_SIZE+0
	beq	@end
	cmp	#FILE_BLOCK_READ
	bcs	read16_loop
	sta	FILE_LOAD_RDSIZE
	bra	read16_loop
@end:
	clc
	rts

;;;
; SD card read sector
;;;
; TMP_PTR_* - destination

file_read_sector:
	; dummy byte
	lda	#$FF
	sta	VERA_SPI_DATA

	; select
	lda	#$01
	sta	VERA_SPI_CTRL

	; sector read command
	lda	#$51
	sta	VERA_SPI_DATA
	lda	SD_SEC_U
	sta	VERA_SPI_DATA
	lda	SD_SEC_H
	sta	VERA_SPI_DATA
	lda	SD_SEC_M
	sta	VERA_SPI_DATA
	lda	SD_SEC_L
	sta	VERA_SPI_DATA
	nop
	stz	VERA_SPI_DATA

	; wait for response
	lda	#$FF
	sta	VERA_SPI_DATA
	lda	#$05
	sta	VERA_SPI_CTRL
@loop_cmd:
	lda	VERA_SPI_DATA
	bmi	@loop_cmd

	; check
	beq	:+

	; got error
@fail:
	jsr	@stop
	sec
	rts
:
	; wait for data
@loop_dw:
	lda	VERA_SPI_DATA
	ina
	beq	@loop_dw

	; check
	ina
	bne	@fail

	; read 512 bytes
	jsr	@read
	inc	TMP_PTR_H
	jsr	@read

	; dummy checksum
	lda	VERA_SPI_DATA
	inc	TMP_PTR_H

	; increment sector
	inc	SD_SEC_L
	bne	:+
	inc	SD_SEC_M
	bne	:+
	inc	SD_SEC_H
	bne	:+
	inc	SD_SEC_U
:
@stop:
	; deselect
	stz	VERA_SPI_CTRL

	; dummy byte
	lda	#$FF
	sta	VERA_SPI_DATA

	clc
	rts

@read:
	ldy	#0
@loop_data:
	lda	VERA_SPI_DATA
	sta	(TMP_PTR_L),y
	iny
	bne	@loop_data
	rts

;;;
; read data block
;;;
; X - sector count
; A - HI of destination

file_read_sectors:
	; destination
	sta	TMP_PTR_H
	stz	TMP_PTR_L

	; block loop
@loop:
	; read
	jsr	file_read_sector
	bcc	:+

	; fail
	rts
:
	; bank check
	lda	TMP_PTR_H
	cmp	#$C0
	bne	:+

	; bank change
	lda	#$A0
	sta	TMP_PTR_H
	inc	REG_RAM_BANK
:
	; next
	dex
	bne	@loop

	clc
	rts

