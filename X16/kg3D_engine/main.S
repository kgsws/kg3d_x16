.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "banks.inc"
.include "tables.inc"
.include "zeropage.inc"
.include "engine.inc"
.include "math.inc"
.include "map.inc"
.include "video.inc"
.include "render.inc"
.include "things.inc"
.include "input.inc"
.include "sound.inc"

.export clear_bank
.export time_update
.export func_jsr
.export load_mapfs
.export load_render
.export rng_get

.segment "CODE"

start:
	; RNG
	lda	#>TAB_RANDOM
	sta	G_RNG_H
	stz	G_RNG_L

	; init
	jsr	map_init
	jsr	input_init
	jsr	video_init

	; set video mode
	jsr	video_setup

	;; run engine

	jsr	math_setup_vera

	; clear VSYNC flag
	lda	#1
	sta	VERA_ISR
	stz	VERA_IEN

	; turn off LED
	lda	#0
	ldx	#SMC_I2C_ADDR
	ldy	#SMC_CMD_SET_LED
	jsr	k_i2c_write_byte

	; clear current input
	jsr	input_clear

	;; DEBUG: load map

	; code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	; load map
	jsr	map_load

	;;;;
	;; GAME LOOP
	;;;;

	jsr	load_render

@game_loop:
	jsr	render_view
	jsr	video_swap

	; wait for at least one new tick
@wait_loop:
	jsr	time_update
	lda	G_NEW_TICS
	beq	@wait_loop
	sta	TICK_COUNT
	stz	G_NEW_TICS

	; enable math
	jsr	math_setup_vera

	;; INPUT
	jsr	input_process

	;; TICKers

	; run tickers
@tick_loop:
	; level time
	inc	G_LEVEL_TICK_L
	bne	:+
	inc	G_LEVEL_TICK_M
	bne	:+
	inc	G_LEVEL_TICK_H
:
	; things
	jsr	thing_tick

	; next tick
	dec	TICK_COUNT
	bne	@tick_loop

	; debugger framerate
;	dbg_framerate

	;;
	bra @game_loop

;;;
; clear bank
;;;

clear_bank:
	lda	#$A0
	sta	@loop+2
	lda	#0
	ldy	#0
	ldx	#32
@loop:
	sta	$A000,y	; modified address
	dey
	bne	@loop
	inc	@loop+2
	dex
	bne	@loop

	rts

;;;
; JSR to FUNC_ARG_* pointer
;;;

func_jsr:
	jmp	(FUNC_ARG_L)

;;;
; time ticker
;;;

time_update:
	; TEST
	ldx	#$FF
	ldy	#$FF

	; check VSYNC flag
	lda	VERA_ISR
	lsr
	bcs	:+
	rts
:
	; clear IRQ flag
	lda	#1
	sta	VERA_ISR

	; 15 Hz check
	dec	G_TICKER
	bne	:+
	lda	#4
	sta	G_TICKER

	; increment ticks
	inc	G_NEW_TICS
:
	; update sound
	jmp	sound_update

;;;
; load different parts of code
;;;

load_mapfs:
	lda	#BANK_SWAP_CODE
	sta	REG_RAM_BANK

	lda	#>LDR_CODE_MAPFS_ADDR
	sta	copy_loop+2
	ldy	#>LDR_CODE_MAPFS_SIZE

	jmp	copy_code

load_render:
	lda	#BANK_SWAP_CODE
	sta	REG_RAM_BANK

	lda	#>LDR_CODE_RENDER_ADDR
	sta	copy_loop+2
	ldy	#>LDR_CODE_RENDER_SIZE

	jsr	copy_code

	; init rendering
	jsr	render_init

	rts

copy_code:
	lda	#>LDR_CODE_DEST
	sta	copy_loop+5
	ldx	#0
copy_loop:
	lda	$CC00,x	; modified address
	sta	$CC00,x	; modified address
	dex
	bne	copy_loop
	inc	copy_loop+2
	lda	#$C0
	cmp	copy_loop+2
	bne	:+
	lda	#$A0
	sta	copy_loop+2
	inc	REG_RAM_BANK
:
	inc	copy_loop+5
	dey
	bne	copy_loop

	rts

;;;
; return RNG byte and advance pointer
;;;
; result is in MATH_TEMP

rng_get:
	lda	REG_RAM_BANK
	pha

	lda	#BANK_RANDOM
	sta	REG_RAM_BANK

	lda	(G_RNG_L)
	sta	MATH_TEMP

	inc	G_RNG_L
	bne	:+

	inc	G_RNG_H
	lda	G_RNG_H
	cmp	#>TAB_RNG_END
	bcc	:+

	lda	#>TAB_RANDOM
	sta	G_RNG_H
:
	pla
	sta	REG_RAM_BANK

	rts

