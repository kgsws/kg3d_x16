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

.export input_init
.export input_clear
.export input_process

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
	lda	REG_RAM_BANK
	pha
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK
	jsr	ex_input_init
	pla
	sta	REG_RAM_BANK
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
; input process
;;;

ex_input_process:
	;;;;;;;;;;;;;;;;;;;;;;;;
	; read input device(s)

	lda	INPUT_MOUSE_BTN
	sta	INPUT_MOUSE_A

	stz	INPUT_MOUSE_TMP_X
	stz	INPUT_MOUSE_TMP_Y

	stz	INPUT_WEAPON_SLOT

@loop:
	; PS2 poll
	lda	#e_ps2data_fetch
	jsr	k_extapi

	; get input
	lda	#e_ps2data_raw
	jsr	k_extapi
	beq	@loop_stop

	; check weapon slots
	cmp	#$82
	bcc	:+
	cmp	#$8C
	bcs	:+
	sbc	#$80
	sta	INPUT_WEAPON_SLOT
	bra	@skip_keys
:
	; check F5 release
	cmp	#$F4
	bne	:+

	; toggle details
	lda	#$04
	sta	VERA_CTRL
	stz	VERA_FX_CTRL
	lda	#$FF
	jsr	render_set_detail
	jsr	math_setup_vera
	bra	@skip_keys
:
	; check keyboard
	asl
	beq	@skip_keys

	; check key binding
	tay
	bcc	:+

	; unset
	lda	TAB_INPUT_BITS,y
	trb	INPUT_STATE_L
	iny
	lda	TAB_INPUT_BITS,y
	trb	INPUT_STATE_H

	bra	@skip_keys
:
	; set
	lda	TAB_INPUT_BITS,y
	tsb	INPUT_STATE_L
	iny
	lda	TAB_INPUT_BITS,y
	tsb	INPUT_STATE_H

@skip_keys:
	; check mouse
	txa
	beq	@loop

	; X
	clc
	lda	INPUT_MOUSE_TMP_X
	adc	INPUT_MOUSE_B
	sta	INPUT_MOUSE_TMP_X

	; Y
	clc
	lda	INPUT_MOUSE_TMP_Y
	adc	INPUT_MOUSE_C
	sta	INPUT_MOUSE_TMP_Y

	; loop check
	bra	@loop

@loop_stop:

	;;;;;;;;;;;;;;;;;;;;;;;;
	; generate tick command

	;; mouse X
	lda	INPUT_MOUSE_TMP_X
	beq	@skip_mouse_x

	;; dx = mx * sensitivity

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; mx
	stz	VERA_FX_CACHE_M
	lda	INPUT_MOUSE_TMP_X
	sta	VERA_FX_CACHE_L
	bpl	:+
	lda	#$FF
	sta	VERA_FX_CACHE_M
:
	; sensitivity
	lda	TAB_MOUSE_SENS_H
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
	lda	INPUT_MOUSE_TMP_Y
	beq	@skip_mouse_y

	;; dy = my * sensitivity

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; my
	stz	VERA_FX_CACHE_M
	lda	INPUT_MOUSE_TMP_Y
	sta	VERA_FX_CACHE_L
	bpl	:+
	lda	#$FF
	sta	VERA_FX_CACHE_M
:
	; sensitivity
	lda	TAB_MOUSE_SENS_V
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
	lda	INPUT_MOUSE_A
	sta	INPUT_MOUSE_BTN
	and	#3
	asl
	tsb	TICK_CMD_BITS_L

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

	; clear all input bits
	lda	#$00
	ldy	#$00
@clr_loop:
	sta	TAB_INPUT_BITS,y
	iny
	bne	@clr_loop

	; mouse sensitivity
	lda	#$90
	sta	TAB_MOUSE_SENS_H
	lda	#$80
	sta	TAB_MOUSE_SENS_V

	; $12 'W'
	ldy	#$24
	lda	#INBIT_FORWARD
	sta	TAB_INPUT_BITS,y

	; $20 'S'
	ldy	#$40
	lda	#INBIT_BACKWARD
	sta	TAB_INPUT_BITS,y

	; $1F 'A'
	ldy	#$3E
	lda	#INBIT_SLEFT
	sta	TAB_INPUT_BITS,y

	; $21 'D'
	ldy	#$42
	lda	#INBIT_SRIGHT
	sta	TAB_INPUT_BITS,y

	; $3D space
	ldy	#$7A
	lda	#INBIT_GO_UP
	sta	TAB_INPUT_BITS,y

	; $30 'C'
	ldy	#$60
	lda	#INBIT_GO_DOWN
	sta	TAB_INPUT_BITS,y

	; $3A lctrl
	ldy	#$75
	lda	#INBIT_ATTACK
	sta	TAB_INPUT_BITS,y

	; $3C lalt
	ldy	#$79
	lda	#INBIT_ALTACK
	sta	TAB_INPUT_BITS,y

	; $13 'E'
	ldy	#$26
	lda	#INBIT_USE
	sta	TAB_INPUT_BITS,y

	; $53 up
	ldy	#$A6
	lda	#INBIT_FORWARD
	sta	TAB_INPUT_BITS,y

	; $54 down
	ldy	#$A8
	lda	#INBIT_BACKWARD
	sta	TAB_INPUT_BITS,y

	; $4F left
	ldy	#$9F
	lda	#INBIT_TLEFT
	sta	TAB_INPUT_BITS,y

	; $59 right
	ldy	#$B3
	lda	#INBIT_TRIGHT
	sta	TAB_INPUT_BITS,y

	; done
	rts

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

