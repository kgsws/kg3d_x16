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
.include "text.inc"

.export menu_toggle
.export menu_input

; menu structure
STRUCT_MENU_NOW = $00	; must be zero
STRUCT_MENU_MAX = $01
STRUCT_MENU_PREV = $02
STRUCT_MENU_TITLE = $04	; empty title must be set to $00FF
STRUCT_MENU_ITEM_TEXT = $06
STRUCT_MENU_ITEM_FUNC = $08

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

	; no swing
	lda	#$D0
	sta	MATH_VAR_L
	lda	#$FF
	sta	MATH_VAR_H
	stz	MATH_VAR_U
	stz	MATH_COUNTER

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
; menu pointer
;;;

set_menu_ptr:
	ldy	TAB_MENU_ACTIVE+0
	sty	TMP_PTR_H
	ldy	TAB_MENU_ACTIVE+1
	sty	TMP_PTR_L
	rts

;;;
; ESC key
;;;

menu_toggle:
	; invalidate weapon sprite
	lda	#$FF
	sta	TMP_REND_WPN_IDX
	sta	G_WPN_NOW_LIGHT
	stz	G_WPN_NOW_OFFS_L
	stz	G_WPN_NOW_OFFS_H
	stz	G_WPN_AVG
	stz	G_WPN_LIGHT_NEW

	; toggle
	lda	TAB_MENU_ACTIVE
	bne	@disable

	; activate
	lda	#>mptr_main
	sta	TAB_MENU_ACTIVE+0
	sta	TMP_PTR_H
	lda	#<mptr_main
	sta	TAB_MENU_ACTIVE+1
	sta	TMP_PTR_L

	; use logo
	lda	TAB_MENU_LOGO
	sta	mptr_main+STRUCT_MENU_TITLE

	; draw items
	jmp	menu_update

@disable:
	; deactivate
	stz	TAB_MENU_ACTIVE

	; update weapon sprite
	jsr	menu_wspr_draw

	rts

;;;
; menu input
;;;

menu_input:
	; check ESC
	cmp	#$EE
	beq	menu_toggle

	; current menu
	jsr	set_menu_ptr

	; UP
	cmp	#$D3
	beq	@move_up

	; DOWN
	cmp	#$D4
	beq	@move_down

	; ENTER
	cmp	#$AB
	beq	@select

	; none
	rts

@move_up:
	ldy	#STRUCT_MENU_MAX
	lda	(TMP_PTR_L)
	dea
	bpl	:+
	lda	(TMP_PTR_L),y
	dea
:
	sta	(TMP_PTR_L)
	bra	menu_update

@move_down:
	ldy	#STRUCT_MENU_MAX
	lda	(TMP_PTR_L)
	ina
	cmp	(TMP_PTR_L),y
	bcc	:+
	lda	#0
:
	sta	(TMP_PTR_L)
	bra	menu_update

@select:
	; current
	lda	(TMP_PTR_L)
	asl
	asl
	clc
	adc	#STRUCT_MENU_ITEM_FUNC
	tay

	; pointer
	lda	(TMP_PTR_L),y
	sta	DBG_BYTE0
	iny
	lda	(TMP_PTR_L),y
	sta	DBG_BYTE1

	bra	menu_update

;;;
; update menu sprites
;;;

menu_update:
	; reset sprites
	stz	TAB_TEXT_I

	; title
	ldy	#STRUCT_MENU_TITLE+1
	lda	(TMP_PTR_L),y
	bne	@title

	; logo
	dey
	lda	(TMP_PTR_L),y
	sta	TMP_REND_WPN_IDX

	; update weapon sprite
	jsr	menu_wspr_draw

	; restore menu pointer
	jsr	set_menu_ptr

	bra	@items

@title:
;	sta DBG_BYTE1

@items:
	; max
	ldy	#STRUCT_MENU_MAX
	lda	(TMP_PTR_L),y

	; *16
	tax
	lda	TAB_SWAP,x
	and	#$F0
	sta	TAB_TEXT_Y

	; 120
	sec
	lda	#120
	sbc	TAB_TEXT_Y
	sta	TAB_TEXT_Y

	; now
;	lda	(TMP_PTR_L)
;	sta	DBG_BYTE1

	lda	#64
	sta	TAB_TEXT_X

	; item loop
	ldy	#STRUCT_MENU_ITEM_TEXT
@loop:
	; text pointer
	lda	(TMP_PTR_L),y
	sta	FUNC_ARG_L
	iny
	lda	(TMP_PTR_L),y
	sta	FUNC_ARG_H
	iny

	; write
	jsr	text_write

	; func pointer
	iny
	iny

	; +16
	clc
	lda	TAB_TEXT_Y
	adc	#16
	sta	TAB_TEXT_Y

	; next
	dex
	bne	@loop

	rts

;;;
; menu strings
;;;

mtxt_new_game:
	.asciiz	"New Game"
mtxt_options:
	.asciiz	"Options"
mtxt_test:
	.asciiz	"... TeSt ..."

;;;
; MENU: options
;;;

mptr_options:
	; position
	.byte	$00
	; numitems
	.byte	$02
	; previous
	.word	mptr_main
	; title
	.word	mtxt_options
	; item 0
	.word	mtxt_test
	.word	$0000	; TODO
	; item 1
	.word	mtxt_test
	.word	$0000	; TODO

;;;
; MENU: main
;;;

mptr_main:
	; position
	.byte	$00
	; numitems
	.byte	$03
	; previous
	.word	$0000
	; title
	.word	$0000	; replaced with LOGO
	; item 0
	.word	mtxt_new_game
	.word	$0000	; TODO
	; item 1
	.word	mtxt_options
	.word	mptr_options
	; item 2
	.word	mtxt_test
	.word	$0000	; TODO
