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
; Y is current thing; 0 = player weapon; (does not have to be preserved)
; THANIM_PTR_* points to current frame
; ARG_THING_SPAWN_SECTOR is current thing sector; invalid for weapons
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
; get args
;;;

handle_args:
	phy
	ldy	#THSTATE_STRUCT_ARG0
	lda	(THANIM_PTR_L),y
	sta	TMP_STACT_ARG_0
	iny
	lda	(THANIM_PTR_L),y
	sta	TMP_STACT_ARG_1
	iny
	lda	(THANIM_PTR_L),y
	sta	TMP_STACT_ARG_2
	ply
	rts

;;;
; get base info
;;;

handle_base_info:
	; args
	jsr	handle_args

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

	; spawn type
	lda	TMP_STACT_ARG_0
	ora	#>THTYPE_STRUCT_SPAWN_A
	sta	@opcode+2
@opcode:
	lda	THTYPE_STRUCT_SPAWN_A,y	; modified address
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

	; angle + pitch
	jsr	aim_rng
	clc
	lda	ARG_HITSCAN_ANGLE
	adc	TMP_STACT_ARG_2
	sta	THING_STRUCT_ANGLE,x
	lda	ARG_HITSCAN_HALF_PITCH
	asl
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

@loop:
	; save
	phx
	phy
	lda	ARG_HITSCAN_TYPE
	pha

	; angle + pitch
	jsr	aim_rng

	; attack
	jsr	hitscan_attack

	; restore
	pla
	sta	ARG_HITSCAN_TYPE
	ply
	plx

	; next
	dec	TMP_STACT_ARG_2
	bne	@loop

	; no changes
	clc
	rts

;;;
; effect: blood splat
;;;

act_eff_blood:
	; args
	jsr	handle_args

	; tmp = rng_get()
	jsr	rng_get

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; th->mz = 0
	lda	#0
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y

	; if(st->arg[0])
	ldx	TMP_STACT_ARG_0
	beq	@no_power

	; tmp & 0x80
	lda	MATH_TEMP
	and	#$80
	sta	MATH_VAR_L

	; th->angle += 0x40
	clc
	lda	THING_STRUCT_ANGLE,y
	adc	#$40

	; th->angle ^= tmp & 0x80
	eor	MATH_VAR_L
	sta	THING_STRUCT_ANGLE,y

	; thing_launch(tdx, st->arg[0])
	jsr	thing_launch

	; th->mz = st->arg[0] << 8
	lda	TMP_STACT_ARG_0
	sta	THING_STRUCT_MZ_L,y

@no_power:
	; tmp & 0x7F
	lda	MATH_TEMP
	and	#$7F

	; if(st->arg[2] > (tmp & 0x7F))
	cmp	TMP_STACT_ARG_2
	bcs	@no_boost

	; th->mz = st->arg[1] << 8
	lda	TMP_STACT_ARG_1
	sta	THING_STRUCT_MZ_L,y

@no_boost:
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
; X is randomizer index to table
; randomizer will never pick zero
; result is in A

randomize:
	jsr	rng_get
	lda	rng_aim_tab-1,x
	and	MATH_TEMP
	inc
	bpl	:+
	eor	#$7F
	inc
:
	clc
	rts

;;;
; angle and pitch with RNG from ARG1
;;;

aim_rng:
	phx

	; angle
	lda	THING_STRUCT_ANGLE,y
	sta	ARG_HITSCAN_ANGLE

	; random
	lda	TMP_STACT_ARG_1
	and	#$07
	beq	:+
	tax
	lda	rng_aim_tab-1,x
	sta	MATH_COUNTER
	jsr	randomize
	adc	ARG_HITSCAN_ANGLE
	sta	ARG_HITSCAN_ANGLE
:
	; pitch
	lda	THING_STRUCT_PITCH,y
	lsr
	sta	ARG_HITSCAN_HALF_PITCH

	; random
	lda	TMP_STACT_ARG_1
	and	#$70
	beq	:+
	tax
	lda	TAB_SWAP,x
	tax
	lda	rng_aim_tab-1,x
	sta	MATH_COUNTER
	jsr	randomize
	adc	ARG_HITSCAN_HALF_PITCH
	sta	ARG_HITSCAN_HALF_PITCH
:
	plx
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
	.word	act_eff_blood

;;;
; extra tables
;;;

rng_aim_tab:
	.byte	$80
	.byte	$81
	.byte	$83
	.byte	$87
	.byte	$8F
	.byte	$9F
	.byte	$BF

