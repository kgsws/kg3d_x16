.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "tables.inc"
.include "main.inc"
.include "file.inc"
.include "video.inc"

.export math_setup_vera
.export math_scale_down
.export math_shift_down
.export	math_idiv
.export	math_idiv_ns
.export	math_atan
.export math_p2a

.segment "CODE"

;;;
; VERA math setup
;;;

math_setup_vera:
	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; enable cache write
	lda	#%01000000
	sta	VERA_FX_CTRL

	; reset accumulator, set addition, enable multiplication
	lda	#%10010000
	sta	VERA_FX_MULT

	; dcsel = 6, addrsel = 0
	lda	#$0C
	sta	VERA_CTRL

	; ADDR0 = 0x001F00, increment = 1
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	rts

;;;
; scale down math_var to be < 256
;;;
; does not support negative values
; 16 bit only
; uses X

math_scale_down:
	ldx	#0
@loop:
	lda	MATH_VAR_H
	bne	:+
	; done
	stx	MATH_COUNTER
	rts
:
	; halve the value
	lsr	MATH_VAR_H
	ror	MATH_VAR_L

	; increment counter
	inx

	bra	@loop

;;;
; shift right math_var using counter
;;;
; does not support negative values
; 24 bit only
; uses X

math_shift_down:
	ldx	MATH_COUNTER
	bne	@loop
	rts
@loop:
	; shift right
	lsr	MATH_VAR_U
	ror	MATH_VAR_H
	ror	MATH_VAR_L

	; decrement counter
	dex
	bne	@loop

	rts

;;;
; division using inverse multiplication
;;;
; set REG_RAM_BANK to #BANK_IDIV_L before use
; uses Y register

math_idiv:
	; save sign
	lda	MATH_VAR_H
	sta	MATH_VAR_SIGN
math_idiv_ns:
	; sign check
	bpl	@positive
	; negation
	sec
	lda	#0
	sbc	MATH_VAR_L
	sta	MATH_VAR_L
	lda	#0
	sbc	MATH_VAR_H
	sta	MATH_VAR_H
@positive:
	; if value < 8192
	cmp	#$20
	bcc	@lookup_l
	; clear H
	stz	MATH_VAR_H
	; if value < 11008
	ldy	#3
	sty	MATH_VAR_L
	cmp	#$2B
	bcc	@continue
	; if value < 16384
	ldy	#2
	sty	MATH_VAR_L
	cmp	#$40
	bcc	@continue
	; lowest possible
	ldy	#1
	sty	MATH_VAR_L
	bra	@continue
@lookup_l:
	; modify address
	ora	#>TAB_IDIV_L
	sta	@opcode+2
	ldy	MATH_VAR_L
	; table lookup L
@opcode:
	lda	$A000,y	; modified address
	sta	MATH_VAR_L
	; load and clear H
	lda	MATH_VAR_H
	stz	MATH_VAR_H
	; if value >= 256
	bne	@continue
	; table lookup H
	lda	TAB_IDIV_H,y
	sta	MATH_VAR_H
@continue:
	; sign check
	lda	MATH_VAR_SIGN
	bpl	@skip
	; negation
	sec
	lda	#0
	sbc	MATH_VAR_L
	sta	MATH_VAR_L
	lda	#0
	sbc	MATH_VAR_H
	sta	MATH_VAR_H
@skip:
	; done
	rts

;;;
; arc tan
;;;
; set REG_RAM_BANK to #BANK_ATAN before use
; uses Y register

math_atan:
	; change lookup address
	lda	MATH_VAR_H
	; modify opcode (lo)
	ora	#>TAB_ATAN_L
	sta	@op0+2
	; modify opcode (hi)
	ora	#>TAB_ATAN_H
	sta	@op1+2
	; do lookup
	ldy	MATH_VAR_L
@op0:
	lda	$A000,y	; modified address
	sta	MATH_VAR_L
@op1:
	lda	$B000,y	; modified address
	sta	MATH_VAR_H
	; done
	rts

;;;
; point to angle
;;;
; changes RAM bank
; uses X and Y register
; assuming multiplication enabled
; assuming cache write enabled
; assuming dcsel = 6, addrsel = 0
; assuming ADDR0 = 0x001F00, increment = 1
; result is in MATH_VAR_*

math_p2a:
	; prepare octant
	ldx	#%00000000

	; if(p2a_coord.x < 0)
	lda	MATH_P2A_X_H
	bpl	@skip_xneg
	; p2a_coord.x = -p2a_coord.x
	sec
	lda	#0
	sbc	MATH_P2A_X_L
	sta	MATH_P2A_X_L
	lda	#0
	sbc	MATH_P2A_X_H
	sta	MATH_P2A_X_H
	; change quadrant
	ldx	#%00000010

@skip_xneg:
	; if(p2a_coord.y < 0)
	lda	MATH_P2A_Y_H
	bpl	@skip_yneg
	; p2a_coord.y = -p2a_coord.y
	sec
	lda	#0
	sbc	MATH_P2A_Y_L
	sta	MATH_P2A_Y_L
	lda	#0
	sbc	MATH_P2A_Y_H
	sta	MATH_P2A_Y_H
	; change quadrant
	txa
	ora	#%00000100
	tax

@skip_yneg:
	; use invdiv bank
	lda	#BANK_IDIV_L
	sta	REG_RAM_BANK

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; if(p2a_coord.x > p2a_coord.y)
	sec
	lda	MATH_P2A_Y_L
	sbc	MATH_P2A_X_L
	lda	MATH_P2A_Y_H
	sbc	MATH_P2A_X_H
	bpl	@x_is_less

	; change octant bit
	txa
	ora	#%00001000
	tax

	;; res = p2a_coord.y * inv_div[p2a_coord.x]

	; inv_div[p2a_coord.x]
	lda	MATH_P2A_X_L
	sta	MATH_VAR_L
	lda	MATH_P2A_X_H
	sta	MATH_VAR_H
	jsr	math_idiv

	; p2a_coord.y
	lda	MATH_P2A_Y_L
	sta	VERA_FX_CACHE_L
	lda	MATH_P2A_Y_H
	sta	VERA_FX_CACHE_M

	; inv_div
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_H
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	bra	@have_result

@x_is_less:
	;; res = p2a_coord.x * inv_div[p2a_coord.y]

	; inv_div[p2a_coord.y]
	lda	MATH_P2A_Y_L
	sta	MATH_VAR_L
	lda	MATH_P2A_Y_H
	sta	MATH_VAR_H
	jsr	math_idiv

	; p2a_coord.x
	lda	MATH_P2A_X_L
	sta	VERA_FX_CACHE_L
	lda	MATH_P2A_X_H
	sta	VERA_FX_CACHE_M

	; inv_div
	lda	MATH_VAR_L
	sta	VERA_FX_CACHE_H
	lda	MATH_VAR_H
	sta	VERA_FX_CACHE_U

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

@have_result:
	; read result
	stz	VERA_ADDRx_L
	lda	VERA_DATA0
	sta	MATH_VAR_L
	lda	VERA_DATA0
	sta	MATH_VAR_H
	lda	VERA_DATA0

	; res >>= 3
	lsr
	sta	MATH_TEMP
	ror	MATH_VAR_H
	ror	MATH_VAR_L
	lsr	MATH_TEMP
	ror	MATH_VAR_H
	ror	MATH_VAR_L
	lsr	MATH_TEMP
	ror	MATH_VAR_H
	ror	MATH_VAR_L

	; use atan bank
	lda	#BANK_ATAN
	sta	REG_RAM_BANK

	; res = atan_tab[res]
	jsr	math_atan

	; pick octant
	jmp	(@p2a_octant,x)

@p2a_oct0:
	; return res
	rts

@p2a_oct1:
	; return 1024 - res
	sec
	lda	#$00
	sbc	MATH_VAR_L
	sta	MATH_VAR_L
	lda	#$04
	sbc	MATH_VAR_H
	sta	MATH_VAR_H
	rts

@p2a_oct2:
	; return res + 1024
	clc
	lda	#$04
	adc	MATH_VAR_H
	sta	MATH_VAR_H
	rts

@p2a_oct3:
	; return 2048 - res
	sec
	lda	#$00
	sbc	MATH_VAR_L
	sta	MATH_VAR_L
	lda	#$08
	sbc	MATH_VAR_H
	sta	MATH_VAR_H
	rts

@p2a_oct4:
	; return 2048 + res
	clc
	lda	#$08
	adc	MATH_VAR_H
	sta	MATH_VAR_H
	rts

@p2a_oct5:
	; return 3072 - res
	sec
	lda	#$00
	sbc	MATH_VAR_L
	sta	MATH_VAR_L
	lda	#$0C
	sbc	MATH_VAR_H
	sta	MATH_VAR_H
	rts

@p2a_oct6:
	; return 3072 + res
	clc
	lda	#$0C
	adc	MATH_VAR_H
	sta	MATH_VAR_H
	rts

@p2a_oct7:
	; return (0 - res) & 4095
	sec
	lda	#$00
	sbc	MATH_VAR_L
	sta	MATH_VAR_L
	lda	#$00
	sbc	MATH_VAR_H
	and	#$0F
	sta	MATH_VAR_H
	rts

@p2a_octant:
	.word	@p2a_oct0
	.word	@p2a_oct7
	.word	@p2a_oct3
	.word	@p2a_oct4
	.word	@p2a_oct1
	.word	@p2a_oct6
	.word	@p2a_oct2
	.word	@p2a_oct5

