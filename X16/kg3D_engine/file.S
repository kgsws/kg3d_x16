.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "tables.inc"

.export file_load_wram
.export file_open_rd
.export file_close
.export file_read_8_wram
.export file_read_16_wram
.export file_read_16_vram
.export file_restore_handle

;;;
; settings
;;;

LOAD_FILE_HANDLE = 1
BLOCK_READ_SIZE = 240
DEVICE_ID = 8

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
	lda	#LOAD_FILE_HANDLE
	ldx	#DEVICE_ID
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
	ldx	#DEVICE_ID
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

file_read_16_vram:
	lda	#<VERA_DATA0
	sta	TAB_FILE_DEST+0
	lda	#>VERA_DATA0
	sta	TAB_FILE_DEST+1

	lda	#$38	; SEC opcode
	sta	read16_loop
	lda	#read16_ai-read16_bcc-2
	sta	read16_bcc+1
	bra	read16_start

file_read_16_wram:
	lda	#$18	; CLC opcode
	sta	read16_loop
	lda	#read16_bi-read16_bcc-2
	sta	read16_bcc+1

read16_start:
	; first block size
	lda	#BLOCK_READ_SIZE
	sta	FILE_LOAD_RDSIZE
	lda	TAB_FILE_BUF_SIZE+1
	bne	:+
	lda	TAB_FILE_BUF_SIZE+0
	beq	:+
	cmp	#BLOCK_READ_SIZE
	bcs	:+
	sta	FILE_LOAD_RDSIZE
:
	; the loop
read16_loop:
	; read
	clc	; modified opcode
	lda	FILE_LOAD_RDSIZE
	ldx	TAB_FILE_DEST+0
	ldy	TAB_FILE_DEST+1
	jsr	k_MACPTR
read16_bcc:
	bcc	read16_bi	; modified offset
	rts
read16_bi:
	; increment address
	lda	TAB_FILE_DEST+0
	adc	FILE_LOAD_RDSIZE
	sta	TAB_FILE_DEST+0
	bcc	read16_ai
	inc	TAB_FILE_DEST+1
	lda	TAB_FILE_DEST+1
	cmp	#$C0
	bne	read16_ai
	lda	#$A0
	sta	TAB_FILE_DEST+1
read16_ai:
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
	cmp	#BLOCK_READ_SIZE
	bcs	read16_loop
	sta	FILE_LOAD_RDSIZE
	bra	read16_loop
@end:
	clc
	rts

