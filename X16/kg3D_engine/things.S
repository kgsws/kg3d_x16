.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "tables.inc"
.include "main.inc"
.include "math.inc"
.include "map.inc"
.include "tick.inc"
.include "render.inc"
.include "zeropage.inc"
.include "engine.inc"
.include "actions.inc"

.export thing_spawn
.export thing_spawn_org
.export thing_clear
.export thing_launch_ang
.export thing_launch

.export thing_tick
.export thing_plyr

;;;
; CODE
;;;

.segment "CODE"

;;;
; thing spawn
;;;
; requires all ARG_THING_* arguments
; X is originator for '_org' variant
; zero flag set = fail
; returns thing in X

thing_spawn:
	ldx	#0
thing_spawn_org:
	phx	; <origin>

	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; check thing count in this sector
	ldx	ARG_THING_SECTOR
	lda	$BF00,x
	cmp	#30
	bcc	:+

	; stop
@stop:
	pla
	ldx	#0
	rts
:
	; new ticker
	jsr	tick_add
	beq	@stop

	; thing type
	ldy	ARG_THING_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; <eflags>
	lda	THTYPE_STRUCT_EFLAGS,y
	pha

	; <radius>
	lda	THTYPE_STRUCT_RADIUS,y
	pha

	; <height>
	lda	THTYPE_STRUCT_HEIGHT,y
	pha

	; <gravity>
	lda	THTYPE_STRUCT_GRAVITY,y
	pha

	; <scale>
	lda	THTYPE_STRUCT_SCALE,y
	pha

	; <health>
	lda	THTYPE_STRUCT_HEALTH_H,y
	pha
	lda	THTYPE_STRUCT_HEALTH_L,y
	pha

	; <blocking>
	lda	THTYPE_STRUCT_BLOCKING,y
	pha

	; <blockedby>
	lda	THTYPE_STRUCT_BLOCKEDBY,y
	pha

	; get spawn animation
	lda	THTYPE_ANIM_SPAWN_L,y
	pha	; <next frame LO>
	sta	THTEMP_PTR_H
	and	#$F8
	ora	#THSTATE_STRUCT_SPRITE
	sta	THTEMP_PTR_L
	lda	THTEMP_PTR_H
	and	#$07
	ora	#$A0
	sta	THTEMP_PTR_H
	lda	THTYPE_ANIM_SPAWN_H,y
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
	lda	#$FF
	bra	:++
:
	; sprite and frame
	tay
	dec	THTEMP_PTR_L
	lda	(THTEMP_PTR_L)
	and	#$1F
	clc
	adc	TAB_SPRITE_REMAP,y
:
	pha	; <sprite>

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; ticker
	lda	#TICKER_FUNC_THING
	sta	TICKER_STRUCT_FUNC,x

	; type
	lda	ARG_THING_TYPE
	sta	TICKER_STRUCT_TYPE,x

	; clear
	lda	#THING_STRUCT_CLEAR_START
	sta	@loop_clr+2
	sta	@op_clr+2
	stx	@loop_clr+1
	txa
	ora	#$80
	sta	@op_clr+1
	ldy	#(THING_STRUCT_CLEAR_STOP-THING_STRUCT_CLEAR_START)
@loop_clr:
	stz	$AAAA	; modified address
@op_clr:
	stz	$AAAA	; modified address
	inc	@loop_clr+2
	inc	@op_clr+2
	dey
	bne	@loop_clr

	; X
	lda	ARG_THING_X_S
	sta	THING_STRUCT_X_S,x
	lda	ARG_THING_X_L
	sta	THING_STRUCT_X_L,x
	lda	ARG_THING_X_H
	sta	THING_STRUCT_X_H,x

	; Y
	lda	ARG_THING_Y_S
	sta	THING_STRUCT_Y_S,x
	lda	ARG_THING_Y_L
	sta	THING_STRUCT_Y_L,x
	lda	ARG_THING_Y_H
	sta	THING_STRUCT_Y_H,x

	; Z
	lda	ARG_THING_Z_S
	sta	THING_STRUCT_Z_S,x
	lda	ARG_THING_Z_L
	sta	THING_STRUCT_Z_L,x
	lda	ARG_THING_Z_H
	sta	THING_STRUCT_Z_H,x

	; ticks
	lda	#1
	sta	THING_STRUCT_TICKS,x

	; <sprite>
	pla
	sta	THING_STRUCT_SPRITE,x

	; <next frame HI>
	pla
	sta	THING_STRUCT_NXTFRM_H,x

	; <next frame LO>
	pla
	sta	THING_STRUCT_NXTFRM_L,x

	; <blockedby>
	pla
	sta	THING_STRUCT_BLOCKEDBY,x

	; <blocking>
	pla
	sta	THING_STRUCT_BLOCKING,x

	; <health>
	pla
	sta	THING_STRUCT_HEALTH_L,x
	pla
	sta	THING_STRUCT_HEALTH_H,x

	; <scale>
	pla
	sta	THING_STRUCT_SCALE,x

	; <gravity>
	pla
	sta	THING_STRUCT_GRAVITY,x

	; <height>
	pla
	sta	THING_STRUCT_HEIGHT,x

	; <radius>
	pla
	sta	THING_STRUCT_RADIUS,x

	; <eflags>
	pla
	sta	THING_STRUCT_EFLAGS,x

	; <origin>
	pla
	sta	THING_STRUCT_TH_ORIGIN,x

	;; position check FAILED
	; place only to spawn sector
	; this ignores linked sectors
	; this ignores floor dist

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; sec->maskblock = 0
	ldy	ARG_THING_SECTOR
	lda	#0
	sta	SECTOR_STRUCT_MASKBLOCK,y

	; place to sector
	stx	ARG_THING_IDX
	lda	#$A0
	sta	THMOVE_PTR_H
	lda	ARG_THING_SECTOR
	jsr	place_to_sector

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; get sector pointer
	ldy	ARG_THING_SECTOR

	; get ceiling height
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	pha
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	pha

	; get floor height
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	pha
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	pha

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->floorz = poscheck.floorz
	pla
	sta	THING_STRUCT_FLOORZ_L,x
	pla
	sta	THING_STRUCT_FLOORZ_H,x

	; th->ceilingz = poscheck.ceilingz - height
	sec
	pla
	sbc	THING_STRUCT_HEIGHT,x
	sta	THING_STRUCT_CEILZ_L,x
	pla
	sbc	#0
	sta	THING_STRUCT_CEILZ_H,x

	; sector
	lda	ARG_THING_SECTOR
	sta	THING_STRUCT_FLOORS,x
	sta	THING_STRUCT_CEILS,x

	rts

;;;
; thing clear
;;;

thing_clear:
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
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; save type
	lda	#THING_WEAPON_FIRST
	sta	TICKER_STRUCT_TYPE

	; reset weapon
	stz	THING_STRUCT_IFLAGS
	lda	#64
	sta	THING_STRUCT_HEIGHT

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
; assuming 'BANK_TICKER_STRUCT' is selected
; Y - thing index
; X - speed
; A - angle (for 'ang' variant)

thing_launch_ang:
	; speed
	stx	MATH_TEMP_A

	; angle
	pha

	; speed check
	txa
	bpl	:+

	; speed limit
	lda	#127
	sta	MATH_TEMP_A
:
	bra	skip_pitch

thing_launch:
	; speed
	stx	MATH_TEMP_A

	; speed check
	txa
	bpl	:+

	; speed limit
	lda	#127
	sta	MATH_TEMP_A
:
	; get angle
	lda	THING_STRUCT_ANGLE,y
	pha

	; angle from pitch
	lda	THING_STRUCT_PITCH,y
	lsr
	sec
	sbc	#64
	beq	skip_pitch
	tax

	;; th->mz += tab_sin[pitch] * speed

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; speed
	lda	MATH_TEMP_A
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

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

	; speed
	lda	MATH_TEMP_A
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; tab_cos
	math_read_cos	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	MATH_TEMP_A

skip_pitch:
	; get angle
	plx

	;; th->mx = mlimit(th->mx + tab_sin[angle] * speed)

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; speed
	lda	MATH_TEMP_A
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; tab_sin
	math_read_sin	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; th->mx * 1
	lda	THING_STRUCT_MX_S,y
	sta	VERA_FX_CACHE_L
	lda	THING_STRUCT_MX_L,y
	sta	VERA_FX_CACHE_M
	lda	#1
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	THING_STRUCT_MX_S,y
	lda	VERA_DATA0
	sta	THING_STRUCT_MX_L,y

	; overflow check
	eor	VERA_DATA0
	bpl	:+

	; max
	lda	#$FF
	sta	THING_STRUCT_MX_S,y
	lsr
	sta	THING_STRUCT_MX_L,y

	; check sign bit
	lda	VERA_DATA0
	bpl	:+

	; min
	lda	#$80
	sta	THING_STRUCT_MX_L,y
	asl
	sta	THING_STRUCT_MX_S,y
:
	;; th->my = mlimit(th->my + tab_cos[angle] * speed)

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; speed
	lda	MATH_TEMP_A
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; tab_cos
	math_read_cos	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; th->my * 1
	lda	THING_STRUCT_MY_S,y
	sta	VERA_FX_CACHE_L
	lda	THING_STRUCT_MY_L,y
	sta	VERA_FX_CACHE_M
	lda	#1
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	THING_STRUCT_MY_S,y
	lda	VERA_DATA0
	sta	THING_STRUCT_MY_L,y

	; overflow check
	eor	VERA_DATA0
	bpl	:+

	; max
	lda	#$FF
	sta	THING_STRUCT_MY_S,y
	lsr
	sta	THING_STRUCT_MY_L,y

	; check sign bit
	lda	VERA_DATA0
	bpl	:+

	; min
	lda	#$80
	sta	THING_STRUCT_MY_L,y
	asl
	sta	THING_STRUCT_MY_S,y
:
	; done
	rts

;;;
; thing ticker
;;;

thing_tick:
	;; XY movement
	lda	THING_STRUCT_MX_S,y
	ora	THING_STRUCT_MX_L,y
	ora	THING_STRUCT_MY_S,y
	ora	THING_STRUCT_MY_L,y
	bne	:+
	jmp	@no_xy_move
:
	; new X
	ldx	THING_STRUCT_MX_L,y
	lda	TAB_SIGN_EXT,x
	sta	MATH_TEMP_A
	clc
	lda	THING_STRUCT_X_S,y
	adc	THING_STRUCT_MX_S,y
	sta	ARG_THING_X_S
	lda	THING_STRUCT_X_L,y
	adc	THING_STRUCT_MX_L,y
	sta	ARG_THING_X_L
	lda	THING_STRUCT_X_H,y
	adc	MATH_TEMP_A
	sta	ARG_THING_X_H

	; new Y
	ldx	THING_STRUCT_MY_L,y
	lda	TAB_SIGN_EXT,x
	sta	MATH_TEMP_A
	clc
	lda	THING_STRUCT_Y_S,y
	adc	THING_STRUCT_MY_S,y
	sta	ARG_THING_Y_S
	lda	THING_STRUCT_Y_L,y
	adc	THING_STRUCT_MY_L,y
	sta	ARG_THING_Y_L
	lda	THING_STRUCT_Y_H,y
	adc	MATH_TEMP_A
	sta	ARG_THING_Y_H

	; TEST
	lda	ARG_THING_X_S
	sta	THING_STRUCT_X_S,y
	lda	ARG_THING_X_L
	sta	THING_STRUCT_X_L,y
	lda	ARG_THING_X_H
	sta	THING_STRUCT_X_H,y
	lda	ARG_THING_Y_S
	sta	THING_STRUCT_Y_S,y
	lda	ARG_THING_Y_L
	sta	THING_STRUCT_Y_L,y
	lda	ARG_THING_Y_H
	sta	THING_STRUCT_Y_H,y

	;; XY friction

	; check flag
	lda	THING_STRUCT_EFLAGS,y
	and	#THING_EFLAG_PROJECTILE
	bne	@no_xy_friction

	; thing
	ldx	G_TICKER_IDX

	; MX
	lda	THING_STRUCT_MX_L,x
	beq	:++
	cmp	#$FF
	beq	:+
	rol
	ror	THING_STRUCT_MX_L,x
	ror	THING_STRUCT_MX_S,x
	bra	:+++
:
	stz	THING_STRUCT_MX_L,x
:
	stz	THING_STRUCT_MX_S,x
:
	; MY
	lda	THING_STRUCT_MY_L,x
	beq	:++
	cmp	#$FF
	beq	:+
	rol
	ror	THING_STRUCT_MY_L,x
	ror	THING_STRUCT_MY_S,x
	bra	:+++
:
	stz	THING_STRUCT_MY_L,x
:
	stz	THING_STRUCT_MY_S,x
:
@no_xy_friction:
@no_xy_move:
	;; Z movement

	; new Z
	ldx	THING_STRUCT_MZ_L,y
	lda	TAB_SIGN_EXT,x
	sta	MATH_TEMP_A
	clc
	lda	THING_STRUCT_Z_S,y
	adc	THING_STRUCT_MZ_S,y
	sta	ARG_THING_Z_S
	lda	THING_STRUCT_Z_L,y
	adc	THING_STRUCT_MZ_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_Z_H,y
	adc	MATH_TEMP_A
	sta	ARG_THING_Z_H

	; ceiling check
	sec
	lda	THING_STRUCT_CEILZ_L,y
	sbc	ARG_THING_Z_L
	lda	THING_STRUCT_CEILZ_H,y
	sbc	ARG_THING_Z_H
	bpl	:+

	; over ceiling
	lda	#0
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y
	stz	ARG_THING_Z_S
	lda	THING_STRUCT_CEILZ_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_CEILZ_H,y
	sta	ARG_THING_Z_H
:
	; floor check
	sec
	lda	ARG_THING_Z_L
	sbc	THING_STRUCT_FLOORZ_L,y
	lda	ARG_THING_Z_H
	sbc	THING_STRUCT_FLOORZ_H,y
	bpl	:+

	; under floor
	lda	#0
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y
	stz	ARG_THING_Z_S
	lda	THING_STRUCT_FLOORZ_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_FLOORZ_H,y
	sta	ARG_THING_Z_H
:
	; apply Z
	lda	ARG_THING_Z_S
	sta	THING_STRUCT_Z_S,y
	lda	ARG_THING_Z_L
	sta	THING_STRUCT_Z_L,y
	lda	ARG_THING_Z_H
	sta	THING_STRUCT_Z_H,y

	;; gravity

	; on floor check
	sec
	lda	THING_STRUCT_FLOORZ_L,y
	sbc	ARG_THING_Z_L
	lda	THING_STRUCT_FLOORZ_H,y
	sbc	ARG_THING_Z_H
	bpl	:+

	; th->mz -= th->gravity << 4
	lda	THING_STRUCT_GRAVITY,y
	tax
	lda	TAB_SWAP,x
	and	#$F0
	sta	MATH_TEMP_A
	lda	TAB_SWAP,x
	and	#$0F
	sta	MATH_TEMP_B
	sec
	lda	THING_STRUCT_MZ_S,y
	sbc	MATH_TEMP_A
	sta	THING_STRUCT_MZ_S,y
	lda	THING_STRUCT_MZ_L,y
	sbc	MATH_TEMP_B
	sta	THING_STRUCT_MZ_L,y

	bra	@no_z_friction
:

@no_z_friction:

	rts

;;;
; player ticker
;;;

thing_plyr:
	; check freeze
	lda	THING_STRUCT_COUNTER,y
	beq	:+

	; ticcmd.angle = th->angle
	lda	THING_STRUCT_ANGLE,y
	sta	TICK_CMD_ANGLE

	; ticcmd.pitch = th->pitch
	lda	THING_STRUCT_PITCH,y
	sta	TICK_CMD_PITCH

	; done
	jmp	thing_tick
:
	; th->angle = ticcmd.angle
	lda	TICK_CMD_ANGLE
	sta	THING_STRUCT_ANGLE,y

	; th->pitch = ticcmd.pitch
	lda	TICK_CMD_PITCH
	sta	THING_STRUCT_PITCH,y

	; if(ticcmd.bits & TCMD_MOVING)
	ldx	TICK_CMD_BITS_L
	bpl	@skip_move

	; get type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; speed
	lda	THTYPE_STRUCT_SPEED,x
	tax

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; angle
	lda	TICK_CMD_BITS_H
	and	#$E0
	clc
	adc	TICK_CMD_ANGLE

	; move
	jsr	thing_launch_ang

@skip_move:
	; done
	jmp	thing_tick

;;;
; place to sector
;;;
; ARG_THING_SECTOR - desired sector
; ARG_THING_IDX - desired thing
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
	ldy	ARG_THING_SECTOR
@loop:
	lda	(HI_PTR_L),y
	bne	@not_free

	; add thing to this slot
	lda	ARG_THING_IDX
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
	ldy	ARG_THING_IDX

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

