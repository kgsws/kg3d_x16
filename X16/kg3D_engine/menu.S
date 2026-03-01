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

.export menu_init
.export menu_toggle

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
	stz	TAB_HUD_UPDATE
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
; enter or leave the menu
;;;

menu_toggle:
	; clear current input
	stz	INPUT_STATE_L
	stz	INPUT_STATE_H

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

	; set keyboard handler
	lda	#<khdl_menu
	sta	input_menu_opcode+1
	lda	#>khdl_menu
	sta	input_menu_opcode+2

	; draw items
	jmp	menu_update

@disable:
	; save config
	jsr	input_save_config

	; set keyboard handler
	lda	#<input_handle_key
	sta	input_menu_opcode+1
	lda	#>input_handle_key
	sta	input_menu_opcode+2

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
	jsr	text_clean_all

	; re-enable math
	jsr	math_setup_vera

	; update HUD
	sta	TAB_HUD_UPDATE

	rts

;;;
; menu input
;;;

khdl_menu:
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

	; ENTER (release)
	cmp	#$AB
	beq	@select

	; LEFT
	cmp	#$4F
	beq	@select

	; RIGHT
	cmp	#$59
	beq	@select

	; none
@done:
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
	; save scan code
	sta	MATH_TEMP

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
	bmi	:+
	ora	#$80
	tay

	; check key release
	lda	MATH_TEMP
	bpl	@done

	; save pointer
	sty	TAB_MENU_ACTIVE+0
	lda	MATH_VAR_L
	sta	TAB_MENU_ACTIVE+1

	; change menu
	jsr	set_menu_ptr
	bra	menu_update
:
	; run item callback
	sta	MATH_VAR_H
	lda	MATH_TEMP
	jmp	(MATH_VAR_L)

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
	lda	TAB_HUD_MENU_COLOR_TITLE
	sta	ARG_TEXT_C

	; write
	jsr	text_write_la

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

	; selection highlight
	lda	(TMP_PTR_L)
	ina
	asl
	asl
	clc
	adc	#STRUCT_MENU_ITEM_TEXT
	sta	SECTOR_PTR_I

	; check special
	ldy	#STRUCT_MENU_SPECIAL
	lda	(TMP_PTR_L),y
	beq	@not_special

	; fake 7 extra spaces
	lda	TAB_FONT_SPACE
	asl
	asl
	asl
	adc	MATH_TEMP
	sec
	sbc	TAB_FONT_SPACE
	sta	MATH_TEMP

	; calculate X
	clc
	lda	#160
	adc	MATH_TEMP
	ror
	sta	TMP_PTR_I

	; draw special part
	lda	(TMP_PTR_L),y
	tax
	lda	ARG_TEXT_Y
	pha
	jsr	draw_special
	pla
	sta	ARG_TEXT_Y

@not_special:
	; calculate X
	sec
	lda	#160
	sbc	MATH_TEMP
	lsr
	sta	MATH_TEMP

	;; draw visible items

	; prepare entry list
	jsr	prepare_draw

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

	; fix pointer
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
	lda	TAB_HUD_MENU_COLOR_GROUP

	bra	@set_color

@normal:
	; color
	lda	TAB_HUD_MENU_COLOR_ITEM
	cpy	SECTOR_PTR_I
	bne	@set_color
	lda	TAB_HUD_MENU_COLOR_SELECT
@set_color:
	sta	ARG_TEXT_C

	; write
	jsr	text_write_la

	; Y += font_height
	clc
	lda	ARG_TEXT_Y
	adc	TAB_FONT_HEIGHT
	sta	ARG_TEXT_Y

	; next
	dex
	bne	@loop_item

	; clear other characters
	jsr	text_clean_all

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
; prepare entry draw
;;;

prepare_draw:
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

	rts

;;;
; special drawing
;;;

draw_special:
	jmp	(@spec_tab-2,x)

@spec_tab:
	.word	draw_keys
	.word	draw_mouse
	.word	draw_video

;;;
; config special drawing
;;;
; MENUCFG_PTR_* is config offset table for each entry
; FUNC_ARG_* is entry drawer

draw_config:
	; config source
	ldy	#STRUCT_MENU_TOP
	lda	(TMP_PTR_L),y
	tay
	lda	(MENUCFG_PTR_L),y
	sta	MENUCFG_PTR_L
	lda	#>TAB_CONFIG_BASE
	sta	MENUCFG_PTR_H

	; prepare entry list
	jsr	prepare_draw

	; item loop
@loop_item:
	; text pointer
	iny
	lda	(TMP_PTR_L),y
	pha
	iny

	; function pointer
	iny
	iny

	; color
	lda	TAB_HUD_MENU_COLOR_EXTRA
	cpy	SECTOR_PTR_I
	bne	@set_color
	lda	TAB_HUD_MENU_COLOR_EXSEL
@set_color:
	sta	ARG_TEXT_C

	; check entry type
	pla
	bpl	@skip

	; entry drawer
	jsr	func_jsr

@skip:
	; Y += font_height
	clc
	lda	ARG_TEXT_Y
	adc	TAB_FONT_HEIGHT
	sta	ARG_TEXT_Y

	; next
	dex
	bne	@loop_item

	; done
	rts

;;;
; write special entry text
;;;

write_spec_int:
	; conversion result
	lda	#<math_txtint
	sta	ARG_TEXT_PTR_L
	lda	#>math_txtint
	sta	ARG_TEXT_PTR_H

write_spec_ent:
	; X
	jsr	text_width
	sec
	lda	TMP_PTR_I
	sbc	RET_TEXT_LEN
	sta	ARG_TEXT_X

	; write
	jmp	text_write_la

;;;
; special: keyboard
;;;

draw_keys:
	; base text pointer
	lda	#>key_names_start
	sta	ARG_TEXT_PTR_H

	; entry drawer
	lda	#<@ent_draw
	sta	FUNC_ARG_L
	lda	#>@ent_draw
	sta	FUNC_ARG_H

	; config source
	lda	#<conf_top_keys
	sta	MENUCFG_PTR_L
	lda	#>conf_top_keys
	sta	MENUCFG_PTR_H

	; draw
	jmp	draw_config

@ent_draw:
	phx

	; read config
	lda	(MENUCFG_PTR_L)
	inc	MENUCFG_PTR_L
	tax
	lda	key_table,x

	; check unknown
	bne	@is_known

	; generate key code
	txa
	and	#$0F
	jsr	get_hex
	sta	knam_nib_lo
	lda	TAB_SWAP,x
	and	#$0F
	jsr	get_hex
	sta	knam_nib_hi

	; text pointer
	lda	#<key_names_start
	sta	ARG_TEXT_PTR_L

	bra	@write

@is_known:
	; check type
	bpl	@is_text

	; generate character
	and	#$7F
	sta	knam_nib_lo

	; text pointer
	lda	#<knam_nib_lo
	sta	ARG_TEXT_PTR_L

	bra	@write

@is_text:
	; text pointer
	sta	ARG_TEXT_PTR_L

@write:
	; write
	jsr	write_spec_ent

	plx
	rts

sel_key_set:
	cmp	#$AB
	beq	:+
	rts
:
	; set keyboard handler
	lda	#<khdl_setkey
	sta	input_menu_opcode+1
	lda	#>khdl_setkey
	sta	input_menu_opcode+2

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; reset sprites
	stz	TAB_TEXT_I

	; color
	lda	TAB_HUD_MENU_COLOR_TITLE
	sta	ARG_TEXT_C

	; Y
	sec
	lda	#60
	sbc	TAB_FONT_HEIGHT
	sta	ARG_TEXT_Y

	; prompt
	lda	#<mtxt_key_in_0
	sta	ARG_TEXT_PTR_L
	lda	#>mtxt_key_in_0
	sta	ARG_TEXT_PTR_H
	jsr	text_write_ca

	; Y
	lda	#60
	sta	ARG_TEXT_Y

	; prompt
	lda	#<mtxt_key_in_1
	sta	ARG_TEXT_PTR_L
	lda	#>mtxt_key_in_1
	sta	ARG_TEXT_PTR_H
	jsr	text_write_ca

	; clear other characters
	jsr	text_clean_all

	; re-enable math
	jmp	math_setup_vera

khdl_setkey:
	; check key release
	bmi	:+
	rts
:
	and	#$7F

	; current menu
	jsr	set_menu_ptr

	; check ESC
	cmp	#$6E
	bcs	@cancel

	; the key
	pha

	; config option
	lda	(TMP_PTR_L)
	tax
	lda	conf_top_keys,x

	; config pointer
	sta	FUNC_ARG_L
	lda	#>TAB_CONFIG_BASE
	sta	FUNC_ARG_H

	; save
	pla
	sta	(FUNC_ARG_L)

	; apply
	jsr	input_apply_keys

@cancel:
	; set keyboard handler
	lda	#<khdl_menu
	sta	input_menu_opcode+1
	lda	#>khdl_menu
	sta	input_menu_opcode+2

	; draw items
	jmp	menu_update

;;;
; special: mouse
;;;

draw_mouse:
	; entry drawer
	lda	#<@ent_draw
	sta	FUNC_ARG_L
	lda	#>@ent_draw
	sta	FUNC_ARG_H

	; config source
	lda	#<conf_top_mouse
	sta	MENUCFG_PTR_L
	lda	#>conf_top_mouse
	sta	MENUCFG_PTR_H

	; draw
	jmp	draw_config

@ent_draw:
	; check buttons
	lda	MENUCFG_PTR_L
	cmp	#<TAB_CFG_MOUSE_BTN_L

	; read config
	lda	(MENUCFG_PTR_L)
	inc	MENUCFG_PTR_L

	; buttons branch
	bcs	@btn_draw

	; config stuff
	pha

	; enable math
	jsr	math_setup_vera

	; value
	pla
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; convert
	jsr	math_u16
	jsr	math_intclip

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; write
	jsr	write_spec_int

	rts

@btn_draw:
	phx

	; text lookup
	tax
	lda	mnam_offs,x
	sta	ARG_TEXT_PTR_L
	lda	#>mnam_offs
	sta	ARG_TEXT_PTR_H

	; write
	jsr	write_spec_ent

	plx
	rts

sel_mouse_set:
	bpl	:+
	rts
:
	; opcode DEC or INC // $CE or $EE
	asl
	eor	#$50
	ora	#$0E
	sta	@opcode

	; config option
	lda	(TMP_PTR_L)
	tax
	lda	conf_top_mouse,x

	; config pointer
	sta	@opcode+1

	; change
@opcode:
	inc	TAB_CONFIG_BASE

	; update mouse
	jsr	input_apply_mouse

	; draw items
	jmp	menu_update

;;;
; special: video
;;;

draw_video:
	; entry drawer
	lda	#<@ent_draw
	sta	FUNC_ARG_L
	lda	#>@ent_draw
	sta	FUNC_ARG_H

	; config source
	lda	#<conf_top_video
	sta	MENUCFG_PTR_L
	lda	#>conf_top_video
	sta	MENUCFG_PTR_H

	; draw
	jmp	draw_config

@ent_draw:
	; read config
	lda	(MENUCFG_PTR_L)
	inc	MENUCFG_PTR_L
	pha

	; enable math
	jsr	math_setup_vera

	; value
	pla
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; convert
	jsr	math_u16
	jsr	math_intclip

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; write
	jsr	write_spec_int

	rts

sel_video_set:
	bpl	:+
	rts
:
	; opcode DEC or INC // $CE or $EE
	asl
	eor	#$50
	ora	#$0E
	sta	@opcode

	; config option
	lda	(TMP_PTR_L)
	tax
	lda	conf_top_video,x

	; config pointer
	sta	@opcode+1

	; change
@opcode:
	inc	TAB_CONFIG_BASE

	; update video
	jsr	video_set_scale

	; draw items
	jmp	menu_update

;;;;;;;;;;;;;;;;;;;;;

;;;
; prompt strings
;;;

mtxt_key_in_0:
	.asciiz	"Press a new key"
mtxt_key_in_1:
	.asciiz	"or ESC to cancel."

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
mtxt_mouse:
	.asciiz	"Mouse"
mtxt_video:
	.asciiz	"Video"

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
mtxt_ctrl_other:
	.asciiz	"Other"
mtxt_ctrl_use:
	.asciiz	"Use"

;; mouse and video

mtxt_scale:
	.asciiz	"Scale"
mtxt_horizontal:
	.asciiz	"Horizontal"
mtxt_vertical:
	.asciiz	"Vertical"

;; video

mtxt_offset:
	.asciiz	"Offset"

;; mouse

mtxt_buttons:
	.asciiz	"Buttons"
mtxt_middle:
	.asciiz	"Middle"

;;;;;;;;;;;;;;;;

.align  256

;;;
; MENU: keyboard
;;;

mptr_keyboard:
	; position
	.byte	1
	.byte	0
	; numitems
	.byte	16
	; special
	.byte	2
	; previous
	.word	mptr_options
	; title
	.word	mtxt_keyboard
;;	; item 0 [S]
	.word	mtxt_ctrl_movement & $7FFF
	.word	$0000
	; item 1
	.word	mtxt_ctrl_forward
	.word	sel_key_set
	; item 2
	.word	mtxt_ctrl_backward
	.word	sel_key_set
	; item 3
	.word	mtxt_ctrl_jump
	.word	sel_key_set
	; item 4
	.word	mtxt_ctrl_crouch
	.word	sel_key_set
;;	; item 5 [S]
	.word	mtxt_ctrl_strafing & $7FFF
	.word	$0000
	; item 6
	.word	mtxt_ctrl_left
	.word	sel_key_set
	; item 7
	.word	mtxt_ctrl_right
	.word	sel_key_set
;;	; item 8 [S]
	.word	mtxt_ctrl_turning & $7FFF
	.word	$0000
	; item 9
	.word	mtxt_ctrl_left
	.word	sel_key_set
	; item 10
	.word	mtxt_ctrl_right
	.word	sel_key_set
;;	; item 11 [S]
	.word	mtxt_ctrl_attack & $7FFF
	.word	$0000
	; item 12
	.word	mtxt_ctrl_primary
	.word	sel_key_set
	; item 13
	.word	mtxt_ctrl_secondary
	.word	sel_key_set
;;	; item 14 [S]
	.word	mtxt_ctrl_other & $7FFF
	.word	$0000
	; item 15
	.word	mtxt_ctrl_use
	.word	sel_key_set

;;;
; MENU: mouse
;;;

mptr_mouse:
	; position
	.byte	1
	.byte	0
	; numitems
	.byte	7
	; special
	.byte	4
	; previous
	.word	mptr_options
	; title
	.word	mtxt_mouse
;;	; item 0 [S]
	.word	mtxt_scale & $7FFF
	.word	$0000
	; item 1
	.word	mtxt_horizontal
	.word	sel_mouse_set
	; item 2
	.word	mtxt_vertical
	.word	sel_mouse_set
;;	; item 3 [S]
	.word	mtxt_buttons & $7FFF
	.word	$0000
	; item 4
	.word	mtxt_ctrl_left
	.word	sel_mouse_set
	; item 5
	.word	mtxt_ctrl_right
	.word	sel_mouse_set
	; item 6
	.word	mtxt_middle
	.word	sel_mouse_set

;;;
; MENU: video
;;;

mptr_video:
	; position
	.byte	1
	.byte	0
	; numitems
	.byte	6
	; special
	.byte	6
	; previous
	.word	mptr_options
	; title
	.word	mtxt_video
;;	; item 0 [S]
	.word	mtxt_scale & $7FFF
	.word	$0000
	; item 1
	.word	mtxt_horizontal
	.word	sel_video_set
	; item 2
	.word	mtxt_vertical
	.word	sel_video_set
;;	; item 3 [S]
	.word	mtxt_offset & $7FFF
	.word	$0000
	; item 4
	.word	mtxt_horizontal
	.word	sel_video_set
	; item 5
	.word	mtxt_vertical
	.word	sel_video_set

;;;
; MENU: options
;;;

mptr_options:
	; position
	.byte	0
	.byte	0
	; numitems
	.byte	3
	; special
	.byte	0
	; previous
	.word	mptr_main
	; title
	.word	mtxt_options
	; item 0
	.word	mtxt_keyboard
	.word	mptr_keyboard & $7FFF
	; item 1
	.word	mtxt_mouse
	.word	mptr_mouse & $7FFF
	; item 2
	.word	mtxt_video
	.word	mptr_video & $7FFF

;;;
; MENU: dummy TO BE REMOVED
;;;

dtxt_no:
	.asciiz	"this item is"
dtxt_pe:
	.asciiz	"not implemented"

mptr_dummy:
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
	.word	$0000	; replaced with LOGO
	; item 0
	.word	dtxt_no
	.word	menu_toggle
	; item 1
	.word	dtxt_pe & $7FFF
	.word	menu_toggle

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
	.word	mptr_dummy & $7FFF ; TODO
	; item 1
	.word	mtxt_options
	.word	mptr_options & $7FFF
	; item 2
	.word	mtxt_load_game
	.word	mptr_dummy & $7FFF ; TODO
	; item 3
	.word	mtxt_save_game
	.word	mptr_dummy & $7FFF ; TODO

;;;
; top offsets
;;;
; list of elements must be sequential as it is used as start index
; separator entry must match entry below it
; (separators are the only reason these tables exist)

conf_top_keys:
	.byte	<TAB_CFG_KEY_FORWARD	; [S]
	.byte	<TAB_CFG_KEY_FORWARD
	.byte	<TAB_CFG_KEY_BACKWARD
	.byte	<TAB_CFG_KEY_JUMP
	.byte	<TAB_CFG_KEY_CROUCH
	.byte	<TAB_CFG_KEY_S_LEFT	; [S]
	.byte	<TAB_CFG_KEY_S_LEFT
	.byte	<TAB_CFG_KEY_S_RIGHT
	.byte	<TAB_CFG_KEY_T_LEFT	; [S]
	.byte	<TAB_CFG_KEY_T_LEFT
	.byte	<TAB_CFG_KEY_T_RIGHT
	.byte	<TAB_CFG_KEY_ATK_P	; [S]
	.byte	<TAB_CFG_KEY_ATK_P
	.byte	<TAB_CFG_KEY_ATK_S
	.byte	<TAB_CFG_KEY_USE	; [S]
	.byte	<TAB_CFG_KEY_USE

conf_top_mouse:
	.byte	<TAB_CFG_MOUSE_SENS_H	; [S]
	.byte	<TAB_CFG_MOUSE_SENS_H
	.byte	<TAB_CFG_MOUSE_SENS_V
	.byte	<TAB_CFG_MOUSE_BTN_L	; [S]
	.byte	<TAB_CFG_MOUSE_BTN_L
	.byte	<TAB_CFG_MOUSE_BTN_R
	.byte	<TAB_CFG_MOUSE_BTN_M

conf_top_video:
	.byte	<TAB_CFG_VIDEO_SCALE_X	; [S]
	.byte	<TAB_CFG_VIDEO_SCALE_X
	.byte	<TAB_CFG_VIDEO_SCALE_Y
	.byte	<TAB_CFG_VIDEO_OFFS_X	; [S]
	.byte	<TAB_CFG_VIDEO_OFFS_X
	.byte	<TAB_CFG_VIDEO_OFFS_Y

;;;;;;;;;;;;;;;;

.align  256

;;;
; key names
;;;
; all strings must fit into 256 byte block

key_names_start:
	.byte	'$'
knam_nib_hi:
	.byte	'#'
knam_nib_lo:
	.byte	'#'
knam_none:
	.byte	0
knam_up:
	.asciiz	"up"
knam_down:
	.asciiz	"down"
knam_left:
	.asciiz	"left"
knam_right:
	.asciiz	"right"
knam_space:
	.asciiz	"space"
knam_lctrl:
	.asciiz	"Lctrl"
knam_rctrl:
	.asciiz	"Rctrl"
knam_lalt:
	.asciiz	"Lalt"
knam_ralt:
	.asciiz	"Ralt"
knam_lshift:
	.asciiz	"Lshift"
knam_rshift:
	.asciiz	"Rshift"
knam_tab:
	.asciiz	"tab"
knam_enter:
	.asciiz	"enter"
knam_backspace:
	.asciiz	"bkspc"
knam_ins:
	.asciiz	"ins"
knam_del:
	.asciiz	"del"
knam_home:
	.asciiz	"home"
knam_end:
	.asciiz	"end"
knam_pgup:
	.asciiz	"pgUp"
knam_pgdn:
	.asciiz	"pgDn"

;;;
; mouse actions
;;;
; all strings must fit into 256 byte block

mnam_atkpri:
	.asciiz	"pri.atk"
mnam_atkalt:
	.asciiz	"alt.atk"
mnam_use:
	.asciiz	"use"
mnam_none:
	.asciiz	"none"
mnam_offs:
	.byte	<mnam_atkpri, <mnam_atkalt, <mnam_use, <mnam_none

;;;
; key name list
;;;

key_table:
	; 0x00
	.byte	knam_none - key_names_start
	.byte	0
	.byte	$80 + '1'
	.byte	$80 + '2'
	.byte	$80 + '3'
	.byte	$80 + '4'
	.byte	$80 + '5'
	.byte	$80 + '6'
	.byte	$80 + '7'
	.byte	$80 + '8'
	.byte	$80 + '9'
	.byte	$80 + '0'
	.byte	0
	.byte	0
	.byte	0
	.byte	knam_backspace - key_names_start
	; 0x10
	.byte	knam_tab - key_names_start
	.byte	$80 + 'Q'
	.byte	$80 + 'W'
	.byte	$80 + 'E'
	.byte	$80 + 'R'
	.byte	$80 + 'T'
	.byte	$80 + 'Y'
	.byte	$80 + 'U'
	.byte	$80 + 'I'
	.byte	$80 + 'O'
	.byte	$80 + 'P'
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	$80 + 'A'
	; 0x20
	.byte	$80 + 'S'
	.byte	$80 + 'D'
	.byte	$80 + 'F'
	.byte	$80 + 'G'
	.byte	$80 + 'H'
	.byte	$80 + 'J'
	.byte	$80 + 'K'
	.byte	$80 + 'L'
	.byte	0
	.byte	0
	.byte	0
	.byte	knam_enter - key_names_start
	.byte	knam_lshift - key_names_start
	.byte	0
	.byte	$80 + 'Z'
	.byte	$80 + 'X'
	; 0x30
	.byte	$80 + 'C'
	.byte	$80 + 'V'
	.byte	$80 + 'B'
	.byte	$80 + 'N'
	.byte	$80 + 'M'
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	knam_rshift - key_names_start
	.byte	knam_lctrl - key_names_start
	.byte	0
	.byte	knam_lalt - key_names_start
	.byte	knam_space - key_names_start
	.byte	knam_ralt - key_names_start
	.byte	0
	; 0x40
	.byte	knam_rctrl - key_names_start
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	knam_ins - key_names_start
	.byte	knam_del - key_names_start
	.byte	0
	.byte	0
	.byte	knam_left - key_names_start
	; 0x50
	.byte	knam_home - key_names_start
	.byte	knam_end - key_names_start
	.byte	0
	.byte	knam_up - key_names_start
	.byte	knam_down - key_names_start
	.byte	knam_pgup - key_names_start
	.byte	knam_pgdn - key_names_start
	.byte	0
	.byte	0
	.byte	knam_right - key_names_start
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	; 0x60
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
