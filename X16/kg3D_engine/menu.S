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

.export menu_esc


;;;
; CODE
;;;

.segment "CODE"

menu_wspr_draw:
	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; force update weapon
	jsr	render_wspr_draw

	; re-enable math
	jsr	math_setup_vera

	; extra code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	rts

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.segment "CODE_EXTRA"

;;;
; ESC key
;;;

menu_esc:
	; invalidate weapon sprite
	lda	#$FF
	sta	TMP_REND_WPN_IDX
	sta	G_WPN_NOW_LIGHT
	stz	G_WPN_NOW_OFFS_L
	stz	G_WPN_NOW_OFFS_H
	stz	G_WPN_AVG

	; toggle
	lda	TAB_MENU_ACTIVE
	eor	#$FF
	sta	TAB_MENU_ACTIVE
	beq	:+

	; use logo
	lda	TAB_MENU_LOGO
	sta	TMP_REND_WPN_IDX
:
	jsr	menu_wspr_draw

	rts
