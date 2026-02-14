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

.export action_xy
.export action_table

;;;
; NOTES: action callback
;;;
; X and Y can be trashed
; Y - the thing; 0 = player weapon; (also stored in THTICK_ACTION_TDX)
; THTICK_ACTION_ST_IDX : THTICK_ACTION_ST_BNK - current state (also current bank)
; jump to 'thing_actdone' to continue; BANK can be trashed
; jump to 'thing_actnext'; X is LO, A is HI of new animation, BANK_THING_INFO must be set

;;;
; CODE
;;;

.segment "CODE"

;;;
; copy XY to arguments
;;;
; Y - the thing

action_xy:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; X
	lda	THING_STRUCT_X_S,y
	sta	ARG_THING_X_S
	lda	THING_STRUCT_X_L,y
	sta	ARG_THING_X_L
	lda	THING_STRUCT_X_H,y
	sta	ARG_THING_X_H

	; Y
	lda	THING_STRUCT_Y_S,y
	sta	ARG_THING_Y_S
	lda	THING_STRUCT_Y_L,y
	sta	ARG_THING_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	ARG_THING_Y_H

	rts

;;;
; prepare angles
;;;
; Y - the thing
; uses THTICK_ACTION_SPREAD_*
; stores THTICK_ACTION_ANGLE and THTICK_ACTION_HALF_PITCH
; destroys X

prepare_spread:
	; X (angle)
	lda	THING_STRUCT_ANGLE,y
	ldx	THTICK_ACTION_SPREAD_X
	beq	@set_x

	sec
	sbc	THTICK_ACTION_SPREAD_X

	asl	THTICK_ACTION_SPREAD_X

@set_x:
	sta	THTICK_ACTION_ANGLE

	; Y (pitch)
	lda	THING_STRUCT_PITCH,y
	lsr
	ldx	THTICK_ACTION_SPREAD_Y
	beq	@set_y

	sec
	sbc	THTICK_ACTION_SPREAD_Y

	asl	THTICK_ACTION_SPREAD_Y

@set_y:
	sta	THTICK_ACTION_HALF_PITCH

	rts

;;;
; apply RNG spread
;;;
; uses THTICK_ACTION_SPREAD_* and THTICK_ACTION_ANGLE and THTICK_ACTION_HALF_PITCH
; stores ARG_HITSCAN_ANGLE and ARG_HITSCAN_HALF_PITCH
; destroys X

apply_spread:
	; X (angle)
	lda	THTICK_ACTION_ANGLE
	ldx	THTICK_ACTION_SPREAD_X
	beq	@set_x

	jsr	rng_val

	clc
	lda	MATH_TEMP
	adc	THTICK_ACTION_ANGLE
@set_x:
	sta	ARG_HITSCAN_ANGLE

	; Y (pitch)
	lda	THTICK_ACTION_HALF_PITCH
	ldx	THTICK_ACTION_SPREAD_Y
	beq	@set_y

	jsr	rng_val

	clc
	lda	MATH_TEMP
	adc	THTICK_ACTION_HALF_PITCH
@set_y:
	sta	ARG_HITSCAN_HALF_PITCH

	rts

;;;
; get spread from ARG1
;;;
; X - the state
; destroys X

get_spread_arg:
	lda	THSTATE_STRUCT_ARG1,x
	tax

	; spread X
	and	#$0F
	sta	THTICK_ACTION_SPREAD_X

	; spread Y
	lda	TAB_SWAP,x
	and	#$0F
	sta	THTICK_ACTION_SPREAD_Y

	rts

;;;
; hitscan action base
;;;

prepare_hitscan:
	ldx	THTICK_ACTION_ST_IDX

	; type
	lda	THSTATE_STRUCT_ARG0,x
	ora	#>THTYPE_STRUCT_SPAWN_A
	sta	@opcode_spawn+2

	; count
	lda	THSTATE_STRUCT_ARG2,x
	sta	THTICK_ACTION_COUNT

	; spread
	jsr	get_spread_arg

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->ticker.type
	lda	TICKER_STRUCT_TYPE,y
	sta	@opcode_spawn+1

	; check_weapon_th
	tya
	bne	:+
	ldy	G_PLAYER_THING
:
	sty	THTICK_ACTION_ORIGIN

	; th->ticker.type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; type = thing_type[th->ticker.type].spawn[st->arg[0]]
@opcode_spawn:
	lda	THTYPE_STRUCT_SPAWN_A	; modified address
	sta	THTICK_ACTION_TEMP

	; attack Z
	lda	THTYPE_STRUCT_ATK_HEIGHT,x
	sta	ARG_HITSCAN_ATK_Z

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; angles
	jsr	prepare_spread

	rts

;;;
; death action base
;;;

normal_death:
	ldx	THTICK_ACTION_ST_IDX

	; arguments
	lda	THSTATE_STRUCT_ARG0,x
	pha
	lda	THSTATE_STRUCT_ARG1,x
	pha
	lda	THSTATE_STRUCT_ARG2,x
	pha

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; arguments
	pla
	sta	THING_STRUCT_BLOCKEDBY,y
	pla
	sta	THING_STRUCT_BLOCKING,y
	pla
	sta	THING_STRUCT_GRAVITY,y

	rts

;;;
; explosion action
;;;

explosion_action:
	ldx	THTICK_ACTION_ST_IDX

	; thing states bank
	lda	THTICK_ACTION_ST_BNK
	sta	REG_RAM_BANK

	; arguments
	lda	THSTATE_STRUCT_ARG0,x
	sta	ARG_EXPLODE_RADIUS
	lda	THSTATE_STRUCT_ARG1,x
	sta	ARG_EXPLODE_DAMAGE
	lda	THSTATE_STRUCT_ARG2,x
	sta	ARG_EXPLODE_BLOCKEDBY

	; boom
	jsr	thing_explode

	; done
	jmp	thing_actdone

;;;
; weapon: ready
;;;
; can be run only from local weapon

act_wpn_ready:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; not attacking
	stz	THING_STRUCT_IFLAGS

	; cmd = ticcmd.bits_h & 0x1F
	lda	TICK_CMD_BITS_H
	and	#$1F

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
	cmp	TICKER_STRUCT_TYPE
	beq	@no_swap

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; check animation	; TO BE REPLACED (inventory check)
	lda	THTYPE_ANIM_RAISE_L,x
	ora	THTYPE_ANIM_RAISE_H,x
	beq	@no_swap

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; current type
	ldy	TICKER_STRUCT_TYPE

	; th->ticker.type = cmd
	stx	TICKER_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; animation
	ldx	THTYPE_ANIM_LOWER_L,y
	lda	THTYPE_ANIM_LOWER_H,y
	jmp	thing_actnext

@no_swap:
	; check primary fire
	lda	TICK_CMD_BITS_L
	and	#TCMD_ATK
	bne	@atk_pri

	; check secondary fire
	lda	TICK_CMD_BITS_L
	and	#TCMD_ALT
	bne	@atk_alt

@done:
	; done
	jmp	thing_actdone

@atk_pri:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; type
	ldy	TICKER_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; animation
	lda	THTYPE_ANIM_ATK_L,y
	ora	THTYPE_ANIM_ATK_H,y
	beq	@done
	ldx	THTYPE_ANIM_ATK_L,y
	lda	THTYPE_ANIM_ATK_H,y
	jmp	thing_actnext

@atk_alt:
	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; animation
	lda	THTYPE_ANIM_ALT_L,y
	ora	THTYPE_ANIM_ALT_H,y
	beq	@done
	ldx	THTYPE_ANIM_ALT_L,y
	lda	THTYPE_ANIM_ALT_H,y
	jmp	thing_actnext

;;;
; weapon: raise
;;;
; can be run only from local weapon

act_wpn_raise:
	ldx	THTICK_ACTION_ST_IDX

	; argument
	lda	THSTATE_STRUCT_ARG0,x
	sta	MATH_TEMP_A

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; raise
	sec
	lda	THING_STRUCT_HEIGHT
	sbc	MATH_TEMP_A
	bcs	:+
	lda	#0
:
	sta	THING_STRUCT_HEIGHT

	; check
	beq	@change

	; done
	jmp	thing_actdone

@change:
	ldy	TICKER_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; animation
	ldx	THTYPE_ANIM_READY_L,y
	lda	THTYPE_ANIM_READY_H,y
	jmp	thing_actnext

;;;
; weapon: lower
;;;
; can be run only from local weapon

act_wpn_lower:
	ldx	THTICK_ACTION_ST_IDX

	; argument
	lda	THSTATE_STRUCT_ARG0,x
	sta	MATH_TEMP_A

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; lower
	clc
	lda	THING_STRUCT_HEIGHT
	adc	MATH_TEMP_A
	sta	THING_STRUCT_HEIGHT
	cmp	#52
	bcs	@do_change

	; done
	jmp	thing_actdone

@do_change:
	; th->height = 52
	lda	#52
	sta	THING_STRUCT_HEIGHT

	; new type
	ldy	TICKER_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; animation
	ldx	THTYPE_ANIM_RAISE_L,y
	lda	THTYPE_ANIM_RAISE_H,y
	jmp	thing_actnext

;;;
; attack: projectile
;;;

act_atk_projectile:
	; type
	ldx	THTICK_ACTION_ST_IDX
	lda	THSTATE_STRUCT_ARG0,x
	ora	#>THTYPE_STRUCT_SPAWN_A
	sta	@opcode_spawn+2

	; spread
	jsr	get_spread_arg

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->ticker.type
	ldx	TICKER_STRUCT_TYPE,y

	; check_weapon_th
	tya
	bne	:+
	ldy	G_PLAYER_THING
:
	sty	THTICK_ACTION_ORIGIN

	; th->ticker.type
	lda	TICKER_STRUCT_TYPE,y
	sta	@opcode_orgt+1

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; type = thing_type[th->ticker.type].spawn[st->arg[0]]
@opcode_spawn:
	lda	THTYPE_STRUCT_SPAWN_A,x	; modified address
	sta	ARG_THING_TYPE
	tax

	; speed
	lda	THTYPE_STRUCT_SPEED,x
	sta	PROJECTILE_SPEED

	; diff = thing_type[th->ticker.type].atk_height - thing_type[type].height / 2
	lda	THTYPE_STRUCT_HEIGHT,x
	lsr
	sta	MATH_VAR_L
	sec
@opcode_orgt:
	lda	THTYPE_STRUCT_ATK_HEIGHT	; modified address
	sbc	MATH_VAR_L
	sta	MATH_VAR_L

	; XY
	jsr	action_xy

	; Z
	stz	ARG_THING_Z_S
	clc
	lda	THING_STRUCT_Z_L,y
	adc	MATH_VAR_L
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_Z_H,y
	adc	#0
	sta	ARG_THING_Z_H

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; sector
	lda	$B000,y
	sta	ARG_THING_SECTOR

	; spawn
	jsr	thing_spawn_org
	beq	@no_spawn

	; restore
	ldy	THTICK_ACTION_ORIGIN

	; angles
	jsr	prepare_spread

	; spread
	jsr	apply_spread

	; restore
	ldx	RET_THING_IDX

	; angle
	lda	ARG_HITSCAN_ANGLE
	sta	THING_STRUCT_ANGLE,x

	; pitch
	lda	ARG_HITSCAN_HALF_PITCH
	asl
	sta	THING_STRUCT_PITCH,x

	; go
	lda	PROJECTILE_SPEED
	sta	MATH_TEMP_A
	ldy	RET_THING_IDX
	jsr	thing_launch

@no_spawn:
	; done
	jmp	thing_actdone

;;;
; action: hitscan attack
;;;

act_atk_hitscan:
	jsr	prepare_hitscan

	; range
	stz	ARG_HITSCAN_ATK_RANGE

	; the loop
@loop:
	; spread
	jsr	apply_spread

	; attack
	lda	THTICK_ACTION_TEMP
	sta	ARG_THING_TYPE
	ldy	THTICK_ACTION_ORIGIN
	jsr	hitscan_attack

	; next
	dec	THTICK_ACTION_COUNT
	bne	@loop

	; done
	jmp	thing_actdone

;;;
; action: hitscan melee
;;;

act_atk_melee:
	jsr	prepare_hitscan

	; spread
	jsr	apply_spread

	; range
	lda	THTICK_ACTION_COUNT
	sta	ARG_HITSCAN_ATK_RANGE

	; attack
	lda	THTICK_ACTION_TEMP
	sta	ARG_THING_TYPE
	ldy	THTICK_ACTION_ORIGIN
	jsr	hitscan_attack

	; done
	jmp	thing_actdone

;;;
; effect: blood splat
;;;

act_eff_blood:
	ldx	THTICK_ACTION_ST_IDX

	; arguments
	lda	THSTATE_STRUCT_ARG0,x
	sta	MATH_TEMP_A
	lda	THSTATE_STRUCT_ARG1,x
	sta	BLOODFX_BOOST
	lda	THSTATE_STRUCT_ARG2,x
	sta	BLOODFX_CHANCE

	; tmp = rng_get()
	jsr	rng_get

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->mz = 0
	lda	#0
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y

	; if(st->arg[0])
	ldx	MATH_TEMP_A
	beq	@no_power

	; tmp & 0x80
	lda	MATH_TEMP
	and	#$80
	sta	MATH_TEMP_B

	; th->angle += 0x40
	clc
	lda	THING_STRUCT_ANGLE,y
	adc	#$40

	; th->angle ^= tmp & 0x80
	eor	MATH_TEMP_B
	sta	THING_STRUCT_ANGLE,y

	; thing_launch_ang(tdx, th->angle, st->arg[0])
	jsr	thing_launch_ang

	; th->mz = st->arg[0] << 8
	lda	MATH_TEMP_A
	sta	THING_STRUCT_MZ_L,y

@no_power:
	; tmp & 0x7F
	lda	MATH_TEMP
	and	#$7F

	; if(st->arg[2] > (tmp & 0x7F))
	cmp	BLOODFX_CHANCE
	bcs	@no_boost

	; th->mz = st->arg[1] << 8
	lda	BLOODFX_BOOST
	sta	THING_STRUCT_MZ_L,y

@no_boost:
	; done
	jmp	thing_actdone


;;;
; ticks: add
;;;

act_tick_add:
	;; th->ticks += rng_val(st->arg[0])

	ldy	THTICK_ACTION_ST_IDX
	ldx	THSTATE_STRUCT_ARG0,y
	jsr	rng_val
	lda	MATH_TEMP
	sta	TMP_ACTION_TICKADD

	; done
	jmp	thing_actdone

;;;
; death: simple
;;;

act_die_simple:
	jsr	normal_death
	jmp	thing_actdone

;;;
; death: radius
;;;

act_die_radius:
	jsr	normal_death

	; th->eflags |= THING_EFLAG_NORADIUS
	lda	THING_STRUCT_EFLAGS,y
	ora	#THING_EFLAG_NORADIUS
	sta	THING_STRUCT_EFLAGS,y

	; th->iflags &= ~THING_IFLAG_HEIGHTCHECK
	lda	THING_STRUCT_IFLAGS,y
	and	#<~THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,y

	; th->ticker.type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; <alt_radius>
	lda	THTYPE_STRUCT_ALT_RADIUS,x
	pha

	; XY
	jsr	action_xy

	; Z
	lda	THING_STRUCT_Z_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_Z_H,y
	sta	ARG_THING_Z_H

	; th->radius = thing_type[th->ticker.type].alt_radius
	pla	; <alt_radius>
	sta	THING_STRUCT_RADIUS,y

	; check new position
	stz	ARG_THING_SECTOR
	jsr	thing_check_pos

	; check result
	lda	THMOVE_SECTOR
	beq	:+

	; apply
	jsr	thing_apply_pos
:
	; done
	jmp	thing_actdone

;;;
; explosion
;;;

act_explode_origin:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; origin
	lda	THING_STRUCT_TH_ORIGIN,y
	sta	ARG_EXPLODE_SOURCE

	; action
	jmp	explosion_action

act_explode_target:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; target
	lda	THING_STRUCT_TH_TARGET,y
	sta	ARG_EXPLODE_SOURCE

	; action
	jmp	explosion_action

act_explode_damager:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; target
	lda	THING_STRUCT_TH_DAMAGER,y
	sta	ARG_EXPLODE_SOURCE

	; action
	jmp	explosion_action

act_explode_self:
	sty	ARG_EXPLODE_SOURCE
	jmp	explosion_action

;;;
; state action table
;;;

action_table:
	.word	act_wpn_ready
	.word	act_wpn_raise
	.word	act_wpn_lower
	.word	act_atk_projectile
	.word	act_atk_hitscan
	.word	act_atk_melee
	.word	act_eff_blood
	.word	act_tick_add
	.word	act_die_simple
	.word	act_die_radius
	.word	act_explode_origin
	.word	act_explode_target
	.word	act_explode_damager
	.word	act_explode_self

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

