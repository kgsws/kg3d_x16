;; KG3D engine
; first stage loader

.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "zeropage.inc"
.include "banks.inc"
.include "tables.inc"

COLORS_TITLE = $67
COLORS_ALERT = $0C
COLORS_STATUS = $0F
COLORS_ERROR = $0A
COLORS_WARN = $08

DEVICE_ID = 8

FILE_HANDLE = 2
FILE_CHANNEL = 2

SYS_HANDLE = 1
SYS_CHANNEL = 15

VAR_SIZE = $30
VAR_FIRST = $34
VAR_LAST = $38

PTR_L = $3C
PTR_H = $3D

FILE_ERROR = $3E
FRAG_ERROR = $3F

SYS_TXT_ADDR = $6C

;;;

.org $080D

.segment "STARTUP"
.segment "INIT"
.segment "ONCE"
.segment "CODE"

;;;
; main
;;;

start:
	;; ENTRY
	stz	FILE_ERROR
	stz	FRAG_ERROR

	; reset stack
	ldx	#$FF
	txs

	;; CHARACTER SET

	; copy character set to VRAM
	stz	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$22
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H
	ldy	#6
	ldx	#0
@loop_vram_tile:
	lda	dos_font,x	; modified opcode
	sta	VERA_DATA0
	inx
	bne	@loop_vram_tile
	inc	@loop_vram_tile+2
	dey
	bne	@loop_vram_tile

	;; SCREEN CELAR

	; setup VRAM clear (using cache)
	lda	#$04
	sta	VERA_CTRL
	lda	#%01000000
	sta	VERA_FX_CTRL
	lda	#$0C
	sta	VERA_CTRL
	lda	#%00110001
	sta	VERA_ADDRx_H

	; setup clear character
	lda	#' '
	sta	VERA_FX_CACHE_L
	sta	VERA_FX_CACHE_H

	; setup clear colors
	lda	#COLORS_TITLE
	sta	VERA_FX_CACHE_M
	sta	VERA_FX_CACHE_U

	; clear first line (title)
	stz	VERA_ADDRx_L
	stz	VERA_ADDRx_M
	jsr	clear_line

	; setup clear colors
	lda	#COLORS_ALERT
	sta	VERA_FX_CACHE_M
	sta	VERA_FX_CACHE_U

	; clear second line (alert)
	stz	VERA_ADDRx_L
	inc	VERA_ADDRx_M
	jsr	clear_line

	; setup clear colors
	lda	#COLORS_STATUS
	sta	VERA_FX_CACHE_M
	sta	VERA_FX_CACHE_U

	; clear lines (status)
	ldx	#12
@loop_clear:
	inc	VERA_ADDRx_M
	stz	VERA_ADDRx_L
	jsr	clear_line
	dex
	bne	@loop_clear

	; setup clear colors
	lda	#COLORS_WARN
	sta	VERA_FX_CACHE_M
	sta	VERA_FX_CACHE_U

	; clear last line (warn)
	inc	VERA_ADDRx_M
	stz	VERA_ADDRx_L
	jsr	clear_line

	; disable cache
	lda	#$04
	sta	VERA_CTRL
	stz	VERA_FX_CTRL
	stz	VERA_CTRL

	; text: title
	jsr	write_text
	.word	$000C
	.word	text_title

	; text: alert
	jsr	write_text
	.word	$010E
	.word	text_alert

	;; SET VIDEO

	; backup video mode
	lda	VERA_DC_VIDEO
	and	#%00001111
	sta	REG_VIDEO_MODE
	and	#2
	asl
	asl
	tax

	; disable all layers
	sta	VERA_DC_VIDEO

	; setup layer 1
	lda	#%00100000
	sta	VERA_L1_CONFIG
	lda	#%10000000
	sta	VERA_L1_MAPBASE
	lda	#%10010010
	sta	VERA_L1_TILEBASE
	stz	VERA_L1_HSCROLL_L
	stz	VERA_L1_HSCROLL_H
	stz	VERA_L1_VSCROLL_L
	stz	VERA_L1_VSCROLL_H

	; dcsel = 1, addrsel = 0
	lda	#$02
	sta	VERA_CTRL

	; set offset
	lda	video_table+0,x
	sta	VERA_DC_HSTART
	lda	video_table+1,x
	sta	VERA_DC_HSTOP
	lda	video_table+2,x
	sta	VERA_DC_VSTART
	lda	video_table+3,x
	sta	VERA_DC_VSTOP

	; dcsel = 0, addrsel = 0
	stz	VERA_CTRL

	; set scale
	lda	video_table+4,x
	sta	VERA_DC_HSCALE
	lda	video_table+5,x
	sta	VERA_DC_VSCALE

	; enable layer 1
	lda	REG_VIDEO_MODE
	ora	#%00100000
	sta	VERA_DC_VIDEO

	;; SPRITES

	; ADDR0 = 0x01FFFF, decrement = 1
	stz	VERA_CTRL
	lda	#$FF
	sta	VERA_ADDRx_L
	lda	#$FF
	sta	VERA_ADDRx_M
	lda	#%00011001
	sta	VERA_ADDRx_H

	; setup sprites
	ldy	#48
@loop_spr:
	lda	sprite_data-1,y
	sta	VERA_DATA0
	dey
	bne	@loop_spr

	;; VERSION CHECK

	stz	REG_ROM_BANK
	lda	$FF80
	cmp	#48
	bcs	:+

	; text: version
	jsr	write_text
	.word	$0710
	.word	text_version

	jmp	error_plain
:
	cmp	#49
	bcs	:+

	; patch sector read
	stz	sys_read_fl+1
:
	;; FILE CHECKER

	; text: checking files
	jsr	write_text
	.word	$0200
	.word	text_check

	;; CHECK GFX

	jsr	file_check
	bcc	:+
	jmp	error
:
	; save sector
	lda	VAR_FIRST+0
	sta	LDR_GFX_SECTOR+0
	lda	VAR_FIRST+1
	sta	LDR_GFX_SECTOR+1
	lda	VAR_FIRST+2
	sta	LDR_GFX_SECTOR+2
	lda	VAR_FIRST+3
	sta	LDR_GFX_SECTOR+3

	;; CHECK ENGINE

	lda	#'B'
	sta	file_name+5
	sta	text_miss+10
	sta	text_frag0+10
	lda	#'I'
	sta	file_name+6
	sta	text_miss+11
	sta	text_frag0+11
	lda	#'N'
	sta	file_name+7
	sta	text_miss+12
	sta	text_frag0+12

	jsr	file_check
	bcc	:+
	jmp	error
:
	; save sector
	lda	VAR_FIRST+0
	sta	LDR_BIN_SECTOR+0
	sta	LDR_TEMP_L
	lda	VAR_FIRST+1
	sta	LDR_BIN_SECTOR+1
	sta	LDR_TEMP_H
	lda	VAR_FIRST+2
	sta	LDR_BIN_SECTOR+2
	sta	LDR_TEMP_I
	lda	VAR_FIRST+3
	sta	LDR_BIN_SECTOR+3
	sta	LDR_TEMP_T

	;; LOADER

	; text: loading binary
	jsr	write_text
	.word	$0300
	.word	text_load

	;; READ

	ldx	#<(LDR_LOADER_SIZE / 512)
	lda	#<LDR_LOADER_ADDR
	sta	PTR_L
	lda	#>LDR_LOADER_ADDR
	sta	PTR_H
@loop_read:
	; read
	jsr	read_sector

	; next
	inc	LDR_TEMP_L
	bne	:+
	inc	LDR_TEMP_H
	bne	:+
	inc	LDR_TEMP_I
	bne	:+
	inc	LDR_TEMP_T
:
	dex
	bne	@loop_read

	;; RUN
	jmp	LDR_LOADER_ADDR

;;;
; error handlers
;;;

error:
	; restore input
	ldx	#0
	jsr	k_CHKIN

	; decrement = 2
	lda	#%00101001
	sta	VERA_ADDRx_H

	; backspace
	lda	VERA_DATA0
	lda	VERA_DATA0
	lda	VERA_DATA0

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
	sta	LDR_TEMP_L
	lda	#>text_error
	sta	LDR_TEMP_H
:
	lda	(LDR_TEMP_L),y
	beq	:+
	sta	VERA_DATA0
	stx	VERA_DATA0
	iny
	bra	:-
:
	; fragmentation error
	lda	FRAG_ERROR
	beq	:+

	; text: fragmented
	jsr	write_text
	.word	$060C
	.word	text_frag0
	jsr	write_text
	.word	$080E
	.word	text_frag1
	jsr	write_text
	.word	$090E
	.word	text_frag2
:
	; file error
	lda	FILE_ERROR
	beq	:+

	; text: missing
	jsr	write_text
	.word	$0710
	.word	text_miss
:
error_plain:
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
	rts

;;;
; get file sector and check clusters
;;;

file_check:
	; name
	lda	#(file_name_end-file_name)
	ldx	#<file_name
	ldy	#>file_name
	jsr	k_SETNAM

	; set mode
	lda	#FILE_HANDLE
	ldx	#DEVICE_ID
	ldy	#FILE_CHANNEL
	jsr	k_SETLFS

	; open
	jsr	k_OPEN
	bcc	:+
@fail_open:
	; epic fail
	inc	FILE_ERROR
	rts
@fail_frag:
	; fail
	inc	FRAG_ERROR
	rts
:
	; get file size
	jsr	sys_read
	.word	file_info_t
	.byte	<(file_info_t_end - file_info_t)
	.byte	SYS_TXT_ADDR+19
	.byte	VAR_SIZE
	bcs	@fail_open

	; read first sector
	jsr	sys_read_fl
	.word	file_info_fl
	.byte	<(file_info_fl_end - file_info_fl)
	.byte	SYS_TXT_ADDR+10
	.byte	VAR_FIRST
	bcs	@fail_open

	; calculate seek offset (lazy)
	sec
	lda	VAR_SIZE+0
	sta	file_info_p+2
	lda	VAR_SIZE+1
	sbc	#2
	sta	file_info_p+3
	lda	VAR_SIZE+2
	sbc	#0
	sta	file_info_p+4
	lda	VAR_SIZE+3
	sbc	#0
	sta	file_info_p+5

	; seek command
	jsr	sys_read
	.word	file_info_p
	.byte	<(file_info_p_end - file_info_p)
	.word	0
	bcs	@fail_open

	; read last sector
	jsr	sys_read_fl
	.word	file_info_fl
	.byte	<(file_info_fl_end - file_info_fl)
	.byte	SYS_TXT_ADDR+10
	.byte	VAR_LAST
	bcs	@fail_open

	; get sector count
	sec
	lda	VAR_LAST+0
	sbc	VAR_FIRST+0
	sta	LDR_TEMP_L
	lda	VAR_LAST+1
	sbc	VAR_FIRST+1
	sta	LDR_TEMP_H
	lda	VAR_LAST+2
	sbc	VAR_FIRST+2
	sta	LDR_TEMP_I
	bpl	:+
	jmp	@fail_frag
:
	lda	VAR_LAST+3
	sbc	VAR_FIRST+3
	beq	:+
	jmp	@fail_frag
:
	;
	inc	LDR_TEMP_L
	bne	:+
	inc	LDR_TEMP_H
	bne	:+
	inc	LDR_TEMP_I
	bne	:+
	inc	LDR_TEMP_T
:
	; convert to size
	asl	LDR_TEMP_L
	rol	LDR_TEMP_H
	rol	LDR_TEMP_I

	; compare to size
	lda	LDR_TEMP_L
	cmp	VAR_SIZE+1
	beq	:+
	jmp	@fail_frag
:
	lda	LDR_TEMP_H
	cmp	VAR_SIZE+2
	beq	:+
	jmp	@fail_frag
:
	lda	LDR_TEMP_I
	cmp	VAR_SIZE+3
	beq	:+
	jmp	@fail_frag
:
	; close
	lda	#FILE_HANDLE
	jsr	k_CLOSE

	; pass
	clc
	rts

;;;
; read from system file
;;;

sys_read_fl:
	bra	sys_read

	; read stack
	tsx

	; load pointer
	lda	$0101,x
	sta	LDR_TEMP_L
	lda	$0102,x
	sta	LDR_TEMP_H

	; advance return address
	clc
	lda	$0101,x
	adc	#5
	sta	$0101,x
	bcc	:+
	inc	$0102,x
:
	; destination
	ldy	#5
	lda	(LDR_TEMP_L),y
	sta	LDR_TEMP_L
	stz	LDR_TEMP_H

	; ROM r48
	stz	REG_RAM_BANK
	ldy	#0
	lda	$B7E1
	sta	(LDR_TEMP_L),y
	iny
	lda	$B7E2
	sta	(LDR_TEMP_L),y
	iny
	lda	$B7E3
	sta	(LDR_TEMP_L),y
	iny
	lda	$B7E4
	sta	(LDR_TEMP_L),y

	clc
	rts

sys_read:
	; read stack
	tsx

	; load pointer
	lda	$0101,x
	sta	LDR_TEMP_L
	lda	$0102,x
	sta	LDR_TEMP_H

	; advance return address
	clc
	lda	$0101,x
	adc	#5
	sta	$0101,x
	bcc	:+
	inc	$0102,x
:
	; address (lo)
	ldy	#1
	lda	(LDR_TEMP_L),y
	tax

	; address (hi)
	iny
	lda	(LDR_TEMP_L),y
	pha

	; length
	iny
	lda	(LDR_TEMP_L),y

	; setup file name
	ply
	jsr	k_SETNAM

	; setup file transfer
	lda	#SYS_HANDLE
	ldx	#DEVICE_ID
	ldy	#SYS_CHANNEL
	jsr	k_SETLFS

	; open
	jsr	k_OPEN

	; set handle
	ldx	#SYS_HANDLE
	jsr	k_CHKIN

	; read input
	lda	#SYS_TXT_ADDR
	sta	@opctxt+1
@loop:
	jsr	k_BASIN
@opctxt:
	sta	SYS_TXT_ADDR	; modified address
	inc	@opctxt+1
	bpl	@loop

	; close
	lda	#SYS_HANDLE
	jsr	k_CLOSE

	; check
	lda	SYS_TXT_ADDR+0
	cmp	#'0'
	bne	@bad
	lda	SYS_TXT_ADDR+1
	cmp	#'0'
	beq	@good
	cmp	#'7'
	beq	@good
@bad:
	sec
	rts
@good:
	; destination
	ldy	#5
	lda	(LDR_TEMP_L),y
	beq	@pass
	tax

	; source
	dey
	lda	(LDR_TEMP_L),y
	tay
	sec
	sbc	#8
	sta	@opcend+1

	; loop
	stz	LDR_TEMP_T
@loop_conv:
	; convert first
	lda	$00,y
	jsr	@conv_hex
	sta	LDR_TEMP_I

	; convert second
	dey
	lda	$00,y
	jsr	@conv_hex

	; save
	asl
	asl
	asl
	asl
	ora	LDR_TEMP_I
	tsb	LDR_TEMP_T
	sta	$00,x
	inx

	; next
	dey
@opcend:
	cpy	#$00	; modified value
	bne	@loop_conv

	; check
	lda	LDR_TEMP_T
	bne	@pass

	; fail
	sec
	rts
@pass:
	; pass
	clc
	rts

@conv_hex:
	sec
	sbc	#48
	cmp	#17
	bcc	:+
	sec
	sbc	#7
:
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

@stop:
	; deselect
	stz	VERA_SPI_CTRL

	; dummy byte
	lda	#$FF
	sta	VERA_SPI_DATA

	rts

@read:
	ldy	#0
@loop_data:
	lda	VERA_SPI_DATA
	sta	(PTR_L),y
	iny
	bne	@loop_data
	rts

;;;
; EGA font 8x16
;;;

dos_font:
	.byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $18, $3C, $3C, $3C, $18, $18, $18, $00, $18, $18, $00, $00, $00, $00
	.byte	$00, $C6, $C6, $C6, $44, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $6C, $6C, $FE, $6C, $6C, $6C, $FE, $6C, $6C, $00, $00, $00, $00
	.byte	$18, $18, $7C, $C6, $C2, $C0, $7C, $06, $06, $86, $C6, $7C, $18, $18, $00, $00
	.byte	$00, $00, $00, $00, $C2, $C6, $0C, $18, $30, $60, $C6, $86, $00, $00, $00, $00
	.byte	$00, $00, $38, $6C, $6C, $38, $76, $DC, $CC, $CC, $CC, $76, $00, $00, $00, $00
	.byte	$00, $30, $30, $30, $60, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $0C, $18, $30, $30, $30, $30, $30, $30, $18, $0C, $00, $00, $00, $00
	.byte	$00, $00, $30, $18, $0C, $0C, $0C, $0C, $0C, $0C, $18, $30, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $66, $3C, $FF, $3C, $66, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $18, $18, $7E, $18, $18, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $18, $18, $18, $30, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $00, $00, $FE, $00, $00, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $18, $18, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $02, $06, $0C, $18, $30, $60, $C0, $80, $00, $00, $00, $00
	.byte	$00, $00, $38, $6C, $C6, $C6, $D6, $D6, $C6, $C6, $6C, $38, $00, $00, $00, $00
	.byte	$00, $00, $18, $38, $78, $18, $18, $18, $18, $18, $18, $7E, $00, $00, $00, $00
	.byte	$00, $00, $7C, $C6, $06, $0C, $18, $30, $60, $C0, $C6, $FE, $00, $00, $00, $00
	.byte	$00, $00, $7C, $C6, $06, $06, $3C, $06, $06, $06, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $0C, $1C, $3C, $6C, $CC, $FE, $0C, $0C, $0C, $1E, $00, $00, $00, $00
	.byte	$00, $00, $FE, $C0, $C0, $C0, $FC, $06, $06, $06, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $38, $60, $C0, $C0, $FC, $C6, $C6, $C6, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $FE, $C6, $06, $0C, $18, $30, $30, $30, $30, $30, $00, $00, $00, $00
	.byte	$00, $00, $7C, $C6, $C6, $C6, $7C, $C6, $C6, $C6, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $7C, $C6, $C6, $C6, $7E, $06, $06, $06, $0C, $78, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $18, $18, $00, $00, $00, $18, $18, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $18, $18, $00, $00, $00, $18, $18, $30, $00, $00, $00, $00
	.byte	$00, $00, $00, $06, $0C, $18, $30, $60, $30, $18, $0C, $06, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $7E, $00, $00, $7E, $00, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $60, $30, $18, $0C, $06, $0C, $18, $30, $60, $00, $00, $00, $00
	.byte	$00, $00, $7C, $C6, $C6, $0C, $18, $18, $18, $00, $18, $18, $00, $00, $00, $00
	.byte	$00, $00, $00, $7C, $C6, $C6, $DE, $DE, $DE, $DC, $C0, $7C, $00, $00, $00, $00
	.byte	$00, $00, $10, $38, $6C, $C6, $C6, $FE, $C6, $C6, $C6, $C6, $00, $00, $00, $00
	.byte	$00, $00, $FC, $66, $66, $66, $7C, $66, $66, $66, $66, $FC, $00, $00, $00, $00
	.byte	$00, $00, $3C, $66, $C2, $C0, $C0, $C0, $C0, $C2, $66, $3C, $00, $00, $00, $00
	.byte	$00, $00, $F8, $6C, $66, $66, $66, $66, $66, $66, $6C, $F8, $00, $00, $00, $00
	.byte	$00, $00, $FE, $66, $62, $68, $78, $68, $60, $62, $66, $FE, $00, $00, $00, $00
	.byte	$00, $00, $FE, $66, $62, $68, $78, $68, $60, $60, $60, $F0, $00, $00, $00, $00
	.byte	$00, $00, $3C, $66, $C2, $C0, $C0, $DE, $C6, $C6, $66, $3A, $00, $00, $00, $00
	.byte	$00, $00, $C6, $C6, $C6, $C6, $FE, $C6, $C6, $C6, $C6, $C6, $00, $00, $00, $00
	.byte	$00, $00, $3C, $18, $18, $18, $18, $18, $18, $18, $18, $3C, $00, $00, $00, $00
	.byte	$00, $00, $1E, $0C, $0C, $0C, $0C, $0C, $CC, $CC, $CC, $78, $00, $00, $00, $00
	.byte	$00, $00, $E6, $66, $66, $6C, $78, $78, $6C, $66, $66, $E6, $00, $00, $00, $00
	.byte	$00, $00, $F0, $60, $60, $60, $60, $60, $60, $62, $66, $FE, $00, $00, $00, $00
	.byte	$00, $00, $C6, $EE, $FE, $FE, $D6, $C6, $C6, $C6, $C6, $C6, $00, $00, $00, $00
	.byte	$00, $00, $C6, $E6, $F6, $FE, $DE, $CE, $C6, $C6, $C6, $C6, $00, $00, $00, $00
	.byte	$00, $00, $7C, $C6, $C6, $C6, $C6, $C6, $C6, $C6, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $FC, $66, $66, $66, $7C, $60, $60, $60, $60, $F0, $00, $00, $00, $00
	.byte	$00, $00, $7C, $C6, $C6, $C6, $C6, $C6, $C6, $D6, $DE, $7C, $0C, $0E, $00, $00
	.byte	$00, $00, $FC, $66, $66, $66, $7C, $6C, $66, $66, $66, $F6, $00, $00, $00, $00
	.byte	$00, $00, $7C, $C6, $C6, $60, $38, $0C, $06, $C6, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $7E, $7E, $5A, $18, $18, $18, $18, $18, $18, $3C, $00, $00, $00, $00
	.byte	$00, $00, $C6, $C6, $C6, $C6, $C6, $C6, $C6, $C6, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $C6, $C6, $C6, $C6, $C6, $C6, $C6, $6C, $38, $10, $00, $00, $00, $00
	.byte	$00, $00, $C6, $C6, $C6, $C6, $D6, $D6, $D6, $FE, $EE, $6C, $00, $00, $00, $00
	.byte	$00, $00, $C6, $C6, $6C, $7C, $38, $38, $7C, $6C, $C6, $C6, $00, $00, $00, $00
	.byte	$00, $00, $66, $66, $66, $66, $3C, $18, $18, $18, $18, $3C, $00, $00, $00, $00
	.byte	$00, $00, $FE, $C6, $86, $0C, $18, $30, $60, $C2, $C6, $FE, $00, $00, $00, $00
	.byte	$00, $00, $3C, $30, $30, $30, $30, $30, $30, $30, $30, $3C, $00, $00, $00, $00
	.byte	$00, $00, $00, $80, $C0, $E0, $70, $38, $1C, $0E, $06, $02, $00, $00, $00, $00
	.byte	$00, $00, $3C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $0C, $3C, $00, $00, $00, $00
	.byte	$10, $38, $6C, $C6, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $FF, $00, $00
	.byte	$30, $30, $18, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $78, $0C, $7C, $CC, $CC, $CC, $76, $00, $00, $00, $00
	.byte	$00, $00, $E0, $60, $60, $78, $6C, $66, $66, $66, $66, $7C, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $7C, $C6, $C0, $C0, $C0, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $1C, $0C, $0C, $3C, $6C, $CC, $CC, $CC, $CC, $76, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $7C, $C6, $FE, $C0, $C0, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $1C, $36, $32, $30, $78, $30, $30, $30, $30, $78, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $76, $CC, $CC, $CC, $CC, $CC, $7C, $0C, $CC, $78, $00
	.byte	$00, $00, $E0, $60, $60, $6C, $76, $66, $66, $66, $66, $E6, $00, $00, $00, $00
	.byte	$00, $00, $18, $18, $00, $38, $18, $18, $18, $18, $18, $3C, $00, $00, $00, $00
	.byte	$00, $00, $06, $06, $00, $0E, $06, $06, $06, $06, $06, $06, $66, $66, $3C, $00
	.byte	$00, $00, $E0, $60, $60, $66, $6C, $78, $78, $6C, $66, $E6, $00, $00, $00, $00
	.byte	$00, $00, $38, $18, $18, $18, $18, $18, $18, $18, $18, $3C, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $EC, $FE, $D6, $D6, $D6, $D6, $C6, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $DC, $66, $66, $66, $66, $66, $66, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $7C, $C6, $C6, $C6, $C6, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $DC, $66, $66, $66, $66, $66, $7C, $60, $60, $F0, $00
	.byte	$00, $00, $00, $00, $00, $76, $CC, $CC, $CC, $CC, $CC, $7C, $0C, $0C, $1E, $00
	.byte	$00, $00, $00, $00, $00, $DC, $76, $66, $60, $60, $60, $F0, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $7C, $C6, $60, $38, $0C, $C6, $7C, $00, $00, $00, $00
	.byte	$00, $00, $10, $30, $30, $FC, $30, $30, $30, $30, $36, $1C, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $CC, $CC, $CC, $CC, $CC, $CC, $76, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $CC, $CC, $CC, $CC, $CC, $78, $30, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $C6, $C6, $D6, $D6, $D6, $FE, $6C, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $C6, $6C, $38, $38, $38, $6C, $C6, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $00, $C6, $C6, $C6, $C6, $C6, $C6, $7E, $06, $0C, $F8, $00
	.byte	$00, $00, $00, $00, $00, $FE, $CC, $18, $30, $60, $C6, $FE, $00, $00, $00, $00
	.byte	$00, $00, $0E, $18, $18, $18, $70, $18, $18, $18, $18, $0E, $00, $00, $00, $00
	.byte	$00, $00, $18, $18, $18, $18, $00, $18, $18, $18, $18, $18, $00, $00, $00, $00
	.byte	$00, $00, $70, $18, $18, $18, $0E, $18, $18, $18, $18, $70, $00, $00, $00, $00
	.byte	$00, $00, $76, $DC, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
	.byte	$00, $00, $00, $00, $10, $38, $6C, $C6, $C6, $C6, $FE, $00, $00, $00, $00, $00

;;;
; sprite data (framebuffers for bitmap)
;;;

sprite_data:
	;;;
	.word	$8200
	.word	128
	.word	0
	.word	$F00C
	;;;
	.word	$8280
	.word	128
	.word	64
	.word	$F00C
	;;;
	.word	$8100
	.word	64
	.word	0
	.word	$F00C
	;;;
	.word	$8180
	.word	64
	.word	64
	.word	$F00C
	;;;
	.word	$8000
	.word	0
	.word	0
	.word	$F00C
	;;;
	.word	$8080
	.word	0
	.word	64
	.word	$F00C

;;;
; video scale and offset
;;;

video_table:
	; VGA
	.byte	0
	.byte	160
	.byte	0
	.byte	240
	.byte	64
	.byte	64
	.byte	0, 0
	; composite
	.byte	9
	.byte	151
	.byte	13
	.byte	226
	.byte	72
	.byte	72
	.byte	0, 0

;;;
; strings
;;;

file_name:
	.byte	"KG3D.GFX,?,M"
file_name_end:

file_info_t:
	.byte	"T"
	.byte	FILE_CHANNEL
file_info_t_end:

file_info_p:
	.byte	"P"
	.byte	FILE_CHANNEL
	.byte	$00
	.byte	$00
	.byte	$00
	.byte	$00
file_info_p_end:

file_info_fl:
	.byte	"FL"
	.byte	FILE_CHANNEL
file_info_fl_end:

text_title:
	.asciiz	"-= kgsws' 3D engine DEMO =-"
text_alert:
	.asciiz	"[this is not a raycaster]"
text_check:
	.asciiz	"Checking game files ..."
text_load:
	.asciiz	"Loading engine code ..."
text_reboot:
	.asciiz	"Press any key to reboot ..."
text_version:
	.asciiz	"Outdated X16-ROM version!"
text_miss:
	.asciiz	"File KG3D.GFX is missing!"
text_frag0:
	.asciiz	"File KG3D.GFX is fragmented!"
text_frag1:
	.asciiz	"Run defragmentation or use"
text_frag2:
	.asciiz	"freshly formatted SD card."
text_error:
	.asciiz	" ERROR"

