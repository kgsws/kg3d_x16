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
.export thing_check_pos
.export thing_apply_pos

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
; returns thing in X and ARG_THING_IDX

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

	; check position
	stx	ARG_THING_IDX
	jsr	thing_check_spawn
	beq	@pos_fail

	; place to sectors
	ldy	ARG_THING_IDX
	jsr	thing_apply_spawn

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; DONE
	ldx	ARG_THING_IDX
	rts

@pos_fail:
	;; position check FAILED
	; place only to spawn sector
	; this ignores linked sectors
	; this ignores floor dist

	ldx	ARG_THING_IDX

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; sec->maskblock = 0
	ldy	ARG_THING_SECTOR
	lda	#0
	sta	SECTOR_STRUCT_MASKBLOCK,y

	; place to sector
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
; thing position check
;;;
; ARG_THING_X_*, ARG_THING_Y_*, ARG_THING_Z_* (only L and H) - desired position
; ARG_THING_SECTOR - target sector (zero = from thing)
; ARG_THING_IDX - thing index
; zero flag set = fail
; A = new thing sector
; THMOVE_SECTOR = new thing sector

thing_check_pos:
	ldx	ARG_THING_IDX

	; target sector
	lda	ARG_THING_SECTOR
	bne	:+

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector pointer
	lda	$A000,x
	sta	ARG_THING_SECTOR
:
thing_check_spawn:
	; prepare
	jsr	prepare_pos_check

	; TODO
	lda	#$80
	trb	THMOVE_STEP_HEIGHT

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; portals[0].sector = sdx
	ldx	ARG_THING_SECTOR
	stx	MEM_PORTALS_SECTOR

	; portals[0].islink = 0
	stz	MEM_PORTALS_ISLINK

	; portals[0].touch = 0
	stz	MEM_PORTALS_TOUCH

	; portal_rd = 0
	stz	THMOVE_PORTAL_RD

	; portal_wr = 1
	lda	#1
	sta	THMOVE_PORTAL_WR

	; poscheck.sector = 0
	stz	THMOVE_SECTOR

	; poscheck.maskblock = 0
	stz	THMOVE_MASKBLOCK

	; while(poscheck.portal_rd < poscheck.portal_wr)
@loop_sec:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; inside = any non-zero number
	sta	THMOVE_INSIDE

	; touch = any non-zero number
	sta	THMOVE_TOUCH

	; check floorz
	jsr	get_sector_floorz
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

	; poscheck.floors
	stx	THMOVE_FLOORS
:
	; check ceilingz
	jsr	get_sector_ceilingz
	sec
	sbc	THMOVE_TCZ_L
	lda	THMOVE_CEILINGZ_L
	sbc	THMOVE_TCZ_H
	lda	THMOVE_CEILINGZ_H
	bpl	:+

	; poscheck.ceilingz = poscheck.tcz
	lda	THMOVE_TCZ_L
	sta	THMOVE_CEILINGZ_L
	lda	THMOVE_TCZ_H
	sta	THMOVE_CEILINGZ_H

	; poscheck.ceilings = sdx
	stx	THMOVE_CEILINGS
:
	; walls
	lda	SECTOR_STRUCT_WALL_BANK,x
	ora	#BANK_MAPDATA
	sta	WALL_BANK
	lda	SECTOR_STRUCT_WALL_FIRST,x
	sta	WALL_FIRST

	; wall loop
@loop_wall:
	sta	WALL_NOW
	tay

	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

	; swap check
	lda	WALL_STRUCT_ANGLE_H,y
	bpl	:+

	; V1 diff (X)
	ldx	WALL_STRUCT_NEXT,y
	jsr	v1_diff_x

	; V1 diff (-Y)
	sec
	lda	ARG_THING_Y_L
	sbc	WALL_STRUCT_VTX_Y_L,x
	sta	MATH_VAR_L
	lda	ARG_THING_Y_H
	sbc	WALL_STRUCT_VTX_Y_H,x
	sta	MATH_VAR_H

	; restore
	ldx	ARG_THING_SECTOR

	bra	:++
:
	; V0 diff (X)
	jsr	v0_diff_x

	; V0 diff (-Y)
	sec
	lda	ARG_THING_Y_L
	sbc	WALL_STRUCT_VTX_Y_L,y
	sta	MATH_VAR_L
	lda	ARG_THING_Y_H
	sbc	WALL_STRUCT_VTX_Y_H,y
	sta	MATH_VAR_H
:
	;; get distance
	; dist = (dd.x * wall->dist.y - dd.y * wall->dist.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dd.x is already set

	; wall->dist.y
	lda	WALL_STRUCT_DIST_Y_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_Y_H,y
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; -dd.y
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_L
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_M

	; wall->dist.x
	lda	WALL_STRUCT_DIST_X_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_X_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	MATH_VAR_L
	lda	VERA_DATA0
	sta	MATH_VAR_H

	; if(dist < 0)
	bpl	:+

	; inside = 0
	stz	THMOVE_INSIDE
:
	; if(dist >= poscheck.radius)
	sec
	lda	MATH_VAR_L
	sbc	THMOVE_RADIUS
	lda	MATH_VAR_H
	sbc	#0
	bmi	:+
	jmp	@next_wall
:
	; if(dist + poscheck.radius < 0)
	clc
	lda	MATH_VAR_L
	adc	THMOVE_RADIUS
	lda	MATH_VAR_H
	adc	#0
	bpl	:+

	; touch = 0
	stz	THMOVE_TOUCH
:
	;; left side
	; dist = (dd.x * wall->dist.x + dd.y * wall->dist.y) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dd.x
	; V0 diff (X)
	jsr	v0_diff_x

	; wall->dist.x
	lda	WALL_STRUCT_DIST_X_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_X_H,y
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; dd.y
	; V0 diff (Y)
	sec
	lda	WALL_STRUCT_VTX_Y_L,y
	sbc	ARG_THING_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_STRUCT_VTX_Y_H,y
	sbc	ARG_THING_Y_H
	sta	VERA_FX_CACHE_M

	; wall->dist.y
	lda	WALL_STRUCT_DIST_Y_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_Y_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	lda	VERA_DATA0

	; if(dist < 0)
	bpl	:+

	; TODO: point check
	bra	@next_wall
:
	;; right side
	; dist = (dd.x * wall->dist.x + dd.y * wall->dist.y) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dd.y
	; V1 diff (Y)
	ldx	WALL_STRUCT_NEXT,y
	sec
	lda	WALL_STRUCT_VTX_Y_L,x
	sbc	ARG_THING_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_STRUCT_VTX_Y_H,x
	sbc	ARG_THING_Y_H
	sta	VERA_FX_CACHE_M

	; wall->dist.y is already set

	; accumulate
	lda	VERA_FX_ACCUM

	; dd.x
	; V1 diff (X)
	jsr	v1_diff_x

	; restore
	ldx	ARG_THING_SECTOR

	; wall->dist.x
	lda	WALL_STRUCT_DIST_X_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_X_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	lda	VERA_DATA0

	; if(dist < 0)
	bmi	:+

	; TODO: point check
	bra	@next_wall
:
	;; check backsector

	; wall->backsector
	ldx	WALL_STRUCT_BACKSECTOR,y
	bne	:+

@do_block:
	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

	; TODO: store vector

	;; STOP - FAILED
	; set zero flag
	lda	#0
	sta	THMOVE_SECTOR
	rts
:
	; !(wall->blocking & poscheck.blockedby & 0x7F)
	lda	WALL_STRUCT_BLOCKING,y
	and	THMOVE_BLOCKEDBY
	and	#$7F
	bne	@do_block

	; !check_backsector(wall->backsector, zz)
	jsr	check_backsector
	bmi	@do_block

	; add sector
	jsr	add_sector

	; restore
	ldy	WALL_NOW

	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

@next_wall:
	; next wall
	lda	WALL_STRUCT_NEXT,y
	cmp	WALL_FIRST
	beq	:+
	jmp	@loop_wall
:
	; if(inside && !poscheck.sector)
	lda	THMOVE_INSIDE
	beq	:+
	lda	THMOVE_SECTOR
	bne	:+

	; poscheck.sector = sdx
	lda	ARG_THING_SECTOR
	sta	THMOVE_SECTOR
:
	; next sector
	inc	THMOVE_PORTAL_RD
	ldy	THMOVE_PORTAL_RD
	cpy	THMOVE_PORTAL_WR
	bcs	@done

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; get sector
	ldx	MEM_PORTALS_SECTOR,y
	stx	ARG_THING_SECTOR
	jmp	@loop_sec

@done:
	;; STOP
	; unset zero flag
	lda	THMOVE_SECTOR
	rts

;;;
; thing position apply
;;;
; run after successful 'thing_check_pos'

;;;
; apply thing position
;;;
; ARG_THING_IDX - the thing
; run only after 'thing_check_position' or to remove from all sectors
; if THMOVE_SECTOR is zero, thing is only removed from all sectors

thing_apply_pos:
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
	; thing index
	ldy	ARG_THING_IDX

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
	lda	THMOVE_SECTOR
	bne	:+

	; stop
	rts
:

;;;
; apply spawned thing position
;;;
; falltrough from 'thing_apply_pos'

thing_apply_spawn:
	; place to first sector
	sta	ARG_THING_SECTOR
	lda	#$A0
	sta	THMOVE_PTR_H
	jsr	place_to_sector

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; place to detected sectors
	ldx	#0
@loop_place:
	; check touching flag
	lda	MEM_PORTALS_TOUCH,x
	beq	:+

	; get sector from portal
	lda	MEM_PORTALS_SECTOR,x

	; skip main sector
	cmp	THMOVE_SECTOR
	beq	:+

	; backup
	phx

	; place to this sector
	sta	ARG_THING_SECTOR
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
	ldy	ARG_THING_IDX

;;;
; apply thing position
;;;
; Y - the thing
; call after 'thing_check_heights'
; falltrough from 'thing_apply_pos'

thing_apply_heights:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->floorz = poscheck.floorz
	lda	THMOVE_FLOORZ_L
	sta	THING_STRUCT_FLOORZ_L,y
	lda	THMOVE_FLOORZ_H
	sta	THING_STRUCT_FLOORZ_H,y

	; th->ceilingz = poscheck.ceilingz - poscheck.height
	sec
	lda	THMOVE_CEILINGZ_L
	sbc	THMOVE_HEIGHT
	sta	THING_STRUCT_CEILZ_L,y
	lda	THMOVE_CEILINGZ_H
	sbc	#0
	sta	THING_STRUCT_CEILZ_H,y

	; sectors
	lda	THMOVE_FLOORS
	sta	THING_STRUCT_FLOORS,y
	lda	THMOVE_CEILINGS
	sta	THING_STRUCT_CEILS,y

	; iflags
	lda	THING_STRUCT_IFLAGS,y
	and	#<~(THING_IFLAG_HEIGHTCHECK|THING_IFLAG_NOJUMP)
	ora	THMOVE_IFLAGS
	sta	THING_STRUCT_IFLAGS,y

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

	; original Z
	lda	THING_STRUCT_Z_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_Z_H,y
	sta	ARG_THING_Z_H

	; check new position
	stz	ARG_THING_SECTOR
	sty	ARG_THING_IDX
	jsr	thing_check_pos

	; restore
	ldy	ARG_THING_IDX

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; check result
	lda	THMOVE_SECTOR
	beq	@xy_fail

	; apply
	jsr	thing_apply_pos
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

@xy_fail:

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
; position check intro
;;;
; X - thing index

prepare_pos_check:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; thing type
	ldy	TICKER_STRUCT_TYPE,x

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; <water_height>
	lda	THTYPE_STRUCT_WATER_HEIGHT,y
	pha

	; <step_height>
	lda	THTYPE_STRUCT_STEP_HEIGHT,y
	pha

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; poscheck.radius = th->radius
	lda	THING_STRUCT_RADIUS,x
	sta	THMOVE_RADIUS

	; poscheck.height = th->height
	lda	THING_STRUCT_HEIGHT,x
	sta	THMOVE_HEIGHT

	; poscheck.blockedby = th->blockedby
	lda	THING_STRUCT_BLOCKEDBY,x
	sta	THMOVE_BLOCKEDBY

	; poscheck.step_height = thing_type[th->ticker.type].step_height
	pla	; <step_height>
	sta	THMOVE_STEP_HEIGHT

	; poscheck.water_height = thing_type[th->ticker.type].water_height
	pla	; <water_height>
	sta	THMOVE_WATER_HEIGHT

	; poscheck.th_zh = z + poscheck.height
	clc
	lda	ARG_THING_Z_L
	adc	THMOVE_HEIGHT
	sta	THMOVE_TH_ZH_L
	lda	ARG_THING_Z_H
	adc	#0
	sta	THMOVE_TH_ZH_H

	; poscheck.floorz = -16384
	; poscheck.waterz = -16384
	stz	THMOVE_FLOORZ_L
	stz	THMOVE_WATERZ_L
	lda	#$C0
	sta	THMOVE_FLOORZ_H
	sta	THMOVE_WATERZ_H

	; poscheck.ceilingz = 16384
	stz	THMOVE_CEILINGZ_L
	lda	#$40
	sta	THMOVE_CEILINGZ_H

	; poscheck.floors = 0
	stz	THMOVE_FLOORS

	; poscheck.ceilings = 0
	stz	THMOVE_CEILINGS

	; poscheck.iflags = 0
	stz	THMOVE_IFLAGS

	rts

;;;
; V0 X diff
;;;

v0_diff_x:
	sec
	lda	WALL_STRUCT_VTX_X_L,y
	sbc	ARG_THING_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_STRUCT_VTX_X_H,y
	sbc	ARG_THING_X_H
	sta	VERA_FX_CACHE_M
	rts

;;;
; V1 X diff
;;;

v1_diff_x:
	sec
	lda	WALL_STRUCT_VTX_X_L,x
	sbc	ARG_THING_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_STRUCT_VTX_X_H,x
	sbc	ARG_THING_X_H
	sta	VERA_FX_CACHE_M
	rts

;;;
; get sector floor
;;;
; X - the sector

get_sector_floorz:
	; poscheck.tfz = sec->floor.height
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	THMOVE_TFZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sta	THMOVE_TFZ_H
	rts

;;;
; get sector ceiling
;;;
; X - the sector

get_sector_ceilingz:
	; poscheck.tcz = sec->ceiling.height
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sta	THMOVE_TCZ_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sta	THMOVE_TCZ_H
	rts

;;;
; check back sector
;;;
; X - the sector

check_backsector:
	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; check thing count in this sector
	lda	$BF00,x
	cmp	#31
	bcc	:+

	; set negative flag
	lda	#$FF
	rts
:
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
	sta	MATH_VAR_L
	lda	THMOVE_TCZ_H
	sbc	THMOVE_TFZ_H
	sta	MATH_VAR_H

	; dist -= poscheck.height
	sec
	lda	MATH_VAR_L
	sbc	THMOVE_HEIGHT
	lda	MATH_VAR_H
	sbc	#0
	bmi	@done

	; dist = th_z - poscheck.tfz
	sec
	lda	ARG_THING_Z_L
	sbc	THMOVE_TFZ_L
	sta	MATH_VAR_L
	lda	ARG_THING_Z_H
	sbc	THMOVE_TFZ_H
	sta	MATH_VAR_H

	; dist += poscheck.step_height
	clc
	lda	MATH_VAR_L
	adc	THMOVE_STEP_HEIGHT
	lda	MATH_VAR_H
	adc	#0
	bmi	@done

	; dist = poscheck.tcz - poscheck.th_zh
	sec
	lda	THMOVE_TCZ_L
	sbc	THMOVE_TH_ZH_L
	lda	THMOVE_TCZ_H
	sbc	THMOVE_TH_ZH_H

@done:
	rts

;;;
; add sector to portals
;;;
; X - the sector

add_sector:
	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; check existing portals for duplicates
	ldy	THMOVE_PORTAL_WR
@loop:
	dey
	bmi	@do_save

	; if(portals[i].sector == sdx)
	txa
	cmp	MEM_PORTALS_SECTOR,y
	bne	@loop

	; portals[i].touch |= touch
	lda	MEM_PORTALS_TOUCH,y
	ora	THMOVE_TOUCH
	sta	MEM_PORTALS_TOUCH,y

	rts

@do_save:
	; portals[portal_wr].sector = sdx
	ldy	THMOVE_PORTAL_WR
	txa
	sta	MEM_PORTALS_SECTOR,y
;	lda	THMOVE_ISLINK_SAVE
;	sta	MEM_PORTALS_ISLINK,y
	lda	THMOVE_TOUCH
	sta	MEM_PORTALS_TOUCH,y

	; portal_wr++
	inc	THMOVE_PORTAL_WR

	rts

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
	stz	THMOVE_PTR_L

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

