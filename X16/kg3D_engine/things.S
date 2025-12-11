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
.export thing_check_heights
.export thing_apply_pos
.export thing_nextfrm

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

	; the thing
	stx	ARG_THING_IDX

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

	; <view height>
	lda	THTYPE_STRUCT_VIEW_HEIGHT,y
	pha

	; get spawn animation
	ldx	THTYPE_ANIM_SPAWN_L,y
	phx	; <next frame LO>
	lda	THTYPE_ANIM_SPAWN_H,y
	pha	; <next frame HI>

	; thing states bank
	and	#$07
	ora	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; spawn sprite
	ldy	THSTATE_STRUCT_SPRITE,x
	bmi	:+

	; sprite and frame
	lda	THSTATE_STRUCT_FRM_NXT,x
	and	#$1F
	clc
	adc	TAB_SPRITE_REMAP,y
:
	pha	; <sprite>

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; ticker
	ldx	ARG_THING_IDX
	lda	#TICKER_FUNC_THING
	sta	TICKER_STRUCT_FUNC,x

	; type
	lda	ARG_THING_TYPE
	sta	TICKER_STRUCT_TYPE,x

	; clear
	lda	#THING_STRUCT_CLEAR_FIRST
	sta	@loop_clr+2
	sta	@op_clr+2
	stx	@loop_clr+1
	txa
	ora	#$80
	sta	@op_clr+1
	ldy	#(1+THING_STRUCT_CLEAR_LAST-THING_STRUCT_CLEAR_FIRST)
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

	; <view height>
	pla
	sta	THING_STRUCT_VIEW_HEIGHT,x

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

	; th->floorz = -16384
	stz	THING_STRUCT_FLOORZ_L,x
	lda	#$C0
	sta	THING_STRUCT_FLOORZ_H,x

	; check position
	jsr	thing_check_spawn
	beq	@pos_fail

	; place to sectors
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

	; place to sector
	stz	THMOVE_MIDHIT
	lda	#$B0
	sta	THMOVE_PTR_H
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
	jsr	clear_bank_bot

	;; PLAYER WEAPON
	; thing 0 is always weapon of local player

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; current weapon
	ldx	#THING_WEAPON_FIRST

	; animation
	ldy	THTYPE_ANIM_RAISE_L,x
	phy	; <next frame LO>
	lda	THTYPE_ANIM_RAISE_H,x
	pha	; <next frame HI>

	; thing states bank
	and	#$07
	ora	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; the sprite
	ldx	THSTATE_STRUCT_SPRITE,y
	bmi	:+

	; sprite and frame
	lda	THSTATE_STRUCT_FRM_NXT,y
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
	stx	THING_STRUCT_SPRITE

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
; thing height check
;;;
; ARG_THING_X_*, ARG_THING_Y_*, ARG_THING_Z_* (only L and H) - current position
; ARG_THING_IDX - thing index

thing_check_heights:
	ldx	ARG_THING_IDX

	; prepare
	jsr	prepare_pos_check

	; poscheck.th_sh = nz - 256
	lda	ARG_THING_Z_L
	sta	THMOVE_TH_SH_L
	lda	ARG_THING_Z_H
	ina
	sta	THMOVE_TH_SH_H

	; go trough sectors
	stx	TMP_PTR_L
	lda	#$B0
	sta	TMP_PTR_H
@loop:
	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; get sector at this slot
	lda	(TMP_PTR_L)	; thingsec
	beq	@stop
	tax

	; get midhit flag
	ldy	#$80
	lda	(TMP_PTR_L),y	; thingces
	and	#$80
	sta	THMOVE_MIDHIT

	; floor and ceiling
	phx
	jsr	check_planes

	; go trough things
	ply
	jsr	check_things

	; next
	inc	TMP_PTR_H
	lda	TMP_PTR_H
	cmp	#$C0
	bcc	@loop

@stop:
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

	; thing sector
	lda	$B000,x
	sta	ARG_THING_SECTOR
:
thing_check_spawn:
	; prepare
	jsr	prepare_pos_check

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

	; portals[0].midhit = 0
	stz	MEM_PORTALS_MIDHIT

	; portal_rd = 0
	stz	THMOVE_PORTAL_RD

	; portal_wr = 1
	lda	#1
	sta	THMOVE_PORTAL_WR

	; poscheck.sector = 0
	stz	THMOVE_SECTOR

	; poscheck.midsec = 0
	stz	THMOVE_MIDSEC

	; poscheck.htype = 0
	stz	THMOVE_HTYPE

	; not-a-link
	stz	THMOVE_ISLINK

	; no-midhit
	stz	THMOVE_MIDHIT

	; while(poscheck.portal_rd < poscheck.portal_wr)
@loop_sec:
	; inside = any non-zero number
	lda	#$FF
	sta	THMOVE_INSIDE

	; poscheck.pthit = 0
	stz	THMOVE_PTHIT

	; floor and ceiling
	jsr	check_planes

	; link check
	lda	THMOVE_ISLINK
	beq	:+

	; skip walls
	jmp	@next_sector
:
	; walls
	lda	SECTOR_STRUCT_WALL_BANK,x
	ora	#BANK_MAPWALLS
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

	; touch = 0xFF
	lda	#$FF
	sta	THMOVE_TOUCH

	; swap check
	lda	WALL_STRUCT_ANGLE_H,y
	bpl	:+

	; V1 diff (X)
	ldx	WALL_STRUCT_NEXT,y
	sec
	lda	WALL_STRUCT_VTX_X_L,x
	sbc	ARG_THING_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_STRUCT_VTX_X_H,x
	sbc	ARG_THING_X_H
	sta	VERA_FX_CACHE_M

	; V1 diff (-Y)
	sec
	lda	ARG_THING_Y_L
	sbc	WALL_STRUCT_VTX_Y_L,x
	sta	MATH_VAR_L
	lda	ARG_THING_Y_H
	sbc	WALL_STRUCT_VTX_Y_H,x
	sta	MATH_VAR_H

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
	jmp	@next_wall
:
	; poscheck.midhit = touch & wall->blockmid & poscheck.blockedby ? 0x80 : 0x00
	lda	WALL_STRUCT_BLOCKMID,y
	and	THMOVE_TOUCH
	and	THMOVE_BLOCKEDBY
	beq	:+
	lda	#$80
:
	sta	THMOVE_MIDHIT

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
	sta	MATH_P2A_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_STRUCT_VTX_Y_H,x
	sbc	ARG_THING_Y_H
	sta	MATH_P2A_Y_H
	sta	VERA_FX_CACHE_M

	; wall->dist.y is already set

	; accumulate
	lda	VERA_FX_ACCUM

	; dd.x
	; V1 diff (X)
	sec
	lda	WALL_STRUCT_VTX_X_L,x
	sbc	ARG_THING_X_L
	sta	MATH_P2A_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_STRUCT_VTX_X_H,x
	sbc	ARG_THING_X_H
	sta	MATH_P2A_X_H
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

	; if(dist >= 0)
	bmi	:+

	; !poscheck.htype
	lda	THMOVE_HTYPE
	bne	@next_wall

	; dist - poscheck.radius < 0
	sec
	lda	MATH_VAR_L
	sbc	THMOVE_RADIUS
	lda	MATH_VAR_H
	sbc	#0
	bpl	@next_wall

	; check this point
	jsr	check_point

	bra	@next_wall
:
	; !poscheck.htype
	lda	THMOVE_HTYPE
	bne	:+

	; dist + poscheck.radius >= 0
	clc
	lda	MATH_VAR_L
	adc	THMOVE_RADIUS
	lda	MATH_VAR_H
	adc	#0
	bmi	:+

	; check this point
	jsr	check_point
:
	;; check backsector

	; wall->backsector
	ldx	WALL_STRUCT_BACKSECTOR,y
	bne	@check_backsector

@do_block:
	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

	; save
	sta	THMOVE_HTYPE
	sty	THMOVE_HIDX

	; store angle
	ldx	WALL_STRUCT_ANGLE_L,y
	lda	TAB_SWAP,x
	and	#$0F
	sta	THMOVE_HITANG
	ldx	WALL_STRUCT_ANGLE_H,y
	lda	TAB_SWAP,x
	and	#$F0
	tsb	THMOVE_HITANG

	;; STOP - FAILED
	; set zero flag
	lda	#0
	sta	THMOVE_SECTOR
	rts

@check_backsector:
	; !(wall->blocking & poscheck.blockedby & 0x7F)
	lda	WALL_STRUCT_BLOCKING,y
	and	THMOVE_BLOCKEDBY
	and	#$7F
	bne	@do_block

	; !check_line_block(wall->backsector, zz)
	jsr	check_line_block
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
	; check point
	lda	THMOVE_PTHIT
	bne	:+
	jmp	@skip_point
:
	; wall
	ldy	THMOVE_PTWALL

	; waln
	ldx	WALL_STRUCT_NEXT,y

	; p2a_coord.x = waln->vtx.x
	lda	WALL_STRUCT_VTX_X_L,x
	sta	MATH_P2A_X_L
	lda	WALL_STRUCT_VTX_X_H,x
	sta	MATH_P2A_X_H

	; p2a_coord.y = waln->vtx.y
	lda	WALL_STRUCT_VTX_Y_L,x
	sta	MATH_P2A_Y_L
	lda	WALL_STRUCT_VTX_Y_H,x
	sta	MATH_P2A_Y_H

@point_next:
	; wall->backsector
	ldx	WALL_STRUCT_BACKSECTOR,y
	bne	@point_backsector

@point_block:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; save
	lda	WALL_BANK
	sta	THMOVE_HTYPE
	sty	THMOVE_HIDX

	; thing
	ldx	ARG_THING_IDX

	; p2a_coord.x -= th->x / 256
	sec
	lda	MATH_P2A_X_L
	sbc	THING_STRUCT_X_L,x
	sta	MATH_P2A_X_L
	lda	MATH_P2A_X_H
	sbc	THING_STRUCT_X_H,x
	sta	MATH_P2A_X_H

	; p2a_coord.y -= th->y / 256
	sec
	lda	MATH_P2A_Y_L
	sbc	THING_STRUCT_Y_L,x
	sta	MATH_P2A_Y_L
	lda	MATH_P2A_Y_H
	sbc	THING_STRUCT_Y_H,x
	sta	MATH_P2A_Y_H

	; angle
	jsr	math_p2a
	jsr	math_ang8
	clc
	lda	MATH_VAR_U
	adc	#$40
	sta	THMOVE_HITANG

	bra	@skip_point

@point_backsector:
	; (wall->blocking & poscheck.blockedby & 0x7F)
	lda	WALL_STRUCT_BLOCKING,y
	and	THMOVE_BLOCKEDBY
	and	#$7F
	bne	@point_block

	; poscheck.midhit = wall->blockmid & poscheck.blockedby ? 0x80 : 0x00
	lda	WALL_STRUCT_BLOCKMID,y
	and	THMOVE_BLOCKEDBY
	beq	:+
	lda	#$80
:
	sta	THMOVE_MIDHIT

	; check_line_block(wall->backsector, zz)
	jsr	check_line_block
	bmi	@point_block

	; add backsector
	stx	THMOVE_TOUCH
	jsr	add_sector

	; handle next
	lda	THMOVE_PTHIT
	beq	@skip_point
	stz	THMOVE_PTHIT

	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

	; try next
	ldx	THMOVE_PTWALL
	ldy	WALL_STRUCT_NEXT,x
	bra	@point_next

@skip_point:
	; if(inside && !poscheck.sector)
	lda	THMOVE_INSIDE
	beq	:+
	lda	THMOVE_SECTOR
	bne	:+

	; poscheck.sector = sdx
	ldx	ARG_THING_SECTOR
	stx	THMOVE_SECTOR

	; if(poscheck.midsec == sdx)
	cpx	THMOVE_MIDSEC
	bne	@skip_mid_sec

	; poscheck.midhit = 0x80
	lda	#$80
	sta	THMOVE_MIDHIT

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; re-check floor
	jsr	check_floor_plane

@skip_mid_sec:
	; add sector links
	stx	THMOVE_TOUCH
	jsr	add_sector_links
:
@next_sector:
	; next sector
	inc	THMOVE_PORTAL_RD
	ldy	THMOVE_PORTAL_RD
	cpy	THMOVE_PORTAL_WR
	bcs	@done

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; get sector
	lda	MEM_PORTALS_MIDHIT,y
	sta	THMOVE_MIDHIT
	lda	MEM_PORTALS_ISLINK,y
	sta	THMOVE_ISLINK
	ldx	MEM_PORTALS_SECTOR,y
	stx	ARG_THING_SECTOR
	jmp	@loop_sec

@done:
	; check point hit
	lda	THMOVE_HTYPE
	beq	:+

	;; STOP - FAILED
	; set zero flag
	lda	#0
	sta	THMOVE_SECTOR
	rts
:
	;; check things
	stz	THMOVE_PORTAL_RD
@loop_thsec:
	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; get sector
	ldx	THMOVE_PORTAL_RD
	ldy	MEM_PORTALS_SECTOR,x

	; go trough things
	jsr	check_things
	bmi	@do_thing_block

	; next
	inc	THMOVE_PORTAL_RD
	lda	THMOVE_PORTAL_RD
	cmp	THMOVE_PORTAL_WR
	bcs	:+
	jmp	@loop_thsec
:
	; check heights
	sec
	lda	THMOVE_CEILINGZ_L
	sbc	THMOVE_FLOORZ_L
	sta	MATH_VAR_L
	lda	THMOVE_CEILINGZ_H
	sbc	THMOVE_FLOORZ_H
	sta	MATH_VAR_H
	sec
	lda	MATH_VAR_L
	sbc	THMOVE_HEIGHT
	lda	MATH_VAR_H
	sbc	#0
	bpl	:+

	; bad hit
	stz	THMOVE_HTYPE

	;; STOP - FAILED
	; set zero flag
	lda	#0
	sta	THMOVE_SECTOR
	rts
:
	;; STOP - PASSED
	; unset zero flag
	lda	THMOVE_SECTOR
	rts

@do_thing_block:
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

	; poscheck.hitang = point_to_angle() >> 4
	jsr	math_p2a
	jsr	math_ang8
	clc
	lda	MATH_VAR_U
	adc	#$40
	sta	THMOVE_HITANG

	; poscheck.htype = 0xFF
	dec	THMOVE_HTYPE

	;; STOP - FAILED
	; set zero flag
	lda	#0
	sta	THMOVE_SECTOR
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
	lda	#$B0
	sta	THSEC_PTR_H	; thingsec
	sta	THCES_PTR_H	; thingces
	lda	#$80
	sta	THCES_PTR_L
	ldx	#16
@loop_del:
	; thing index
	ldy	ARG_THING_IDX

	; get sector at this slot
	lda	(THSEC_PTR_L),y

	; check for NONE
	beq	@end

	; save sector at this slot
	sta	TMP_PTR_I

	; remove sector from thing
	lda	#0
	sta	(THSEC_PTR_L),y

	; get slot in sector
	lda	(THCES_PTR_L),y
	and	#31
	ora	#$A0
	sta	TMP_PTR_H

	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; clear thing in this sector slot
	lda	#0
	ldy	TMP_PTR_I
	sta	(TMP_PTR_L),y

	; decrement thing count for this sector
	lda	$BF00,y
	dea
	sta	$BF00,y

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; next
	inc	THSEC_PTR_H
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

	; poscheck.midsec == poscheck.sector ? 0x80 : 0x00
	stz	THMOVE_MIDHIT
	cmp	THMOVE_MIDSEC
	bne	:+
	lda	#$80
	sta	THMOVE_MIDHIT
:
	; place to first sector
	lda	#$B0
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

	; the sector
	sta	ARG_THING_SECTOR

	; mid hit
	lda	MEM_PORTALS_MIDHIT,x
	sta	THMOVE_MIDHIT

	; backup
	phx

	; place to this sector
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
	ldx	ARG_THING_IDX

;;;
; apply thing position
;;;
; X - the thing
; call after 'thing_check_heights'
; falltrough from 'thing_apply_pos'

thing_apply_heights:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->floorz = poscheck.floorz
	lda	THMOVE_FLOORZ_L
	sta	THING_STRUCT_FLOORZ_L,x
	lda	THMOVE_FLOORZ_H
	sta	THING_STRUCT_FLOORZ_H,x

	; th->ceilingz = poscheck.ceilingz - poscheck.height
	sec
	lda	THMOVE_CEILINGZ_L
	sbc	THMOVE_HEIGHT
	sta	THING_STRUCT_CEILZ_L,x
	lda	THMOVE_CEILINGZ_H
	sbc	#0
	sta	THING_STRUCT_CEILZ_H,x

	; planes
	lda	THMOVE_FLOORS
	sta	THING_STRUCT_FLOORS,x
	lda	THMOVE_FLOORT
	sta	THING_STRUCT_FLOORT,x
	lda	THMOVE_CEILINGS
	sta	THING_STRUCT_CEILS,x
	lda	THMOVE_CEILINGT
	sta	THING_STRUCT_CEILT,x

	; done
	rts

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

	; poscheck.water_height = thing_type[th->ticker.type].water_height
	lda	THTYPE_STRUCT_WATER_HEIGHT,y
	sta	THMOVE_WATER_HEIGHT

	; step_height
	lda	THTYPE_STRUCT_STEP_HEIGHT,y
	sta	MATH_VAR_U

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; if(th->mz / 256 > -32)
	lda	THING_STRUCT_MZ_L,x
	bpl	:+
	cmp	#$E1
	bcs	:+

	; poscheck.water_height = 255
	lda	#$FF
	sta	THMOVE_WATER_HEIGHT
:
	; poscheck.radius = th->radius
	lda	THING_STRUCT_RADIUS,x
	sta	THMOVE_RADIUS

	; poscheck.height = th->height
	lda	THING_STRUCT_HEIGHT,x
	sta	THMOVE_HEIGHT

	; poscheck.blockedby = th->blockedby
	lda	THING_STRUCT_BLOCKEDBY,x
	sta	THMOVE_BLOCKEDBY

	; mz >= 0
	lda	THING_STRUCT_MZ_L,x
	bmi	:+

	; fz - nz
	sec
	lda	THING_STRUCT_FLOORZ_L,x
	sbc	ARG_THING_Z_L
	sta	MATH_VAR_L
	lda	THING_STRUCT_FLOORZ_H,x
	sbc	ARG_THING_Z_H
	sta	MATH_VAR_H

	; + step_height
	clc
	lda	MATH_VAR_L
	adc	MATH_VAR_U
	lda	MATH_VAR_H
	adc	#0

	; check
	bpl	:++
:
	stz	MATH_VAR_U
:
	; poscheck.th_sh = nz + thing_type[th->ticker.type].step_height
	clc
	lda	MATH_VAR_U
	adc	ARG_THING_Z_L
	sta	THMOVE_TH_SH_L
	lda	#0
	adc	ARG_THING_Z_H
	sta	THMOVE_TH_SH_H

	; poscheck.th_zh = z + poscheck.height
	clc
	lda	ARG_THING_Z_L
	adc	THMOVE_HEIGHT
	sta	THMOVE_TH_ZH_L
	lda	ARG_THING_Z_H
	adc	#0
	sta	THMOVE_TH_ZH_H

	; poscheck.floorz = -16384
	stz	THMOVE_FLOORZ_L
	lda	#$C0
	sta	THMOVE_FLOORZ_H

	; poscheck.ceilingz = 16384
	stz	THMOVE_CEILINGZ_L
	lda	#$40
	sta	THMOVE_CEILINGZ_H

	; poscheck.floors = 0
	stz	THMOVE_FLOORS

	; poscheck.ceilings = 0
	stz	THMOVE_CEILINGS

	; poscheck.floort = 0
	stz	THMOVE_FLOORT

	; poscheck.ceilingt = 0
	stz	THMOVE_CEILINGT

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
; get sector floor
;;;
; X - the sector

get_sector_floorz:
	; if(sec->floor.link)
	lda	SECTOR_STRUCT_FLOOR_LINK,x
	beq	@no_link

	phy
	tay

	; poscheck.water_height < 0xFF
	lda	THMOVE_WATER_HEIGHT
	ina
	beq	@no_water

	; map_sectors[sec->floor.link].flags & SECTOR_FLAG_WATER
	lda	SECTOR_STRUCT_FLAGS,y
	bpl	@no_water

	; poscheck.tfz = sec->floor.height - poscheck.water_height
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sbc	THMOVE_WATER_HEIGHT
	sta	THMOVE_TFZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sbc	#0
	sta	THMOVE_TFZ_H
	ply
	rts

@no_water:
	; poscheck.tfz = -16384
	stz	THMOVE_TFZ_L
	lda	#$C0
	sta	THMOVE_TFZ_H
	ply
	rts

@no_link:
	lda	SECTOR_STRUCT_FLOOR_DIST,x
	bne	@do_dist

	; poscheck.tfz = sec->floor.height
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	THMOVE_TFZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sta	THMOVE_TFZ_H

@do_midhit:
	; if(poscheck.midhit)
	lda	THMOVE_MIDHIT
	bne	:+

	rts
:
	; poscheck.tfz += (int32_t)sec->midheight * 2
	clc
	lda	SECTOR_STRUCT_MIDHEIGHT,x
	adc	SECTOR_STRUCT_MIDHEIGHT,x
	sta	MATH_VAR_L
	stz	MATH_VAR_H
	rol	MATH_VAR_H
;	clc
	lda	THMOVE_TFZ_L
	adc	MATH_VAR_L
	sta	THMOVE_TFZ_L
	lda	THMOVE_TFZ_H
	adc	MATH_VAR_H
	sta	THMOVE_TFZ_H

	rts

@do_dist:
	; sign extend
	phy
	tay
	lda	TAB_SIGN_EXT,y
	sta	MATH_VAR_SIGN
	ply

	; poscheck.tfz = sec->floor.height
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sbc	SECTOR_STRUCT_FLOOR_DIST,x
	sta	THMOVE_TFZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sbc	MATH_VAR_SIGN
	sta	THMOVE_TFZ_H

	bra	@do_midhit

;;;
; get sector ceiling
;;;
; X - the sector

get_sector_ceilingz:
	; if(sec->ceiling.link)
	lda	SECTOR_STRUCT_CEILING_LINK,x
	beq	:+

	; poscheck.tcz = 16384
	stz	THMOVE_TCZ_L
	lda	#$40
	sta	THMOVE_TCZ_H
	rts
:
	; poscheck.tcz = sec->ceiling.height
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sta	THMOVE_TCZ_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sta	THMOVE_TCZ_H
	rts

;;;
; check plane heights
;;;
; X - the sector

check_planes:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; check ceilingz
	jsr	get_sector_ceilingz
	sec
	lda	THMOVE_TCZ_L
	sbc	THMOVE_CEILINGZ_L
	lda	THMOVE_TCZ_H
	sbc	THMOVE_CEILINGZ_H
	bpl	:+

	; poscheck.ceilingz = poscheck.tcz
	lda	THMOVE_TCZ_L
	sta	THMOVE_CEILINGZ_L
	lda	THMOVE_TCZ_H
	sta	THMOVE_CEILINGZ_H

	; poscheck.ceilings = sdx
	stx	THMOVE_CEILINGS
:
check_floor_plane:
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
	stz	THMOVE_FLOORS
	lda	THMOVE_MIDHIT
	bne	:+
	stx	THMOVE_FLOORS
:
	rts

;;;
; check things in a sector
;;;
; Y - the sector
; negative flag set = fail

check_things:
	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; sector things
	lda	#$A0
	sta	THMOVE_PTR_H
	sty	THMOVE_PTR_L
@loop_thing:
	; check this slot
	lda	(THMOVE_PTR_L)
	beq	@next_slot

	; self
	cmp	ARG_THING_IDX
	beq	@next_slot
	tax

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; blocking
	lda	THING_STRUCT_BLOCKING,x
	and	THMOVE_BLOCKEDBY
	beq	@next_slot_ex

	; radius = ht->radius + poscheck.radius
	clc
	lda	THING_STRUCT_RADIUS,x
	adc	THMOVE_RADIUS
	sta	THMOVE_RADIUS_L
	stz	THMOVE_RADIUS_H
	rol	THMOVE_RADIUS_H

	; X diff
	sec
	lda	ARG_THING_X_L
	sbc	THING_STRUCT_X_L,x
	sta	THMOVE_DIFF_L
	sta	MATH_P2A_X_L
	lda	ARG_THING_X_H
	sbc	THING_STRUCT_X_H,x
	sta	THMOVE_DIFF_H
	sta	MATH_P2A_X_H

	; X check
	jsr	check_radist
	bpl	@next_slot_ex

	; Y diff
	sec
	lda	ARG_THING_Y_L
	sbc	THING_STRUCT_Y_L,x
	sta	THMOVE_DIFF_L
	sta	MATH_P2A_Y_L
	lda	ARG_THING_Y_H
	sbc	THING_STRUCT_Y_H,x
	sta	THMOVE_DIFF_H
	sta	MATH_P2A_Y_H

	; Y check
	jsr	check_radist
	bpl	@next_slot_ex

	; save
	stx	THMOVE_HIDX

	; dist = point_to_dist()
	jsr	math_p2d

	; if(dist - radius >= 0)
	sec
	lda	MATH_VAR_L
	sbc	THMOVE_RADIUS_L
	lda	MATH_VAR_H
	sbc	THMOVE_RADIUS_H
	bmi	@do_hit_thing

@next_slot_ex:
	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

@next_slot:
	; next
	inc	THMOVE_PTR_H
	lda	THMOVE_PTR_H
	cmp	#$BF
	beq	:+
	jmp	@loop_thing
:
	; clear negative flag
	lda	#0
	rts

@do_hit_thing:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; things
	ldy	THMOVE_HIDX
	ldx	ARG_THING_IDX

	; under check
	sec
	lda	THING_STRUCT_Z_L,y
	sbc	THMOVE_TH_ZH_L
	lda	THING_STRUCT_Z_H,y
	sbc	THMOVE_TH_ZH_H
	bmi	@not_under

	; check ceilingz
	sec
	lda	THING_STRUCT_Z_L,y
	sbc	THMOVE_CEILINGZ_L
	lda	THING_STRUCT_Z_H,y
	sbc	THMOVE_CEILINGZ_H
	bpl	:+

	; poscheck.ceilingz = dist
	lda	THING_STRUCT_Z_L,y
	sta	THMOVE_CEILINGZ_L
	lda	THING_STRUCT_Z_H,y
	sta	THMOVE_CEILINGZ_H

	; poscheck.ceilingt = odx
	sty	THMOVE_CEILINGT
:
	; set overlap
@do_overlap:
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,y

	lda	THING_STRUCT_IFLAGS,x
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,x

	bra	@next_slot_ex

@not_under:
	; over check
	clc
	lda	THING_STRUCT_Z_L,y
	adc	THING_STRUCT_HEIGHT,y
	sta	MATH_VAR_L
	lda	THING_STRUCT_Z_H,y
	adc	#0
	sta	MATH_VAR_H
	sec
	lda	THMOVE_TH_SH_L
	sbc	MATH_VAR_L
	lda	THMOVE_TH_SH_H
	sbc	MATH_VAR_H
	bmi	@hit_over

	; check floorz
	sec
	lda	THMOVE_FLOORZ_L
	sbc	MATH_VAR_L
	lda	THMOVE_FLOORZ_H
	sbc	MATH_VAR_H
	bpl	:+

	; poscheck.floorz = dist
	lda	MATH_VAR_L
	sta	THMOVE_FLOORZ_L
	lda	MATH_VAR_H
	sta	THMOVE_FLOORZ_H

	; poscheck.floort = odx
	sty	THMOVE_FLOORT
:
	bra	@do_overlap

@hit_over:
	rts

;;;
; check point
;;;

check_point:
	jsr	math_p2d

	; if(dist >= poscheck.radius)
	lda	MATH_VAR_H
	bne	@skip
	lda	MATH_VAR_L
	cmp	THMOVE_RADIUS
	bcs	@skip

	; save point hit
	lda	#$FF
	sta	THMOVE_PTHIT
	lda	WALL_NOW
	sta	THMOVE_PTWALL

@skip:
	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

	; restore
	ldy	WALL_NOW

	rts

;;;
; check back sector
;;;
; X - the sector
; Y - the wall

check_line_block:
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
	; touch
	lda	THMOVE_TOUCH
	beq	@no_mid

	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

	; wall->blockmid & poscheck.blockedby
	lda	WALL_STRUCT_BLOCKMID,y
	and	THMOVE_BLOCKEDBY
	beq	@no_mid

	; !(wall->tflags & 0b10000000)
	lda	WALL_STRUCT_TFLAGS,y
	bmi	@no_mid

	; bkup = poscheck.midhit
	lda	THMOVE_MIDHIT
	pha

	; poscheck.midhit = 0x80

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; get height
	phx
	ldx	ARG_THING_SECTOR
	jsr	get_sector_floorz
	plx

	; poscheck.midhit = bkup
	pla
	sta	THMOVE_MIDHIT

	; dist = poscheck.ceilingz - poscheck.tfz
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

	; dist = poscheck.th_sh - poscheck.tfz
	sec
	lda	THMOVE_TH_SH_L
	sbc	THMOVE_TFZ_L
	sta	MATH_VAR_L
	lda	THMOVE_TH_SH_H
	sbc	THMOVE_TFZ_H
	sta	MATH_VAR_H
	bmi	@done

	; poscheck.midsec = sec - map_sectors
	lda	ARG_THING_SECTOR
	sta	THMOVE_MIDSEC

@no_mid:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

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

	; dist = poscheck.th_sh - poscheck.tfz
	sec
	lda	THMOVE_TH_SH_L
	sbc	THMOVE_TFZ_L
	sta	MATH_VAR_L
	lda	THMOVE_TH_SH_H
	sbc	THMOVE_TFZ_H
	sta	MATH_VAR_H
	bmi	@done

	; dist = poscheck.tcz - poscheck.th_zh
	sec
	lda	THMOVE_TCZ_L
	sbc	THMOVE_TH_ZH_L
	lda	THMOVE_TCZ_H
	sbc	THMOVE_TH_ZH_H
	bmi	@done

	; poscheck.tcz - poscheck.floorz
	sec
	lda	THMOVE_TCZ_L
	sbc	THMOVE_FLOORZ_L
	sta	MATH_VAR_L
	lda	THMOVE_TCZ_H
	sbc	THMOVE_FLOORZ_H
	sta	MATH_VAR_H
	sec
	lda	MATH_VAR_L
	sbc	THMOVE_HEIGHT
	lda	MATH_VAR_H
	sbc	#0
	bmi	@done

	; poscheck.ceilingz - poscheck.tfz
	sec
	lda	THMOVE_CEILINGZ_L
	sbc	THMOVE_TFZ_L
	sta	MATH_VAR_L
	lda	THMOVE_CEILINGZ_H
	sbc	THMOVE_TFZ_H
	sta	MATH_VAR_H
	sec
	lda	MATH_VAR_L
	sbc	THMOVE_HEIGHT
	lda	MATH_VAR_H
	sbc	#0

@done:
	rts

;;;
; check radius distance
;;;

check_radist:
	; if(dist >= 0)
	bmi	@negative

	; if(dist - radius >= 0)
	sec
	lda	THMOVE_DIFF_L
	sbc	THMOVE_RADIUS_L
	lda	THMOVE_DIFF_H
	sbc	THMOVE_RADIUS_H

	rts

@negative:
	; if(dist + radius < 0)
	clc
	lda	THMOVE_DIFF_L
	adc	THMOVE_RADIUS_L
	lda	THMOVE_DIFF_H
	adc	THMOVE_RADIUS_H
	eor	#$FF

	rts

;;;
; add sector(s) to portals
;;;
; X - the sector

add_sector:
	; base
	jsr	add_sector_raw

	; links
	lda	THMOVE_TOUCH
	beq	addsec_skip_links

add_sector_links:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; floor
	lda	SECTOR_STRUCT_FLOOR_LINK,x
	beq	@no_flr

	; add
	phx
	tax
	stx	THMOVE_ISLINK
	jsr	add_sector_raw
	plx

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

@no_flr:
	; ceiling
	lda	SECTOR_STRUCT_CEILING_LINK,x
	beq	@no_clg

	; add
	phx
	tax
	stx	THMOVE_ISLINK
	jsr	add_sector_raw
	plx

@no_clg:
	stz	THMOVE_ISLINK
addsec_skip_links:
	rts

add_sector_raw:
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

	; portals[i].midhit |= poscheck.midhit
	lda	MEM_PORTALS_MIDHIT,y
	ora	THMOVE_MIDHIT
	sta	MEM_PORTALS_MIDHIT,y

	rts

@do_save:
	; portals[portal_wr].sector = sdx
	ldy	THMOVE_PORTAL_WR
	txa
	sta	MEM_PORTALS_SECTOR,y
	lda	THMOVE_ISLINK
	sta	MEM_PORTALS_ISLINK,y
	lda	THMOVE_TOUCH
	sta	MEM_PORTALS_TOUCH,y
	lda	THMOVE_MIDHIT
	sta	MEM_PORTALS_MIDHIT,y

	; portal_wr++
	inc	THMOVE_PORTAL_WR

	rts

;;;
; place to sector
;;;
; ARG_THING_SECTOR - desired sector
; ARG_THING_IDX - desired thing
; THMOVE_PTR_H - slot index pointer in thing, auto increment
; THMOVE_MIDHIT - mid hit flag

place_to_sector:
	; check pointer slot
	lda	THMOVE_PTR_H
	cmp	#$C0
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

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing pointer
	tya
	ldy	ARG_THING_IDX

	; add this sector to thing
	sta	(THMOVE_PTR_L),y

	; add slot index to thing (pointer)
	lda	THMOVE_PTR_H
	sta	@opcode+2
	lda	HI_PTR_H
	and	#31
	ora	THMOVE_MIDHIT
@opcode:
	sta	$B080,y	; modified address

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
; swap thing main sector
;;;
; Y - thing index
; ARG_THING_SECTOR - new main sector

swap_thing_sector:
	phy

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; find slot
	ldx	#15
	lda	#$B1
	sta	TMP_PTR_H
	stz	TMP_PTR_L
@loop:
	lda	(TMP_PTR_L),y
	beq	@stop
	cmp	ARG_THING_SECTOR
	beq	@found

	; next
	inc	TMP_PTR_H
	dex
	bne	@loop
@stop:
	ply
	rts

@found:
	; swap sector
	lda	$B000,y
	sta	(TMP_PTR_L),y
	lda	ARG_THING_SECTOR
	sta	$B000,y

	; swap index
	lda	#$80
	sta	TMP_PTR_L
	ldx	$B080,y
	lda	(TMP_PTR_L),y
	sta	$B080,y
	txa
	sta	(TMP_PTR_L),y

	; done
	ply
	rts

;;;
; thing ticker
;;;

thing_tick:
	sty	ARG_THING_IDX

	;; camera
	cpy	G_CAMERA_THING
	bne	@skip_camera

	; projection.wh += projection.wd
	clc
	lda	G_VIEWDELTA
	beq	@skip_camera
	adc	G_VIEWHEIGHT
	bcs	:+
	sta	G_VIEWHEIGHT
	lda	THING_STRUCT_VIEW_HEIGHT,y
	cmp	G_VIEWHEIGHT
	bcs	@skip_camera
:
	; projection.wh = th->view_height
	sta	G_VIEWHEIGHT

	; projection.wd = 0
	stz	G_VIEWDELTA

@skip_camera:
	; th->iflags &= ~THING_IFLAG_BLOCKED
	lda	THING_STRUCT_IFLAGS,y
	and	#<~THING_IFLAG_BLOCKED
	sta	THING_STRUCT_IFLAGS,y

	;; XY movement
	lda	THING_STRUCT_MX_S,y
	ora	THING_STRUCT_MX_L,y
	ora	THING_STRUCT_MY_S,y
	ora	THING_STRUCT_MY_L,y
	bne	@do_xy_move

	; if(th->iflags & THING_IFLAG_HEIGHTCHECK)
	lda	THING_STRUCT_IFLAGS,y
;	bpl	@no_heights

	; th->iflags &= ~THING_IFLAG_HEIGHTCHECK
	and	#<~THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,y

	; read X
	lda	THING_STRUCT_X_L,y
	sta	ARG_THING_X_L
	lda	THING_STRUCT_X_H,y
	sta	ARG_THING_X_H

	; read Y
	lda	THING_STRUCT_Y_L,y
	sta	ARG_THING_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	ARG_THING_Y_H

	; read Z
	lda	THING_STRUCT_Z_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_Z_H,y
	sta	ARG_THING_Z_H

	; new heights
	jsr	thing_check_heights
	ldx	ARG_THING_IDX
	jsr	thing_apply_heights
	ldy	ARG_THING_IDX

@no_heights:
	jmp	@no_xy_move

@do_xy_move:
	stz	THTICK_RETRY
@xy_retry:
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
	jsr	thing_check_pos

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; check result
	lda	THMOVE_SECTOR
	beq	:+
	jmp	@xy_pass
:
	; restore
	ldx	ARG_THING_IDX

	; check retry
	lda	THTICK_RETRY
	beq	@skip_wiggle

	; stop
	stz	THING_STRUCT_MX_S,x
	stz	THING_STRUCT_MX_L,x
	stz	THING_STRUCT_MY_S,x
	stz	THING_STRUCT_MY_L,x

	; ang = level_tick << 5
	lda	G_LEVEL_TICK_L
	ror
	ror
	ror
	ror
	and	#$E0
	tax
	phx

	; X
	math_read_sin	MATH_VAR_L, MATH_VAR_H
	tay
	lda	TAB_SIGN_EXT,y
	sta	MATH_VAR_U
	ldx	ARG_THING_IDX
	clc
	lda	THING_STRUCT_X_S,x
	adc	MATH_VAR_L
	sta	ARG_THING_X_S
	lda	THING_STRUCT_X_L,x
	adc	MATH_VAR_H
	sta	ARG_THING_X_L
	lda	THING_STRUCT_X_H,x
	adc	MATH_VAR_U
	sta	ARG_THING_X_H

	; Y
	plx
	math_read_cos	MATH_VAR_L, MATH_VAR_H
	tay
	lda	TAB_SIGN_EXT,y
	sta	MATH_VAR_U
	ldx	ARG_THING_IDX
	clc
	lda	THING_STRUCT_Y_S,x
	adc	MATH_VAR_L
	sta	ARG_THING_Y_S
	lda	THING_STRUCT_Y_L,x
	adc	MATH_VAR_H
	sta	ARG_THING_Y_L
	lda	THING_STRUCT_Y_H,x
	adc	MATH_VAR_U
	sta	ARG_THING_Y_H

	; check new position
	stz	ARG_THING_SECTOR
	jsr	thing_check_pos

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; check result
	lda	THMOVE_SECTOR
	beq	:+
	jmp	@xy_pass
:
	jmp	@no_xy_friction

@skip_wiggle:
	; th->iflags |= THING_IFLAG_BLOCKED
	lda	THING_STRUCT_IFLAGS,x
	ora	#THING_IFLAG_BLOCKED
	sta	THING_STRUCT_IFLAGS,x

	; check projectile flag
	lda	THING_STRUCT_EFLAGS,x
	bpl	:+

@just_stop:
	; stop
	stz	THING_STRUCT_MX_S,x
	stz	THING_STRUCT_MX_L,x
	stz	THING_STRUCT_MY_S,x
	stz	THING_STRUCT_MY_L,x

	jmp	@no_xy_friction
:
	; check fail hit
	lda	THMOVE_HTYPE
	beq	@just_stop

	; get speed and angle
	lda	THING_STRUCT_MX_S,x
	sta	MATH_P2A_X_L
	lda	THING_STRUCT_MX_L,x
	sta	MATH_P2A_X_H
	lda	THING_STRUCT_MY_S,x
	sta	MATH_P2A_Y_L
	lda	THING_STRUCT_MY_L,x
	sta	MATH_P2A_Y_H
	jsr	math_p2d

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; restore
	ldx	ARG_THING_IDX

	; ang = 0x40 - (poscheck.hitang - p2a_coord.a)
	sec
	lda	THMOVE_HITANG
	sbc	MATH_VAR_U
	sta	MATH_VAR_U
	sec
	lda	#$40
	sbc	MATH_VAR_U
	sta	MATH_VAR_U

	;; dist *= ang * 4

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_L
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_M

	; ang
	ldy	MATH_VAR_U
	sty	VERA_FX_CACHE_H
	lda	TAB_SIGN_EXT,y
	sta	VERA_FX_CACHE_U

	; accumulate 3x
	lda	VERA_FX_ACCUM
	lda	VERA_FX_ACCUM
	lda	VERA_FX_ACCUM

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_M

	;; vect.x = (tab_sin[poscheck.hitang] * dist) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; tab_sin
	ldy	THMOVE_HITANG
	lda	TAB_SIN_L,y
	sta	VERA_FX_CACHE_H
	lda	TAB_SIN_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	THING_STRUCT_MX_S,x
	lda	VERA_DATA0
	sta	THING_STRUCT_MX_L,x

	;; vect.y = (tab_cos[poscheck.hitang] * dist) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; tab_cos
	ldy	THMOVE_HITANG
	lda	TAB_COS_L,y
	sta	VERA_FX_CACHE_H
	lda	TAB_COS_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	THING_STRUCT_MY_S,x
	lda	VERA_DATA0
	sta	THING_STRUCT_MY_L,x

	; retry
	inc	THTICK_RETRY
	ldy	ARG_THING_IDX
	jmp	@xy_retry

@xy_pass:
	; apply
	jsr	thing_apply_pos
	lda	ARG_THING_X_S
	sta	THING_STRUCT_X_S,x
	lda	ARG_THING_X_L
	sta	THING_STRUCT_X_L,x
	lda	ARG_THING_X_H
	sta	THING_STRUCT_X_H,x
	lda	ARG_THING_Y_S
	sta	THING_STRUCT_Y_S,x
	lda	ARG_THING_Y_L
	sta	THING_STRUCT_Y_L,x
	lda	ARG_THING_Y_H
	sta	THING_STRUCT_Y_H,x

	;; XY friction

	; check flag
	lda	THING_STRUCT_EFLAGS,x
	and	#THING_EFLAG_PROJECTILE
	bne	@no_xy_friction

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
	; restore
	ldy	ARG_THING_IDX

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
	sta	MATH_VAR_L
	lda	ARG_THING_Z_H
	sbc	THING_STRUCT_FLOORZ_H,y
	bpl	@not_in_floor

	; camera check
	cpy	G_CAMERA_THING
	bne	@no_floor_cam

	; camera bump
	sta	MATH_VAR_H

	; if(th->mz >= 0)
	lda	THING_STRUCT_MZ_L,y
	bpl	:+

	; diff = th->mz >> 10
	sec
	ror
	sec
	ror
	sta	MATH_VAR_L
:
	; diff = projection.wh + diff
	clc
	lda	G_VIEWHEIGHT
	adc	MATH_VAR_L
	sta	MATH_VAR_U
	lda	#0
	adc	MATH_VAR_H

	; if(diff < 0)
	bpl	:+
	stz	G_VIEWHEIGHT
	bra	@force_dip
:
	; if((uint8_t)diff <= projection.wh)
	lda	MATH_VAR_U
	cmp	G_VIEWHEIGHT
	bcs	@no_floor_cam
	sta	G_VIEWHEIGHT

@force_dip:
	; projection.wd = (th->view_height - projection.wh) >> 1
	sec
	lda	THING_STRUCT_VIEW_HEIGHT,y
	sbc	G_VIEWHEIGHT
	sta	G_VIEWDELTA
	lsr	G_VIEWDELTA
	bne	:+
	inc	G_VIEWDELTA
:
@no_floor_cam:
	; under floor
	lda	#0
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y
	stz	ARG_THING_Z_S
	lda	THING_STRUCT_FLOORZ_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_FLOORZ_H,y
	sta	ARG_THING_Z_H

@not_in_floor:
	; apply Z
	lda	ARG_THING_Z_S
	sta	THING_STRUCT_Z_S,y
	lda	ARG_THING_Z_L
	sta	THING_STRUCT_Z_L,y
	lda	ARG_THING_Z_H
	sta	THING_STRUCT_Z_H,y

	;; links

	; view Z
	clc
	lda	THING_STRUCT_VIEW_HEIGHT,y
	adc	ARG_THING_Z_L
	sta	MATH_VAR_L
	lda	#0
	adc	ARG_THING_Z_H
	sta	MATH_VAR_H

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector
	ldx	$B000,y

	; <sector>
	phx

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; floor
	lda	SECTOR_STRUCT_FLOOR_LINK,x
	beq	@no_floor
	sta	ARG_THING_SECTOR

	; check view Z
	sec
	lda	MATH_VAR_L
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	lda	MATH_VAR_H
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	bpl	@no_floor

	; swap sectors
	jsr	swap_thing_sector

	; <sector>
	pla
	lda	ARG_THING_SECTOR
	pha

	bra	@no_ceil

@no_floor:
	lda	SECTOR_STRUCT_CEILING_LINK,x
	beq	@no_ceil
	sta	ARG_THING_SECTOR

	; check view Z
	sec
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sbc	MATH_VAR_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sbc	MATH_VAR_H
	bpl	@no_ceil

	; swap sectors
	jsr	swap_thing_sector

	; <sector>
	pla
	lda	ARG_THING_SECTOR
	pha

@no_ceil:
	;; water

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; <sector>
	plx

	; if(sec->flags & SECTOR_FLAG_WATER)
	lda	SECTOR_STRUCT_FLAGS,x
	bpl	@skip_water

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; if(th->eflags & THING_EFLAG_WATERSPEC)
	lda	THING_STRUCT_EFLAGS,y
	and	#THING_EFLAG_WATERSPEC
	beq	@do_z_friction

	; gravity
	lda	THING_STRUCT_GRAVITY,y
	sta	MATH_VAR_L
	stz	MATH_VAR_H

	; get type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; direction check
	lda	THTYPE_STRUCT_WATER_HEIGHT,x
	cmp	THTYPE_STRUCT_VIEW_HEIGHT,x
	bcc	:+

	lda	#$00
	sbc	MATH_VAR_L
	sta	MATH_VAR_L
	bcs	:+
	dec	MATH_VAR_H
:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; add
	clc
	lda	THING_STRUCT_MZ_S,y
	adc	MATH_VAR_L
	sta	THING_STRUCT_MZ_S,y
	lda	THING_STRUCT_MZ_L,y
	adc	MATH_VAR_H
	sta	THING_STRUCT_MZ_L,y

	bra	@no_z_friction

@skip_water:
	;; gravity

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; gravity check
	ldx	THING_STRUCT_GRAVITY,y
	beq	@do_z_friction

	; on floor check
	sec
	lda	THING_STRUCT_FLOORZ_L,y
	sbc	ARG_THING_Z_L
	lda	THING_STRUCT_FLOORZ_H,y
	sbc	ARG_THING_Z_H
	bpl	@no_z_friction

	; th->mz -= th->gravity << 4
	lda	TAB_SWAP,x
	and	#$F0
	sta	MATH_VAR_L
	lda	TAB_SWAP,x
	and	#$0F
	sta	MATH_VAR_H
	sec
	lda	THING_STRUCT_MZ_S,y
	sbc	MATH_VAR_L
	sta	THING_STRUCT_MZ_S,y
	lda	THING_STRUCT_MZ_L,y
	sbc	MATH_VAR_H
	sta	THING_STRUCT_MZ_L,y

	bra	@no_z_friction

@do_z_friction:
	; restore
	ldx	ARG_THING_IDX

	; MZ
	lda	THING_STRUCT_MZ_L,x
	beq	:++
	cmp	#$FF
	beq	:+
	rol
	ror	THING_STRUCT_MZ_L,x
	ror	THING_STRUCT_MZ_S,x
	bra	:+++
:
	stz	THING_STRUCT_MZ_L,x
:
	stz	THING_STRUCT_MZ_S,x
:
@no_z_friction:

;;;
; thing animation
;;;

thing_anim:
	; if(!th->ticks)
	lda	THING_STRUCT_TICKS,y
	bne	:+
	rts
:
	; th->ticks--
	dea

	; if(th->ticks)
	beq	:+
	sta	THING_STRUCT_TICKS,y
	rts
:
	; state
	ldx	THING_STRUCT_NXTFRM_L,y
	lda	THING_STRUCT_NXTFRM_H,y
	and	#$07

thing_nextfrm:
	; check for STOP
	cmp	#0
	bne	:+
	cpx	#0
	bne	:+

	; delete
	stz	THMOVE_SECTOR
	jsr	thing_apply_pos
	jmp	tick_del
:
	; thing states bank
	ora	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; action
	lda	THSTATE_STRUCT_ACTION,x
	asl
	beq	:+

	; run action
	;sta	DBG_BYTE0
:
	; fullbright
	lda	THSTATE_STRUCT_ACTION,x
	and	#$80
	sta	MATH_VAR_H

	; next H
	ldy	THSTATE_STRUCT_FRM_NXT,x
	lda	TAB_BANK,y
	tsb	MATH_VAR_H

	; next L
	lda	THSTATE_STRUCT_NEXT,x
	pha	; <next L>

	; spawn sprite
	ldy	THSTATE_STRUCT_SPRITE,x
	bmi	:+

	; sprite and frame
	lda	THSTATE_STRUCT_FRM_NXT,x
	and	#$1F
	clc
	adc	TAB_SPRITE_REMAP,y
:
	pha	; <sprite>

	; ticks
	lda	THSTATE_STRUCT_TICKS,x
	pha	; <ticks>

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; ticker
	ldy	ARG_THING_IDX

	; <ticks>
	pla
	sta	THING_STRUCT_TICKS,y

	; <sprite>
	pla
	sta	THING_STRUCT_SPRITE,y

	; <next L>
	pla
	sta	THING_STRUCT_NXTFRM_L,y

	; next H
	lda	MATH_VAR_H
	sta	THING_STRUCT_NXTFRM_H,y

	rts

;;;
; player ticker
;;;

thing_plyr:
	; ntype = THING_TYPE_PLAYER_N
	lda	#THING_TYPE_PLAYER_N
	sta	PLTICK_NTYPE

	; if(!th->gravity)
	lda	THING_STRUCT_GRAVITY,y
	bne	:+

	; ntype = THING_TYPE_PLAYER_F
	lda	#THING_TYPE_PLAYER_F
	sta	PLTICK_NTYPE
:
	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector
	ldx	$B000,y
	stx	PLTICK_SECTOR

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; floor link
	lda	SECTOR_STRUCT_FLOOR_LINK,x
	sta	PLTICK_SEC_FLINK

	; flags
	lda	SECTOR_STRUCT_FLAGS,x
	sta	PLTICK_SEC_FLAGS

	; if(sec->flags & SECTOR_FLAG_WATER)
	bpl	:+

	; ntype = THING_TYPE_PLAYER_S
	lda	#THING_TYPE_PLAYER_S
	sta	PLTICK_NTYPE
:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; check freeze
	lda	THING_STRUCT_COUNTER,y
	beq	@no_freeze

	; ticcmd.angle = th->angle
	lda	THING_STRUCT_ANGLE,y
	sta	TICK_CMD_ANGLE

	; ticcmd.pitch = th->pitch
	lda	THING_STRUCT_PITCH,y
	sta	TICK_CMD_PITCH

	; done
	jmp	@skip_move

@no_freeze:
	; get type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; speed
	lda	THTYPE_STRUCT_SPEED,x
	sta	PLTICK_SPEED

	; jump
	lda	THTYPE_STRUCT_JUMPZ,x
	sta	PLTICK_JUMPZ

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->angle = ticcmd.angle
	lda	TICK_CMD_ANGLE
	sta	THING_STRUCT_ANGLE,y

	; th->pitch = ticcmd.pitch
	lda	TICK_CMD_PITCH
	sta	THING_STRUCT_PITCH,y

	; if(ticcmd.bits_l & TCMD_GO_UP)
	bit	TICK_CMD_BITS_L
	bvs	@try_up

	; th->iflags &= ~THING_IFLAG_JUMPED
	lda	THING_STRUCT_IFLAGS,y
	and	#<~THING_IFLAG_JUMPED
	sta	THING_STRUCT_IFLAGS,y

	bra	@skip_up

@try_up:
	; if(sec->flags & SECTOR_FLAG_WATER)
	lda	PLTICK_SEC_FLAGS
	bmi	@go_up

	; th->mz >= 0
	lda	THING_STRUCT_MZ_L,y
	bmi	@skip_up

	; !(th->iflags & THING_IFLAG_JUMPED)
	lda	THING_STRUCT_IFLAGS,y
	and	#THING_IFLAG_JUMPED
	bne	@skip_up

	; th->floorz - (th->z / 256) >= 0
	sec
	lda	THING_STRUCT_FLOORZ_L,y
	sbc	THING_STRUCT_Z_L,y
	lda	THING_STRUCT_FLOORZ_H,y
	sbc	THING_STRUCT_Z_H,y
	bmi	@skip_up

	; th->floort
	lda	THING_STRUCT_FLOORT,y
	bne	@do_jump

	; !sec->floor.link
	lda	PLTICK_SEC_FLINK
	beq	@do_jump

	; th->floors != thingsec[tick_idx][0]
	lda	THING_STRUCT_FLOORS,y
	cmp	PLTICK_SECTOR
	bne	@do_jump

	; th->iflags & THING_IFLAG_BLOCKED
	lda	THING_STRUCT_IFLAGS,y
	lsr
	bcc	@skip_up

@do_jump:
	; th->iflags |= THING_IFLAG_JUMPED
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_JUMPED
	sta	THING_STRUCT_IFLAGS,y

@go_up:
	; th->mz += jump << 8
	clc
	lda	THING_STRUCT_MZ_L,y
	adc	PLTICK_JUMPZ
	sta	THING_STRUCT_MZ_L,y

@skip_up:
	; if(ticcmd.bits_l & TCMD_GO_DOWN)
	lda	TICK_CMD_BITS_L
	ror
	bcc	@skip_down

	; if(sec->flags & SECTOR_FLAG_WATER)
	lda	PLTICK_SEC_FLAGS
	bpl	:+

	; th->mz -= jump << 8
	sec
	lda	THING_STRUCT_MZ_L,y
	sbc	PLTICK_JUMPZ
	sta	THING_STRUCT_MZ_L,y

	bra	@skip_down
:
	; ntype = THING_TYPE_PLAYER_C
	lda	#THING_TYPE_PLAYER_C
	sta	PLTICK_NTYPE

@skip_down:
	; if(ticcmd.bits & TCMD_MOVING)
	lda	TICK_CMD_BITS_L
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
	; if(th->ticker.type != ntype)
	lda	PLTICK_NTYPE
	cmp	TICKER_STRUCT_TYPE,y
	bne	:+
	jmp	@no_change
:
	tax

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; height
	lda	THTYPE_STRUCT_HEIGHT,x
	sta	PLTICK_HEIGHT

	; view height
	lda	THTYPE_STRUCT_VIEW_HEIGHT,x
	sta	PLTICK_VIEWH

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; diff = th->ceilingz - th->floorz + th->height
	sec
	lda	THING_STRUCT_CEILZ_L,y
	sbc	THING_STRUCT_FLOORZ_L,y
	sta	MATH_VAR_L
	lda	THING_STRUCT_CEILZ_H,y
	sbc	THING_STRUCT_FLOORZ_H,y
	sta	MATH_VAR_H
	clc
	lda	MATH_VAR_L
	adc	THING_STRUCT_HEIGHT,y
	sta	MATH_VAR_L
	bcc	:+
	inc	MATH_VAR_H
:
	; if(ti->height - diff < 0)
	sec
	lda	PLTICK_HEIGHT
	sbc	MATH_VAR_L
	lda	#0
	sbc	MATH_VAR_H
	bpl	@no_change

	; diff = th->view_height - ti->view_height
	sec
	lda	THING_STRUCT_VIEW_HEIGHT,y
	sbc	PLTICK_VIEWH
	tax

	; nz = th->z / 256 + diff
	clc
	adc	THING_STRUCT_Z_L,y
	sta	PLTICK_NZ_L
	lda	TAB_SIGN_EXT,x
	adc	THING_STRUCT_Z_H,y
	sta	PLTICK_NZ_H

	; diff = nz + ti->height - th->height
	clc
	lda	PLTICK_NZ_L
	adc	PLTICK_HEIGHT
	sta	MATH_VAR_L
	lda	PLTICK_NZ_H
	adc	#0
	sta	MATH_VAR_H
	sec
	lda	MATH_VAR_L
	sbc	THING_STRUCT_HEIGHT,y
	sta	MATH_VAR_L
	bcs	:+
	dec	MATH_VAR_H
:
	; if(th->ceilingz - diff >= 0)
	sec
	lda	THING_STRUCT_CEILZ_L,y
	sbc	MATH_VAR_L
	lda	THING_STRUCT_CEILZ_H,y
	sbc	MATH_VAR_H
	bmi	@no_change

	; th->z = (nz * 256) | (th->z & 0xFF)
	lda	PLTICK_NZ_L
	sta	THING_STRUCT_Z_L,y
	lda	PLTICK_NZ_H
	sta	THING_STRUCT_Z_H,y

	; th->height = ti->height
	lda	PLTICK_HEIGHT
	sta	THING_STRUCT_HEIGHT,y

	; th->view_height = ti->view_height
	lda	PLTICK_VIEWH
	sta	THING_STRUCT_VIEW_HEIGHT,y

	; projection.wh = ti->view_height
	sta	G_VIEWHEIGHT

	; th->iflags |= THING_IFLAG_HEIGHTCHECK
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,y

	; th->ticker.type = ntype
	lda	PLTICK_NTYPE
	sta	TICKER_STRUCT_TYPE,y

@no_change:
	; thing tick
	jsr	thing_tick

	; weapon
	ldy	#0
	sty	ARG_THING_IDX
	jmp	thing_anim

