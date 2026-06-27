;; KG3D engine
; second stage loader

.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "zeropage.inc"
.include "banks.inc"
.include "tables.inc"

COLORS_STATUS = $0F
COLORS_ERROR = $0A
COLORS_INFO = $05

ADDR_L = $F8
ADDR_M = $F9

PTR_L = $3C
PTR_H = $3D
PTR_I = $3E

FILE_ERROR = $3F

VAR_FILE_BUF_SIZE = $E0
VAR_FILE_DEST = $E2

BLOCK_READ_SIZE = 240

DEVICE_ID = 8

FILE_HANDLE = 2
FILE_CHANNEL = 2

;;;

.org $2500

.segment "STARTUP"
.segment "INIT"
.segment "ONCE"
.segment "CODE"

;;;
; main
;;;

start:
	stz	FILE_ERROR

	;; ENGINE CODE

	jsr	read_data
	.byte	LDR_ENGINE_SIZE / 512
	.byte	LDR_ENGINE_DEST / 256

	;; EXTRA CODE

	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	jsr	read_data
	.byte	LDR_EXTRA_SIZE / 512
	.byte	LDR_EXTRA_DEST / 256

	;; SWAPPABLE CODE

	lda	#BANK_SWAP_CODE
	sta	REG_RAM_BANK

	jsr	read_data
	.byte	LDR_SWAP_SIZE / 512
	.byte	LDR_SWAP_DEST / 256

	;; TABLES (WRAM)

	; text: loading tables
	jsr	write_text
	.word	$0400
	.word	text_tabload

	; get sector
	clc
	lda	LDR_GFX_SECTOR+0
	adc	#TAB_GFX_INFO_PAL
	sta	LDR_TEMP_L
	lda	LDR_GFX_SECTOR+1
	adc	#0
	sta	LDR_TEMP_H
	lda	LDR_GFX_SECTOR+2
	adc	#0
	sta	LDR_TEMP_I
	lda	LDR_GFX_SECTOR+3
	adc	#0
	sta	LDR_TEMP_T

	;; GFX info and palette
	lda	#BAMK_GFX_INFO
	sta	REG_RAM_BANK

	jsr	read_data
	.byte	$40
	.byte	$A0

	;; T0

	; open
	jsr	file_open
	bcc	:+
	jmp	error
:
	jsr	file_read
	.byte	TAB_T0_SIZE / 256
	.byte	TAB_T0_ADDR / 256

	;; T1

	lda	#BANK_TABLES_T1
	sta	REG_RAM_BANK

	jsr	file_read
	.byte	TAB_T1_SIZE / 256
	.byte	TAB_T1_ADDR / 256

	; close
	lda	#FILE_HANDLE
	jsr	k_CLOSE

	;; TABLES (VRAM)

	lda	#'V'
	sta	text_tabload+8
	sta	FILE_ERROR

	; text: loading tables
	jsr	write_text
	.word	$0500
	.word	text_tabload

	; setup VRAM clear (using cache)
	lda	#$04
	sta	VERA_CTRL
	lda	#%01000000
	sta	VERA_FX_CTRL
	lda	#$0C
	sta	VERA_CTRL
	stz	VERA_ADDRx_L
	stz	VERA_ADDRx_M
	lda	#%00110000
	sta	VERA_ADDRx_H
	stz	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M
	stz	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; clear loop
	ldy	#$28
@loop_vclr_h:
	ldx	#$00
@loop_vclr_l:
	stz	VERA_DATA0
	dex
	bne	@loop_vclr_l
	dey
	bpl	@loop_vclr_h

	; disable cache
	lda	#$04
	sta	VERA_CTRL
	stz	VERA_FX_CTRL
	stz	VERA_CTRL

	; get sector
	clc
	lda	LDR_GFX_SECTOR+0
	adc	#TAB_GFX_VRAM
	sta	LDR_TEMP_L
	lda	LDR_GFX_SECTOR+1
	adc	#0
	sta	LDR_TEMP_H
	lda	LDR_GFX_SECTOR+2
	adc	#0
	sta	LDR_TEMP_I
	lda	LDR_GFX_SECTOR+3
	adc	#0
	sta	LDR_TEMP_T

	; change opcode
	lda	#$8D
	sta	sd_sta_opcode+0
	lda	#<VERA_DATA0
	sta	sd_sta_opcode+1
	lda	#>VERA_DATA0
	sta	sd_sta_opcode+2

	;; HUD

	jsr	read_secvram
	.word	30 | 4096

	jsr	read_secvram
	.word	62 | 4096

	;; font (first part)

	jsr	read_secvram
	.word	94 | 4096

	jsr	read_secvram
	.word	126 | 4096

	jsr	read_secvram
	.word	158 | 4096

	;; everything else

	; address
	lda	#$E8
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; loop
	lda	#9
	sta	PTR_I
@loop_vram:
	jsr	read_sector
	dec	PTR_I
	bne	@loop_vram

	;; THINGDEF

	; text: starting
	jsr	write_text
	.word	$0600
	.word	text_thgload

	; change name
	lda	#'H'
	sta	text_readfail+16
	lda	#'D'
	sta	text_readfail+17

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; open
	jsr	file_open
	bcc	:+
	jmp	error
:
	; load thing info
	stz	VAR_FILE_DEST+0
	lda	#$A0
	sta	VAR_FILE_DEST+1
	stz	VAR_FILE_BUF_SIZE+0
	lda	#$20
	sta	VAR_FILE_BUF_SIZE+1
	jsr	file_read_16
	bcc	:+
	jmp	error
:
	; load thing states
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK
@loop_sta:
	stz	VAR_FILE_DEST+0
	lda	#$B8
	sta	VAR_FILE_DEST+1
	stz	VAR_FILE_BUF_SIZE+0
	lda	#$08
	sta	VAR_FILE_BUF_SIZE+1
	jsr	file_read_16
	bcc	:+
	jmp	error
:
	inc	REG_RAM_BANK
	lda	REG_RAM_BANK
	cmp	#BANK_THING_STATES+8
	bne	@loop_sta

	; close
	lda	#FILE_HANDLE
	jsr	k_CLOSE

	;; START

	; text: starting
	jsr	write_text
	.word	$0700
	.word	text_starting

	; copy some info values to T0 empty space
	lda	LDR_GFX_SECTOR+0
	sta	TAB_GFX_SECTOR_L
	lda	LDR_GFX_SECTOR+1
	sta	TAB_GFX_SECTOR_M
	lda	LDR_GFX_SECTOR+2
	sta	TAB_GFX_SECTOR_H
	lda	LDR_GFX_SECTOR+3
	sta	TAB_GFX_SECTOR_U

	; disable IRQ
	sei

	; clear VSYNC flag
	lda	#1
	sta	VERA_ISR
	stz	VERA_IEN

	; increment = 2
	lda	#%00100001
	sta	VERA_ADDRx_H

	; second line
	lda	#$01
	sta	VERA_ADDRx_M

	; reset
	lda	#<anim_ent
	sta	PTR_L
	lda	#>anim_ent
	sta	PTR_H

	; animate
	ldx	#43
@loop_anim:
	; line color loop
	lda	#$01
	sta	VERA_ADDRx_L
	ldy	#39
@loop_line:
	lda	(PTR_L),y
	sta	VERA_DATA0
	dey
	bpl	@loop_line

	; vsync wait
@loop_vsync:
	lda	VERA_ISR
	lsr
	bcc	@loop_vsync
	lda	#1
	sta	VERA_ISR

	; next
	dec	PTR_L
	dex
	bne	@loop_anim

	; zero
	stz	PLANE_PTR_TOP_L
	stz	PLANE_PTR_BOT_L
	stz	COLUMN_PTR_L

	; RUN
	jmp	LDR_ENGINE_DEST

.align	128

anim_tab:
	.byte	$0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C
	.byte	$0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C
	.byte	$0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C
	.byte	$0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C
	.byte	$0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0F, $01
anim_ent:
	.byte	$0F, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C
	.byte	$0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C
	.byte	$0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C
	.byte	$0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C

;;;
; errors
;;;

error:
	; reset input
	ldx	#0
	jsr	k_CHKIN

	; address
	lda	ADDR_L
	sta	VERA_ADDRx_L
	lda	ADDR_M
	sta	VERA_ADDRx_M

	; increment = 1
	lda	#%00010001
	sta	VERA_ADDRx_H

	; character
	lda	#'-'
	sta	VERA_DATA0
	lda	#COLORS_STATUS
	sta	VERA_DATA0

	; ERROR with colors
	ldy	#0
	ldx	#COLORS_ERROR
	lda	#<text_error
	sta	PTR_L
	lda	#>text_error
	sta	PTR_H
:
	lda	(PTR_L),y
	beq	:+
	sta	VERA_DATA0
	stx	VERA_DATA0
	iny
	bra	:-
:
	; file error
	lda	FILE_ERROR
	beq	:+

	; text: read fail
	jsr	write_text
	.word	$0A0E
	.word	text_readfail
:
	; text: reboot
	jsr	write_text
	.word	$0E0E
	.word	text_reboot

@key_wait:
	jsr	wait_key

	; reset command
	ldx	#SMC_I2C_ADDR
	ldy	#SMC_CMD_RESET
	lda	#$00
	jsr	k_i2c_write_byte

	; repeat
	bra	@key_wait

;;;
; wait for any key
;;;

wait_key:
	jsr	k_GETIN
	beq	wait_key
	rts

;;;
; 40 character line clear
;;;

clear_line:
	ldy	#5
@loop:
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	dey
	bne	@loop
	rts

;;;
; write
;;;

write_text:
	; text address, increment = 2
	stz	VERA_CTRL
	lda	#%00100001
	sta	VERA_ADDRx_H

	; read stack
	tsx

	; load pointer
	lda	$0101,x
	sta	PTR_L
	lda	$0102,x
	sta	PTR_H

	; advance return address
	clc
	lda	$0101,x
	adc	#4
	sta	$0101,x
	bcc	:+
	inc	$0102,x
:
	; VERA address (lo)
	ldy	#1
	lda	(PTR_L),y
	sta	VERA_ADDRx_L

	; VERA address (hi)
	iny
	lda	(PTR_L),y
	sta	VERA_ADDRx_M

	; text address (lo)
	iny
	lda	(PTR_L),y
	sta	@write_opcode+1

	; text address (hi)
	iny
	lda	(PTR_L),y
	sta	@write_opcode+2

	; actual write
	ldy	#0
@write_opcode:
	lda	$C0DE,y	; modified address
	beq	:+
	sta	VERA_DATA0
	iny
	bne	@write_opcode
:
	; store position (-6)
	sec
	lda	VERA_ADDRx_L
	sbc	#6
	sta	ADDR_L
	lda	VERA_ADDRx_M
	sbc	#0
	sta	ADDR_M

	rts

;;;
; open file
;;;

file_open:
	; name
	lda	#8
	ldx	#<(text_readfail+10)
	ldy	#>(text_readfail+10)
	jsr	k_SETNAM

	; set mode
	lda	#FILE_HANDLE
	ldx	#DEVICE_ID
	ldy	#FILE_CHANNEL
	jsr	k_SETLFS

	; open
	jsr	k_OPEN

	; set handle
	ldx	#FILE_HANDLE
	jmp	k_CHKIN

;;;
; read from file
;;;

file_read:
	; read stack
	tsx

	; load pointer
	lda	$0101,x
	sta	PTR_L
	lda	$0102,x
	sta	PTR_H

	; advance return address
	clc
	lda	$0101,x
	adc	#2
	sta	$0101,x
	bcc	:+
	inc	$0102,x
:
	; block count
	ldy	#1
	lda	(PTR_L),y
	sta	PTR_I

	; destination
	iny
	lda	(PTR_L),y
	sta	PTR_H

	; block loop
@loop:
	; first half
	lda	#$80
	ldx	#$00
	ldy	PTR_H
	clc
	jsr	k_MACPTR
	bcc	:+
	jmp	error
:
	; second half
	lda	#$80
	tax
	ldy	PTR_H
	clc
	jsr	k_MACPTR
	bcc	:+
	jmp	error
:
	; increment
	inc	PTR_H

	; bank check
	lda	#$C0
	cmp	PTR_H
	bne	:+

	; bank change; actual bank increment was done by MACPTR
	lda	#$A0
	sta	PTR_H
:
	; next
	dec	PTR_I
	bne	@loop

	rts

;;;
; SD card read sector
;;;

read_sector:
	; dummy byte
	lda	#$FF
	sta	VERA_SPI_DATA

	; select
	lda	#$01
	sta	VERA_SPI_CTRL

	; sector read command
	lda	#$51
	sta	VERA_SPI_DATA
	lda	LDR_TEMP_T
	sta	VERA_SPI_DATA
	lda	LDR_TEMP_I
	sta	VERA_SPI_DATA
	lda	LDR_TEMP_H
	sta	VERA_SPI_DATA
	lda	LDR_TEMP_L
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
@got_cmd:
	; check
	beq	:+

	; got error
@fail:
	jsr	@stop
	jmp	error
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
	inc	PTR_H
	jsr	@read

	; dummy checksum
	lda	VERA_SPI_DATA
	inc	PTR_H

	; increment sector
	inc	LDR_TEMP_L
	bne	:+
	inc	LDR_TEMP_H
	bne	:+
	inc	LDR_TEMP_I
	bne	:+
	inc	LDR_TEMP_T
:
@stop:
	; deselect
	stz	VERA_SPI_CTRL

	; dummy byte
	lda	#$FF
	sta	VERA_SPI_DATA

	rts

@read:
	ldy	#0
sd_loop_data:
	lda	VERA_SPI_DATA
sd_sta_opcode:
	sta	(PTR_L),y ; modified
	nop	 ; modified
	iny
	bne	sd_loop_data
	rts

;;;
; read data block
;;;

read_data:
	; read stack
	tsx

	; load pointer
	lda	$0101,x
	sta	PTR_L
	lda	$0102,x
	sta	PTR_H

	; advance return address
	clc
	lda	$0101,x
	adc	#2
	sta	$0101,x
	bcc	:+
	inc	$0102,x
:
	; block count
	ldy	#1
	lda	(PTR_L),y
	tax

	; destination
	iny
	lda	(PTR_L),y
	sta	PTR_H
	stz	PTR_L

	; block loop
@loop:
	; read
	jsr	read_sector

	; bank check
	lda	PTR_H
	cmp	#$C0
	bne	:+

	; bank change
	lda	#$A0
	sta	PTR_H
	inc	REG_RAM_BANK
:
	; next
	dex
	bne	@loop

	rts

;;;
; read sector to VRAM
;;;

read_secvram:
	; read stack
	tsx

	; load pointer
	lda	$0101,x
	sta	PTR_L
	lda	$0102,x
	sta	PTR_H

	; advance return address
	clc
	lda	$0101,x
	adc	#2
	sta	$0101,x
	bcc	:+
	inc	$0102,x
:
	; addr
	ldy	#1
	lda	(PTR_L),y
	sta	VERA_ADDRx_M
	iny
	lda	(PTR_L),y
	sta	VERA_ADDRx_H
	stz	VERA_ADDRx_L

	jmp	read_sector

;;;
; read from file (65536 bytes max)
;;;
; VAR_FILE_BUF_SIZE is size to read; 0 is 65536; this value is decremented
; VAR_FILE_DEST is destination pointer, assumend to be in banked area; this value is advanced, with banking support
; C is set on error

file_read_16:
	; first block size
	lda	#BLOCK_READ_SIZE
	sta	FILE_LOAD_RDSIZE
	lda	VAR_FILE_BUF_SIZE+1
	bne	:+
	lda	VAR_FILE_BUF_SIZE+0
	beq	:+
	cmp	#BLOCK_READ_SIZE
	bcs	:+
	sta	FILE_LOAD_RDSIZE
:
	; the loop
read16_loop:
	; read
	clc
	lda	FILE_LOAD_RDSIZE
	ldx	VAR_FILE_DEST+0
	ldy	VAR_FILE_DEST+1
	jsr	k_MACPTR
	bcc	:+
	rts
:
	; increment address
	lda	VAR_FILE_DEST+0
	adc	FILE_LOAD_RDSIZE
	sta	VAR_FILE_DEST+0
	bcc	:+
	inc	VAR_FILE_DEST+1
	lda	VAR_FILE_DEST+1
	cmp	#$C0
	bne	:+
	lda	#$A0
	sta	VAR_FILE_DEST+1
:
	; decrement size
	sec
	lda	VAR_FILE_BUF_SIZE+0
	sbc	FILE_LOAD_RDSIZE
	sta	VAR_FILE_BUF_SIZE+0
	bcs	:+
	dec	VAR_FILE_BUF_SIZE+1
:
	; check size
	lda	VAR_FILE_BUF_SIZE+1
	bne	read16_loop
	lda	VAR_FILE_BUF_SIZE+0
	beq	@end
	cmp	#BLOCK_READ_SIZE
	bcs	read16_loop
	sta	FILE_LOAD_RDSIZE
	bra	read16_loop
@end:
	clc
	rts

;;;
; strings
;;;

text_reboot:
	.asciiz	"Press any key to reboot ..."
text_error:
	.asciiz	" ERROR"
text_readfail:
	.asciiz	"Read from KG3D.TBL failed!"
text_tabload:
	.asciiz	"Loading WRAM tables ..."
text_thgload:
	.asciiz	"Loading thing tables ..."
text_starting:
	.asciiz	"Starting engine ..."
