.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "main.inc"
.include "math.inc"
.include "engine.inc"
.include "tables.inc"
.include "render.inc"
.include "video.inc"
.include "input.inc"
.include "text.inc"

.export hud_update

SPRITE_IDX_TOP = 104
SPRITE_IDX_HUD = SPRITE_IDX_TOP - 12

;;;
; CODE
;;;

.segment "CODE"

hud_update:
	lda	TAB_HUD_UPDATE
	bne	:+
	rts
:
	stz	TAB_HUD_UPDATE

	; player check
	ldx	G_PLAYER_THING
	cpx	G_CAMERA_THING
	beq	:+

	; extra code bank
	ldx	#BANK_EXTRA_CODE
	stx	REG_RAM_BANK

	bra	@skip
:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; health
	lda	THING_STRUCT_HEALTH_H,x
	pha
	lda	THING_STRUCT_HEALTH_L,x
	pha

	; extra code bank
	ldx	#BANK_EXTRA_CODE
	stx	REG_RAM_BANK

	; health
	pla
	sta	TAB_HUD_TEMP_HP_L
	pla
	sta	TAB_HUD_TEMP_HP_H

	; update
	ora	TAB_HUD_TEMP_HP_L
	beq	@skip
	jmp	hud_stuff
@skip:
	lda	#SPRITE_IDX_HUD
	sta	TAB_TEXT_I
	jsr	text_clean_all

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.segment "CODE_EXTRA"

;;;
; write HUD number
;;;

write_num:
	; enable math
	jsr	math_setup_vera

	; convert
	lda	ARG_HUD_VAL_L
	sta	VERA_FX_CACHE_L
	lda	ARG_HUD_VAL_H
	sta	VERA_FX_CACHE_M
	jsr	math_u16

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; remove zeros
	ldx	#3
	ldy	#0
@loop_zr:
	lda	math_txtint,y
	and	#$0F
	bne	:+
	lda	#10
	sta	math_txtint,y
	iny
	dex
	bne	@loop_zr
:
	; set ADDR0
	jsr	text_sprite_addr

	; character count
	ldx	#4
	ldy	#0
	lda	ARG_HUD_D
	bmi	@loop_wr
	dex
	iny
@loop_wr:
	; numeric lookup
	lda	#$01
	sta	MATH_VAR_U
	lda	math_txtint,y
	and	#$08
	bne	:+
	lda	#$F8
	bra	:++
:
	inc	MATH_VAR_U
	lda	#$70
:
	ora	math_txtint,y
	iny
	asl

	; addr (LO)
	sta	VERA_DATA0

	; addr (HI), mode (4bpp)
	lda	MATH_VAR_U
	sta	VERA_DATA0

	; X
	lda	ARG_TEXT_X
	sta	VERA_DATA0
	stz	VERA_DATA0

	; Y
	lda	ARG_TEXT_Y
	sta	VERA_DATA0
	stz	VERA_DATA0

	; info
	lda	#$0C
	sta	VERA_DATA0
	lda	ARG_TEXT_C
	sta	VERA_DATA0

	; next
	inc	TAB_TEXT_I
	clc
	lda	ARG_HUD_X
	adc	TAB_HUD_STYLE_SPACE
	sta	ARG_HUD_X
	dex
	bne	@loop_wr

	rts

;;;
; update HUD
;;;

hud_stuff:
	; reset
	lda	#SPRITE_IDX_HUD
	sta	TAB_TEXT_I

	; common
	lda	TAB_HUD_STYLE_BAR_Y
	sta	ARG_HUD_Y

	;; health

	; color
	lda	TAB_HUD_STAT_COLOR_HP
	sta	ARG_TEXT_C

	; write
	lda	TAB_HUD_STYLE_DIGITS
	sta	ARG_HUD_D
	lda	TAB_HUD_STYLE_HP_X0
	sta	ARG_HUD_X
	lda	TAB_HUD_TEMP_HP_L
	sta	ARG_HUD_VAL_L
	lda	TAB_HUD_TEMP_HP_H
	sta	ARG_HUD_VAL_H
	jsr	write_num

	;; ammo

	; color
	lda	TAB_HUD_STAT_COLOR_AMMO
	sta	ARG_TEXT_C

	; write
	asl	ARG_HUD_D
	lda	TAB_HUD_STYLE_AM_X0
	sta	ARG_HUD_X
	lda	#$A7
	sta	ARG_HUD_VAL_L
	lda	#$01
	sta	ARG_HUD_VAL_H
	jsr	write_num

	rts
