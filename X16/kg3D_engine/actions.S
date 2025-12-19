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

.export action_table

;;;
; NOTES: action callback
;;;
; X Y and BANK can be trashed
; Y - the thing; 0 = player weapon; (also stored in THTICK_STATE_TDX)
; THTICK_STATE_IDX : THTICK_STATE_BANK - current state (also current bank)
; jump to 'thing_actdone' to continue
; jump to 'thing_actnext'; TODO: more info on X and A

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
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
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
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; lower
	clc
	lda	THING_STRUCT_HEIGHT
	adc	#8
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
	; st->arg[0]
	ldx	THTICK_STATE_IDX
	lda	THSTATE_STRUCT_ARG0,x
	ora	#>THTYPE_STRUCT_SPAWN_A
	sta	@opcode_spawn+2

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
	sty	THTICK_ACT_TDX

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
	sta	THTICK_ACT_SPEED

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
	beq	:+

	; restore
	ldy	THTICK_ACT_TDX

	; angle
	lda	THING_STRUCT_ANGLE,y
	sta	THING_STRUCT_ANGLE,x

	; pitch
	lda	THING_STRUCT_PITCH,y
	sta	THING_STRUCT_PITCH,x

	; go
	ldx	THTICK_ACT_SPEED
	ldy	RET_THING_IDX
	jsr	thing_launch
:
	; done
	jmp	thing_actdone

;;;
; TODO
;;;

act_atk_hitscan:
act_eff_blood:
act_tick_add:
act_die_simple:
;	stx	DBG_BYTE0

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
	.word	act_eff_blood
	.word	act_tick_add
	.word	act_die_simple

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

