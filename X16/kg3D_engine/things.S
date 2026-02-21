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
.include "hitscan.inc"

.export thing_spawn
.export thing_spawn_org
.export thing_clear
.export thing_launch_ang
.export thing_launch
.export thing_check_pos
.export thing_check_heights
.export thing_apply_pos
.export thing_damage
.export thing_explode
.export thing_unmark_all
.export thing_actdone
.export thing_actnext

.export thing_tick_full
.export thing_tick_move
.export thing_tick_anim
.export thing_tick_plyr

;;;
; CODE
;;;

.segment "CODE"

;;;
; thing spawn
;;;
; requires all ARG_THING_* arguments
; Y is originator for '_org' variant
; zero flag set = fail
; returns thing in X and RET_THING_IDX

thing_spawn:
	ldy	#0
thing_spawn_org:
	phy	; <origin>

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
	; thing type
	ldx	ARG_THING_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; check spawn animation
	lda	THTYPE_ANIM_SPAWN_L,x
	ora	THTYPE_ANIM_SPAWN_H,x
	beq	@stop

	; new ticker
	jsr	tick_add
	beq	@stop

	; the thing
	stx	RET_THING_IDX

	; thing type
	ldy	ARG_THING_TYPE

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; thing mode
	lda	THTYPE_STRUCT_MODE,y
	sta	@opcode_mode+1

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
	ldx     RET_THING_IDX

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
	stz	THMOVE_MOVING
	jsr	thing_check_spawn
	beq	@pos_fail

	; place to sectors
	jsr	thing_apply_spawn

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

@finish:
	ldx	RET_THING_IDX

@opcode_mode:
	; mode
	lda	#$00	; modified value

	; if(ti->mode & 0x80)
	bpl	:+

	; th->iflags = THING_IFLAG_ALTRADIUS
	lda	#THING_IFLAG_ALTRADIUS
	sta	THING_STRUCT_IFLAGS,x
:
	; th->ticker.func = (ti->mode & 3) << 1
	and	#3
	asl
	sta	TICKER_STRUCT_FUNC,x

	; DONE
	ldx	RET_THING_IDX
	rts

@pos_fail:
	;; position check FAILED
	; place only to spawn sector
	; this ignores linked sectors
	; this ignores floor dist

	ldx	RET_THING_IDX

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

	; DONE
	bra	@finish

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
; damage thing
;;;
; Y - thing to damage
; X - damage origin thing
; A - thrust angle
; ARG_THING_DAMAGE_*

thing_damage:
	pha	; <angle>
	phx	; <origin>

	; check for camera damage
	cpy	G_CAMERA_THING
	bne	@no_camera

	; check damage HI
	lda	ARG_THING_DAMAGE_H
	bne	:+

	; add level
	clc
	lda	G_DAMAGE_LEVEL
	adc	ARG_THING_DAMAGE_L

	; check limit
	bcs	:+
	cmp	#255-14
	bcc	:++
:
	; max
	lda	#255-15
:
	sta	G_DAMAGE_LEVEL

@no_camera:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; th->damager = odx
	txa
	sta	THING_STRUCT_TH_DAMAGER,y

	; check health
	lda	THING_STRUCT_HEALTH_L,y
	ora	THING_STRUCT_HEALTH_H,y
	beq	@skip_death

	; apply damage
	sec
	lda	THING_STRUCT_HEALTH_L,y
	sbc	ARG_THING_DAMAGE_L
	sta	MATH_TEMP_A
	lda	THING_STRUCT_HEALTH_H,y
	sbc	ARG_THING_DAMAGE_H
	sta	MATH_TEMP_B
	bcc	:+
	ora	MATH_TEMP_A
	beq	:+

	; new health
	lda	MATH_TEMP_A
	sta	THING_STRUCT_HEALTH_L,y
	lda	MATH_TEMP_B
	sta	THING_STRUCT_HEALTH_H,y

	bra	@skip_death
:
	; death
	lda	#0
	sta	THING_STRUCT_HEALTH_L,y
	sta	THING_STRUCT_HEALTH_H,y

	; if(th->ticker.func < TICKER_FUNC_THING)
	lda	TICKER_STRUCT_FUNC,y
	cmp	#TICKER_FUNC_THING
	bcs	:+

	; th->ticker.func |= TICKER_FUNC_ANIM
	ora	#TICKER_FUNC_ANIM
	sta	TICKER_STRUCT_FUNC,y
:
	; thing type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; new animation
	lda	THTYPE_ANIM_DEATH_L,x
	pha	; <next frame LO>
	lda	THTYPE_ANIM_DEATH_H,x
	pha	; <next frame HI>

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; corpsify
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_CORPSE
	sta	THING_STRUCT_IFLAGS,y

	; can't push
	lda	THING_STRUCT_EFLAGS,y
	and	#<~THING_EFLAG_CANPUSH
	sta	THING_STRUCT_EFLAGS,y

@set_anim:
	; set ticks
	lda	#1
	sta	THING_STRUCT_TICKS,y

	; set new frame
	pla	; <next frame HI>
	sta	THING_STRUCT_NXTFRM_H,y
	pla	; <next frame LO>
	sta	THING_STRUCT_NXTFRM_L,y

	bra	@skip_pain

@skip_death:
	; if(!(th->iflags & THING_IFLAG_GOTHIT))
	lda	THING_STRUCT_IFLAGS,y
	and	#(THING_IFLAG_GOTHIT|THING_IFLAG_CORPSE)
	bne	@skip_pain

	; thing type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; pain chance
	lda	THTYPE_STRUCT_PAIN_CHANCE,x
	beq	@skip_pain_bank
	sta	MATH_VAR_U

	; rng
	jsr	rng_get
	lda	MATH_TEMP
	and	#$7F
	cmp	MATH_VAR_U
	bcs	@skip_pain_bank

	; new animation
	lda	THTYPE_ANIM_PAIN_L,x
	pha	; <next frame LO>
	lda	THTYPE_ANIM_PAIN_H,x
	pha	; <next frame HI>

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	bra	@set_anim

@skip_pain_bank:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

@skip_pain:
	; set hit flag
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_GOTHIT
	sta	THING_STRUCT_IFLAGS,y

	; check for thrust
	pla	; <origin>
	beq	@no_thrust

	; get thing type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; get mass
	lda	THTYPE_STRUCT_IMASS,x
	beq	@no_thrust

	;; damage = (damage * info->imass) >> 6

	; damage
	lda	ARG_THING_DAMAGE_L
	sta	VERA_FX_CACHE_L
	lda	ARG_THING_DAMAGE_H
	sta	VERA_FX_CACHE_M

	; info->imass
	lda	THTYPE_STRUCT_IMASS,x
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; accumulate
	lda	VERA_FX_ACCUM
	lda	VERA_FX_ACCUM
	lda	VERA_FX_ACCUM

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0

	; if(damage > 127)
	bmi	@do_limit
	ldx	VERA_DATA0
	beq	@skip_limit

@do_limit:
	lda	#$7F
@skip_limit:
	sta	MATH_TEMP_A

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; thrust
	pla	; <angle>
	jsr	thing_launch_ang

	; done
	rts

@no_thrust:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	pla	; <angle>
	rts

;;;
; explosion from thing
;;;
; Y - damage origin thing
; ARG_EXPLODE_*

thing_explode:
	sty	RET_THING_IDX

	; check source
	lda	ARG_EXPLODE_SOURCE
	bne	:+
	sty	ARG_EXPLODE_SOURCE
:
	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector
	lda	$B000,y
	tax

	; poscheck.vis_tab = map_vis_tab + ((sdx & 0x1F) * 256)
	and	#$1F
	ora	#$A0
	sta	TMP_PTR_H

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; portals[0].sector = sdx
	stx	MEM_PORTALS_SECTOR

	; poscheck.vis_bit = 1 << (sdx >> 5)
	lda	TAB_BANK,x
	tax
	lda	TAB_POW2,x
	sta	EXPLODE_VIS_BIT

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; save XYZ
	lda	THING_STRUCT_X_L,y
	sta	ARG_THING_X_L
	lda	THING_STRUCT_X_H,y
	sta	ARG_THING_X_H
	lda	THING_STRUCT_Y_L,y
	sta	ARG_THING_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	ARG_THING_Y_H
	lda	THING_STRUCT_Z_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_Z_H,y
	sta	ARG_THING_Z_H

	; hitscan.blockedby = blockedby
	; poscheck.radius = radius
	; these are forced overlap

	; poscheck.portal_wr = 1
	lda	#1
	sta	THMOVE_PORTAL_WR

	; poscheck.portal_rd = 0
	stz	THMOVE_PORTAL_RD

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; while(poscheck.portal_rd < poscheck.portal_wr)
	ldy	#0
@loop_portal:
	; sdx = portals[poscheck.portal_rd].sector
	ldx	MEM_PORTALS_SECTOR,y
	stx	EXPLODE_SDX

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

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

	; explode_check_wall
	jsr	@check_wall

	; dist >= 0 && dist <= poscheck.radius
	bne	@next_wall
	lda	MATH_VAR_L
	cmp	ARG_EXPLODE_RADIUS
	bcs	@next_wall

	; wall->backsector
	ldx	WALL_STRUCT_BACKSECTOR,y
	beq	@next_wall

	; !(wall->blocking & hitscan.blockedby & 0x7F)
	lda	WALL_STRUCT_BLOCKING,y
	and	ARG_EXPLODE_BLOCKEDBY
	and	#$7F
	bne	@next_wall

	; vistab bank
	lda	#BANK_VISTAB
	sta	REG_RAM_BANK

	; poscheck.vis_tab[sdx] & poscheck.vis_bit
	lda	(TMP_PTR_L)
	and	EXPLODE_VIS_BIT
	beq	:+

	; add this sector
	jsr	add_sector_raw

	; wall
	ldy	WALL_NOW
:
	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

@next_wall:
	; next wall
	lda	WALL_STRUCT_NEXT,y
	cmp	WALL_FIRST
	bne	@loop_wall

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; next sector
	inc	THMOVE_PORTAL_RD
	ldy	THMOVE_PORTAL_RD
	cpy	THMOVE_PORTAL_WR
	bcc	@loop_portal

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; the thing
	ldx	RET_THING_IDX

	; poscheck.th_zh = th->z / 256 + th->height / 2
	lda	THING_STRUCT_HEIGHT,x
	lsr
	clc
	adc	THING_STRUCT_Z_L,x
	sta	EXPLODE_TH_ZH_L
	lda	THING_STRUCT_Z_H,x
	adc	#0
	sta	EXPLODE_TH_ZH_H

	; inverse division bank
	lda	#BANK_IDIV_L
	sta	REG_RAM_BANK

	; poscheck.th_sh = inv_div[poscheck.radius]
	lda	ARG_EXPLODE_RADIUS
	sta	MATH_VAR_L
	stz	MATH_VAR_H
	jsr	math_idiv
	lda	MATH_VAR_L
	sta	EXPLODE_TH_SH_L
	lda	MATH_VAR_H
	sta	EXPLODE_TH_SH_H

	; while(poscheck.portal_rd)
	ldy	THMOVE_PORTAL_RD
	dey
@loop_sector:
	phy

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; sdx = portals[poscheck.portal_rd].sector
	ldx	MEM_PORTALS_SECTOR,y

	; sector things
	lda	#$A0
	sta	THMOVE_PTR_H
	stx	THMOVE_PTR_L
@loop_thing:
	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; check this slot
	lda	(THMOVE_PTR_L)
	beq	@next_slot
	tax

	; odx == tdx
	cpx	RET_THING_IDX
	beq	@next_slot

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; ht->marked
	lda	THING_STRUCT_MARKED,x
	bne	@next_slot

	; ht->marked = <anything non-zero>
	ina
	sta	THING_STRUCT_MARKED,x

	; !(hitscan.blockedby & ht->blocking)
	lda	THING_STRUCT_BLOCKING,x
	and	ARG_EXPLODE_BLOCKEDBY
	bne	@check_thing

@next_slot:
	; next
	inc	THMOVE_PTR_H
	lda	THMOVE_PTR_H
	cmp	#$BF
	bne	@loop_thing

	; next portal
	ply
	dey
	bpl	@loop_sector

	; cleanup and done
	jmp	thing_unmark_all

@check_wall:
	;; (dd.x * wall->dist.y - dd.y * wall->dist.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; V0 diff (X)
	jsr	v0_diff_x

	; wall->dist.y
	lda	WALL_STRUCT_DIST_Y_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_Y_H,y
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; V0 diff (-Y)
	sec
	lda	ARG_THING_Y_L
	sbc	WALL_STRUCT_VTX_Y_L,y
	sta	VERA_FX_CACHE_L
	lda	ARG_THING_Y_H
	sbc	WALL_STRUCT_VTX_Y_H,y
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

	rts

@check_thing:
	; zist = ht->z / 256 - poscheck.th_zh
	sec
	lda	THING_STRUCT_Z_L,x
	sbc	EXPLODE_TH_ZH_L
	sta	EXPLODE_ZIST_L
	lda	THING_STRUCT_Z_H,x
	sbc	EXPLODE_TH_ZH_H
	sta	EXPLODE_ZIST_H

	; zist < 0
	bpl	@skip_zist

	; zist += ht->height
	clc
	lda	EXPLODE_ZIST_L
	adc	THING_STRUCT_HEIGHT,x
	sta	EXPLODE_ZIST_L
	lda	EXPLODE_ZIST_H
	adc	#0
	sta	EXPLODE_ZIST_H

	; zist < 0
	bmi	:+

	; zist = 0
	stz	EXPLODE_ZIST_L
	stz	EXPLODE_ZIST_H

	bra	@skip_zist
:
	; zist = -zist
	sec
	lda	#0
	sbc	EXPLODE_ZIST_L
	sta	EXPLODE_ZIST_L
	lda	#0
	sbc	EXPLODE_ZIST_H
	sta	EXPLODE_ZIST_H

@skip_zist:
	; zist - poscheck.radius >= 0
	sec
	lda	EXPLODE_ZIST_L
	sbc	ARG_EXPLODE_RADIUS
	lda	EXPLODE_ZIST_H
	sbc	#0
	bmi	:+

	; skip
@th_local_skip:
	jmp	@next_slot
:
	; the thing
	stx	MATH_COUNTER

	; dist = t2t_dist(ht, x, y, ht->radius + poscheck.radius)
	clc
	lda	THING_STRUCT_RADIUS,x
	adc	ARG_EXPLODE_RADIUS
	sta	THMOVE_RADIUS_L
	lda	#0
	rol
	sta	THMOVE_RADIUS_H
	jsr	t2t_dist
	bpl	@th_local_skip

	; save distance
	lda	MATH_VAR_L
	sta	HITSCAN_SIGHT_DIST_L
	lda	MATH_VAR_H
	sta	HITSCAN_SIGHT_DIST_H

	; poscheck.hitang = p2a_coord.a
	lda	MATH_VAR_U
	sta	ARG_HITSCAN_ANGLE

	; the thing
	ldx	MATH_COUNTER

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; dmg = dist - ht->radius
	sec
	lda	HITSCAN_SIGHT_DIST_L
	sbc	THING_STRUCT_RADIUS,x
	sta	MATH_P2A_X_L
	lda	HITSCAN_SIGHT_DIST_H
	sbc	#0
	sta	MATH_P2A_X_H

	; dmg < 0
	bpl	:+

	; dmg = 0
	stz	MATH_P2A_X_L
	stz	MATH_P2A_X_H
:
	; p2a_coord.x = dmg
	; already set

	; p2a_coord.y = zist
	lda	EXPLODE_ZIST_L
	sta	MATH_P2A_Y_L
	lda	EXPLODE_ZIST_H
	sta	MATH_P2A_Y_H

	; dmg = point_to_dist()
	jsr	math_p2d

	; dmg = poscheck.radius - dmg
	sec
	lda	ARG_EXPLODE_RADIUS
	sbc	MATH_VAR_L
	sta	ARG_THING_DAMAGE_L
	lda	#0
	sbc	MATH_VAR_H
	sta	ARG_THING_DAMAGE_H

	; dmg < 0
	bmi	@th_local_skip

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; sight check
	ldy	RET_THING_IDX
	ldx	MATH_COUNTER
	jsr	hitscan_sight_ex

	; hitscan.pbot >= hitscan.ptop
	lda	HITSCAN_SIGHT_PBOT
	cmp	HITSCAN_SIGHT_PTOP
	bcs	@th_local_skip

	;; dmg *= poscheck.th_sh

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dmg
	lda	ARG_THING_DAMAGE_L
	sta	VERA_FX_CACHE_L
	lda	ARG_THING_DAMAGE_H
	sta	VERA_FX_CACHE_M

	; poscheck.th_sh
	lda	EXPLODE_TH_SH_L
	sta	VERA_FX_CACHE_H
	lda	EXPLODE_TH_SH_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_M

	;; dmg *= damage * 2

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dmg is already set

	; damage
	lda	ARG_EXPLODE_DAMAGE
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; accumulate (double the value)
	lda	VERA_FX_ACCUM
	lda	VERA_FX_ACCUM

	; add "one"
	lda	#1
	sta	VERA_FX_CACHE_M
	sta	VERA_FX_CACHE_U
	stz	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_H

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 16)
	lda	VERA_DATA0
	lda	VERA_DATA0
	sta	ARG_THING_DAMAGE_L
	lda	VERA_DATA0
	sta	ARG_THING_DAMAGE_H

	; do damage
	ldx	ARG_EXPLODE_SOURCE
	ldy	MATH_COUNTER
	lda	ARG_HITSCAN_ANGLE
	jsr	thing_damage

	jmp	@next_slot

;;;
; clear thing marks
;;;

thing_unmark_all:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; loop
	ldx	#0
@loop:
	stz	THING_STRUCT_MARKED,x
	inx
	bpl	@loop

	rts

;;;
; momentnum with limit
;;;

get_mlimit_x:
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
	rts

get_mlimit_y:
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
	rts

;;;
; launch thing
;;;
; assuming 'BANK_TICKER_STRUCT' is selected
; Y - thing index
; A - angle (for 'ang' variant)
; MATH_TEMP_A - speed

thing_launch_ang:
	; angle
	pha

	; speed check
	lda	MATH_TEMP_A
	bpl	:+

	; speed limit
	lda	#127
	sta	MATH_TEMP_A
:
	bra	skip_pitch

thing_launch:
	; speed check
	lda	MATH_TEMP_A
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

	; get mlimit X result
	jsr	get_mlimit_x

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

	; get mlimit Y result
	jmp	get_mlimit_y

;;;
; thing height check
;;;
; ARG_THING_X_*, ARG_THING_Y_*, ARG_THING_Z_* (only L and H) - current position
; RET_THING_IDX - thing index

thing_check_heights:
	ldx	RET_THING_IDX

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector
	ldy	$B000,x

	; prepare
	jsr	prepare_pos_check

	; poscheck.th_sh = nz - 256
	lda	ARG_THING_Z_L
	sta	THMOVE_TH_SH_L
	lda	ARG_THING_Z_H
	ina
	sta	THMOVE_TH_SH_H

	; restore opcode
	lda	#$90	; BCC

	; if(poscheck.noradius)
	ldy	THMOVE_NORADIUS
	beq	:+

	; modify opcode
	lda	#$A9	; LDA <imm>
:
	sta	@opcode_loop

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; restore opcode
	lda	#$20	; JSR

	; check blocking
	ldy	THING_STRUCT_BLOCKEDBY,x
	bne	:+

	; modify opcode
	lda	#$AD	; LDA <abs>
:
	sta	@opcode_things

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

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; floor and ceiling
	phx
	jsr	check_planes

	; go trough things
	ply
@opcode_things:
	jsr	check_things

	; next
	inc	TMP_PTR_H
	lda	TMP_PTR_H
	cmp	#$C0
@opcode_loop:
	bcc	@loop	; modified opcode

@stop:
	rts

;;;
; thing position check
;;;
; ARG_THING_X_*, ARG_THING_Y_*, ARG_THING_Z_* (only L and H) - desired position
; ARG_THING_SECTOR - target sector (zero = from thing)
; RET_THING_IDX - thing index (INPUT)
; zero flag set = fail
; A = new thing sector
; THMOVE_SECTOR = new thing sector

thing_check_pos:
	ldx	RET_THING_IDX

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
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; move check
	lda	THING_STRUCT_MX_S,x
	ora	THING_STRUCT_MX_L,x
	ora	THING_STRUCT_MY_S,x
	ora	THING_STRUCT_MY_L,x
	sta	THMOVE_MOVING

thing_check_spawn:
	; reset
	stz	THMOVE_HITFIX

@failsafe:
	; prepare
	ldy	ARG_THING_SECTOR
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

	; poscheck.portal_rd = 0
	stz	THMOVE_PORTAL_RD

	; poscheck.portal_wr = 1
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

	; if(!poscheck.noradius)
	ldy	#$20	; JSR
	lda	THMOVE_NORADIUS
	beq	:+
	ldy	#$AD	; LDA <abs>
:
	sty	@opcode_planes+0

	; while(poscheck.portal_rd < poscheck.portal_wr)
@loop_sec:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; floor and ceiling
@opcode_planes:
	jsr	check_planes	; modified opcode

	; link check
	lda	THMOVE_ISLINK
	beq	:+

	; skip walls
	jmp	@next_sector
:
	; !poscheck.noradius
	lda	THMOVE_NORADIUS
	bne	@skip_sobj

	; sec->sobj_hi
	lda	SECTOR_STRUCT_OBJ_H,x
	beq	@skip_sobj

	jmp	@check_sobj

@skip_sobj:
	; inside = any non-zero number
	lda	#$FF
	sta	THMOVE_INSIDE

	; poscheck.pthit = 0
	stz	THMOVE_PTHIT

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

	; if(poscheck.noradius)
	lda	THMOVE_NORADIUS
	bne	@next_wall

@do_block_wall:
	; save
	lda	WALL_BANK
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
	; if(poscheck.noradius)
	lda	THMOVE_NORADIUS
	bne	@next_point

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; save
	lda	WALL_BANK
	sta	THMOVE_HTYPE
	sty	THMOVE_HIDX

	; thing
	ldx	RET_THING_IDX

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

@next_point:
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
	lda	THMOVE_BLOCKEDBY
	beq	:+
	lda	THMOVE_NORADIUS
	bne	:+
	jsr	check_things
	bpl	:+
	jmp	@do_thing_block
:
	; next
	inc	THMOVE_PORTAL_RD
	lda	THMOVE_PORTAL_RD
	cmp	THMOVE_PORTAL_WR
	bcs	:+
	jmp	@loop_thsec
:
	; if(poscheck.noradius)
	lda	THMOVE_NORADIUS
	beq	:+

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; check_planes
	ldx	THMOVE_SECTOR
	jsr	check_planes
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
	; !poscheck.sector
	lda	THMOVE_SECTOR
	bne	@pass_ex

	; hitfix
	lda	THMOVE_HITFIX
	bne	@pass

	; the thing
	ldy	RET_THING_IDX

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; poscheck.moving
	lda	THMOVE_MOVING
	beq	@pass

	; fill p2a_coord
	lda	THING_STRUCT_MX_S,y
	sta	MATH_P2A_X_L
	lda	THING_STRUCT_MX_L,y
	sta	MATH_P2A_X_H
	lda	THING_STRUCT_MY_S,y
	sta	MATH_P2A_Y_L
	lda	THING_STRUCT_MY_L,y
	sta	MATH_P2A_Y_H

	; point_to_angle
	jsr	math_p2a
	jsr	math_ang8

	; backup
	lda	ARG_THING_X_S
	pha
	lda	ARG_THING_X_L
	pha
	lda	ARG_THING_X_H
	pha
	lda	ARG_THING_Y_S
	pha
	lda	ARG_THING_Y_L
	pha
	lda	ARG_THING_Y_H
	pha

	; hitscan_func
	lda	MATH_VAR_U
	sta	ARG_HITSCAN_ANGLE
	ldy	RET_THING_IDX
	lda	#<cb_hitfix
	sta	hitscan_cb_wall+1
	lda	#>cb_hitfix
	sta	hitscan_cb_wall+2
	jsr	hitscan_func

	; restore
	pla
	sta	ARG_THING_Y_H
	pla
	sta	ARG_THING_Y_L
	pla
	sta	ARG_THING_Y_S
	pla
	sta	ARG_THING_X_H
	pla
	sta	ARG_THING_X_L
	pla
	sta	ARG_THING_X_S

	; if(poscheck.sector)
	lda	THMOVE_HITFIX
	beq	@pass_bad

	; sdx = poscheck.sector
	sta	ARG_THING_SECTOR

	jmp	@failsafe

@pass_bad:
	stz	THMOVE_SECTOR
	stz	THMOVE_HTYPE

@pass:
	;; STOP - PASSED
	; unset zero flag
	lda	THMOVE_SECTOR
@pass_ex:
	rts

@do_thing_block:
	; ht->eflags & THING_EFLAG_PUSHABLE
	lda	THING_STRUCT_EFLAGS,y
	lsr
	bcc	@no_push

	; th->eflags & THING_EFLAG_CANPUSH
	lda	THING_STRUCT_EFLAGS,x
	and	#THING_EFLAG_CANPUSH
	beq	@no_push

	; ht->mx += th->mx
	clc
	lda	THING_STRUCT_MX_S,y
	adc	THING_STRUCT_MX_S,x
	sta	THING_STRUCT_MX_S,y
	lda	THING_STRUCT_MX_L,y
	adc	THING_STRUCT_MX_L,x
	sta	THING_STRUCT_MX_L,y

	; ht->my += th->my
	clc
	lda	THING_STRUCT_MY_S,y
	adc	THING_STRUCT_MY_S,x
	sta	THING_STRUCT_MY_S,y
	lda	THING_STRUCT_MY_L,y
	adc	THING_STRUCT_MY_L,x
	sta	THING_STRUCT_MY_L,y

@no_push:
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

@check_sobj:
	; get pointer
	and	#$7F
	asl
	asl
	asl
	ldy	SECTOR_STRUCT_OBJ_L,x
	ora	TAB_BANK,y
	sta	TMP_PTR_I
	tya
	and	#$1F
	ora	#$A0
	sta	TMP_PTR_H
	lda	SECTOR_STRUCT_OBJ_S,x
	sta	TMP_PTR_L

	; data bank
	lda	TMP_PTR_I
	sta	REG_RAM_BANK

	; loop
	lda	(TMP_PTR_L)
@loop_sobj:
	; bank
	ora	#BANK_MAPWALLS
	sta	WALL_BANK

	; wall hit info
	stz	THMOVE_SOBJ_HIT

	; first
	ldy	#SECOBJ_STRUCT_FIRST
	lda	(TMP_PTR_L),y
	sta	WALL_FIRST

@loop_sobj_wall:
	sta	WALL_NOW
	tay

	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

	;; dist = (dd.x * wall->dist.y - dd.y * wall->dist.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dd.x
	jsr	v0_diff_x

	; wall->dist.y
	lda	WALL_STRUCT_DIST_Y_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_Y_H,y
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; -dd.y
	sec
	lda	ARG_THING_Y_L
	sbc	WALL_STRUCT_VTX_Y_L,y
	sta	VERA_FX_CACHE_L
	lda	ARG_THING_Y_H
	sbc	WALL_STRUCT_VTX_Y_H,y
	sta	VERA_FX_CACHE_M

	; wall->dist.x
	lda	WALL_STRUCT_DIST_X_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_X_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; if(dist >= poscheck.radius)
	sec
	lda	VERA_DATA0
	sbc	THMOVE_RADIUS
	lda	VERA_DATA0
	sbc	#0
	bpl	@next_sobj

	; if(poscheck.moving)
	lda	THMOVE_MOVING
	bne	@check_cross

@next_sobj_wall:
	; next wall
	lda	WALL_STRUCT_NEXT,y
	cmp	WALL_FIRST
	bne	@loop_sobj_wall

	;; STOP - FAILED

	; poscheck.htype = 0
	stz	THMOVE_HTYPE
	stz	THMOVE_SECTOR

	; check wall hit
	lda	THMOVE_SOBJ_HIT
	bne	:+

	; zero flag is set
	rts
:
	ldy	THMOVE_SOBJ_WALL
	jmp	@do_block_wall

@next_sobj:
	; advance
	clc
	lda	TMP_PTR_L
	adc	#SECOBJ_STRUCT_SIZE
	sta	TMP_PTR_L

	; data bank
	lda	TMP_PTR_I
	sta	REG_RAM_BANK

	; next
	lda	(TMP_PTR_L)
	bpl	@loop_sobj

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; restore
	ldx	ARG_THING_SECTOR

	jmp	@skip_sobj

@check_cross:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; the thing
	ldx	RET_THING_IDX

	; X
	lda	THING_STRUCT_X_L,x
	sta	MATH_VAR_L
	lda	THING_STRUCT_X_H,x
	sta	MATH_VAR_H

	; <Y>
	lda	THING_STRUCT_Y_H,x
	pha
	lda	THING_STRUCT_Y_L,x
	pha

	; wall bank
	lda	WALL_BANK
	sta	REG_RAM_BANK

	;; dist = (dd.x * wall->dist.y - dd.y * wall->dist.x) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; -dd.y
	sec
	pla
	sbc	WALL_STRUCT_VTX_Y_L,y
	sta	VERA_FX_CACHE_L
	pla
	sbc	WALL_STRUCT_VTX_Y_H,y
	sta	VERA_FX_CACHE_M

	; wall->dist.x is already set

	; accumulate
	lda	VERA_FX_ACCUM

	; dd.x
	sec
	lda	WALL_STRUCT_VTX_X_L,y
	sbc	MATH_VAR_L
	sta	VERA_FX_CACHE_L
	lda	WALL_STRUCT_VTX_X_H,y
	sbc	MATH_VAR_H
	sta	VERA_FX_CACHE_M

	; wall->dist.y
	lda	WALL_STRUCT_DIST_Y_L,y
	sta	VERA_FX_CACHE_H
	lda	WALL_STRUCT_DIST_Y_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; if(dist >= poscheck.radius)
	sec
	lda	VERA_DATA0
	sbc	THMOVE_RADIUS
	lda	VERA_DATA0
	sbc	#0
	bmi	:+

	; save hit
	lda	#$FF
	sta	THMOVE_SOBJ_HIT

	; save the wall
	sty	THMOVE_SOBJ_WALL
:
	jmp	@next_sobj_wall

;;;
; thing position apply
;;;
; run after successful 'thing_check_pos'

;;;
; apply thing position
;;;
; RET_THING_IDX - the thing
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
	ldy	RET_THING_IDX

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
	ldx	RET_THING_IDX

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
; Y - sector index

prepare_pos_check:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; flags
	lda	SECTOR_STRUCT_FLAGS,y
	pha

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; thing type
	ldy	TICKER_STRUCT_TYPE,x

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; poscheck.water_height = info->water_height
	lda	THTYPE_STRUCT_WATER_HEIGHT,y
	sta	THMOVE_WATER_HEIGHT

	; step_height
	lda	THTYPE_STRUCT_STEP_HEIGHT,y
	sta	MATH_VAR_U

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; !(sflags & SECTOR_FLAG_WATER)
	pla
	bmi	@water_nope

	; th->mz / 256 > -32
	lda	THING_STRUCT_MZ_L,x
	bpl	@water_ok
	cmp	#$E1
	bcs	@water_ok

@water_nope:
	; poscheck.water_height = 255
	lda	#$FF
	sta	THMOVE_WATER_HEIGHT

@water_ok:
	; poscheck.radius = th->radius
	lda	THING_STRUCT_RADIUS,x
	sta	THMOVE_RADIUS

	; poscheck.height = th->height
	lda	THING_STRUCT_HEIGHT,x
	sta	THMOVE_HEIGHT

	; poscheck.blockedby = th->blockedby
	lda	THING_STRUCT_BLOCKEDBY,x
	sta	THMOVE_BLOCKEDBY

	; poscheck.noradius = th->eflags & THING_EFLAG_NORADIUS
	lda	THING_STRUCT_EFLAGS,x
	and	#THING_EFLAG_NORADIUS
	sta	THMOVE_NORADIUS

	; origin
	lda	THING_STRUCT_TH_ORIGIN,x
	sta	THMOVE_ORIGIN

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
	; poscheck.th_sh = nz + info->step_height
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
	; poscheck.tfz = sec->floor.height
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	THMOVE_TFZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sta	THMOVE_TFZ_H

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
; thing to thing distance
;;;

t2t_dist:
	; X diff
	sec
	lda	THING_STRUCT_X_L,x
	sbc	ARG_THING_X_L
	sta	THMOVE_DIFF_L
	sta	MATH_P2A_X_L
	lda	THING_STRUCT_X_H,x
	sbc	ARG_THING_X_H
	sta	THMOVE_DIFF_H
	sta	MATH_P2A_X_H

	; X check
	jsr	check_radist
	bmi	:+
	rts
:
	; Y diff
	sec
	lda	THING_STRUCT_Y_L,x
	sbc	ARG_THING_Y_L
	sta	THMOVE_DIFF_L
	sta	MATH_P2A_Y_L
	lda	THING_STRUCT_Y_H,x
	sbc	ARG_THING_Y_H
	sta	THMOVE_DIFF_H
	sta	MATH_P2A_Y_H

	; Y check
	jsr	check_radist
	bmi	:+
	rts
:
	; dist = point_to_dist()
	jsr	math_p2d

	lda	#$FF
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
	cmp	RET_THING_IDX
	beq	@next_slot
	tax

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; origin
	cpx	THMOVE_ORIGIN
	beq	@next_slot_ex

	; blocking
	lda	THING_STRUCT_BLOCKING,x
	and	THMOVE_BLOCKEDBY
	beq	@next_slot_ex

	; for addition
	clc

	; if(ht->iflags & THING_IFLAG_ALTRADIUS)
	lda	THING_STRUCT_IFLAGS,x
	and	#THING_IFLAG_ALTRADIUS
	beq	@normal_radius

	; type
	phy
	ldy	TICKER_STRUCT_TYPE,x

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; alt_radius
	lda	THTYPE_STRUCT_ALT_RADIUS,y

	; ticker structure bank
	ldy	#BANK_TICKER_STRUCT
	sty	REG_RAM_BANK

	; restore
	ply
	bra	@alt_radius

@normal_radius:
	; radius = ht->radius + poscheck.radius
	lda	THING_STRUCT_RADIUS,x
@alt_radius:
	adc	THMOVE_RADIUS
	sta	THMOVE_RADIUS_L
	stz	THMOVE_RADIUS_H
	rol	THMOVE_RADIUS_H

	; save
	stx	THMOVE_HIDX

	; dist
	jsr	t2t_dist
	bpl	@next_slot_ex

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
	ldx	RET_THING_IDX

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
	; if(poscheck.noradius)
	lda	THMOVE_NORADIUS
	beq	:+

	; no negative flag
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
; failsafe hitscan
;;;

cb_hitfix:
	ldy	WALL_NOW
	lda	WALL_STRUCT_BACKSECTOR,y
	sta	THMOVE_HITFIX
	sec
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
; RET_THING_IDX - desired thing
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
	lda	RET_THING_IDX
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
	ldy	RET_THING_IDX

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
; projectile hit something
;;;

projectile_death:
	; reset
	stz	PRJDTH_THING
	stz	PRJDTH_TEXTURE

	; get position
	lda	RET_THING_IDX
	tax
	tay
	jsr	action_xy
	lda	THING_STRUCT_Z_L,x
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_Z_H,x
	sta	ARG_THING_Z_H

	; angle
	lda	THING_STRUCT_ANGLE,x
	sta	ARG_HITSCAN_ANGLE

	; half pitch
	lda	THING_STRUCT_PITCH,x
	lsr
	sta	ARG_HITSCAN_HALF_PITCH

	; hitscan.radius = th->radius
	lda	THING_STRUCT_RADIUS,x
	sta	PRJDTH_RADIUS
	sta	VERA_FX_CACHE_H

	; stop
	lda	#0
	sta	THING_STRUCT_MX_S,x
	sta	THING_STRUCT_MX_L,x
	sta	THING_STRUCT_MY_S,x
	sta	THING_STRUCT_MY_L,x
	sta	THING_STRUCT_MZ_S,x
	sta	THING_STRUCT_MZ_L,x

	; unblock
	stz	THING_STRUCT_BLOCKING,x
	stz	THING_STRUCT_BLOCKEDBY,x

	; eflags
	lda	THING_STRUCT_EFLAGS,x
	and	#<~THING_EFLAG_PROJECTILE
	ora	#THING_EFLAG_NORADIUS
	sta	THING_STRUCT_EFLAGS,x

	; iflags
	lda	THING_STRUCT_IFLAGS,x
	and	#<~THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,x

	; th->ticker.func |= TICKER_FUNC_ANIM
	lda	TICKER_STRUCT_FUNC,x
	ora	#TICKER_FUNC_ANIM
	sta	TICKER_STRUCT_FUNC,x

	; thing type
	ldy	TICKER_STRUCT_TYPE,x

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; new animation
	lda	THTYPE_ANIM_DEATH_L,y
	pha	; <next frame LO>
	lda	THTYPE_ANIM_DEATH_H,y
	pha	; <next frame HI>

	; new radius
	lda	THTYPE_STRUCT_ALT_RADIUS,y
	pha	; <new radius>

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; <new radius>
	pla
	sta	THING_STRUCT_RADIUS,x

	; <next frame HI>
	pla
	sta	THING_STRUCT_NXTFRM_H,x

	; <next frame LO>
	pla
	sta	THING_STRUCT_NXTFRM_L,x

	; th->ticks = 1
	lda	#1
	sta	THING_STRUCT_TICKS,x

	; check hit type
	lda	THMOVE_HTYPE
	bne	:+

	; hit nothing
	ldy	RET_THING_IDX
	rts
:
	bmi	:+
	jmp	@not_thing
:
	; thing = poscheck.hidx
	lda	THMOVE_HIDX
	sta	PRJDTH_THING

	; prepare
	jsr	hitscan_angles

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; calculate
	ldy	THMOVE_HIDX
	jsr	hitscan_thing_dd
	jsr	hitscan_thing_dt

	; get result (>> 8)
	; dd += dt
	clc
	lda	VERA_DATA0
	adc	HITSCAN_TSID_L
	sta	HITSCAN_TSID_L
	lda	VERA_DATA0
	adc	HITSCAN_TSID_H
	sta	HITSCAN_TSID_H

	; dist -= hitscan.radius
	sec
	lda	HITSCAN_TSID_L
	sbc	PRJDTH_RADIUS
	sta	VERA_FX_CACHE_H
	lda	HITSCAN_TSID_H
	sbc	#0
	sta	VERA_FX_CACHE_U

	;; d1.x = (tab_sin[angle] * dist) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; tab_sin
	ldx	ARG_HITSCAN_ANGLE
	math_read_sin	VERA_FX_CACHE_L, VERA_FX_CACHE_M

	; dist is already set

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	; hitscan.x + d1.x
	clc
	lda	VERA_DATA0
	adc	ARG_THING_X_L
	sta	ARG_THING_X_L
	lda	VERA_DATA0
	adc	ARG_THING_X_H
	sta	ARG_THING_X_H

	;; d1.y = (tab_cos[angle] * dist) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; tab_cos
	ldx	ARG_HITSCAN_ANGLE
	math_read_cos	VERA_FX_CACHE_L, VERA_FX_CACHE_M

	; dist is already set

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	; hitscan.y + d1.y
	clc
	lda	VERA_DATA0
	adc	ARG_THING_Y_L
	sta	ARG_THING_Y_L
	lda	VERA_DATA0
	adc	ARG_THING_Y_H
	sta	ARG_THING_Y_H

	;; dist = (dist * hitscan.wtan) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; hitscan.wtan
	lda	HITSCAN_WTAN_L
	sta	VERA_FX_CACHE_L
	lda	HITSCAN_WTAN_H
	sta	VERA_FX_CACHE_M

	; dist is already set

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	; hitscan.z + dist
	clc
	lda	VERA_DATA0
	adc	ARG_THING_Z_L
	sta	ARG_THING_Z_L
	lda	VERA_DATA0
	adc	ARG_THING_Z_H
	sta	ARG_THING_Z_H

@fail_thing:
	jmp	@handled

@not_thing:
	and	#$10
	bne	:+
	jmp	@not_wall
:
	; wall bank
	lda	THMOVE_HTYPE
	sta	REG_RAM_BANK

	; the wall
	ldy	THMOVE_HIDX

	; wsrc->vtx.y
	lda	WALL_STRUCT_VTX_Y_H,y
	pha	; <vtx.x HI>
	lda	WALL_STRUCT_VTX_Y_L,y
	pha	; <vtx.x LO>

	; wall.dist.x = wsrc->dist.x
	lda	WALL_STRUCT_DIST_X_H,y
	pha	; <dist.x HI>
	lda	WALL_STRUCT_DIST_X_L,y
	pha	; <dist.x LO>

	; wsrc->vtx.x
	lda	WALL_STRUCT_VTX_X_H,y
	pha	; <vtx.x HI>
	lda	WALL_STRUCT_VTX_X_L,y
	pha	; <vtx.x LO>

	; wall.dist.y = wsrc->dist.y
	lda	WALL_STRUCT_DIST_Y_H,y
	pha	; <dist.y HI>
	lda	WALL_STRUCT_DIST_Y_L,y
	pha	; <dist.y LO>

	; wall.angle = wsrc->angle
	lda	WALL_STRUCT_ANGLE_H,y
	pha	; <angle HI>
	lda	WALL_STRUCT_ANGLE_L,y
	pha	; <angle LO>

	; wall bank (hack)
	lda	#BANK_MAPWALLS
	sta	REG_RAM_BANK

	; wall.angle = wsrc->angle
	pla	; <angle LO>
	sta	WALL_STRUCT_ANGLE_L
	pla	; <angle HI>
	sta	WALL_STRUCT_ANGLE_H

	;; wc = (wall.dist.y * hitscan.radius) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; wall.dist.y
	pla	; <dist.y LO>
	sta	WALL_STRUCT_DIST_Y_L
	sta	VERA_FX_CACHE_L
	pla	; <dist.y HI>
	sta	WALL_STRUCT_DIST_Y_H
	sta	VERA_FX_CACHE_M

	; hitscan.radius is partially set
	stz	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	; wall.vtx.x = wsrc->vtx.x - wc
	sec
	pla	; <vtx.x LO>
	sbc	VERA_DATA0
	sta	WALL_STRUCT_VTX_X_L
	pla	; <vtx.x HI>
	sbc	VERA_DATA0
	sta	WALL_STRUCT_VTX_X_H

	;; ws = (wall.dist.x * hitscan.radius) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; wall.dist.x
	pla	; <dist.x LO>
	sta	WALL_STRUCT_DIST_X_L
	sta	VERA_FX_CACHE_L
	pla	; <dist.x HI>
	sta	WALL_STRUCT_DIST_X_H
	sta	VERA_FX_CACHE_M

	; hitscan.radius is already set

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	; wall.vtx.y = wsrc->vtx.y + ws
	clc
	pla	; <vtx.y LO>
	adc	VERA_DATA0
	sta	WALL_STRUCT_VTX_Y_L
	pla	; <vtx.y HI>
	adc	VERA_DATA0
	sta	WALL_STRUCT_VTX_Y_H

	; prepare
	jsr	hitscan_angles

	; wall bank (hack)
	lda	#BANK_MAPWALLS
	sta	REG_RAM_BANK

	; calculate
	stz	WALL_NOW
	jsr	hitscan_wall_pos
	jsr	hitscan_wall_hitz

	; new position
	lda	HITSCAN_D0_X_L
	sta	ARG_THING_X_L
	lda	HITSCAN_D0_X_H
	sta	ARG_THING_X_H
	lda	HITSCAN_D0_Y_L
	sta	ARG_THING_Y_L
	lda	HITSCAN_D0_Y_H
	sta	ARG_THING_Y_H
	lda	HITSCAN_ZZ_L
	sta	ARG_THING_Z_L
	lda	HITSCAN_ZZ_H
	sta	ARG_THING_Z_H

	; wall bank
	lda	THMOVE_HTYPE
	sta	REG_RAM_BANK

	; the wall
	ldy	THMOVE_HIDX

	; texture = wsrc->top.texture
	lda	WALL_STRUCT_TOP_TEX,y
	sta	PRJDTH_TEXTURE

	; if(wsrc->backsector)
	lda	WALL_STRUCT_BACKSECTOR,y
	beq	@no_back

	; the sector
	tay

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; if(zz <= sec->floor.height)
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sbc	ARG_THING_Z_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	sbc	ARG_THING_Z_H
	bmi	@no_bot

	; wall bank
	lda	THMOVE_HTYPE
	sta	REG_RAM_BANK

@set_wall_bot:
	; texture = wsrc->bot.texture
	ldy	THMOVE_HIDX
	lda	WALL_STRUCT_BOT_TEX,y
	sta	PRJDTH_TEXTURE

	bra	@handled

@no_bot:
	; if(zz <= sec->ceiling.height)
	sec
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	sbc	ARG_THING_Z_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	sbc	ARG_THING_Z_H
	bmi	@handled

	; texture = 0
	stz	PRJDTH_TEXTURE

	bra	@handled

@no_back:
	; wsrc->angle & WALL_MARK_EXTENDED
	lda	WALL_STRUCT_ANGLE_H,y
	and	#WALL_MARK_EXTENDED
	beq	@handled

	; wsrc->split != -32768
	lda	WALL_STRUCT_SPLIT_H,y
	cmp	$80	; lazy check
	beq	@handled

	; zz <= wsrc->split
	sec
	lda	WALL_STRUCT_SPLIT_L,y
	sbc	ARG_THING_Z_L
	lda	WALL_STRUCT_SPLIT_H,y
	sbc	ARG_THING_Z_H
	bpl	@set_wall_bot

	bra	@handled

@not_wall:
	bit	THMOVE_HTYPE
	bvs	@hit_floor

	; check ceiling thing
	lda	THING_STRUCT_CEILT,x
	beq	@no_ceilingt

	; thing = th->ceilingt
	sta	PRJDTH_THING

	bra	@handled

@no_ceilingt:
	; the sector
	ldy	THING_STRUCT_CEILS,x

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; ceiling
	lda	SECTOR_STRUCT_CEILING_TEXTURE,y
	sta	PRJDTH_TEXTURE

	bra	@handled

@hit_floor:
	; check floor thing
	lda	THING_STRUCT_FLOORT,x
	beq	@no_floort

	; thing = th->floort
	sta	PRJDTH_THING

	bra	@handled

@no_floort:
	; the sector
	ldy	THING_STRUCT_FLOORS,x

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; floor
	lda	SECTOR_STRUCT_FLOOR_TEXTURE,y
	sta	PRJDTH_TEXTURE

@handled:
	; restore
	ldx	RET_THING_IDX

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; if(texture == 0xFF)
	lda	PRJDTH_TEXTURE
	ina
	bne	:+

	; th->next_state = 0
	stz	THING_STRUCT_NXTFRM_L,x
	stz	THING_STRUCT_NXTFRM_H,x

	; hit sky
	ldy	RET_THING_IDX
	rts
:
	; check new position
	stz	ARG_THING_SECTOR
	jsr	thing_check_pos

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; check result
	lda	THMOVE_SECTOR
	beq	:+

	; apply
	jsr	thing_apply_pos
	stz	THING_STRUCT_X_S,x
	lda	ARG_THING_X_L
	sta	THING_STRUCT_X_L,x
	lda	ARG_THING_X_H
	sta	THING_STRUCT_X_H,x
	lda	ARG_THING_Y_S
	stz	THING_STRUCT_Y_S,x
	lda	ARG_THING_Y_L
	sta	THING_STRUCT_Y_L,x
	lda	ARG_THING_Y_H
	sta	THING_STRUCT_Y_H,x
	stz	THING_STRUCT_Z_S,x
	lda	ARG_THING_Z_L
	sta	THING_STRUCT_Z_L,x
	lda	ARG_THING_Z_H
	sta	THING_STRUCT_Z_H,x
:
	; if(thing)
	ldy	PRJDTH_THING
	beq	@no_damage

	; do damage
	ldx	RET_THING_IDX
	lda	THING_STRUCT_HEALTH_L,x
	sta	ARG_THING_DAMAGE_L
	lda	THING_STRUCT_HEALTH_H,x
	sta	ARG_THING_DAMAGE_H
	lda	THING_STRUCT_ANGLE,x
	jsr	thing_damage

@no_damage:
	; animate
	ldy	RET_THING_IDX
	rts

;;;
; thing ticker (normal)
;;;

thing_tick_full:
	jsr	thing_tick_move
	jmp	thing_tick_anim

;;;
; thing ticker (movement)
;;;

thing_tick_move:
	sty	RET_THING_IDX

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
	; th->iflags &= ~(THING_IFLAG_BLOCKED | THING_IFLAG_GOTHIT)
	lda	THING_STRUCT_IFLAGS,y
	and	#<~(THING_IFLAG_BLOCKED | THING_IFLAG_GOTHIT)
	sta	THING_STRUCT_IFLAGS,y

	; reset repeat
	stz	THTICK_REPEAT

	; XY momentnum
	lda	THING_STRUCT_MX_S,y
	sta	THTICK_MX_S
	lda	THING_STRUCT_MX_L,y
	sta	THTICK_MX_L
	lda	THING_STRUCT_MY_S,y
	sta	THTICK_MY_S
	lda	THING_STRUCT_MY_L,y
	sta	THTICK_MY_L
	ora	THTICK_MX_S
	ora	THTICK_MX_L
	ora	THTICK_MY_S
	bne	@do_xy_move

	; if(th->iflags & THING_IFLAG_HEIGHTCHECK)
	lda	THING_STRUCT_IFLAGS,y
	bpl	@no_heights

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
	ldx	RET_THING_IDX
	jsr	thing_apply_heights
	ldy	RET_THING_IDX

@no_heights:
	jmp	@no_xy_move

@do_xy_move:
	; if(th->eflags & THING_EFLAG_PROJECTILE)
	lda	THING_STRUCT_EFLAGS,y
	bpl	@check_limit

	; get type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; repeat count
	lda	THTYPE_STRUCT_JUMPZ,x
	sta	THTICK_REPEAT

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	bra	@try_xy_move

@check_limit:
	; rad = 0x7F00
	lda	#$7F
	sta	MATH_TEMP_A

	; !(th->radius & 0x80)
	lda	THING_STRUCT_RADIUS,y
	bmi	@try_xy_move

	; rad = th->radius << 9
	; if(th->radius > 0x7F00)
	asl
	bmi	:+
	sta	MATH_TEMP_A
:
	; if(th->mx < 0)
	lda	THTICK_MX_L
	bpl	@mx_pos

	; if(th->mx + rad < 0)
	clc
	adc	MATH_TEMP_A
	bpl	@check_my

	; tmx = -rad
	stz	THTICK_MX_S
	lda	MATH_TEMP_A
	eor	#$FF
	ina
	sta	THTICK_MX_L

	bra	@check_my

@mx_pos:
	; if(rad - th->mx < 0)
	sec
	lda	#0
	sbc	THTICK_MX_S
	lda	MATH_TEMP_A
	sbc	THTICK_MX_L
	bpl	@check_my

	; tmx = rad
	stz	THTICK_MX_S
	lda	MATH_TEMP_A
	sta	THTICK_MX_L

@check_my:
	; if(th->my < 0)
	lda	THTICK_MY_L
	bpl	@my_pos

	; if(th->my + rad < 0)
	clc
	adc	MATH_TEMP_A
	bpl	@try_xy_move

	; tmy = -rad
	stz	THTICK_MY_S
	lda	MATH_TEMP_A
	eor	#$FF
	ina
	sta	THTICK_MY_L

	bra	@try_xy_move

@my_pos:
	; if(rad - th->my < 0)
	sec
	lda	#0
	sbc	THTICK_MY_S
	lda	MATH_TEMP_A
	sbc	THTICK_MY_L
	bpl	@try_xy_move

	; tmy = rad
	stz	THTICK_MY_S
	lda	MATH_TEMP_A
	sta	THTICK_MY_L

@try_xy_move:
	; try to move
	stz	THTICK_RETRY

@xy_retry:
	; new X
	ldx	THTICK_MX_L
	lda	TAB_SIGN_EXT,x
	sta	MATH_TEMP_A
	clc
	lda	THING_STRUCT_X_S,y
	adc	THTICK_MX_S
	sta	ARG_THING_X_S
	lda	THING_STRUCT_X_L,y
	adc	THTICK_MX_L
	sta	ARG_THING_X_L
	lda	THING_STRUCT_X_H,y
	adc	MATH_TEMP_A
	sta	ARG_THING_X_H

	; new Y
	ldx	THTICK_MY_L
	lda	TAB_SIGN_EXT,x
	sta	MATH_TEMP_A
	clc
	lda	THING_STRUCT_Y_S,y
	adc	THTICK_MY_S
	sta	ARG_THING_Y_S
	lda	THING_STRUCT_Y_L,y
	adc	THTICK_MY_L
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
	ldx	RET_THING_IDX

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
	ldx	RET_THING_IDX
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
	ldx	RET_THING_IDX
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

	; projectile death
	jmp	projectile_death
:
	; check fail hit
	lda	THMOVE_HTYPE
	bne	@try_slide

@just_stop:
	; stop
	stz	THING_STRUCT_MX_S,x
	stz	THING_STRUCT_MX_L,x
	stz	THING_STRUCT_MY_S,x
	stz	THING_STRUCT_MY_L,x

	jmp	@no_xy_friction

@try_slide:
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
	ldx	RET_THING_IDX

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
	sta	THTICK_MX_S
	lda	VERA_DATA0
	sta	THTICK_MX_L

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
	sta	THTICK_MY_S
	lda	VERA_DATA0
	sta	THTICK_MY_L

	; check
	ora	THTICK_MX_S
	ora	THTICK_MX_L
	ora	THTICK_MY_S
	bne	:+
	jmp	@just_stop
:
	; retry counter
	inc	THTICK_RETRY
	ldy	RET_THING_IDX

	; if(th->ticker.func == TFUNC_PLAYER)
	lda	TICKER_STRUCT_FUNC,y
	cmp	#TICKER_FUNC_PLAYER
	beq	:+

	;; th->mx = mlimit((th->mx >> 1) + vect.x)

	; * 1
	lda	#1
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; th->mx >> 1
	lda	THING_STRUCT_MX_L,y
	asl
	lda	THING_STRUCT_MX_L,y
	ror
	sta	VERA_FX_CACHE_U
	lda	THING_STRUCT_MX_S,y
	ror
	sta	VERA_FX_CACHE_H

	; accumulate
	lda	VERA_FX_ACCUM

	; vect.x
	lda	THTICK_MX_S
	sta	VERA_FX_CACHE_H
	lda	THTICK_MX_L
	sta	VERA_FX_CACHE_U

	; get mlimit X result
	jsr	get_mlimit_x

	;; th->my = mlimit((th->my >> 1) + vect.y)

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; th->my >> 1
	lda	THING_STRUCT_MY_L,y
	asl
	lda	THING_STRUCT_MY_L,y
	ror
	sta	VERA_FX_CACHE_U
	lda	THING_STRUCT_MY_S,y
	ror
	sta	VERA_FX_CACHE_H

	; accumulate
	lda	VERA_FX_ACCUM

	; vect.x
	lda	THTICK_MY_S
	sta	VERA_FX_CACHE_H
	lda	THTICK_MY_L
	sta	VERA_FX_CACHE_U

	; get mlimit Y result
	jsr	get_mlimit_y

	; retry
	jmp	@xy_retry
:
	; appply (player)
	lda	THTICK_MX_S
	sta	THING_STRUCT_MX_S,y
	lda	THTICK_MX_L
	sta	THING_STRUCT_MX_L,y
	lda	THTICK_MY_S
	sta	THING_STRUCT_MY_S,y
	lda	THTICK_MY_L
	sta	THING_STRUCT_MY_L,y

	; retry
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

	; check projectile flag
	lda	THING_STRUCT_EFLAGS,x
	bmi	@no_xy_friction

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
	ldy	RET_THING_IDX

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
	bpl	@under_ceiling

	; over ceiling
	lda	#0
	sta	THING_STRUCT_MZ_S,y
	sta	THING_STRUCT_MZ_L,y
	stz	ARG_THING_Z_S
	lda	THING_STRUCT_CEILZ_L,y
	sta	ARG_THING_Z_L
	lda	THING_STRUCT_CEILZ_H,y
	sta	ARG_THING_Z_H

	; check projectile flag
	lda	THING_STRUCT_EFLAGS,y
	bpl	:+

	; projectile death
	lda	#$01
	sta	THMOVE_HTYPE
	jmp	projectile_death
:
@under_ceiling:
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

	; check projectile flag
	lda	THING_STRUCT_EFLAGS,y
	bpl	:+

	; projectile death
	lda	#$41
	sta	THMOVE_HTYPE
	jmp	projectile_death
:
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

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; set overlap
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,y

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

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; set overlap
	lda	THING_STRUCT_IFLAGS,y
	ora	#THING_IFLAG_HEIGHTCHECK
	sta	THING_STRUCT_IFLAGS,y

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
	bne	:+
	jmp	@do_z_friction
:
	; gravity << 4
	ldx	THING_STRUCT_GRAVITY,y
	lda	TAB_SWAP,x
	and	#$F0
	sta	MATH_VAR_L
	lda	TAB_SWAP,x
	and	#$0F
	sta	MATH_VAR_H

	; get type
	ldx	TICKER_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; direction check
	lda	THTYPE_STRUCT_WATER_HEIGHT,x
	cmp	THTYPE_STRUCT_VIEW_HEIGHT,x
	bcc	@float

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; on floor check
	sec
	lda	THING_STRUCT_FLOORZ_L,y
	sbc	ARG_THING_Z_L
	lda	THING_STRUCT_FLOORZ_H,y
	sbc	ARG_THING_Z_H
	bpl	@no_z_friction

	; flip direction
	sec
	lda	#$00
	sbc	MATH_VAR_L
	sta	MATH_VAR_L
	lda	#$00
	sbc	MATH_VAR_H
	sta	MATH_VAR_H
@float:
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

	bra	@do_z_friction

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
	ldx	RET_THING_IDX

	; if(th->eflags & THING_EFLAG_PROJECTILE)
	lda	THING_STRUCT_EFLAGS,x
	bmi	@no_z_friction

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

	; repeat
	lda	THTICK_REPEAT
	beq	:+

	dec	THTICK_REPEAT
	jmp	@try_xy_move
:
	rts

;;;
; thing ticker (animation)
;;;

thing_tick_anim:
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

thing_actnext:
	; reset
	stz	TMP_ACTION_TICKADD

	; restore
	ldy	G_TICKER_IDX

	; check for STOP
	and	#$07
	bne	:+
	cpx	#0
	bne	:+

	; delete
	jmp	delete_thing
:
	; thing states bank
	ora	#BANK_THING_STATES
	sta	REG_RAM_BANK
	sta	THTICK_ACTION_ST_BNK

	; action
	lda	THSTATE_STRUCT_ACTION,x
	asl
	beq	:+

	; backup
	stx	THTICK_ACTION_ST_IDX

	; run action
	tax
	jmp	(action_table-2,x)

thing_actdone:
	; restore
	ldx	THTICK_ACTION_ST_IDX

	; thing states bank
	lda	THTICK_ACTION_ST_BNK
	sta	REG_RAM_BANK
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
	lda	#$FF
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
	clc
	lda	THSTATE_STRUCT_TICKS,x
	adc	TMP_ACTION_TICKADD
	pha	; <ticks>

	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; ticker
	ldy	G_TICKER_IDX

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

delete_thing:
	; remove ticker
	jsr	tick_del

	; unlink pointers
	lda	G_TICKER_TOP
@loop_del:
	tax
	lda	TICKER_STRUCT_TYPE,x
	bmi	@next

	; origin
	lda	THING_STRUCT_TH_ORIGIN,x
	cmp	G_TICKER_IDX
	bne	:+
	stz	THING_STRUCT_TH_ORIGIN,x
:
	; target
	lda	THING_STRUCT_TH_TARGET,x
	cmp	G_TICKER_IDX
	bne	:+
	stz	THING_STRUCT_TH_TARGET,x
:
	; damager
	lda	THING_STRUCT_TH_DAMAGER,x
	cmp	G_TICKER_IDX
	bne	:+
	stz	THING_STRUCT_TH_DAMAGER,x
:
	; next
@next:
	lda	TICKER_STRUCT_NEXT,x
	bne	@loop_del

	; remove from sectors
	stz	THMOVE_SECTOR
	jmp	thing_apply_pos

;;;
; thing ticker (player)
;;;

thing_tick_plyr:
	; ntype = THING_TYPE_PLAYER_N
	lda	#THING_TYPE_PLAYER_N
	sta	PLTICK_NTYPE

	; if(!th->gravity)
	lda	THING_STRUCT_GRAVITY,y
	bne	:+

	; ntype = thing_state->plr_fly
	lda	TAB_PLAYER_F
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

	; ntype = thing_state->plr_swim
	lda	TAB_PLAYER_S
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
	; ntype = thing_state->plr_crouch
	lda	TAB_PLAYER_C
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
	sta	MATH_TEMP_A

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
	; movement and animation
	jsr	thing_tick_full

	; weapon
	ldy	#0
	sty	G_TICKER_IDX
	jmp	thing_tick_anim

