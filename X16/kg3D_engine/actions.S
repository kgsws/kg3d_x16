.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "tables.inc"
.include "math.inc"
.include "main.inc"
.include "zeropage.inc"
.include "engine.inc"
.include "things.inc"
.include "hitscan.inc"

.export action_run
.export action_get_XY

;;;
; NOTES: action callback
;;;
; Y is current thing; 0 = player weapon
; THANIM_PTR_* points to current frame
; BANK_THING_STATES is active bank
; use 'clc + rts' to continue
; use 'sec + rts' for frame change; active bank must be 'BANK_THING_STRUCT'

;;;
; CODE
;;;

.segment "CODE"

;;;
; get XY
;;;

action_get_XY:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; X
	lda	THING_STRUCT_X_S,y
	sta	ARG_THING_SPAWN_X_S
	lda	THING_STRUCT_X_L,y
	sta	ARG_THING_SPAWN_X_L
	lda	THING_STRUCT_X_H,y
	sta	ARG_THING_SPAWN_X_H

	; Y
	lda	THING_STRUCT_Y_S,y
	sta	ARG_THING_SPAWN_Y_S
	lda	THING_STRUCT_Y_L,y
	sta	ARG_THING_SPAWN_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	ARG_THING_SPAWN_Y_H

	rts

;;;
; get base info
;;;

handle_base_info:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; check for weapon
	tya
	bne	:+

	; use player thing
	ldy	G_PLAYER_THING

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector pointer
	lda	$A000,y
	sta	ARG_THING_SPAWN_SECTOR

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK
:
	; save for return
	phy

	; origin type
	ldx	THING_STRUCT_TYPE,y

	; weapon type
	ldy	TICK_IDX
	lda	THING_STRUCT_TYPE,y
	tay

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; spawn type	; TODO: spawn slot
	lda	THTYPE_STRUCT_SPAWN_A,y
	sta	ARG_THING_SPAWN_TYPE

	; attack height
	lda	THTYPE_STRUCT_ATK_HEIGHT,x

	ply
	rts

;;;
; action: weapon ready
;;;
; can be run only from local weapon

act_wpn_ready:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; not attacking
	stz	THING_STRUCT_IFLAGS

	; cmd = ticcmd.bits_h & 0x1F
	lda	TICK_CMD_BITS_H
	and	#$1F

	; if(cmd)
	beq	@no_swap

	; cmd--
	dea

	; if(cmd < 10)
	cmp	#11
	bcs	@no_swap

	; cmd += THING_WEAPON_FIRST
	clc
	adc	#THING_WEAPON_FIRST
	tax

	; ticcmd.bits_h &= 0xE0
	lda	#$1F
	trb	TICK_CMD_BITS_H

	; if(cmd != th->type)
	txa
	cmp	THING_STRUCT_TYPE
	beq	@no_swap

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; check animation	; TO BE REPLACED
	lda	THTYPE_ANIM_RAISE_L,x
	ora	THTYPE_ANIM_RAISE_H,x
	beq	:+

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; th->counter = cmd
	stx	THING_STRUCT_COUNTER

	; get thing type
	ldx	THING_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; get animation
	lda	THTYPE_ANIM_LOWER_L,x
	sta	TMP_STACT_FRM_L
	lda	THTYPE_ANIM_LOWER_H,x
	sta	TMP_STACT_FRM_H

	jmp	apply_animation
:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

@no_swap:
	; check primary fire
	lda	TICK_CMD_BITS_L
	and	#TCMD_ATK
	beq	@skip_atk

	; get thing type
	ldx	THING_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; check animation
	lda	THTYPE_ANIM_ATK_L,x
	sta	TMP_STACT_FRM_L
	lda	THTYPE_ANIM_ATK_H,x
	sta	TMP_STACT_FRM_H
	ora	TMP_STACT_FRM_L
	beq	@no_atk

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; attacking
	lda	#1
	sta	THING_STRUCT_IFLAGS

	; apply
	jmp	apply_animation_ex

@no_atk:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

@skip_atk:
	; check secondary fire
	lda	TICK_CMD_BITS_L
	and	#TCMD_ALT
	beq	@skip_alt

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; get thing type
	ldx	THING_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; check animation
	lda	THTYPE_ANIM_ALT_L,x
	sta	TMP_STACT_FRM_L
	lda	THTYPE_ANIM_ALT_H,x
	sta	TMP_STACT_FRM_H
	ora	TMP_STACT_FRM_L
	beq	@no_alt

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; attacking
	lda	#2
	sta	THING_STRUCT_IFLAGS

	; apply
	jmp	apply_animation_ex

@no_alt:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

@skip_alt:
	; no changes
	clc
	rts

;;;
; action: weapon raise
;;;
; can be run only from local weapon

act_wpn_raise:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; raise
	sec
	lda	THING_STRUCT_HEIGHT
	sbc	#8
	bcs	:+
	lda	#0
:
	sta	THING_STRUCT_HEIGHT

	; check
	bne	:+

	; get thing type
	ldx	THING_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; get animation
	lda	THTYPE_ANIM_READY_L,x
	sta	TMP_STACT_FRM_L
	lda	THTYPE_ANIM_READY_H,x
	sta	TMP_STACT_FRM_H

	jmp	apply_animation
:
	; no changes
	clc
	rts

;;;
; action: weapon lower
;;;
; can be run only from local weapon

act_wpn_lower:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; lower
	clc
	lda	THING_STRUCT_HEIGHT
	adc	#8
	sta	THING_STRUCT_HEIGHT
	cmp	#64
	bcs	:+

	; no changes
	clc
	rts
:
	; th->height = 64
	lda	#64
	sta	THING_STRUCT_HEIGHT

	; th->type = th->counter
	lda	THING_STRUCT_COUNTER
	sta	THING_STRUCT_TYPE
	tax

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; get animation
	lda	THTYPE_ANIM_RAISE_L,x
	sta	TMP_STACT_FRM_L
	lda	THTYPE_ANIM_RAISE_H,x
	sta	TMP_STACT_FRM_H

	jmp	apply_animation

;;;
; action: projectile attack
;;;

act_atk_projectile:
	; origin
	jsr	handle_base_info
	pha

	; projectile height / 2
	ldx	ARG_THING_SPAWN_TYPE
	lda	THTYPE_STRUCT_HEIGHT,x
	lsr
	sta	MATH_TEMP_A

	; Z offset
	pla
	sec
	sbc	MATH_TEMP_A
	sta	MATH_TEMP_A

	; projectile speed
	lda	THTYPE_STRUCT_SPEED,x
	pha

	; X Y
	jsr	action_get_XY

	; Z
	lda	THING_STRUCT_Z_S,y
	sta	ARG_THING_SPAWN_Z_S
	clc
	lda	THING_STRUCT_Z_L,y
	adc	MATH_TEMP_A
	sta	ARG_THING_SPAWN_Z_L
	lda	THING_STRUCT_Z_H,y
	adc	#0
	sta	ARG_THING_SPAWN_Z_H

	; spawn thing
	phy
	jsr	thing_spawn_org
	ply
	ldx	RET_THING_SPAWN
	beq	:+

	; copy angle and pitch
	lda	THING_STRUCT_ANGLE,y
	sta	THING_STRUCT_ANGLE,x
	lda	THING_STRUCT_PITCH,y
	sta	THING_STRUCT_PITCH,x

	; launch
	ldy	RET_THING_SPAWN
	plx
	jsr	thing_launch
	pha
:
	pla

	; no changes
	clc
	rts

;;;
; action: projectile attack
;;;

act_atk_hitscan:
	; origin
	jsr	handle_base_info
	tax

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; angle
	lda	THING_STRUCT_ANGLE,y
	sta	ARG_HITSCAN_ANGLE

	; random	; TODO
	lda	#$87
	sta	MATH_COUNTER
	jsr	randomize
	adc	ARG_HITSCAN_ANGLE
	sta	ARG_HITSCAN_ANGLE

	; pitch
	lda	THING_STRUCT_PITCH,y
	lsr
	sta	ARG_HITSCAN_HALF_PITCH

	; random	; TODO
	lda	#$81
	sta	MATH_COUNTER
	jsr	randomize
	adc	ARG_HITSCAN_HALF_PITCH
	sta	ARG_HITSCAN_HALF_PITCH

	; attack
	jsr	hitscan_attack

	; no changes
	clc
	rts

;;;
; apply new animation
;;;

apply_animation:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

apply_animation_ex:
	; change animation
	lda	TMP_STACT_FRM_L
	sta	THING_STRUCT_NXTFRM_L
	lda	TMP_STACT_FRM_H
	sta	THING_STRUCT_NXTFRM_H

	; apply
	sec
	rts

;;;
; randomize
;;;
; MATH_COUNTER is randomizer mask
; set MSB to enable negative numbers
; randomizer will never pick zero
; result is in A

randomize:
	jsr	rng_get
	lda	MATH_TEMP
	and	MATH_COUNTER
	inc
	bpl	:+
	eor	#$7F
	inc
:
	clc
	rts

;;;
; state action calling
;;;

action_run:
	asl
	tax
	jmp	(action_table-2,x)

action_table:
	.word	act_wpn_ready
	.word	act_wpn_raise
	.word	act_wpn_lower
	.word	act_atk_projectile
	.word	act_atk_hitscan
	.word	act_TEST

;;

act_TEST:
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	lda	#$72
	sta	THING_STRUCT_PITCH,y

	clc
	lda	THING_STRUCT_ANGLE,y
	adc	#$22
	sta	THING_STRUCT_ANGLE,y

	clc
	rts

