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

.export menu_init
.export menu_toggle
.export menu_input

; menu structure
STRUCT_MENU_NOW = $00	; must be zero
STRUCT_MENU_TOP = $01
STRUCT_MENU_MAX = $02	; must be after TOP
STRUCT_MENU_SPECIAL = $03
STRUCT_MENU_PREV = $04
STRUCT_MENU_TITLE = $06	; empty title must be set to $00FF

; each menu item is 4 bytes
; item count per menu is (256 - 12) / 4
STRUCT_MENU_ITEM_TEXT = $08
STRUCT_MENU_ITEM_FUNC = $0A

;;;
; CODE
;;;

.segment "CODE"

menu_wspr_draw:
	; no swing
	lda	#$D0
	sta	MATH_VAR_L
	lda	#$FF
	sta	MATH_VAR_H
	stz	MATH_VAR_U
	stz	MATH_COUNTER

	; force update weapon
	jsr	render_wspr_draw

	; extra code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	; restore menu pointer
	jmp	set_menu_ptr

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.segment "CODE_EXTRA"

;;;
; init
;;;

menu_init:
	; maxcnt = (120 - text_height * 4) / text_height
	sec
	lda	#120
	sbc	TAB_FONT_HEIGHT
	sbc	TAB_FONT_HEIGHT
	sbc	TAB_FONT_HEIGHT
	sbc	TAB_FONT_HEIGHT
	ldx	#0
@loop:
	sec
	sbc	TAB_FONT_HEIGHT
	bcc	@stop
	inx
	bra	@loop
@stop:
	stx	TAB_MENU_MAX
	rts

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

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; update weapon sprite
	jsr	menu_wspr_draw

	; clear text
	stz	TAB_TEXT_I
	jsr	text_clean

	; re-enable math
	jsr	math_setup_vera

	rts

;;;
; menu input
;;;

menu_input:
	; current menu
	jsr	set_menu_ptr

	; backup the key
	sta	MATH_TEMP

	; check ESC
	cmp	#$EE
	bne	@check_move

	; check previous menu
	ldy	#STRUCT_MENU_PREV+1
	lda	(TMP_PTR_L),y
	beq	menu_toggle

	; copy new pointer
	sta	TAB_MENU_ACTIVE+0
	dey
	lda	(TMP_PTR_L),y
	sta	TAB_MENU_ACTIVE+1

	; new menu
	jsr	set_menu_ptr
	jmp	menu_update

@check_move:
	; UP
	cmp	#$53
	beq	@move_up

	; DOWN
	cmp	#$54
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
	bra	@do_move

@move_down:
	ldy	#STRUCT_MENU_MAX
	lda	(TMP_PTR_L)
	ina
	cmp	(TMP_PTR_L),y
	bcc	:+
	lda	#0
:
	sta	(TMP_PTR_L)
	bra	@do_move

@do_move:
	; top < now
	dey
	cmp	(TMP_PTR_L),y
	bcs	:+
	sta	(TMP_PTR_L),y
	bra	@check_type
:
	; now - menu_max - top
	sbc	TAB_MENU_MAX
	tax
	sec
	sbc	(TMP_PTR_L),y
	bmi	@check_type

	inx
	txa
	sta	(TMP_PTR_L),y

@check_type:
	; current item text (HI)
	lda	(TMP_PTR_L)
	asl
	asl
	clc
	adc	#STRUCT_MENU_ITEM_TEXT+1
	tay
	lda	(TMP_PTR_L),y

	; check type
	bmi	menu_update

	; repeat the move
	lda	MATH_TEMP
	bra	@check_move

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
	sta	MATH_VAR_L
	iny
	lda	(TMP_PTR_L),y
	bpl	:+

	; change menu
	sta	TAB_MENU_ACTIVE+0
	lda	MATH_VAR_L
	sta	TAB_MENU_ACTIVE+1
	jsr	set_menu_ptr
:
	bra	menu_update

;;;
; update menu sprites
;;;

menu_update:
	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; reset sprites
	stz	TAB_TEXT_I

	; check title type
	ldy	#STRUCT_MENU_TITLE+1
	lda	(TMP_PTR_L),y
	bne	@title

	;;;;;;
	;;; TITLE (LOGO)

	; logo
	dey
	lda	(TMP_PTR_L),y
	sta	TMP_REND_WPN_IDX

	; update weapon sprite
	jsr	menu_wspr_draw

	; item Y
	lda	#64
	sta	ARG_TEXT_Y

	bra	@items

	;;;;;;
	;;; TITLE (TEXT)
@title:
	; text pointer
	sta	ARG_TEXT_PTR_H
	dey
	lda	(TMP_PTR_L),y
	sta	ARG_TEXT_PTR_L

	; update weapon sprite
	lda	#$FF
	sta	TMP_REND_WPN_IDX
	jsr	menu_wspr_draw

	; X
	jsr	text_width
	sec
	lda	#160
	sbc	RET_TEXT_LEN
	lsr
	sta	ARG_TEXT_X

	; title Y = font_height * 1
	lda	TAB_FONT_HEIGHT
	sta	ARG_TEXT_Y

	; color
	lda	#15
	sta	ARG_TEXT_C

	; write
	jsr	text_write

	; item Y = font_height * 3
	lda	TAB_FONT_HEIGHT
	asl
	adc	TAB_FONT_HEIGHT
	sta	ARG_TEXT_Y

	;;;;;;
	;;; ITEMS
@items:
	;; get length of all items

	; max
	ldy	#STRUCT_MENU_MAX
	lda	(TMP_PTR_L),y
	tax

	; reset
	stz	MATH_TEMP

	; item loop
	ldy	#STRUCT_MENU_ITEM_TEXT
@loop_calc:
	; text pointer
	jsr	get_text_ptr

	; func pointer
	iny
	iny

	; check entry type
	lda	ARG_TEXT_PTR_H
	bpl	:+

	; get width
	jsr	text_width

	; check width
	lda	RET_TEXT_LEN
	cmp	MATH_TEMP
	bcc	:+

	; use new width
	sta	MATH_TEMP
:
	; next
	dex
	bne	@loop_calc

	; check special
	ldy	#STRUCT_MENU_SPECIAL
	lda	(TMP_PTR_L),y
	beq	:+

	; fake extra spaces
	lda	TAB_FONT_SPACE
	asl
	asl
	adc	TAB_FONT_SPACE
	adc	MATH_TEMP
	sta	MATH_TEMP
:
	; calculate X
	sec
	lda	#160
	sbc	MATH_TEMP
	lsr
	sta	MATH_TEMP

	;; draw visible items

	; selection highlight
	lda	(TMP_PTR_L)
	ina
	asl
	asl
	clc
	adc	#STRUCT_MENU_ITEM_TEXT
	sta	FUNC_ARG_L

	; max - top
	ldy	#STRUCT_MENU_MAX
	lda	(TMP_PTR_L),y
	dey
	sec
	sbc	(TMP_PTR_L),y

	; get draw count
	cmp	TAB_MENU_MAX
	bcc	:+
	lda	TAB_MENU_MAX
:
	tax

	; top item
	lda	(TMP_PTR_L),y
	asl
	asl
	clc
	adc	#STRUCT_MENU_ITEM_TEXT
	tay

	; item loop
@loop_item:
	; text pointer
	jsr	get_text_ptr

	; function pointer
	iny
	iny

	; X
	lda	MATH_TEMP
	sta	ARG_TEXT_X

	; check entry type
	lda	ARG_TEXT_PTR_H
	bmi	@normal

	; fix
	ora	#$80
	sta	ARG_TEXT_PTR_H

	; center X
	jsr	text_width
	sec
	lda	#160
	sbc	RET_TEXT_LEN
	lsr
	sta	ARG_TEXT_X

	; color
	lda	#12

	bra	@set_color

@normal:
	; color
	lda	#14
	cpy	FUNC_ARG_L
	bne	@set_color
	dea
@set_color:
	sta	ARG_TEXT_C

	; write
	jsr	text_write

	; Y += font_height
	clc
	lda	ARG_TEXT_Y
	adc	TAB_FONT_HEIGHT
	sta	ARG_TEXT_Y

	; next
	dex
	bne	@loop_item

	; clear other characters
	jsr	text_clean

	; re-enable math
	jsr	math_setup_vera

	rts

;;;
; read text pointer
;;;

get_text_ptr:
	lda	(TMP_PTR_L),y
	sta	ARG_TEXT_PTR_L
	iny
	lda	(TMP_PTR_L),y
	sta	ARG_TEXT_PTR_H
	iny
	rts

;;;
; menu strings
;;;

mtxt_new_game:
	.asciiz	"New Game"
mtxt_options:
	.asciiz	"Options"
mtxt_load_game:
	.asciiz	"Load Game"
mtxt_save_game:
	.asciiz	"Save Game"
mtxt_keyboard:
	.asciiz	"Keyboard"

;; keyboard

mtxt_ctrl_movement:
	.asciiz	"Movement"
mtxt_ctrl_forward:
	.asciiz	"Forward"
mtxt_ctrl_backward:
	.asciiz	"Backward"
mtxt_ctrl_jump:
	.asciiz	"Jump"
mtxt_ctrl_crouch:
	.asciiz	"Crouch"
mtxt_ctrl_strafing:
	.asciiz	"Strafing"
mtxt_ctrl_turning:
	.asciiz	"Turning"
mtxt_ctrl_left:
	.asciiz	"Left"
mtxt_ctrl_right:
	.asciiz	"Right"
mtxt_ctrl_attack:
	.asciiz	"Attack"
mtxt_ctrl_primary:
	.asciiz	"Primary"
mtxt_ctrl_secondary:
	.asciiz	"Secondary"

;;;;

mtxt_test:
	.asciiz	"... TeSt ..."

;;;
; MENU: keyboard
;;;

mptr_keyboard:
	; position
	.byte	1
	.byte	0
	; numitems
	.byte	14
	; special
	.byte	1
	; previous
	.word	mptr_options
	; title
	.word	mtxt_keyboard
;;	; item 0 [S]
	.word	mtxt_ctrl_movement & $7FFF
	.word	$0000	; TODO
	; item 1
	.word	mtxt_ctrl_forward
	.word	$0000	; TODO
	; item 2
	.word	mtxt_ctrl_backward
	.word	$0000	; TODO
	; item 3
	.word	mtxt_ctrl_jump
	.word	$0000	; TODO
	; item 4
	.word	mtxt_ctrl_crouch
	.word	$0000	; TODO
;;	; item 5 [S]
	.word	mtxt_ctrl_strafing & $7FFF
	.word	$0000	; TODO
	; item 6
	.word	mtxt_ctrl_left
	.word	$0000	; TODO
	; item 7
	.word	mtxt_ctrl_right
	.word	$0000	; TODO
;;	; item 8 [S]
	.word	mtxt_ctrl_turning & $7FFF
	.word	$0000	; TODO
	; item 9
	.word	mtxt_ctrl_left
	.word	$0000	; TODO
	; item 10
	.word	mtxt_ctrl_right
	.word	$0000	; TODO
;;	; item 11 [S]
	.word	mtxt_ctrl_attack & $7FFF
	.word	$0000	; TODO
	; item 12
	.word	mtxt_ctrl_primary
	.word	$0000	; TODO
	; item 13
	.word	mtxt_ctrl_secondary
	.word	$0000	; TODO

;;;
; MENU: options
;;;

mptr_options:
	; position
	.byte	0
	.byte	0
	; numitems
	.byte	2
	; special
	.byte	0
	; previous
	.word	mptr_main
	; title
	.word	mtxt_options
	; item 0
	.word	mtxt_keyboard
	.word	mptr_keyboard
	; item 1
	.word	mtxt_test
	.word	$0000	; TODO

;;;
; MENU: main
;;;

mptr_main:
	; position
	.byte	0
	.byte	0
	; numitems
	.byte	4
	; special
	.byte	0
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
	.word	mtxt_load_game
	.word	$0000	; TODO
	; item 3
	.word	mtxt_save_game
	.word	$0000	; TODO
