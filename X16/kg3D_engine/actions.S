.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "tables.inc"
.include "math.inc"
.include "zeropage.inc"
.include "engine.inc"
.include "things.inc"
.include "hitscan.inc"

.export action_run

;;;
; CODE
;;;

.segment "CODE"

;;;
; get XY
;;;

handle_XY:
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
	ldy	TICK_IDX
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

	; view height
	lda	THTYPE_STRUCT_VIEW_HEIGHT,x

	ply
	clc
	rts

;;;
; action: weapon ready
;;;
; can be run only from local weapon

act_wpn_ready:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

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

	; change animation
	lda	TMP_STACT_FRM_L
	sta	THING_STRUCT_NXTFRM_L
	lda	TMP_STACT_FRM_H
	sta	THING_STRUCT_NXTFRM_H

	; attacking
	lda	#1
	sta	THING_STRUCT_IFLAGS

	; apply
	sec
	rts

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

	; change animation
	lda	TMP_STACT_FRM_L
	sta	THING_STRUCT_NXTFRM_L
	lda	TMP_STACT_FRM_H
	sta	THING_STRUCT_NXTFRM_H

	; attacking
	lda	#2
	sta	THING_STRUCT_IFLAGS

	; apply
	sec
	rts

@no_alt:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

@skip_alt:
	; not attacking
	stz	THING_STRUCT_IFLAGS

	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

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
	lda	THING_STRUCT_COUNTER
	sbc	#8
	bcs	:+
	lda	#0
:
	sta	THING_STRUCT_COUNTER

	; check
	bne	:+

	; get thing type
	ldx	THING_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; check animation
	lda	THTYPE_ANIM_READY_L,x
	sta	TMP_STACT_FRM_L
	lda	THTYPE_ANIM_READY_H,x
	sta	TMP_STACT_FRM_H

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; change animation
	lda	TMP_STACT_FRM_L
	sta	THING_STRUCT_NXTFRM_L
	lda	TMP_STACT_FRM_H
	sta	THING_STRUCT_NXTFRM_H

	; apply
	sec
	rts
:
	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; no changes
	clc
	rts

;;;
; action: weapon lower
;;;
; can be run only from local weapon

act_wpn_lower:
	; TODO
	clc
	rts

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
	sbc	MATH_TEMP_A
	sta	MATH_TEMP_A

	; projectile speed
	lda	THTYPE_STRUCT_SPEED,x
	pha

	; X Y
	jsr	handle_XY

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

	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; no changes
	clc
	rts

;;;
; action: projectile attack
;;;

cb_hitscan:
	; TEST CODE
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	eor	#$22
	sta	(WALL_PTR_L),y

	clc
	rts

act_atk_hitscan:
	; origin
	jsr	handle_base_info
	pha

	; X Y
	jsr	handle_XY

	; Z + view height
	pla
	adc	THING_STRUCT_Z_L,y
	sta	ARG_THING_SPAWN_Z_L
	lda	THING_STRUCT_Z_H,y
	adc	#0
	sta	ARG_THING_SPAWN_Z_H

	; angle
	lda	THING_STRUCT_ANGLE,y
	sta	ARG_HITSCAN_ANGLE

	; callback
	ldx	#<cb_hitscan
	ldy	#>cb_hitscan

	; attack
	jsr	hitscan

	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; no changes
	clc
	rts

;;;
; state action calling
;;;
; THANIM_PTR_* points to current frame
; BANK_THING_STATES is active bank
; use 'clc + rts' to continue; active bank must be 'BANK_THING_STATES'
; use 'sec + rts' for frame change; active bank must be 'BANK_THING_STRUCT'

action_run:
	jmp	(action_table-2,x)

action_table:
	.word	act_wpn_ready
	.word	act_wpn_raise
	.word	act_wpn_lower
	.word	act_atk_projectile
	.word	act_atk_hitscan

