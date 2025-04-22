.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "tables.inc"
.include "math.inc"
.include "zeropage.inc"
.include "engine.inc"
.include "things.inc"

.export hitscan

;;;
; CODE
;;;

.segment "CODE"


;;;
; get vetex angle
;;;
; returns angle in A

get_angle:
	; p2a_coord.x = v1->x - x
	sec
	ldy	#WALL_STRUCT_VTX_X
	lda	(WALN_PTR_L),y
	sbc	ARG_HITSCAN_X_L
	sta	MATH_P2A_X_L
	iny
	lda	(WALN_PTR_L),y
	sbc	ARG_HITSCAN_X_H
	sta	MATH_P2A_X_H

	; p2a_coord.y = v1->y - y
	sec
	iny
	lda	(WALN_PTR_L),y
	sbc	ARG_HITSCAN_Y_L
	sta	MATH_P2A_Y_L
	iny
	lda	(WALN_PTR_L),y
	sbc	ARG_HITSCAN_Y_H
	sta	MATH_P2A_Y_H

	; angle
	jsr	math_p2a
	math_lsr4_u16	MATH_VAR_L, MATH_VAR_H

	rts

;;;
; run 2D hitscan
;;;
; ARG_THING_SPAWN_SECTOR is origin sector
; ARG_HITSCAN_* is position
; X is callback LO
; Y is callback HI

hitscan:
	; TODO
	stx	hs_cb_opcode+1
	sty	hs_cb_opcode+2

	; first sector
	lda	ARG_THING_SPAWN_SECTOR
	sta	HITSCAN_SDX

hs_sector_loop:
	; current sector
	ldx	HITSCAN_SDX

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
	bpl	hs_cb_opcode

	; last_angle = angle
	lda	HITSCAN_NEW_ANGLE
	sta	HITSCAN_LAST_ANGLE

	; next
	bra	@loop_wall

hs_cb_opcode:
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
	sta	HITSCAN_SDX
	jmp	hs_sector_loop

