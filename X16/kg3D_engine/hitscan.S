.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "tables.inc"
.include "math.inc"
.include "zeropage.inc"
.include "engine.inc"
.include "things.inc"
.include "actions.inc"

.export hitscan_func
.export hitscan_attack

;;;
; NOTES: hitscan callback
;;;
; Hitscan callback is entered with wall bank set.
; WALL_PTR_L is current wall.
; ARG_THING_SPAWN_SECTOR is current sector.
; Set carry to STOP checking.
; Wall bank must be restored on exit, if carry is cleared.

;;;
; CODE
;;;

.segment "CODE"

;;;
; bullet attack callback
;;;

cb_hitscan:
	;; dist = (d0.x * wall->solid.dist.y - d0.y * wall->solid.dist.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.x = vtx->x - hitscan.x
	ldy	#WALL_STRUCT_VTX_X
	sec
	lda	(WALL_PTR_L),y
	sbc	ARG_THING_SPAWN_X_L
	sta	VERA_FX_CACHE_L
	iny
	lda	(WALL_PTR_L),y
	sbc	ARG_THING_SPAWN_X_H
	sta	VERA_FX_CACHE_M

	; wall->solid.dist.y
	ldy	#WALL_STRUCT_DIST_Y
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_H
	iny
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; d0.y = hitscan.y - vtx->y
	sec
	ldy	#WALL_STRUCT_VTX_Y
	lda	ARG_THING_SPAWN_Y_L
	sbc	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_L
	iny
	lda	ARG_THING_SPAWN_Y_H
	sbc	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_M

	; wall->solid.dist.x
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
	sta	VERA_FX_CACHE_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_M

	; angle = wall->solid.angle >> 4
	ldy	#WALL_STRUCT_ANGLE
	lda	(WALL_PTR_L),y
	tax
	lda	TAB_SWAP,x
	and	#$0F
	sta	HITSCAN_HIT_ANGLE
	iny
	lda	(WALL_PTR_L),y
	tax
	lda	TAB_SWAP,x
	and	#$F0
	tsb	HITSCAN_HIT_ANGLE

	; angle -= hitscan.angle
	sec
	lda	HITSCAN_HIT_ANGLE
	sbc	ARG_HITSCAN_ANGLE
	sta	HITSCAN_HIT_ANGLE
	tax

	; d0.y = hitscan.y
	; d0.y -= (wall->solid.dist.x * dist) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; wall->solid.dist.x is already set

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	sec
	lda	ARG_THING_SPAWN_Y_L
	sbc	VERA_DATA0
	sta	HITSCAN_D0_Y_L
	lda	ARG_THING_SPAWN_Y_H
	sbc	VERA_DATA0
	sta	HITSCAN_D0_Y_H

	; d0.x = hitscan.x
	; d0.x += (wall->solid.dist.y * dist) >> 8

	; dist is already set

	; wall->solid.dist.y
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
	clc
	lda	ARG_THING_SPAWN_X_L
	adc	VERA_DATA0
	sta	HITSCAN_D0_X_L
	lda	ARG_THING_SPAWN_X_H
	adc	VERA_DATA0
	sta	HITSCAN_D0_X_H

	;; dist *= tab_tan_hs[angle]

	; dist is already set

	; tan
	lda	TAB_HTAN_L,x
	sta	VERA_FX_CACHE_H
	lda	TAB_HTAN_H,x
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_M

	;; d0.x += (wall->solid.dist.x * dist) >> 8

	; dist is already set

	; wall->solid.dist.x
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
	lda	HITSCAN_D0_X_L
	adc	VERA_DATA0
	sta	HITSCAN_D0_X_L
	lda	HITSCAN_D0_X_H
	adc	VERA_DATA0
	sta	HITSCAN_D0_X_H

	;; d0.y += (wall->solid.dist.y * dist) >> 8

	; dist is already set

	; wall->solid.dist.y
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
	clc
	lda	HITSCAN_D0_Y_L
	adc	VERA_DATA0
	sta	HITSCAN_D0_Y_L
	lda	HITSCAN_D0_Y_H
	adc	VERA_DATA0
	sta	HITSCAN_D0_Y_H

	; if(hitscan.axis & 0x80)
	lda	HITSCAN_AXIS
	bpl	@aY

	; dist = d0.x - hitscan.x
	sec
	lda	HITSCAN_D0_X_L
	sbc	ARG_THING_SPAWN_X_L
	sta	VERA_FX_CACHE_L
	lda	HITSCAN_D0_X_H
	sbc	ARG_THING_SPAWN_X_H
	sta	VERA_FX_CACHE_M

	bra	@askip
@aY:
	; dist = d0.y - hitscan.y
	sec
	lda	HITSCAN_D0_Y_L
	sbc	ARG_THING_SPAWN_Y_L
	sta	VERA_FX_CACHE_L
	lda	HITSCAN_D0_Y_H
	sbc	ARG_THING_SPAWN_Y_H
	sta	VERA_FX_CACHE_M

@askip:
	;; dist = dist * hitscan.idiv * 2

	; dist is already set

	; hitscan.idiv
	lda	HITSCAN_IDIV_L
	sta	VERA_FX_CACHE_H
	lda	HITSCAN_IDIV_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_M

	;; dist *= hitscan.wtan

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; hitscan.wtan
	lda	HITSCAN_WTAN_L
	sta	VERA_FX_CACHE_H
	lda	HITSCAN_WTAN_H
	sta	VERA_FX_CACHE_U

	;; zz = hitscan.z + (dist >> 8)

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	clc
	lda	VERA_DATA0
	adc	ARG_THING_SPAWN_Z_L
	sta	HITSCAN_ZZ_L
	lda	VERA_DATA0
	adc	ARG_THING_SPAWN_Z_H
	sta	HITSCAN_ZZ_H

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; sector
	ldx	ARG_THING_SPAWN_SECTOR

	; dd = sec->floor.height
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	MATH_VAR_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sta	MATH_VAR_H

	; check floor
	sec
	lda	HITSCAN_ZZ_L
	sbc	MATH_VAR_L
	lda	HITSCAN_ZZ_H
	sbc	MATH_VAR_H
	bpl	@no_floor

	; zz = sec->floor.height
	lda	MATH_VAR_L
	sta	HITSCAN_ZZ_L
	lda	MATH_VAR_H
	sta	HITSCAN_ZZ_H

	bra	@in_plane

@no_floor:
	; dd = sec->ceiling.height
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sta	MATH_VAR_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sta	MATH_VAR_H

	; check ceiling
	sec
	lda	MATH_VAR_L
	sbc	HITSCAN_ZZ_L
	lda	MATH_VAR_H
	sbc	HITSCAN_ZZ_H
	bpl	@no_ceiling

	; zz = sec->ceiling.height - hitscan.height
	sec
	lda	MATH_VAR_L
	sbc	HITSCAN_HEIGHT
	sta	HITSCAN_ZZ_L
	lda	MATH_VAR_H
	sbc	#0
	sta	HITSCAN_ZZ_H

@in_plane:
	; dist = hitscan.z - dd
	sec
	lda	ARG_THING_SPAWN_Z_L
	sbc	MATH_VAR_L
	sta	VERA_FX_CACHE_L
	lda	ARG_THING_SPAWN_Z_H
	sbc	MATH_VAR_H
	sta	VERA_FX_CACHE_M

	;; dist *= hitscan.ptan

	; dist is already set

	; hitscan.ptan
	lda	HITSCAN_PTAN_L
	sta	VERA_FX_CACHE_H
	lda	HITSCAN_PTAN_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_M

	;; hitscan.sin * dist

	; dist is already set

	; hitscan.sin
	lda	HITSCAN_SIN_L
	sta	VERA_FX_CACHE_H
	lda	HITSCAN_SIN_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; d0.x = hitscan.x + (result >> 8)
	clc
	lda	VERA_DATA0
	adc	ARG_THING_SPAWN_X_L
	sta	HITSCAN_D0_X_L
	lda	VERA_DATA0
	adc	ARG_THING_SPAWN_X_H
	sta	HITSCAN_D0_X_H

	; dist is already set

	; hitscan.sin
	lda	HITSCAN_COS_L
	sta	VERA_FX_CACHE_H
	lda	HITSCAN_COS_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; d0.y = hitscan.y + (result >> 8)
	clc
	lda	VERA_DATA0
	adc	ARG_THING_SPAWN_Y_L
	sta	HITSCAN_D0_Y_L
	lda	VERA_DATA0
	adc	ARG_THING_SPAWN_Y_H
	sta	HITSCAN_D0_Y_H

	jmp	@do_hit

@no_ceiling:
	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; check backsector and blocking
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_PORTAL
	beq	@no_bs
	ldy	#WALL_STRUCT_BLOCKING
	lda	(WALL_PTR_L),y
	and	HITSCAN_BLOCKBY
	bne	@no_bs
	dey
	lda	(WALL_PTR_L),y
	beq	@no_bs
	tax

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; zz > bs->floor.height
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sbc	HITSCAN_ZZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sbc	HITSCAN_ZZ_H
	bpl	@no_bs

	; zz < bs->ceiling.height
	sec
	lda	HITSCAN_ZZ_L
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	lda	HITSCAN_ZZ_H
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	bpl	@no_bs

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; no hit detected
	clc
	rts

@no_bs:
	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	;; d0.x -= (wall->solid.dist.y * hitscan.radius) >> 8

	; hitscan.radius
	lda	HITSCAN_RADIUS
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; wall->solid.dist.y
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
	sec
	lda	HITSCAN_D0_X_L
	sbc	VERA_DATA0
	sta	HITSCAN_D0_X_L
	lda	HITSCAN_D0_X_H
	sbc	VERA_DATA0
	sta	HITSCAN_D0_X_H

	;; d0.y += (wall->solid.dist.x * hitscan.radius) >> 8

	; hitscan.radius is already set

	; wall->solid.dist.x
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
	lda	HITSCAN_D0_Y_L
	adc	VERA_DATA0
	sta	HITSCAN_D0_Y_L
	lda	HITSCAN_D0_Y_H
	adc	VERA_DATA0
	sta	HITSCAN_D0_Y_H

@do_hit:
	; spawn puff
	stz	ARG_THING_SPAWN_X_S
	lda	HITSCAN_D0_X_L
	sta	ARG_THING_SPAWN_X_L
	lda	HITSCAN_D0_X_H
	sta	ARG_THING_SPAWN_X_H

	stz	ARG_THING_SPAWN_Y_S
	lda	HITSCAN_D0_Y_L
	sta	ARG_THING_SPAWN_Y_L
	lda	HITSCAN_D0_Y_H
	sta	ARG_THING_SPAWN_Y_H

	stz	ARG_THING_SPAWN_Z_S
	lda	HITSCAN_ZZ_L
	sta	ARG_THING_SPAWN_Z_L
	lda	HITSCAN_ZZ_H
	sta	ARG_THING_SPAWN_Z_H

	lda	#110
	sta	ARG_THING_SPAWN_TYPE

	jsr	thing_spawn

	sec
	rts

;;;
; get vetex angle
;;;
; returns angle in A

get_angle:
	; p2a_coord.x = v1->x - x
	sec
	ldy	#WALL_STRUCT_VTX_X
	lda	(WALN_PTR_L),y
	sbc	ARG_THING_SPAWN_X_L
	sta	MATH_P2A_X_L
	iny
	lda	(WALN_PTR_L),y
	sbc	ARG_THING_SPAWN_X_H
	sta	MATH_P2A_X_H

	; p2a_coord.y = v1->y - y
	sec
	iny
	lda	(WALN_PTR_L),y
	sbc	ARG_THING_SPAWN_Y_L
	sta	MATH_P2A_Y_L
	iny
	lda	(WALN_PTR_L),y
	sbc	ARG_THING_SPAWN_Y_H
	sta	MATH_P2A_Y_H

	; angle
	jsr	math_p2a
	math_lsr4_u16	MATH_VAR_L, MATH_VAR_H

	rts

;;;
; run 2D hitscan
;;;
; uses ARG_HITSCAN_ANGLE
; Y is thing IDX
; set hitscan_cb_wall+1 and hitscan_cb_wall+2

hitscan_func:
	; get XY
	jsr	action_get_XY

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector
	lda	$A000,y
	sta	ARG_THING_SPAWN_SECTOR

hs_sector_loop:
	; current sector
	ldx	ARG_THING_SPAWN_SECTOR

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

	; angle
	jsr	get_angle
	sta	HITSCAN_LAST_ANGLE

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
	sta	HITSCAN_WALL_FLAGS
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

	; angle
	jsr	get_angle
	sta	HITSCAN_NEW_ANGLE

	; hit = angle - thang
	sec
	sbc	ARG_HITSCAN_ANGLE
	sta	HITSCAN_HIT_ANGLE

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; hit |= thang - last_angle
	sec
	lda	ARG_HITSCAN_ANGLE
	sbc	HITSCAN_LAST_ANGLE
	ora	HITSCAN_HIT_ANGLE

	; check for hit
	bpl	hitscan_cb_wall

	; last_angle = angle
	lda	HITSCAN_NEW_ANGLE
	sta	HITSCAN_LAST_ANGLE

	; next
	bra	@loop_wall

hitscan_cb_wall:
	; run callback
	jsr	$0	; modified address
	bcc	:+
	rts
:
	; portal check
	lda	HITSCAN_WALL_FLAGS
	and	#WALL__MARK_PORTAL
	bne	:+
	rts
:
	; backsector check
	ldy	#WALL_STRUCT_BACKSECTOR
	lda	(WALL_PTR_L),y
	bne	:+
	rts
:
	sta	ARG_THING_SPAWN_SECTOR
	jmp	hs_sector_loop

;;;
; bullet attack
;;;
; uses ARG_HITSCAN_ANGLE, ARG_HITSCAN_HALF_PITCH, ARG_HITSCAN_TYPE
; Y is thing IDX
; X is view height

hitscan_attack:
	phy

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; get Z
	clc
	txa
	adc	THING_STRUCT_Z_L,y
	sta	ARG_THING_SPAWN_Z_L
	lda	THING_STRUCT_Z_H,y
	adc	#0
	sta	ARG_THING_SPAWN_Z_H

	; callback
	lda	#<cb_hitscan
	sta	hitscan_cb_wall+1
	lda	#>cb_hitscan
	sta	hitscan_cb_wall+2

	; angle
	ldx	ARG_HITSCAN_ANGLE

	; sin
	math_read_sin	HITSCAN_SIN_L, HITSCAN_SIN_H

	; cos
	math_read_cos	HITSCAN_COS_L, HITSCAN_COS_H

	; hitscan.axis = (hang + 0x20) << 1
	txa
	clc
	adc	#$20
	asl
	sta	HITSCAN_AXIS

	; if(hitscan.axis & 0x80)
	bpl	@aY

	; use hitscan.sin
	lda	HITSCAN_SIN_L
	sta	MATH_VAR_L
	lda	HITSCAN_SIN_H
	sta	MATH_VAR_H

	bra	@askip
@aY:
	; use hitscan.cos
	lda	HITSCAN_COS_L
	sta	MATH_VAR_L
	lda	HITSCAN_COS_H
	sta	MATH_VAR_H

@askip:
	; inverse division bank
	lda	#BANK_IDIV_L
	sta	REG_RAM_BANK

	; inv_div
	jsr	math_idiv

	; save hitscan.idiv
	lda	MATH_VAR_L
	sta	HITSCAN_IDIV_L
	lda	MATH_VAR_H
	sta	HITSCAN_IDIV_H

	; pitch
	ldx	ARG_HITSCAN_HALF_PITCH

	; wtan
	math_read_htan	HITSCAN_WTAN_L, HITSCAN_WTAN_H

	; if(halfpitch & 0x40)
	txa
	bit	ARG_HITSCAN_HALF_PITCH
	bvs	@up

	; halfpitch += 0x40
	clc
	adc	#$40

	bra	@skip
@up:
	; halfpitch -= 0x40
	sec
	sbc	#$40

@skip:
	; hitscan.ptan = tab_tan_hs[halfpitch]
	tax
	math_read_htan	HITSCAN_PTAN_L, HITSCAN_PTAN_H

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; type
	ldx	ARG_HITSCAN_TYPE

	; height
	lda	THTYPE_STRUCT_HEIGHT,x
	sta	HITSCAN_HEIGHT

	; radius + 1
	lda	THTYPE_STRUCT_HEIGHT,x
	ina
	sta	HITSCAN_RADIUS

	; blocked by
	lda	THTYPE_STRUCT_BLOCKEDBY,x
	sta	HITSCAN_BLOCKBY

	; run
	ply
	jmp	hitscan_func

