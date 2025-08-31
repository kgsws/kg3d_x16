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
.include "menu.inc"

.export input_init
.export input_clear
.export input_process
.export input_apply_keys
.export input_apply_mouse
.export input_save_config
.export input_menu_opcode
.export input_handle_key

;; stuff
MOUSE_ACT_COUNT = 3	; 'none' is not counted
MOUSE_BTN_COUNT = 3	; this is forced by config and menu

;; keybind lookup bits

; input key bits L (do not touch - used with 'move_angle' lookup)
INBIT_FORWARD = %00000001
INBIT_BACKWARD = %00000010
INBIT_SLEFT = %00000100
INBIT_SRIGHT = %00001000

; input key bits L
INBIT_GO_UP = %00010000
INBIT_GO_DOWN = %00100000
INBIT_USE = %01000000

; input key bits H
INBIT_ALTACK = %00010000
INBIT_ATTACK = %00100000
INBIT_TLEFT = %01000000
INBIT_TRIGHT = %10000000

;;;
; CODE
;;;

.segment "CODE"

;;;
; input clear
;;;

input_clear:
	stz	INPUT_MOUSE_X_L
	stz	INPUT_MOUSE_X_H
	stz	INPUT_MOUSE_Y_L
	stz	INPUT_MOUSE_Y_H

	; PS2 poll
	lda	#e_ps2data_fetch
	jsr	k_extapi

	; get input
	lda	#e_ps2data_raw
	jsr	k_extapi
	bne	input_clear

	rts

;;;
; input init
;;;

input_init:
	; clear menu
	stz	TAB_MENU_ACTIVE

	; extra code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	; actual init
	jsr	ex_input_init

	rts

;;;
; input process
;;;

input_process:
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK
	jmp	ex_input_process

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.segment "CODE_EXTRA"

;;;
; set file IO for config
;;;

set_conf_file:
	pha
	lda	#(fn_config_end-fn_config)
	ldx	#<fn_config
	ldy	#>fn_config
	jsr	k_SETNAM
	lda	#FILE_LOAD_HANDLE
	ldx	#FILE_DEVICE_ID
	ply
	jsr	k_SETLFS
	lda	#0
	rts

;;;
; input process
;;;

ex_input_process:
	;;;;;;;;;;;;;;;;;;;;;;;;
	; read input device(s)

	lda	INPUT_MOUSE_BTN
	sta	INPUT_MOUSE_A

	stz	INPUT_MOUSE_TMP_XL
	stz	INPUT_MOUSE_TMP_XH
	stz	INPUT_MOUSE_TMP_YL
	stz	INPUT_MOUSE_TMP_YH
	stz	INPUT_WEAPON_SLOT

in_loop:
	; PS2 poll
	lda	#e_ps2data_fetch
	jsr	k_extapi

	; get input
	lda	#e_ps2data_raw
	jsr	k_extapi
	beq	in_loop_stop

	; check keyboard
	ora	#0
	beq	in_skip_keys

	; pass to handler
	phx
input_menu_opcode:
	jsr	input_handle_key
	plx

in_skip_keys:
	; check mouse
	txa
	beq	in_loop

	; B
	lda	INPUT_MOUSE_A
	ora	INPUT_MOUSE_BTN

	; X
	stz	INPUT_MOUSE_TMP_SIGN
	lda	INPUT_MOUSE_B
	bpl	:+
	dec	INPUT_MOUSE_TMP_SIGN
:
	clc
	adc	INPUT_MOUSE_TMP_XL
	sta	INPUT_MOUSE_TMP_XL
	lda	INPUT_MOUSE_TMP_XH
	adc	INPUT_MOUSE_TMP_SIGN
	sta	INPUT_MOUSE_TMP_XH

	; Y
	stz	INPUT_MOUSE_TMP_SIGN
	lda	INPUT_MOUSE_C
	bpl	:+
	dec	INPUT_MOUSE_TMP_SIGN
:
	clc
	adc	INPUT_MOUSE_TMP_YL
	sta	INPUT_MOUSE_TMP_YL
	lda	INPUT_MOUSE_TMP_YH
	adc	INPUT_MOUSE_TMP_SIGN
	sta	INPUT_MOUSE_TMP_YH

	; loop check
	bra	in_loop

in_loop_stop:

	;;;;;;;;;;;;;;;;;;;;;;;;
	; generate tick command

	; check menu flag
	lda	TAB_MENU_ACTIVE
	beq	:+

	; empty ticcmd
	stz	TICK_CMD_BITS_L
	stz	TICK_CMD_BITS_H

	rts
:
	;; mouse X
	lda	INPUT_MOUSE_TMP_XL
	ora	INPUT_MOUSE_TMP_XH
	beq	@skip_mouse_x

	;; dx = mx * sensitivity

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; mx
	lda	INPUT_MOUSE_TMP_XL
	sta	VERA_FX_CACHE_L
	lda	INPUT_MOUSE_TMP_XH
	sta	VERA_FX_CACHE_M

	; sensitivity
	lda	TAB_CFG_MOUSE_SENS_H
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	INPUT_MOUSE_X_L
	adc	VERA_DATA0
	sta	INPUT_MOUSE_X_L
	lda	INPUT_MOUSE_X_H
	adc	VERA_DATA0
	sta	INPUT_MOUSE_X_H

	bpl	@angle_add

	; invert
	eor	#$FF
	sta	MATH_VAR_H
	lda	INPUT_MOUSE_X_L
	eor	#$FF
	ina
	sta	MATH_VAR_L
	bne	:+
	inc	MATH_VAR_H
:
	; sub result
	sec
	lda	TICK_CMD_ANGLE
	sbc	MATH_VAR_H
	sta	TICK_CMD_ANGLE

	; remove delta
	stz	MATH_VAR_H

	; invert
	lda	#$FF
	sta	INPUT_MOUSE_X_H
	lda	MATH_VAR_L
	eor	#$FF
	sta	INPUT_MOUSE_X_L

	bra	@skip_mouse_x

@angle_add:
	; add result
	clc
	lda	TICK_CMD_ANGLE
	adc	INPUT_MOUSE_X_H
	sta	TICK_CMD_ANGLE

	; remove delta
	stz	INPUT_MOUSE_X_H

@skip_mouse_x:
	;; mouse Y
	lda	INPUT_MOUSE_TMP_YL
	ora	INPUT_MOUSE_TMP_YH
	beq	@skip_mouse_y

	;; dy = my * sensitivity

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; my
	lda	INPUT_MOUSE_TMP_YL
	sta	VERA_FX_CACHE_L
	lda	INPUT_MOUSE_TMP_YH
	sta	VERA_FX_CACHE_M

	; sensitivity
	lda	TAB_CFG_MOUSE_SENS_V
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; accumulate (double the value)
	lda	VERA_FX_ACCUM

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	INPUT_MOUSE_Y_L
	adc	VERA_DATA0
	sta	INPUT_MOUSE_Y_L
	lda	INPUT_MOUSE_Y_H
	adc	VERA_DATA0
	sta	INPUT_MOUSE_Y_H

	bpl	@pitch_add

	; invert
	eor	#$FF
	sta	MATH_VAR_H
	lda	INPUT_MOUSE_Y_L
	eor	#$FF
	ina
	sta	MATH_VAR_L
	bne	:+
	inc	MATH_VAR_H
:
	; sub result
	sec
	lda	TICK_CMD_PITCH
	sbc	MATH_VAR_H
	sta	TICK_CMD_PITCH

	; check range
	cmp	#$4A
	bcs	:+
	lda	#$4A
	sta	TICK_CMD_PITCH
:
	; remove delta
	stz	MATH_VAR_H

	; invert
	lda	#$FF
	sta	INPUT_MOUSE_Y_H
	lda	MATH_VAR_L
	eor	#$FF
	sta	INPUT_MOUSE_Y_L

	bra	@skip_mouse_y

@pitch_add:
	; add result
	clc
	lda	TICK_CMD_PITCH
	adc	INPUT_MOUSE_Y_H
	sta	TICK_CMD_PITCH

	; check range
	cmp	#$B7
	bcc	:+
	lda	#$B6
	sta	TICK_CMD_PITCH
:
	; remove delta
	stz	INPUT_MOUSE_Y_H

@skip_mouse_y:

	; prepare bits
	stz	TICK_CMD_BITS_L

	;; turning

	; turn left
	lda	INPUT_STATE_H
	and	#INBIT_TLEFT
	beq	:++
	ldx	TICK_COUNT
:
	sec
	lda	TICK_CMD_ANGLE
	sbc	#5
	sta	TICK_CMD_ANGLE
	dex
	bne	:-
:
	; turn right
	lda	INPUT_STATE_H
	and	#INBIT_TRIGHT
	beq	:++
	ldx	TICK_COUNT
:
	clc
	lda	TICK_CMD_ANGLE
	adc	#5
	sta	TICK_CMD_ANGLE
	dex
	bne	:-
:
	;; movement

	; get move angle from input
	lda	INPUT_STATE_L
	and	#%00001111
	tay
	lda	move_angle,y
	bmi	:+
	asl
	sta	TICK_CMD_BITS_H
	lda	#TCMD_MOVING
	sta	TICK_CMD_BITS_L
:
	;; (mouse) buttons
	lda	INPUT_MOUSE_BTN
	and	#7
	tax
	lda	mouse_btn,x
	tsb	TICK_CMD_BITS_L

	; last button state
	lda	INPUT_MOUSE_A
	sta	INPUT_MOUSE_BTN

	;; (key) buttons

	; jump
	lda	INPUT_STATE_L
	and	#INBIT_GO_UP
	beq	:+
	lda	#TCMD_GO_UP
	tsb	TICK_CMD_BITS_L
:
	; crouch
	lda	INPUT_STATE_L
	and	#INBIT_GO_DOWN
	beq	:+
	lda	#TCMD_GO_DOWN
	tsb	TICK_CMD_BITS_L
:
	; use
	lda	INPUT_STATE_L
	and	#INBIT_USE
	beq	:+
	lda	#TCMD_USE
	tsb	TICK_CMD_BITS_L
:
	; attack
	lda	INPUT_STATE_H
	and	#INBIT_ATTACK
	beq	:+
	lda	#TCMD_ATK
	tsb	TICK_CMD_BITS_L
:
	; altack
	lda	INPUT_STATE_H
	and	#INBIT_ALTACK
	beq	:+
	lda	#TCMD_ALT
	tsb	TICK_CMD_BITS_L
:
	;; weapon changes
	lda	INPUT_WEAPON_SLOT
	tsb	TICK_CMD_BITS_H

	; done
	rts

;;;
; input init
;;;

ex_input_init:
	; enable mouse
	ldx	#0
	ldy	#0
	lda	#$FF
	jsr	k_mouse_config

	; ADDR0 = 0x01FC06, increment = 1
	lda	#$06
	sta	VERA_ADDRx_L
	lda	#$FC
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; disable cursor sprite
	stz	VERA_DATA0

	; disable sprites
	lda	VERA_DC_VIDEO
	and	#%10111111
	sta	VERA_DC_VIDEO

	; clear current input
	stz	INPUT_STATE_L
	stz	INPUT_STATE_H

	; default config
	ldx	#TAB_CONFIG_SIZE-1
@loop_defcfg:
	lda	default_config,x
	sta	TAB_CONFIG_BASE,x
	dex
	bpl	@loop_defcfg

	; patch default config for composite
	lda	REG_VIDEO_MODE
	and	#2
	beq	:+
	lda	#9
	sta	TAB_CFG_VIDEO_OFFS_X
	sta	TAB_CFG_VIDEO_OFFS_Y
	lda	#36
	sta	TAB_CFG_VIDEO_SCALE_X
	lda	#35
	sta	TAB_CFG_VIDEO_SCALE_Y
:
	; reset
	stz	TAB_TEMP_DATA

	; load config from file
	lda	#2
	jsr	set_conf_file
	ldx	#<TAB_TEMP_DATA
	ldy	#>TAB_TEMP_DATA
	jsr	k_LOAD
	bcs	:+
	lda	TAB_TEMP_DATA
	cmp	#CONFIG_VERSION
	bne	:+

	; copy config
	ldx	#TAB_CONFIG_SIZE
@loop_cpy:
	lda	TAB_TEMP_DATA,x
	sta	TAB_CONFIG_BASE,x
	dex
	bpl	@loop_cpy
:
	; apply
	jsr	input_apply_keys
	jsr	input_apply_mouse
	stz	TAB_CONFIG_MAGIC

	; done
	rts

;;;
; apply keys from config
;;;

input_apply_keys:
	; config changed
	lda	#CONFIG_VERSION
	sta	TAB_CONFIG_MAGIC

	; clear all input bits
	lda	#0
	tay
@clr_loop:
	sta	TAB_INPUT_BITS_L,y
	iny
	bne	@clr_loop

	;; apply config

	ldy	TAB_CFG_KEY_FORWARD
	lda	#INBIT_FORWARD
	sta	TAB_INPUT_BITS_L,y

	ldy	TAB_CFG_KEY_BACKWARD
	lda	#INBIT_BACKWARD
	sta	TAB_INPUT_BITS_L,y

	ldy	TAB_CFG_KEY_JUMP
	lda	#INBIT_GO_UP
	sta	TAB_INPUT_BITS_L,y

	ldy	TAB_CFG_KEY_CROUCH
	lda	#INBIT_GO_DOWN
	sta	TAB_INPUT_BITS_L,y

	ldy	TAB_CFG_KEY_S_LEFT
	lda	#INBIT_SLEFT
	sta	TAB_INPUT_BITS_L,y

	ldy	TAB_CFG_KEY_S_RIGHT
	lda	#INBIT_SRIGHT
	sta	TAB_INPUT_BITS_L,y

	ldy	TAB_CFG_KEY_T_LEFT
	lda	#INBIT_TLEFT
	sta	TAB_INPUT_BITS_H,y

	ldy	TAB_CFG_KEY_T_RIGHT
	lda	#INBIT_TRIGHT
	sta	TAB_INPUT_BITS_H,y

	ldy	TAB_CFG_KEY_ATK_P
	lda	#INBIT_ATTACK
	sta	TAB_INPUT_BITS_H,y

	ldy	TAB_CFG_KEY_ATK_S
	lda	#INBIT_ALTACK
	sta	TAB_INPUT_BITS_H,y

	ldy	TAB_CFG_KEY_USE
	lda	#INBIT_USE
	sta	TAB_INPUT_BITS_L,y

	rts

;;;
; apply mouse from config
;;;

input_apply_mouse:
	; config changed
	lda	#CONFIG_VERSION
	sta	TAB_CONFIG_MAGIC

	; check limits and clear
	ldx	#MOUSE_ACT_COUNT-1
@loop_chk:
	lda	TAB_CFG_MOUSE_BTN_L,x
	bpl	:+
	lda	#MOUSE_ACT_COUNT
	sta	TAB_CFG_MOUSE_BTN_L,x
	bra	:++
:
	cmp	#MOUSE_ACT_COUNT+1
	bcc	:+
	stz	TAB_CFG_MOUSE_BTN_L,x
:
	dex
	bpl	@loop_chk

	; clear bits
	ldx	#(1 << MOUSE_BTN_COUNT)-1
@loop_clr:
	stz	mouse_btn,x
	dex
	bpl	@loop_clr

	; assign bits
	lda	#(1 << MOUSE_BTN_COUNT)
	sta	INPUT_MOUSE_B
	ldy	#MOUSE_BTN_COUNT-1
@loop_bit:
	lda	TAB_CFG_MOUSE_BTN_L,y
	tax
	lda	mouse_bit,x
	sta	INPUT_MOUSE_A
	lsr	INPUT_MOUSE_B
	ldx	#7
@loop_opt:
	txa
	and	INPUT_MOUSE_B
	beq	:+
	lda	INPUT_MOUSE_A
	ora	mouse_btn,x
	sta	mouse_btn,x
:
	dex
	bpl	@loop_opt
	dey
	bpl	@loop_bit

	rts

;;;
; save config but only if changed
;;;

input_save_config:
	lda	TAB_CONFIG_MAGIC
	beq	:+

	; copy config
	ldx	#TAB_CONFIG_SIZE
@loop_cpy:
	lda	TAB_CONFIG_BASE,x
	sta	TAB_TEMP_DATA,x
	dex
	bpl	@loop_cpy

	; save
	lda	#1
	jsr	set_conf_file
	ldx	#<TAB_TEMP_DATA+TAB_CONFIG_SIZE
	ldy	#>TAB_TEMP_DATA
	sty	FUNC_ARG_H
	lda	#<TAB_TEMP_DATA
	sta	FUNC_ARG_L
	lda	#FUNC_ARG_L
	jsr	k_BSAVE

	stz	TAB_CONFIG_MAGIC
:
	rts

;;;
; game input handler
;;;

input_handle_key:
	; check ESC
	cmp	#$EE
	bne	:+
	phx
	jsr	menu_toggle
	plx
	rts
:
	; check weapon slots
	cmp	#$82
	bcc	:+
	cmp	#$8C
	bcs	:+
	sbc	#$80
	sta	INPUT_WEAPON_SLOT
	rts
:
	; check key binding
	tay
	bpl	:+
	and	#$7F
	tay

	; unset
	lda	TAB_INPUT_BITS_L,y
	trb	INPUT_STATE_L
	lda	TAB_INPUT_BITS_H,y
	trb	INPUT_STATE_H
	rts
:
	; set
	lda	TAB_INPUT_BITS_L,y
	tsb	INPUT_STATE_L
	lda	TAB_INPUT_BITS_H,y
	tsb	INPUT_STATE_H
	rts

;;;;;;;;;;;;;;;;

.align  256

;;;
; mouse buttons to tick command
;;;

mouse_btn:
	.byte	$00
	.byte	$02
	.byte	$04
	.byte	$06
	.byte	$08
	.byte	$0A
	.byte	$0C
	.byte	$0E

mouse_bit:
	.byte	$02
	.byte	$04
	.byte	$08
	.byte	$00

;;;
; movement angles
;;;

move_angle:
	.byte	$80
	.byte	$00
	.byte	$40
	.byte	$80
	.byte	$60
	.byte	$70
	.byte	$50
	.byte	$60
	.byte	$20
	.byte	$10
	.byte	$30
	.byte	$20
	.byte	$80
	.byte	$00
	.byte	$40
	.byte	$80

;;;
; default config
;;;

default_config:
	; magic
	.byte	0
	; keys
	.byte	18
	.byte	32
	.byte	61
	.byte	58
	.byte	31
	.byte	33
	.byte	79
	.byte	89
	.byte	0
	.byte	0
	.byte	19
	; mouse
	.byte	80
	.byte	72
	.byte	0
	.byte	1
	.byte	2
	; video
	.byte	32
	.byte	32
	.byte	0
	.byte	0

;;;
; config name
;;;

fn_config:
	.byte	"CONFIG"
fn_config_end:
