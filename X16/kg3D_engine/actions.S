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
; Y - the thing; 0 = player weapon; (also stored in G_TICKER_IDX)
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
	lda	THING_STRUCT_X_L,y
	sta	ARG_THING_X_L
	lda	THING_STRUCT_X_H,y
	sta	ARG_THING_X_H

	; Y
	lda	THING_STRUCT_Y_L,y
	sta	ARG_THING_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	ARG_THING_Y_H

	rts

;;;
; skip current state
;;;

do_skip_state:
	ldy	THTICK_ACTION_ST_IDX

	; thing states bank
	lda	THTICK_ACTION_ST_BNK
	sta	REG_RAM_BANK

	; next
	ldx	THSTATE_STRUCT_FRM_NXT,y
	lda	TAB_BANK,x
	ldx	THSTATE_STRUCT_NEXT,y

	; thing info bank
	ldy	#BANK_THING_INFO
	sty	REG_RAM_BANK

	jmp	thing_actnext

;;;
; get distance between two things
;;;
; X - thing
; Y - thing (trashed)

get_dist:
	; if(dist_calc >= 0)
	lda	MATH_VAR_H
	bpl	@skip

	; backup
	lda	REG_RAM_BANK
	pha
	phx

	; p2a_coord.x = ht->x / 256 - th->x / 256
	sec
	lda	THING_STRUCT_X_L,y
	sbc	THING_STRUCT_X_L,x
	sta	MATH_P2A_X_L
	lda	THING_STRUCT_X_H,y
	sbc	THING_STRUCT_X_H,x
	sta	MATH_P2A_X_H

	; p2a_coord.y = ht->y / 256 - th->y / 256
	sec
	lda	THING_STRUCT_Y_L,y
	sbc	THING_STRUCT_Y_L,x
	sta	MATH_P2A_Y_L
	lda	THING_STRUCT_Y_H,y
	sbc	THING_STRUCT_Y_H,x
	sta	MATH_P2A_Y_H

	; dist_calc = point_to_dist()
	jsr	math_p2d

	; restore
	plx
	pla
	sta	REG_RAM_BANK

@skip:
	rts

;;;
; get angle between two things
;;;
; X - thing
; Y - thing (trashed)
; returns 8bit angle in 'MATH_VAR_U'

get_angle:
	; backup
	lda	REG_RAM_BANK
	pha
	phx

	; p2a_coord.x = ht->x / 256 - th->x / 256
	sec
	lda	THING_STRUCT_X_L,y
	sbc	THING_STRUCT_X_L,x
	sta	MATH_P2A_X_L
	lda	THING_STRUCT_X_H,y
	sbc	THING_STRUCT_X_H,x
	sta	MATH_P2A_X_H

	; p2a_coord.y = ht->y / 256 - th->y / 256
	sec
	lda	THING_STRUCT_Y_L,y
	sbc	THING_STRUCT_Y_L,x
	sta	MATH_P2A_Y_L
	lda	THING_STRUCT_Y_H,y
	sbc	THING_STRUCT_Y_H,x
	sta	MATH_P2A_Y_H

	jsr	math_p2a
	jsr	math_ang8

	; restore
	plx
	pla
	sta	REG_RAM_BANK

	rts

;;;
; visibility table check
;;;
; Y - thing
; X - thing (trashed)

vis_check:
	lda	REG_RAM_BANK
	pha

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; Y thing sector
	lda	$B000,y
	sta	TMP_PTR_L

	; X thing sector
	lda	$B000,x
	tax

	; addr HI
	and	#$1F
	ora	#$A0
	sta	TMP_PTR_H

	; (1 << (s1 >> 5)
	lda	TAB_BANK,x
	tax
	lda	TAB_POW2,x

	; vistab bank
	ldx	#BANK_VISTAB
	stx	REG_RAM_BANK

	; restore
	plx

	; check
	and	(TMP_PTR_L)

	; done
	stx	REG_RAM_BANK
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
; prepare attack from chase
;;;

prepare_atk:
	; th->dist = dist_calc
	lda	HITSCAN_SIGHT_DIST_L
	sta	THING_STRUCT_DIST_L,x
	lda	HITSCAN_SIGHT_DIST_H
	sta	THING_STRUCT_DIST_H,x

	; th->counter = 10
	lda	#10
	sta	THING_STRUCT_COUNTER,x

	; type
	ldy	TICKER_STRUCT_TYPE,x

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	rts

;;;
; flip angle with dist check
;;;

dist_flip_ang:
	; if(dist_calc - st->arg[0] * 4 < 0)
	sec
	lda	MATH_VAR_L
	sbc	TMP_ACTION_ARG0
	lda	MATH_VAR_H
	sbc	TMP_ACTION_ARG1
	bpl	:+

	; p2a_coord.a ^= 0x80
	lda	MATH_VAR_U
	eor	#$80
	sta	MATH_VAR_U
:
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
	sta	THING_STRUCT_VIEW_HEIGHT,y
	pla
	sta	THING_STRUCT_EFLAGS,y
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
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; type
	ldy	TICKER_STRUCT_TYPE

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
@no_change:
	jmp	thing_actdone

@do_change:
	; th->height = 52
	lda	#52
	sta	THING_STRUCT_HEIGHT

	; check death
	lda	THING_STRUCT_COUNTER
	bne	@no_change

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
	lda	THING_STRUCT_X_S,y
	sta	ARG_THING_X_S
	lda	THING_STRUCT_Y_S,y
	sta	ARG_THING_Y_S

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
; death: base
;;;

act_die_base:
	jsr	normal_death
	jmp	thing_actdone

;;;
; death: base (alt)
;;;

act_die_alt:
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
; death: heights
;;;

act_die_height:
	ldx	THTICK_ACTION_ST_IDX

	; arguments
	lda	THSTATE_STRUCT_ARG2,x
	pha
	lda	THSTATE_STRUCT_ARG1,x
	pha
	lda	THSTATE_STRUCT_ARG0,x
	pha

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->height = st->arg[0]
	pla
	beq	:+
	sta	THING_STRUCT_HEIGHT,y
:
	; th->step_height = st->arg[1]
	pla
	beq	:+
	sta	THING_STRUCT_STEP_HEIGHT,y
:
	; th->water_height = st->arg[2]
	pla
	beq	:+
	sta	THING_STRUCT_WATER_HEIGHT,y

	; th->eflags |= THING_EFLAG_WATERSPEC
	lda	THING_STRUCT_EFLAGS,y
	ora	#THING_EFLAG_WATERSPEC
	sta	THING_STRUCT_EFLAGS,y
:
	jmp	thing_actdone

;;;
; death: blocking
;;;

act_die_block:
	ldx	THTICK_ACTION_ST_IDX

	; arguments
	lda	THSTATE_STRUCT_ARG1,x
	pha
	lda	THSTATE_STRUCT_ARG0,x
	pha

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->blocking = st->arg[0]
	pla
	sta	THING_STRUCT_BLOCKING,y

	; th->blockedby = st->arg[1]
	pla
	sta	THING_STRUCT_BLOCKEDBY,y

	jmp	thing_actdone

;;;
; death: blocking
;;;

act_die_type:
	; type
	ldx	THTICK_ACTION_ST_IDX
	lda	THSTATE_STRUCT_ARG0,x
	ora	#>THTYPE_STRUCT_SPAWN_A
	sta	@opcode_spawn+2

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; type = thing_type[th->ticker.type].spawn[st->arg[0]]
@opcode_spawn:
	ldx	THTYPE_STRUCT_SPAWN_A	; modified address

	; <blocking>
	lda	THTYPE_STRUCT_BLOCKING,x
	pha

	; <blocked by>
	lda	THTYPE_STRUCT_BLOCKEDBY,x
	pha

	; <health>
	lda	THTYPE_STRUCT_HEALTH_L,x
	pha
	lda	THTYPE_STRUCT_HEALTH_H,x
	pha

	; <height>
	lda	THTYPE_STRUCT_HEIGHT,x
	pha

	; <water height>
	lda	THTYPE_STRUCT_WATER_HEIGHT,x
	pha

	; <step height>
	lda	THTYPE_STRUCT_STEP_HEIGHT,x
	pha

	; <eflags>
	lda	THTYPE_STRUCT_EFLAGS,x
	pha

	; <gravity>
	lda	THTYPE_STRUCT_GRAVITY,x
	pha

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; <gravity>
	pla
	sta	THING_STRUCT_GRAVITY,y

	; <eflags>
	pla
	sta	THING_STRUCT_EFLAGS,y

	; <step height>
	pla
	sta	THING_STRUCT_STEP_HEIGHT,y

	; <water height>
	pla
	sta	THING_STRUCT_WATER_HEIGHT,y

	; <height>
	pla
	sta	THING_STRUCT_HEIGHT,y

	; <health>
	pla
	sta	THING_STRUCT_HEALTH_H,y
	pla
	sta	THING_STRUCT_HEALTH_L,y

	; <blocked by>
	pla
	sta	THING_STRUCT_BLOCKEDBY,y

	; <blocking>
	pla
	sta	THING_STRUCT_BLOCKING,y

	; type
	txa
	sta	TICKER_STRUCT_TYPE,y

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
; set: property
;;;

act_set_prop:
	ldx	THTICK_ACTION_ST_IDX

	; arguments
	lda	THSTATE_STRUCT_ARG1,x
	pha
	lda	THSTATE_STRUCT_ARG0,x
	tax

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; get offset
	lda	@prop_offs,x
	sec
	ror
	sta	TMP_PTR_H
	lda	#0
	ror
	sta	TMP_PTR_L

	; save
	pla
	sta	(TMP_PTR_L),y

	jmp	thing_actdone

@prop_offs:
	.byte	(THING_STRUCT_HEIGHT >> 7) & $FF
	.byte	(THING_STRUCT_RADIUS >> 7) & $FF
	.byte	(THING_STRUCT_VIEW_HEIGHT >> 7) & $FF
	.byte	(THING_STRUCT_STEP_HEIGHT >> 7) & $FF
	.byte	(THING_STRUCT_WATER_HEIGHT >> 7) & $FF
	.byte	(THING_STRUCT_GRAVITY >> 7) & $FF
	.byte	(THING_STRUCT_ANGLE >> 7) & $FF
	.byte	(THING_STRUCT_PITCH >> 7) & $FF
	.byte	(THING_STRUCT_SCALE >> 7) & $FF
	.byte	(THING_STRUCT_BLOCKING >> 7) & $FF
	.byte	(THING_STRUCT_BLOCKEDBY >> 7) & $FF

;;;
; set: flags
;;;

act_set_flags:
	ldx	THTICK_ACTION_ST_IDX

	; arguments
	lda	THSTATE_STRUCT_ARG0,x
	sta	MATH_TEMP_A
	lda	THSTATE_STRUCT_ARG1,x
	sta	MATH_TEMP_B

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; get
	lda	THING_STRUCT_EFLAGS,y

	; th->eflags &= st->arg[0]
	and	MATH_TEMP_A

	; th->eflags |= st->arg[1]
	ora	MATH_TEMP_B

	; set
	sta	THING_STRUCT_EFLAGS,y

	jmp	thing_actdone

;;;
; aim: angle
;;;

act_aim_attack: ; TODO TODO TODO
act_aim_angle:
	ldx	THTICK_ACTION_ST_IDX

	; arg
	lda	THSTATE_STRUCT_ARG0,x
	pha

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; if(th->target)
	ldx	G_TICKER_IDX
	ldy	THING_STRUCT_TH_TARGET,x
	beq	:+

	; th->angle = get_angle(tdx, th->target)
	jsr	get_angle
	lda	MATH_VAR_U
	sta	THING_STRUCT_ANGLE,x

	; th->pitch = 0x80
	lda	#$80
	sta	THING_STRUCT_PITCH,x

	; if(st->arg[0])
	pla
	beq	:+
	jmp	do_skip_state
:
	jmp	thing_actdone

;;;
; enemy: look
;;;

act_enemy_look:
	ldx	THTICK_ACTION_ST_IDX

	; arg
	lda	THSTATE_STRUCT_ARG0,x
	sta	TMP_ACTION_ARG0

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; if(th->damager)
	lda	THING_STRUCT_TH_DAMAGER,y
	beq	:+

	; th->target = th->damager
	sta	THING_STRUCT_TH_TARGET,y

	; th->damager = 0
	lda	#0
	sta	THING_STRUCT_TH_DAMAGER,y

	ldx	G_TICKER_IDX
	bra	@activate
:
	; if(ht->iflags & THING_IFLAG_CORPSE)
	ldx	G_PLAYER_THING
	lda	THING_STRUCT_IFLAGS,x
	and	#THING_IFLAG_CORPSE
	bne	@nosee

	; if(!vis_check(thingsec[tdx][0], thingsec[player_thing][0]))
	jsr	vis_check
	bne	@do_check

@nosee:
	; if(st->arg[0])
	lda	TMP_ACTION_ARG0
	beq	:+

	; th->ticks += rng_val(st->arg[0])
	tax
	jsr	rng_val
	lda	MATH_TEMP
	sta	TMP_ACTION_TICKADD
:
	jmp	thing_actdone

@do_check:
	; the thing
	ldx	G_TICKER_IDX

	; if(th->maparg)
	lda	THING_STRUCT_MAPARG,x
	beq	@skip_sight

	; dist_calc = -1
	lda	#$FF
	sta	MATH_VAR_H

	; get_dist(tdx, player_thing)
	ldy	G_PLAYER_THING
	jsr	get_dist

	; th->chaseang = p2a_coord.a & 0xF0
	lda	MATH_VAR_U
	and	#$F0
	sta	THING_STRUCT_CHASEANG,x

	; if(dist_calc > 192)
	lda	MATH_VAR_H
	bne	:+
	lda	MATH_VAR_L
	cmp	#193
	bcc	@skip_sight
:
	; th->maparg > 1
	lda	THING_STRUCT_MAPARG,x
	dea
	beq	:+

	; (p2a_coord.a + 0x40 - th->angle) & 0x80
	clc
	lda	MATH_VAR_U
	sta	ARG_HITSCAN_ANGLE
	adc	#$40
	sec
	sbc	THING_STRUCT_ANGLE,x
	bmi	@nosee
:
	; hitscan_sight_ex(tdx, player_thing, p2a_coord.a, dist_calc)
	ldy	G_TICKER_IDX
	ldx	G_PLAYER_THING
	lda	MATH_VAR_L
	sta	HITSCAN_SIGHT_DIST_L
	lda	MATH_VAR_H
	sta	HITSCAN_SIGHT_DIST_H
	jsr	hitscan_sight_xy

	; if(hitscan.pbot >= hitscan.ptop)
	lda	HITSCAN_SIGHT_PBOT
	cmp	HITSCAN_SIGHT_PTOP
	bcs	@nosee

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; restore
	ldx	G_TICKER_IDX

@skip_sight:
	; th->target = player_thing
	lda	G_PLAYER_THING
	sta	THING_STRUCT_TH_TARGET,x

@activate:
	; th->counter = 15
	lda	#15
	sta	THING_STRUCT_COUNTER,x

	; type
	ldy	TICKER_STRUCT_TYPE,x

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; th->next_state = thing_anim[th->ticker.type][ANIM_MOVE].state
	ldx	THTYPE_ANIM_MOVE_L,y
	lda	THTYPE_ANIM_MOVE_H,y
	jmp	thing_actnext

;;;
; enemy: chase
;;;

act_enemy_chase:
	ldx	THTICK_ACTION_ST_IDX

	; args
	lda	THSTATE_STRUCT_ARG0,x
	sta	TMP_ACTION_ARG0
	lda	THSTATE_STRUCT_ARG1,x
	sta	TMP_ACTION_ARG1
	lda	THSTATE_STRUCT_ARG2,x
	sta	TMP_ACTION_ARG2

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; if(th->target)
	ldx	THING_STRUCT_TH_TARGET,y
	bne	:+

@stop:
	; th->damager = 0
	lda	#0
	sta	THING_STRUCT_TH_DAMAGER,y

	; type
	ldx	G_TICKER_IDX
	ldy	TICKER_STRUCT_TYPE,x

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; th->next_state = thing_anim[th->ticker.type][ANIM_SPAWN].state
	ldx	THTYPE_ANIM_SPAWN_L,y
	lda	THTYPE_ANIM_SPAWN_H,y
	jmp	thing_actnext
:
	; if(ht->iflags & THING_IFLAG_CORPSE)
	lda	THING_STRUCT_IFLAGS,x
	and	#THING_IFLAG_CORPSE
	bne	@stop

	; if(!vis_check(thingsec[tdx][0], thingsec[th->target][0]))
	jsr	vis_check
	bne	:+

@done:
	jmp	thing_actdone
:
	; dist_calc = -1
	lda	#$FF
	sta	MATH_VAR_H

	; prepare
	ldx	G_TICKER_IDX
	ldy	THING_STRUCT_TH_TARGET,x

	; st->arg[2]
	lda	TMP_ACTION_ARG2
	beq	@no_ranged

	; (rng_get() & 0x7F) < st->arg[2]
	jsr	rng_get
	lda	MATH_TEMP
	and	#$7F
	cmp	TMP_ACTION_ARG2
	bcs	@no_ranged

	; get_dist(tdx, th->target)
	jsr	get_dist

	; hitscan_sight_ex(tdx, th->target, p2a_coord.a, dist_calc)
	ldy	G_TICKER_IDX
	ldx	THING_STRUCT_TH_TARGET,y
	lda	MATH_VAR_U
	sta	ARG_HITSCAN_ANGLE
	lda	MATH_VAR_L
	sta	HITSCAN_SIGHT_DIST_L
	lda	MATH_VAR_H
	sta	HITSCAN_SIGHT_DIST_H
	jsr	hitscan_sight_xy

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; restore
	ldx	G_TICKER_IDX

	; if(hitscan.pbot >= hitscan.ptop)
	lda	HITSCAN_SIGHT_PBOT
	cmp	HITSCAN_SIGHT_PTOP
	bcs	@no_atk

	; stuff
	jsr	prepare_atk

	; th->next_state = thing_anim[th->ticker.type][ANIM_FIRE].state
	ldx	THTYPE_ANIM_FIRE_L,y
	lda	THTYPE_ANIM_FIRE_H,y
	jmp	thing_actnext

@no_ranged:
	; if(st->arg[1])
	lda	TMP_ACTION_ARG1
	beq	@no_atk

	; get_dist(tdx, th->target)
	jsr	get_dist

	; if(st->arg[1] * 4 - dist_calc >= 0)
	lda	#0
	asl	TMP_ACTION_ARG1
	rol
	asl	TMP_ACTION_ARG1
	rol
	pha
	sec
	lda	TMP_ACTION_ARG1
	sbc	MATH_VAR_L
	pla
	sbc	MATH_VAR_H
	bmi	@no_atk

	; hitscan_sight_ex(tdx, th->target, p2a_coord.a, dist_calc)
	ldy	G_TICKER_IDX
	ldx	THING_STRUCT_TH_TARGET,y
	lda	MATH_VAR_U
	sta	ARG_HITSCAN_ANGLE
	lda	MATH_VAR_L
	sta	HITSCAN_SIGHT_DIST_L
	lda	MATH_VAR_H
	sta	HITSCAN_SIGHT_DIST_H
	jsr	hitscan_sight_xy

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; restore
	ldx	G_TICKER_IDX

	; if(hitscan.pbot >= hitscan.ptop)
	lda	HITSCAN_SIGHT_PBOT
	cmp	HITSCAN_SIGHT_PTOP
	bcs	@no_atk

	; stuff
	jsr	prepare_atk

	; th->next_state = thing_anim[th->ticker.type][ANIM_MELEE].state
	ldx	THTYPE_ANIM_MELEE_L,y
	lda	THTYPE_ANIM_MELEE_H,y
	jmp	thing_actnext

@no_atk:
	; st->arg[0] * 4
	lda	#0
	asl	TMP_ACTION_ARG0
	rol
	asl	TMP_ACTION_ARG0
	rol
	sta	TMP_ACTION_ARG1

	; th->angle = th->chaseang
	lda	THING_STRUCT_CHASEANG,x
	sta	THING_STRUCT_ANGLE,x

	; th->counter--
	dec	THING_STRUCT_COUNTER,x

	; if(!th->counter)
	bne	@keep_angle

	; get_dist(tdx, th->target)
	jsr	get_dist

	; flip check
	jsr	dist_flip_ang

	; th->counter = 10 + (rng_get() & 7)
	jsr	rng_get
	lda	MATH_TEMP
	and	#7
	clc
	adc	#10
	sta	THING_STRUCT_COUNTER,x

	; th->chaseang = p2a_coord.a
	lda	MATH_VAR_U

	; th->chaseang -= 0x0F
	clc
	sbc	#$0F
	sta	TMP_ACTION_ARG2

	; th->chaseang += rng_get() & 0x1F
	jsr	rng_get
	lda	MATH_TEMP
	and	#$1F
	clc
	adc	TMP_ACTION_ARG2
	sta	THING_STRUCT_CHASEANG,x

	; th->angle = th->chaseang
	sta	THING_STRUCT_ANGLE,x

@keep_angle:
	; type
	ldy	TICKER_STRUCT_TYPE,x

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; speed
	lda	THTYPE_STRUCT_SPEED,y
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; angle
	ldy	THING_STRUCT_CHASEANG,x

	;; nx = th->x + tab_sin[th->chaseang] * info->speed

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; tab_sin[th->chaseang]
	lda	TAB_SIN_L,y
	sta	VERA_FX_CACHE_H
	lda	TAB_SIN_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result + th->x
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	THING_STRUCT_X_S,x
	sta	ARG_THING_X_S
	lda	VERA_DATA0
	adc	THING_STRUCT_X_L,x
	sta	ARG_THING_X_L
	lda	VERA_DATA0
	adc	THING_STRUCT_X_H,x
	sta	ARG_THING_X_H

	;; ny = th->y + tab_cos[th->chaseang] * info->speed

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; tab_cos[th->chaseang]
	lda	TAB_COS_L,y
	sta	VERA_FX_CACHE_H
	lda	TAB_COS_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result + th->y
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	THING_STRUCT_Y_S,x
	sta	ARG_THING_Y_S
	lda	VERA_DATA0
	adc	THING_STRUCT_Y_L,x
	sta	ARG_THING_Y_L
	lda	VERA_DATA0
	adc	THING_STRUCT_Y_H,x
	sta	ARG_THING_Y_H

	; backup
	lda	MATH_VAR_L
	pha
	lda	MATH_VAR_H
	pha
	lda	MATH_VAR_U
	pha

	; if(thing_check_pos(tdx, nx / 256, ny / 256, th->z / 256, 0))
	stx	RET_THING_IDX
	stz	ARG_THING_SECTOR
	jsr	thing_check_pos

	; restore
	pla
	sta	MATH_VAR_U
	pla
	sta	MATH_VAR_H
	pla
	sta	MATH_VAR_L

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; restore
	ldx	G_TICKER_IDX

	; check result
	lda	THMOVE_SECTOR
	beq	@move_fail

	; th->x = nx
	lda	ARG_THING_X_S
	sta	THING_STRUCT_X_S,x
	lda	ARG_THING_X_L
	sta	THING_STRUCT_X_L,x
	lda	ARG_THING_X_H
	sta	THING_STRUCT_X_H,x

	; th->y = ny
	lda	ARG_THING_Y_S
	sta	THING_STRUCT_Y_S,x
	lda	ARG_THING_Y_L
	sta	THING_STRUCT_Y_L,x
	lda	ARG_THING_Y_H
	sta	THING_STRUCT_Y_H,x

	; thing_apply_pos()
	jsr	thing_apply_pos

	jmp	thing_actdone

@move_fail:
	; if(rng_get() < 80)
	jsr	rng_get
	lda	MATH_TEMP
	cmp	#80
	bcs	:+

	; th->chaseang = poscheck.hitang + 0x40
	clc
	lda	THMOVE_HITANG
	adc	#$40
	sta	THING_STRUCT_CHASEANG,x

	jmp	thing_actdone
:
	; get_dist(tdx, th->target)
	ldy	THING_STRUCT_TH_TARGET,x
	jsr	get_dist

	; flip check
	jsr	dist_flip_ang

	; ang = p2a_coord.a
	; ang = (poscheck.hitang + 0x40) - ang
	clc
	lda	THMOVE_HITANG
	adc	#$40
	sec
	sbc	MATH_VAR_U

	; ang &= 0x80
	and	#$80

	; th->chaseang = poscheck.hitang ^ ang
	eor	THMOVE_HITANG
	sta	THING_STRUCT_CHASEANG,x

	; th->angle = th->chaseang
	sta	THING_STRUCT_ANGLE,x

	jmp	thing_actdone

;;;
; enemy: pain
;;;

act_enemy_pain:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; if(th->target)
	lda	THING_STRUCT_TH_TARGET,y
	bne	:+

	; th->target = th->damager
	lda	THING_STRUCT_TH_DAMAGER,y
	sta	THING_STRUCT_TH_TARGET,y
:
	jmp	thing_actdone

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
	.word	act_die_base
	.word	act_die_alt
	.word	act_die_height
	.word	act_die_block
	.word	act_die_type
	.word	act_explode_origin
	.word	act_explode_target
	.word	act_explode_damager
	.word	act_explode_self
	.word	act_set_prop
	.word	act_set_flags
	.word	act_aim_angle
	.word	act_aim_attack
	.word	act_enemy_look
	.word	act_enemy_chase
	.word	act_enemy_pain

