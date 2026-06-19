.include "../kernal.inc"
.include "../vera.inc"
.include "../smc.inc"
.include "banks.inc"
.include "zeropage.inc"
.include "main.inc"
.include "math.inc"
.include "tables.inc"
.include "video.inc"
.include "engine.inc"
.include "things.inc"
.include "render.inc"
.include "input.inc"
.include "tick.inc"
.include "file.inc"
.include "sound.inc"

.export map_init
.export map_load

;;;
; settings
;;;

MAP_FILE_HANDLE = 2

; structure temp storage
MAP_TEMP_DATA_STRUCT = TAB_TEMP_DATA + 512
MAP_TEMP_STACK = TAB_TEMP_DATA + 512 + 64 + 32

; map light in temp storage
MAP_TEMP_LIGHT = MAP_TEMP_DATA_STRUCT

; map texture in temp storage
MAP_TEMP_TEX = MAP_TEMP_DATA_STRUCT

; map thing in temp storage
MAP_TEMP_THING = MAP_TEMP_DATA_STRUCT
MAP_TEMP_THING_HASH_0 = MAP_TEMP_THING + 0
MAP_TEMP_THING_HASH_1 = MAP_TEMP_THING + 1
MAP_TEMP_THING_HASH_2 = MAP_TEMP_THING + 2
MAP_TEMP_THING_HASH_3 = MAP_TEMP_THING + 3
MAP_TEMP_THING_X_L = MAP_TEMP_THING + 4
MAP_TEMP_THING_X_H = MAP_TEMP_THING + 5
MAP_TEMP_THING_Y_L = MAP_TEMP_THING + 6
MAP_TEMP_THING_Y_H = MAP_TEMP_THING + 7
MAP_TEMP_THING_Z_L = MAP_TEMP_THING + 8
MAP_TEMP_THING_Z_H = MAP_TEMP_THING + 9
MAP_TEMP_THING_SECTOR = MAP_TEMP_THING + 10
MAP_TEMP_THING_ANGLE = MAP_TEMP_THING + 11
MAP_TEMP_THING_FLAGS = MAP_TEMP_THING + 12
MAP_TEMP_THING_EXTRA = MAP_TEMP_THING + 13

;;;
; info
;;;

MAP_HEAD__VERSION = 31
MAP_HEAD__SIZE = 32
MAP_LIGHT__SIZE = 4
MAP_TEXTURE__SIZE = 14
MAP_PLAYER_START__SIZE = 10
MAP_THING__SIZE = 14

;;;
; stuff
;;;

MAP_TEMP_TH_TYPE = TICK_CMD_BITS_H
MAP_TEMP_COUNT_THING = TICK_CMD_ANGLE
MAP_TEMP_COUNT_EBLOCKS = G_LEVEL_TICK_M
MAP_TEMP_COUNT_LIGHTS = TICK_CMD_PITCH
MAP_TEMP_START_COUNT = G_TICKER_IDX
MAP_LOAD_WRAM_L = FUNC_ARG_L
MAP_LOAD_WRAM_H = FUNC_ARG_H
MAP_LOAD_WTST_L = COLORMAP_L
MAP_LOAD_WTST_H = COLORMAP_H
MAP_LOAD_WRET_L = LIGHTMAP_L
MAP_LOAD_WRET_H = LIGHTMAP_H

;;;
; CODE
;;;

.segment "CODE"

;;;
; load map
;;;

map_load:
	jsr	load_mapfs
	jmp	ex_map_load

;;;
; map init
;;;

map_init:
	jsr	load_mapfs

	; precache stuff
	jmp	ex_precache

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.segment "CODE_MAP_FS"

;;;
; precache things
;;;

ex_precache:
	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; get thing sprite count
	lda	THSTATE_STRUCT_THG_SPRLNK
	sta	TAB_THG_SPRLNK

	; get logo sprite
	lda	THSTATE_STRUCT_MENU_LOGO
	sta	TAB_MENU_LOGO

	; get player types
	lda	THSTATE_STRUCT_PLAYER_C
	sta	TAB_PLAYER_C
	lda	THSTATE_STRUCT_PLAYER_S
	sta	TAB_PLAYER_S
	lda	THSTATE_STRUCT_PLAYER_F
	sta	TAB_PLAYER_F

	; reset memory
	lda	#$C0
	sta	MAP_LOAD_WRAM_L
	lda	#$04
	sta	MAP_LOAD_WRAM_H

	;;;;;;;;; TODO

	; save precached values
	lda	MAP_LOAD_WRAM_L
	sta	TAB_PRELOAD_WRAM+0
	lda	MAP_LOAD_WRAM_H
	sta	TAB_PRELOAD_WRAM+1

	; show WRAM usage (preload)
;	lda	MAP_LOAD_WRAM_L
;	sta	DBG_BYTE0
;	lda	MAP_LOAD_WRAM_H
;	sta	DBG_BYTE1

	rts

;;;
; load map
;;;

ex_map_load:
	; disable sound
	jsr	sound_start

	; backup stack pointer
	tsx
	stx	TAB_TEMP_STACK

	; cleanup
	stz	COLORMAP_L
	stz	TMP_PTR_L

	; use precached values
	lda	TAB_PRELOAD_WRAM+0
	sta	MAP_LOAD_WRAM_L
	lda	TAB_PRELOAD_WRAM+1
	sta	MAP_LOAD_WRAM_H

	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; disable VERA FX
	stz	VERA_FX_CTRL

	; open MAP
	lda	#MAP_FILE_HANDLE
	sta	TAB_FILE_HANDLE
	lda	#(filename_map_end-filename_map)
	ldx	#<filename_map
	ldy	#>filename_map
	jsr	file_open_rd

	; load map header
	lda	#MAP_HEAD__SIZE
	ldx	#<MAP_HEAD_MAGIC
	ldy	#>MAP_HEAD_MAGIC
	jsr	file_read_8_wram
	bcc	:+

	; error
	lda	#0
	jmp	load_error

@verify_error:
	lda	#1
	jmp	load_error
:
	;;;;
	; verification

	; verify version
	lda	MAP_HEAD_VERSION
	cmp	#MAP_HEAD__VERSION
	bne	@verify_error

	; verify header
	ldx	#8
@loop_magic:
	lda	MAP_HEAD_MAGIC-1,x
	cmp	map_magic-1,x
	bne	@verify_error
	dex
	bne	@loop_magic

	; check light count
	lda	MAP_HEAD_COUNT_LIGHTS
	cmp	#8
	bcs	@verify_error
	ina
	sta	MAP_TEMP_COUNT_LIGHTS

	; check texture count
	lda	MAP_HEAD_COUNT_TEXTURES
	beq	@verify_error
	cmp	#254
	bcs	@verify_error

	; check starts count
	sec	; +1
	lda	MAP_HEAD_COUNT_STARTS_NORM
	adc	MAP_HEAD_COUNT_STARTS_COOP
	bcs	@verify_error
	adc	MAP_HEAD_COUNT_STARTS_DM
	bcs	@verify_error
	beq	@verify_error
	sta	MAP_TEMP_START_COUNT

	; check wall bank count
	lda	MAP_HEAD_COUNT_WBANKS
	beq	@verify_error
	cmp	#17
	bcs	@verify_error

	; copy extra storage size
	lda	MAP_HEAD_COUNT_EBLOCKS
	sta	MAP_TEMP_COUNT_EBLOCKS

	; check thing count
	lda	MAP_HEAD_COUNT_THINGS
	beq	@verify_error
	sta	MAP_TEMP_COUNT_THING

	;;;;
	; map loading

	; check logo flag
	lda	MAP_HEAD_FLAGS
	bpl	:+

	; load and show map logo
	jsr	video_image_from_file
	bcc	:+

	; error
	lda	#2
	jmp	load_error
:
	;;;;
	; sky

	; check sky texture
	lda	MAP_HEAD_HASH_SKY+0
	ora	MAP_HEAD_HASH_SKY+1
	ora	MAP_HEAD_HASH_SKY+2
	ora	MAP_HEAD_HASH_SKY+3
	beq	@after_sky

	; GFX info bank
	lda	#BAMK_GFX_INFO
	sta	REG_RAM_BANK

	; find sky
	ldy	#31
@loop_skyfind:
	lda	TAB_GINFO_SKY_HASH_L,y
	cmp	MAP_HEAD_HASH_SKY+0
	bne	:+
	lda	TAB_GINFO_SKY_HASH_M,y
	cmp	MAP_HEAD_HASH_SKY+1
	bne	:+
	lda	TAB_GINFO_SKY_HASH_H,y
	cmp	MAP_HEAD_HASH_SKY+2
	bne	:+
	lda	TAB_GINFO_SKY_HASH_U,y
	cmp	MAP_HEAD_HASH_SKY+3
	bne	:+

	; source sector
	clc
	lda	TAB_GINFO_SKY_SECT_L,y
	adc	TAB_GFX_SECTOR_L
	sta	SD_SEC_L
	lda	TAB_GINFO_SKY_SECT_M,y
	adc	TAB_GFX_SECTOR_M
	sta	SD_SEC_M
	lda	TAB_GINFO_SKY_SECT_H,y
	adc	TAB_GFX_SECTOR_H
	sta	SD_SEC_H
	lda	TAB_GINFO_SKY_SECT_U,y
	adc	TAB_GFX_SECTOR_U
	sta	SD_SEC_U

	; WRAM pointer
	lda	MAP_LOAD_WRAM_L
	sta	TAB_SKY_BASE+0
	lda	MAP_LOAD_WRAM_H
	sta	TAB_SKY_BASE+1

	; WRAM space
	lda	#0
	jsr	get_wram

	; get destination pointer
	lda	TAB_SKY_BASE+1
	asl
	asl
	asl
	ldx	TAB_SKY_BASE+0
	ora	TAB_DIV32,x
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0

	; read data
	ldx	#$80
	jsr	file_read_sectors

	; done
	bra	@after_sky
:
	; next
	dey
	bpl	@loop_skyfind

	;;; TODO: load sky texture

@after_sky:

	;;;;
	; lights

	; generate white light
	lda	#0
@loop_white:
	tay
	sta	TAB_LIGHTMAPS,y
	dea
	bne	@loop_white

	; load other lights
	lda	MAP_HEAD_COUNT_LIGHTS
	beq	@no_lights
@loop_lights:
	; load info
	lda	#MAP_LIGHT__SIZE
	ldx	#<MAP_TEMP_LIGHT
	ldy	#>MAP_TEMP_LIGHT
	jsr	file_read_8_wram
	bcc	:+

	; error
	lda	#3
	jmp	load_error
:
	; DEBUG
	lda	MAP_TEMP_LIGHT+0
;	sta	DBG_BYTE0
	lda	MAP_TEMP_LIGHT+1
;	sta	DBG_BYTE0
	lda	MAP_TEMP_LIGHT+2
;	sta	DBG_BYTE0
	lda	MAP_TEMP_LIGHT+3
;	sta	DBG_BYTE1

	; next
	dec	MAP_HEAD_COUNT_LIGHTS
	bne	@loop_lights
@no_lights:

	;;;;
	; textures

@loop_textures:
	; load info
	lda	#MAP_TEXTURE__SIZE
	ldx	#<MAP_TEMP_TEX
	ldy	#>MAP_TEMP_TEX
	jsr	file_read_8_wram
	bcc	:+

	; error
	lda	#4
	jmp	load_error
:
	; DEBUG
	lda	MAP_TEMP_TEX+0
;	sta	DBG_BYTE0
	lda	MAP_TEMP_TEX+1
;	sta	DBG_BYTE0
	lda	MAP_TEMP_TEX+2
;	sta	DBG_BYTE0
	lda	MAP_TEMP_TEX+3
;	sta	DBG_BYTE1

	; next
	dec	MAP_HEAD_COUNT_TEXTURES
	bne	@loop_textures

	;;;;
	; sectors

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; load raw sector data
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$20
	sta	TAB_FILE_BUF_SIZE+1
	stz	TAB_FILE_DEST+0
	lda	#$A0
	sta	TAB_FILE_DEST+1
	jsr	file_read_16_wram
	bcc	:+

	; error
	lda	#8
	jmp	load_error
:
	;;;;
	; player starts

	; player start bank
	lda	#BANK_PLAYER_START
	sta	REG_RAM_BANK

	; load
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$10
	sta	TAB_FILE_BUF_SIZE+1
	stz	TAB_FILE_DEST+0
	lda	#$B0
	sta	TAB_FILE_DEST+1
	jsr	file_read_16_wram
	bcc	:+

	; error
	lda	#9
	jmp	load_error
:
	;;;;
	; wall banks

	; other walls
	lda	#BANK_MAPWALLS
	sta	REG_RAM_BANK
@loop_wdat:
	; load
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$10
	sta	TAB_FILE_BUF_SIZE+1
	stz	TAB_FILE_DEST+0
	lda	#$A0
	sta	TAB_FILE_DEST+1
	jsr	file_read_16_wram
	bcc	:+

	; error
	lda	#10
	jmp	load_error
:
	; next
	inc	REG_RAM_BANK
	dec	MAP_HEAD_COUNT_WBANKS
	bne	@loop_wdat

	;;;;
	; extra storage

	lda	MAP_TEMP_COUNT_EBLOCKS
	beq	@no_extra

	; WRAM pointer
	lda	MAP_LOAD_WRAM_L
	sta	MAP_LOAD_TMP_L
	lda	MAP_LOAD_WRAM_H
	sta	MAP_LOAD_TMP_H

	; WRAM space
	lda	MAP_TEMP_COUNT_EBLOCKS
	jsr	get_wram

	; get destination pointer
	lda	MAP_LOAD_TMP_H
	asl
	asl
	asl
	ldx	MAP_LOAD_TMP_L
	ora	TAB_DIV32,x
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	TAB_FILE_DEST+1

	; load extra data
	stz	TAB_FILE_DEST+0
	stz	TAB_FILE_BUF_SIZE+0
	lda	MAP_TEMP_COUNT_EBLOCKS
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_wram

@no_extra:

	;;;;
	; fix objects

	; sector bank
	lda	#BANK_SECTORS
	sta	REG_RAM_BANK

	; go trough sectors
	ldx	#1
@loop_sobj_fix:
	lda	SECTOR_STRUCT_OBJ_H,x
	beq	:+

	; fix pointer
	clc
	lda	MAP_LOAD_TMP_L
	adc	SECTOR_STRUCT_OBJ_L,x
	sta	SECTOR_STRUCT_OBJ_L,x
	lda	MAP_LOAD_TMP_H
	adc	#$80
	sta	SECTOR_STRUCT_OBJ_H,x
:
	; next
	inx
	bne	@loop_sobj_fix

	;;;;
	; visibility table

	; visibility bank
	lda	#BANK_VISTAB
	sta	REG_RAM_BANK

	; load
	stz	TAB_FILE_BUF_SIZE+0
	lda	#$20
	sta	TAB_FILE_BUF_SIZE+1
	stz	TAB_FILE_DEST+0
	lda	#$A0
	sta	TAB_FILE_DEST+1
	jsr	file_read_16_wram
	bcc	:+

	; error
	lda	#12
	jmp	load_error
:
	;;;;
	; things

	; enable math
	jsr	math_setup_vera

	; reset stuff
	jsr	tick_clear

	; thing load stack pointer
	; (to prevent sprite load recursion)
	tsx
	dex
	dex
	stx	MAP_TEMP_STACK

	; load things
@loop_thing:
	; load info
	lda	#MAP_THING__SIZE
	ldx	#<MAP_TEMP_THING
	ldy	#>MAP_TEMP_THING
	jsr	file_read_8_wram
	bcc	:+

	; error
	lda	#13
	jmp	load_error
:
	; thing bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; find type
	ldx	#0
@loop_thing_type:
	lda	MAP_TEMP_THING_HASH_0
	cmp	THTYPE_NHASH_0,x
	bne	:+
	lda	MAP_TEMP_THING_HASH_1
	cmp	THTYPE_NHASH_1,x
	bne	:+
	lda	MAP_TEMP_THING_HASH_2
	cmp	THTYPE_NHASH_2,x
	bne	:+
	lda	MAP_TEMP_THING_HASH_3
	cmp	THTYPE_NHASH_3,x
	bne	:+

	stx	MAP_TEMP_TH_TYPE
	bra	@do_thing_spawn
:
	inx
	bpl	@loop_thing_type

	; error
	lda	#14
	jmp	load_error

@do_thing_spawn:
	; load sprites
	jsr	load_thing_sprites

	; type
	lda	MAP_TEMP_TH_TYPE
	sta	ARG_THING_TYPE

	; X
	stz	ARG_THING_X_S
	lda	MAP_TEMP_THING_X_L
	sta	ARG_THING_X_L
	lda	MAP_TEMP_THING_X_H
	sta	ARG_THING_X_H

	; Y
	stz	ARG_THING_Y_S
	lda	MAP_TEMP_THING_Y_L
	sta	ARG_THING_Y_L
	lda	MAP_TEMP_THING_Y_H
	sta	ARG_THING_Y_H

	; Z
	stz	ARG_THING_Z_S
	lda	MAP_TEMP_THING_Z_L
	sta	ARG_THING_Z_L
	lda	MAP_TEMP_THING_Z_H
	sta	ARG_THING_Z_H

	; sector
	lda	MAP_TEMP_THING_SECTOR
	sta	ARG_THING_SECTOR

	; spawn thing
	jsr	thing_spawn

	; set thing angle
	lda	MAP_TEMP_THING_ANGLE
	sta	THING_STRUCT_ANGLE,x
	sta	THING_STRUCT_CHASEANG,x

	; set thing extra
;	lda	MAP_TEMP_THING_EXTRA
lda #2 ;; TODO remove
	sta	THING_STRUCT_MAPARG,x

	; next thing
	dec	MAP_TEMP_COUNT_THING
	beq	:+
	jmp	@loop_thing
:
	; close file
	lda	#MAP_FILE_HANDLE
	sta	TAB_FILE_HANDLE
	jsr	file_close

	;;;;
	; finalize

	;; spawn player

	; player start bank
	lda	#BANK_PLAYER_START
	sta	REG_RAM_BANK

	; reset tick command
	stz	TICK_CMD_BITS_L
	stz	TICK_CMD_BITS_H

	; angle to tick command
	lda	PLRSTART_STRUCT_ANGLE
	sta	TICK_CMD_ANGLE

	; pitch to tick command
	lda	PLRSTART_STRUCT_PITCH
	sta	TICK_CMD_PITCH

	; thing type
	lda	#THING_TYPE_PLAYER_N
	sta	ARG_THING_TYPE

	; X
	stz	ARG_THING_X_S
	lda	PLRSTART_STRUCT_X_L
	sta	ARG_THING_X_L
	lda	PLRSTART_STRUCT_X_H
	sta	ARG_THING_X_H

	; Y
	stz	ARG_THING_Y_S
	lda	PLRSTART_STRUCT_Y_L
	sta	ARG_THING_Y_L
	lda	PLRSTART_STRUCT_Y_H
	sta	ARG_THING_Y_H

	; Z
	stz	ARG_THING_Z_S
	lda	PLRSTART_STRUCT_Z_L
	sta	ARG_THING_Z_L
	lda	PLRSTART_STRUCT_Z_H
	sta	ARG_THING_Z_H

	; sector
	lda	PLRSTART_STRUCT_SECTOR
	sta	ARG_THING_SECTOR

	; spawn thing
	jsr	thing_spawn

	; save thing ID
	stx	G_PLAYER_THING
	stx	G_CAMERA_THING

	; update HUD
	stx	TAB_HUD_UPDATE

	; change ticker
	lda	#TICKER_FUNC_PLAYER
	sta	TICKER_STRUCT_FUNC,x

	; set thing angle
	lda	TICK_CMD_ANGLE
	sta	THING_STRUCT_ANGLE,x

	; set thing pitch
	lda	TICK_CMD_PITCH
	sta	THING_STRUCT_PITCH,x

	; camera reset
	lda	THING_STRUCT_VIEW_HEIGHT,x
	sta	G_VIEWHEIGHT
	stz	G_VIEWDELTA
	stz	G_DAMAGE_LEVEL

	; reset level time
	stz	G_LEVEL_TICK_L
	stz	G_LEVEL_TICK_M
	stz	G_LEVEL_TICK_H

	; reset frame time
	stz	G_NEW_TICS
	lda	#2
	sta	G_TICKER

	; clear VSYNC flag
	lda	#1
	sta	VERA_ISR

	; turn off LED
	lda	#0
	ldx	#SMC_I2C_ADDR
	ldy	#SMC_CMD_SET_LED
	jsr	k_i2c_write_byte

	; clear current input
	jsr	input_clear

	; force palette reset
	lda	#$FF
	sta	G_PALETTE_NOW

	; show WRAM usage
;	lda	MAP_LOAD_WRAM_L
;	sta	DBG_BYTE0
;	lda	MAP_LOAD_WRAM_H
;	sta	DBG_BYTE1

	; reset VERA
	lda	#$04
	sta	VERA_CTRL
	stz	VERA_FX_CTRL

	; enable sound
	jsr	sound_start

	rts

;;;
; load thing sprites
;;;

load_thing_sprites:
	rts

;;;
; check reserve space in WRAM
;;;
; A - block count; 0 = 256

get_wram:
	; check count
	tax
	bne	:+

	; = 256
	lda	MAP_LOAD_WRAM_L
	sta	MAP_LOAD_WTST_L
	lda	MAP_LOAD_WRAM_H
	ina
	sta	MAP_LOAD_WTST_H

	bra	:++
:
	; < 256
	clc
	adc	MAP_LOAD_WRAM_L
	sta	MAP_LOAD_WTST_L
	lda	MAP_LOAD_WRAM_H
	adc	#0
	sta	MAP_LOAD_WTST_H
:
	; check size
	lda	MAP_LOAD_WTST_H
	cmp	#$20
	bcs	:+

@pass:
	lda	MAP_LOAD_WTST_L
	sta	MAP_LOAD_WRAM_L
	lda	MAP_LOAD_WTST_H
	sta	MAP_LOAD_WRAM_H
	rts
:
	cmp	#$21
	bcs	@fail
	lda	MAP_LOAD_WTST_L
	beq	@pass

@fail:
	lda	#$FF
	; FALL TROUGH TO 'load_error'

;;;
; map loading error
;;;
; this function restores stack pointer - use 'jmp'
; called when map loading fails
; A is error code

load_error:
	; TODO
	sta DBG_BYTE1
	lda #$EE
	sta DBG_BYTE1
	bra *

	; restore stack pointer
	ldx	TAB_TEMP_STACK
	txs

	; close
	lda	#MAP_FILE_HANDLE
	jsr	file_close_raw

	rts

;;;
; strings
;;;

map_magic:
	.byte	"kgMapX16"

;;; TESTING ONLY

filename_map:
	.byte	"DEFAULT.MAP"
filename_map_end:

