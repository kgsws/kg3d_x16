.include "../kernal.inc"
.include "../vera.inc"
.include "banks.inc"
.include "tables.inc"
.include "main.inc"
.include "math.inc"
.include "zeropage.inc"
.include "engine.inc"
.include "things.inc"

.export tick_clear
.export tick_run
.export tick_add
.export tick_del

;;;
; CODE
;;;

.segment "CODE"

;;;
; clear all tickers
;;;

tick_clear:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	lda	#$FF
	ldx	#1
@loop:
	sta	TICKER_STRUCT_FUNC,x
	stz	TICKER_STRUCT_NEXT,x
	inx
	bpl	@loop

	stz	G_TICKER_TOP
	stz	G_TICKER_CUR

	; things
	jsr	thing_clear

	rts

;;;
; execute all tickers
;;;

tick_run:
	lda	G_TICKER_TOP
	sta	G_TICKER_IDX
@loop:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; current ticker
	ldy	G_TICKER_IDX

	; next
	lda	TICKER_STRUCT_NEXT,y
	pha

	; run function
	ldx	TICKER_STRUCT_FUNC,y
	jsr	@ticker_func

	; next check
	pla
	bne	:+

	; done
@dummy:
	rts
:
	; next
	sta	G_TICKER_IDX
	bra	@loop

@ticker_func:
	jmp	(@ticker_list,x)

@ticker_list:
	.word	@dummy
	.word	thing_tick
	.word	thing_plyr

;;;
; create ticker
;;;
; zero flag set = fail
; returns ticker in X

tick_add:
	; ticker structure bank
	lda	#BANK_TICKER_STRUCT
	sta	REG_RAM_BANK

	; find free slot
	ldx	#1
@loop:
	lda	TICKER_STRUCT_FUNC,x
	bmi	@found
	inx
	bpl	@loop

	; not found
	ldx	#0
	rts
@found:
	ldy	G_TICKER_CUR

	; ticker[i].base.func = TFUNC_DUMMY
	stz	TICKER_STRUCT_FUNC,x

	; ticker[i].base.prev = cur
	tya
	sta	TICKER_STRUCT_PREV,x

	; ticker[cur].base.next = i
	txa
	sta	TICKER_STRUCT_NEXT,y

	; cur = i
	stx	G_TICKER_CUR

	; if(!top)
	lda	G_TICKER_TOP
	bne	:+

	; top = i
	stx	G_TICKER_TOP
:
	txa
	rts

;;;
; remove ticker
;;;

tick_del:
	rts

