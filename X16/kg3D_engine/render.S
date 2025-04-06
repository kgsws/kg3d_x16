.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "tables.inc"
.include "main.inc"
.include "math.inc"
.include "map.inc"
.include "engine.inc"
.include "video.inc"
.include "things.inc"

.export render_init
.export render_view
.export render_set_detail

;;;
; NOTES
;;;
; Affine helper X and Y is swapped.

;;;
; projected sprite
;;;

SPRITE_FIRST_PROP = $A0

;;;
; projected masked wall
;;;

MASKED_FIRST_PROP = $F0

;;;
; RAM layout
;;;
; specific interleaving of plane and clipping arrays is crucial
; plane ranges are 0 to 79, but -1 (= 255) is also used

RENDER_PLF_TOP = TAB_TEMP_DATA + $0000
RENDER_CLIP_BOT = TAB_TEMP_DATA + $0058
RENDER_TEXSCALE_L = TAB_TEMP_DATA + $00A8
RENDER_PLF_BOT = TAB_TEMP_DATA + $0100
RENDER_CLIP_TOP = TAB_TEMP_DATA + $0158
RENDER_TEXSCALE_H = TAB_TEMP_DATA + $01A8
RENDER_PLC_TOP = TAB_TEMP_DATA + $0200
RENDER_PLANE_X = TAB_TEMP_DATA + $0258
RENDER_PLC_BOT = TAB_TEMP_DATA + $0300
RENDER_TEXTURE_X = TAB_TEMP_DATA + $0358

;;;
; CODE
;;;

.segment "CODE_RENDER"

;;;
; render init
;;;

render_init:
	; prepare planes
	stz	RENDER_PLF_BOT+80
	stz	RENDER_PLC_BOT+80
	stz	RENDER_PLF_BOT+255
	stz	RENDER_PLC_BOT+255

	; prepare weapon
	lda	#$8E
	sta	G_WPN_VERA_SLOT
	lda	#$FF
	sta	G_WPN_NOW_LIGHT
	stz	G_WPN_NOW_OFFS_L
	stz	G_WPN_NOW_OFFS_H
	stz	G_WPN_AVG

	rts

;;;
; set rendering detail mode
;;;
; Z flag: 0 = hight, pos = low, neg = toggle

render_set_detail:
	bpl	:+

	; get current mode
	lda	detcode_wall_vline
	and	#$01
	beq	:+
	pha
	jsr	video_lowcrl
	pla
:
	bne	:+

	; texturing
	lda	#$EA	; NOP opcode
	sta	opcode_texture_map

	; walls
	lda	#$A9	; LDA opcode
	sta	detcode_wall_vline

	; masked
	lda	#$A9	; LDA opcode
	sta	detcode_masked_vline

	; sprites
	lda	#$A9	; LDA opcode
	sta	detcode_sprite_vline

	; sky planes
	lda	#$A9	; LDA opcode
	sta	detcode_sky_line

	; planes
	lda	#<video_draw_hline
	sta	detcode_hline+1
	lda	#>video_draw_hline
	sta	detcode_hline+2
	lda	#$80	; BRA opcode
	sta	detcode_hscale

	rts
:
	; texturing
	lda	#$E8	; INX opcode
	sta	opcode_texture_map

	; walls
	lda	#$80	; BRA opcode
	sta	detcode_wall_vline

	; masked
	lda	#$B0	; BCS opcode
	sta	detcode_masked_vline

	; sprites
	lda	#$B0	; BCS opcode
	sta	detcode_sprite_vline

	; sky planes
	lda	#$80	; BRA opcode
	sta	detcode_sky_line

	; planes
	lda	#<video_draw_hline_low
	sta	detcode_hline+1
	lda	#>video_draw_hline_low
	sta	detcode_hline+2
	lda	#$A9	; LDA opcode
	sta	detcode_hscale

	rts

;;;
; render view
;;;

render_view:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	;; setup camera position
	ldy	G_CAMERA_THING

	; X
	lda	THING_STRUCT_X_L,y
	sta	PROJECTION_X_L
	lda	THING_STRUCT_X_H,y
	sta	PROJECTION_X_H

	; Y
	lda	THING_STRUCT_Y_L,y
	sta	PROJECTION_Y_L
	lda	THING_STRUCT_Y_H,y
	sta	PROJECTION_Y_H

	; Z (+ viewheight)
	clc
	lda	THING_STRUCT_Z_L,y
	adc	G_VIEWHEIGHT
	sta	PROJECTION_Z_L
	lda	THING_STRUCT_Z_H,y
	adc	#0
	sta	PROJECTION_Z_H

	; angle
	lda	THING_STRUCT_ANGLE,y
	sta	PROJECTION_A8

	;; Y center

	; get pitch
	lda	THING_STRUCT_PITCH,y
	eor	#$80
	tax

	; Y center bank
	lda	#BANK_Y_CENTER
	sta	REG_RAM_BANK

	; get center (walls)
	lda	TAB_PITCH2YC,x
	sta	PROJECTION_YCW

	; get center (planes)
	clc
	lda	#128
	sbc	PROJECTION_YCW
	sta	PROJECTION_YCP

	;; get sector of this thing

	; thing sectors bank
	lda	#BANK_THING_SECTORS
	sta	REG_RAM_BANK

	; thing sector pointer
	lda	$A000,y
	sta	PROJECTION_SECTOR
	tax

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; palette
	lda	G_DAMAGE_LEVEL
	lsr
	lsr
	lsr
	ora	SECTOR_STRUCT_FLAGS,x
	and	#$0F
	sta	G_PALETTE_NEW

	; light
	lda	SECTOR_STRUCT_FLAGS,x
	and	#$70
	sta	G_WPN_LIGHT_NEW

	;; limit view Z

	; limit = sec->ceiling.height - 2
	sec
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sbc	#2
	sta	MATH_VAR_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sbc	#0
	sta	MATH_VAR_H

	; if(projection.z > limit)
	sec
	lda	MATH_VAR_L
	sbc	PROJECTION_Z_L
	lda	MATH_VAR_H
	sbc	PROJECTION_Z_H
	bpl	:+

	; projection.z = limit
	lda	MATH_VAR_L
	sta	PROJECTION_Z_L
	lda	MATH_VAR_H
	sta	PROJECTION_Z_H
:
	; limit = sec->floor.height + 2
	clc
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	adc	#2
	sta	MATH_VAR_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	adc	#0
	sta	MATH_VAR_H

	; if(projection.z < limit)
	sec
	lda	PROJECTION_Z_L
	sbc	MATH_VAR_L
	lda	PROJECTION_Z_H
	sbc	MATH_VAR_H
	bpl	:+

	; projection.z = limit
	lda	MATH_VAR_L
	sta	PROJECTION_Z_L
	lda	MATH_VAR_H
	sta	PROJECTION_Z_H
:
	;; precalculate rendering stuff

	; generate 12bit angle
	ldx	PROJECTION_A8
	lda	TAB_SWAP,x
	tax
	and	#$F0
	sta	PROJECTION_A_L
	txa
	and	#$0F
	sta	PROJECTION_A_H

	; projection.sin = tab_sin[camera.a]
	ldx	PROJECTION_A8
	math_read_sin	PROJECTION_SIN_L, PROJECTION_SIN_H

	; projection.cos = tab_cos[camera.a]
	math_read_cos	PROJECTION_COS_L, PROJECTION_COS_H

	;; clear buffers

	; reset sprites
	stz	PROJECTION_SPRITE_IDX
	stz	PROJECTION_MASKED_IDX

	; reset clipping and depth
	lda	#120
	ldx	#80
@loop_clip:
	dex
	stz	RENDER_TEXSCALE_L,x
	stz	RENDER_TEXSCALE_H,x
	stz	RENDER_CLIP_TOP,x
	sta	RENDER_CLIP_BOT,x
	dex
	stz	RENDER_TEXSCALE_L,x
	stz	RENDER_TEXSCALE_H,x
	stz	RENDER_CLIP_TOP,x
	sta	RENDER_CLIP_BOT,x
	dex
	stz	RENDER_TEXSCALE_L,x
	stz	RENDER_TEXSCALE_H,x
	stz	RENDER_CLIP_TOP,x
	sta	RENDER_CLIP_BOT,x
	dex
	stz	RENDER_TEXSCALE_L,x
	stz	RENDER_TEXSCALE_H,x
	stz	RENDER_CLIP_TOP,x
	sta	RENDER_CLIP_BOT,x
	bne	@loop_clip

	;; prepare portals

	; reset portals
	stz	PROJECTION_PORTAL_RD
	lda	#1
	sta	PROJECTION_PORTAL_WR

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; setup first portal
	lda	PROJECTION_SECTOR
	sta	MEM_PORTALS_SECTOR
	stz	MEM_PORTALS_X0
	lda	#80
	sta	MEM_PORTALS_X1
	lda	#$FF
	sta	MEM_PORTALS_MASKED

	;; render loop

@loop_render:
	; while(portal_rd < portal_wr)
	ldy	PROJECTION_PORTAL_RD
	cpy	PROJECTION_PORTAL_WR
	bcs	@render_done

	; get sector pointer
	lda	MEM_PORTALS_SECTOR,y
	beq	@skip_render
	sta	SECTOR_PTR_I

	; get X0
	lda	MEM_PORTALS_X0,y
	sta	PROJECTION_X0
	asl
	sta	PROJECTION_X0D

	; get X1
	lda	MEM_PORTALS_X1,y
	sta	PROJECTION_X1
	asl
	sta	PROJECTION_X1D

	; reset last portal
	sty	PROJECTION_PORTAL_LAST

	; render sector
	jsr	render_sector

@skip_render:
	; portal_rd++
	inc	PROJECTION_PORTAL_RD

	; next
	bra	@loop_render

@render_done:
	; dcsel = 3, addrsel = 0
	lda	#$06
	sta	VERA_CTRL

	; reset affine X increment
	stz	VERA_FX_Y_INCR_L
	stz	VERA_FX_Y_INCR_H

	; dcsel = 4, addrsel = 0
	lda	#$08
	sta	VERA_CTRL

	; set affine X position (256px, mirrored)
	lda	#1
	sta	VERA_FX_Y_POS_L
	stz	VERA_FX_Y_POS_H

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; vera FX
	lda	#$F8
	sta	VERA_FX_TILEBASE
	lda	#$FE
	sta	VERA_FX_MAPBASE

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; sprites and masked
@loop_sprmsk:
	; portal_rd--
	dec	PROJECTION_PORTAL_RD
	ldy	PROJECTION_PORTAL_RD

	; if(!portal_rd)
	bmi	@end_sprite

	; idx = portal_rd->first_sprite
	lda	MEM_PORTALS_SPRITE,y

	; if(idx & 0x80)
	bmi	@skip_sprite

@next_sprite:
	; pointer
	ora	#$A0
	sta	HI_PTR_H

	; render
	jsr	render_sprite

	; sprite bank
	lda	#BANK_DRAW_SPRITES
	sta	REG_RAM_BANK

	; idx = proj_spr[idx].next
	ldy	#SPRITE_FIRST_PROP+2
	lda	(HI_PTR_L),y

	; if(!(idx & 0x80))
	bpl	@next_sprite

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

@skip_sprite:
	; masked
	ldy	PROJECTION_PORTAL_RD
	lda	MEM_PORTALS_MASKED,y
	bmi	@loop_sprmsk

	; render
	jsr	render_masked

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; next
	bra	@loop_sprmsk

@end_sprite:
	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	;;; PALETTE

	; palette check
	lda	G_PALETTE_NEW
	cmp	G_PALETTE_NOW
	beq	:+

	; update palette
	sta	G_PALETTE_NOW
	jsr	video_palette
:
	;;; WEAPON

	; weapon check
	lda	#$FF
	sta	TMP_REND_WPN_IDX

	; if(camera_thing == player_thing)
	lda	G_PLAYER_THING
	cmp	G_CAMERA_THING
	bne	@wspr_no_gfx

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; show_wpn.idx = things[0].sprite
	lda	THING_STRUCT_SPRITE
	sta	TMP_REND_WPN_IDX

	; check sprite
	lda	TMP_REND_WPN_IDX
	bmi	@wspr_no_gfx

	jsr	update_weapon_gfx

@wspr_no_gfx:
	; prepare sprite update
	inc	TMP_PTR_L
	inc	TMP_PTR_L
	ldx	#15

	; ADDR0 = 0x1FF2E, increment = 8
	lda	#$2E
	sta	VERA_ADDRx_L
	lda	#$FF
	sta	VERA_ADDRx_M
	lda	#%01000001
	sta	VERA_ADDRx_H

	; check if empty
	lda	TMP_REND_WPN_IDX
	bmi	@loop_wspr_inv

	; XY swing
	jsr	update_weapon_swing
	ldx	#15

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; wsprite frame bank
	lda	#BANK_WSPR_INFO
	sta	REG_RAM_BANK

	; ADDR0 = 0x1FF28, increment = 1
	lda	#$28
	sta	VERA_ADDRx_L
	lda	#$FF
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; copy valid sprites
@loop_wspr_prt:
	; addr (lo)
	lda	(TMP_PTR_L)
	bmi	@do_wspr_inv
	sta	VERA_DATA0
	inc	TMP_PTR_L

	; addr (hi)
	lda	G_WPN_VERA_SLOT
	sta	VERA_DATA0

	; X
	clc
	lda	(TMP_PTR_L)
	adc	MATH_VAR_L
	sta	VERA_DATA0
	lda	#0
	adc	MATH_VAR_H
	sta	VERA_DATA0
	inc	TMP_PTR_L

	; Y
	clc
	lda	(TMP_PTR_L)
	adc	MATH_VAR_U
	clc
	adc	MATH_COUNTER
	sta	VERA_DATA0
	stz	VERA_DATA0
	inc	TMP_PTR_L

	; infoA
	lda	(TMP_PTR_L)
	sta	VERA_DATA0

	; infoB
	and	#$F0
	sta	VERA_DATA0
	inc	TMP_PTR_L

	; next
	dex
	bne	@loop_wspr_prt
	bra	@wspr_done

@do_wspr_inv:
	; dummy
	stz	VERA_DATA0
	stz	VERA_DATA0
	lda	#%00110001
	sta	VERA_ADDRx_H
	stz	VERA_DATA0

	; increment = 8
	lda	#%01000001
	sta	VERA_ADDRx_H

	; invalidate the rest
@loop_wspr_inv:
	stz	VERA_DATA0
	dex
	bne	@loop_wspr_inv
@wspr_done:

	; done
	rts

;;;
; render_sector
;;;

render_sector:
	; fix leaking planes
	lda	#$F0
	ldx	PROJECTION_X0
	sta	RENDER_PLC_TOP,x
	sta	RENDER_PLF_TOP,x
	ldx	PROJECTION_X1
	dex
	sta	RENDER_PLC_TOP,x
	sta	RENDER_PLF_TOP,x

	; reset sprites
	lda	#$FF
	sta	PROJECTION_FIRST_SPRITE

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; light level
	ldy	SECTOR_PTR_I
	ldx	SECTOR_STRUCT_FLAGS,y
	lda	TAB_SWAP,x
	and	#$07
	sta	PROJECTION_LIGHT

	;; prepare sprites in this sector

	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK

	; the loop
	lda	#$A0
	sta	SPR_PTR_H
@loop_spr_prep:
	; get thing at this slot
	lda	(SPR_PTR_L),y

	; empty slot check
	beq	:+

	; visiblity check
	cmp	G_CAMERA_THING
	beq	:+

	; prepare sprite
	jsr	prepare_sprite
	ldy	SECTOR_PTR_I

	; sector things bank
	lda	#BANK_SECTOR_THINGS
	sta	REG_RAM_BANK
:
	inc	SPR_PTR_H
	lda	SPR_PTR_H
	cmp	#$BF
	bne	@loop_spr_prep

	;; sector projection

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	;; walls
	jsr	render_walls

	;; PLANES

	;; floor

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; check for sky
	ldy	SECTOR_PTR_I
	lda	SECTOR_STRUCT_FLOOR_TEXTURE,y
	ina
	bne	:+

	; disable cache write
	stz	VERA_FX_CTRL

	; sky
	lda	#>RENDER_PLF_TOP
	sta	PLANE_PTR_TOP_H
	lda	#>RENDER_PLF_BOT
	sta	PLANE_PTR_BOT_H
	jsr	render_sky_plane

	bra	:++
:
	; height = projection.z - sector.height_floor
	ldy	SECTOR_PTR_I
	sec
	lda	PROJECTION_Z_L
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sta	PLANE_HEIGHT_L
	lda	PROJECTION_Z_H
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	bmi	:+
	sta	PLANE_HEIGHT_H
	ora	PLANE_HEIGHT_L
	beq	:+

	; texture offsets
	lda	SECTOR_STRUCT_FLOOR_OX,y
	sta	RENDER_TEXTURE_OX
	lda	SECTOR_STRUCT_FLOOR_OY,y
	sta	RENDER_TEXTURE_OY
	lda	SECTOR_STRUCT_FLOOR_ANG,y
	sta	RENDER_TEXTURE_ANGLE

	; texture
	stz	WALL_TEXTURE_FLAGS
	lda	SECTOR_STRUCT_FLOOR_TEXTURE,y
	jsr	texture_set

	; enable math
	jsr	math_setup_vera

	; texture effect
	jsr	plane_apply_effect

	; texture angle
	jsr	plane_set_angle

	; plane
	lda	#>RENDER_PLF_TOP
	sta	PLANE_PTR_TOP_H
	lda	#>RENDER_PLF_BOT
	sta	PLANE_PTR_BOT_H
	jsr	render_plane
:
	;; ceiling

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; check for sky
	ldy	SECTOR_PTR_I
	lda	SECTOR_STRUCT_CEILING_TEXTURE,y
	ina
	bne	:+

	; disable cache write
	stz	VERA_FX_CTRL

	; sky
	lda	#>RENDER_PLC_TOP
	sta	PLANE_PTR_TOP_H
	lda	#>RENDER_PLC_BOT
	sta	PLANE_PTR_BOT_H
	jsr	render_sky_plane

	bra	:++
:
	; height = projection.z - sector.height_ceiling
	ldy	SECTOR_PTR_I
	sec
	lda	PROJECTION_Z_L
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	sta	PLANE_HEIGHT_L
	lda	PROJECTION_Z_H
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	bpl	:+
	sta	PLANE_HEIGHT_H

	; texture offsets
	lda	SECTOR_STRUCT_CEILING_OX,y
	sta	RENDER_TEXTURE_OX
	lda	SECTOR_STRUCT_CEILING_OY,y
	sta	RENDER_TEXTURE_OY
	lda	SECTOR_STRUCT_CEILING_ANG,y
	sta	RENDER_TEXTURE_ANGLE

	; texture
	stz	WALL_TEXTURE_FLAGS
	lda	SECTOR_STRUCT_CEILING_TEXTURE,y
	jsr	texture_set

	; enable math
	jsr	math_setup_vera

	; texture effect
	jsr	plane_apply_effect

	; texture angle
	jsr	plane_set_angle

	; plane
	lda	#>RENDER_PLC_TOP
	sta	PLANE_PTR_TOP_H
	lda	#>RENDER_PLC_BOT
	sta	PLANE_PTR_BOT_H
	jsr	render_plane
:
	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache write
	stz	VERA_FX_CTRL

	; sprite bank
	lda	#BANK_DRAW_SPRITES
	sta	REG_RAM_BANK

	; finish sprites in this sector
	lda	PROJECTION_FIRST_SPRITE
@loop_sprites:
	bmi	@end_sprites

	; pointer
	ora	#$A0
	sta	HI_PTR_H

	; structure - first property
	ldy	#SPRITE_FIRST_PROP

	; spr->depth
	lda	(HI_PTR_L),y
	sta	SPRITE_DEPTH_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_DEPTH_H

	; spr->next
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_NEXT

	; spr->x0
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_X0_L

	; spr->x1
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_X1_L

	; other depth check
	lda	SPRITE_X0_L
	lsr
	tay
	asl
	sta	SPRITE_X0_L
	sec
	lda	SPRITE_X1_L
	ina
	sbc	SPRITE_X0_L
	lsr
	tax
@loop_depth:
	; if(spr->depth < tmap_scale[x0])
	sec
	lda	SPRITE_DEPTH_L
	sbc	RENDER_TEXSCALE_L,y
	sta	MATH_TEMP
	lda	SPRITE_DEPTH_H
	sbc	RENDER_TEXSCALE_H,y
	bpl	:+
	ora	MATH_TEMP
	beq	:+

	; spr->clip_top[x0] = 120
	lda	#120
	sta	(HI_PTR_L),y

:
	; next
	iny
	dex
	bne	@loop_depth

	; next
	lda	SPRITE_NEXT

	bra	@loop_sprites

@end_sprites:
	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; save first sprite
	ldy	PROJECTION_PORTAL_RD
	lda	PROJECTION_FIRST_SPRITE
	sta	MEM_PORTALS_SPRITE,y

	; done
	rts

;;;
; render_walls
;;;

render_walls:
	ldy	SECTOR_PTR_I
	lda	SECTOR_STRUCT_WALLS_L,y
	sta	WALN_PTR_L
	lda	SECTOR_STRUCT_WALLS_H,y
	sta	WALN_PTR_H
	tax

	; wall pointer
	and	#$1F
	ora	#$A0
	sta	WALN_PTR_H
	lda	TAB_BANK,x
	ora	#BANK_MAPDATA
	sta	WALL_PTR_B

	; last_angle = 'invalid'
	lda	#$80
	sta	WALL_LAST_ANGLE_H

@loop:
	; wall pointer
	lda	WALN_PTR_L
	sta	WALL_PTR_L
	lda	WALN_PTR_H
	sta	WALL_PTR_H

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; reset accumulator, set addition, enable multiplication
	lda	#%10010000
	sta	VERA_FX_MULT

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

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

	; wall->angle & MARK_XPEG
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	sta	WALL_XPEG

	; WALL
	jsr	render_wall

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; check for STOP
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	bpl	@loop

	rts

;;;
; render plane
;;;

render_plane:
	; set bounds
	lda	#$FF
	ldy	PROJECTION_X1
	sta	(PLANE_PTR_TOP_L),y
	ldy	PROJECTION_X0
	dey
	sty	PLANE_LOOP_X
	sta	(PLANE_PTR_TOP_L),y

@loopX:
	; load previous column
	lda	(PLANE_PTR_TOP_L),y
	sta	PLANE_LOOP_T0
	lda	(PLANE_PTR_BOT_L),y
	sta	PLANE_LOOP_B0

	; load current column
	iny
	lda	(PLANE_PTR_TOP_L),y
	sta	PLANE_LOOP_T1
	lda	(PLANE_PTR_BOT_L),y
	sta	PLANE_LOOP_B1

	; while(t0 < t1 && t0 <= b0)
@loop0:
	lda	PLANE_LOOP_T0
	cmp	PLANE_LOOP_T1
	bcs	@end0
	cmp	PLANE_LOOP_B0
	beq	:+
	bcs	@end0
:
	tax
	lda	RENDER_PLANE_X,x
	jsr	draw_span
	inc	PLANE_LOOP_T0
	bra	@loop0
@end0:

	; while(b0 > b1 && b0 >= t0)
@loop1:
	lda	PLANE_LOOP_B1
	cmp	PLANE_LOOP_B0
	bcs	@end1
	lda	PLANE_LOOP_B0
	cmp	PLANE_LOOP_T0
	bcc	@end1
	tax
	lda	RENDER_PLANE_X,x
	jsr	draw_span
	dec	PLANE_LOOP_B0
	bra	@loop1
@end1:

	; while(t1 < t0 && t1 <= b1)
@loop2:
	lda	PLANE_LOOP_T1
	cmp	PLANE_LOOP_T0
	bcs	@end2
	cmp	PLANE_LOOP_B1
	beq	:+
	bcs	@end2
:
	tax
	tya
	sta	RENDER_PLANE_X,x
	inc	PLANE_LOOP_T1
	bra	@loop2
@end2:

	; while(b1 > b0 && b1 >= t1)
@loop3:
	lda	PLANE_LOOP_B0
	cmp	PLANE_LOOP_B1
	bcs	@end3
	lda	PLANE_LOOP_B1
	cmp	PLANE_LOOP_T1
	bcc	@end3
	tax
	tya
	sta	RENDER_PLANE_X,x
	dec	PLANE_LOOP_B1
	bra	@loop3
@end3:

	; next
	inc	PLANE_LOOP_X
	ldy	PLANE_LOOP_X
	cpy	PROJECTION_X1
	bcc	@loopX

	rts

;;;
; draw span
;;;
; A is x0
; X is y
; Y is x1

draw_span:
	sty	PLANE_TEMP
	asl
	sta	PLANE_DRAW_X0
	tya
	asl
	sta	PLANE_DRAW_X1
	stx	PLANE_DRAW_Y
	stz	PLANE_DRAW_SX_H

	;; sx = x0 - 80
	sec
	lda	PLANE_DRAW_X0
	sbc	#80
	sta	PLANE_DRAW_SX_L
	bpl	:+
	lda	#$FF
	sta	PLANE_DRAW_SX_H
:
	; plane X bank
	lda	#BANK_PLANE_X
	sta	REG_RAM_BANK

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	;; step = (height * tab_planex[y + projection.ycp]) >> 7

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; height
	lda	PLANE_HEIGHT_L
	sta	VERA_FX_CACHE_L
	lda	PLANE_HEIGHT_H
	sta	VERA_FX_CACHE_M

	; tab_planex[y + projection.ycp]
	clc
	txa
	adc	PROJECTION_YCP
	tay
	lda	TAB_PLANEX_L,y
	sta	VERA_FX_CACHE_H
	lda	TAB_PLANEX_H,y
	sta	VERA_FX_CACHE_U

	; accumulate (double the value)
	lda	VERA_FX_ACCUM

	; save result
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	PLANE_DRAW_STEP_L
	lda	VERA_DATA0
	sta	PLANE_DRAW_STEP_H

	;; xstep = (step * projection.pl_cos) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; step
	lda	PLANE_DRAW_STEP_L
	sta	VERA_FX_CACHE_L
	lda	PLANE_DRAW_STEP_H
	sta	VERA_FX_CACHE_M

	; projection.pl_cos
	lda	PLANE_COS_L
	sta	VERA_FX_CACHE_H
	lda	PLANE_COS_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	PLANE_DRAW_XS_L
	lda	VERA_DATA0
	sta	PLANE_DRAW_XS_H

	;; ystep = (step * projection.pl_sin) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; step is aready set

	; projection.pl_sin
	lda	PLANE_SIN_L
	sta	VERA_FX_CACHE_H
	lda	PLANE_SIN_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	PLANE_DRAW_YS_L
	lda	VERA_DATA0
	sta	PLANE_DRAW_YS_H

	;; xnow = xstep * sx + ystep * 80

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; xstep
	lda	PLANE_DRAW_XS_L
	sta	VERA_FX_CACHE_L
	lda	PLANE_DRAW_XS_H
	sta	VERA_FX_CACHE_M

	; sx
	lda	PLANE_DRAW_SX_L
	sta	VERA_FX_CACHE_H
	lda	PLANE_DRAW_SX_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; ystep
	lda	PLANE_DRAW_YS_L
	sta	VERA_FX_CACHE_L
	lda	PLANE_DRAW_YS_H
	sta	VERA_FX_CACHE_M

	; +80
	lda	#80
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	PLANE_POS_X_L
	sta	PLANE_DRAW_XN_L
	lda	VERA_DATA0
	adc	PLANE_POS_X_H
	sta	PLANE_DRAW_XN_H

	;; ynow = ystep * sx - xstep * 80

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; ystep is already set

	; sx
	lda	PLANE_DRAW_SX_L
	sta	VERA_FX_CACHE_H
	lda	PLANE_DRAW_SX_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; xstep
	lda	PLANE_DRAW_XS_L
	sta	VERA_FX_CACHE_L
	lda	PLANE_DRAW_XS_H
	sta	VERA_FX_CACHE_M

	; -80
	lda	#$B0
	sta	VERA_FX_CACHE_H
	lda	#$FF
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result
	stz	VERA_ADDRx_L
	sec
	lda	VERA_DATA0
	sbc	PLANE_POS_Y_L
	sta	PLANE_DRAW_YN_L
	lda	VERA_DATA0
	sbc	PLANE_POS_Y_H
	sta	PLANE_DRAW_YN_H

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache, enable affine helper
	lda	#%00000011
	sta	VERA_FX_CTRL

	; dcsel = 3, addrsel = 0
	lda	#$06
	sta	VERA_CTRL

detcode_hscale:
	bra	:+

	; X
	lda	PLANE_DRAW_XS_L
	asl
	sta	PLANE_DRAW_XS_L
	lda	PLANE_DRAW_XS_H
	rol
	sta	PLANE_DRAW_XS_H

	; X
	lda	PLANE_DRAW_YS_L
	asl
	sta	PLANE_DRAW_YS_L
	lda	PLANE_DRAW_YS_H
	rol
	sta	PLANE_DRAW_YS_H
:

	; set affine X step
	lda	PLANE_DRAW_XS_L
	asl
	sta	VERA_FX_Y_INCR_L
	lda	PLANE_DRAW_XS_H
	rol
	and	#$7F
	sta	VERA_FX_Y_INCR_H

	; set affine Y step
	lda	PLANE_DRAW_YS_L
	asl
	sta	VERA_FX_X_INCR_L
	lda	PLANE_DRAW_YS_H
	rol
	and	#$7F
	sta	VERA_FX_X_INCR_H

	; dcsel = 5, addrsel = 0
	lda	#$0A
	sta	VERA_CTRL

	; set affine X sub
	lda	PLANE_DRAW_XN_L
	sta	VERA_FX_Y_POS_S

	; set affine Y sub
	lda	PLANE_DRAW_YN_L
	sta	VERA_FX_X_POS_S

	; dcsel = 4, addrsel = 0
	lda	#$08
	sta	VERA_CTRL

	; set affine X position
	clc
	lda	PLANE_DRAW_XN_H
	adc	RENDER_TEXTURE_OX
	sta	VERA_FX_Y_POS_L
	stz	VERA_FX_Y_POS_H

	; set affine Y position
	clc
	lda	PLANE_DRAW_YN_H
	adc	RENDER_TEXTURE_OY
	sta	VERA_FX_X_POS_L
	stz	VERA_FX_X_POS_H

	; draw span
detcode_hline:
	jsr	video_draw_hline	; modified address

	ldy	PLANE_TEMP
	rts

;;;
; render sky
;;;

render_sky_plane:
	ldy	PROJECTION_X0
	sty	PLANE_LOOP_X
sky_plane_loop:
	; get top clipping
	lda	(PLANE_PTR_TOP_L),y
	sta	VIDEO_DRAW_YY

	; skip check
	cmp	#$F0
	beq	:+

	; get bot clipping
	lda	(PLANE_PTR_BOT_L),y
	sta	PLANE_DRAW_Y1

	; length
	sec
	sbc	VIDEO_DRAW_YY
	bmi	:+
	ina
	sta	VIDEO_DRAW_L

	; y0--
	dec	VIDEO_DRAW_YY

	; double X
	tya
	asl
	sta	VIDEO_DRAW_X

	; draw
	jsr	video_draw_sline

	; increment doubled X
	inc	VIDEO_DRAW_X

	; skip odd column
detcode_sky_line:
	bra	:+	; modified opcode

	; draw
	jsr	video_draw_sline
:
	; next
	inc	PLANE_LOOP_X
	ldy	PLANE_LOOP_X
	cpy	PROJECTION_X1
	bcc	sky_plane_loop

	rts

;;;
; render wall
;;;

render_wall:
	;; V0 diff

	; d0.x = v0->x - (projection.x >> 8)
	sec
	ldy	#WALL_STRUCT_VTX_X
	lda	(WALL_PTR_L),y
	sbc	PROJECTION_X_L
	sta	WALL_D0_X_L
	iny
	lda	(WALL_PTR_L),y
	sbc	PROJECTION_X_H
	sta	WALL_D0_X_H

	; d0.y = v0->y - (projection.y >> 8)
	sec
	ldy	#WALL_STRUCT_VTX_Y
	lda	(WALL_PTR_L),y
	sbc	PROJECTION_Y_L
	sta	WALL_D0_Y_L
	iny
	lda	(WALL_PTR_L),y
	sbc	PROJECTION_Y_H
	sta	WALL_D0_Y_H

	;; get distance Y
	; ld.y = (d0.x * wall->dist.y - d0.y * wall->dist.x) >> 8;

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.x
	lda	WALL_D0_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D0_X_H
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
	sbc	WALL_D0_Y_L
	sta	VERA_FX_CACHE_L
	lda	#$00
	sbc	WALL_D0_Y_H
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
	sta	WALL_LD_Y_L
	lda	VERA_DATA0
	sta	WALL_LD_Y_H

	; if(ld.y <= 0)
	bpl	:+

@invalidate:
	; last_angle = 'invalid'
	lda	#$80
	sta	WALL_LAST_ANGLE_H

	; inside the wall, stop
	rts
:
	; inside = 0
	stz	WALL_PORTAL_INSIDE

	; if(ld.y <= 1)
	bne	@done_inside
	lda	WALL_LD_Y_L
	cmp	#2
	bcs	@done_inside

	; inside line
	sty	WALL_PORTAL_INSIDE

	; if(ld.y <= 0)
	lda	WALL_LD_Y_L
	bne	@done_inside

	; check projection sector
	lda	PROJECTION_SECTOR
	cmp	SECTOR_PTR_I
	beq	@done_inside

	; inside the wall, stop
	bra	@invalidate

@done_inside:
	;; V0 angle
	lda	WALL_LAST_ANGLE_L
	sta	WALL_A0_L
	lda	WALL_LAST_ANGLE_H
	bpl	@skip_a0

	; p2a_coord.x = d0.x
	lda	WALL_D0_X_L
	sta	MATH_P2A_X_L
	lda	WALL_D0_X_H
	sta	MATH_P2A_X_H

	; p2a_coord.y = d0.y
	lda	WALL_D0_Y_L
	sta	MATH_P2A_Y_L
	lda	WALL_D0_Y_H
	sta	MATH_P2A_Y_H

	; a0 = point_to_angle()
	jsr	math_p2a
	lda	MATH_VAR_L
	sta	WALL_A0_L
	lda	MATH_VAR_H

@skip_a0:
	sta	WALL_A0_H

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	;; V1 diff

	; d1.x = v1->x - (projection.x >> 8)
	sec
	ldy	#WALL_STRUCT_VTX_X
	lda	(WALN_PTR_L),y
	sbc	PROJECTION_X_L
	sta	WALL_D1_X_L
	iny
	lda	(WALN_PTR_L),y
	sbc	PROJECTION_X_H
	sta	WALL_D1_X_H

	; d1.y = v1->y - (projection.y >> 8)
	sec
	ldy	#WALL_STRUCT_VTX_Y
	lda	(WALN_PTR_L),y
	sbc	PROJECTION_Y_L
	sta	WALL_D1_Y_L
	iny
	lda	(WALN_PTR_L),y
	sbc	PROJECTION_Y_H
	sta	WALL_D1_Y_H

	;; V1 angle

	; p2a_coord.x = d1.x
	lda	WALL_D1_X_L
	sta	MATH_P2A_X_L
	lda	WALL_D1_X_H
	sta	MATH_P2A_X_H

	; p2a_coord.y = d1.y
	lda	WALL_D1_Y_L
	sta	MATH_P2A_Y_L
	lda	WALL_D1_Y_H
	sta	MATH_P2A_Y_H

	; a1 = point_to_angle()
	jsr	math_p2a
	lda	MATH_VAR_L
	sta	WALL_A1_L
	sta	WALL_LAST_ANGLE_L
	lda	MATH_VAR_H
	sta	WALL_A1_H
	sta	WALL_LAST_ANGLE_H

	;; side check

	; if((a1 - a0) & 2048)
	sec
	lda	WALL_A1_L
	sbc	WALL_A0_L
	lda	WALL_A1_H
	sbc	WALL_A0_H
	and	#$08
	beq	:++

	; check if inside portal
	lda	WALL_PORTAL_INSIDE
	beq	:+
	lda	WALL_A0_H
	eor	WALL_A1_H
	and	#$08
	bne	:++
:
	; behind the wall, stop
	rts
:
	;; V0 rejection and clipping checks

	stz	WALL_CLIP_LEFT

	; a0 = (a0 - projection.a + 512) & 4095
	sec
	lda	WALL_A0_L
	sbc	PROJECTION_A_L
	sta	WALL_A0_L
	lda	WALL_A0_H
	sbc	PROJECTION_A_H
	clc
	adc	#$02
	and	#$0F
	sta	WALL_A0_H
	tay

	; if(a0 & 0x800)
	and	#$08
	beq	:+
	; clipping required
	sta	WALL_CLIP_LEFT
	bra	@v1_check
:
	; if(a0 >= 0x400)
	tya
	cmp	#$04
	bcc	@v1_check
	; wall is too left, stop
	rts

@v1_check:
	;; V1 rejection and clipping checks

	stz	WALL_CLIP_RIGHT

	; a1 = (a1 - projection.a + 512) & 4095
	sec
	lda	WALL_A1_L
	sbc	PROJECTION_A_L
	sta	WALL_A1_L
	lda	WALL_A1_H
	sbc	PROJECTION_A_H
	clc
	adc	#$02
	and	#$0F
	sta	WALL_A1_H

	; if(a1 >= 0xC00)
	cmp	#$0C
	bcc	:+
	; wall is too right, stop
	rts
:
	; if(a1 >= 0x400)
	cmp	#$04
	bcc	:+
	; clipping required
	sta	WALL_CLIP_RIGHT
:
	; if(a0 & a1 & 0x800)
	lda	#$08
	and	WALL_A0_H
	and	WALL_A1_H
	beq	:+
	; wall is behind camera, stop
	rts
:

	;; angle clipping and X projection

	; a2x bank
	lda	#BANK_A2X
	sta	REG_RAM_BANK

	; x1 = 80
	lda	#80
	sta	WALL_X1

	; if(do_right_clip)
	lda	WALL_CLIP_RIGHT
	bne	:+
	; x1 = angle2x[a1]
	ldy	WALL_A1_L
	lda	WALL_A1_H
	ora	#>TAB_A2X_L
	sta	HI_PTR_H
	lda	(HI_PTR_L),y
	lsr
	sta	WALL_X1
:
	; x0 = 0
	stz	WALL_X0

	; if(do_left_clip)
	lda	WALL_CLIP_LEFT
	bne	:+
	; x0 = angle2x[a0]
	ldy	WALL_A0_L
	lda	WALL_A0_H
	ora	#>TAB_A2X_L
	sta	HI_PTR_H
	lda	(HI_PTR_L),y
	lsr
	sta	WALL_X0
:
	; if(x0 >= x1)
	lda	WALL_X0
	cmp	WALL_X1
	bcc	:+
	; wall does not cross single pixel, stop
	rts
:
	; if(x1 <= projection.x0)
	lda	PROJECTION_X0
	cmp	WALL_X1
	bcc	:+
	; wall is too left, stop
	rts
:
	; if(x0 >= projection.x1)
	lda	WALL_X0
	cmp	PROJECTION_X1
	bcc	:+
	; wall is too right, stop
	rts
:
	;; projection

	;; project Y0
	; p0.y = (d0.x * projection.sin + d0.y * projection.cos) >> 8;

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.x
	lda	WALL_D0_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D0_X_H
	sta	VERA_FX_CACHE_M

	; projection.sin
	lda	PROJECTION_SIN_L
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_SIN_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; d0.y
	lda	WALL_D0_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D0_Y_H
	sta	VERA_FX_CACHE_M

	; projection.cos
	lda	PROJECTION_COS_L
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_COS_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	WALL_P0_Y_L
	lda	VERA_DATA0
	sta	WALL_P0_Y_H

	;; project Y1
	; p1.y = (d1.x * projection.sin + d1.y * projection.cos) >> 8;

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d1.y
	lda	WALL_D1_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D1_Y_H
	sta	VERA_FX_CACHE_M

	; projection.cos is already set

	; accumulate
	lda	VERA_FX_ACCUM

	; d1.x
	lda	WALL_D1_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D1_X_H
	sta	VERA_FX_CACHE_M

	; projection.sin
	lda	PROJECTION_SIN_L
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_SIN_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	WALL_P1_Y_L
	lda	VERA_DATA0
	sta	WALL_P1_Y_H

	;; angle clipping (both)
	lda	WALL_CLIP_LEFT
	ora	WALL_CLIP_RIGHT
	bne	:+
	jmp	@skip_angle_clip
:
	;; check line distance
	lda	WALL_LD_Y_H
	bne	:+
	lda	WALL_LD_Y_L
	cmp	#6
	bcs	:+

	; clear Y distance
	stz	WALL_P0_Y_L
	stz	WALL_P0_Y_H
	stz	WALL_P1_Y_L
	stz	WALL_P1_Y_H

	; skip clipping
	jmp	@skip_angle_clip
:
	;; project X0
	; p0.x = (d0.x * projection.cos - d0.y * projection.sin) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; d0.x
	lda	WALL_D0_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D0_X_H
	sta	VERA_FX_CACHE_M

	; projection.cos
	lda	PROJECTION_COS_L
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_COS_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; -d0.y
	sec
	lda	#$00
	sbc	WALL_D0_Y_L
	sta	VERA_FX_CACHE_L
	lda	#$00
	sbc	WALL_D0_Y_H
	sta	VERA_FX_CACHE_M

	; projection.sin
	lda	PROJECTION_SIN_L
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_SIN_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	WALL_P0_X_L
	lda	VERA_DATA0
	sta	WALL_P0_X_H

	;; project X1
	; p1.x = (-d1.y * projection.sin + d1.x * projection.cos) >> 8;

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; -d1.y
	sec
	lda	#$00
	sbc	WALL_D1_Y_L
	sta	VERA_FX_CACHE_L
	lda	#$00
	sbc	WALL_D1_Y_H
	sta	VERA_FX_CACHE_M

	; projection.sin is already set

	; accumulate
	lda	VERA_FX_ACCUM

	; d1.x
	lda	WALL_D1_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D1_X_H
	sta	VERA_FX_CACHE_M

	; projection.cos
	lda	PROJECTION_COS_L
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_COS_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	WALL_P1_X_L
	lda	VERA_DATA0
	sta	WALL_P1_X_H

	; dc.x = p1.x - p0.x
	sec
	lda	WALL_P1_X_L
	sbc	WALL_P0_X_L
	sta	WALL_DC_X_L
	lda	WALL_P1_X_H
	sbc	WALL_P0_X_H
	sta	WALL_DC_X_H

	; dc.y = p1.y - p0.y
	sec
	lda	WALL_P1_Y_L
	sbc	WALL_P0_Y_L
	sta	WALL_DC_Y_L
	lda	WALL_P1_Y_H
	sbc	WALL_P0_Y_H
	sta	WALL_DC_Y_H

	; inverse division bank
	lda	#BANK_IDIV_L
	sta	REG_RAM_BANK

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	;; left clip
	lda	WALL_CLIP_LEFT
	bne	:+
	jmp	@no_left_clip
:
	;; r = (- p0.x - p0.y) * inv_div[dc.x + dc.y]

	; inv_div[dc.x + dc.y]
	clc
	lda	WALL_DC_X_L
	adc	WALL_DC_Y_L
	sta	MATH_VAR_L
	lda	WALL_DC_X_H
	adc	WALL_DC_Y_H
	sta	MATH_VAR_H
	jsr	math_scale_down
	jsr	math_idiv

	; inv_div
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_L
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_M

	; - p0.x - p0.y
	sec
	lda	#$00
	sbc	WALL_P0_X_L
	sta	MATH_VAR_L
	lda	#$00
	sbc	WALL_P0_X_H
	sta	MATH_VAR_H
	sec
	lda	MATH_VAR_L
	sbc	WALL_P0_Y_L
	sta	VERA_FX_CACHE_H
	lda	MATH_VAR_H
	sbc	WALL_P0_Y_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	MATH_VAR_L
	lda	VERA_DATA0
	sta	MATH_VAR_H
	lda	VERA_DATA0
	sta	MATH_VAR_U

	; special shift
	jsr	math_shift_down

	; save to cache
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_H
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_U
	lda	MATH_VAR_U
	bne	@no_left_clip

	;; p0.y += (dc.y * r) >> 15

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dc.y
	lda	WALL_DC_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_DC_Y_H
	sta	VERA_FX_CACHE_M

	; accumulate (double the value)
	lda	VERA_FX_ACCUM

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; update p0.y (get result >> 16)
	lda	VERA_DATA0
	clc
	lda	VERA_DATA0
	adc	WALL_P0_Y_L
	sta	WALL_P0_Y_L
	lda	VERA_DATA0
	adc	WALL_P0_Y_H
	sta	WALL_P0_Y_H

@no_left_clip:
	;; right clip
	lda	WALL_CLIP_RIGHT
	bne	:+
	jmp	@no_right_clip
:
	;; r = (p1.x - p1.y) * inv_div[dc.x - dc.y]

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; inv_div[dc.x + dc.y]
	sec
	lda	WALL_DC_X_L
	sbc	WALL_DC_Y_L
	sta	MATH_VAR_L
	lda	WALL_DC_X_H
	sbc	WALL_DC_Y_H
	sta	MATH_VAR_H
	jsr	math_scale_down
	jsr	math_idiv

	; inv_div
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_L
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_M

	; p1.x - p1.y
	sec
	lda	WALL_P1_X_L
	sbc	WALL_P1_Y_L
	sta	VERA_FX_CACHE_H
	lda	WALL_P1_X_H
	sbc	WALL_P1_Y_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	MATH_VAR_L
	lda	VERA_DATA0
	sta	MATH_VAR_H
	lda	VERA_DATA0
	sta	MATH_VAR_U

	; special shift
	jsr	math_shift_down

	; save to cache
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_H
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_U
	lda	MATH_VAR_U
	bne	@no_right_clip

	;; p1.y -= (dc.y * r) >> 15

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dc.y
	lda	WALL_DC_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_DC_Y_H
	sta	VERA_FX_CACHE_M

	; accumulate (double the value)
	lda	VERA_FX_ACCUM

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; update p1.y (get result >> 16)
	lda	VERA_DATA0
	sec
	lda	WALL_P1_Y_L
	sbc	VERA_DATA0
	sta	WALL_P1_Y_L
	lda	WALL_P1_Y_H
	sbc	VERA_DATA0
	sta	WALL_P1_Y_H

@no_right_clip:

@skip_angle_clip:
	; depth table bank
	lda	#BANK_YDEPTH_L
	sta	REG_RAM_BANK

	;; Y0 scale
	; NOTE: this overflows at values >= 4096

	; p0.y = tab_depth[p0.y]
	ldy	WALL_P0_Y_L
	lda	WALL_P0_Y_H
	ora	#>TAB_YDEPTH_L
	sta	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	WALL_P0_Y_L
	lda	WALL_P0_Y_H
	stz	WALL_P0_Y_H
	bne	:+
	lda	TAB_YDEPTH_H,y
	sta	WALL_P0_Y_H
:
	;; Y1 scale
	; NOTE: this overflows at values >= 4096

	; p1.y = tab_depth[p1.y]
	ldy	WALL_P1_Y_L
	lda	WALL_P1_Y_H
	ora	#>TAB_YDEPTH_L
	sta	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	WALL_P1_Y_L
	lda	WALL_P1_Y_H
	stz	WALL_P1_Y_H
	bne	:+
	lda	TAB_YDEPTH_H,y
	sta	WALL_P1_Y_H
:
	;; get distance X
	; ld.x = (d0.x * wall->dist.x + d0.y * wall->dist.y) >> 1
	; ld.x = (d1.x * wall->dist.x + d1.y * wall->dist.y) >> 1

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; check X peg
	lda	WALL_XPEG
	bpl	:+

	; d1.x
	lda	WALL_D1_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D1_X_H
	sta	VERA_FX_CACHE_M
	bra	:++
:
	; d0.x
	lda	WALL_D0_X_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D0_X_H
	sta	VERA_FX_CACHE_M
:
	; wall->dist.x
	ldy	#WALL_STRUCT_DIST_X
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_H
	iny
	lda	(WALL_PTR_L),y
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; check X peg
	lda	WALL_XPEG
	bpl	:+

	; d1.y
	lda	WALL_D1_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D1_Y_H
	sta	VERA_FX_CACHE_M
	bra	:++
:
	; d0.y
	lda	WALL_D0_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_D0_Y_H
	sta	VERA_FX_CACHE_M
:
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

	; get result (>> 1)
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	WALL_LD_X_L
	lda	VERA_DATA0
	sta	WALL_LD_X_H
	lda	VERA_DATA0
	lsr
	ror	WALL_LD_X_H
	ror	WALL_LD_X_L

	; lca = wall->angle - projection.a
	sec
	ldy	#WALL_STRUCT_ANGLE
	lda	(WALL_PTR_L),y
	sbc	PROJECTION_A_L
	sta	WALL_LCA_L
	iny
	lda	(WALL_PTR_L),y
	sbc	PROJECTION_A_H
	sta	WALL_LCA_H

	;; scale_now = p0.y << 8
	stz	WALL_SCALE_NOW_L
	; WALL_SCALE_NOW_M and WALL_SCALE_NOW_H is forced overlap

	;; scale_step = (ydiff * inv_div[xdiff]) >> 7

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; inverse division bank
	lda	#BANK_IDIV_L
	sta	REG_RAM_BANK

	; ydiff = p1.y - p0.y
	sec
	lda	WALL_P1_Y_L
	sbc	WALL_P0_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_P1_Y_H
	sbc	WALL_P0_Y_H
	sta	VERA_FX_CACHE_M

	; xdiff = x1 - x0
	sec
	lda	WALL_X1
	sbc	WALL_X0
	tay

	; if(x1 > projection.x1)
	lda	PROJECTION_X1
	cmp	WALL_X1
	bcs	:+
	sta	WALL_X1
:
	; inv_div[xdiff]
	lda	TAB_IDIV_L,y
	sta	VERA_FX_CACHE_H
	lda	TAB_IDIV_H,y
	sta	VERA_FX_CACHE_U

	; accumulate (double the value)
	lda	VERA_FX_ACCUM

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	WALL_SCALE_STEP_L
	lda	VERA_DATA0
	sta	WALL_SCALE_STEP_M
	lda	VERA_DATA0
	sta	WALL_SCALE_STEP_H

	; kinda limit 'scale_step' to signed 17 bits
	; workaround for leaking floor and ceiling when looking parallel to a wall
	; this does not fix texture scaling
	bmi	:+
	lda	WALL_SCALE_STEP_M
	bpl	:+
	stz	WALL_SCALE_STEP_H
	lda	#$7F
	sta	WALL_SCALE_STEP_M
	lda	#$FF
	sta	WALL_SCALE_STEP_L
:
	; if(x0 < projection.x0)
	lda	WALL_X0
	cmp	PROJECTION_X0
	bcs	@skip_x0_clip

	;; scale_now += scale_step * diff

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; diff = projection.x0 - x0
	sec
	lda	PROJECTION_X0
	sbc	WALL_X0
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; scale_step
	lda	WALL_SCALE_STEP_L
	sta	VERA_FX_CACHE_H
	lda	WALL_SCALE_STEP_M
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; scale_now += result
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	WALL_SCALE_NOW_L
	clc
	lda	WALL_SCALE_NOW_M
	adc	VERA_DATA0
	sta	WALL_SCALE_NOW_M
	lda	WALL_SCALE_NOW_H
	adc	VERA_DATA0
	sta	WALL_SCALE_NOW_H

	; x0 = projection.x0
	lda	PROJECTION_X0
	sta	WALL_X0
@skip_x0_clip:

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;; texture mapping loop
	; this loop creates Y scaling
	; Y scaling is also used for depth check on sprites
	; using VERA for accumulation is very slightly faster (like 0.2%)

	; time
	jsr	time_update

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; scale_now >> 8	; NOTE: 'WALL_SCALE_NOW_L' is missing - possible precision loss!
	lda	WALL_SCALE_NOW_M
	sta	VERA_FX_CACHE_L
	lda	WALL_SCALE_NOW_H
	sta	VERA_FX_CACHE_M

	; 256
	stz	VERA_FX_CACHE_H
	lda	#1
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; + $A000 << 8 (address of TAB_TSCALE_L)
	lda	#$05
	sta	VERA_FX_CACHE_M
	stz	VERA_FX_CACHE_L
	lda	#$20
	sta	VERA_FX_CACHE_U
	stz	VERA_FX_CACHE_H

	; accumulate
	lda	VERA_FX_ACCUM

	; scale_step * -1
	lda	WALL_SCALE_STEP_L
	sta	VERA_FX_CACHE_L
	lda	WALL_SCALE_STEP_M
	sta	VERA_FX_CACHE_M
	lda	#$FF
	sta	VERA_FX_CACHE_H
	lda	#$FF
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; scale_step * 1
	lda	#1
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U

	; texture bank
	lda	#BANK_TSCALE
	sta	REG_RAM_BANK

	; loop start
	ldx	WALL_X0
@loop_texture_map_y:
	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	WALL_TOP_NOW_M
	lda	VERA_DATA0
	sta	WALL_TOP_NOW_H

	; tstep = tex_scale[scale_now >> 8]
	lda	(WALL_TOP_NOW_M)
	sta	RENDER_TEXSCALE_L,x
	lda	#>TAB_TSCALE_H
	tsb	WALL_TOP_NOW_H
	lda	(WALL_TOP_NOW_M)
	sta	RENDER_TEXSCALE_H,x

	; scale_now += scale_step (accumulate)
	lda	VERA_FX_ACCUM

	; next
	inx
	cpx	WALL_X1
	bne	@loop_texture_map_y

	; time
	jsr	time_update

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;; wall render loops

	lda	#$FF
	sta	WALL_MASKED_IDX
	stz	WALL_HAVE_TEX_X
	stz	WALL_TEXTURE_FIX

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; if(proj_msk_idx < MAX_DRAW_MASKED)
	lda	PROJECTION_MASKED_IDX
	cmp	#16
	bcs	@skip_masked

	; if((wall->solid.angle & MARK_TYPE_BITS) == MARK_MASKED)
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_MID_BITS
	cmp	#WALL__MARK_MASKED
	bne	@skip_masked

	; if(wall->masked.texture_mid)
	; if(!(wall->masked.texture_mid & 0x80))
	ldy	#WALL_STRUCT_TEX_MID
	lda	(WALL_PTR_L),y
	beq	@skip_masked
	bmi	@skip_masked

	; if(!(texture_remap[wall->masked.texture_mid] & 0x80))
	sta	WALL_MT_TEXTURE
	tax
	lda	TAB_TEXTURE_REMAP,x
	bmi	@skip_masked

	; prepare masked texture
	jsr	prepare_masked

@skip_masked:

	; if(!(wall->angle & MARK_PORTAL))
	ldy	#WALL_STRUCT_ANGLE+1
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_PORTAL
	beq	:+

	; if(!wall->backsector)
	ldy	#WALL_STRUCT_BACKSECTOR
	lda	(WALL_PTR_L),y
	beq	:+
	jmp	@two_sided
:
	; if((wall->solid.angle & MARK_TYPE_BITS) != MARK_SPLIT)
	lda	(WALL_PTR_L),y
	and	#WALL__MARK_MID_BITS
	cmp	#WALL__MARK_SPLIT
	bne	:+

	; get split height
	ldy	#WALL_STRUCT_HEIGHT_SPLIT
	lda	(WALL_PTR_L),y
	sta	WALL_TMP_CALC_L
	iny
	lda	(WALL_PTR_L),y
	sta	WALL_TMP_CALC_H

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; fake height
	lda	WALL_TMP_CALC_L
	sta	SECTOR_STRUCT_FLOOR_HEIGHT_L
	sta	SECTOR_STRUCT_CEILING_HEIGHT_L
	lda	WALL_TMP_CALC_H
	sta	SECTOR_STRUCT_FLOOR_HEIGHT_H
	sta	SECTOR_STRUCT_CEILING_HEIGHT_H

	; fake back sector (sector zero)
	stz	BACKSEC_PTR_I

	jmp	@two_split

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;; solid wall
:
	; texture Y peg
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	and	#WALL__TEXFLAG_PEG_Y
	beq	:+

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; projection.oy = (int16_t)(sec->height_floor - sec->height_ceiling) >> 1
	ldy	SECTOR_PTR_I
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	tax
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	ror
	txa
	ror
	sta	WALL_TEXTURE_FIX
:
@single_hack_top:
	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; texture offsets
	ldy	#WALL_STRUCT_TEX_TOP_X
	lda	(WALL_PTR_L),y
	sta	RENDER_TEXTURE_OX
	iny
	sec
	lda	(WALL_PTR_L),y
	sbc	WALL_TEXTURE_FIX
	sta	RENDER_TEXTURE_OY

	; texture flags
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	sta	WALL_TEXTURE_FLAGS

	; texture
	ldy	#WALL_STRUCT_TEX_TOP
	lda	(WALL_PTR_L),y
	jsr	texture_set

@single_hack_bot:
	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; masked clip
	lda	WALL_MASKED_IDX
	bmi	:+
	lda	WALL_X0
	sta	WALL_X0_TMP
	jsr	masked_clip_top
	jsr	masked_clip_bot
:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; height_top
	ldy	SECTOR_PTR_I
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	sta	WALL_TOP_NOW_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	sta	WALL_TOP_NOW_H

	; height_bot
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sta	WALL_BOT_NOW_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	sta	WALL_BOT_NOW_H

	; calculate
	jsr	wall_top
	jsr	wall_bot

	; wall flags: 7
	lda	#$A9	; LDA opcode
	sta	wall_render_flag_1
	sta	wall_render_flag_2
	sta	wall_render_flag_4

	; loop start
	jsr	wall_render_start

	; invalidate last portal
	lda	PROJECTION_PORTAL_RD
	sta	PROJECTION_PORTAL_LAST

@dummy_portal:
	; portal limit check
	bit	PROJECTION_PORTAL_WR
	bvs	:+

	; check masked texture
	lda	WALL_MASKED_IDX
	bmi	:+

	; portal bank
	ldx	#BANK_PORTALS
	stx	REG_RAM_BANK

	; add dummy portal
	ldy	PROJECTION_PORTAL_WR
	sta	MEM_PORTALS_MASKED,y
	lda	#0
	sta	MEM_PORTALS_SECTOR,y
	lda	#$FF
	sta	MEM_PORTALS_SPRITE,y

	; portal_wr++
	inc	PROJECTION_PORTAL_WR
:
	; done
	rts

@two_sided:
	; get back sector
	sta	BACKSEC_PTR_I

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

@two_split:
	; if(bs->height_floor - sec->height_ceiling >= 0)
	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	bmi	@front_bot_ok

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; texture flags
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	tax
	lda	TAB_SWAP,x
	sta	WALL_TEXTURE_FLAGS

	; texture
	ldy	#WALL_STRUCT_TEX_BOT
	lda	(WALL_PTR_L),y
	jsr	texture_set

	; texture Y peg
	lda	WALL_TEXTURE_FLAGS
	and	#WALL__TEXFLAG_PEG_Y
	beq	:+

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; projection.oy = (int16_t)(sec->height_floor - bs->height_floor) >> 1
	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	WALL_TMP_CALC_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	ror
	lda	WALL_TMP_CALC_L
	ror
	sta	WALL_TEXTURE_FIX

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK
:
	; texture offsets
	ldy	#WALL_STRUCT_TEX_BOT_X
	lda	(WALL_PTR_L),y
	sta	RENDER_TEXTURE_OX
	iny
	sec
	lda	(WALL_PTR_L),y
	sbc	WALL_TEXTURE_FIX
	sta	RENDER_TEXTURE_OY

	jmp	@single_hack_bot

@front_bot_ok:
	; diff = sec->height_floor - bs->height_ceiling
	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sta	MATH_VAR_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,x

	; if(diff >= 0)
	bmi	@front_top_ok

	; projection.oy = diff >> 1
	ror
	lda	MATH_VAR_L
	ror
	sta	WALL_TEXTURE_FIX

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; texture Y peg
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	and	#WALL__TEXFLAG_PEG_Y
	beq	:+

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; projection.oy += (int16_t)(bs->height_ceiling - sec->height_ceiling) >> 1
	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	sta	WALL_TMP_CALC_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	ror
	lda	WALL_TMP_CALC_L
	ror
	clc
	adc	WALL_TEXTURE_FIX
	sta	WALL_TEXTURE_FIX
:
	jmp	@single_hack_top

@front_top_ok:
	; reset portal status
	lda	#$FF
	sta	WALL_PORTAL_TOP
	sta	WALL_PORTAL_BOT

	; backup X0
	lda	WALL_X0
	sta	WALL_X0_TMP

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;; top wall

	; height_top
	ldy	SECTOR_PTR_I
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	sta	WALL_TOP_NOW_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	sta	WALL_TOP_NOW_H
	jsr	wall_top

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; if(bs->height_ceiling < sec->height_ceiling)
	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sta	WALL_TMP_CALC_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	bmi	@do_mark_top
	ora	WALL_TMP_CALC_L
	beq	@do_mark_top

	; masked clip
	lda	WALL_MASKED_IDX
	bmi	:+
	jsr	masked_clip_top
	ldx	BACKSEC_PTR_I
:
	; height_bot
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sta	WALL_BOT_NOW_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sta	WALL_BOT_NOW_H
	jsr	wall_bot

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; texture flags
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	sta	WALL_TEXTURE_FLAGS

	; texture Y peg
	and	#WALL__TEXFLAG_PEG_Y
	beq	:+

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; projection.oy = (int16_t)(bs->height_ceiling - sec->height_ceiling) >> 1
	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	sta	WALL_TMP_CALC_L
	lda	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	ror
	lda	WALL_TMP_CALC_L
	ror
	sta	WALL_TEXTURE_FIX

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK
:
	; texture offsets
	ldy	#WALL_STRUCT_TEX_TOP_X
	lda	(WALL_PTR_L),y
	sta	RENDER_TEXTURE_OX
	iny
	sec
	lda	(WALL_PTR_L),y
	sbc	WALL_TEXTURE_FIX
	sta	RENDER_TEXTURE_OY

	; texture
	ldy	#WALL_STRUCT_TEX_TOP
	lda	(WALL_PTR_L),y
	jsr	texture_set

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; wall flags: 1
	lda	#$A9	; LDA opcode
	sta	wall_render_flag_1
	lda	#$80	; BRA opcode
	sta	wall_render_flag_2
	sta	wall_render_flag_4

	; loop start
	jsr	wall_render_start

	bra	@check_wall_bot

@do_mark_top:
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;; only mark clipping and plane

	; get top clip
	ldx	WALL_X0
	lda	RENDER_CLIP_TOP,x
	sta	WALL_CLIP_TOP

	; get bot clip
	lda	RENDER_CLIP_BOT,x
	sta	WALL_CLIP_BOT

	; if((top_now >> 8) >= botclip)
	lda	WALL_TOP_NOW_H
	bmi	:+
	bne	@mark_skip_top
	lda	WALL_TOP_NOW_M
	cmp	WALL_CLIP_BOT
	bcs	@mark_skip_top
:
	; y0 = top_now >> 8
	lda	WALL_TOP_NOW_M
	sta	WALL_Y0
	lda	WALL_TOP_NOW_H
	beq	:+
	stz	WALL_Y0
	bmi	:+
	lda	#$FF
	sta	WALL_Y0
:
	; portal_top = 0
	stz	WALL_PORTAL_TOP

	; if(y0 < clip_top[x0])
	lda	WALL_CLIP_TOP
	cmp	WALL_Y0
	bcc	:+

	; y0 = clip_top[x0]
	sta	WALL_Y0
:
	bra	@do_top_next

@mark_skip_top:
	; y0 = clip_bot[x0]
	lda	WALL_CLIP_BOT
	sta	WALL_Y0

@do_top_next:
	; plc_top[x0] = clip_top[x0] + 1
	lda	WALL_CLIP_TOP
	ina
	sta	RENDER_PLC_TOP,x

	; plc_bot[x0] = y0
	lda	WALL_Y0
	sta	RENDER_PLC_BOT,x

	; clip_top[x0] = y0
	lda	WALL_Y0
	sta	RENDER_CLIP_TOP,x

	; top_now += top_step
	clc
	lda	WALL_TOP_NOW_L
	adc	WALL_TOP_STEP_L
	sta	WALL_TOP_NOW_L
	lda	WALL_TOP_NOW_M
	adc	WALL_TOP_STEP_M
	sta	WALL_TOP_NOW_M
	lda	WALL_TOP_NOW_H
	adc	WALL_TOP_STEP_H
	sta	WALL_TOP_NOW_H

	; next
	inc	WALL_X0
	lda	WALL_X0
	cmp	WALL_X1
	bcc	@do_mark_top

	; time
	jsr	time_update

	; masked clip
	lda	WALL_MASKED_IDX
	bmi	:+
	jsr	masked_clip_top
:
@check_wall_bot:
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;; bot wall

	; restore X0
	lda	WALL_X0_TMP
	sta	WALL_X0

	; reset texture clip Y fix
	stz	WALL_TEXTURE_FIX

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; texture flags
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	tax
	lda	TAB_SWAP,x
	sta	WALL_TEXTURE_FLAGS

	; texture peg
	and	#WALL__TEXFLAG_PEG_Y
	beq	:+

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; projection.oy = (int16_t)(sec->height_floor - bs->height_floor) >> 1
	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	WALL_TMP_CALC_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	ror
	lda	WALL_TMP_CALC_L
	ror
	sta	WALL_TEXTURE_FIX

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK
:
	; texture offsets
	ldy	#WALL_STRUCT_TEX_BOT_X
	lda	(WALL_PTR_L),y
	sta	RENDER_TEXTURE_OX
	iny
	sec
	lda	(WALL_PTR_L),y
	sbc	WALL_TEXTURE_FIX
	sta	RENDER_TEXTURE_OY

	; texture
	ldy	#WALL_STRUCT_TEX_BOT
	lda	(WALL_PTR_L),y
	jsr	texture_set

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; reset accumulator, set addition, enable multiplication
	lda	#%10010000
	sta	VERA_FX_MULT

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; height_bot
	ldy	SECTOR_PTR_I
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sta	WALL_BOT_NOW_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	sta	WALL_BOT_NOW_H
	jsr	wall_bot

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; if(bs->height_floor > sec->height_floor)
	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sta	WALL_TMP_CALC_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	bmi	@do_mark_bot
	ora	WALL_TMP_CALC_L
	beq	@do_mark_bot

	; masked clip
	lda	WALL_MASKED_IDX
	bmi	:+
	jsr	masked_clip_bot
	ldx	BACKSEC_PTR_I
:
	; height_top
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sta	WALL_TOP_NOW_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sta	WALL_TOP_NOW_H
	jsr	wall_top

	; wall flags: 2
	lda	#$A9	; LDA opcode
	sta	wall_render_flag_2
	lda	#$80	; BRA opcode
	sta	wall_render_flag_1
	sta	wall_render_flag_4

	; loop start
	jsr	wall_render_start

	bra	@check_wall_portal

@do_mark_bot:
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;; only mark clipping and plane

	; get top clip
	ldx	WALL_X0
	lda	RENDER_CLIP_TOP,x
	sta	WALL_CLIP_TOP

	; get bot clip
	lda	RENDER_CLIP_BOT,x
	sta	WALL_CLIP_BOT

	; if((bot_now >> 8) <= topclip)
	lda	WALL_BOT_NOW_H
	bmi	@mark_skip_bot
	bne	:+
	lda	WALL_CLIP_TOP
	cmp	WALL_BOT_NOW_M
	bcs	@mark_skip_bot
:
	; y1 = bot_now >> 8
	lda	WALL_BOT_NOW_M
	sta	WALL_Y1
	lda	WALL_BOT_NOW_H
	bpl	:+
	stz	WALL_Y1
	bra	:++
:
	beq	:+
	lda	#120
	sta	WALL_Y1
:
	; portal_bot = 0
	stz	WALL_PORTAL_BOT

	; if(y1 > clip_bot[x0])
	lda	WALL_Y1
	cmp	WALL_CLIP_BOT
	beq	:+
	bcc	:+

	; y1 = clip_bot[x0]
	lda	WALL_CLIP_BOT
	sta	WALL_Y1
:
	bra	@do_bot_next

@mark_skip_bot:
	; y1 = clip_top[x0]
	lda	WALL_CLIP_TOP
	sta	WALL_Y1

@do_bot_next:
	; plf_top[x0] = y1 + 1
	lda	WALL_Y1
	ina
	sta	RENDER_PLF_TOP,x

	; plf_bot[x0] = clip_bot[x0]
	lda	WALL_CLIP_BOT
	sta	RENDER_PLF_BOT,x

	; clip_bot[x0] = y1
	lda	WALL_Y1
	sta	RENDER_CLIP_BOT,x

	; bot_now += bot_step
	clc
	lda	WALL_BOT_NOW_L
	adc	WALL_BOT_STEP_L
	sta	WALL_BOT_NOW_L
	lda	WALL_BOT_NOW_M
	adc	WALL_BOT_STEP_M
	sta	WALL_BOT_NOW_M
	lda	WALL_BOT_NOW_H
	adc	WALL_BOT_STEP_H
	sta	WALL_BOT_NOW_H

	; next
	inc	WALL_X0
	lda	WALL_X0
	cmp	WALL_X1
	bcc	@do_mark_bot

	; time
	jsr	time_update

	; masked clip
	lda	WALL_MASKED_IDX
	bmi	:+
	jsr	masked_clip_bot
:
@check_wall_portal:
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;; portal

	; render check
	lda	WALL_PORTAL_TOP
	ora	WALL_PORTAL_BOT
	beq	:+
@dummy_portal_jmp:
	jmp	@dummy_portal
:
	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; if(bs->height_ceiling > bs->height_floor)
	ldx	BACKSEC_PTR_I
	sec
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,x
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,x
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,x
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,x
	bpl	@dummy_portal_jmp

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; portal slot
	ldy	PROJECTION_PORTAL_WR

	; check for portal merge
	ldx	PROJECTION_PORTAL_LAST
	lda	BACKSEC_PTR_I
	cmp	MEM_PORTALS_SECTOR,x
	bne	@new_portal

	; update X1
	lda	WALL_X1
	sta	MEM_PORTALS_X1,x

	; limit check
	bit	PROJECTION_PORTAL_WR
	bvs	:+

	; check masked texture
	lda	WALL_MASKED_IDX
	bmi	:+

	; portal bank
	lda	#BANK_PORTALS
	sta	REG_RAM_BANK

	; *portal_wr = *portal_last
	ldy	PROJECTION_PORTAL_WR
	lda	MEM_PORTALS_SECTOR,x
	sta	MEM_PORTALS_SECTOR,y
	lda	MEM_PORTALS_X0,x
	sta	MEM_PORTALS_X0,y
	lda	WALL_X1
	sta	MEM_PORTALS_X1,y
	lda	MEM_PORTALS_SPRITE,x
	sta	MEM_PORTALS_SPRITE,y
	lda	MEM_PORTALS_MASKED,x
	sta	MEM_PORTALS_MASKED,y

	; add dummy portal
	lda	WALL_MASKED_IDX
	sta	MEM_PORTALS_MASKED,x
	lda	#0
	sta	MEM_PORTALS_SECTOR,x
	lda	#$FF
	sta	MEM_PORTALS_SPRITE,x

	; portal_last = portal_wr
	sty	PROJECTION_PORTAL_LAST

	; portal_wr++
	inc	PROJECTION_PORTAL_WR
:
	; done
	rts

@new_portal:
	; limit check
	bit	PROJECTION_PORTAL_WR
	bvs	:+

	; save portal sector
	lda	BACKSEC_PTR_I
	sta	MEM_PORTALS_SECTOR,y

	; save portal X0
	lda	WALL_X0_TMP
	sta	MEM_PORTALS_X0,y

	; save portal X1
	lda	WALL_X1
	sta	MEM_PORTALS_X1,y

	; save portal masked
	lda	WALL_MASKED_IDX
	sta	MEM_PORTALS_MASKED,y

	; update last portal
	sty	PROJECTION_PORTAL_LAST

	; portal_wr++
	inc	PROJECTION_PORTAL_WR
:
	; done
	rts

;;;
; wall top
;;;

wall_top:
	;; top_step = (scale_step * zdiff) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; zdiff = projection.z - height_top
	sec
	lda	PROJECTION_Z_L
	sbc	WALL_TOP_NOW_L
	sta	VERA_FX_CACHE_L
	lda	PROJECTION_Z_H
	sbc	WALL_TOP_NOW_H
	sta	VERA_FX_CACHE_M

	; scale_step
	lda	WALL_SCALE_STEP_L
	sta	VERA_FX_CACHE_H
	lda	WALL_SCALE_STEP_M
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	WALL_TOP_STEP_L
	lda	VERA_DATA0
	sta	WALL_TOP_STEP_M
	lda	VERA_DATA0
	sta	WALL_TOP_STEP_H

	;; top_now = (scale_now >> 8) * zdiff

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; zdiff is already set

	; scale_now >> 8
	lda	WALL_SCALE_NOW_M
	sta	VERA_FX_CACHE_H
	lda	WALL_SCALE_NOW_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result, add (projection.ycw << 8)
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	WALL_TOP_NOW_L
	clc
	lda	VERA_DATA0
	adc	PROJECTION_YCW
	sta	WALL_TOP_NOW_M
	lda	VERA_DATA0
	adc	#0
	sta	WALL_TOP_NOW_H

	; done
	rts

;;;
; wall bot
;;;

wall_bot:
	;; bot_step = (scale_step * zdiff) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; zdiff = projection.z - height_bot
	sec
	lda	PROJECTION_Z_L
	sbc	WALL_BOT_NOW_L
	sta	VERA_FX_CACHE_L
	lda	PROJECTION_Z_H
	sbc	WALL_BOT_NOW_H
	sta	VERA_FX_CACHE_M

	; scale_step
	lda	WALL_SCALE_STEP_L
	sta	VERA_FX_CACHE_H
	lda	WALL_SCALE_STEP_M
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	WALL_BOT_STEP_L
	lda	VERA_DATA0
	sta	WALL_BOT_STEP_M
	lda	VERA_DATA0
	sta	WALL_BOT_STEP_H

	;; bot_now = s0 * zdiff

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; zdiff is already set

	; scale_now >> 8
	lda	WALL_SCALE_NOW_M
	sta	VERA_FX_CACHE_H
	lda	WALL_SCALE_NOW_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result, add (projection.ycw << 8)
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	WALL_BOT_NOW_L
	clc
	lda	VERA_DATA0
	adc	PROJECTION_YCW
	sta	WALL_BOT_NOW_M
	lda	VERA_DATA0
	adc	#0
	sta	WALL_BOT_NOW_H

	; done
	rts

;;;
; wall renderer
;;;

wall_render_start:
	; check generated X map
	lda	WALL_HAVE_TEX_X
	bne	wall_render_loop

	; generate X map
	jsr	generate_texture_map_wall

	bra	wall_render_loop

wall_render_do_next:
	; restore X
	ldx	WALL_X0

wall_render_flag_1:
	bra	wall_render_flag_2	; modified opcode

	; plc_top[x0] = clip_top[x0] + 1
	lda	WALL_CLIP_TOP
	ina
	sta	RENDER_PLC_TOP,x

	; plc_bot[x0] = y0
	lda	WALL_Y0
	sta	RENDER_PLC_BOT,x

	; clip_top[x0] = y1
	lda	WALL_Y1
	sta	RENDER_CLIP_TOP,x

wall_render_flag_2:
	bra	wall_render_flag_4	; modified opcode

	; plf_top[x0] = y1 + 1
	lda	WALL_Y1
	ina
	sta	RENDER_PLF_TOP,x

	; plf_bot[x0] = clip_bot[x0]
	lda	WALL_CLIP_BOT
	sta	RENDER_PLF_BOT,x

	; clip_bot[x0] = y0
	lda	WALL_Y0
	sta	RENDER_CLIP_BOT,x

wall_render_flag_4:
	bra	wall_render_done	; modified opcode

	; clip_top[x0] = 0xFF
	lda	#$F0
	sta	RENDER_CLIP_TOP,x

wall_render_done:
	; top_now += top_step
	clc
	lda	WALL_TOP_NOW_L
	adc	WALL_TOP_STEP_L
	sta	WALL_TOP_NOW_L
	lda	WALL_TOP_NOW_M
	adc	WALL_TOP_STEP_M
	sta	WALL_TOP_NOW_M
	lda	WALL_TOP_NOW_H
	adc	WALL_TOP_STEP_H
	sta	WALL_TOP_NOW_H

	; bot_now += bot_step
	clc
	lda	WALL_BOT_NOW_L
	adc	WALL_BOT_STEP_L
	sta	WALL_BOT_NOW_L
	lda	WALL_BOT_NOW_M
	adc	WALL_BOT_STEP_M
	sta	WALL_BOT_NOW_M
	lda	WALL_BOT_NOW_H
	adc	WALL_BOT_STEP_H
	sta	WALL_BOT_NOW_H

	; next
	inc	WALL_X0
	lda	WALL_X0
	cmp	WALL_X1
	bcc	wall_render_loop

	; done
	rts

wall_render_skip_top:
	; y0 = clip_bot[x0]
	lda	WALL_CLIP_BOT
	sta	WALL_Y0

	; y1 = 120
	lda	#120
	sta	WALL_Y1

	; portal_bot = 0
	stz	WALL_PORTAL_BOT

	; continue
	bra	wall_render_do_next

wall_render_skip_bot:
	; y1 = clip_top[x0]
	lda	WALL_CLIP_TOP
	sta	WALL_Y1

	; y0 = 0
	stz	WALL_Y0

	; portal_top = 0
	stz	WALL_PORTAL_TOP

	; continue
	bra	wall_render_do_next

wall_render_loop:
	; get top clip
	ldx	WALL_X0
	lda	RENDER_CLIP_TOP,x
	sta	WALL_CLIP_TOP

	; get bot clip
	lda	RENDER_CLIP_BOT,x
	sta	WALL_CLIP_BOT

	; if((top_now >> 8) >= botclip)
	lda	WALL_TOP_NOW_H
	bmi	:+
	bne	wall_render_skip_top
	lda	WALL_TOP_NOW_M
	cmp	WALL_CLIP_BOT
	bcs	wall_render_skip_top
:
	; if((bot_now >> 8) <= topclip)
	lda	WALL_BOT_NOW_H
	bmi	wall_render_skip_bot
	bne	:+
	lda	WALL_CLIP_TOP
	cmp	WALL_BOT_NOW_M
	bcs	wall_render_skip_bot
:
	; y0 = top_now >> 8
	lda	WALL_TOP_NOW_M
	sta	WALL_Y0
	lda	WALL_TOP_NOW_H
	beq	:+
	stz	WALL_Y0
	bmi	:+
	lda	#$FF
	sta	WALL_Y0
:
	; y1 = bot_now >> 8
	lda	WALL_BOT_NOW_M
	sta	WALL_Y1
	lda	WALL_BOT_NOW_H
	beq	:+
	stz	WALL_Y1
:
	; prepare draw Y
	lda	#$FF
	sta	WALL_Y_START_L
	sta	WALL_Y_START_M
	sta	WALL_Y_START_H

	; if(y0 < clip_top[x0])
	lda	WALL_CLIP_TOP
	cmp	WALL_Y0
	bcc	:+
	; y0 = clip_top[x0]
	sta	WALL_Y0
	bra	:++
:
	stz	WALL_PORTAL_BOT
:
	; clipdiff = clip_bot[x0] - (bot_now >> 8)
	sec
	lda	WALL_BOT_NOW_M
	sbc	WALL_CLIP_BOT
	sta	WALL_CLIP_DIFF_L
	lda	WALL_BOT_NOW_H
	sbc	#0
	sta	WALL_CLIP_DIFF_H
	bmi	@y1_is_ok
	ora	WALL_CLIP_DIFF_L
	beq	@y1_is_ok

	; y1 = clip_bot[x0]
	lda	WALL_CLIP_BOT
	sta	WALL_Y1

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; tnow = clipdiff * tmap_scale[x0]

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; Y step
	lda	RENDER_TEXSCALE_L,x
	sta	VERA_FX_CACHE_L
	lda	RENDER_TEXSCALE_H,x
	sta	VERA_FX_CACHE_M

	; clipdiff
	lda	WALL_CLIP_DIFF_L
	sta	VERA_FX_CACHE_H
	lda	WALL_CLIP_DIFF_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_DATA0

	; get result (>> 2)
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	WALL_Y_START_L
	lda	VERA_DATA0
	sta	WALL_Y_START_M
	lda	VERA_DATA0
	sec
	ror
	sta	WALL_Y_START_H
	ror	WALL_Y_START_M
	ror	WALL_Y_START_L
	sec
	ror	WALL_Y_START_H
	ror	WALL_Y_START_M
	ror	WALL_Y_START_L

	bra	:+

@y1_is_ok:
	stz	WALL_PORTAL_TOP
:
	; offset Y start
	clc
	lda	WALL_Y_START_M
	adc	RENDER_TEXTURE_OY
	sta	WALL_Y_START_M
	bcc	:+
	inc	WALL_Y_START_H
:
	; double X resolution
	lda	WALL_X0
	asl
	sta	VIDEO_DRAW_X

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache, enable affine helper
	lda	#%00000011
	sta	VERA_FX_CTRL

	; dcsel = 3, addrsel = 0
	lda	#$06
	sta	VERA_CTRL

	; set affine Y position
wall_affine_opcode_C:
	jsr	set_affine_yn	; modified address

	; set affine X position
wall_affine_opcode_A:
	jsr	set_affine_xpn	; modified address

	; draw column
	sec
	lda	WALL_Y1
	sbc	WALL_Y0
	bmi	wall_skip_draw
	sta	VIDEO_DRAW_L
wall_vline_opcode_A:
	jsr	video_draw_vline	; modified address

	; skip odd column
detcode_wall_vline:
	bra	wall_skip_draw	; modified opcode

	; next column
	inc	VIDEO_DRAW_X

	; set affine Y position
wall_affine_opcode_D:
	jsr	set_affine_yhn	; modified address

	; set affine X position
wall_affine_opcode_B:
	jsr	set_affine_xpn	; modified address

	; draw column
	ldx	VIDEO_DRAW_L
wall_vline_opcode_B:
	jsr	video_draw_vline	; modified address

wall_skip_draw:
	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; next
	jmp	wall_render_do_next

;;;
; sprite: prepare
;;;

prepare_sprite:
	sta	SPRITE_THING
	tay

	; if(proj_spr_idx >= 32)
	lda	PROJECTION_SPRITE_IDX
	cmp	#32
	bcc	:+
@bail:
	rts
:
	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; thing sprite
	lda	THING_STRUCT_SPRITE,y
	sta	THTEMP_PTR_L
	ina
	beq	@bail

	; thing angle
	lda	THING_STRUCT_ANGLE,y
	sta	SPRITE_ANG_L

	; thing bright flag
	lda	THING_STRUCT_NXTFRM_H,y
	sta	SPRITE_BRIGHT

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; reset accumulator, set addition, enable multiplication
	lda	#%10010000
	sta	VERA_FX_MULT

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; p2a_coord.x = (th->x >> 8) - (projection.x >> 8)
	sec
	lda	THING_STRUCT_X_L,y
	sbc	PROJECTION_X_L
	sta	MATH_P2A_X_L
	lda	THING_STRUCT_X_H,y
	sbc	PROJECTION_X_H
	sta	MATH_P2A_X_H

	; p2a_coord.y = (th->y >> 8) - (projection.y >> 8)
	sec
	lda	THING_STRUCT_Y_L,y
	sbc	PROJECTION_Y_L
	sta	MATH_P2A_Y_L
	lda	THING_STRUCT_Y_H,y
	sbc	PROJECTION_Y_H
	sta	MATH_P2A_Y_H

	; dist = (p2a_coord.x * projection.sin + p2a_coord.y * projection.cos) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; p2a_coord.x
	lda	MATH_P2A_X_L
	sta	VERA_FX_CACHE_L
	lda	MATH_P2A_X_H
	sta	VERA_FX_CACHE_M

	; projection.sin
	lda	PROJECTION_SIN_L
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_SIN_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; p2a_coord.y
	lda	MATH_P2A_Y_L
	sta	VERA_FX_CACHE_L
	lda	MATH_P2A_Y_H
	sta	VERA_FX_CACHE_M

	; projection.cos
	lda	PROJECTION_COS_L
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_COS_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	SPRITE_DIST_L
	lda	VERA_DATA0
	sta	SPRITE_DIST_H

	; if(dist <= 16)
	bpl	:+
@behind:
	; behind camera
	rts
:
	bne	:+
	lda	SPRITE_DIST_L
	cmp	#17
	bcc	@behind
:
	; a0 = point_to_angle()
	jsr	math_p2a

	; tsprite info bank
	lda	#BANK_TSPR_INFO
	sta	REG_RAM_BANK

	; default to no rotation
	lda	#$A8
	sta	THTEMP_PTR_H

	; if(si->rotate)
	ldx	THTEMP_PTR_L
	lda	SPRINFO_STRUCT_ROTATE,x
	beq	:+

	; a0 >> 4
	ldx	MATH_VAR_L
	lda	TAB_SWAP,x
	and	#$0F
	sta	SPRITE_ANG_H
	ldx	MATH_VAR_H
	lda	TAB_SWAP,x
	and	#$F0
	ora	SPRITE_ANG_H
	sta	SPRITE_ANG_H

	; ang = th->angle - (a0 >> 4)
	sec
	lda	SPRITE_ANG_L
	sbc	SPRITE_ANG_H

	; ang += 16
	clc
	adc	#16

	; ang >>= 5
	tax
	lda	TAB_BANK,x

	ora	#$A8
	sta	THTEMP_PTR_H
:
	; frame index
	lda	(THTEMP_PTR_L)
	tax
	lda	#$08
	trb	THTEMP_PTR_H
	lda	(THTEMP_PTR_L)
	sta	THTEMP_PTR_L

	; sprite frame bank
	lda	#BANK_SPRITE_FRM
	sta	REG_RAM_BANK

	; get sprite frame
	lda	TAB_SWAP,x
	ora	#$A0
	sta	THTEMP_PTR_H

	; frm->width
	lda	(THTEMP_PTR_L)
	sta	SPRITE_WIDTH

	; frm->height
	inc	THTEMP_PTR_H
	lda	(THTEMP_PTR_L)
	sta	SPRITE_HEIGHT

	; frm->ox
	inc	THTEMP_PTR_H
	lda	(THTEMP_PTR_L)
	sta	SPRITE_OFFS_X

	; frm->oy
	inc	THTEMP_PTR_H
	lda	(THTEMP_PTR_L)
	sta	SPRITE_OFFS_Y_L

	; frm->offs_cols
	inc	THTEMP_PTR_H
	lda	(THTEMP_PTR_L)
	sta	SPRITE_COLS_L
	inc	THTEMP_PTR_H
	lda	(THTEMP_PTR_L)
	sta	SPRITE_COLS_H

	; frm->offs_data
	inc	THTEMP_PTR_H
	lda	(THTEMP_PTR_L)
	sta	SPRITE_DATA_L
	inc	THTEMP_PTR_H
	lda	(THTEMP_PTR_L)
	sta	SPRITE_DATA_H

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; thing scale
	ldy	SPRITE_THING
	lda	THING_STRUCT_SCALE,y
	asl
	sta	SPRITE_SCALE_L
	lda	#0
	rol
	sta	SPRITE_SCALE_H
	clc
	lda	#64
	adc	SPRITE_SCALE_L
	sta	SPRITE_SCALE_L
	bcc	:+
	inc	SPRITE_SCALE_H
:
	; a0 = (a0 - projection.a + 512) & 2047
	sec
	lda	MATH_VAR_L
	sbc	PROJECTION_A_L
	sta	SPRITE_ANG_L
	lda	MATH_VAR_H
	sbc	PROJECTION_A_H
	clc
	adc	#$02
	and	#$07
	sta	SPRITE_ANG_H

	; a2x bank
	lda	#BANK_A2X
	sta	REG_RAM_BANK

	; xc = angle2x[a0]
	ldy	SPRITE_ANG_L
	lda	SPRITE_ANG_H
	ora	#>TAB_A2X_L
	sta	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	SPRITE_XC_L
	lda	SPRITE_ANG_H
	ora	#>TAB_A2X_H
	sta	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	SPRITE_XC_H

	; depth table bank
	lda	#BANK_YDEPTH_L
	sta	REG_RAM_BANK

	; dist = tab_depth[dist]
	ldy	SPRITE_DIST_L
	lda	SPRITE_DIST_H
	ora	#>TAB_YDEPTH_L
	sta	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	SPRITE_DIST_L
	lda	SPRITE_DIST_H
	stz	SPRITE_DIST_H
	bne	:+
	lda	TAB_YDEPTH_H,y
	sta	SPRITE_DIST_H
:
	; dscl = (dist * spr_scale) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist
	lda	SPRITE_DIST_L
	sta	VERA_FX_CACHE_L
	lda	SPRITE_DIST_H
	sta	VERA_FX_CACHE_M

	; spr_scale
	lda	SPRITE_SCALE_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_SCALE_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	SPRITE_DSCL_L
	lda	VERA_DATA0
	sta	SPRITE_DSCL_H

	; zdiff = (spr_xoffs * dscl) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; spr_xoffs
	stz	VERA_FX_CACHE_M
	lda	SPRITE_OFFS_X
	asl
	sta	VERA_FX_CACHE_L
	bcc	:+
	lda	#$FF
	sta	VERA_FX_CACHE_M
:
	; dscl
	lda	SPRITE_DSCL_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_DSCL_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; x0 = xc - zdiff
	sec
	lda	SPRITE_XC_L
	sbc	VERA_DATA0
	sta	SPRITE_X0_L
	lda	SPRITE_XC_H
	sbc	VERA_DATA0
	sta	SPRITE_X0_H

	; zdiff = (spr_width * dscl) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; spr_width
	lda	SPRITE_WIDTH
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; dscl is already set

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; x1 = x0 + zdiff
	clc
	lda	SPRITE_X0_L
	adc	VERA_DATA0
	sta	SPRITE_X1_L
	lda	SPRITE_X0_H
	adc	VERA_DATA0
	sta	SPRITE_X1_H

	; xdiff = x1 - x0
	sec
	lda	SPRITE_X1_L
	sbc	SPRITE_X0_L
	sta	SPRITE_XDIFF_L
	lda	SPRITE_X1_H
	sbc	SPRITE_X0_H
	sta	SPRITE_XDIFF_H

	; if(xdiff <= 0)
	bpl	:+
@too_small:
	rts
:
	ora	SPRITE_XDIFF_L
	beq	@too_small

	; if(x1 <= projection.x0d)
	sec
	lda	SPRITE_X1_L
	sbc	PROJECTION_X0D
	sta	SPRITE_TMP_0
	lda	SPRITE_X1_H
	sbc	#$00
	bpl	:+
@too_left:
	rts
:
	ora	SPRITE_TMP_0
	beq	@too_left

	; if(x0 >= projection.x1d)
	sec
	lda	PROJECTION_X1D
	sbc	SPRITE_X0_L
	sta	SPRITE_TMP_0
	lda	#$00
	sbc	SPRITE_X0_H
	bpl	:+
@too_right:
	rts
:
	ora	SPRITE_TMP_0
	beq	@too_right

	; spr_yoffs = (spr_yoffs * spr_scale) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; spr_yoffs
	stz	VERA_FX_CACHE_M
	lda	SPRITE_OFFS_Y_L
	asl
	sta	VERA_FX_CACHE_L
	bcc	:+
	lda	#$FF
	sta	VERA_FX_CACHE_M
:
	; spr_scale
	lda	SPRITE_SCALE_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_SCALE_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	SPRITE_OFFS_Y_L
	lda	VERA_DATA0
	sta	SPRITE_OFFS_Y_H

	;; BOT

	ldy	SPRITE_THING

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; zdiff = projection.z - ((th->z >> 8) + spr_yoffs)
	clc
	lda	THING_STRUCT_Z_L,y
	adc	SPRITE_OFFS_Y_L
	sta	SPRITE_DIFF_L
	lda	THING_STRUCT_Z_H,y
	adc	SPRITE_OFFS_Y_H
	sta	SPRITE_DIFF_H
	sec
	lda	PROJECTION_Z_L
	sbc	SPRITE_DIFF_L
	sta	SPRITE_DIFF_L
	lda	PROJECTION_Z_H
	sbc	SPRITE_DIFF_H
	sta	SPRITE_DIFF_H

	; bot = (dist * zdiff) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist
	lda	SPRITE_DIST_L
	sta	VERA_FX_CACHE_L
	lda	SPRITE_DIST_H
	sta	VERA_FX_CACHE_M

	; zdiff
	lda	SPRITE_DIFF_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_DIFF_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; bot += projection.ycw
	clc
	lda	VERA_DATA0
	adc	PROJECTION_YCW
	sta	SPRITE_BOT_L
	lda	VERA_DATA0
	adc	#0
	sta	SPRITE_BOT_H

	; if(bot <= 0)
	bpl	:+
@too_top:
	rts
:
	ora	SPRITE_BOT_L
	beq	@too_top

	;; TOP

	; zdiff -= (spr_height * spr_scale) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; spr_height
	lda	SPRITE_HEIGHT
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; spr_scale
	lda	SPRITE_SCALE_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_SCALE_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; zdiff -= result >> 8
	sec
	lda	SPRITE_DIFF_L
	sbc	VERA_DATA0
	sta	SPRITE_DIFF_L
	lda	SPRITE_DIFF_H
	sbc	VERA_DATA0
	sta	SPRITE_DIFF_H

	; top = (dist * zdiff) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist
	lda	SPRITE_DIST_L
	sta	VERA_FX_CACHE_L
	lda	SPRITE_DIST_H
	sta	VERA_FX_CACHE_M

	; zdiff
	lda	SPRITE_DIFF_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_DIFF_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; top += projection.ycw
	clc
	lda	VERA_DATA0
	adc	PROJECTION_YCW
	sta	SPRITE_TOP
	lda	VERA_DATA0
	adc	#0

	; if(top < 0)
	bmi	@top_is_negative

	; if(top >= 256)
	beq	:+
@too_bot:
	rts
:
	; if(top >= 120)
	lda	SPRITE_TOP
	cmp	#120
	bcs	@too_bot

@top_is_negative:

	;; BOT CLIP
	lda	#$FF
	sta	SPRITE_CLIP_Y1

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; check if required (dist)
	ldy	SECTOR_PTR_I
	lda	SECTOR_STRUCT_FLOOR_DIST,y
	bne	:+

	; check if required (link)
	lda	SECTOR_STRUCT_FLOOR_LINK,y
	beq	@no_bot_clip
:
	;; y1 = (dist * (projection.z - sec->height_floor)) >> 8;

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; projection.z - sec->height_floor
	sec
	lda	PROJECTION_Z_L
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_Z_H
	sbc	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; y1 += projection.ycw
	clc
	lda	VERA_DATA0
	adc	PROJECTION_YCW
	tax
	lda	VERA_DATA0
	adc	#0
	bne	:+
	stx	SPRITE_CLIP_Y1
	bpl	:+
	stz	SPRITE_CLIP_Y1
:
@no_bot_clip:

	;; TOP CLIP
	stz	SPRITE_CLIP_Y0

	; check if required
	lda	SECTOR_STRUCT_CEILING_LINK,y
	beq	@no_top_clip

	;; y1 = (dist * (projection.z - sec->height_ceiling)) >> 8;

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; projection.z - sec->height_ceiling
	sec
	lda	PROJECTION_Z_L
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_L,y
	sta	VERA_FX_CACHE_H
	lda	PROJECTION_Z_H
	sbc	SECTOR_STRUCT_CEILING_HEIGHT_H,y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; y1 += projection.ycw
	clc
	lda	VERA_DATA0
	adc	PROJECTION_YCW
	tax
	lda	VERA_DATA0
	adc	#0
	bmi	:+
	stx	SPRITE_CLIP_Y0
	beq	:+
	lda	#$FF
	sta	SPRITE_CLIP_Y0
:
@no_top_clip:

	;; texture

	; inverse division bank
	lda	#BANK_IDIV_L
	sta	REG_RAM_BANK

	; inv_div[xdiff]
	jsr	math_idiv

	; tex_step = (spr_width * inv_div) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; spr_width
	lda	SPRITE_WIDTH
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; inv_div
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_H
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	SPRITE_TEX_STEP_L
	lda	VERA_DATA0
	sta	SPRITE_TEX_STEP_H

	; if(x1 > projection.x1d)
	sec
	lda	PROJECTION_X1D
	sbc	SPRITE_X1_L
	lda	#0
	sbc	SPRITE_X1_H
	bpl	:+

	; x1 = projection.x1d
	lda	PROJECTION_X1D
	sta	SPRITE_X1_L
:
	; xdiff = x0 - projection.x0d
	sec
	lda	SPRITE_X0_L
	sbc	PROJECTION_X0D
	sta	SPRITE_XDIFF_L
	lda	SPRITE_X0_H
	sbc	#0
	sta	SPRITE_XDIFF_H

	; tex_now = 0
	stz	SPRITE_TEX_NOW_L
	stz	SPRITE_TEX_NOW_H

	; if(xdiff < 0)
	bpl	:+

	; x0 = projection.x0d
	lda	PROJECTION_X0D
	sta	SPRITE_X0_L

	; tex_now = tex_step * -xdiff

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; tex_step
	lda	SPRITE_TEX_STEP_L
	sta	VERA_FX_CACHE_L
	lda	SPRITE_TEX_STEP_H
	sta	VERA_FX_CACHE_M

	; -xdiff
	sec
	lda	#0
	sbc	SPRITE_XDIFF_L
	sta	VERA_FX_CACHE_H
	lda	#0
	sbc	SPRITE_XDIFF_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	SPRITE_TEX_NOW_L
	lda	VERA_DATA0
	sta	SPRITE_TEX_NOW_H
:
	; texture bank
	lda	#BANK_TSCALE
	sta	REG_RAM_BANK

	; depth = tex_scale[dist]
	ldy	SPRITE_DIST_L
	lda	#>TAB_TSCALE_L
	ora	SPRITE_DIST_H
	sta	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	SPRITE_DEPTH_L
	lda	#>TAB_TSCALE_H
	tsb	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	SPRITE_DEPTH_H

	; tex_scale = tex_scale[dscl]
	lda	#>TAB_TSCALE_L
	ora	SPRITE_DSCL_H
	sta	HI_PTR_H
	ldy	SPRITE_DSCL_L
	lda	(HI_PTR_L),y
	sta	SPRITE_TEX_SCALE_L
	lda	#>TAB_TSCALE_H
	tsb	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	SPRITE_TEX_SCALE_H

	; pointer
	lda	PROJECTION_SPRITE_IDX
	ora	#$A0
	sta	HI_PTR_H

	; sprite bank
	lda	#BANK_DRAW_SPRITES
	sta	REG_RAM_BANK

	; structure - first property
	ldy	#SPRITE_FIRST_PROP

	; spr->depth
	lda	SPRITE_DEPTH_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_DEPTH_H
	sta	(HI_PTR_L),y

	; spr->next
	iny
	lda	#$FF
	sta	(HI_PTR_L),y

	; spr->x0
	iny
	lda	SPRITE_X0_L
	sta	(HI_PTR_L),y

	; spr->x1
	iny
	lda	SPRITE_X1_L
	sta	(HI_PTR_L),y

	; spr->bot
	iny
	lda	SPRITE_BOT_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_BOT_H
	sta	(HI_PTR_L),y

	; spr->y0
	iny
	lda	SPRITE_CLIP_Y0
	sta	(HI_PTR_L),y

	; spr->y1
	iny
	lda	SPRITE_CLIP_Y1
	sta	(HI_PTR_L),y

	; spr->tex_now
	iny
	lda	SPRITE_TEX_NOW_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_TEX_NOW_H
	sta	(HI_PTR_L),y

	; spr->tex_step
	iny
	lda	SPRITE_TEX_STEP_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_TEX_STEP_H
	sta	(HI_PTR_L),y

	; spr->tex_scale
	iny
	lda	SPRITE_TEX_SCALE_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_TEX_SCALE_H
	sta	(HI_PTR_L),y

	; spr->dist
	iny
	lda	SPRITE_DIST_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_DIST_H
	sta	(HI_PTR_L),y

	; spr->scale
	iny
	lda	SPRITE_SCALE_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_SCALE_H
	sta	(HI_PTR_L),y

	; spr->cols
	iny
	lda	SPRITE_COLS_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_COLS_H
	sta	(HI_PTR_L),y

	; spr->data
	iny
	lda	SPRITE_DATA_L
	sta	(HI_PTR_L),y
	iny
	lda	SPRITE_DATA_H
	sta	(HI_PTR_L),y

	; spr->light
	iny
	lda	#0
	ldx	SPRITE_BRIGHT
	bmi	:+
	lda	PROJECTION_LIGHT
:
	sta	(HI_PTR_L),y

	; other pointer
	lda	#80
	sta	TMP_PTR_L
	lda	HI_PTR_H
	sta	TMP_PTR_H

	; clipping copy & depth check
	lda	SPRITE_X0_L
	lsr
	tay
	asl
	sta	SPRITE_X0_L
	sec
	lda	SPRITE_X1_L
	ina
	sbc	SPRITE_X0_L
	lsr
	tax
@loop_proj:
	; spr->clip_bot[x0] = clip_bot[x0]
	lda	RENDER_CLIP_BOT,y
	sta	(TMP_PTR_L),y

	; if(spr->depth > tmap_scale[x0])
	sec
	lda	SPRITE_DEPTH_L
	sbc	RENDER_TEXSCALE_L,y
	sta	MATH_TEMP
	lda	SPRITE_DEPTH_H
	sbc	RENDER_TEXSCALE_H,y
	bmi	:+
	ora	MATH_TEMP
	beq	:+

	; spr->clip_top[x0] = 120
	lda	#120
	sta	(HI_PTR_L),y
	bra	:++
:
	; spr->clip_top[x0] = clip_top[x0]
	lda	RENDER_CLIP_TOP,y
	sta	(HI_PTR_L),y
:
	; next
	iny
	dex
	bne	@loop_proj

	; depth sorting

	; prev = 0xFF
	lda	#$FF
	sta	SPRITE_SORT_PREV

	; check = proj_spr_first
	lda	PROJECTION_FIRST_SPRITE
	sta	SPRITE_SORT_CHECK

	; while(!(check & 0x80))
@loop_sort:
	lda	SPRITE_SORT_CHECK
	bmi	@done

	; 'check' pointer
	ora	#$A0
	sta	IH_PTR_H

	; if(spr->depth > check->depth)
	ldy	#SPRITE_FIRST_PROP
	sec
	lda	(IH_PTR_L),y
	sbc	SPRITE_DEPTH_L
	iny
	lda	(IH_PTR_L),y
	sbc	SPRITE_DEPTH_H
	bpl	@done

	; prev = check
	lda	SPRITE_SORT_CHECK
	sta	SPRITE_SORT_PREV

	; check = check->next
	iny
	lda	(IH_PTR_L),y
	sta	SPRITE_SORT_CHECK

	; next
	bra	@loop_sort
@done:
	; if(!(check & 0x80))
	lda	SPRITE_SORT_CHECK
	bmi	:+

	; spr->next = check
	ldy	#SPRITE_FIRST_PROP+2
	sta	(HI_PTR_L),y
:
	; if(!(prev & 0x80))
	lda	SPRITE_SORT_PREV
	bmi	:+

	; 'prev' pointer
	ora	#$A0
	sta	IH_PTR_H

	; prev->next = proj_spr_idx
	lda	PROJECTION_SPRITE_IDX
	sta	(IH_PTR_L),y
	bra	:++
:
	; proj_spr_first = proj_spr_idx
	lda	PROJECTION_SPRITE_IDX
	sta	PROJECTION_FIRST_SPRITE
:
	; proj_spr_idx++
	inc	PROJECTION_SPRITE_IDX

	; time
	jmp	time_update

;;;
; sprite: render
;;;

render_sprite:
	; other pointer
	lda	#80
	sta	TMP_PTR_L
	lda	HI_PTR_H
	sta	TMP_PTR_H

	; sprite bank
	lda	#BANK_DRAW_SPRITES
	sta	REG_RAM_BANK

	; structure - first property
	ldy	#SPRITE_FIRST_PROP

	; spr->depth
	lda	(HI_PTR_L),y
	sta	SPRITE_DEPTH_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_DEPTH_H

	; spr->next
	iny

	; spr->x0
	iny
	lda	(HI_PTR_L),y
	sta	VIDEO_DRAW_X

	; spr->x1
	iny
	lda	(HI_PTR_L),y

	; get width
	sec
	sbc	VIDEO_DRAW_X
	sta	SPRITE_X1_L

	; spr->bot
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_BOT_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_BOT_H

	; spr->y0
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_CLIP_Y0

	; spr->y1
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_CLIP_Y1

	; spr->tex_now
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_TEX_NOW_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_TEX_NOW_H

	; spr->tex_step
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_TEX_STEP_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_TEX_STEP_H

	; spr->tex_scale
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_TEX_SCALE_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_TEX_SCALE_H

	; spr->dist
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_DIST_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_DIST_H

	; spr->scale (doubled)
	iny
	lda	(HI_PTR_L),y
	asl
	sta	SPRITE_SCALE_L
	iny
	lda	(HI_PTR_L),y
	rol
	sta	SPRITE_SCALE_H

	; spr->cols
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_COLS_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_COLS_H

	; spr->data
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_DATA_L
	iny
	lda	(HI_PTR_L),y
	sta	SPRITE_DATA_H

	; spr->light
	iny
	lda	(HI_PTR_L),y
	asl
	asl

	; TODO: add remap index, check for player

	; check for fullbright + no remap
	bne	:+

	; use fullbright code
	lda	#<video_draw_vline
	sta	sprite_draw_opcode+1
	lda	#>video_draw_vline
	sta	sprite_draw_opcode+2

	bra	:++
:
	; enable light table
	clc
	adc	#>TAB_LIGHTMAPS
	sta	LIGHTMAP_H

	; use remapping code
	lda	#<video_draw_tline
	sta	sprite_draw_opcode+1
	lda	#>video_draw_tline
	sta	sprite_draw_opcode+2
:
	; column loop
sprite_loop_col:
	; sprite bank
	lda	#BANK_DRAW_SPRITES
	sta	REG_RAM_BANK

	; half X
	lda	VIDEO_DRAW_X
	lsr
	tay

	; skip odd column
detcode_sprite_vline:
	bcs	:+	; modified opcode

	; load top clip
	lda	(HI_PTR_L),y
	sta	SPRITE_CLIP_TOP
	bpl	:++
:
	jmp	sprite_skip_col
:
	cmp	SPRITE_CLIP_Y0
	bcs	:+
	lda	SPRITE_CLIP_Y0
	sta	SPRITE_CLIP_TOP
:
	; load bot clip
	lda	(TMP_PTR_L),y
	sta	SPRITE_CLIP_BOT
	cmp	SPRITE_CLIP_Y1
	bcc	:+
	lda	SPRITE_CLIP_Y1
	sta	SPRITE_CLIP_BOT
:
	; texture X
	ldy	SPRITE_TEX_NOW_H

	; get column pointer
	lda	SPRITE_COLS_H
	asl
	asl
	asl
	ldx	SPRITE_COLS_L
	ora	TAB_BANK,x
	sta	COLUMN_PTR_B
	txa
	and	#$1F
	ora	#$A0
	sta	COLUMN_PTR_H

	; get column offset
	lda	COLUMN_PTR_B
	sta	REG_RAM_BANK
	stz	COLUMN_PTR_L
	lda	(COLUMN_PTR_L),y
	sta	COLORMAP_L
	lda	#$80
	sta	COLUMN_PTR_L
	lda	(COLUMN_PTR_L),y

	; add to data
	clc
	adc	SPRITE_DATA_L
	sta	COLORMAP_H
	lda	SPRITE_DATA_H
	adc	#0

	; get texture pointer
	asl
	asl
	asl
	ldx	COLORMAP_H
	ora	TAB_BANK,x
	sta	COLORMAP_B
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	COLORMAP_H

	; get column length
	lda	(COLORMAP_L)
	sta	SPRITE_COL_LEN
	bne	:+
	jmp	sprite_skip_col
:
	;; diff = (offs * spr->scale) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; offs
	inc	COLORMAP_L
	lda	(COLORMAP_L)
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M
	inc	COLORMAP_L

	; spr->scale
	lda	SPRITE_SCALE_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_SCALE_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	;; bot = spr->bot - ((spr->dist * diff) >> 8)

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; diff
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_M

	; spr->dist
	lda	SPRITE_DIST_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_DIST_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8), and subtract
	sec
	lda	SPRITE_BOT_L
	sbc	VERA_DATA0
	sta	SPRITE_DRAW_BOT_L
	lda	SPRITE_BOT_H
	sbc	VERA_DATA0
	sta	SPRITE_DRAW_BOT_H

	; if(bot <= 0)
	bmi	:+

	; if(bot >= 256)
	bne	:+++

	; if(bot == 0)
	ora	SPRITE_DRAW_BOT_L
	bne	:++
:
	jmp	sprite_skip_col
:
	; if(bot < clip_top)
	lda	SPRITE_DRAW_BOT_L
	cmp	SPRITE_CLIP_TOP
	bcc	:--
:
	;; diff = (len * spr->scale) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; offs
	lda	SPRITE_COL_LEN
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; spr->scale
	lda	SPRITE_SCALE_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_SCALE_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	;; top = bot - ((spr->dist * diff) >> 8) - 1

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; diff
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_L
	lda	VERA_DATA0
	sta	VERA_FX_CACHE_M

	; spr->dist
	lda	SPRITE_DIST_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_DIST_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8), and subtract with clc
	clc
	lda	SPRITE_DRAW_BOT_L
	sbc	VERA_DATA0
	sta	SPRITE_DRAW_TOP
	lda	SPRITE_DRAW_BOT_H
	sbc	VERA_DATA0

	; if(top < 0)
	bmi	@top_is_negative

	; if(top >= 256)
	bne	:+

	; if(top >= clip_bot)
	lda	SPRITE_DRAW_TOP
	cmp	SPRITE_CLIP_BOT
	bcc	:++
:
	jmp	sprite_skip_col
:
	; if(top < clip_top)
	lda	SPRITE_DRAW_TOP
	cmp	SPRITE_CLIP_TOP
	bcs	:+

@top_is_negative:
	; top = clip_top
	lda	SPRITE_CLIP_TOP
	sta	SPRITE_DRAW_TOP
:
	; y1 = bot
	lda	SPRITE_DRAW_BOT_L
	sta	VIDEO_DRAW_Y

	; tnow = -1
	lda	#$FF
	sta	SPRITE_DRAW_TNOW_L
	sta	SPRITE_DRAW_TNOW_M
	sta	SPRITE_DRAW_TNOW_H

	; diff = bot - clip_bot
	sec
	lda	SPRITE_DRAW_BOT_L
	sbc	SPRITE_CLIP_BOT
	sta	SPRITE_DRAW_BOT_L
	lda	SPRITE_DRAW_BOT_H
	sbc	#0
	sta	SPRITE_DRAW_BOT_H

	; if(diff >= 0)
	bmi	@skip_bot_clip
	ora	SPRITE_DRAW_BOT_L
	beq	@skip_bot_clip

	;; tnow = (spr->tex_scale * diff) >> 2

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; spr->tex_scale
	lda	SPRITE_TEX_SCALE_L
	sta	VERA_FX_CACHE_L
	lda	SPRITE_TEX_SCALE_H
	sta	VERA_FX_CACHE_M

	; diff
	lda	SPRITE_DRAW_BOT_L
	sta	VERA_FX_CACHE_H
	lda	SPRITE_DRAW_BOT_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 2)
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	SPRITE_DRAW_TNOW_L
	lda	VERA_DATA0
	sta	SPRITE_DRAW_TNOW_M
	lda	VERA_DATA0
	sec
	ror
	sta	SPRITE_DRAW_TNOW_H
	ror	SPRITE_DRAW_TNOW_M
	ror	SPRITE_DRAW_TNOW_L
	sec
	ror	SPRITE_DRAW_TNOW_H
	ror	SPRITE_DRAW_TNOW_M
	ror	SPRITE_DRAW_TNOW_L

	; y1 = clip_bot
	lda	SPRITE_CLIP_BOT
	sta	VIDEO_DRAW_Y

@skip_bot_clip:
	; get draw length
	sec
	lda	VIDEO_DRAW_Y
	sbc	SPRITE_DRAW_TOP
	bmi	sprite_skip_col
	sta	VIDEO_DRAW_L

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache, enable affine helper, transparent writes
	lda	#%10000011
	sta	VERA_FX_CTRL

	; dcsel = 3, addrsel = 0
	lda	#$06
	sta	VERA_CTRL

	; set affine Y step
	lda	SPRITE_TEX_SCALE_H
	lsr
	sta	VERA_FX_X_INCR_H
	lda	SPRITE_TEX_SCALE_L
	ror
	sta	VERA_FX_X_INCR_L

	; dcsel = 5, addrsel = 0
	lda	#$0A
	sta	VERA_CTRL

	; set affine Y sub
	lda	SPRITE_DRAW_TNOW_L
	sta	VERA_FX_X_POS_S

	; dcsel = 4, addrsel = 0
	lda	#$08
	sta	VERA_CTRL

	; set affine Y position
	lda	SPRITE_DRAW_TNOW_M
	sta	VERA_FX_X_POS_L
	lda	SPRITE_DRAW_TNOW_H
	sta	VERA_FX_X_POS_H

	; draw column
sprite_draw_opcode:
	jsr	video_draw_tline	; modified address

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

sprite_skip_col:
	; texture
	clc
	lda	SPRITE_TEX_NOW_L
	adc	SPRITE_TEX_STEP_L
	sta	SPRITE_TEX_NOW_L
	lda	SPRITE_TEX_NOW_H
	adc	SPRITE_TEX_STEP_H
	sta	SPRITE_TEX_NOW_H

	; next
	inc	VIDEO_DRAW_X
	dec	SPRITE_X1_L
	beq	:+
	jmp	sprite_loop_col
:
	rts

;;;
; masked: prepare
;;;

prepare_masked:
	; mt_ox = wall->masked.tex_mid_ox
	iny
	lda	(WALL_PTR_L),y
	sta	WALL_MT_OX

	; mt_bz = wall->masked.tex_mid_oy * 2
	iny
	lda	(WALL_PTR_L),y
	asl
	sta	WALL_MT_BZ_L
	lda	#0
	rol
	sta	WALL_MT_BZ_H

	; mt_xor = wall->masked.tflags & 0b00001000 ? 0x00 : 0xFF
	stz	WALL_MT_XOR
	ldy	#WALL_STRUCT_TEX_FLAGS
	lda	(WALL_PTR_L),y
	and	#%00001000
	bne	:+
	dec	WALL_MT_XOR
:
	lda	SECTOR_PTR_I
	sta	MATH_VAR_L

	; if(wall->masked.blockmid & 0b10000000)
	ldy	#WALL_STRUCT_TEX_MID_B
	lda	(WALL_PTR_L),y
	bpl	:+

	; if(wall->portal.backsector)
	ldy	#WALL_STRUCT_BACKSECTOR
	lda	(WALL_PTR_L),y
	beq	:+

	sta	MATH_VAR_L
:
	; generate X map
	jsr	generate_texture_map

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; mt_sflags = sec->flags
	ldy	SECTOR_PTR_I
	lda	SECTOR_STRUCT_FLAGS,y
	sta	WALL_MT_SFLAGS

	; mt_bz += ss->floor.height
	ldy	MATH_VAR_L
	clc
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_L,y
	adc	WALL_MT_BZ_L
	sta	WALL_MT_BZ_L
	lda	SECTOR_STRUCT_FLOOR_HEIGHT_H,y
	adc	WALL_MT_BZ_H
	sta	WALL_MT_BZ_H

	; masked bank
	lda	#BANK_DRAW_MASKED
	sta	REG_RAM_BANK

	; masked_idx = proj_msk_idx
	lda	PROJECTION_MASKED_IDX
	asl
	sta	WALL_MASKED_IDX

	; masked pointers
	ora	#$A0
	sta	HI_PTR_H
	sta	MASKED_PTR_H
	lda	#80
	sta	MASKED_PTR_L

	; mt->scale_now
	ldy	#MASKED_FIRST_PROP
	lda	WALL_SCALE_NOW_L
	sta	(HI_PTR_L),y
	iny
	lda	WALL_SCALE_NOW_M
	sta	(HI_PTR_L),y
	iny
	lda	WALL_SCALE_NOW_H
	sta	(HI_PTR_L),y

	; mt->scale_step = scale_step
	iny
	lda	WALL_SCALE_STEP_L
	sta	(HI_PTR_L),y
	iny
	lda	WALL_SCALE_STEP_M
	sta	(HI_PTR_L),y
	iny
	lda	WALL_SCALE_STEP_H
	sta	(HI_PTR_L),y

	; mt->x0
	iny
	lda	WALL_X0
	sta	(HI_PTR_L),y

	; mt->x1
	iny
	lda	WALL_X1
	sta	(HI_PTR_L),y

	; mt->texture
	iny
	lda	WALL_MT_TEXTURE
	sta	(HI_PTR_L),y

	; mt->bz
	iny
	lda	WALL_MT_BZ_L
	sta	(HI_PTR_L),y
	iny
	lda	WALL_MT_BZ_H
	sta	(HI_PTR_L),y

	; mt->xor
	iny
	lda	WALL_MT_XOR
	sta	(HI_PTR_L),y

	; mt->ox
	iny
	lda	WALL_MT_OX
	sta	(HI_PTR_L),y

	; mt->sflags
	iny
	lda	WALL_MT_SFLAGS
	sta	(HI_PTR_L),y

	; mt->tmap_scale
	sec
	lda	WALL_X1
	sbc	WALL_X0
	pha
	tax
	ldy	WALL_X0
@loop_scale:
	lda	RENDER_TEXSCALE_L,y
	sta	(HI_PTR_L),y
	lda	RENDER_TEXSCALE_H,y
	sta	(MASKED_PTR_L),y
	iny
	dex
	bne	@loop_scale

	jsr	time_update

	; mt->tmap_coord
	inc	HI_PTR_H
	plx
	lda	WALL_X0
	asl
	tay
@loop_coord:
	lda	RENDER_TEXTURE_X,y
	sta	(HI_PTR_L),y
	iny
	lda	RENDER_TEXTURE_X,y
	sta	(HI_PTR_L),y
	iny
	dex
	bne	@loop_coord

	; proj_msk_idx
	inc	PROJECTION_MASKED_IDX

	; wall bank
	lda	WALL_PTR_B
	sta	REG_RAM_BANK

	; done
	lda	#160
	sta	MASKED_PTR_L

	jmp	time_update

;;;
; masked: clip
;;;

masked_clip_top:
	; masked bank
	lda	#BANK_DRAW_MASKED
	sta	REG_RAM_BANK

	; copy top clip
	sec
	lda	WALL_X1
	sbc	WALL_X0_TMP
	tax
	ldy	WALL_X0_TMP
@loop:
	lda	RENDER_CLIP_TOP,y
	sta	(MASKED_PTR_L),y
	iny
	dex
	bne	@loop

	inc	MASKED_PTR_H

	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	jmp	time_update

masked_clip_bot:
	; masked bank
	lda	#BANK_DRAW_MASKED
	sta	REG_RAM_BANK

	; copy bot clip
	sec
	lda	WALL_X1
	sbc	WALL_X0_TMP
	tax
	ldy	WALL_X0_TMP
@loop:
	lda	RENDER_CLIP_BOT,y
	sta	(MASKED_PTR_L),y
	iny
	dex
	bne	@loop

	inc	MASKED_PTR_H

	ldy	SECTOR_PTR_I
	ldx	BACKSEC_PTR_I

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	jmp	time_update

;;;
; masked: render
;;;

render_masked:
	; masked pointers
	ora	#$A0
	sta	HI_PTR_H
	sta	WALL_PTR_H
	sta	MASKED_PTR_H
	ina
	sta	IH_PTR_H
	sta	WALN_PTR_H
	lda	#160
	sta	WALL_PTR_L
	sta	WALN_PTR_L
	lda	#80
	sta	MASKED_PTR_L

	; masked bank
	lda	#BANK_DRAW_MASKED
	sta	REG_RAM_BANK

	; structure - first property
	ldy	#MASKED_FIRST_PROP

	; mt->scale_now
	lda	(HI_PTR_L),y
	sta	MASKED_SCALE_NOW_L
	iny
	lda	(HI_PTR_L),y
	sta	MASKED_SCALE_NOW_M
	iny
	lda	(HI_PTR_L),y
	sta	MASKED_SCALE_NOW_H

	; mt->scale_step = scale_step
	iny
	lda	(HI_PTR_L),y
	sta	MASKED_SCALE_STEP_L
	iny
	lda	(HI_PTR_L),y
	sta	MASKED_SCALE_STEP_M
	iny
	lda	(HI_PTR_L),y
	sta	MASKED_SCALE_STEP_H

	; mt->x0
	iny
	lda	(HI_PTR_L),y
	asl
	sta	VIDEO_DRAW_X

	; mt->x1
	iny
	lda	(HI_PTR_L),y
	asl

	; get width
	sec
	sbc	VIDEO_DRAW_X
	sta	MASKED_X1

	; mt->texture
	iny
	lda	(HI_PTR_L),y
	sta	MASKED_MT_TEXTURE

	; projection.z - mt->bz
	sec
	iny
	lda	PROJECTION_Z_L
	sbc	(HI_PTR_L),y
	sta	MASKED_MT_BZ_L
	iny
	lda	PROJECTION_Z_H
	sbc	(HI_PTR_L),y
	sta	MASKED_MT_BZ_H

	; mt->xor
	iny
	lda	(HI_PTR_L),y
	sta	MASKED_MT_XOR

	; mt->ox
	iny
	lda	(HI_PTR_L),y
	sta	MASKED_MT_OX

	; mt->sflags
	iny
	lda	(HI_PTR_L),y

	; get light
	tax
	lda	TAB_SWAP,x
	and	#$07
	sta	PROJECTION_LIGHT

	; texture
	stz	MASKED_TEXTURE_FLAGS
	lda	MASKED_MT_TEXTURE
	jsr	texture_set

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; column loop
masked_loop_col:
	; masked bank
	lda	#BANK_DRAW_MASKED
	sta	REG_RAM_BANK

	; half X
	lda	VIDEO_DRAW_X
	lsr
	tay

	; skip odd column
detcode_masked_vline:
	bcs	:+	; modified opcode

	; top clip
	lda	(WALL_PTR_L),y
	sta	MASKED_CLIP_TOP
	bpl	:++
:
	jmp	@skip_col
:
	; bot clip
	lda	(WALN_PTR_L),y
	sta	MASKED_CLIP_BOT

	; texture scale
	lda	(HI_PTR_L),y
	sta	MASKED_TEX_SCALE_L
	lda	(MASKED_PTR_L),y
	sta	MASKED_TEX_SCALE_H

	; texture X
	sec
	ldy	VIDEO_DRAW_X
	lda	(IH_PTR_L),y
	sbc	MASKED_MT_OX
	eor	MASKED_MT_XOR
	and	#$7F
	tay

	; get column offset
	lda	COLUMN_PTR_B
	sta	REG_RAM_BANK
	stz	COLUMN_PTR_L
	lda	(COLUMN_PTR_L),y
	sta	COLORMAP_L
	lda	#$80
	sta	COLUMN_PTR_L
	lda	(COLUMN_PTR_L),y

	; add to data
	clc
	adc	TEXTURE_PTR_L
	sta	COLORMAP_H
	lda	TEXTURE_PTR_H
	adc	#0

	; get texture pointer
	asl
	asl
	asl
	ldx	COLORMAP_H
	ora	TAB_BANK,x
	sta	COLORMAP_B
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	COLORMAP_H

	; get column length
	lda	(COLORMAP_L)
	bne	:+
	jmp	@skip_col
:
	asl
	sta	MASKED_COL_LEN

	; offs
	inc	COLORMAP_L
	lda	(COLORMAP_L)
	asl
	sta	MATH_VAR_L
	inc	COLORMAP_L

	; zdiff -= offs
	sec
	lda	MASKED_MT_BZ_L
	sbc	MATH_VAR_L
	sta	MASKED_ZDIFF_L
	lda	MASKED_MT_BZ_H
	sbc	#0
	sta	MASKED_ZDIFF_H

	;; bot = ((scale_now >> 8) * zdiff) >> 8
	;; bot += projection.ycw

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; scale_now >> 8
	lda	MASKED_SCALE_NOW_M
	sta	VERA_FX_CACHE_L
	lda	MASKED_SCALE_NOW_H
	sta	VERA_FX_CACHE_M

	; zdiff
	lda	MASKED_ZDIFF_L
	sta	VERA_FX_CACHE_H
	lda	MASKED_ZDIFF_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8), and add ycw
	clc
	lda	VERA_DATA0
	adc	PROJECTION_YCW
	sta	MASKED_DRAW_BOT_L
	lda	VERA_DATA0
	adc	#0
	sta	MASKED_DRAW_BOT_H

	; if(bot <= 0)
	bmi	:+

	; if(bot >= 256)
	bne	:+++

	; if(bot == 0)
	ora	MASKED_DRAW_BOT_L
	bne	:++
:
	jmp	@skip_col
:
	; if(bot < clip_top)
	lda	MASKED_DRAW_BOT_L
	cmp	MASKED_CLIP_TOP
	bcc	:--
:
	;; top = ((scale_now >> 8) * (zdiff - len)) >> 8
	;; top += projection.ycw

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; scale_now >> 8 is already set

	; zdiff - len
	sec
	lda	MASKED_ZDIFF_L
	sbc	MASKED_COL_LEN
	sta	VERA_FX_CACHE_H
	lda	MASKED_ZDIFF_H
	sbc	#0
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8), and add ycw
	clc
	lda	VERA_DATA0
	adc	PROJECTION_YCW
	sta	MASKED_DRAW_TOP
	lda	VERA_DATA0
	adc	#0

	; if(top < 0)
	bmi	@top_is_negative

	; if(top >= 256)
	bne	:+

	; if(top >= clip_bot)
	lda	MASKED_DRAW_TOP
	cmp	MASKED_CLIP_BOT
	bcc	:++
:
	jmp	@skip_col
:
	; if(top < clip_top)
	lda	MASKED_DRAW_TOP
	cmp	MASKED_CLIP_TOP
	bcs	:+

@top_is_negative:
	; top = clip_top
	lda	MASKED_CLIP_TOP
	sta	MASKED_DRAW_TOP
:
	; y1 = bot
	lda	MASKED_DRAW_BOT_L
	sta	VIDEO_DRAW_Y

	; tnow = -1
	lda	#$FF
	sta	MASKED_DRAW_TNOW_L
	sta	MASKED_DRAW_TNOW_M
	sta	MASKED_DRAW_TNOW_H

	; diff = bot - clip_bot
	sec
	lda	MASKED_DRAW_BOT_L
	sbc	MASKED_CLIP_BOT
	sta	MASKED_DRAW_BOT_L
	lda	MASKED_DRAW_BOT_H
	sbc	#0
	sta	MASKED_DRAW_BOT_H

	; if(diff >= 0)
	bmi	@skip_bot_clip
	ora	MASKED_DRAW_BOT_L
	beq	@skip_bot_clip

	;; tnow = (spr->tex_scale * diff) >> 2

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; tmap_scale
	lda	MASKED_TEX_SCALE_L
	sta	VERA_FX_CACHE_L
	lda	MASKED_TEX_SCALE_H
	sta	VERA_FX_CACHE_M

	; diff
	lda	MASKED_DRAW_BOT_L
	sta	VERA_FX_CACHE_H
	lda	MASKED_DRAW_BOT_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 2)
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	MASKED_DRAW_TNOW_L
	lda	VERA_DATA0
	sta	MASKED_DRAW_TNOW_M
	lda	VERA_DATA0
	sec
	ror
	sta	MASKED_DRAW_TNOW_H
	ror	MASKED_DRAW_TNOW_M
	ror	MASKED_DRAW_TNOW_L
	sec
	ror	MASKED_DRAW_TNOW_H
	ror	MASKED_DRAW_TNOW_M
	ror	MASKED_DRAW_TNOW_L

	; y1 = clip_bot
	lda	MASKED_CLIP_BOT
	sta	VIDEO_DRAW_Y

@skip_bot_clip:
	; get draw length
	sec
	lda	VIDEO_DRAW_Y
	sbc	MASKED_DRAW_TOP
	bmi	@skip_col
	sta	VIDEO_DRAW_L

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable cache, enable affine helper, transparent writes
	lda	#%10000011
	sta	VERA_FX_CTRL

	; dcsel = 3, addrsel = 0
	lda	#$06
	sta	VERA_CTRL

	; set affine Y step
	lda	MASKED_TEX_SCALE_H
	lsr
	sta	VERA_FX_X_INCR_H
	lda	MASKED_TEX_SCALE_L
	ror
	sta	VERA_FX_X_INCR_L

	; dcsel = 5, addrsel = 0
	lda	#$0A
	sta	VERA_CTRL

	; set affine Y sub
	lda	MASKED_DRAW_TNOW_L
	sta	VERA_FX_X_POS_S

	; dcsel = 4, addrsel = 0
	lda	#$08
	sta	VERA_CTRL

	; set affine Y position
	lda	MASKED_DRAW_TNOW_M
	sta	VERA_FX_X_POS_L
	lda	MASKED_DRAW_TNOW_H
	sta	VERA_FX_X_POS_H

	; draw column
	jsr	video_draw_vline

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

@skip_col:
	; texture
	lda	VIDEO_DRAW_X
	lsr
	bcc	:+
	clc
	lda	MASKED_SCALE_NOW_L
	adc	MASKED_SCALE_STEP_L
	sta	MASKED_SCALE_NOW_L
	lda	MASKED_SCALE_NOW_M
	adc	MASKED_SCALE_STEP_M
	sta	MASKED_SCALE_NOW_M
	lda	MASKED_SCALE_NOW_H
	adc	MASKED_SCALE_STEP_H
	sta	MASKED_SCALE_NOW_H
:
	; next
	inc	VIDEO_DRAW_X
	dec	MASKED_X1
	beq	:+
	jmp	masked_loop_col
:
	rts

;;;
; apply plane effect
;;;

plane_apply_effect:
	; check texture index
	ldy	RENDER_TEXTURE_INFO_IDX
	bpl	:+
	rts
:
	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; check texture effect
	lda	TEXINFO_STRUCT_EFFECT_0,y
	and	#3
	tax
	bne	:+
	rts
:
	; get ticks
	lda	G_LEVEL_TICK_L
	sta	RENDER_TEXTURE_ETIME_L
	lda	G_LEVEL_TICK_H
	sta	RENDER_TEXTURE_ETIME_H

	; scale time
	lda	TEXINFO_STRUCT_EFFECT_1,y
	beq	@skip_ticks
	bpl	:+

	; up
	and	#$7F
	beq	@skip_ticks
@loop_up:
	asl	RENDER_TEXTURE_ETIME_L
	dea
	bne	@loop_up
	bra	@skip_ticks
:
	; down
@loop_dn:
	lsr	RENDER_TEXTURE_ETIME_H
	ror	RENDER_TEXTURE_ETIME_L
	dea
	bne	@loop_dn

@skip_ticks:
	; pick effect
	cpx	#1
	bne	@sincos

	;; random

	lda	RENDER_TEXTURE_ETIME_H
	and	#$03
	ora	#>TAB_RANDOM
	sta	RENDER_TEXTURE_ETIME_H

	phy
	lda	TEXINFO_STRUCT_EFFECT_2,y
	sta	MATH_VAR_L

	; random bank
	lda	#BANK_RANDOM
	sta	REG_RAM_BANK

	; X
	bit	MATH_VAR_L
	bmi	:+
	clc
	lda	(RENDER_TEXTURE_ETIME_L)
	adc	RENDER_TEXTURE_OX
	sta	RENDER_TEXTURE_OX
:
	; Y
	bit	MATH_VAR_L
	bvs	:+
	clc
	ldy	#4
	lda	(RENDER_TEXTURE_ETIME_L),y
	adc	RENDER_TEXTURE_OY
	sta	RENDER_TEXTURE_OY
:
	; A
	lda	MATH_VAR_L
	lsr
	bcs	:+
	clc
	ldy	#8
	lda	(RENDER_TEXTURE_ETIME_L),y
	adc	RENDER_TEXTURE_ANGLE
	sta	RENDER_TEXTURE_ANGLE
:
	ply
	rts

@sincos:
	;; 'O' or '8'

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; load Y scale
	lda	TEXINFO_STRUCT_EFFECT_3,y
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M
	lda	TEXINFO_STRUCT_EFFECT_0,y
	asl
	bpl	:+
	; negative
	lda	TEXINFO_STRUCT_EFFECT_3,y
	eor	#$FF
	ina
	sta	VERA_FX_CACHE_L
	lda	#$FF
	sta	VERA_FX_CACHE_M
:
	; load cos
	ldx	RENDER_TEXTURE_ETIME_L
	math_read_cos	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add (result >> 8) to Y offset
	clc
	lda	VERA_DATA0
	adc	RENDER_TEXTURE_OY
	sta	RENDER_TEXTURE_OY

	; check for '8'
	lda	TEXINFO_STRUCT_EFFECT_0,y
	and	#3
	cmp	#3
	bne	:+

	; double ticks
	asl	RENDER_TEXTURE_ETIME_L
:
	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; load X scale
	lda	TEXINFO_STRUCT_EFFECT_2,y
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M
	lda	TEXINFO_STRUCT_EFFECT_0,y
	bpl	:+

	; negative
	lda	TEXINFO_STRUCT_EFFECT_2,y
	eor	#$FF
	ina
	sta	VERA_FX_CACHE_L
	lda	#$FF
	sta	VERA_FX_CACHE_M
:
	; load sin
	ldx	RENDER_TEXTURE_ETIME_L
	math_read_sin	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; add (result >> 8) to X offset
	clc
	lda	VERA_DATA0
	adc	RENDER_TEXTURE_OX
	sta	RENDER_TEXTURE_OX

	rts

;;;
; set plane angle
;;;
; A is angle
; assuming A was just loaded

plane_set_angle:
	; get angle
	lda	RENDER_TEXTURE_ANGLE
	bne	:+

	;; copy values

	lda	PROJECTION_X_H
	ror
	lda	PROJECTION_X_L
	ror
	sta	PLANE_POS_X_H
	lda	#0
	ror
	sta	PLANE_POS_X_L

	lda	PROJECTION_Y_H
	ror
	lda	PROJECTION_Y_L
	ror
	sta	PLANE_POS_Y_H
	lda	#0
	ror
	sta	PLANE_POS_Y_L

	lda	PROJECTION_SIN_L
	sta	PLANE_SIN_L
	lda	PROJECTION_SIN_H
	sta	PLANE_SIN_H

	lda	PROJECTION_COS_L
	sta	PLANE_COS_L
	lda	PROJECTION_COS_H
	sta	PLANE_COS_H

	rts
:
	;; angle calculations

	; save angle
	tax
	tay

	;; projection.pl_x = projection.y * tab_sin[ang] + projection.x * tab_cos[ang]

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; projection.y
	lda	PROJECTION_Y_L
	sta	VERA_FX_CACHE_L
	lda	PROJECTION_Y_H
	sta	VERA_FX_CACHE_M

	; tab_sin[ang]
	math_read_sin	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; projection.x
	lda	PROJECTION_X_L
	sta	VERA_FX_CACHE_L
	lda	PROJECTION_X_H
	sta	VERA_FX_CACHE_M

	; tab_cos[ang]
	math_read_cos	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 1)
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	PLANE_POS_X_L
	lda	VERA_DATA0
	sta	PLANE_POS_X_H
	lda	VERA_DATA0
	ror
	ror	PLANE_POS_X_H
	ror	PLANE_POS_X_L

	;; projection.pl_y = projection.y * tab_cos[ang] - projection.x * tab_sin[ang]

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; projection.y
	lda	PROJECTION_Y_L
	sta	VERA_FX_CACHE_L
	lda	PROJECTION_Y_H
	sta	VERA_FX_CACHE_M

	; tab_cos[ang] is already set

	; accumulate
	lda	VERA_FX_ACCUM

	; projection.x
	lda	PROJECTION_X_L
	sta	VERA_FX_CACHE_L
	lda	PROJECTION_X_H
	sta	VERA_FX_CACHE_M

	; -tab_sin[ang]
	tya
	eor	#$80
	tax
	math_read_sin	VERA_FX_CACHE_H, VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 1)
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	PLANE_POS_Y_L
	lda	VERA_DATA0
	sta	PLANE_POS_Y_H
	lda	VERA_DATA0
	ror
	ror	PLANE_POS_Y_H
	ror	PLANE_POS_Y_L

	; ang += projection.a8
	clc
	tya
	adc	PROJECTION_A8
	tax

	; projection.pl_sin = tab_sin[ang]
	math_read_sin	PLANE_SIN_L, PLANE_SIN_H

	; projection.pl_cos = tab_cos[ang]
	math_read_cos	PLANE_COS_L, PLANE_COS_H

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	rts

;;;
; update weapon pixels
;;;

update_weapon_gfx:
	; sprite info pointer
	lda	TMP_REND_WPN_IDX
	sta	TMP_PTR_H
	stz	TMP_PTR_L
	lsr	TMP_PTR_H
	ror	TMP_PTR_L
	lsr	TMP_PTR_H
	ror	TMP_PTR_L
	lda	#$A0
	tsb	TMP_PTR_H

	; wsprite frame bank
	lda	#BANK_WSPR_INFO
	sta	REG_RAM_BANK

	; get offset
	lda	(TMP_PTR_L)
	sta	TMP_REND_WPN_OFFS_L
	inc	TMP_PTR_L
	lda	(TMP_PTR_L)
	sta	TMP_REND_WPN_OFFS_H
	inc	TMP_PTR_L

	; check offset
	lda	TMP_REND_WPN_OFFS_L
	cmp	G_WPN_NOW_OFFS_L
	bne	:+
	lda	TMP_REND_WPN_OFFS_H
	cmp	G_WPN_NOW_OFFS_H
	beq	@wspr_no_copy
:
	lda	TMP_REND_WPN_OFFS_L
	sta	G_WPN_NOW_OFFS_L
	lda	TMP_REND_WPN_OFFS_H
	sta	G_WPN_NOW_OFFS_H

	; force light update
	lda	#$FF
	sta	G_WPN_NOW_LIGHT

	; swap bank
	lda	G_WPN_VERA_SLOT
	eor	#1
	sta	G_WPN_VERA_SLOT
	and	#1
	tax

	; check bright count
	ldy	#1
	lda	(TMP_PTR_L),y
	pha
	beq	@wspr_no_copy

	; ADDR0 = 0x1C000+offs, increment = 1
	clc
	stz	VERA_ADDRx_L
	lda	@page_lookup,x
	adc	(TMP_PTR_L)
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; copy pixels
	clc
	lda	TMP_REND_WPN_OFFS_L
	adc	(TMP_PTR_L)
	tax
	lda	TMP_REND_WPN_OFFS_H
	adc	#0
	tay
	pla
	jsr	video_copy_normal

	; wsprite frame bank
	lda	#BANK_WSPR_INFO
	sta	REG_RAM_BANK

@wspr_no_copy:
	; check light
	lda	G_WPN_LIGHT_NEW
	cmp	G_WPN_NOW_LIGHT
	beq	@wspr_no_light
	sta	G_WPN_NOW_LIGHT

	; ADDR0 = 0x1C000, increment = 1
	lda	G_WPN_VERA_SLOT
	and	#1
	tax
	stz	VERA_ADDRx_L
	lda	@page_lookup,x
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; copy pixels
	ldx	TMP_REND_WPN_OFFS_L
	ldy	TMP_REND_WPN_OFFS_H
	lda	(TMP_PTR_L)
	beq	@wspr_no_light
	jsr	video_copy_remap

@wspr_no_light:
	rts

@page_lookup:
	.word	$E0C0

;;;
; update weapon XY offset
;;;

update_weapon_swing:
	; enable math
	jsr	math_setup_vera

	; get player thing
	ldy	G_PLAYER_THING

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; get thing pitch
	stz	MATH_COUNTER
	lda	THING_STRUCT_PITCH,y
	cmp	#148
	bcc	:+
	sec
	sbc	#148
	lsr
	lsr
	sta	MATH_COUNTER
:
	; add Y offset
	clc
	lda	MATH_COUNTER
	adc	THING_STRUCT_COUNTER
	sta	MATH_COUNTER

	; get weapon iflags
	lda	THING_STRUCT_IFLAGS
	pha

	; get thing type
	ldx	THING_STRUCT_TYPE,y

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; get thing speed
	lda	THTYPE_STRUCT_SPEED,x
	sta	MATH_VAR_L
	stz	MATH_VAR_H

	; thing structure bank
	lda	#BANK_THING_STRUCT
	sta	REG_RAM_BANK

	; abs(mx)
	lda	THING_STRUCT_MX_L,y
	bpl	:+
	eor	#$FF
	ina
:
	sta	MATH_P2A_X_L

	; abs(my)
	lda	THING_STRUCT_MY_L,y
	bpl	:+
	eor	#$FF
	ina
:
	sta	MATH_P2A_Y_L

	; dist = ox > oy ? ox + oy / 2 : oy + ox / 2
	cmp	MATH_P2A_X_L
	bcs	:+

	; ox + oy / 2
	lsr
	clc
	adc	MATH_P2A_X_L
	sta	VERA_FX_CACHE_H

	bra	:++
:
	; oy + ox / 2
	lda	MATH_P2A_X_L
	lsr
	clc
	adc	MATH_P2A_Y_L
	sta	VERA_FX_CACHE_H
:
	; inverse division bank
	lda	#BANK_IDIV_L
	sta	REG_RAM_BANK

	;; dist *= inv_div[speed]

	; inv_div[speed]
	jsr	math_idiv
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_L
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_M

	; dist
	stz	VERA_FX_CACHE_U

	; save result
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0

	; limit to 127
	bpl	:+
	lda	#127
:
	sta	MATH_VAR_U

	; attack check
	plx
	beq	:+

	; change swing
	lsr
	lsr
	sta	MATH_VAR_U
:
	; show_wpn_now.avg >>= 1
	lda	G_WPN_AVG
	lsr

	; show_wpn_now.avg += dist
	clc
	adc	MATH_VAR_U
	sta	G_WPN_AVG

	; dist = show_wpn_now.avg >> 1
	lsr
	bne	:+

	; X = -48
	lda	#$D0
	sta	MATH_VAR_L
	lda	#$FF
	sta	MATH_VAR_H

	; Y = 0
	stz	MATH_VAR_U

	rts
:
	;; ox = ((tab_sin[(level_tick << 4) & 0xFF] * dist) >> 16) - 48

	; dist << 4
	tax
	lda	TAB_SWAP,x
	and	#$F0
	sta	VERA_FX_CACHE_H
	lda	TAB_SWAP,x
	and	#$0F
	sta	VERA_FX_CACHE_U

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; level_tick << 4
	ldx	G_LEVEL_TICK_L
	lda	TAB_SWAP,x
	and	#$F0
	tax

	; tab_sin
	math_read_sin	VERA_FX_CACHE_L, VERA_FX_CACHE_M

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 16) - 48
	lda	VERA_DATA0
	sec
	lda	VERA_DATA0
	sbc	#48
	sta	MATH_VAR_L
	lda	VERA_DATA0
	sbc	#0
	sta	MATH_VAR_H

	;; oy = ((tab_cos[(level_tick << 4) & 0xFF] * dist) >> 16)

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; dist is already set

	; tab_cos
	math_read_cos	VERA_FX_CACHE_L, VERA_FX_CACHE_M

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 16)
	lda	VERA_DATA0
	lda	VERA_DATA0
	bpl	:+
	eor	#$FF
	ina
:
	sta	MATH_VAR_U

	rts

;;;
; set wall texture Y info
;;;

set_affine_yn:
	; set affine Y step
	lda	RENDER_TEXSCALE_H,x
	lsr
	sta	VERA_FX_X_INCR_H
	lda	RENDER_TEXSCALE_L,x
	ror
	sta	VERA_FX_X_INCR_L

set_affine_yhn:
	; dcsel = 5, addrsel = 0
	lda	#$0A
	sta	VERA_CTRL

	; set affine Y sub
	lda	WALL_Y_START_L
	sta	VERA_FX_X_POS_S

	; dcsel = 4, addrsel = 0
	lda	#$08
	sta	VERA_CTRL

	; set affine Y position
	lda	WALL_Y_START_M
	sta	VERA_FX_X_POS_L
	lda	WALL_Y_START_H
	sta	VERA_FX_X_POS_H

	rts

set_affine_ys:
	; set affine Y step
	lda	RENDER_TEXSCALE_H,x
	lsr
	sta	VERA_FX_Y_INCR_H
	lda	RENDER_TEXSCALE_L,x
	ror
	sta	VERA_FX_Y_INCR_L

set_affine_yhs:
	; dcsel = 5, addrsel = 0
	lda	#$0A
	sta	VERA_CTRL

	; set affine Y sub
	lda	WALL_Y_START_L
	sta	VERA_FX_Y_POS_S

	; dcsel = 4, addrsel = 0
	lda	#$08
	sta	VERA_CTRL

	; set affine Y position
	lda	WALL_Y_START_M
	sta	VERA_FX_Y_POS_L
	lda	WALL_Y_START_H
	sta	VERA_FX_Y_POS_H

	rts

;;;
; set wall texture X column
;;;

set_affine_xpn:
	ldx	VIDEO_DRAW_X
	sec
	lda	RENDER_TEXTURE_X,x
	sbc	RENDER_TEXTURE_OX
	eor	WALL_TEXTURE_XOR
	sta	VERA_FX_Y_POS_L
set_affine_sky:
	rts

set_affine_xps:
	ldx	VIDEO_DRAW_X
	sec
	lda	RENDER_TEXTURE_X,x
	sbc	RENDER_TEXTURE_OX
	eor	WALL_TEXTURE_XOR
	sta	VERA_FX_X_POS_L
	rts

set_affine_xw:
	; texture height
	lda	WALL_TEXTURE_WX
	sta	VERA_FX_Y_POS_L

	; X position
	ldx	VIDEO_DRAW_X
	sec
	lda	RENDER_TEXTURE_X,x
	sbc	RENDER_TEXTURE_OX
	eor	WALL_TEXTURE_XOR
	and	#$7F
	tay

	; get column offset
	lda	COLUMN_PTR_B
	sta	REG_RAM_BANK
	stz	COLUMN_PTR_L
	lda	(COLUMN_PTR_L),y
	sta	COLORMAP_L
	lda	#$80
	sta	COLUMN_PTR_L
	lda	(COLUMN_PTR_L),y

	; add to data
	clc
	adc	TEXTURE_PTR_L
	sta	COLORMAP_H
	lda	TEXTURE_PTR_H
	adc	#0

	; get texture pointer
	asl
	asl
	asl
	ldx	COLORMAP_H
	ora	TAB_BANK,x
	sta	COLORMAP_B
	txa
	and	#$1F
	ora	#$A0
	sta	COLORMAP_H

	rts

;;;
; generate texture X map
;;;

generate_texture_map_wall:
	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

generate_texture_map:
	; tangent bank
	lda	#BANK_TAN
	sta	REG_RAM_BANK

	; mark as 'generated'
	sta	WALL_HAVE_TEX_X

	; loop start
	lda	WALL_X1
	asl
	sta	WALL_LEN_BKUP
	lda	WALL_X0
	asl
	tax
loop_texture_map_x:
	; ang = (lca + x2angle[x0]) & 2047
	clc
	lda	WALL_LCA_L
	adc	TAB_X2A_L,x
	tay
	lda	WALL_LCA_H
	adc	TAB_X2A_H,x
	and	#$07
	ora	#>TAB_TAN_L
	sta	HI_PTR_H

	; toffs = (-1 * ld.x + ld.y * tab_tan[ang]) >> 8

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; -1
	lda	#$FF
	sta	VERA_FX_CACHE_L
	sta	VERA_FX_CACHE_M

	; ld.x
	lda	WALL_LD_X_L
	sta	VERA_FX_CACHE_H
	lda	WALL_LD_X_H
	sta	VERA_FX_CACHE_U

	; accumulate
	lda	VERA_FX_ACCUM

	; ld.y
	lda	WALL_LD_Y_L
	sta	VERA_FX_CACHE_L
	lda	WALL_LD_Y_H
	sta	VERA_FX_CACHE_M

	; tab_tan[ang]
	lda	(HI_PTR_L),y
	sta	VERA_FX_CACHE_H
	lda	#>TAB_TAN_H
	tsb	HI_PTR_H
	lda	(HI_PTR_L),y
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result (>> 8)
	lda	VERA_DATA0
	sta	RENDER_TEXTURE_X,x

	; skip odd column
opcode_texture_map:
	inx

	; next
	inx
	cpx	WALL_LEN_BKUP
	bne	loop_texture_map_x

	; done
	jmp	time_update

;;;
; texture set
;;;
; A is texture ID

texture_set:
	sty	VIDEO_TMP_0
	tay

	; dcsel = 3, addrsel = 0
	lda	#$06
	sta	VERA_CTRL

	; reset affine increment
	stz	VERA_FX_X_INCR_L
	stz	VERA_FX_X_INCR_H
	stz	VERA_FX_Y_INCR_L
	stz	VERA_FX_Y_INCR_H

	; dcsel = 5, addrsel = 0
	lda	#$0A
	sta	VERA_CTRL

	; reset affine sub
	stz	VERA_FX_X_POS_S
	stz	VERA_FX_Y_POS_S

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; default to invalid texture
	lda	#$FF
	sta	RENDER_TEXTURE_INFO_IDX

	; default to 'plane' type
	lda	#<set_affine_xpn
	sta	wall_affine_opcode_A+1
	sta	wall_affine_opcode_B+1
	lda	#>set_affine_xpn
	sta	wall_affine_opcode_A+2
	sta	wall_affine_opcode_B+2
	stz	COLORMAP_L

	; default to textured drawing
	lda	#<video_draw_vline
	sta	wall_vline_opcode_A+1
	sta	wall_vline_opcode_B+1
	lda	#>video_draw_vline
	sta	wall_vline_opcode_A+2
	sta	wall_vline_opcode_B+2

	; default to 'no XY swap'
	lda	#<set_affine_yn
	sta	wall_affine_opcode_C+1
	lda	#>set_affine_yn
	sta	wall_affine_opcode_C+2
	lda	#<set_affine_yhn
	sta	wall_affine_opcode_D+1
	lda	#>set_affine_yhn
	sta	wall_affine_opcode_D+2

	; check for normal texture
	tya
	bpl	:+

	; check for special texture
	ina
	bne	@is_invalid

	;; it is sky texture

	; set sky mapping
	lda	#<set_affine_sky
	sta	wall_affine_opcode_A+1
	sta	wall_affine_opcode_B+1
	lda	#>set_affine_sky
	sta	wall_affine_opcode_A+2
	sta	wall_affine_opcode_B+2

	; set sky drawing
	lda	#<video_draw_sline
	sta	wall_vline_opcode_A+1
	sta	wall_vline_opcode_B+1
	lda	#>video_draw_sline
	sta	wall_vline_opcode_A+2
	sta	wall_vline_opcode_B+2

	jmp	@done
:
	; get texture from remap table
	lda	TAB_TEXTURE_REMAP,y
	sta	RENDER_TEXTURE_INFO_IDX

	; check for invalid texture
	bpl	:+

	; it is invalid texture
@is_invalid:
	; vera FX
	lda	#$FC
	sta	VERA_FX_TILEBASE
	sta	VERA_FX_MAPBASE

	; colormap pointer
	lda	#>TAB_LIGHTMAPS
	sta	COLORMAP_H

	; math XOR
	lda	#$FF
	sta	WALL_TEXTURE_XOR

	; NOTE: mirror flag is skipped
	jmp	@done
:
	tay

	; backup current bank
	lda	REG_RAM_BANK
	sta	VIDEO_TMP_1

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; check for animation
	lda	TEXINFO_STRUCT_EFFECT_0,y
	and	#4
	beq	:+

	; get ticks
	lda	G_LEVEL_TICK_L
	sta	RENDER_TEXTURE_ETIME_L
	lda	G_LEVEL_TICK_H
	sta	RENDER_TEXTURE_ETIME_H

	; scale ticks
	lda	TEXINFO_STRUCT_EFFECT_1,y
@loop_ts:
	lsr	RENDER_TEXTURE_ETIME_H
	ror	RENDER_TEXTURE_ETIME_L
	dea
	bne	@loop_ts

	; calculate animation
	clc
	lda	RENDER_TEXTURE_ETIME_L
	adc	TEXINFO_STRUCT_EFFECT_3,y
	and	TEXINFO_STRUCT_EFFECT_2,y
	sta	RENDER_TEXTURE_ETIME_L
	sec
	tya
	sbc	TEXINFO_STRUCT_EFFECT_3,y
	clc
	adc	RENDER_TEXTURE_ETIME_L
	tay
:
	; type check
	lda	TEXINFO_STRUCT_TYPE,y
	bpl	@set_wall

	; check colormap type
	ror
	bcc	:+

	; get lightmap (8bpp)
	lda	PROJECTION_LIGHT
	asl
	asl
	clc
	adc	#>TAB_LIGHTMAPS
	sta	COLORMAP_H

	bra	:++
:
	; get colormap index (4 bpp)
	lda	#>TEXINFO_STRUCT_P_COLORMAP
	ora	PROJECTION_LIGHT
	sta	COLORMAP_H
	lda	(COLORMAP_L),y
	tax
	lda	#$80
	sta	COLORMAP_L
	lda	(COLORMAP_L),y

	; colormap pointer
	asl
	asl
	asl
	ora	TAB_BANK,x
	sta	COLORMAP_B
	txa
	and	#$1F
	ora	#$A0
	sta	COLORMAP_H
	stz	COLORMAP_L
:
	; vera FX
	lda	TEXINFO_STRUCT_P_TILE_DATA,y
	sta	VERA_FX_TILEBASE
	lda	TEXINFO_STRUCT_P_TILE_MAP,y
	sta	VERA_FX_MAPBASE

	; check for XY swap
	lda	WALL_TEXTURE_FLAGS
	and	#WALL__TEXFLAG_MIRROR_Y_SWAP_XY
	beq	:+

	; set to 'swapped plane' type
	lda	#<set_affine_xps
	sta	wall_affine_opcode_A+1
	sta	wall_affine_opcode_B+1
	lda	#>set_affine_xps
	sta	wall_affine_opcode_A+2
	sta	wall_affine_opcode_B+2
	stz	COLORMAP_L

	; set 'XY swap'
	lda	#<set_affine_ys
	sta	wall_affine_opcode_C+1
	lda	#>set_affine_ys
	sta	wall_affine_opcode_C+2
	lda	#<set_affine_yhs
	sta	wall_affine_opcode_D+1
	lda	#>set_affine_yhs
	sta	wall_affine_opcode_D+2

:
@flags:
	; math XOR
	lda	#$FF
	sta	WALL_TEXTURE_XOR

	; mirror flag
	lda	WALL_TEXTURE_FLAGS
	and	#WALL__TEXFLAG_MIRROR_X
	beq	:+
	stz	WALL_TEXTURE_XOR
:
	; restore correct bank
	lda	VIDEO_TMP_1
	sta	REG_RAM_BANK

@done:
	ldy	VIDEO_TMP_0
	rts

@set_wall:
	; lookup X (height)
	sta	WALL_TEXTURE_WX

	; check for Y mirror
	lda	WALL_TEXTURE_FLAGS
	and	#WALL__TEXFLAG_MIRROR_Y_SWAP_XY
	beq	:+
	inc	WALL_TEXTURE_WX
:
	; vera FX
	lda	#$F8
	sta	VERA_FX_TILEBASE
	lda	#$FE
	sta	VERA_FX_MAPBASE

	; get column pointer
	lda	TEXINFO_STRUCT_W_COLS_H,y
	asl
	asl
	asl
	ldx	TEXINFO_STRUCT_W_COLS_L,y
	ora	TAB_BANK,x
	sta	COLUMN_PTR_B
	txa
	and	#$1F
	ora	#$A0
	sta	COLUMN_PTR_H

	; get light index
	lda	#>TEXINFO_STRUCT_W_DATA_LH
	ora	PROJECTION_LIGHT
	sta	TMP_PTR_H
	stz	TMP_PTR_L

	; get data WRAM pointer
	lda	(TMP_PTR_L),y
	sta	TEXTURE_PTR_L
	lda	#$80
	sta	TMP_PTR_L
	lda	(TMP_PTR_L),y
	sta	TEXTURE_PTR_H

	; set to 'wall' type
	lda	#<set_affine_xw
	sta	wall_affine_opcode_A+1
	sta	wall_affine_opcode_B+1
	lda	#>set_affine_xw
	sta	wall_affine_opcode_A+2
	sta	wall_affine_opcode_B+2

	bra	@flags

