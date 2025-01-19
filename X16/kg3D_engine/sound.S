.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "file.inc"

.export sound_stop
.export sound_start
.export sound_update

;;;
; stuff
;;;

SNDBNK_A = REG_0_L
SNDBNK_B = REG_0_H

;;;
; CODE
;;;

.segment "CODE"

;;;
; stop and disable sound
;;;

sound_stop:
	rts

;;;
; reset and enable sound
;;;

sound_start:
	rts

;;;
; update sound
;;;

sound_update:
	; backup
	lda	VERA_CTRL
	pha

	; dcsel = 2, addrsel = 1
	lda	#$04
	sta	VERA_CTRL

	; backup
	lda	VERA_FX_CTRL
	pha

	; disable cache write
	lda	#%00000000
	sta	VERA_FX_CTRL

	; ADDR1 = 0x01F9C0, increment = 1
	lda	#$C0
	sta	VERA_ADDRx_L
	lda	#$F9
	sta	VERA_ADDRx_M
	lda	#%00010001
	sta	VERA_ADDRx_H

	; STUFF

	; restore
	stz	VERA_ADDRx_L
	lda	#$1F
	sta	VERA_ADDRx_M
	lda	#%00010000
	sta	VERA_ADDRx_H

	; restore
	pla
	sta	VERA_FX_CTRL

	; restore
	pla
	sta	VERA_CTRL

	rts

