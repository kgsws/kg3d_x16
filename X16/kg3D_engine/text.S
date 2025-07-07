.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "main.inc"
.include "math.inc"
.include "engine.inc"
.include "tables.inc"

.export text_write

;;;
; CODE
;;;

.segment "CODE_EXTRA"

;;;
; write text using sprites
;;;

text_write:
	phy
	phx

	lda	TAB_TEXT_I
	sta	DBG_BYTE0

	ldy	#0
@loop:
	lda	(FUNC_ARG_L),y
	beq	@stop
	iny

	sta	DBG_BYTE1
	inc	TAB_TEXT_I

	bra	@loop

@stop:
	plx
	ply
	rts
