.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "main.inc"
.include "math.inc"
.include "file.inc"
.include "tables.inc"

.export video_init
.export video_load
.export video_setup
.export video_palette
.export video_set_scale
.export video_swap
.export video_lowcrl
.export video_setup_imgline
.export video_image_from_file
.export video_data_from_file
.export video_draw_vline
.export video_draw_tline
.export video_draw_hline
.export video_draw_hline_low
.export video_draw_sline
.export video_copy_normal
.export video_copy_remap

.segment "CODE"

;; NOTE
; Many functions jump to time check function at the end.

;;;
; video init
;;;

video_init:
	; page
	lda	#$C0
	sta	VIDEO_PAGE_L
	lda	#$1F
	sta	VIDEO_PAGE_H
	stz	VIDEO_PAGE
	jmp	video_swap

;;;
; load video tables to VRAM
;;;

video_load:
	jsr	file_open_rd

	; ADDR0 = 0x003E00, increment = 1
	stz	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$3E
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; HUD (1/2)
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$02
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_vram
	bcs	@error

	; ADDR0 = 0x005E00
	lda	#$5E
	sta	VERA_ADDRx_M

	; HUD (2/2)
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$02
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_vram
	bcs	@error

	; ADDR0 = 0x007E00
	lda	#$7E
	sta	VERA_ADDRx_M

	; font (1 / 4)
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$02
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_vram
	bcs	@error

	; ADDR0 = 0x009E00
	lda	#$9E
	sta	VERA_ADDRx_M

	; font (2 / 4)
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$02
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_vram
	bcs	@error

	; ADDR0 = 0x01D000
	lda	#$D0
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; tilemaps + font mix
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$10
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_vram
	bcs	@error

	; ADDR0 = 0x01F000
	lda	#$F0
	sta	VERA_ADDRx_M

	; ranges + invalid texture
	lda	#$C0
	sta	TAB_FILE_BUF_SIZE+0
	lda	#$09
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_vram
	bcs	@error

	jsr	file_close

	; done
	clc
	rts

@error:
	jsr	file_close
	sec
	rts

;;;
; video setup
;;;

video_setup:
	; disable all layers
	lda	REG_VIDEO_MODE
	sta	VERA_DC_VIDEO

	; set main palette
	lda	#0
	jsr	video_palette

	;; set video scale
	ldx	#5*4
	lda	REG_VIDEO_MODE
	and	#3
	cmp	#1
	beq	:+
	ldx	#5*3
:
	jsr	video_set_scale

	rts

;;;
; set screen scale
;;;
; X is index to mode table

video_set_scale:
	; disable sprites
	lda	REG_VIDEO_MODE
	sta	VERA_DC_VIDEO

	; dcsel = 1, addrsel = 0
	lda	#$02
	sta	VERA_CTRL

	; horizontal
	lda	scale_table+0,x
	sta	VERA_DC_HSTART
	lda	scale_table+1,x
	sta	VERA_DC_HSTOP

	; vertical
	lda	scale_table+2,x
	sta	VERA_DC_VSTART
	lda	scale_table+3,x
	sta	VERA_DC_VSTOP

	; dcsel = 0, addrsel = 0
	stz	VERA_CTRL

	; video scale
	lda	scale_table+4,x
	sta	VERA_DC_HSCALE
	sta	VERA_DC_VSCALE

	; enable sprites
	lda	REG_VIDEO_MODE
	ora	#%01000000
	sta	VERA_DC_VIDEO

	rts

;;;
; video palette
;;;
; A is palette index

video_palette:
	; source index
	asl
	ora	#$A0
	sta	@loop+2
	ina
	sta	@pool+2

	; palette bank
	lda	#BANK_PALETTE
	sta	REG_RAM_BANK

	; ADDR1 = 0x01FB00, increment = 1
	lda	#%00000001
	sta	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$FB
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; ADDR0 = 0x01FA00, increment = 1
	stz	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$FA
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; loop
	ldy	#0
@loop:
	lda	$A000,y	; modified address
	sta	VERA_DATA0
@pool:
	lda	$A100,y	; modified address
	sta	VERA_DATA1
	iny
	bne	@loop

	; TIME
	jmp	time_update

;;;
; swap buffers
;;;

video_swap:
	; ADDR1 = 0x01FFA0, increment = 8
	lda	#$01
	sta	VERA_CTRL
	lda	#$A0
	sta	VERA_ADDRx_L
	lda	#$FF
	sta	VERA_ADDRx_M
	lda	#%01000001
	sta	VERA_ADDRx_H

	; ADDR0 = 0x01FFC1, increment = 8
	stz	VERA_CTRL
	lda	#$C1
	sta	VERA_ADDRx_L
	lda	#$FF
	sta	VERA_ADDRx_M
	lda	#%01000001
	sta	VERA_ADDRx_H

	; swap
	lda	VIDEO_PAGE_L
	eor	#$20
	sta	VIDEO_PAGE_L
	lda	VIDEO_PAGE_H
	eor	#$A0
	sta	VIDEO_PAGE_H
	lda	VIDEO_PAGE
	eor	#$60
	sta	VIDEO_PAGE
	bne	:+

	lda	#$84
	sta	VERA_DATA0
	sta	VERA_DATA0
	sta	VERA_DATA0
	sta	VERA_DATA0

	lda	#$83
	sta	VERA_DATA0
	sta	VERA_DATA0
	sta	VERA_DATA0
	sta	VERA_DATA0

	lda	#$01
	sta	VERA_DATA1
	sta	VERA_DATA1
	lda	#$81
	sta	VERA_DATA1
	sta	VERA_DATA1

	rts
:
	lda	#$81
	sta	VERA_DATA0
	sta	VERA_DATA0
	sta	VERA_DATA0
	sta	VERA_DATA0

	lda	#$80
	sta	VERA_DATA0
	sta	VERA_DATA0
	sta	VERA_DATA0
	sta	VERA_DATA0

	stz	VERA_DATA1
	stz	VERA_DATA1
	lda	#$80
	sta	VERA_DATA1
	sta	VERA_DATA1

	rts

;;;
; clear every odd column in both framebuffers
;;;

video_lowcrl:
	; dcsel = 0, addrsel = 0
	stz	VERA_CTRL

	; ADDR0 = 0x0000??, increment = 2
	lda	#%00100000
	sta	VERA_ADDRx_H
	stz	VERA_ADDRx_M

	; clear blocks
	ldy	#5
@loop:
	; ADDR0 = 0x????01
	lda	#$01
	sta	VERA_ADDRx_L

	; clear columns
	ldx	#0
@loop_col:
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	stz	VERA_DATA0
	dex
	bne	@loop_col

	clc
	lda	VERA_ADDRx_M
	adc	#02
	sta	VERA_ADDRx_M

	dey
	bne	@loop

	rts

;;;
; setup line transfer to current frame
;;;
; uses X and Y

video_setup_imgline:
	ldx	VIDEO_DRAW_X
	clc
	lda	VIDEO_PAGE
	eor	#$60
	adc	TAB_XOFFS_H,x
	cmp	#$A0
	bcc	:+
	txa
	ora	#$20
	tax
	lda	#$40
:
	sta	VERA_ADDRx_M
	clc
	txa
	and	#63
	sta	VERA_ADDRx_L

	rts

;;;
; transfer image from opened file to current frame
;;;
; assuming VERA FX disabled
; assuming file is open and setup to read image data

video_image_from_file:
	; dcsel = 0, addrsel = 0
	stz	VERA_CTRL

	; increment = 64
	lda	#%01110000
	sta	VERA_ADDRx_H

	; the loop
	stz	VIDEO_DRAW_X
@loop:
	; setup X
	jsr	video_setup_imgline

	; read from file
	sec
	lda	#120
	ldx	#<VERA_DATA0
	ldy	#>VERA_DATA0
	jsr	k_MACPTR
	bcc	:+
	; error
	rts
:
	; next
	inc	VIDEO_DRAW_X
	lda	VIDEO_DRAW_X
	cmp	#160
	bne	@loop

	clc
	rts

;;;
; transfer data from file to VRAM
;;;
; starting at 0xA000
; assuming file is open and setup to read VRAM data
; X is count of 2048 blocks

video_data_from_file:
	; dcsel = 0, addrsel = 0
	stz	VERA_CTRL

	; ADDR0 = 0x00A000, increment = 1
	stz	VERA_ADDRx_L
	lda	#$A0
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	stx	VIDEO_TMP_0
@loop_2k:
	lda	#8
	sta	VIDEO_TMP_1
@loop_240:
	; read from file (8 * 240)
	sec
	lda	#240
	ldx	#<VERA_DATA0
	ldy	#>VERA_DATA0
	jsr	k_MACPTR
	bcc	:+
	; error
	rts
:
	dec	VIDEO_TMP_1
	bne	@loop_240

	; read from file (128)
	sec
	lda	#128
	ldx	#<VERA_DATA0
	ldy	#>VERA_DATA0
	jsr	k_MACPTR
	bcc	:+
	; error
	rts
:
	dec	VIDEO_TMP_0
	bne	@loop_2k

	clc
	rts

;;;
; vertical textured line
;;;
; assuming affine helper set-up
; assuming addrsel = 0
; VIDEO_DRAW_X is X on screen
; VIDEO_DRAW_L is line length-1
; VIDEO_DRAW_Y is off by one
; uses X and Y

video_draw_vline:
	; colormap bank
	lda	COLORMAP_B
	sta	REG_RAM_BANK

	; jump table lookup (length)
	ldx	VIDEO_DRAW_L
	lda	TAB_PVJUMP_L,x
	sta	@opcode + 1
	lda	TAB_PVJUMP_H,x
	sta	@opcode + 2

	; decrement = 64
	lda	#%01111000
	sta	VERA_ADDRx_H

	; Y lookup + X lookup
	ldx	VIDEO_DRAW_X
	ldy	VIDEO_DRAW_Y
	dey
	clc
	lda	TAB_XOFFS_H,x
	adc	TAB_YOFFS_H,y
	adc	VIDEO_PAGE
	cmp	#$A0
	bcc	:+
	txa
	ora	#$20
	tax
	clc
	lda	#$40
	adc	TAB_YOFFS_H,y
:
	sta	VERA_ADDRx_M
	clc
	txa
	and	#63
	adc	TAB_YOFFS_L,y
	sta	VERA_ADDRx_L

@opcode:
	jsr	$C0DE	; modified address

	; TIME
	jmp	time_update

;;;
; vertical textured line for sprites
;;;
; assuming affine helper set-up
; assuming addrsel = 0
; VIDEO_DRAW_X is X on screen
; VIDEO_DRAW_L is line length-1
; VIDEO_DRAW_Y is off by one
; uses X and Y

video_draw_tline:
	; colormap bank
	lda	COLORMAP_B
	sta	REG_RAM_BANK

	; jump table lookup (length)
	ldx	VIDEO_DRAW_L
	lda	TAB_PTJUMP_L,x
	sta	@opcode + 1
	lda	TAB_PTJUMP_H,x
	sta	@opcode + 2

	; decrement = 64
	lda	#%01111000
	sta	VERA_ADDRx_H

	; Y lookup + X lookup
	ldx	VIDEO_DRAW_X
	ldy	VIDEO_DRAW_Y
	dey
	clc
	lda	TAB_XOFFS_H,x
	adc	TAB_YOFFS_H,y
	adc	VIDEO_PAGE
	cmp	#$A0
	bcc	:+
	txa
	ora	#$20
	tax
	clc
	lda	#$40
	adc	TAB_YOFFS_H,y
:
	sta	VERA_ADDRx_M
	clc
	txa
	and	#63
	adc	TAB_YOFFS_L,y
	sta	VERA_ADDRx_L

@opcode:
	jsr	$C0DE	; modified address

	; TIME
	jmp	time_update

;;;
; horizontal textured line
;;;
; assuming affine helper set-up
; assuming addrsel = 0
; VIDEO_DRAW_X is X on screen
; VIDEO_DRAW_Y is off by one
; uses X and Y

video_draw_hline_low:
	; increment = 2
	lda	#%00100000
	sta	VERA_ADDRx_H

	; X start and stop
	lda	VIDEO_DRAW_XX
	lsr
	clc
	adc	#(TAB_PLJUMP_L-TAB_PHJUMP_L)
	pha
	lda	VIDEO_DRAW_X
	lsr
	clc
	adc	#(TAB_PLJUMP_L-TAB_PHJUMP_L)
	pha

	bra	hline_code

video_draw_hline:
	; increment = 1
	lda	#%00010000
	sta	VERA_ADDRx_H

	; X start and stop
	lda	VIDEO_DRAW_XX
	lsr
	pha
	lda	VIDEO_DRAW_X
	lsr
	pha

hline_code:
	; jump table lookup (start)
	clc
	plx
	lda	TAB_PHJUMP_L,x
	sta	@opcode + 1
	lda	TAB_PHJUMP_H,x
	sta	@opcode + 2

	; jump table lookup (end)
	clc
	plx
	lda	TAB_PHJUMP_L,x
	sta	VIDEO_TMP_0
	lda	TAB_PHJUMP_H,x
	sta	VIDEO_TMP_1

	; backup opcode
	lda	(VIDEO_TMP_0)
	sta	@oprestore+1

	; place 'rts'
	lda	#$60
	sta	(VIDEO_TMP_0)

	; colormap bank
	lda	COLORMAP_B
	sta	REG_RAM_BANK

	; Y lookup + X lookup
	ldx	VIDEO_DRAW_X
	ldy	VIDEO_DRAW_Y
	dey
	clc
	lda	TAB_XOFFS_H,x
	adc	TAB_YOFFS_H,y
	adc	VIDEO_PAGE
	cmp	#$A0
	bcc	:+
	txa
	ora	#$20
	tax
	clc
	lda	#$40
	adc	TAB_YOFFS_H,y
:
	sta	VERA_ADDRx_M
	clc
	txa
	and	#63
	adc	TAB_YOFFS_L,y
	sta	VERA_ADDRx_L

@opcode:
	jsr	$C0DE	; modified address

	; restore opcode
@oprestore:
	lda	#0	; modified value
	sta	(VIDEO_TMP_0)

	; TIME
	jmp	time_update

;;;
; vertical sky line
;;;
; 'colormap' pointer holds sky texture
; VIDEO_DRAW_L is length
; VIDEO_DRAW_YY is used instead of VIDEO_DRAW_Y
; VIDEO_DRAW_X is X on screen
; assuming addrsel = 0
; drawing starts at TOP
; uses X and Y

video_draw_sline:
	; xx = x2angle[x] - projection.a
	ldx	VIDEO_DRAW_X
	sec
	lda	TAB_X2A_L,x
	sbc	PROJECTION_A_L
	sta	MATH_VAR_L
	lda	TAB_X2A_H,x
	sbc	PROJECTION_A_H
	sta	MATH_VAR_H

	; xx >>= 3
	lsr	MATH_VAR_H
	ror	MATH_VAR_L
	lsr	MATH_VAR_H
	ror	MATH_VAR_L
	lsr	MATH_VAR_H
	ror	MATH_VAR_L

	; add to sky base
	clc
	lda	TAB_SKY_BASE+0
	adc	MATH_VAR_L
	sta	COLORMAP_L
	lda	TAB_SKY_BASE+1
	adc	#0

	; texture pointer
	asl
	asl
	asl
	ldx	COLORMAP_L
	ora	TAB_BANK,x
	sta	COLORMAP_B
	txa
	and	#$1F
	ora	#$A0
	sta	COLORMAP_H

	; half select
	stz	COLORMAP_L
	lda	MATH_VAR_H
	ror
	bcc	:+
	lda	#$80
	sta	COLORMAP_L
:
	; ys = projection.ycw - y0
	sec
	lda	PROJECTION_YCW
	sbc	VIDEO_DRAW_YY
	sta	VIDEO_DRAW_S

	; jump table lookup (start)
	ldy	VIDEO_DRAW_S
	lda	TAB_PSJUMP_L,y
	sta	@opcode + 1
	lda	TAB_PSJUMP_H,y
	sta	@opcode + 2

	; jump table lookup (end)
	sec
	lda	VIDEO_DRAW_S
	sbc	VIDEO_DRAW_L
	tax
	lda	TAB_PSJUMP_L,x
	sta	VIDEO_TMP_0
	lda	TAB_PSJUMP_H,x
	sta	VIDEO_TMP_1

	; backup opcode
	lda	(VIDEO_TMP_0)
	sta	@oprestore+1

	; place 'rts'
	lda	#$60
	sta	(VIDEO_TMP_0)

	; colormap bank
	lda	COLORMAP_B
	sta	REG_RAM_BANK

	; increment = 64
	lda	#%01110000
	sta	VERA_ADDRx_H

	; Y lookup + X lookup (no dey)
	ldx	VIDEO_DRAW_X
	ldy	VIDEO_DRAW_YY
	clc
	lda	TAB_XOFFS_H,x
	adc	TAB_YOFFS_H,y
	adc	VIDEO_PAGE
	cmp	#$A0
	bcc	:+
	txa
	ora	#$20
	tax
	clc
	lda	#$40
	adc	TAB_YOFFS_H,y
:
	sta	VERA_ADDRx_M
	clc
	txa
	and	#63
	adc	TAB_YOFFS_L,y
	sta	VERA_ADDRx_L

	; draw
	lda	VIDEO_DRAW_S
	bpl	:+
	eor	#$FF
	ina
:
	tay
@opcode:
	jsr	$C0DE	; modified address

	; restore opcode
@oprestore:
	lda	#0	; modified value
	sta	(VIDEO_TMP_0)

	; TIME
	jmp	time_update

;;;
; copy to VRAM
;;;
; copy from WRAM to VRAM in 256B blocks
; YX is VRAM 256B block source
; A is number of blocks
; 

video_copy_normal:
	pha
video_copy_normal_x:
	; sky rendering loop (128 pixels)
	lda	TAB_PSJUMP_L+255
	sta	FUNC_ARG_L
	lda	TAB_PSJUMP_H+255
	sta	FUNC_ARG_H

	; ADDRSEL = 1
	lda	#%00000001
	sta	VERA_CTRL

video_copy_blocks:
	; source
	tya
	asl
	asl
	asl
	ora	TAB_BANK,x
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	COLORMAP_H
	stz	COLORMAP_L

	; the loop
	ldy	#0
	plx
@loop:
	jsr	func_jsr
	jsr	func_jsr

	; check end
	dex
	beq	@done

	; reset ADDR1
	lda	#$1E
	sta	VERA_ADDRx_M

	; update destination
	inc	COLORMAP_H
	lda	COLORMAP_H
	cmp	#$C0
	bcc	:+
	lda	#$A0
	sta	COLORMAP_H
	inc	REG_RAM_BANK
:
	; next
	bra	@loop

@done:
	stz	VERA_CTRL

	; TIME
	jmp	time_update

;;;
; variant of 'video_copy_normal' with light remap
;;;
; uses G_WPN_NOW_LIGHT for light index

video_copy_remap:
	pha

	; light map
	clc
	lda	G_WPN_NOW_LIGHT
	beq	video_copy_normal_x
	lsr
	lsr
	and	#$1C
	adc	#>TAB_LIGHTMAPS
	sta	LIGHTMAP_H

	; ADDR1 = 0x001E00, increment = 1
	lda	#%00000001
	sta	VERA_CTRL
	stz	VERA_ADDRx_L
	lda	#$1E
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; sprite rendering loop (128 pixels)
	lda	TAB_PTJUMP_L+127
	sta	FUNC_ARG_L
	lda	TAB_PTJUMP_H+127
	sta	FUNC_ARG_H

	jmp	video_copy_blocks

;;;
; screen scale table
;;;

scale_table:
	; 1x
	.byte	60
	.byte	100
	.byte	90
	.byte	150
	.byte	128
	; 2x
	.byte	40
	.byte	120
	.byte	60
	.byte	180
	.byte	64
	; ~2.976744186x
	.byte	20
	.byte	139
	.byte	31
	.byte	210
	.byte	43
	; ~3.047619048x
	.byte	19
	.byte	141
	.byte	28
	.byte	211
	.byte	42
	; 4x
	.byte	0
	.byte	160
	.byte	0
	.byte	240
	.byte	32
