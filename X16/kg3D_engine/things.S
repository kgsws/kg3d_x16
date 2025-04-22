.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "tables.inc"
.include "main.inc"
.include "math.inc"
.include "map.inc"
.include "zeropage.inc"
.include "engine.inc"
.include "actions.inc"

.export thing_spawn
.export thing_spawn_org
.export thing_clear
.export thing_launch
.export thing_tick
.export thing_check_position
.export thing_apply_position
.export thing_check_heights
.export thing_apply_heights

.export wall_sizes

;;;
; CODE
;;;

.segment "CODE"

;;;
; thing spawn
;;;
; requires all ARG_THING_SPAWN_* arguments
; ends with 'BANK_THING_STRUCT' used
; Y is originator for '_org' variant

thing_spawn:
	ldy	#0
thing_spawn_org:
	phy	; <origin>

	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; check thing count in this sector
	ldx	ARG_THING_SPAWN_SECTOR
	lda	$BF00,x
	cmp	#30
	bcc	:+

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; stop
	pla
	stz	RET_THING_SPAWN
	rts
:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; find free thing slot (type >= 128)
	ldy	#1
@find_loop:
	lda	THING_STRUCT_TYPE,y
	bmi	@found_slot
	iny
	bpl	@find_loop

	; no more free thing slots
	pla
	stz	RET_THING_SPAWN
	rts

@found_slot:
	sty	RET_THING_SPAWN

	; clear structure stuff
	lda	#0
	sta	THING_STRUCT_ANGLE,y
	sta	THING_STRUCT_PITCH,y
	sta	THING_STRUCT_IFLAGS,y
	sta	THING_STRUCT_TH_ORIGIN,y
	sta	THING_STRUCT_TH_TARGET,y
	sta	THING_STRUCT_COUNTER,y
	sta	THING_STRUCT_MX_S,y
	sta	THING_STRUCT_MX_L,y
	sta	THING_STRUCT_MY_S,y
	sta	THING_STRUCT_MY_L,y
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y

	; save type
	lda	ARG_THING_SPAWN_TYPE
	sta	THING_STRUCT_TYPE,y
	tax

	; copy position
	lda	ARG_THING_SPAWN_X_S
	sta	THING_STRUCT_X_S,y
	lda	ARG_THING_SPAWN_X_L
	sta	THING_STRUCT_X_L,y
	lda	ARG_THING_SPAWN_X_H
	sta	THING_STRUCT_X_H,y
	lda	ARG_THING_SPAWN_Y_S
	sta	THING_STRUCT_Y_S,y
	lda	ARG_THING_SPAWN_Y_L
	sta	THING_STRUCT_Y_L,y
	lda	ARG_THING_SPAWN_Y_H
	sta	THING_STRUCT_Y_H,y
	lda	ARG_THING_SPAWN_Z_S
	sta	THING_STRUCT_Z_S,y
	lda	ARG_THING_SPAWN_Z_L
	sta	THING_STRUCT_Z_L,y
	lda	ARG_THING_SPAWN_Z_H
	sta	THING_STRUCT_Z_H,y

	;;;
	;; load stuff from info

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; radius
	lda	THTYPE_STRUCT_RADIUS,x
	pha	; <radius>

	; height
	lda	THTYPE_STRUCT_HEIGHT,x
	pha	; <height>

	; eflags
	lda	THTYPE_STRUCT_EFLAGS,x
	pha	; <eflags>

	; blocking
	lda	THTYPE_STRUCT_BLOCKING,x
	pha	; <blocking>

	; blocked by
	lda	THTYPE_STRUCT_BLOCKEDBY,x
	pha	; <blocked by>

	; gravity
	lda	THTYPE_STRUCT_GRAVITY,x
	pha	; <gravity>

	; scale
	lda	THTYPE_STRUCT_SCALE,x
	pha	; <scale>

	; health
	lda	THTYPE_STRUCT_HEALTH_L,x
	pha	; <health LO>
	lda	THTYPE_STRUCT_HEALTH_H,x
	pha	; <health HI>

	; spawn animation
	lda	THTYPE_ANIM_SPAWN_L,x
	pha	; <next frame LO>
	sta	THTEMP_PTR_H
	and	#$F8
	ora	#THSTATE_STRUCT_SPRITE
	sta	THTEMP_PTR_L

	lda	THTEMP_PTR_H
	and	#$07
	ora	#$A0
	sta	THTEMP_PTR_H

	lda	THTYPE_ANIM_SPAWN_H,x
	pha	; <next frame HI>
	lsr
	lsr
	ora	THTEMP_PTR_H
	sta	THTEMP_PTR_H

	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; spawn sprite
	lda	(THTEMP_PTR_L)
	bpl	:+

	; no sprite
	ldx	#$FF
	bra	:++
:
	; sprite and frame
	tax
	dec	THTEMP_PTR_L
	lda	(THTEMP_PTR_L)
	and	#$1F
	clc
	adc	TAB_SPRITE_REMAP,x
	tax
:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	;;;
	;; save loaded info

	; save sprite
	txa
	sta	THING_STRUCT_SPRITE,y

	; state ticks
	lda	#1
	sta	THING_STRUCT_TICKS,y

	; save next frame
	pla	; <next frame HI>
	sta	THING_STRUCT_NXTFRM_H,y
	pla	; <next frame LO>
	sta	THING_STRUCT_NXTFRM_L,y

	; health
	pla	; <health HI>
	sta	THING_STRUCT_HEALTH_H,y
	pla	; <health LO>
	sta	THING_STRUCT_HEALTH_L,y

	; scale
	pla	; <scale>
	sta	THING_STRUCT_SCALE,y

	; gravity
	pla	; <gravity>
	sta	THING_STRUCT_GRAVITY,y

	; blocked by
	pla	; <blocked by>
	sta	THING_STRUCT_BLOCKEDBY,y

	; blocking
	pla	; <blocking>
	sta	THING_STRUCT_BLOCKING,y

	; eflags
	pla	; <eflags>
	sta	THING_STRUCT_EFLAGS,y

	; height
	pla	; <height>
	sta	THING_STRUCT_HEIGHT,y

	; radius
	pla	; <radius>
	sta	THING_STRUCT_RADIUS,y

	; origin
	pla	; <origin>
	sta	THING_STRUCT_TH_ORIGIN,y

	;;;
	;; positioning

	; check spawn position
	jsr	thing_check_position

	; check result
	lda	THMOVE_BLOCKED
	beq	@spawn_ok

	;; position check FAILED
	; place only to spawn sector
	; this ignores linked sectors
	; this ignores floor dist

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; sec->maskblock = 0
	ldx	ARG_THING_SPAWN_SECTOR
	stz	SECTOR_STRUCT_MASKBLOCK,x

	; place to sector
	lda	#$A0
	sta	THMOVE_PTR_H
	lda	ARG_THING_SPAWN_SECTOR
	sta	ARG_THING_PLACE_SECTOR
	jsr	place_to_sector

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; get sector pointer
	ldx	ARG_THING_SPAWN_SECTOR

	; get floor height
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	THMOVE_FLOORZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sta	THMOVE_FLOORZ_H

	; get ceiling height
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sta	THMOVE_CEILZ_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sta	THMOVE_CEILZ_H

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; get thing
	ldy	RET_THING_SPAWN

	; th->floorz = poscheck.floorz
	lda	THMOVE_FLOORZ_L
	sta	THING_STRUCT_FLOORZ_L,y
	lda	THMOVE_FLOORZ_H
	sta	THING_STRUCT_FLOORZ_H,y

	; th->ceilingz = poscheck.ceilingz
	; this does ommit subtraction of thing height
	lda	THMOVE_CEILZ_L
	sta	THING_STRUCT_CEILZ_L,y
	lda	THMOVE_CEILZ_H
	sta	THING_STRUCT_CEILZ_H,y

	; sector
	lda	ARG_THING_SPAWN_SECTOR
	sta	THING_STRUCT_FLOORS,y
	sta	THING_STRUCT_CEILS,y

	; done
	rts

@spawn_ok:
	;; position check PASSED

	; place to all sectors
	jsr	thing_apply_position

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; position might have changed
	ldy	RET_THING_SPAWN
	lda	ARG_THING_SPAWN_X_L
	sta	THING_STRUCT_X_L,y
	lda	ARG_THING_SPAWN_X_H
	sta	THING_STRUCT_X_H,y
	lda	ARG_THING_SPAWN_Y_L
	sta	THING_STRUCT_Y_L,y
	lda	ARG_THING_SPAWN_Y_H
	sta	THING_STRUCT_Y_H,y

	; done
	rts

;;;
; thing clear
;;;

thing_clear:
	;; CLEAR

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; clear thing types
	lda	#$FF
	ldy	#1
@loop:
	sta	THING_STRUCT_TYPE,y
	iny
	bpl	@loop

	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; clear links
	jsr	clear_bank

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; clear links
	jsr	clear_bank

	;; PLAYER WEAPON
	; thing 0 is always weapon of local player

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; current weapon
	ldx	#THING_WEAPON_FIRST

	; animation
	lda	THTYPE_ANIM_RAISE_L,x
	pha	; <next frame LO>
	sta	THTEMP_PTR_H
	and	#$F8
	ora	#THSTATE_STRUCT_SPRITE
	sta	THTEMP_PTR_L

	lda	THTEMP_PTR_H
	and	#$07
	ora	#$A0
	sta	THTEMP_PTR_H

	lda	THTYPE_ANIM_RAISE_H,x
	pha	; <next frame HI>
	lsr
	lsr
	ora	THTEMP_PTR_H
	sta	THTEMP_PTR_H

	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; the sprite
	lda	(THTEMP_PTR_L)
	bpl	:+

	; no sprite
	ldx	#$FF
	bra	:++
:
	; sprite and frame
	tax
	dec	THTEMP_PTR_L
	lda	(THTEMP_PTR_L)
	and	#$1F
	clc
	adc	TAB_SPRITE_REMAP,x
	tax
:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; save type
	lda	#THING_WEAPON_FIRST
	sta	THING_STRUCT_TYPE

	; reset weapon
	stz	THING_STRUCT_IFLAGS
	lda	#64
	sta	THING_STRUCT_COUNTER

	; save sprite
	txa
	sta	THING_STRUCT_SPRITE

	; state ticks
	lda	#1
	sta	THING_STRUCT_TICKS

	; save next frame
	pla	; <next frame HI>
	sta	THING_STRUCT_NXTFRM_H
	pla	; <next frame LO>
	sta	THING_STRUCT_NXTFRM_L

	rts

;;;
; launch thing
;;;
; assuming 'BANK_THING_STRUCT' is selected
; Y - thing index
; X - speed

thing_launch:
	; speed
	stx	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; get angle
	lda	THING_STRUCT_ANGLE,y
	pha

	; angle from pitch
	lda	THING_STRUCT_PITCH,y
	lsr
	sec
	sbc	#64
	beq	@no_pitch
	tax

	;; th->mz += tab_sin[pitch] * speed

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; speed is already set

	; tab_sin
	math_read_sin	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_S,y
	lda	VERA_DATA0
	adc	THING_STRUCT_MZ_L,y
	sta	THING_STRUCT_MZ_L,y

	;; speed = (tab_cos[pitch] * speed) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; speed is already set

	; tab_cos
	math_read_cos	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_L

@no_pitch:
	;; th->mx += tab_sin[angle] * speed

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; get angle
	plx

	; speed is already set

	; tab_sin
	math_read_sin	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	THING_STRUCT_MX_S,y
	sta	THING_STRUCT_MX_S,y
	lda	VERA_DATA0
	adc	THING_STRUCT_MX_L,y
	sta	THING_STRUCT_MX_L,y

	;; th->my += tab_cos[angle] * speed

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; speed is already set

	; tab_cos
	math_read_cos	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	THING_STRUCT_MY_S,y
	sta	THING_STRUCT_MY_S,y
	lda	VERA_DATA0
	adc	THING_STRUCT_MY_L,y
	sta	THING_STRUCT_MY_L,y

	; done
	rts

;;;
; thing tick
;;;

thing_tick:
	ldy	#0
	sty	TICK_IDX
	jmp	@do_animation

	; the loop
@thing_loop:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; thing index
	ldy	TICK_IDX

	; if(th->type >= 128)
	lda	THING_STRUCT_TYPE,y
	sta	THMOVE_THING_TYPE
	bpl	:+
	jmp	@skip_thing
:
	; decrement cooldown
	lda	THING_STRUCT_COUNTER,y
	sta	MATH_TEMP_A
	beq	:+
	dea
	sta	THING_STRUCT_COUNTER,y
:
	; old_fz = th->floorz
	lda	THING_STRUCT_FLOORZ_L,y
	sta	THMOVE_OLD_FZ_L
	lda	THING_STRUCT_FLOORZ_H,y
	sta	THMOVE_OLD_FZ_H

	; save thing Z for checks
	lda	THING_STRUCT_Z_L,y
	sta	ARG_THING_CHECK_Z_L
	lda	THING_STRUCT_Z_H,y
	sta	ARG_THING_CHECK_Z_H

	; make 'on floor' flag
	stz	ARG_THING_CHECK_ONFLOOR
	sec
	lda	THMOVE_OLD_FZ_L
	sbc	ARG_THING_CHECK_Z_L
	lda	THMOVE_OLD_FZ_H
	sbc	ARG_THING_CHECK_Z_H
	bmi	:+
	sty	ARG_THING_CHECK_ONFLOOR
:
	; if(i == player_thing)
	cpy	G_PLAYER_THING
	beq	:+
	jmp	@skip_player
:
	; check if frozen
	lda	MATH_TEMP_A
	beq	:+

	;; frozen player

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; th->angle = ticcmd.angle
	lda	THING_STRUCT_ANGLE,y
	sta	TICK_CMD_ANGLE

	; th->pitch = ticcmd.pitch
	lda	THING_STRUCT_PITCH,y
	sta	TICK_CMD_PITCH

	jmp	@skip_player
:
	;; player stuff

	; if(ticcmd.bits_l & TCMD_USE)
	lda	TICK_CMD_BITS_L
	and	#TCMD_USE
	bne	:+

	; clear use flag
	lda	THING_STRUCT_IFLAGS,y
	and	#<~THING_IFLAG_USED
	sta	THING_STRUCT_IFLAGS,y

	bra	@skip_use
:
	; check use flag
	lda	THING_STRUCT_IFLAGS,y
	and	#THING_IFLAG_USED
	bne	@skip_use

	; set use flag
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_USED
	sta	THING_STRUCT_IFLAGS,y

	jsr	player_use

@skip_use:
	; if(ticcmd.bits & TCMD_MOVING)
	ldx	TICK_CMD_BITS_L
	bmi	:+
	jmp	@skip_pmove
:
	;; th->mx += tab_sin[ticcmd.move] * thing_type[th->type].speed

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; get type
	ldx	THMOVE_THING_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; speed
	lda	THTYPE_STRUCT_SPEED,x
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; tab_sin
	lda	TICK_CMD_BITS_H
	and	#$E0
	clc
	adc	TICK_CMD_ANGLE
	tax
	math_read_sin	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	THING_STRUCT_MX_S,y
	sta	THING_STRUCT_MX_S,y
	lda	VERA_DATA0
	adc	THING_STRUCT_MX_L,y
	sta	THING_STRUCT_MX_L,y

	;; th->my += tab_cos[ticcmd.move] * thing_type[th->type].speed

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; speed is already set

	; tab_cos
	math_read_cos	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	THING_STRUCT_MY_S,y
	sta	THING_STRUCT_MY_S,y
	lda	VERA_DATA0
	adc	THING_STRUCT_MY_L,y
	sta	THING_STRUCT_MY_L,y

@skip_pmove:
	; th->angle = ticcmd.angle
	lda	TICK_CMD_ANGLE
	sta	THING_STRUCT_ANGLE,y

	; th->pitch = ticcmd.pitch
	lda	TICK_CMD_PITCH
	sta	THING_STRUCT_PITCH,y

	;; no-gravity check

	; get thing gravity
	lda	THING_STRUCT_GRAVITY,y
	sta	THMOVE_GRAVITY

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector pointer
	lda	$A000,y
	tax

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; player: swimming
	lda	#THING_TYPE_PLAYER_S
	sta	THMOVE_NEW_PTYPE

	; check sector 'water' flag
	lda	SECTOR_STRUCT_FLAGS,x
	bmi	@player_float

	; player: flying
	lda	#THING_TYPE_PLAYER_F
	sta	THMOVE_NEW_PTYPE

	; check gravity
	lda	THMOVE_GRAVITY
	beq	@player_float

	;; player crouch

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; player: normal
	lda	#THING_TYPE_PLAYER_N
	sta	THMOVE_NEW_PTYPE

	; crouch bit
	lda	TICK_CMD_BITS_L
	lsr
	bcc	:+

	; player: crouch
	lda	#THING_TYPE_PLAYER_C
	sta	THMOVE_NEW_PTYPE
:
	;; player jump

	bit	TICK_CMD_BITS_L
	bvs	:+

	; clear jump flag
	lda	THING_STRUCT_IFLAGS,y
	and	#<~THING_IFLAG_JUMPED
	sta	THING_STRUCT_IFLAGS,y

	bra	@skip_jump
:
	; if(on_floor)
	lda	ARG_THING_CHECK_ONFLOOR
	beq	@skip_jump

	; check jump flag
	lda	THING_STRUCT_IFLAGS,y
	and	#(THING_IFLAG_JUMPED|THING_IFLAG_NOJUMP)
	bne	@skip_jump

	; set jump flag
	ora	#THING_IFLAG_JUMPED
	sta	THING_STRUCT_IFLAGS,y

	; get type
	ldx	THMOVE_THING_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; read jumpz
	lda	THTYPE_STRUCT_JUMPZ,x
	sta	MATH_TEMP_A

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; th->mz = (int32_t)thing_types[th->type].jumpz << 8
	lda	#0
	sta	THING_STRUCT_MZ_S,y
	lda	MATH_TEMP_A
	sta	THING_STRUCT_MZ_L,y

	bra	@skip_jump

@player_float:
	;; player float

	; check up / down
	lda	TICK_CMD_BITS_L
	and	#(TCMD_GO_UP|TCMD_GO_DOWN)
	beq	@skip_jump

	; get float player type
	ldx	THMOVE_NEW_PTYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; read jumpz
	lda	THTYPE_STRUCT_JUMPZ,x
	sta	MATH_TEMP_A

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; check down
	lda	TICK_CMD_BITS_L
	lsr
	bcc	:+

	; go down
	sec
	lda	THING_STRUCT_MZ_L,y
	sbc	MATH_TEMP_A
	sta	THING_STRUCT_MZ_L,y

	bra	@skip_jump
:
	; go up
	clc
	lda	THING_STRUCT_MZ_L,y
	adc	MATH_TEMP_A
	sta	THING_STRUCT_MZ_L,y

@skip_jump:

	;; check type change

	lda	THMOVE_THING_TYPE
	cmp	#THING_TYPE_PLAYER_F
	bcc	@skip_pchange
	cmp	THMOVE_NEW_PTYPE
	beq	@skip_pchange

	ldx	THMOVE_NEW_PTYPE

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; old height
	lda	THING_STRUCT_HEIGHT,y
	sta	THMOVE_PTC_OLD_HEIGHT

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; check if enabled
	lda	THTYPE_STRUCT_HEIGHT,x
	beq	@skip_pchange

	; new height
	sta	THMOVE_PTC_HEIGHT

	; old view height
	ldy	THMOVE_THING_TYPE
	lda	THTYPE_STRUCT_VIEW_HEIGHT,y
	sta	THMOVE_PTC_VIEW_HEIGHT_O

	; new view height
	lda	THTYPE_STRUCT_VIEW_HEIGHT,x
	sta	THMOVE_PTC_VIEW_HEIGHT_N

	; check direction
	cmp	THMOVE_PTC_OLD_HEIGHT
	bcs	:+
	jmp	@ptc_hi_lo
:
	jmp	@ptc_lo_hi

@done_pchange:
	; restore thing index
	ldy	TICK_IDX

@skip_pchange:

@skip_player:
	; if(i == camera_thing)
	cpy	G_CAMERA_THING
	bne	@skip_camera

	;; camera stuff

	; damage flash
	lda	G_DAMAGE_LEVEL
	beq	:+
	dec	G_DAMAGE_LEVEL
:
	; projection.wh += projection.wd
	clc
	lda	G_VIEWHEIGHT
	adc	G_VIEWDELTA
	sta	G_VIEWHEIGHT

	; if(projection.wh > projection.viewheight)
	cmp	G_VIEW_HEIGHT
	bcc	:+

	; projection.wh = projection.viewheight
	lda	G_VIEW_HEIGHT
	sta	G_VIEWHEIGHT

	; projection.wd = 0
	stz	G_VIEWDELTA
:
@skip_camera:
	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector pointer
	lda	$A000,y
	sta	ARG_THING_CHECK_SECTOR

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; if(th->mx || th->my)
	lda	THING_STRUCT_MX_S,y
	ora	THING_STRUCT_MX_L,y
	ora	THING_STRUCT_MY_S,y
	ora	THING_STRUCT_MY_L,y
	bne	@do_xy_move

	; no XY movement, check overlap flag
	lda	THING_STRUCT_IFLAGS,y
	bpl	:+

	; overlap check
	jsr	thing_check_heights

	; restore thing index
	ldy	TICK_IDX

	; check result
	lda	THMOVE_BLOCKED
	bne	:+

	; apply changes
	jsr	thing_apply_heights
:
	; done
	jmp	@no_xymove

	;; XY movement
@do_xy_move:
	stz	THMOVE_TRY_COUNT
@try_move_full:
	; th->x += th->mx
	stz	MATH_TEMP_A
	lda	THING_STRUCT_MX_L,y
	bpl	:+
	dec	MATH_TEMP_A
:
	clc
	lda	THING_STRUCT_X_S,y
	adc	THING_STRUCT_MX_S,y
	sta	ARG_THING_CHECK_X_S
	lda	THING_STRUCT_X_L,y
	adc	THING_STRUCT_MX_L,y
	sta	ARG_THING_CHECK_X_L
	lda	THING_STRUCT_X_H,y
	adc	MATH_TEMP_A
	sta	ARG_THING_CHECK_X_H

	; th->y += th->my
	stz	MATH_TEMP_A
	lda	THING_STRUCT_MY_L,y
	bpl	:+
	dec	MATH_TEMP_A
:
	clc
	lda	THING_STRUCT_Y_S,y
	adc	THING_STRUCT_MY_S,y
	sta	ARG_THING_CHECK_Y_S
	lda	THING_STRUCT_Y_L,y
	adc	THING_STRUCT_MY_L,y
	sta	ARG_THING_CHECK_Y_L
	lda	THING_STRUCT_Y_H,y
	adc	MATH_TEMP_A
	sta	ARG_THING_CHECK_Y_H

	;; check new position
@try_move_cut:
	jsr	thing_check_position

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; check result
	lda	THMOVE_BLOCKED
	bmi	@move_stop
	lda	RET_THING_CHECK_NEWSEC
	bne	@move_ok

	; check projectile flag
	lda	THING_STRUCT_EFLAGS,y
	and	#THING_EFLAG_PROJECTILE
	beq	@not_projectile

	; check for math error
	lda	THMOVE_BLOCKED
	bne	:+

	; remove thing
	sta	THING_STRUCT_NXTFRM_L,y
	sta	THING_STRUCT_NXTFRM_H,y
	ina
	sta	THING_STRUCT_TICKS,y
	jmp	@do_animation
:
	; projectile death
	jsr	projectile_death
	jmp	@do_animation

@not_projectile:
	; check sliding flag
	lda	THING_STRUCT_EFLAGS,y
	bpl	@move_stop

	;; try sliding
	lda	THMOVE_TRY_COUNT
	tax
	ina
	ina
	sta	THMOVE_TRY_COUNT
	jmp	(@slide_try_jmp,x)

@move_stop:
	; can't move, stop
	lda	#0
	sta	THING_STRUCT_MX_S,y
	sta	THING_STRUCT_MX_L,y
	sta	THING_STRUCT_MY_S,y
	sta	THING_STRUCT_MY_L,y

	bra	@move_skip

@move_ok:
	; do the movement
	lda	ARG_THING_CHECK_X_S
	sta	THING_STRUCT_X_S,y
	lda	ARG_THING_CHECK_X_L
	sta	THING_STRUCT_X_L,y
	lda	ARG_THING_CHECK_X_H
	sta	THING_STRUCT_X_H,y
	lda	ARG_THING_CHECK_Y_S
	sta	THING_STRUCT_Y_S,y
	lda	ARG_THING_CHECK_Y_L
	sta	THING_STRUCT_Y_L,y
	lda	ARG_THING_CHECK_Y_H
	sta	THING_STRUCT_Y_H,y

	; sliding check
	lda	THMOVE_TRY_COUNT
	beq	:+

	; poscheck.iflags &= ~THING_IFLAG_NOJUMP
	lda	#THING_IFLAG_NOJUMP
	trb	THMOVE_IFLAGS
:
	; use new sector links
	jsr	thing_apply_position

	; use new sector
	lda	RET_THING_CHECK_NEWSEC
	sta	ARG_THING_CHECK_SECTOR

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

@move_skip:
	;; XY friction

	; check flag
	lda	THING_STRUCT_EFLAGS,y
	and	#THING_EFLAG_PROJECTILE
	bne	@no_xy_friction

	; thing
	ldx	TICK_IDX

	; th->mx /= 2
	lda	THING_STRUCT_MX_L,x
	rol
	ror	THING_STRUCT_MX_L,x
	ror	THING_STRUCT_MX_S,x

	lda	THING_STRUCT_MX_L,x
	bpl	:+
	eor	#$FF
:
	bne	:+
	stz	THING_STRUCT_MX_L,x
	stz	THING_STRUCT_MX_S,x
:
	; th->my /= 2
	lda	THING_STRUCT_MY_L,x
	rol
	ror	THING_STRUCT_MY_L,x
	ror	THING_STRUCT_MY_S,x

	lda	THING_STRUCT_MY_L,x
	bpl	:+
	eor	#$FF
:
	bne	:+
	stz	THING_STRUCT_MY_L,x
	stz	THING_STRUCT_MY_S,x
:
	; th->my /= 2

@no_xy_friction:
@no_xymove:
	; load height limits
	lda	THING_STRUCT_FLOORZ_L,y
	sta	THMOVE_FLOORZ_L
	lda	THING_STRUCT_FLOORZ_H,y
	sta	THMOVE_FLOORZ_H
	lda	THING_STRUCT_CEILZ_L,y
	sta	THMOVE_CEILZ_L
	lda	THING_STRUCT_CEILZ_H,y
	sta	THMOVE_CEILZ_H

	;; Z movement

	; if(th->mz)
	lda	THING_STRUCT_MZ_L,y
	tax
	ora	THING_STRUCT_MZ_S,y
	beq	:++

	; th->z += th->mz
	stz	MATH_TEMP_A
	txa
	bpl	:+
	dec	MATH_TEMP_A
:
	clc
	lda	THING_STRUCT_MZ_S,y
	adc	THING_STRUCT_Z_S,y
	sta	THING_STRUCT_Z_S,y
	txa
	adc	THING_STRUCT_Z_L,y
	sta	THING_STRUCT_Z_L,y
	lda	MATH_TEMP_A
	adc	THING_STRUCT_Z_H,y
	sta	THING_STRUCT_Z_H,y
:
	; if(zz > th->ceilingz)
	sec
	lda	THMOVE_CEILZ_L
	sbc	THING_STRUCT_Z_L,y
	lda	THMOVE_CEILZ_H
	sbc	THING_STRUCT_Z_H,y
	bpl	@ceiling_ok

	; th->z = th->ceilingz << 8
	lda	THMOVE_CEILZ_L
	sta	THING_STRUCT_Z_L,y
	lda	THMOVE_CEILZ_H
	sta	THING_STRUCT_Z_H,y
	lda	#0
	sta	THING_STRUCT_Z_S,y

	; th->mz = 0
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y

	; check projectile flag
	lda	THING_STRUCT_EFLAGS,y
	and	#THING_EFLAG_PROJECTILE
	beq	:+

	; poscheck.blocked = 8
	lda	#8
	sta	THMOVE_BLOCKED

	; explode
	jsr	projectile_death
	jmp	@do_animation
:
@ceiling_ok:
	; floor zdiff
	sec
	lda	THING_STRUCT_Z_L,y
	sbc	THMOVE_FLOORZ_L
	sta	MATH_TEMP_A
	lda	THING_STRUCT_Z_H,y
	sbc	THMOVE_FLOORZ_H
	bpl	@not_in_floor

	;; inside floor

	; if(i == camera_thing)
	cpy	G_CAMERA_THING
	bne	@no_camera_dip

	; if(old_fz < th->floorz)
	sec
	lda	THMOVE_OLD_FZ_L
	sbc	THMOVE_FLOORZ_L
	lda	THMOVE_OLD_FZ_H
	sbc	THMOVE_FLOORZ_H
	bmi	:+

	; momentnum based dip
	lda	THING_STRUCT_MZ_L,y
	sec
	ror
	sta	MATH_TEMP_A
:
	; projection.wh += zdiff
	clc
	lda	G_VIEWHEIGHT
	adc	MATH_TEMP_A
	bpl	:+
	lda	#0
:
	sta	G_VIEWHEIGHT

	; projection.wd = (projection.viewheight - projection.wh) >> 1
	; with zero check
	sec
	lda	G_VIEW_HEIGHT
	sbc	G_VIEWHEIGHT
	lsr
	bne	:+
	ina
:
	sta	G_VIEWDELTA

@no_camera_dip:
	; th->z = th->floorz << 8
	lda	THMOVE_FLOORZ_L
	sta	THING_STRUCT_Z_L,y
	lda	THMOVE_FLOORZ_H
	sta	THING_STRUCT_Z_H,y
	lda	#0
	sta	THING_STRUCT_Z_S,y

	; th->mz = 0
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y

	; on_floor = 1
	sty	ARG_THING_CHECK_ONFLOOR

	; check projectile flag
	lda	THING_STRUCT_EFLAGS,y
	and	#THING_EFLAG_PROJECTILE
	beq	:+

	; poscheck.blocked = 4
	lda	#4
	sta	THMOVE_BLOCKED

	; explode
	jsr	projectile_death
	jmp	@do_animation
:
@not_in_floor:
	; get thing flags
	lda	THING_STRUCT_EFLAGS,y
	sta	THMOVE_EFLAGS

	; get thing gravity
	lda	THING_STRUCT_GRAVITY,y
	beq	@do_z_friction

	; prepare gravity
	tax
	lda	TAB_SWAP,x
	and	#$F0
	sta	TMP_THING_GRAVITY_S
	lda	TAB_SWAP,x
	and	#$0F
	sta	TMP_THING_GRAVITY_L

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; get sector pointer
	ldx	ARG_THING_SPAWN_SECTOR

	; check sector 'water' flag
	lda	SECTOR_STRUCT_FLAGS,x
	bpl	@no_water

	; check water special flag
	lda	THMOVE_EFLAGS
	and	#THING_EFLAG_WATERSPEC
	beq	@do_z_friction

	; double the gravity value
	asl	TMP_THING_GRAVITY_S
	rol	TMP_THING_GRAVITY_L

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; check float direction
	ldx	THMOVE_THING_TYPE
	lda	THTYPE_STRUCT_WATER_HEIGHT,x
	cmp	THTYPE_STRUCT_VIEW_HEIGHT,x
	bcc	@apply_float

	; invert gravity
	lda	TMP_THING_GRAVITY_L
	eor	#$FF
	sta	TMP_THING_GRAVITY_L
	lda	TMP_THING_GRAVITY_S
	eor	#$FF
	ina
	sta	TMP_THING_GRAVITY_S
	bne	:+
	inc	TMP_THING_GRAVITY_L
:
@apply_float:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; set th->mz directly
	lda	TMP_THING_GRAVITY_S
	sta	THING_STRUCT_MZ_S,y
	lda	TMP_THING_GRAVITY_L
	sta	THING_STRUCT_MZ_L,y

	bra	@skip_z_friction

@no_water:
	;;;
	; gravity

	; floor check
	lda	ARG_THING_CHECK_ONFLOOR
	bne	@skip_z_friction

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; th->mz -= th->gravity << 4;
	sec
	lda	THING_STRUCT_MZ_S,y
	sbc	TMP_THING_GRAVITY_S
	sta	THING_STRUCT_MZ_S,y
	lda	THING_STRUCT_MZ_L,y
	sbc	TMP_THING_GRAVITY_L
	sta	THING_STRUCT_MZ_L,y

	bra	@skip_z_friction

@do_z_friction:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; check flag
	lda	THING_STRUCT_EFLAGS,y
	and	#THING_EFLAG_PROJECTILE
	bne	@skip_z_friction

	ldx	TICK_IDX

	; th->mz /= 2
	lda	THING_STRUCT_MZ_L,x
	rol
	ror	THING_STRUCT_MZ_L,x
	ror	THING_STRUCT_MZ_S,x

	lda	THING_STRUCT_MZ_L,x
	bpl	:+
	eor	#$FF
:
	bne	:+
	stz	THING_STRUCT_MZ_L,x
	stz	THING_STRUCT_MZ_S,x
:
@skip_z_friction:

	;;;
	; linked sectors

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; get sector pointer
	ldx	ARG_THING_SPAWN_SECTOR

	; check links
	lda	SECTOR_STRUCT_FLOOR_LINK,x
	ora	SECTOR_STRUCT_CEILING_LINK,x
	beq	@skip_links

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; read type
	ldx	THING_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; read view height
	lda	THTYPE_STRUCT_VIEW_HEIGHT,x

	; thing structure bank
	ldx	#BANK_THING_STRUCT
	stx	REG_RAM_BANK

	; zz = (th->z >> 8) + thing_type[th->type].view_height
	clc
	adc	THING_STRUCT_Z_L,y
	sta	THMOVE_ZZZ_L
	lda	THING_STRUCT_Z_H,y
	adc	#0
	sta	THMOVE_ZZZ_H

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; get sector pointer
	ldx	ARG_THING_SPAWN_SECTOR

	; check floor
	lda	SECTOR_STRUCT_FLOOR_LINK,x
	beq	@skip_floor_link

	; if(zz < sec->floor.height)
	sec
	lda	THMOVE_ZZZ_L
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	lda	THMOVE_ZZZ_H
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	bpl	@skip_floor_link

	; apply new main sector
	lda	SECTOR_STRUCT_FLOOR_LINK,x
	bra	@do_swap

@skip_floor_link:
	; check ceiling
	lda	SECTOR_STRUCT_CEILING_LINK,x
	beq	@skip_ceiling_link

	; if(zz > sec->ceiling.height)
	sec
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sbc	THMOVE_ZZZ_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sbc	THMOVE_ZZZ_H
	bpl	@skip_ceiling_link

	; apply new main sector
	lda	SECTOR_STRUCT_CEILING_LINK,x
@do_swap:
	sta	RET_THING_CHECK_NEWSEC
	jsr	swap_main_sector

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; force height check
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,y

@skip_ceiling_link:
@skip_links:

@do_animation:
	;;;
	; animation

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; if(th->ticks)
	lda	THING_STRUCT_TICKS,y
	beq	@skip_thing

	; th->ticks--
	dea
	sta	THING_STRUCT_TICKS,y

	; if(!th->ticks)
	bne	@skip_thing

@act_again:
	; if(th->next_state)
	lda	THING_STRUCT_NXTFRM_H,y
	and	#$7F
	ora	THING_STRUCT_NXTFRM_L,y
	beq	@remove_thing

	; get new state
	lda	THING_STRUCT_NXTFRM_L,y
	sta	THANIM_PTR_H
	and	#$F8
	sta	THANIM_PTR_L

	lda	THANIM_PTR_H
	and	#$07
	ora	#$A0
	sta	THANIM_PTR_H

	lda	THING_STRUCT_NXTFRM_H,y
	lsr
	lsr
	ora	THANIM_PTR_H
	sta	THANIM_PTR_H

	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; action
	lda	(THANIM_PTR_L)
	beq	:+

	; run action
	asl
	tax
	jsr	action_run
	bcc	:+

	; repeat
	ldy	TICK_IDX
	bra	@act_again
:
	; next (lo)
	ldy	#1
	lda	(THANIM_PTR_L),y
	sta	TMP_THING_NXTFRM_L

	; frame / next (hi)
	iny
	lda	(THANIM_PTR_L),y
	sta	TMP_THING_NXTFRM_H

	; sprite
	iny
	lda	(THANIM_PTR_L),y
	tax

	; ticks
	iny
	lda	(THANIM_PTR_L),y
	pha	; <ticks>

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; restore thing index
	ldy	TICK_IDX

	; sprite
	txa
	bpl	:+

	; no sprite
	sta	THING_STRUCT_SPRITE,y

	bra	:++
:
	; add frame
	lda	TMP_THING_NXTFRM_H
	and	#$1F
	clc
	adc	TAB_SPRITE_REMAP,x
	sta	THING_STRUCT_SPRITE,y
:
	; next state
	lda	TMP_THING_NXTFRM_L
	sta	THING_STRUCT_NXTFRM_L,y
	lda	TMP_THING_NXTFRM_H
	and	#$E0
	sta	THING_STRUCT_NXTFRM_H,y

	; ticks
	pla	; <ticks>
	sta	THING_STRUCT_TICKS,y

@skip_thing:
	; TIME
	jsr	time_update

	; next
	inc	TICK_IDX
	bmi	:+
	jmp	@thing_loop
:
	rts

@remove_thing:
	; check if weapon
	ldy	TICK_IDX
	bne	:+

	; get thing type
	ldx	THING_STRUCT_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; read animation
	lda	THTYPE_ANIM_READY_L,x
	pha
	lda	THTYPE_ANIM_READY_H,x
	tax

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; set state
	pla
	sta	THING_STRUCT_NXTFRM_L
	stx	THING_STRUCT_NXTFRM_H

	jmp	@act_again
:
	; remove from all sectors
	stz	RET_THING_CHECK_NEWSEC
	jsr	thing_apply_position

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; delete thing
	lda	#$FF
	sta	THING_STRUCT_TYPE,y

	; remove links from other things
	ldy	#0
@loop_del_lnk:
	; origin
	lda	THING_STRUCT_TH_ORIGIN,y
	cmp	TICK_IDX
	bne	:+
	lda	#0
	sta	THING_STRUCT_TH_ORIGIN,y
:
	; target
	lda	THING_STRUCT_TH_TARGET,y
	cmp	TICK_IDX
	bne	:+
	lda	#0
	sta	THING_STRUCT_TH_TARGET,y
:
	; next
	dey
	bne	@loop_del_lnk

	; skip states
	bra	@skip_thing

@slide_try_0:
	; modify momentnum using wall angle
	; try to slide using wall angle

	; p2a_coord.x = th->mx
	lda	THING_STRUCT_MX_S,y
	sta	MATH_P2A_X_L
	lda	THING_STRUCT_MX_L,y
	sta	MATH_P2A_X_H

	; p2a_coord.y = th->my
	lda	THING_STRUCT_MY_S,y
	sta	MATH_P2A_Y_L
	lda	THING_STRUCT_MY_L,y
	sta	MATH_P2A_Y_H

	; mx = abs(p2a_coord.x)
	lda	MATH_P2A_X_H
	bpl	:+
	sec
	lda	#0
	sbc	MATH_P2A_X_L
	sta	THMOVE_WMX_S
	lda	#0
	sbc	MATH_P2A_X_H
	sta	THMOVE_WMX_L
	bra	:++
:
	sta	THMOVE_WMX_L
	lda	MATH_P2A_X_L
	sta	THMOVE_WMX_S
:
	; my = abs(p2a_coord.y)
	lda	MATH_P2A_Y_H
	bpl	:+
	sec
	lda	#0
	sbc	MATH_P2A_Y_L
	sta	THMOVE_WMY_S
	lda	#0
	sbc	MATH_P2A_Y_H
	sta	THMOVE_WMY_L
	bra	:++
:
	sta	THMOVE_WMY_L
	lda	MATH_P2A_Y_L
	sta	THMOVE_WMY_S
:
	; if(my > mx)
	sec
	lda	THMOVE_WMX_S
	sbc	THMOVE_WMY_S
	lda	THMOVE_WMX_L
	sbc	THMOVE_WMY_L
	bpl	:+
	; mx <-> my
	ldx	THMOVE_WMX_S
	lda	THMOVE_WMY_S
	sta	THMOVE_WMX_S
	stx	THMOVE_WMY_S
	ldx	THMOVE_WMX_L
	lda	THMOVE_WMY_L
	sta	THMOVE_WMX_L
	stx	THMOVE_WMY_L
:
	; dist = mx + my / 2
	lsr	THMOVE_WMY_L
	ror	THMOVE_WMY_S
	clc
	adc	THMOVE_WMX_S
	sta	THMOVE_DIST_L
	lda	THMOVE_WMY_L
	adc	THMOVE_WMX_L
	sta	THMOVE_DIST_H

	; point_to_angle() >> 4
	jsr	math_p2a
	math_lsr4_u16	MATH_VAR_L, MATH_VAR_H
	sta	THMOVE_ANG_L

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; ang = poscheck.angle
	lda	THMOVE_BVECT_ANG

	; if(poscheck.blocked & 0x40)
	bit	THMOVE_BLOCKED
	bvc	:+

	; ang ^= 0x80
	eor	#$80
:
	; ang = ang - p2a + 0xC0
	sec
	sbc	THMOVE_ANG_L
	clc
	adc	#$C0
	sta	THMOVE_ANG_L
	stz	THMOVE_ANG_H
	bpl	:+
	dec	THMOVE_ANG_H
:
	; ang *= 4
	lda	THMOVE_ANG_L
	asl
	sta	THMOVE_ANG_L
	lda	THMOVE_ANG_H
	rol
	sta	THMOVE_ANG_H
	lda	THMOVE_ANG_L
	asl
	sta	VERA_FX_CACHE_H
	lda	THMOVE_ANG_H
	rol
	sta	VERA_FX_CACHE_U

	; dist = dist * ang
	; dist = -dist * ang

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; ang is already set

	; dist
	lda	THMOVE_DIST_L
	sta	VERA_FX_CACHE_L
	lda	THMOVE_DIST_H
	sta	VERA_FX_CACHE_M

	; if(poscheck.blocked & 0x40)
	bit	THMOVE_BLOCKED
	bvc	:+

	; -dist
	sec
	lda	#0
	sbc	THMOVE_DIST_L
	sta	VERA_FX_CACHE_L
	lda	#0
	sbc	THMOVE_DIST_H
	sta	VERA_FX_CACHE_M
:
	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	THMOVE_DIST_L
	lda	VERA_DATA0
	sta	THMOVE_DIST_H

	; th->mx = (poscheck.dist.x * dist) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist
	lda	THMOVE_DIST_L
	sta	VERA_FX_CACHE_L
	lda	THMOVE_DIST_H
	sta	VERA_FX_CACHE_M

	; poscheck.dist.x
	lda	THMOVE_BVECT_X_L
	sta	VERA_FX_CACHE_H
	lda	THMOVE_BVECT_X_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; thing IDX
	ldy	TICK_IDX

	; get result (>> 8)
	lda	VERA_DATA0
	sta	THING_STRUCT_MX_S,y
	lda	VERA_DATA0
	sta	THING_STRUCT_MX_L,y

	; th->my = (poscheck.dist.y * dist) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; poscheck.dist.y
	lda	THMOVE_BVECT_Y_L
	sta	VERA_FX_CACHE_H
	lda	THMOVE_BVECT_Y_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; thing IDX
	ldy	TICK_IDX

	; get result (>> 8)
	lda	VERA_DATA0
	sta	THING_STRUCT_MY_S,y
	lda	VERA_DATA0
	sta	THING_STRUCT_MY_L,y

	; try new position
	jmp	@try_move_full

@slide_try_1:
	; stop
	lda	#0
	sta	THING_STRUCT_MX_S,y
	sta	THING_STRUCT_MX_L,y
	sta	THING_STRUCT_MY_S,y
	sta	THING_STRUCT_MY_L,y

	; wiggle push

	; ang = level_tick << 5
	lda	G_LEVEL_TICK_L
	ror
	ror
	ror
	ror
	and	#$E0

	; sin
	tax
	math_read_sin	MATH_VAR_L, MATH_VAR_H
	stz	MATH_VAR_U

	; th->x
	clc
	lda	MATH_VAR_L
	adc	THING_STRUCT_X_S,y
	sta	ARG_THING_CHECK_X_S
	lda	MATH_VAR_H
	bpl	:+
	dec	MATH_VAR_U
:
	adc	THING_STRUCT_X_L,y
	sta	ARG_THING_CHECK_X_L
	lda	MATH_VAR_U
	adc	THING_STRUCT_X_H,y
	sta	ARG_THING_CHECK_X_H

	; cos
	math_read_cos	MATH_VAR_L, MATH_VAR_H
	ldx	#0

	; th->y
	clc
	lda	MATH_VAR_L
	adc	THING_STRUCT_Y_S,y
	sta	ARG_THING_CHECK_Y_S
	lda	MATH_VAR_H
	bpl	:+
	dex
:
	adc	THING_STRUCT_Y_L,y
	sta	ARG_THING_CHECK_Y_L
	txa
	adc	THING_STRUCT_Y_H,y
	sta	ARG_THING_CHECK_Y_H

	; try new position
	jmp	@try_move_cut

@slide_try_jmp:
	.word	@slide_try_0
	.word	@slide_try_1
	.word	@move_stop

	;;;
	; player change (HI -> LO)

@ptc_hi_lo:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; thing index in X
	ldx	TICK_IDX

	; ceilingz = th->ceilingz + th->height
	clc
	lda	THING_STRUCT_CEILZ_L,x
	adc	THMOVE_PTC_OLD_HEIGHT
	sta	THMOVE_PTC_CEILZ_L
	lda	THING_STRUCT_CEILZ_H,x
	adc	#0
	sta	THMOVE_PTC_CEILZ_H

	; old gravity
	lda	THING_STRUCT_GRAVITY,x
	sta	THMOVE_PTC_GRAVITY

	; diff = thing_type[th->type].view_height - ti->view_height
	sec
	lda	THMOVE_PTC_VIEW_HEIGHT_O
	sbc	THMOVE_PTC_VIEW_HEIGHT_N
	tay

	; ceilingz -= ti->height
	sec
	lda	THMOVE_PTC_CEILZ_L
	sbc	THMOVE_PTC_HEIGHT
	sta	THMOVE_PTC_CEILZ_L
	bcs	:+
	dec	THMOVE_PTC_CEILZ_H
:
	; th->z += diff << 8
	clc
	tya
	adc	ARG_THING_CHECK_Z_L
	sta	ARG_THING_CHECK_Z_L
	sta	THING_STRUCT_Z_L,x
	bcc	:+
	inc	ARG_THING_CHECK_Z_H
:
	lda	ARG_THING_CHECK_Z_H
	sta	THING_STRUCT_Z_H,x

	; th->mz -= th->gravity
	sec
	lda	THING_STRUCT_MZ_S,x
	sbc	THMOVE_PTC_GRAVITY
	sta	THING_STRUCT_MZ_S,x
	bcs	:+
	dec	THING_STRUCT_MZ_L,x
:
	; th->type = new_type
	lda	THMOVE_NEW_PTYPE
	sta	THMOVE_THING_TYPE
	sta	THING_STRUCT_TYPE,x

	; th->height = ti->height
	lda	THMOVE_PTC_HEIGHT
	sta	THING_STRUCT_HEIGHT,x

	; th->ceilingz = ceilingz
	lda	THMOVE_PTC_CEILZ_L
	sta	THING_STRUCT_CEILZ_L,x
	lda	THMOVE_PTC_CEILZ_H
	sta	THING_STRUCT_CEILZ_H,x

	; th->iflags |= THING_IFLAG_HEIGHTCHECK
	lda	THING_STRUCT_IFLAGS,x
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,x

	; on_floor = no
	stz	ARG_THING_CHECK_ONFLOOR

	; if(i == camera_thing)
	cpx	G_CAMERA_THING
	bne	:+

	; projection.viewheight = ti->view_height
	; projection.wh = ti->view_height
	lda	THMOVE_PTC_VIEW_HEIGHT_N
	sta	G_VIEW_HEIGHT
	sta	G_VIEWHEIGHT

	; projection.wd = 0
	stz	G_VIEWDELTA
:
	; done
	jmp	@done_pchange

	;;;
	; player change (LO -> HI)

@ptc_lo_hi:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; thing index in X
	ldx	TICK_IDX

	; diff = th->ceilingz + th->height
	clc
	lda	THING_STRUCT_CEILZ_L,x
	adc	THMOVE_PTC_OLD_HEIGHT
	sta	THMOVE_PTC_DIFF_L
	lda	THING_STRUCT_CEILZ_H,x
	adc	#0
	sta	THMOVE_PTC_DIFF_H

	; save floorz
	lda	THING_STRUCT_FLOORZ_L,x
	sta	THMOVE_PTC_FLOORZ_L
	lda	THING_STRUCT_FLOORZ_H,x
	sta	THMOVE_PTC_FLOORZ_H

	; diff -= ti->height
	sec
	lda	THMOVE_PTC_DIFF_L
	sbc	THMOVE_PTC_HEIGHT
	sta	THMOVE_PTC_DIFF_L
	bcs	:+
	dec	THMOVE_PTC_DIFF_H
:
	; if(diff >= (th->z >> 8))
	sec
	lda	THMOVE_PTC_DIFF_L
	sbc	ARG_THING_CHECK_Z_L
	lda	THMOVE_PTC_DIFF_H
	sbc	ARG_THING_CHECK_Z_H
	bpl	:+

	; failed (new height is too big)
	jmp	@done_pchange
:
	; wiff = ti->view_height - thing_type[th->type].view_height
	sec
	lda	THMOVE_PTC_VIEW_HEIGHT_N
	sbc	THMOVE_PTC_VIEW_HEIGHT_O
	sta	THMOVE_PTC_WIFF

	; th->z -= wiff << 8
	sec
	lda	ARG_THING_CHECK_Z_L
	sbc	THMOVE_PTC_WIFF
	sta	ARG_THING_CHECK_Z_L
	bcs	:+
	dec	ARG_THING_CHECK_Z_H
:
	; ziff = (th->z >> 8) - th->floorz
	sec
	lda	ARG_THING_CHECK_Z_L
	sbc	THMOVE_PTC_FLOORZ_L
	sta	THMOVE_PTC_ZIFF
	lda	ARG_THING_CHECK_Z_H
	sbc	THMOVE_PTC_FLOORZ_H

	; if(ziff < 0)
	bpl	:+

	; th->z = th->floorz << 8
	lda	#0
	sta	THING_STRUCT_Z_S,x
	lda	THMOVE_PTC_FLOORZ_L
	sta	ARG_THING_CHECK_Z_L
	lda	THMOVE_PTC_FLOORZ_H
	sta	ARG_THING_CHECK_Z_H

	; wiff += ziff
	clc
	lda	THMOVE_PTC_WIFF
	adc	THMOVE_PTC_ZIFF
	sta	THMOVE_PTC_WIFF

	; on_floor = yes
	sty	ARG_THING_CHECK_ONFLOOR
:
	lda	ARG_THING_CHECK_Z_L
	sta	THING_STRUCT_Z_L,x
	lda	ARG_THING_CHECK_Z_H
	sta	THING_STRUCT_Z_H,x

	; th->type = new_type
	lda	THMOVE_NEW_PTYPE
	sta	THMOVE_THING_TYPE
	sta	THING_STRUCT_TYPE,x

	; th->height = ti->height
	lda	THMOVE_PTC_HEIGHT
	sta	THING_STRUCT_HEIGHT,x

	; th->ceilingz = diff
	lda	THMOVE_PTC_DIFF_L
	sta	THING_STRUCT_CEILZ_L,x
	lda	THMOVE_PTC_DIFF_H
	sta	THING_STRUCT_CEILZ_H,x

	; th->iflags |= THING_IFLAG_HEIGHTCHECK
	lda	THING_STRUCT_IFLAGS,x
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,x

	; if(i == camera_thing)
	cpx	G_CAMERA_THING
	bne	@skip_lo_hi_cam

	; projection.wh += wiff
	clc
	lda	G_VIEWHEIGHT
	adc	THMOVE_PTC_WIFF
	sta	G_VIEWHEIGHT

	; projection.viewheight = ti->view_height
	lda	THMOVE_PTC_VIEW_HEIGHT_N
	sta	G_VIEW_HEIGHT

	; halve, for checking
	lsr
	sta	THMOVE_PTC_VIEW_HEIGHT_N

	; projection.wd = projection.viewheight - projection.wh
	sec
	lda	G_VIEW_HEIGHT
	sbc	G_VIEWHEIGHT

	; limit to half view height
	cmp	THMOVE_PTC_VIEW_HEIGHT_N
	bcc	:+
	lda	THMOVE_PTC_VIEW_HEIGHT_N
:
	; projection.wd >>= 1
	; with zero check
	lsr
	bne	:+
	ina
:
	sta	G_VIEWDELTA

@skip_lo_hi_cam:
	; done
	jmp	@done_pchange

;;;
; swap thing main sector
;;;
; Y - thing index
; RET_THING_CHECK_NEWSEC - new main sector

swap_main_sector:
	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; find slot
	ldx	#15
	lda	#$A1
	sta	TMP_PTR_H
	stz	TMP_PTR_L
@loop:
	lda	(TMP_PTR_L),y
	beq	@stop
	cmp	RET_THING_CHECK_NEWSEC
	bne	@next

	; swap sector
	lda	$A000,y
	sta	(TMP_PTR_L),y
	lda	RET_THING_CHECK_NEWSEC
	sta	$A000,y

	; swap index
	lda	#$80
	sta	TMP_PTR_L
	ldx	$A080,y
	lda	(TMP_PTR_L),y
	sta	$A080,y
	txa
	sta	(TMP_PTR_L),y

	; done
	rts

@next:
	; next
	inc	TMP_PTR_H
	dex
	bne	@loop
@stop:
	rts

;;;
; check thing heights (at current position)
;;;
; Y - thing index
; THMOVE_BLOCKED - 0 = not blocked; otherwise blocked

thing_check_heights:
	; save thing
	sty	TMP_PTR_L
	sty	THMOVE_THING
	tya
	ora	#$80
	sta	THCES_PTR_L

	; reset heights
	stz	THMOVE_FLOORZ_L
	stz	THMOVE_WATERZ_L
	lda	#$C0
	sta	THMOVE_FLOORZ_H
	sta	THMOVE_WATERZ_H
	stz	THMOVE_CEILZ_L
	lda	#$40
	sta	THMOVE_CEILZ_H

	; reset info
	stz	THMOVE_IFLAGS
	stz	THMOVE_BLOCKED
	lda	#$40
	sta	THMOVE_ISLINK

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; read current location
	lda	THING_STRUCT_X_L,y
	sta	ARG_THING_CHECK_X_L
	lda	THING_STRUCT_X_H,y
	sta	ARG_THING_CHECK_X_H
	lda	THING_STRUCT_Y_L,y
	sta	ARG_THING_CHECK_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	ARG_THING_CHECK_Y_H
	lda	THING_STRUCT_Z_L,y
	sta	ARG_THING_CHECK_Z_L
	lda	THING_STRUCT_Z_H,y
	sta	ARG_THING_CHECK_Z_H

	; prepare variables
	jsr	prepare_pos_check

	and	#$7F
	sta	THMOVE_STEP_HEIGHT

	; th_zw = th_zb + view_height
	clc
	lda	ARG_THING_CHECK_Z_L
	adc	THTYPE_STRUCT_VIEW_HEIGHT,x
	sta	THMOVE_THZW_L
	lda	ARG_THING_CHECK_Z_H
	adc	#0
	sta	THMOVE_THZW_H

	; loop trough sectors
	lda	#$A0
	sta	TMP_PTR_H	; thingsec
	sta	THCES_PTR_H	; thingces
@loop:
	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; get sector
	lda	(TMP_PTR_L)
	beq	@stop
	sta	THMOVE_SDX
	tax

	; poscheck.maskblock = thingces[tdx][i] & 0x80
	lda	(THCES_PTR_L)
	and	#$80
	sta	THMOVE_MASKBLOCK

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; sector heights
	jsr	update_sector_heights

	; check things
	jsr	check_sector_things
	bcc	@end

	inc	THCES_PTR_H
	inc	TMP_PTR_H
	lda	TMP_PTR_H
	cmp	#$B0
	bne	@loop

@stop:
	;; fix and check height

	; poscheck.ceilingz -= height
	sec
	lda	THMOVE_CEILZ_L
	sbc	THMOVE_HEIGHT
	sta	THMOVE_CEILZ_L
	bcs	:+
	dec	THMOVE_CEILZ_H
:
	; if(poscheck.ceilingz < poscheck.floorz)
	sec
	lda	THMOVE_CEILZ_L
	sbc	THMOVE_FLOORZ_L
	lda	THMOVE_CEILZ_H
	sbc	THMOVE_FLOORZ_H
	bpl	:+

	; blocked
	sta	THMOVE_BLOCKED
	rts
:
	; if(poscheck.floorz <= poscheck.waterz)
	sec
	lda	THMOVE_WATERZ_L
	sbc	THMOVE_FLOORZ_L
	lda	THMOVE_WATERZ_H
	sbc	THMOVE_FLOORZ_H
	bmi	@end

	; poscheck.iflags |= THING_IFLAG_NOJUMP
	lda	#THING_IFLAG_NOJUMP
	tsb	THMOVE_IFLAGS
@end:
	rts

;;;
; check thing position
;;;
; ATTENTION: This function, in rare cases, modifies ARG_THING_CHECK_* argumens. These modified values must be used!
; Y - thing index
; ARG_THING_CHECK_X_L and ARG_THING_CHECK_X_H (S is not used)
; ARG_THING_CHECK_Y_L and ARG_THING_CHECK_Y_H (S is not used)
; ARG_THING_CHECK_Z_L and ARG_THING_CHECK_Z_H (S does not exist)
; ARG_THING_CHECK_SECTOR - use zero to get sector of the thing
; ARG_THING_CHECK_ONFLOOR - set to zero to halve step height
; RET_THING_CHECK_NEWSEC - new sector this thing is in, zero on collision
; THMOVE_BLOCKED - 0 = not blocked; positive = blocked (normal, can slide); negative = blocked (special)
; NOTE: even more stuff is set for use in 'thing_apply_position'

thing_check_position:
	sty	THMOVE_THING

	; check if sector is requested
	lda	ARG_THING_CHECK_SECTOR
	bne	:+

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector pointer
	lda	$A000,y
	sta	ARG_THING_CHECK_SECTOR
:
	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; poscheck.sector = 0
	stz	RET_THING_CHECK_NEWSEC

	; poscheck.fallback = 0
	stz	THMOVE_FALLBACK

	; check thing count in base sector
	ldx	ARG_THING_CHECK_SECTOR
	lda	$BF00,x
	cmp	#31
	bcc	:+
	jmp	@do_block_extra
:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; read current location
	lda	THING_STRUCT_X_L,y
	sta	THMOVE_START_X_L
	lda	THING_STRUCT_X_H,y
	sta	THMOVE_START_X_H
	lda	THING_STRUCT_Y_L,y
	sta	THMOVE_START_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	THMOVE_START_Y_H

	; prepare variables
	jsr	prepare_pos_check
	sta	THMOVE_STEP_HEIGHT

	; special flag ?
	bpl	@skip_stepheight
	and	#$7F
	sta	THMOVE_STEP_HEIGHT

	; on floor ?
	lda	ARG_THING_CHECK_ONFLOOR
	bne	@skip_stepheight

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; check sector link
	ldx	ARG_THING_CHECK_SECTOR
	ldy	SECTOR_STRUCT_FLOOR_LINK,x
	beq	@halve_step

	; other sector water flag
	lda	SECTOR_STRUCT_FLAGS,y
	bpl	@halve_step

	; check actual floor height
	sec
	lda	ARG_THING_CHECK_Z_L
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	lda	ARG_THING_CHECK_Z_H
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	bpl	@halve_step

	; double step height
	asl	THMOVE_STEP_HEIGHT
	bra	@skip_stepheight

@halve_step:
	; halve step height
	lsr	THMOVE_STEP_HEIGHT

@skip_stepheight:
	; reset fail safe counter
	lda	#4
	sta	THMOVE_FAIL_SAFE

	; set overlap flag
	lda	#THING_IFLAG_HEIGHTCHECK
	sta	THMOVE_IFLAGS

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; portals[0].sector = sdx
	lda	ARG_THING_CHECK_SECTOR
	sta	MEM_PORTALS_SECTOR

	; portals[0].islink = 0
	lda	#0
	sta	MEM_PORTALS_ISLINK

	; portals[0].touch = 0
	sta	MEM_PORTALS_TOUCH

@fail_safe:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; sec->maskblock = 0
	ldx	ARG_THING_CHECK_SECTOR
	stz	SECTOR_STRUCT_MASKBLOCK,x

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; reset heights
	stz	THMOVE_FLOORZ_L
	stz	THMOVE_WATERZ_L
	lda	#$C0
	sta	THMOVE_FLOORZ_H
	sta	THMOVE_WATERZ_H
	stz	THMOVE_CEILZ_L
	lda	#$40
	sta	THMOVE_CEILZ_H

	; poscheck.sector = 0
	stz	RET_THING_CHECK_NEWSEC

	; poscheck.blocked = 0
	stz	THMOVE_BLOCKED

	; portal_rd = 0
	stz	THMOVE_PORTAL_RD

	; portal_wr = 1
	lda	#1
	sta	THMOVE_PORTAL_WR

@loop_check_sector:
	; while(portal_rd < portal_wr)
	ldy	THMOVE_PORTAL_RD
	cpy	THMOVE_PORTAL_WR
	bcc	@do_check

	; check for rare 'not in any sector' case
	lda	RET_THING_CHECK_NEWSEC
	bne	@no_fail_safe

	jsr	handle_failsafe
	bcc	@no_fail_safe
	bra	@fail_safe

@no_fail_safe:
	;; check things in all added sectors

	; the loop
	ldx	#0
@loop_things:
	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; get sector from portal
	lda	MEM_PORTALS_SECTOR,x
	sta	THMOVE_SDX

	; check things in this sector
	phx
	jsr	check_sector_things
	plx
	bcs	:+

	; blocked
	stz	RET_THING_CHECK_NEWSEC
	bra	:++
:
	; next
	inx
	cpx	THMOVE_PORTAL_WR
	bne	@loop_things
:
	;; use updated height of origin sector

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; poscheck.maskblock = sec->maskblock
	ldx	ARG_THING_CHECK_SECTOR
	lda	SECTOR_STRUCT_MASKBLOCK,x
	bpl	:+
	sta	THMOVE_MASKBLOCK

	; update heights
	jsr	update_sector_heights
:
	;; fix and check height

	; poscheck.ceilingz -= height
	sec
	lda	THMOVE_CEILZ_L
	sbc	THMOVE_HEIGHT
	sta	THMOVE_CEILZ_L
	bcs	:+
	dec	THMOVE_CEILZ_H
:
	; if(poscheck.ceilingz < poscheck.floorz)
	sec
	lda	THMOVE_CEILZ_L
	sbc	THMOVE_FLOORZ_L
	lda	THMOVE_CEILZ_H
	sbc	THMOVE_FLOORZ_H
	bmi	@do_block_extra

	; if(poscheck.floorz <= poscheck.waterz)
	sec
	lda	THMOVE_WATERZ_L
	sbc	THMOVE_FLOORZ_L
	lda	THMOVE_WATERZ_H
	sbc	THMOVE_FLOORZ_H
	bmi	:+

	; poscheck.iflags |= THING_IFLAG_NOJUMP
	lda	#THING_IFLAG_NOJUMP
	tsb	THMOVE_IFLAGS
:
	; done
	ldy	THMOVE_THING
	rts

@do_block_extra:
	; poscheck.blocked = -1
	lda	#$FF
	sta	THMOVE_BLOCKED

	; done
	ldy	THMOVE_THING
	rts

@do_check:
	; inside = 1
	lda	#1
	sta	THMOVE_INSIDE

	; sdx = portals[portal_rd].sector
	ldx	MEM_PORTALS_SECTOR,y
	stx	THMOVE_SDX

	; walls = portals[portal_rd].islink
	lda	MEM_PORTALS_ISLINK,y
	sta	THMOVE_ISLINK

	; if(!islink)
	lda	THMOVE_ISLINK
	beq	:+
	jmp	@not_blocked
:
	; touch = 0xC0
	lda	#$C0
	sta	THMOVE_TOUCH

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	;; walls
	lda	SECTOR_STRUCT_WALLS_L,x
	sta	WALN_PTR_L
	lda	SECTOR_STRUCT_WALLS_H,x
	sta	WALN_PTR_H

	; get wall bank
	rol
	rol
	rol
	rol
	and	#$03
	ora	#BANK_MAPDATA
	sta	WALL_PTR_B

	; get wall pointer
	lda	WALN_PTR_H
	and	#$1F
	ora	#$A0
	sta	WALN_PTR_H

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

@loop_wall:
	; wall pointer
	lda	WALN_PTR_L
	sta	WALL_PTR_L
	lda	WALN_PTR_H
	sta	WALL_PTR_H

	; next wall pointer
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	tax
	lda	TAB_SWAP,x
	and	#7
	tax
	clc
	lda	WALL_PTR_L
	adc	wall_sizes,x
	sta	WALN_PTR_L
	lda	WALL_PTR_H
	adc	#0
	sta	WALN_PTR_H

	; d0.x = v0->x - x
	sec
	ldy	#WALL_STRUCT_VTX_X
	lda	(WALL_PTR_L),y
	sta	THMOVE_VTX_X_L
	sbc	ARG_THING_CHECK_X_L
	sta	THMOVE_D0_X_L
	iny
	lda	(WALL_PTR_L),y
	sta	THMOVE_VTX_X_H
	sbc	ARG_THING_CHECK_X_H
	sta	THMOVE_D0_X_H

	; d0.y = v0->y - y
	sec
	iny	; ldy	#WALL_STRUCT_VTX_Y
	lda	(WALL_PTR_L),y
	sta	THMOVE_VTX_Y_L
	sbc	ARG_THING_CHECK_Y_L
	sta	THMOVE_D0_Y_L
	iny
	lda	(WALL_PTR_L),y
	sta	THMOVE_VTX_Y_H
	sbc	ARG_THING_CHECK_Y_H
	sta	THMOVE_D0_Y_H

	; dist = (d0.x * wall->dist.y - d0.y * wall->dist.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.x
	lda	THMOVE_D0_X_L
	sta	VERA_FX_CACHE_L
	lda	THMOVE_D0_X_H
	sta	VERA_FX_CACHE_M

	; wall->dist.y
	ldy	#WALL_STRUCT_DIST_Y
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_H
	iny
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; -d0.y
	sec
	lda	#$00
	sbc	THMOVE_D0_Y_L
	sta	VERA_FX_CACHE_L
	lda	#$00
	sbc	THMOVE_D0_Y_H
	sta	VERA_FX_CACHE_M

	; wall->dist.x
	ldy	#WALL_STRUCT_DIST_X
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_H
	iny
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	clc
	lda	VERA_DATA0
	sta	THMOVE_DIST_L
	adc	THMOVE_RADIUS
	lda	VERA_DATA0
	sta	THMOVE_DIST_H
	adc	#0
	sta	THMOVE_TOUCH_SAVE

	; various distance checks
	lda	THMOVE_DIST_H
	bmi	:+
	bne	@skip_checks_ab
	lda	THMOVE_DIST_L
	cmp	THMOVE_RADIUS
	bcs	@skip_checks_ab
	bra	@inside_skip
:
	; inside = 0
	stz	THMOVE_INSIDE

	; if(wall->angle & MARK_PORTAL)
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_PORTAL
	beq	@inside_skip

	; back sector
	ldy	#WALL_STRUCT_BACKSECTOR
	lda	(WALL_PTR_L),y
	beq	@inside_skip
	sta	THMOVE_FALLBACK

@inside_skip:
	; poscheck.maskblock = 0
	stz	THMOVE_MASKBLOCK

	; if(dist + poscheck.radius < 0)
	lda	THMOVE_TOUCH_SAVE
	bpl	:+

	; touch = 0
	stz	THMOVE_TOUCH

	bra	:++
:
	; if((wall->angle & MARK_MID_BITS) == MARK_MASKED)
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_MID_BITS
	cmp	#WALL__MARK_MASKED
	bne	:+

	; if(wall->blockmid & 0x80)
	ldy	#WALL_STRUCT_TEX_MID_B
	lda	(WALL_PTR_L),y
	bpl	:+

	; if(wall->blockmid & poscheck.blockedby & 0x7F)
	ora	THMOVE_BLOCKEDBY
	and	#$7F
	beq	:+

	; poscheck.maskblock = 0x80
	lda	#$80
	sta	THMOVE_MASKBLOCK
:
	; if(!poscheck.blocked)
	lda	THMOVE_BLOCKED
	bne	:+

	; check backsector
	jsr	check_backsector
:
	jmp	@do_checks

@skip_checks:
	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

@skip_checks_ab:
	; check for STOP
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	bmi	:+
	jmp	@loop_wall
:
	; if(poscheck.blocked)
	lda	THMOVE_BLOCKED
	bne	@is_blocked

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; link-touch
	lda	#$80
	sta	THMOVE_TOUCH_SAVE

	; if(sec->floor.link)
	ldy	THMOVE_SDX
	ldx	SECTOR_STRUCT_FLOOR_LINK,y
	beq	:++
	lda	#$01
	sta	THMOVE_ISLINK_SAVE

	; if(portal_wr < 16)
	lda	THMOVE_PORTAL_WR
	cmp	#16
	bcc	:+
	jmp	@do_block_extra
:
	stz	SECTOR_STRUCT_MASKBLOCK,x
	jsr	add_sector

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK
:
	; if(sec->ceiling.link)
	ldy	THMOVE_SDX
	ldx	SECTOR_STRUCT_CEILING_LINK,y
	beq	:++
	lda	#$80
	sta	THMOVE_ISLINK_SAVE

	; if(portal_wr < 16)
	lda	THMOVE_PORTAL_WR
	cmp	#16
	bcc	:+
	jmp	@do_block_extra
:
	stz	SECTOR_STRUCT_MASKBLOCK,x
	jsr	add_sector
:
@not_blocked:
	; sector heights
	jsr	update_sector_heights_masked

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; portal_rd++
	inc	THMOVE_PORTAL_RD

	; if(!poscheck.sector && inside && !islink)
	lda	RET_THING_CHECK_NEWSEC
	bne	:+
	lda	THMOVE_INSIDE
	beq	:+
	lda	THMOVE_ISLINK
	bne	:+

	; poscheck.sector = sdx
	lda	THMOVE_SDX
	sta	RET_THING_CHECK_NEWSEC
:
	; next
	jmp	@loop_check_sector

@is_blocked:
	; poscheck.sector = 0
	stz	RET_THING_CHECK_NEWSEC

	; done
	ldy	THMOVE_THING
	rts

@check_vtx:
	cmp	THMOVE_RADIUS
	bcs	@skip_checks
	lda	THMOVE_BLOCKED
	bne	@skip_checks

	; p2a_coord.x = (vtx->x << 8) - th->x
	sec
	lda	THMOVE_VTX_X_L
	sbc	THMOVE_START_X_L
	sta	MATH_P2A_X_L
	lda	THMOVE_VTX_X_H
	sbc	THMOVE_START_X_H
	sta	MATH_P2A_X_H

	; p2a_coord.y = (vtx->y << 8) - th->y
	sec
	lda	THMOVE_VTX_Y_L
	sbc	THMOVE_START_Y_L
	sta	MATH_P2A_Y_L
	lda	THMOVE_VTX_Y_H
	sbc	THMOVE_START_Y_H
	sta	MATH_P2A_Y_H

	; point_to_angle() >> 4
	jsr	math_p2a
	math_lsr4_u16	MATH_VAR_L, MATH_VAR_H

	; ang += 0x40
	clc
	adc	#$40

	; poscheck.angle = ang
	sta	THMOVE_BVECT_ANG

	; ang += 0x80
	eor	#$80

	; poscheck.vect.x = tab_sin[ang]
	tax
	math_read_sin	THMOVE_BVECT_X_L, THMOVE_BVECT_X_H

	; poscheck.vect.y = tab_cos[ang]
	math_read_cos	THMOVE_BVECT_Y_L, THMOVE_BVECT_Y_H

	; dist = (d0.y * poscheck.vect.x - d0.x * poscheck.vect.y) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.y
	lda	THMOVE_D0_Y_L
	sta	VERA_FX_CACHE_L
	lda	THMOVE_D0_Y_H
	sta	VERA_FX_CACHE_M

	; poscheck.vect.x
	lda	THMOVE_BVECT_X_L
	sta	VERA_FX_CACHE_H
	lda	THMOVE_BVECT_X_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; -d0.x
	sec
	lda	#0
	sbc	THMOVE_D0_X_L
	sta	VERA_FX_CACHE_L
	lda	#0
	sbc	THMOVE_D0_X_H
	sta	VERA_FX_CACHE_M

	; poscheck.vect.y
	lda	THMOVE_BVECT_Y_L
	sta	VERA_FX_CACHE_H
	lda	THMOVE_BVECT_Y_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	THMOVE_DIST_L
	lda	VERA_DATA0

	; if(-dist < radius)
	bpl	:+
	lda	THMOVE_DIST_L
	eor	#$FF
	ina
	cmp	THMOVE_RADIUS
	bcc	:+
	jmp	@skip_checks
:
	lda	THMOVE_BACKBLOCK
	beq	@skip_block

	; poscheck.blocked = 1
	lda	#1
	sta	THMOVE_BLOCKED

	jmp	@skip_checks

@skip_block:
	ldx	THMOVE_BACKSECTOR
	stz	THMOVE_ISLINK_SAVE
	lda	#$C0
	sta	THMOVE_TOUCH_SAVE
	jsr	add_sector
	jmp	@skip_checks

@do_checks:
	; dist = (d0.x * wall->dist.x + d0.y * wall->dist.y) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.x
	lda	THMOVE_D0_X_L
	sta	VERA_FX_CACHE_L
	lda	THMOVE_D0_X_H
	sta	VERA_FX_CACHE_M

	; wall->dist.x is already set

	; accumulate
	lda	VERA_FX_ACCUM

	; d0.y
	lda	THMOVE_D0_Y_L
	sta	VERA_FX_CACHE_L
	lda	THMOVE_D0_Y_H
	sta	VERA_FX_CACHE_M

	; wall->dist.y
	ldy	#WALL_STRUCT_DIST_Y
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_H
	iny
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	tax
	lda	VERA_DATA0

	; if(dist < 0)
	bpl	:+++

	; if(dist >= -255)
	beq	:++
:
	jmp	@skip_checks
:
	; invert distance
	txa
	beq	:--
	eor	#$FF
	ina
	jmp	@check_vtx
:
	; dist = (d1.y * wall->dist.y + d1.x * wall->dist.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d1.y
	sec
	ldy	#WALL_STRUCT_VTX_Y
	lda	(WALN_PTR_L),y
	sta	THMOVE_VTX_Y_L
	sbc	ARG_THING_CHECK_Y_L
	sta	THMOVE_D0_Y_L
	sta	VERA_FX_CACHE_L
	iny
	lda	(WALN_PTR_L),y
	sta	THMOVE_VTX_Y_H
	sbc	ARG_THING_CHECK_Y_H
	sta	THMOVE_D0_Y_H
	sta	VERA_FX_CACHE_M

	; wall->dist.y is already set

	; accumulate
	lda	VERA_FX_ACCUM

	; d1.x
	sec
	ldy	#WALL_STRUCT_VTX_X
	lda	(WALN_PTR_L),y
	sta	THMOVE_VTX_X_L
	sbc	ARG_THING_CHECK_X_L
	sta	THMOVE_D0_X_L
	sta	VERA_FX_CACHE_L
	iny
	lda	(WALN_PTR_L),y
	sta	THMOVE_VTX_X_H
	sbc	ARG_THING_CHECK_X_H
	sta	THMOVE_D0_X_H
	sta	VERA_FX_CACHE_M

	; wall->dist.x
	ldy	#WALL_STRUCT_DIST_X
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_H
	iny
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	tax
	lda	VERA_DATA0

	; if(dist >= 0)
	bmi	:++
	beq	:+
	jmp	@skip_checks
:
	txa
	jmp	@check_vtx
:
	; if(!poscheck.blocked)
	lda	THMOVE_BLOCKED
	beq	:+

	; check backsector
	jsr	check_backsector
:
	; if(!back_block && wall->backsector && portal_wr < 16)
	lda	THMOVE_BACKBLOCK
	bne	@do_block
	lda	THMOVE_PORTAL_WR
	cmp	#16
	bcs	@do_block

	ldx	THMOVE_BACKSECTOR
	stz	THMOVE_ISLINK_SAVE
	lda	THMOVE_TOUCH
	sta	THMOVE_TOUCH_SAVE
	jsr	add_sector

	jmp	@skip_checks

@do_block:
	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; poscheck.blocked = 1
	lda	#1
	sta	THMOVE_BLOCKED

	; poscheck.sector = 0
	stz	RET_THING_CHECK_NEWSEC

	; poscheck.angle = wall->angle >> 4
	ldy	#WALL_STRUCT_ANGLE
	lda	(WALL_PTR_L),y
	tax
	lda	TAB_SWAP,x
	and	#$0F
	sta	THMOVE_BVECT_ANG
	iny
	lda	(WALL_PTR_L),y
	tax
	lda	TAB_SWAP,x
	and	#$F0
	tsb	THMOVE_BVECT_ANG

	; if(inside)
	lda	THMOVE_INSIDE
	bne	:+

	; poscheck.blocked = 0x41
	lda	#$41
	sta	THMOVE_BLOCKED
:
	; poscheck.vect = wall->dist
	ldy	#WALL_STRUCT_DIST_X
	lda	(WALL_PTR_L),y
	sta	THMOVE_BVECT_X_L
	iny
	lda	(WALL_PTR_L),y
	sta	THMOVE_BVECT_X_H
	iny
	lda	(WALL_PTR_L),y
	sta	THMOVE_BVECT_Y_L
	iny
	lda	(WALL_PTR_L),y
	sta	THMOVE_BVECT_Y_H

	; done
	ldy	THMOVE_THING
	rts

;;;
; sector heights
;;;
; extension of 'thing_check_position' and 'thing_check_heights'

update_sector_heights_masked:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; poscheck.maskblock = map_secext[sdx].maskblock
	ldx	THMOVE_SDX
	lda	SECTOR_STRUCT_MASKBLOCK,x
	sta	THMOVE_MASKBLOCK

update_sector_heights:
	; get floorZ
	jsr	get_sector_floorz

	; if(poscheck.tfz > poscheck.floorz)
	sec
	lda	THMOVE_FLOORZ_L
	sbc	THMOVE_TFZ_L
	lda	THMOVE_FLOORZ_H
	sbc	THMOVE_TFZ_H
	bpl	:+

	; poscheck.floorz = poscheck.tfz
	lda	THMOVE_TFZ_L
	sta	THMOVE_FLOORZ_L
	lda	THMOVE_TFZ_H
	sta	THMOVE_FLOORZ_H

	; poscheck.floors = poscheck.maskblock ? 0 : sdx
	stz	THMOVE_FLOORS
	lda	THMOVE_MASKBLOCK
	bne	:+
	stx	THMOVE_FLOORS
:
	; get ceilingZ
	jsr	get_sector_ceilingz

	; if(poscheck.tcz < poscheck.ceilingz)
	sec
	lda	THMOVE_TCZ_L
	sbc	THMOVE_CEILZ_L
	lda	THMOVE_TCZ_H
	sbc	THMOVE_CEILZ_H
	bpl	:+

	; poscheck.ceilingz = poscheck.tcz
	lda	THMOVE_TCZ_L
	sta	THMOVE_CEILZ_L
	lda	THMOVE_TCZ_H
	sta	THMOVE_CEILZ_H

	; poscheck.ceilings = sdx
	stx	THMOVE_CEILS
:
	rts

;;;
; prepare position variables
;;;
; extension of 'thing_check_position' and 'thing_check_heights'

prepare_pos_check:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; read eflags
	lda	THING_STRUCT_EFLAGS,y
	sta	THMOVE_EFLAGS

	; read radius
	lda	THING_STRUCT_RADIUS,y
	sta	THMOVE_RADIUS

	; read height
	lda	THING_STRUCT_HEIGHT,y
	sta	THMOVE_HEIGHT

	; read blocked by
	lda	THING_STRUCT_BLOCKEDBY,y
	sta	THMOVE_BLOCKEDBY

	; th_zt = th_zb + height
	clc
	lda	ARG_THING_CHECK_Z_L
	adc	THMOVE_HEIGHT
	sta	THMOVE_THZT_L
	lda	ARG_THING_CHECK_Z_H
	adc	#0
	sta	THMOVE_THZT_H

	; thing type
	ldx	THING_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; read water height
	lda	THTYPE_STRUCT_WATER_HEIGHT,x
	sta	THMOVE_WATER_HEIGHT

	; read step height
	lda	THTYPE_STRUCT_STEP_HEIGHT,x

	rts

;;;
; check back sector
;;;
; extension of 'thing_check_position'

check_backsector:
	; back_block = 1
	lda	#1
	sta	THMOVE_BACKBLOCK

	; if(wall->angle & MARK_PORTAL)
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_PORTAL
	beq	@do_block

	; back sector
	ldy	#WALL_STRUCT_BACKSECTOR
	lda	(WALL_PTR_L),y
	beq	@do_block
	sta	THMOVE_BACKSECTOR
	tax

	; blocking
	iny
	lda	(WALL_PTR_L),y
	and	THMOVE_BLOCKEDBY
	and	#$7F
	bne	@do_block

	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; check thing count in this sector
	lda	$BF00,x
	cmp	#31
	bcs	@do_block_bank

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; bs->maskblock = poscheck.maskblock
	lda	THMOVE_MASKBLOCK
	sta	SECTOR_STRUCT_MASKBLOCK,x

	; get heights
	jsr	get_sector_floorz
	jsr	get_sector_ceilingz

	; dist = poscheck.tcz - poscheck.tfz
	sec
	lda	THMOVE_TCZ_L
	sbc	THMOVE_TFZ_L
	sta	THMOVE_DIST_L
	lda	THMOVE_TCZ_H
	sbc	THMOVE_TFZ_H
	sta	THMOVE_DIST_H

	; dist -= height
	sec
	lda	THMOVE_DIST_L
	sbc	THMOVE_HEIGHT
	sta	THMOVE_DIST_L
	bcs	:+
	dec	THMOVE_DIST_H
:
	lda	THMOVE_DIST_H
	bmi	@do_block_bank

	; dist = th_zb - poscheck.tfz
	sec
	lda	ARG_THING_CHECK_Z_L
	sbc	THMOVE_TFZ_L
	sta	THMOVE_DIST_L
	lda	ARG_THING_CHECK_Z_H
	sbc	THMOVE_TFZ_H
	sta	THMOVE_DIST_H

	; dist += step_up
	clc
	lda	THMOVE_DIST_L
	adc	THMOVE_STEP_HEIGHT
	sta	THMOVE_DIST_L
	lda	THMOVE_DIST_H
	adc	#0
	sta	THMOVE_DIST_H

	bmi	@do_block_bank

	; if(poscheck.tcz - th_zt < 0)
	sec
	lda	THMOVE_TCZ_L
	sbc	THMOVE_THZT_L
	lda	THMOVE_TCZ_H
	sbc	THMOVE_THZT_H
	bmi	@do_block_bank

	; back_block = 0
	stz	THMOVE_BACKBLOCK

@do_block_bank:
	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

@do_block:
	rts

;;;
; add sector
;;;
; extension of 'thing_check_position'
; X is sector
; THMOVE_ISLINK is special flag

add_sector:
	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; check existing portals for duplicates
	ldy	#0
@loop:
	; if(portals[i].sector == sdx)
	txa
	cmp	MEM_PORTALS_SECTOR,y
	bne	:+

	; portals[i].touch |= touch
	lda	MEM_PORTALS_TOUCH,y
	ora	THMOVE_TOUCH_SAVE
	sta	MEM_PORTALS_TOUCH,y

	; done
	rts
:
	; next
	iny
	cpy	THMOVE_PORTAL_WR
	bne	@loop

	; portals[portal_wr].sector = sdx
	txa
	sta	MEM_PORTALS_SECTOR,y
	lda	THMOVE_ISLINK_SAVE
	sta	MEM_PORTALS_ISLINK,y
	lda	THMOVE_TOUCH_SAVE
	sta	MEM_PORTALS_TOUCH,y

	; portal_wr++
	inc	THMOVE_PORTAL_WR

	rts

;;;
; get floor / ceiling Z
;;;
; extension of 'thing_check_position'
; X is sector

get_sector_floorz:
	; check double-link
	lda	THMOVE_ISLINK
	bpl	:+

	; dummy height
	stz	THMOVE_TFZ_L
	lda	#$C0
	sta	THMOVE_TFZ_H

	rts
:
	; poscheck.tfz = sec->floor.height
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	THMOVE_TFZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sta	THMOVE_TFZ_H

	lda	THMOVE_MASKBLOCK
	bpl	@skip_masked

	; floor masked
	clc
	lda	THMOVE_TFZ_L
	adc	SECTOR_STRUCT_FLOOR_MASKED,x
	sta	THMOVE_TFZ_L
	bcc	:+
	inc	THMOVE_TFZ_H
:
	; done
	rts

@skip_masked:
	lda	SECTOR_STRUCT_FLOOR_LINK,x
	bne	@do_link

	; floor dist
	lda	SECTOR_STRUCT_FLOOR_DIST,x
	bne	:+

	; done
	rts
:
	; sign bit
	stz	MATH_VAR_H
	bpl	:+
	lda	#$FF
	sta	MATH_VAR_H
:
	; subtract
	sec
	lda	THMOVE_TFZ_L
	sbc	SECTOR_STRUCT_FLOOR_DIST,x
	sta	THMOVE_TFZ_L
	lda	THMOVE_TFZ_H
	sbc	MATH_VAR_H
	sta	THMOVE_TFZ_H

	; done
	rts

@do_link:
	; linked sector
	phx
	tax

	; new height
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	MATH_VAR_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sta	MATH_VAR_H

	; check for water (flag)
	lda	SECTOR_STRUCT_FLAGS,x
	bpl	@no_water

	; special overlap check
	bit	THMOVE_ISLINK
	bvc	:+

	; check if already under water
	sec
	lda	THMOVE_THZW_L
	sbc	THMOVE_TFZ_L
	lda	THMOVE_THZW_H
	sbc	THMOVE_TFZ_H
	bmi	@no_water
:
	; subtract water height from original
	sec
	lda	THMOVE_TFZ_L
	sbc	THMOVE_WATER_HEIGHT
	sta	THMOVE_TFZ_L
	bcs	:+
	dec	THMOVE_TFZ_H
:
	; compare to new height
	sec
	lda	MATH_VAR_L
	sbc	THMOVE_TFZ_L
	lda	MATH_VAR_H
	sbc	THMOVE_TFZ_H
	bpl	@no_water

	; if(fz > poscheck.waterz)
	sec
	lda	THMOVE_WATERZ_L
	sbc	THMOVE_TFZ_L
	lda	THMOVE_WATERZ_H
	sbc	THMOVE_TFZ_H
	bpl	:+

	; poscheck.waterz = fz
	lda	THMOVE_TFZ_L
	sta	THMOVE_WATERZ_L
	lda	THMOVE_TFZ_H
	sta	THMOVE_WATERZ_H
:
	plx
	rts

@no_water:
	lda	MATH_VAR_L
	sta	THMOVE_TFZ_L
	lda	MATH_VAR_H
	sta	THMOVE_TFZ_H

	plx
	rts

get_sector_ceilingz:
	phx
	lda	SECTOR_STRUCT_CEILING_LINK,x
	beq	:+
	tax
:
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sta	THMOVE_TCZ_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sta	THMOVE_TCZ_H

	plx
	rts

;;;
; handle failsafe
;;;
; extension of 'thing_check_position'

handle_failsafe:
	; check counter
	ldx	THMOVE_FAIL_SAFE
	beq	@no_fail_safe
	dex
	stx	THMOVE_FAIL_SAFE

	; check fallback sector
	ldy	THMOVE_FALLBACK
	beq	@do_failsafe

	; check projectile flag
	lda	THMOVE_EFLAGS
	and	#THING_EFLAG_PROJECTILE
	beq	@do_failsafe

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; change first portal
	sty	MEM_PORTALS_SECTOR

	; try again
	sec
	rts

@do_failsafe:
	; pick axis
	txa
	ror
	bcc	:+
	lda	ARG_THING_CHECK_X_L
	eor	#2
	sta	ARG_THING_CHECK_X_L

	; try again
	sec
	rts
:
	lda	ARG_THING_CHECK_Y_L
	eor	#2
	sta	ARG_THING_CHECK_Y_L

	; try again
	sec
	rts

@no_fail_safe:
	; continue
	clc
	rts

;;;
; apply thing position
;;;
; run only after 'thing_check_position' or to remove from all sectors
; Y is current thing
; if RET_THING_CHECK_NEWSEC is zero, thing is only removed from all sectors

thing_apply_position:
	sty	THMOVE_THING
	sty	RET_THING_SPAWN

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; remove from all sectors
	stz	TMP_PTR_L
	lda	#$A0
	sta	HI_PTR_H	; thingsec
	sta	THCES_PTR_H	; thingces
	lda	#$80
	sta	THCES_PTR_L
	ldx	#16
@loop_del:
	; get sector at this slot
	lda	(HI_PTR_L),y

	; check for NONE
	beq	@end

	; save sector at this slot
	sta	TMP_PTR_I

	; remove sector from thing
	lda	#0
	sta	(HI_PTR_L),y

	; get slot in sector
	lda	(THCES_PTR_L),y
	and	#$1F
	ora	#$A0
	sta	TMP_PTR_H

	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; clear thing in this sector slot
	lda	#0
	ldy	TMP_PTR_I
	sta	(TMP_PTR_L),y

	; decrement thing coutner for this sector
	lda	$BF00,y
	dea
	sta	$BF00,y

	; restore thing index
	ldy	THMOVE_THING

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; next
	inc	HI_PTR_H
	inc	THCES_PTR_H
	dex
	bne	@loop_del

@end:
	; check for removal-only
	lda	RET_THING_CHECK_NEWSEC
	bne	:+

	; restore
	ldy	THMOVE_THING

	; stop
	rts
:
	;; place to first sector
	sta	ARG_THING_PLACE_SECTOR
	lda	#$A0
	sta	THMOVE_PTR_H
	jsr	place_to_sector

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	;; place to detected sectors
	ldx	#0
@loop_place:
	; check touching flag
	lda	MEM_PORTALS_TOUCH,x
	bpl	:+

	; get sector from portal
	lda	MEM_PORTALS_SECTOR,x

	; skip first sector
	cmp	RET_THING_CHECK_NEWSEC
	beq	:+

	; backup
	phx

	; place to this sector
	sta	ARG_THING_PLACE_SECTOR
	jsr	place_to_sector

	; restore
	plx

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK
:
	; next
	inx
	cpx	THMOVE_PORTAL_WR
	bne	@loop_place

	; restore
	ldy	THMOVE_THING

;;;
; apply thing position
;;;
; Y is current thing
; call after 'thing_check_heights'
; falltrough from 'thing_apply_position'

thing_apply_heights:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; th->floorz = poscheck.floorz
	lda	THMOVE_FLOORZ_L
	sta	THING_STRUCT_FLOORZ_L,y
	lda	THMOVE_FLOORZ_H
	sta	THING_STRUCT_FLOORZ_H,y

	; th->ceilingz = poscheck.ceilingz
	lda	THMOVE_CEILZ_L
	sta	THING_STRUCT_CEILZ_L,y
	lda	THMOVE_CEILZ_H
	sta	THING_STRUCT_CEILZ_H,y

	; sectors
	lda	THMOVE_FLOORS
	sta	THING_STRUCT_FLOORS,y
	lda	THMOVE_CEILS
	sta	THING_STRUCT_CEILS,y

	; iflags
	lda	THING_STRUCT_IFLAGS,y
	and	#<~(THING_IFLAG_HEIGHTCHECK|THING_IFLAG_NOJUMP)
	ora	THMOVE_IFLAGS
	sta	THING_STRUCT_IFLAGS,y

	; done
	rts

;;;
; place to sector
;;;
; ARG_THING_PLACE_SECTOR - desired sector
; RET_THING_SPAWN - desired thing
; THMOVE_PTR_H - slot index pointer in thing, auto increment

place_to_sector:
	; check pointer slot
	lda	THMOVE_PTR_H
	cmp	#$B0
	bcc	:+

	; all thing slots are full
	rts
:
	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; find free slot in this sector
	lda	#$A0
	sta	HI_PTR_H
	ldy	ARG_THING_PLACE_SECTOR
@loop:
	lda	(HI_PTR_L),y
	bne	@not_free

	; add thing to this slot
	lda	RET_THING_SPAWN
	sta	(HI_PTR_L),y

	; increment thing counter for this sector
	lda	$BF00,y
	ina
	sta	$BF00,y

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; maskblock = sec->maskblock
	lda	SECTOR_STRUCT_MASKBLOCK,y
	sta	THMOVE_MASKBLOCK

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing pointer
	tya
	ldy	RET_THING_SPAWN

	; add this sector to thing
	sta	(THMOVE_PTR_L),y

	; add slot index to thing
	lda	THMOVE_PTR_H
	sta	@opcode+2
	lda	HI_PTR_H
	and	#$1F
	ora	THMOVE_MASKBLOCK
@opcode:
	sta	$A080,y	; modified address

	; increment thing slot index
	inc	THMOVE_PTR_H

	; done
	rts

@not_free:
	; next
	inc	HI_PTR_H
	lda	HI_PTR_H
	cmp	#$BF
	bne	@loop

	; out of slots
	rts

;;;
; check thing collisions
;;;
; called from 'thing_check_position' and 'thing_check_heights'
; THMOVE_THING is the thing
; THMOVE_SDX is sector
; THMOVE_RADIUS is thing radius
; THMOVE_HEIGHT is thing height
; THMOVE_STEP_HEIGHT is thing step height
; ARG_THING_CHECK_Z_* is thing Z
; THMOVE_THZT_* is thing Z + height
; returns 'carry set' if not blocked

check_sector_things:
	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; sector
	ldy	THMOVE_SDX

	; the loop
	lda	#$A0
	sta	THTEMP_PTR_H
	stz	THTEMP_PTR_L
@loop_sec_thing:
	; get thing at this slot
	lda	(THTEMP_PTR_L),y

	; empty slot check
	beq	@skip_thing

	; same thing check
	cmp	THMOVE_THING
	beq	@skip_thing

	; save other thing
	tax

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; blocking flags
	lda	THING_STRUCT_BLOCKING,x
	and	THMOVE_BLOCKEDBY
	beq	@skip_check

	; origin check
	lda	THING_STRUCT_TH_ORIGIN,x
	cmp	THMOVE_THING
	beq	@skip_check

	; origin check
	ldy	THMOVE_THING
	txa
	cmp	THING_STRUCT_TH_ORIGIN,y
	beq	@skip_check

	; check this thing
	jsr	check_thing
	bcs	:+

	; blocked
	rts
:
	ldy	THMOVE_SDX

@skip_check:
	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

@skip_thing:
	inc	THTEMP_PTR_H
	lda	THTEMP_PTR_H
	cmp	#$BF
	bne	@loop_sec_thing

	; done
	sec
	rts

;;;
; check thing blocking
;;;
; extension of 'check_sector_things'

check_thing:
	stx	THMOVE_OTHER_THING
	ldy	THMOVE_THING

	; d0.x = (tho->x >> 8) - x
	sec
	lda	THING_STRUCT_X_L,x
	sbc	ARG_THING_CHECK_X_L
	sta	THMOVE_D0_X_L
	lda	THING_STRUCT_X_H,x
	sbc	ARG_THING_CHECK_X_H
	sta	THMOVE_D0_X_H

	; d0.y = (tho->y >> 8) - y
	sec
	lda	THING_STRUCT_Y_L,x
	sbc	ARG_THING_CHECK_Y_L
	sta	THMOVE_D0_Y_L
	lda	THING_STRUCT_Y_H,x
	sbc	ARG_THING_CHECK_Y_H
	sta	THMOVE_D0_Y_H

	; p2a_coord.x = (tho->x >> 8) - (th->x >> 8)
	sec
	lda	THING_STRUCT_X_L,x
	sbc	THING_STRUCT_X_L,y
	sta	MATH_P2A_X_L
	lda	THING_STRUCT_X_H,x
	sbc	THING_STRUCT_X_H,y
	sta	MATH_P2A_X_H

	; p2a_coord.y = (tho->y >> 8) - (th->y >> 8);
	sec
	lda	THING_STRUCT_Y_L,x
	sbc	THING_STRUCT_Y_L,y
	sta	MATH_P2A_Y_L
	lda	THING_STRUCT_Y_H,x
	sbc	THING_STRUCT_Y_H,y
	sta	MATH_P2A_Y_H

	; combined radius
	stz	THMOVE_RADRAD_H
	clc
	lda	THING_STRUCT_RADIUS,x
	adc	THMOVE_RADIUS
	sta	THMOVE_RADRAD_L
	rol	THMOVE_RADRAD_H

	; point_to_angle() >> 4
	jsr	math_p2a
	math_lsr4_u16	MATH_VAR_L, MATH_VAR_H

	; ang += 0x40
	clc
	adc	#$40

	; poscheck.angle = ang
	sta	THMOVE_BVECT_ANG

	; ang += 0x80
	eor	#$80

	; poscheck.vect.x = tab_sin[ang]
	tax
	math_read_sin	THMOVE_BVECT_X_L, THMOVE_BVECT_X_H

	; poscheck.vect.y = tab_cos[ang]
	math_read_cos	THMOVE_BVECT_Y_L, THMOVE_BVECT_Y_H

	; dist = (d0.x * poscheck.vect.y - d0.y * poscheck.vect.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.x
	lda	THMOVE_D0_X_L
	sta	VERA_FX_CACHE_L
	lda	THMOVE_D0_X_H
	sta	VERA_FX_CACHE_M

	; poscheck.vect.y
	lda	THMOVE_BVECT_Y_L
	sta	VERA_FX_CACHE_H
	lda	THMOVE_BVECT_Y_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; -d0.y
	sec
	lda	#0
	sbc	THMOVE_D0_Y_L
	sta	VERA_FX_CACHE_L
	lda	#0
	sbc	THMOVE_D0_Y_H
	sta	VERA_FX_CACHE_M

	; poscheck.vect.x
	lda	THMOVE_BVECT_X_L
	sta	VERA_FX_CACHE_H
	lda	THMOVE_BVECT_X_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; (result >> 8) - radrad
	sec
	lda	VERA_DATA0
	sbc	THMOVE_RADRAD_L
	lda	VERA_DATA0
	sbc	THMOVE_RADRAD_H

	; if(dist >= 0)
	bmi	:+

	; not blocked
	sec
	rts
:
	; overlap detected, check Z

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; other thing
	ldx	THMOVE_OTHER_THING

	; zzz = tho->z >> 8
	lda	THING_STRUCT_Z_L,x
	sta	THMOVE_ZZZ_L
	lda	THING_STRUCT_Z_H,x
	sta	THMOVE_ZZZ_H

	; if(zzz - thzt >= 0)
	sec
	lda	THMOVE_ZZZ_L
	sbc	THMOVE_THZT_L
	lda	THMOVE_ZZZ_H
	sbc	THMOVE_THZT_H
	bmi	@check_over

	; check projectile flag
	lda	THMOVE_EFLAGS
	and	#THING_EFLAG_PROJECTILE
	bne	:+

	; if(zzz < poscheck.ceilingz)
	sec
	lda	THMOVE_ZZZ_L
	sbc	THMOVE_CEILZ_L
	lda	THMOVE_ZZZ_H
	sbc	THMOVE_CEILZ_H
	bpl	:+

	; poscheck.ceilingz = zzz
	lda	THMOVE_ZZZ_L
	sta	THMOVE_CEILZ_L
	lda	THMOVE_ZZZ_H
	sta	THMOVE_CEILZ_H

	; poscheck.ceilings = 0
	stz	THMOVE_CEILS
:
@overlap_ok:
	; set overlap flag
	lda	#THING_IFLAG_HEIGHTCHECK
	sta	THMOVE_IFLAGS

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; set overlap of other thing
	lda	THING_STRUCT_IFLAGS,x
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,x

	; not blocked
	sec
	rts

@check_over:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; zzz += tho->height
	clc
	lda	THMOVE_ZZZ_L
	adc	THING_STRUCT_HEIGHT,x
	sta	THMOVE_ZZZ_L
	lda	THMOVE_ZZZ_H
	adc	#0
	sta	THMOVE_ZZZ_H

	; temp = thzb - zzz
	sec
	lda	ARG_THING_CHECK_Z_L
	sbc	THMOVE_ZZZ_L
	sta	THMOVE_TMP_L
	lda	ARG_THING_CHECK_Z_H
	sbc	THMOVE_ZZZ_H
	sta	THMOVE_TMP_H

	; check climbable flag
	bit	THING_STRUCT_EFLAGS,x
	lda	THMOVE_TMP_H
	bvc	:+

	; temp += step_up
	clc
	lda	THMOVE_TMP_L
	adc	THMOVE_STEP_HEIGHT
	lda	THMOVE_TMP_H
	adc	#0
:
	; if(temp >= 0)
	bmi	@is_blocked

	; check projectile flag
	lda	THMOVE_EFLAGS
	and	#THING_EFLAG_PROJECTILE
	bne	:+

	; if(zzz > poscheck.floorz)
	sec
	lda	THMOVE_FLOORZ_L
	sbc	THMOVE_ZZZ_L
	lda	THMOVE_FLOORZ_H
	sbc	THMOVE_ZZZ_H
	bpl	:+

	; poscheck.floorz = zzz
	lda	THMOVE_ZZZ_L
	sta	THMOVE_FLOORZ_L
	lda	THMOVE_ZZZ_H
	sta	THMOVE_FLOORZ_H

	; poscheck.floors = 0
	stz	THMOVE_FLOORS
:
	; not blocked
	bra	@overlap_ok

@is_blocked:
	;; pushable things

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; check pushable flag
	lda	THING_STRUCT_EFLAGS,x
	lsr
	bcc	@not_pushable

	; get type
	lda	THING_STRUCT_TYPE,x
	tax

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; mass
	lda	THTYPE_STRUCT_MASS,x
	beq	@not_pushable

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; things
	ldy	THMOVE_THING
	ldx	THMOVE_OTHER_THING

	; tho->mx += th->mx
	clc
	lda	THING_STRUCT_MX_S,x
	adc	THING_STRUCT_MX_S,y
	sta	THING_STRUCT_MX_S,x
	lda	THING_STRUCT_MX_L,x
	adc	THING_STRUCT_MX_L,y
	sta	THING_STRUCT_MX_L,x

	; tho->my += th->my
	clc
	lda	THING_STRUCT_MY_S,x
	adc	THING_STRUCT_MY_S,y
	sta	THING_STRUCT_MY_S,x
	lda	THING_STRUCT_MY_L,x
	adc	THING_STRUCT_MY_L,y
	sta	THING_STRUCT_MY_L,x

@not_pushable:
	; poscheck.blocked = 2
	lda	#2
	sta	THMOVE_BLOCKED
	lda	THMOVE_OTHER_THING
	sta	THMOVE_HIT_THING

	; blocked
	clc
	rts

;;;
; projectile death
;;;

projectile_death:
	; get thing height
	lda	THING_STRUCT_HEIGHT,y
	sta	THMOVE_HEIGHT

	; unblock
	lda	#0
	sta	THING_STRUCT_BLOCKING,y
	sta	THING_STRUCT_BLOCKEDBY,y

	; thing type
	ldx	THING_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; new animation
	lda	THTYPE_ANIM_DEATH_L,x
	pha	; <next frame LO>
	lda	THTYPE_ANIM_DEATH_H,x
	pha	; <next frame HI>

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; unflag
	lda	THING_STRUCT_EFLAGS,y
	and	#~THING_EFLAG_PROJECTILE & $FF
	sta	THING_STRUCT_EFLAGS,y

	; set ticks
	lda	#1
	sta	THING_STRUCT_TICKS,y

	; set new frame
	pla	; <next frame HI>
	sta	THING_STRUCT_NXTFRM_H,y
	pla	; <next frame LO>
	sta	THING_STRUCT_NXTFRM_L,y

	; check for NUL frame
	ora	THING_STRUCT_NXTFRM_H,y
	bne	:+

	; thing will be removed - done
	rts
:
	; th->z += th->mz / 2
	clc
	stz	MATH_TEMP_B
	lda	THING_STRUCT_MZ_L,y
	bpl	:+
	sec
	ldx	#$FF
	stx	MATH_TEMP_B
:
	ror
	sta	MATH_TEMP_A
	lda	THING_STRUCT_MZ_S,y
	ror
	clc
	adc	THING_STRUCT_Z_S,y
	sta	THING_STRUCT_Z_S,y
	lda	MATH_TEMP_A
	adc	THING_STRUCT_Z_L,y
	sta	THING_STRUCT_Z_L,y
	lda	MATH_TEMP_B
	adc	THING_STRUCT_Z_H,y
	sta	THING_STRUCT_Z_H,y

	; total stop
	lda	#0
	sta	THING_STRUCT_MX_S,y
	sta	THING_STRUCT_MX_L,y
	sta	THING_STRUCT_MY_S,y
	sta	THING_STRUCT_MY_L,y
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y

	; check wall hit
	lda	THMOVE_BLOCKED
	ror
	bcs	:+
	jmp	@no_wall_hit
:
	; top texture pointer
	clc
	lda	WALL_PTR_L
	adc	#WALL_STRUCT_TEX_TOP
	sta	TMP_PTR_L
	lda	WALL_PTR_H
	adc	#0
	sta	TMP_PTR_H

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; check backsector
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_PORTAL
	bne	:+

	; check split
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_SPLIT
	beq	@no_backsector

	; get split height
	ldy	#WALL_STRUCT_HEIGHT_SPLIT
	lda	(WALL_PTR_L),y
	sta	MATH_TEMP_A
	iny
	lda	(WALL_PTR_L),y
	sta	MATH_TEMP_B

	; zz = wall->split - th->height
	sec
	lda	MATH_TEMP_A
	sbc	THMOVE_HEIGHT
	sta	MATH_TEMP_A
	bcs	@do_backsector
	dec	MATH_TEMP_B
	bra	@do_backsector
:
	; get backsector
	ldy	#WALL_STRUCT_BACKSECTOR
	lda	(WALL_PTR_L),y
	beq	@no_backsector
	tax

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; zz = sec->ceiling.height - th->height
	sec
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sbc	THMOVE_HEIGHT
	sta	MATH_TEMP_A
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sbc	#0
	sta	MATH_TEMP_B

@do_backsector:
	; restore
	ldy	TICK_IDX

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; if((th->z >> 8) <= zz)
	sec
	lda	MATH_TEMP_A
	sbc	THING_STRUCT_Z_L,y
	lda	MATH_TEMP_B
	sbc	THING_STRUCT_Z_H,y
	bmi	:+

	; bot texture pointer
	clc
	lda	WALL_PTR_L
	adc	#WALL_STRUCT_TEX_BOT
	sta	TMP_PTR_L
	lda	WALL_PTR_H
	adc	#0
	sta	TMP_PTR_H
:
	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

@no_backsector:
	; restore
	ldy	TICK_IDX

	; check sky texture
	lda	(TMP_PTR_L)
	ina
	bne	@skip_hit_checks

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; remove thing
	lda	#0
	sta	THING_STRUCT_NXTFRM_L,y
	sta	THING_STRUCT_NXTFRM_H,y

	; done
	rts

@no_wall_hit:
	; check floor hit
	lda	THMOVE_BLOCKED
	and	#4
	beq	@no_floor_hit

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; get floor sector
	ldx	THING_STRUCT_FLOORS,y

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; check texture
	lda	SECTOR_STRUCT_FLOOR_TEXTURE,x
	jsr	plane_texture_hit

	bra	@skip_hit_checks

@no_floor_hit:
	; check ceiling hit
	lda	THMOVE_BLOCKED
	and	#8
	beq	@no_ceil_hit

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; get ceiling sector
	ldx	THING_STRUCT_CEILS,y

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; check texture
	lda	SECTOR_STRUCT_CEILING_TEXTURE,x
	jsr	plane_texture_hit

@no_ceil_hit:
@skip_hit_checks:
	; check wall or thing hit
	lda	THMOVE_BLOCKED
	and	#3
	bne	:+

	; done
	rts
:
	;; fix position

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; nx = th->x
	lda	THING_STRUCT_X_S,y
	sta	ARG_THING_CHECK_X_S
	lda	THING_STRUCT_X_L,y
	sta	ARG_THING_CHECK_X_L
	lda	THING_STRUCT_X_H,y
	sta	ARG_THING_CHECK_X_H

	; ny = th->y
	lda	THING_STRUCT_Y_S,y
	sta	ARG_THING_CHECK_Y_S
	lda	THING_STRUCT_Y_L,y
	sta	ARG_THING_CHECK_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	ARG_THING_CHECK_Y_H

	; block check
	lda	THMOVE_BLOCKED
	ror
	bcc	:+

	;; blocked by wall

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; d0.x = poscheck.wall->vtx.x - (nx >> 8)
	sec
	ldy	#WALL_STRUCT_VTX_X
	lda	(WALL_PTR_L),y
	sbc	ARG_THING_CHECK_X_L
	sta	VERA_FX_CACHE_L
	iny
	lda	(WALL_PTR_L),y
	sbc	ARG_THING_CHECK_X_H
	sta	VERA_FX_CACHE_M

	; d0.y = poscheck.wall->vtx.y - (ny >> 8)
	sec
	iny
	lda	(WALL_PTR_L),y
	sbc	ARG_THING_CHECK_Y_L
	pha	; <d0.y LO>
	iny
	lda	(WALL_PTR_L),y
	sbc	ARG_THING_CHECK_Y_H
	pha	; <d0.y HI>

	; restore
	ldy	TICK_IDX

	bra	@block_next

:
	;; blocked by thing

	; other thing
	ldx	THMOVE_HIT_THING

	; d0.x = (things[poscheck.hit_thing].x - nx) >> 8
	sec
	lda	THING_STRUCT_X_L,x
	sbc	ARG_THING_CHECK_X_L
	sta	VERA_FX_CACHE_L
	lda	THING_STRUCT_X_H,x
	sbc	ARG_THING_CHECK_X_H
	sta	VERA_FX_CACHE_M

	; d0.y = (things[poscheck.hit_thing].y - ny) >> 8
	sec
	lda	THING_STRUCT_Y_L,x
	sbc	ARG_THING_CHECK_Y_L
	pha	; <d0.y LO>
	lda	THING_STRUCT_Y_H,x
	sbc	ARG_THING_CHECK_Y_H
	pha	; <d0.y HI>

@block_next:
	;; dist = (d0.x * poscheck.vect.y - d0.y * poscheck.vect.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.x is already set

	; poscheck.vect.y
	lda	THMOVE_BVECT_Y_L
	sta	VERA_FX_CACHE_H
	lda	THMOVE_BVECT_Y_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; d0.y
	pla	; <d0.y HI>
	sta	VERA_FX_CACHE_M
	pla	; <d0.y LO>
	sta	VERA_FX_CACHE_L

	; -poscheck.vect.x
	sec
	lda	#0
	sbc	THMOVE_BVECT_X_L
	sta	VERA_FX_CACHE_H
	lda	#0
	sbc	THMOVE_BVECT_X_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	THMOVE_DIST_L
	lda	VERA_DATA0
	sta	THMOVE_DIST_H

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; thing check
	lda	THMOVE_BLOCKED
	ror
	bcs	:+

	; dist -= things[poscheck.hit_thing].radius / 2
	lda	THING_STRUCT_RADIUS,x
	lsr
	sta	MATH_TEMP_A
	sec
	lda	THMOVE_DIST_L
	sbc	MATH_TEMP_A
	sta	THMOVE_DIST_L
	bcs	:+
	dec	THMOVE_DIST_H
:
	; dist -= th->radius + 2
	lda	THING_STRUCT_RADIUS,y
	ina
	ina
	sta	MATH_TEMP_A
	sec
	lda	THMOVE_DIST_L
	sbc	MATH_TEMP_A
	sta	THMOVE_DIST_L
	bcs	:+
	dec	THMOVE_DIST_H
:
	; if(dist > 0 && dist < 128)
	lda	THMOVE_DIST_H
	bmi	@done
	ora	THMOVE_DIST_L
	beq	@done
	bpl	:+
@done:
	; done
	rts
:
	;; nx += tab_sin[th->angle] * dist

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist
	lda	THMOVE_DIST_L
	sta	VERA_FX_CACHE_L
	lda	THMOVE_DIST_H
	sta	VERA_FX_CACHE_M

	; tab_sin
	ldx	THING_STRUCT_ANGLE,y
	math_read_sin	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	ARG_THING_CHECK_X_S
	adc	VERA_DATA0
	sta	ARG_THING_CHECK_X_S
	lda	ARG_THING_CHECK_X_L
	adc	VERA_DATA0
	sta	ARG_THING_CHECK_X_L
	lda	ARG_THING_CHECK_X_H
	adc	VERA_DATA0
	sta	ARG_THING_CHECK_X_H

	; ny += tab_cos[th->angle] * dist

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; tab_cos
	math_read_cos	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add result
	stz	VERA_ADDRx_L
	clc
	lda	ARG_THING_CHECK_Y_S
	adc	VERA_DATA0
	sta	ARG_THING_CHECK_Y_S
	lda	ARG_THING_CHECK_Y_L
	adc	VERA_DATA0
	sta	ARG_THING_CHECK_Y_L
	lda	ARG_THING_CHECK_Y_H
	adc	VERA_DATA0
	sta	ARG_THING_CHECK_Y_H

	; try new position
	lda	#1
	sta	ARG_THING_CHECK_ONFLOOR
	stz	ARG_THING_CHECK_SECTOR
	jsr	thing_check_position

	; check result
	lda	THMOVE_BLOCKED
	bne	@done

	; place to all sectors
	jsr	thing_apply_position

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; do the movement
	ldy	TICK_IDX
	lda	ARG_THING_CHECK_X_S
	sta	THING_STRUCT_X_S,y
	lda	ARG_THING_CHECK_X_L
	sta	THING_STRUCT_X_L,y
	lda	ARG_THING_CHECK_X_H
	sta	THING_STRUCT_X_H,y
	lda	ARG_THING_CHECK_Y_S
	sta	THING_STRUCT_Y_S,y
	lda	ARG_THING_CHECK_Y_L
	sta	THING_STRUCT_Y_L,y
	lda	ARG_THING_CHECK_Y_H
	sta	THING_STRUCT_Y_H,y

	; done
	rts

;;;
; projectile hit plane
;;;

plane_texture_hit:
	ina
	bne	:+

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; remove thing
	lda	#0
	sta	THING_STRUCT_NXTFRM_L,y
	sta	THING_STRUCT_NXTFRM_H,y
:
	rts

;;;
; player pressed USE key
;;;

player_use:
	rts

;;;
; wall size by type
;;;

wall_sizes:
	; without scrolling
	.byte	WALL__STRUCT_SIZE_SOLID
	.byte	WALL__STRUCT_SIZE_SPLIT
	.byte	WALL__STRUCT_SIZE_PORTAL
	.byte	WALL__STRUCT_SIZE_MASKED
	; with scrolling
	.byte	WALL__STRUCT_SIZE_SOLID + WALL__STRUCT_SIZE_SCROLL * 1
	.byte	WALL__STRUCT_SIZE_SPLIT + WALL__STRUCT_SIZE_SCROLL * 2
	.byte	WALL__STRUCT_SIZE_PORTAL + WALL__STRUCT_SIZE_SCROLL * 2
	.byte	WALL__STRUCT_SIZE_MASKED + WALL__STRUCT_SIZE_SCROLL * 3

