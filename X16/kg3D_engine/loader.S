.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "zeropage.inc"
.include "banks.inc"
.include "tables.inc"

COLORS_TITLE = $67
COLORS_STATUS = $0F
COLORS_ERROR = $0A
COLORS_INFO = $05

VAR_FILE_BUF_SIZE = $E0
VAR_FILE_DEST = $E2

BLOCK_READ_SIZE = 240
FILE_HANDLE = 2
FILE_DEVICE_ID = 8
FILE_DEST_WRAM = 0
FILE_DEST_VRAM0 = 2
FILE_DEST_VRAM1 = 3

.org $080D
; NOTE:	loader code size limit is 0x21F3 (8691 bytes)
; 	TABLES_0_ADDR = $2A00

.segment "STARTUP"
.segment "INIT"
.segment "ONCE"
.segment "CODE"

;;;
; loader main
;;;

start:
	;;;;
	;; setup video RAM

	; reset stack
	ldx	#$FF
	txs

	; copy character set to VRAM
	stz	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$22
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H
	ldy	#6
	ldx	#0
@vram_tile_loop:
	lda	dos_font,x	; modified opcode
	sta	VERA_DATA0
	inx
	bne	@vram_tile_loop
	inc	@vram_tile_loop+2
	dey
	bne	@vram_tile_loop

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
	lda	#COLORS_STATUS
	sta	VERA_FX_CACHE_M
	sta	VERA_FX_CACHE_U

	; clear lines (status)
	ldx	#13
@clear_loop:
	inc	VERA_ADDRx_M
	stz	VERA_ADDRx_L
	jsr	clear_line
	dex
	bne	@clear_loop

	; setup clear colors
	lda	#COLORS_INFO
	sta	VERA_FX_CACHE_M
	sta	VERA_FX_CACHE_U

	; clear last line (info)
	inc	VERA_ADDRx_M
	stz	VERA_ADDRx_L
	jsr	clear_line

	; clear 'loading GFX' line
	stz	VERA_FX_CACHE_M
	stz	VERA_FX_CACHE_U
	lda	#$05
	sta	VERA_ADDRx_M
	stz	VERA_ADDRx_L
	jsr	clear_line

	; disable cache
	lda	#$04
	sta	VERA_CTRL
	stz	VERA_FX_CTRL
	stz	VERA_CTRL

	; text: title
	jsr	write_text
	.word	$0014
	.word	text_title

	;;;;
	;; setup video MODE

	; backup video mode
	lda	VERA_DC_VIDEO
	and	#%00001111
	sta	REG_VIDEO_MODE

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

	; set scale
	lda	#64
	sta	VERA_DC_HSCALE
	sta	VERA_DC_VSCALE

	; enable layer 1
	lda	REG_VIDEO_MODE
	ora	#%00100000
	sta	VERA_DC_VIDEO

	;;;;
	;; WRAM check

	; check RAM size
	sec
	jsr	k_MEMTOP
	beq	:+	; 2MB check
	cmp	#$80	; 1MB check
	bcs	:+

	; text: error
	jsr	write_text
	.word	$070A
	.word	text_error_ram

	jmp	error
:
	;;;;
	;; setup sprites

	; ADDR0 = 0x01FFFF, decrement = 1
	stz	VERA_CTRL
	lda	#$FF
	sta	VERA_ADDRx_L
	sta	VERA_ADDRx_M
	lda	#%00011001
	sta	VERA_ADDRx_H

	; setup sprites
	ldy	#96
@loop_spr:
	lda	sprite_data-1,y
	sta	VERA_DATA0
	dey
	bne	@loop_spr

	;;;;
	;; load engine

	; text: loading binary
	jsr	write_text
	.word	$0100
	.word	text_load_bin

	; open engine binary
	lda	#(fn_engine_end-fn_engine)
	ldx	#<fn_engine
	ldy	#>fn_engine
	jsr	file_open_rd

	; load main code
	lda	#<LDR_ENGINE_DEST
	sta	VAR_FILE_DEST+0
	lda	#>LDR_ENGINE_DEST
	sta	VAR_FILE_DEST+1
	lda	#<LDR_ENGINE_SIZE
	sta	VAR_FILE_BUF_SIZE+0
	lda	#>LDR_ENGINE_SIZE
	sta	VAR_FILE_BUF_SIZE+1
	jsr	file_read_16_wram
	bcs	@error_engine

	; code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	; load extra code
	lda	#<LDR_CODE_EXTRA_SIZE
	sta	VAR_FILE_BUF_SIZE+0
	lda	#>LDR_CODE_EXTRA_SIZE
	sta	VAR_FILE_BUF_SIZE+1
	jsr	file_read_16_wram_bank
	bcs	@error_engine

	; code bank
	lda	#BANK_SWAP_CODE
	sta	REG_RAM_BANK

	; load swap code
	lda	#<LDR_CODE_SWAP_SIZE
	sta	VAR_FILE_BUF_SIZE+0
	lda	#>LDR_CODE_SWAP_SIZE
	sta	VAR_FILE_BUF_SIZE+1
	jsr	file_read_16_wram_bank
	bcs	@error_engine

	; engine loaded
	jsr	file_close

	bra	:+

@error_engine:
	; loading failed
	jsr	file_close

	; text: error
	jsr	write_text
	.word	$070A
	.word	text_error_bin

	jmp	error
:
	;;;;
	; load tables

	jsr	load_tables

	; text: video init
	jsr	write_text
	.word	$0300
	.word	text_init_video

	;;;;
	;; clear frame buffers

	; ADDR0 = 0x000000, increment = 4
	stz	VERA_CTRL
	stz	VERA_ADDRx_L
	stz	VERA_ADDRx_M
	lda	#%00110000
	sta	VERA_ADDRx_H

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; clear cache
	stz	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M
	stz	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	;; clear screens
	ldx	#0
	ldy	#10
@loop_clr:
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	dex
	bne	@loop_clr
	dey
	bne	@loop_clr

	; disable cache write
	stz	VERA_FX_CTRL
	stz	VERA_CTRL

	;;;;
	;; rearange video tables

	; ADDR0 = 0x01D800, increment = 1
	stz	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$D8
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; ADDR1 = 0x00??00, increment = 1
	lda	#%00000001
	sta	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#%00010000
	sta	VERA_ADDRx_H

	; HUD (1/2)
	jsr	vram_move
	.byte	$3E, $02

	; HUD (2/2)
	jsr	vram_move
	.byte	$5E, $02

	; font (1 / 4)
	jsr	vram_move
	.byte	$7E, $02

	; font (2 / 4)
	jsr	vram_move
	.byte	$9E, $02

	; ADDR1 = 0x01????, increment = 1
	lda	#%00010001
	sta	VERA_ADDRx_H

	; part of ranges (4096 B)
	jsr	vram_move
	.byte	$D0, $10

	;;;;
	;; generate quick remap

	; ADDR0 = 0x001E00, increment = 1
	stz	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$1E
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; loop
	ldx	#0
@loop_remap:
	stx	VERA_DATA0
	inx
	bne	@loop_remap

	;;;;
	;; DEMO MESSAGE

	bra	@skip_msg

	jsr	write_text
	.word	$0606
	.word	text_demo_wait0
	jsr	write_text
	.word	$0708
	.word	text_demo_wait1
	jsr	write_text
	.word	$0904
	.word	text_demo_wait3
	jsr	write_text
	.word	$0A0A
	.word	text_demo_wait4
	jsr	write_text
	.word	$0B06
	.word	text_demo_wait5
	jsr	write_text
	.word	$0C06
	.word	text_demo_wait6
	jsr	write_text
	.word	$0E0E
	.word	text_demo_start

	jsr	wait_key

	; ADDR0 = 0x010800, increment = 2
	stz	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$08
	sta	VERA_ADDRx_M
	lda	#%00100001
	sta	VERA_ADDRx_H

	ldx	#6
@loop_msgclr:
	ldy	#0
@loop_lnclr:
	lda	#' '
	sta	VERA_DATA0
	dey
	bne	@loop_lnclr
	dex
	bne	@loop_msgclr

@skip_msg:
	;;;;
	;; start engine

	; text: starting
	jsr	write_text
	.word	$0400
	.word	text_start

	; text: precaching (hidden)
	jsr	write_text
	.word	$0500
	.word	text_cache

	; disable IRQ
	sei

	; clear zero page
	ldx	#$FC
@loop_zclr:
	stz	$02,x
	dex
	bne	@loop_zclr

	; code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	; start
	jmp	LDR_ENGINE_DEST

;;;
; load all tables
;;;

load_tables:
	;; load tables0

	; text: loading tables
	jsr	write_text
	.word	$0200
	.word	text_load_tab

	; load from file
	jsr	file_load_tab
	.word	TABLES_0_ADDR
	.byte	FILE_DEST_WRAM
	bcs	error_tab

	;; load tables1

	inc	fn_tablesX_end-5

	; tables bank
	lda	#BANK_TABLES
	sta	REG_RAM_BANK

	; load from file
	jsr	file_load_tab
	.word	$A000
	.byte	FILE_DEST_WRAM
	bcs	error_tab

	;; load tables2

	inc	fn_tablesX_end-5

	; load from file
	jsr	file_load_tab
	.word	$D800
	.byte	FILE_DEST_VRAM1
	bcs	error_tab

	;; load tables3

	inc	fn_tablesX_end-5

	; palette bank
	lda	#BANK_PALETTE
	sta	REG_RAM_BANK

	; load from file
	jsr	file_load_tab
	.word	$A000
	.byte	FILE_DEST_WRAM
	bcs	error_tab

	;; load tables4

	inc	fn_tablesX_end-5

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; load from file
	jsr	file_load_tab
	.word	$A000
	.byte	FILE_DEST_WRAM
	bcs	error_tab

	rts

;;;
; error handlers
;;;

error_tab:
	lda	fn_tablesX_end-5
	sta	text_error_tab_end-3

	; text: error
	jsr	write_text
	.word	$0712
	.word	text_error_tab

error:
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

	; read statck
	tsx

	; load pointer
	lda	$0101,x
	sta	FUNC_ARG_L
	lda	$0102,x
	sta	FUNC_ARG_H

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
	lda	(FUNC_ARG_L),y
	sta	VERA_ADDRx_L

	; VERA address (hi)
	iny
	lda	(FUNC_ARG_L),y
	sta	VERA_ADDRx_M

	; text address (lo)
	iny
	lda	(FUNC_ARG_L),y
	sta	@write_opcode+1

	; text address (hi)
	iny
	lda	(FUNC_ARG_L),y
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
; load file to WRAM
;;;
; FILE_DEST_* is destination pointer
; X and Y is file name (lo/hi)
; A is file name length

file_load_tab:
	lda	#(fn_tablesX_end-fn_tablesX)
	ldx	#<fn_tablesX
	ldy	#>fn_tablesX

file_load:
	; setup file name
	jsr	k_SETNAM

	; read statck
	tsx

	; load pointer
	lda	$0101,x
	sta	FUNC_ARG_L
	lda	$0102,x
	sta	FUNC_ARG_H

	; advance return address
	clc
	lda	$0101,x
	adc	#3
	sta	$0101,x
	bcc	:+
	inc	$0102,x
:
	; setup file transfer
	lda	#FILE_HANDLE
	ldx	#FILE_DEVICE_ID
	ldy	#2
	jsr	k_SETLFS

	; address (lo)
	ldy	#1
	lda	(FUNC_ARG_L),y
	pha

	; address (hi)
	iny
	lda	(FUNC_ARG_L),y
	pha

	; mode
	iny
	lda	(FUNC_ARG_L),y

	; load file
	ply
	plx
	jmp	k_LOAD

;;;
; move data in VRAM
;;;

vram_move:
	; read statck
	tsx

	; load pointer
	lda	$0101,x
	sta	FUNC_ARG_L
	lda	$0102,x
	sta	FUNC_ARG_H

	; advance return address
	clc
	lda	$0101,x
	adc	#2
	sta	$0101,x
	bcc	:+
	inc	$0102,x
:
	; address
	ldy	#1
	lda	(FUNC_ARG_L),y
	sta	VERA_ADDRx_M

	; block count
	iny
	lda	(FUNC_ARG_L),y
	tay

	; block loop
@loop_block:
	; copy loop
	ldx	#64
@loop_copy:
	lda	VERA_DATA0
	sta	VERA_DATA1
	lda	VERA_DATA0
	sta	VERA_DATA1
	lda	VERA_DATA0
	sta	VERA_DATA1
	lda	VERA_DATA0
	sta	VERA_DATA1
	dex
	bne	@loop_copy

	; next block
	dey
	bne	@loop_block

	rts

;;;
; open file (read)
;;;
; X and Y is file name (lo/hi)
; A is file name length

file_open_rd:
	; set file name
	jsr	k_SETNAM

	; set open mode
	lda	#FILE_HANDLE
	ldx	#FILE_DEVICE_ID
	tay
	jsr	k_SETLFS

	; open file
	jsr	k_OPEN

	; redirect output
	ldx	#FILE_HANDLE
	jmp	k_CHKIN

;;;
; close file
;;;

file_close:
	lda	#FILE_HANDLE
	jsr	k_CLOSE
	ldx	#0
	jmp	k_CHKIN

;;;
; read from file (65536 bytes max)
;;;
; VAR_FILE_BUF_SIZE is size to read; 0 is 65536; this value is decremented
; VAR_FILE_DEST is destination pointer, assumend to be in banked area; this value is advanced, with banking support
; C is set on error

file_read_16_wram_bank:
	stz	VAR_FILE_DEST+0
	lda	#$A0
	sta	VAR_FILE_DEST+1

file_read_16_wram:
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
	bcc	:+	; modified offset
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
	;
	.word	$8200
	.word	129
	.word	0
	.word	$F00C
	;;;
	.word	$8280
	.word	128
	.word	64
	.word	$F00C
	;
	.word	$8280
	.word	129
	.word	64
	.word	$F00C
	;;;
	.word	$8100
	.word	64
	.word	0
	.word	$F00C
	;
	.word	$8100
	.word	65
	.word	0
	.word	$F00C
	;;;
	.word	$8180
	.word	64
	.word	64
	.word	$F00C
	;
	.word	$8180
	.word	65
	.word	64
	.word	$F00C
	;;;
	.word	$8000
	.word	0
	.word	0
	.word	$F00C
	;
	.word	$8000
	.word	1
	.word	0
	.word	$F00C
	;;;
	.word	$8080
	.word	0
	.word	64
	.word	$F00C
	;
	.word	$8080
	.word	1
	.word	64
	.word	$F00C

;;;
; strings
;;;

fn_engine:
	.byte	"ENGINE.BIN"
fn_engine_end:

fn_tablesX:
	.byte	"DATA/TABLES0.BIN"
fn_tablesX_end:

text_title:
	.asciiz	"kgsws' 3D engine DEMO"
text_load_bin:
	.asciiz	"Loading engine code ..."
text_load_tab:
	.asciiz	"Loading engine data ..."
text_init_video:
	.asciiz	"Initializing video ..."
text_start:
	.asciiz	"Starting engine ..."
text_cache:
	.asciiz	"Loading graphics ..."
text_reboot:
	.asciiz	"Press any key to reboot ..."
text_error_ram:
	.asciiz	"This game requires 1MB of RAM!"
text_error_bin:
	.asciiz	"Unable to load engine binary!"
text_error_tab:
	.asciiz	"Unable to load TABLES0!"
text_error_tab_end:

text_demo_wait0:
	.asciiz	"This is a work in progress, there"
text_demo_wait1:
	.asciiz	"isn't any gameplay ... yet."
text_demo_wait3:
	.asciiz	"Use WASD to move and MOUSE to turn."
text_demo_wait4:
	.asciiz	"Use MOUSE to aim and attack."
text_demo_wait5:
	.asciiz	"Use SPACE to jump CTRL to crouch."
text_demo_wait6:
	.asciiz	"Press F5 to toggle low-res mode."
text_demo_start:
	.asciiz	"Press any key to start ..."

