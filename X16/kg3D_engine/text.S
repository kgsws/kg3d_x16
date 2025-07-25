.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "main.inc"
.include "math.inc"
.include "engine.inc"
.include "tables.inc"

.export text_write_la
.export text_write_ca
.export text_width
.export text_clean

; defs
MAX_CHARACTERS = 96

;;;
; CODE
;;;

.segment "CODE_EXTRA"

;;;
; write text using sprites
;;;
; ARG_TEXT_X and ARG_TEXT_Y is position
; ARG_TEXT_C is color
; ARG_TEXT_PTR_* is pointer to the text
; TAB_TEXT_I is sprite slot index
; uses MATH_VAR_*

text_write_ca:
	jsr	text_width
	sec
	lda	#160
	sbc	RET_TEXT_LEN
	lsr
	sta	ARG_TEXT_X

text_write_la:
	phy
	phx

	; ADDR0 = 0x01FC00+offs, increment = 1
	stz	MATH_VAR_U
	lda	TAB_TEXT_I
	asl
	rol	MATH_VAR_U
	asl
	rol	MATH_VAR_U
	asl
	rol	MATH_VAR_U
	sta	VERA_ADDRx_L
	clc
	lda	#$FC
	adc	MATH_VAR_U
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; characters
	ldy	#0
@loop:
	lda	TAB_TEXT_I
	cmp	#MAX_CHARACTERS
	bcs	@stop

	lda	(ARG_TEXT_PTR_L),y
	beq	@stop
	bmi	@loop	; TODO: text color
	iny

	sec
	sbc	#$20
	sta	MATH_VAR_U
	tax
	beq	@space

	; Y offset
	lda	TAB_FONT_YOFFS,x
	sta	MATH_VAR_L

	; X offset
	lda	TAB_FONT_XOFFS,x
	sta	MATH_VAR_H

	; shift
	lda	TAB_SWAP,x
	and	#$0F
	tax

	; addr (LO)
	lda	MATH_VAR_U
	and	#$0F
	ora	font_base_lo,x
	sta	VERA_DATA0

	; addr (HI), mode (4bpp)
	lda	font_base_hi,x
	sta	VERA_DATA0

	; X
	clc
	lda	ARG_TEXT_X
	adc	MATH_VAR_H
	sta	VERA_DATA0
	stz	VERA_DATA0

	; Y
	clc
	lda	ARG_TEXT_Y
	adc	MATH_VAR_L
	sta	VERA_DATA0
	stz	VERA_DATA0

	; info
	lda	#$0C
	sta	VERA_DATA0
	lda	ARG_TEXT_C
	sta	VERA_DATA0

	inc	TAB_TEXT_I

	ldx	MATH_VAR_U
@space:
	clc
	lda	TAB_FONT_SPACE,x
	adc	ARG_TEXT_X
	sta	ARG_TEXT_X

	bra	@loop

@stop:
	plx
	ply
	rts

;;;
; get text width
;;;
; ARG_TEXT_PTR_* is pointer to the text
; RET_TEXT_LEN is text width

text_width:
	phy
	phx

	stz	RET_TEXT_LEN

	; loop
	ldy	#0
@loop:
	lda	(ARG_TEXT_PTR_L),y
	beq	@stop
	bmi	@loop
	iny
	sec
	sbc	#$20

	tax
	clc
	lda	TAB_FONT_SPACE,x
	adc	RET_TEXT_LEN
	sta	RET_TEXT_LEN

	bra	@loop

@stop:
	plx
	ply
	rts

;;;
; text cleanup
;;;
; clear all sprites after TAB_TEXT_I

text_clean:
	; ADDR0 = 0x01FC00+offs, increment = 8
	stz	MATH_VAR_U
	lda	TAB_TEXT_I
	asl
	rol	MATH_VAR_U
	asl
	rol	MATH_VAR_U
	asl
	rol	MATH_VAR_U
	ora	#$06
	sta	VERA_ADDRx_L
	clc
	lda	#$FC
	adc	MATH_VAR_U
	sta	VERA_ADDRx_M
	lda	#%01000001
	sta	VERA_ADDRx_H

	; loop
	sec
	lda	#MAX_CHARACTERS
	sbc	TAB_TEXT_I
	bne	@loop
	rts
@loop:
	stz	VERA_DATA0
	dea
	bne	@loop

	rts

;;;
; font block origin
;;;

font_base_lo:
	.byte	$F0
	.byte	$F0
	.byte	$A0
	.byte	$B0
	.byte	$E0
	.byte	$F0

font_base_hi:
	.byte	$03
	.byte	$04
	.byte	$0E
	.byte	$0E
	.byte	$0E
	.byte	$0E
