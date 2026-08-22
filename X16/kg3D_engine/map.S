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
MAP_TEMP_DATA_STRUCT = TAB_TEMP_DATA + 256
MAP_TEMP_STACK = TAB_TEMP_DATA + 255

; map light in temp storage
MAP_TEMP_LIGHT = MAP_TEMP_DATA_STRUCT
MAP_TEMP_LIGHT_IDX = MAP_TEMP_DATA_STRUCT + 4
MAP_TEMP_LIGHT_DATA = MAP_TEMP_DATA_STRUCT + 256

; map texture in temp storage
MAP_TEMP_TEX = MAP_TEMP_DATA_STRUCT
MAP_TEMP_TEX_FLAGS = MAP_TEMP_TEX + 4
MAP_TEMP_TEX_VLINK = MAP_TEMP_TEX + 5
MAP_TEMP_TEX_LMAP = MAP_TEMP_TEX + 6
MAP_TEMP_TEX_IDX = MAP_TEMP_DATA_STRUCT + 128
MAP_TEMP_TEX_DATA = MAP_TEMP_DATA_STRUCT + 256

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

; sprite loading
MAP_TEMP_SPR_IDX = MAP_TEMP_THING + 16
MAP_TEMP_SPR_FRAME = MAP_TEMP_THING + 17
MAP_TEMP_SPR_FRMIDX = MAP_TEMP_THING + 18
MAP_TEMP_SPR_COLS_L = MAP_TEMP_THING + 19
MAP_TEMP_SPR_COLS_H = MAP_TEMP_THING + 20
MAP_TEMP_SPR_DAT_L = MAP_TEMP_THING + 21
MAP_TEMP_SPR_DAT_H = MAP_TEMP_THING + 22
MAP_TEMP_SPR_HEAD = MAP_TEMP_DATA_STRUCT + 256
MAP_TEMP_SPRH_LAST = MAP_TEMP_SPR_HEAD + 0
MAP_TEMP_SPRH_FRAME = MAP_TEMP_SPR_HEAD + 1
MAP_TEMP_SPRH_ROTATION = MAP_TEMP_SPR_HEAD + 2
MAP_TEMP_SPRH_WIDTH = MAP_TEMP_SPR_HEAD + 3
MAP_TEMP_SPRH_HEIGHT = MAP_TEMP_SPR_HEAD + 4
MAP_TEMP_SPRH_OX = MAP_TEMP_SPR_HEAD + 5
MAP_TEMP_SPRH_OY = MAP_TEMP_SPR_HEAD + 6
MAP_TEMP_SPRH_DATA_L = MAP_TEMP_SPR_HEAD + 128
MAP_TEMP_SPRH_DATA_M = MAP_TEMP_SPR_HEAD + 129
MAP_TEMP_SPRH_DATA_H = MAP_TEMP_SPR_HEAD + 130
MAP_TEMP_SPRH_DATA_U = MAP_TEMP_SPR_HEAD + 131
MAP_TEMP_SPRH_SIZE = MAP_TEMP_SPR_HEAD + 132
MAP_TEMP_SPRH_COLS = MAP_TEMP_SPR_HEAD + 256

;; math

WADDR_DATA = BANK_WRAM_DATA * 32

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
	; extra code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	; get thing sprite count
	lda	TAB_GFXHEAD_NUM_TSPR
	sta	TAB_NUM_TSPR

	; get logo sprite
	lda	TAB_GFXHEAD_LOGO_SPR
	sta	TAB_MENU_LOGO

	; thing states bank
	lda	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; get player types
	lda	THSTATE_STRUCT_PLAYER_C
	sta	TAB_PLAYER_C
	lda	THSTATE_STRUCT_PLAYER_S
	sta	TAB_PLAYER_S
	lda	THSTATE_STRUCT_PLAYER_F
	sta	TAB_PLAYER_F

	; reset memory
	lda	#<WADDR_DATA
	sta	MAP_LOAD_WRAM_L
	lda	#>WADDR_DATA
	sta	MAP_LOAD_WRAM_H

	; reset remaps
	lda	#$FF
	ldy	#0
@loop_remap:
	sta	TAB_SPRITE_REMAP,y
	iny
	bne	@loop_remap

	; reset sprites
	lda	#1
	sta	MAP_TEMP_SPR_IDX

	; sprite info bank
	lda	#BANK_SPRITE_INFO
	sta	REG_RAM_BANK

	; loops
	ldy	#3
@loop_sprb:
	ldx	#0
@loop_sprr:
	stz	SPRINFO_STRUCT_COLS_H,x
	stz	SPRINFO_STRUCT_COLS_B,x
	stz	SPRINFO_STRUCT_COLS_H+$0800,x
	stz	SPRINFO_STRUCT_COLS_B+$0800,x

	; next sprite
	inx
	bne	@loop_sprr

	; next bank
	inc	REG_RAM_BANK
	dey
	bpl	@loop_sprb
	

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

	; reset vcache
	; for(uint32_t i = VRAM_FIRST_BLOCK; i < VRAM_STOP_BLOCK; i++)
	ldx	#TAB_VCACHE_FIRST_BLOCK
@loop_vclr:
	; vcache[i].prev = i - 1
	txa
	dea
	sta	TAB_VCACHE_PREV,x

	; vcache[i].next = i + 1
	txa
	ina
	sta	TAB_VCACHE_NEXT,x

	; vcache[i].texture = 0
	stz	TAB_VCACHE_TEX,x

	; next
	inx
	cpx	#TAB_VCACHE_STOP_BLOCK
	bcc	@loop_vclr

	; vcache[VRAM_FIRST_BLOCK].prev = 0
	stz	TAB_VCACHE_PREV+TAB_VCACHE_FIRST_BLOCK

	; vcache[VRAM_STOP_BLOCK - 1].next = 0
	stz	TAB_VCACHE_NEXT+TAB_VCACHE_STOP_BLOCK-1

	; vcache_top = VRAM_FIRST_BLOCK
	lda	#TAB_VCACHE_FIRST_BLOCK
	sta	TAB_VCACHE_TOP

	; vcache_cur = VRAM_STOP_BLOCK - 1
	lda	#TAB_VCACHE_STOP_BLOCK-1
	sta	TAB_VCACHE_CUR

	; backup stack pointer
	tsx
	stx	TAB_TEMP_STACK

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; set BAD texture
	ldy	#31
	lda	#$FF
@loop_badtex:
	sta	$A0FF	; modified address
	inc	@loop_badtex+2
	dey
	bpl	@loop_badtex

	lda	#$F5
	sta	TEXINFO_STRUCT_TILEMAP+$FF
	lda	#$F8
	sta	TEXINFO_STRUCT_TILEDATA+$FF

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
	; enable math
	jsr	math_setup_vera

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
	ldy	#15
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

	; load
	jsr	load_sky
	bra	@after_sky
:
	; next
	dey
	bpl	@loop_skyfind

@after_sky:

	;;;;
	; lights

	; reset
	stz	TAB_LIGHT_REMAP
	lda	#1
	sta	MAP_TEMP_LIGHT_IDX

	; load lights
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
	lda	#4
	jmp	load_error
:
	; GFX info bank
	lda	#BAMK_GFX_INFO
	sta	REG_RAM_BANK

	; find light
	ldx	#31
@loop_litfind:
	lda	TAB_GINFO_LIGHT_HASH_L,x
	cmp	MAP_TEMP_LIGHT+0
	bne	:+
	lda	TAB_GINFO_LIGHT_HASH_M,x
	cmp	MAP_TEMP_LIGHT+1
	bne	:+
	lda	TAB_GINFO_LIGHT_HASH_H,x
	cmp	MAP_TEMP_LIGHT+2
	bne	:+
	lda	TAB_GINFO_LIGHT_HASH_U,x
	cmp	MAP_TEMP_LIGHT+3
	bne	:+

	; load
	jsr	load_light
	bra	@next_light
:
	; next
	dex
	bpl	@loop_litfind

@next_light:
	; next
	dec	MAP_HEAD_COUNT_LIGHTS
	bne	@loop_lights
@no_lights:

	;;;;
	; textures

	; reset
	lda	#1
	sta	MAP_TEMP_TEX_IDX

@loop_textures:
	; load info
	lda	#MAP_TEXTURE__SIZE
	ldx	#<MAP_TEMP_TEX
	ldy	#>MAP_TEMP_TEX
	jsr	file_read_8_wram
	bcc	:+

	; error
	lda	#6
	jmp	load_error
:
	; load
	jsr	load_texture

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
	lda	SECTOR_STRUCT_OBJ_B,x
	beq	:+

	; fix address
	clc
	lda	MAP_LOAD_TMP_L
	adc	SECTOR_STRUCT_OBJ_H,x
	sta	SECTOR_STRUCT_OBJ_H,x
	lda	MAP_LOAD_TMP_H
	adc	#0

	; get pointer
	asl
	asl
	asl
	ldy	SECTOR_STRUCT_OBJ_H,x
	ora	TAB_DIV32,y
	sta	SECTOR_STRUCT_OBJ_B,x
	tya
	and	#$1F
	ora	#$A0
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
; load sky
;;;

load_sky:
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
	bcc	:+
	lda	#3
	jmp	load_error
:
	rts

;;;
; load light
;;;

load_light:
	; offset
	txa
	lsr
	clc
	adc	#TAB_GFX_LIGHTS
	sta	MAP_TEMP_LIGHT

	; source sector
	clc
	lda	TAB_GFX_SECTOR_L
	adc	MAP_TEMP_LIGHT
	sta	SD_SEC_L
	lda	TAB_GFX_SECTOR_M
	adc	#0
	sta	SD_SEC_M
	lda	TAB_GFX_SECTOR_H
	adc	#0
	sta	SD_SEC_H
	lda	TAB_GFX_SECTOR_U
	adc	#0
	sta	SD_SEC_U

	; load to temp
	stz	TMP_PTR_L
	lda	#>MAP_TEMP_LIGHT_DATA
	sta	TMP_PTR_H
	jsr	file_read_sector
	bcc	:+
	lda	#5
	jmp	load_error
:
	; source
	clc
	txa
	and	#1
	adc	#>MAP_TEMP_LIGHT_DATA
	sta	@light_copy+2

	; destination (off-by-one)
	clc
	lda	MAP_TEMP_LIGHT_IDX
	adc	#>TAB_LIGHTMAPS-1
	sta	@light_copy+5

	; copy
	ldy	#0
@light_copy:
	lda	MAP_TEMP_LIGHT_DATA,y	; modified address
	sta	TAB_LIGHTMAPS,y	; modified address
	iny
	bne	@light_copy

	; add to remap
	txa
	ldy	MAP_TEMP_LIGHT_IDX
	sta	TAB_LIGHT_REMAP,y
	inc	MAP_TEMP_LIGHT_IDX

	rts

;;;
; load texture
;;;

load_texture:
	; destinations
	ldx	MAP_TEMP_TEX_IDX
	stx	@loop_copy+4
	stx	@loop_clr+1
	lda	#>TEXINFO_STRUCT_LMAP_7
	sta	@loop_copy+5
	lda	#>TEXINFO_STRUCT_VRAM_BASE
	sta	@loop_clr+2

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; copy vlink
	lda	MAP_TEMP_TEX_VLINK
	sta	TEXINFO_STRUCT_VLINK,x

	; clear vram
	ldy	#>(TEXINFO_STRUCT_VRAM_LAST-TEXINFO_STRUCT_VRAM_BASE)
@loop_clr:
	stz	TEXINFO_STRUCT_VRAM_BASE	; modified address
	inc	@loop_clr+2
	dey
	bpl	@loop_clr

	; copy light map
	ldy	#>(TEXINFO_STRUCT_LMAP_7-TEXINFO_STRUCT_LMAP_0)
@loop_copy:
	lda	MAP_TEMP_TEX_LMAP,y
	sta	TEXINFO_STRUCT_VRAM_BASE	; modified address
	dec	@loop_copy+5
	dey
	bpl	@loop_copy

	; GFX info bank
	lda	#BAMK_GFX_INFO
	sta	REG_RAM_BANK

	; get type
	lda	MAP_TEMP_TEX_FLAGS
	bmi	@is_plane

	; find wall
	ldy	#0
@loop_find_wall:
	lda	TAB_GINFO_WALL_HASH_L,y
	cmp	MAP_TEMP_TEX+0
	bne	:+
	lda	TAB_GINFO_WALL_HASH_M,y
	cmp	MAP_TEMP_TEX+1
	bne	:+
	lda	TAB_GINFO_WALL_HASH_H,y
	cmp	MAP_TEMP_TEX+2
	bne	:+
	lda	TAB_GINFO_WALL_HASH_U,y
	cmp	MAP_TEMP_TEX+3
	bne	:+

	jsr	load_wall
	bra	@finish
:
	; next
	iny
	bne	@loop_find_wall

	; not found
@not_found:
	; copy BAD texture
	stx	@loop_bad+4
	lda	#$A0
	sta	@loop_bad+2
	sta	@loop_bad+5
	ldy	#31
@loop_bad:
	lda	$A0FF
	sta	$A000	; modified address
	inc	@loop_bad+2
	inc	@loop_bad+5
	dey
	bpl	@loop_bad

@done:
	inc	MAP_TEMP_TEX_IDX
	rts

@is_plane:
	; find plane
	ldy	#0
@loop_find_plane:
	lda	TAB_GINFO_PLANE_HASH_L,y
	cmp	MAP_TEMP_TEX+0
	bne	:+
	lda	TAB_GINFO_PLANE_HASH_M,y
	cmp	MAP_TEMP_TEX+1
	bne	:+
	lda	TAB_GINFO_PLANE_HASH_H,y
	cmp	MAP_TEMP_TEX+2
	bne	:+
	lda	TAB_GINFO_PLANE_HASH_U,y
	cmp	MAP_TEMP_TEX+3
	bne	:+

	jsr	load_plane

@finish:
	; add light
	sta	VERA_FX_CACHE_H
	stz	VERA_FX_CACHE_U
	lda	MAP_TEMP_TEX_FLAGS
	and	#7
	sta	VERA_FX_CACHE_L
	stz	VERA_FX_CACHE_M

	; reset accumulator
	lda	VERA_FX_ACCUM_RESET

	; save result
	stz	VERA_ADDRx_L
	stz	VERA_DATA0

	; get result + sector
	stz	VERA_ADDRx_L
	clc
	lda	VERA_DATA0
	adc	SD_SEC_L
	sta	TEXINFO_STRUCT_DATA_L,x
	lda	VERA_DATA0
	adc	SD_SEC_M
	sta	TEXINFO_STRUCT_DATA_M,x
	lda	VERA_DATA0
	adc	SD_SEC_H
	sta	TEXINFO_STRUCT_DATA_H,x
	lda	VERA_DATA0
	adc	SD_SEC_U
	sta	TEXINFO_STRUCT_DATA_U,x

	; done
	bra	@done
:
	; next
	iny
	bne	@loop_find_plane

	; not found
	jmp	@not_found

;;;
; load wall
;;;

load_wall:
	; column check
	bit	MAP_TEMP_TEX_FLAGS
	bvc	:+

	jsr	load_columns
:
	; data sector
	clc
	lda	TAB_GINFO_WALL_SECT_L,y
	adc	TAB_GFX_SECTOR_L
	sta	SD_SEC_L
	lda	TAB_GINFO_WALL_SECT_M,y
	adc	TAB_GFX_SECTOR_M
	sta	SD_SEC_M
	lda	TAB_GINFO_WALL_SECT_H,y
	adc	TAB_GFX_SECTOR_H
	sta	SD_SEC_H
	lda	TAB_GINFO_WALL_SECT_U,y
	adc	TAB_GFX_SECTOR_U
	sta	SD_SEC_U

	; type
	lda	TAB_GINFO_WALL_INFO_0,y
	pha

	; tilemap
	lda	TAB_GINFO_WALL_INFO_1,y
	pha

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; origin
	tya
	sta	TEXINFO_STRUCT_ORIGIN,x

	; tilemap
	pla
	sta	TEXINFO_STRUCT_TILEMAP,x

	; type
	pla
	sta	TEXINFO_STRUCT_TYPE,x

	; return sector count
	asl
	asl

	rts

;;;
; load wall
;;;

load_plane:
	; data sector
	clc
	lda	TAB_GINFO_PLANE_SECT_L,y
	adc	TAB_GFX_SECTOR_L
	sta	SD_SEC_L
	lda	TAB_GINFO_PLANE_SECT_M,y
	adc	TAB_GFX_SECTOR_M
	sta	SD_SEC_M
	lda	TAB_GINFO_PLANE_SECT_H,y
	adc	TAB_GFX_SECTOR_H
	sta	SD_SEC_H
	lda	TAB_GINFO_PLANE_SECT_U,y
	adc	TAB_GFX_SECTOR_U
	sta	SD_SEC_U

	; tilemap
	lda	TAB_GINFO_PLANE_INFO,y
	pha

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; origin
	tya
	sta	TEXINFO_STRUCT_ORIGIN,x

	; type
	lda	#$82
	sta	TEXINFO_STRUCT_TYPE,x

	; tilemap
	pla
	sta	TEXINFO_STRUCT_TILEMAP,x

	; ti->tilemap & 0x80
	bmi	:+

	; type
	dec	TEXINFO_STRUCT_TYPE,x
:
	; return sector count
	lda	#8

	rts

;;;
; load wall columns
;;;

load_columns:
	phy

	; WRAM pointer
	lda	MAP_LOAD_WRAM_L
	sta	MAP_LOAD_TMP_L
	lda	MAP_LOAD_WRAM_H
	sta	MAP_LOAD_TMP_H

	; WRAM space
	lda	#1
	jsr	get_wram

	; offset
	tya
	lsr
	clc
	adc	#TAB_GFX_COLUMNS
	sta	MAP_TEMP_TEX

	; source sector
	clc
	lda	TAB_GFX_SECTOR_L
	adc	MAP_TEMP_TEX
	sta	SD_SEC_L
	lda	TAB_GFX_SECTOR_M
	adc	#0
	sta	SD_SEC_M
	lda	TAB_GFX_SECTOR_H
	adc	#0
	sta	SD_SEC_H
	lda	TAB_GFX_SECTOR_U
	adc	#0
	sta	SD_SEC_U

	; load to temp
	stz	TMP_PTR_L
	lda	#>MAP_TEMP_TEX_DATA
	sta	TMP_PTR_H
	phy
	jsr	file_read_sector
	ply
	bcc	:+
	lda	#7
	jmp	load_error
:
	; source
	clc
	tya
	and	#1
	adc	#>MAP_TEMP_TEX_DATA
	sta	@loop_copy+2

	; destination
	lda	MAP_LOAD_TMP_H
	asl
	asl
	asl
	ldy	MAP_LOAD_TMP_L
	ora	TAB_DIV32,y
	sta	REG_RAM_BANK
	pha	; <bank>
	tya
	and	#$1F
	ora	#$A0
	sta	@loop_copy+5
	pha	; <HI>

	; copy columns
	ldy	#0
@loop_copy:
	lda	MAP_TEMP_TEX_DATA,y
	sta	$A000,y	; modified address
	iny
	bne	@loop_copy

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; <HI>
	pla
	sta	TEXINFO_STRUCT_COLS_H,x

	; <bank>
	pla
	sta	TEXINFO_STRUCT_COLS_B,x

	; GFX info bank
	lda	#BAMK_GFX_INFO
	sta	REG_RAM_BANK

	; done
	ply
	rts

;;;
; load thing sprites
;;;
; MAP_TEMP_TH_TYPE is thing type
; load all sprites this thing type uses

load_thing_sprites:
	ldy	MAP_TEMP_TH_TYPE

load_thing_sprites_r:
	; go trough animations
	lda	#THTYPE_ANIM_COUNT
	dec
	sta	MAP_LOAD_AIDX
@loop_anim:
	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; get state count
	lda	MAP_LOAD_AIDX
	ora	#>THTYPE_ANIM_C
	sta	THTEMP_PTR_H
	lda	#$80
	sta	THTEMP_PTR_L
	lda	(THTEMP_PTR_L),y
	beq	@skip_anim
	sta	MAP_LOAD_SIDX

	; get state index pointer
	lda	MAP_LOAD_AIDX
	ora	#>THTYPE_ANIM_L
	sta	THTEMP_PTR_H
	lda	(THTEMP_PTR_L),y
	tax
	lda	MAP_LOAD_AIDX
	ora	#>THTYPE_ANIM_H
	sta	THTEMP_PTR_H
	lda	(THTEMP_PTR_L),y

	; thing states bank
	and	#$07
	ora	#BANK_THING_STATES
	sta	REG_RAM_BANK

	; go trough states
@loop_state:
	phx
	phy

	; get state sprite
	ldy	THSTATE_STRUCT_SPRITE,x

	; check for 'none'
	bmi	@skip_load

	; check if already loaded
	lda	TAB_SPRITE_REMAP,y
	bpl	@skip_load

	; backup bank
	lda	REG_RAM_BANK
	pha

	; load sprite(s)
	jsr	load_sprite

	; thing states bank
	pla
	sta	REG_RAM_BANK

@skip_load:
	ply
	plx

	; increment state pointer
	inx
	bne	:+

	; increment state bank
	inc	REG_RAM_BANK
:
	; next state
	dec	MAP_LOAD_SIDX
	bne	@loop_state

@skip_anim:
	; next anim
	dec	MAP_LOAD_AIDX
	bpl	@loop_anim

	; check for recursion
	tsx
	cpx	MAP_TEMP_STACK
	bne	@skip_recursion

	; go trough spawn types
	lda	#>THTYPE_STRUCT_SPAWN_A
	sta	TMP_PTR_H
	sty	TMP_PTR_L
@loop_spawn:
	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; read spawn[x]
	lda	(TMP_PTR_L)
	bmi	:+

	; recursion
	tay
	jsr	load_thing_sprites_r
:
	; next
	inc	TMP_PTR_H
	lda	#>THTYPE_STRUCT_SPAWN_D+1
	cmp	TMP_PTR_H
	bne	@loop_spawn

@skip_recursion:
	rts

;;;
; load sprite (thing or weapon)
;;;

load_sprite:
	lda	MAP_TEMP_SPR_IDX
	bne	:+

	; error
	lda	#15
	jmp	load_error
:
	phy

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; generate sprite file name
	lda	THTYPE_SHASH_0,y
	sta	MAP_TEMP_THING_HASH_0
	lda	THTYPE_SHASH_1,y
	sta	MAP_TEMP_THING_HASH_1
	lda	THTYPE_SHASH_2,y
	sta	MAP_TEMP_THING_HASH_2
	lda	THTYPE_SHASH_3,y
	sta	MAP_TEMP_THING_HASH_3

	; GFX info bank
	lda	#BAMK_GFX_INFO
	sta	REG_RAM_BANK

	; find sprite
	ldy	#0
@loop_sprfind:
	lda	TAB_GINFO_SPR_HASH_L,y
	cmp	MAP_TEMP_THING_HASH_0
	bne	:+
	lda	TAB_GINFO_SPR_HASH_M,y
	cmp	MAP_TEMP_THING_HASH_1
	bne	:+
	lda	TAB_GINFO_SPR_HASH_H,y
	cmp	MAP_TEMP_THING_HASH_2
	bne	:+
	lda	TAB_GINFO_SPR_HASH_U,y
	cmp	MAP_TEMP_THING_HASH_3
	bne	:+

	; get sector
	clc
	lda	TAB_GINFO_SPR_SECT_L,y
	adc	TAB_GFX_SECTOR_L
	sta	SD_SEC_L
	lda	TAB_GINFO_SPR_SECT_M,y
	adc	TAB_GFX_SECTOR_M
	sta	SD_SEC_M
	lda	TAB_GINFO_SPR_SECT_H,y
	adc	TAB_GFX_SECTOR_H
	sta	SD_SEC_H
	lda	TAB_GINFO_SPR_SECT_U,y
	adc	TAB_GFX_SECTOR_U
	sta	SD_SEC_U
	bra	@spr_found
:
	; next
	iny
	bpl	@loop_sprfind

	; error
;	lda	#16
;	jmp	load_error

	;; DEBUG ONLY TODO
	ply
	rts

@spr_found:
	; check type
	ply
	cpy	TAB_NUM_TSPR
	bcc	@load_tspr
	jmp	@load_wspr

@check_fail:
	lda	#19
	jmp	load_error

@load_tspr:
	;; THING SPRITE

	; mark as loaded
	lda	MAP_TEMP_SPR_IDX
	sta	TAB_SPRITE_REMAP,y

	; load variants
	stz	MAP_TEMP_SPR_FRAME
@loop_tvar:
	; load variant
	stz	TMP_PTR_L
	lda	#>MAP_TEMP_SPR_HEAD
	sta	TMP_PTR_H
	jsr	file_read_sector
	bcc	:+
	lda	#17
	jmp	load_error
:
	; if(sprh->rotation > 7)
	lda	MAP_TEMP_SPRH_ROTATION
	cmp	#8
	bcs	@check_fail

	; if(sprh->frame > 26)
	lda	MAP_TEMP_SPRH_FRAME
	cmp	#27
	bcs	@check_fail

	; if(frame < sprh->frame)
	cmp	MAP_TEMP_SPR_FRAME
	bcc	:+

	; frame = sprh->frame
	sta	MAP_TEMP_SPR_FRAME
:
	; frmidx = num_sprites + sprh->frame
	clc
	adc	MAP_TEMP_SPR_IDX

	; if(frmidx >= 256)
	bcs	@check_fail

	sta	MAP_TEMP_SPR_FRMIDX
	tax

	; si->cols = wram_used / 256
	lda	MAP_LOAD_WRAM_H
	asl
	asl
	asl
	ldy	MAP_LOAD_WRAM_L
	ora	TAB_DIV32,y
	sta	MAP_TEMP_SPR_COLS_H
	tya
	and	#$1F
	ora	#$A0
	sta	MAP_TEMP_SPR_COLS_L
	sta	@loop_tcol+5

	; WRAM space for columns
	lda	#1
	jsr	get_wram

	; si = sprite_info[sprh->rotation] + frmidx
	lda	MAP_TEMP_SPRH_ROTATION
	lsr
	ora	#BANK_SPRITE_INFO
	sta	REG_RAM_BANK
	bcs	:+
	jmp	@tspr_rote
:
	jmp	@tspr_roto

@tspr_next:
	; column bank
	lda	MAP_TEMP_SPR_COLS_H
	sta	REG_RAM_BANK

	; copy columns
	ldy	#0
@loop_tcol:
	lda	MAP_TEMP_SPRH_COLS,y
	sta	$A000,y	; modified address
	dey
	bne	@loop_tcol

	; next
	lda	MAP_TEMP_SPRH_LAST
	beq	@loop_tvar

	; WRAM align
	lda	MAP_LOAD_WRAM_L
	lsr
	bcc	:+
	lda	#1
	jsr	get_wram
:
	; dpos = wram_used / 256
	lda	MAP_LOAD_WRAM_H
	sta	MAP_TEMP_SPR_DAT_H
	asl
	asl
	asl
	ldy	MAP_LOAD_WRAM_L
	sty	MAP_TEMP_SPR_DAT_L
	ora	TAB_DIV32,y
	sta	REG_RAM_BANK
	tya
	and	#$1F
	ora	#$A0
	pha

	; WRAM space for data
	lda	MAP_TEMP_SPRH_SIZE
	asl
	jsr	get_wram

	; get sector
	clc
	lda	MAP_TEMP_SPRH_DATA_L
	adc	TAB_GFX_SECTOR_L
	sta	SD_SEC_L
	lda	MAP_TEMP_SPRH_DATA_M
	adc	TAB_GFX_SECTOR_M
	sta	SD_SEC_M
	lda	MAP_TEMP_SPRH_DATA_H
	adc	TAB_GFX_SECTOR_H
	sta	SD_SEC_H
	lda	MAP_TEMP_SPRH_DATA_U
	adc	TAB_GFX_SECTOR_U
	sta	SD_SEC_U

	; load data
	ldx	MAP_TEMP_SPRH_SIZE
	pla
	jsr	file_read_sectors
	bcc	:+
	lda	#18
	jmp	load_error
:
	; frame += num_sprites + 1
	sec
	lda	MAP_TEMP_SPR_IDX
	tay
	adc	MAP_TEMP_SPR_FRAME
	sta	MAP_TEMP_SPR_FRAME

	; for(uint32_t i = num_sprites; i < frame; i++)
@loop_dset:
	; for(uint32_t j = 0; j < 8; j++)
	ldx	#3
	lda	#BANK_SPRITE_INFO
	sta	REG_RAM_BANK
@loop_rset:
	; sprite_info[j][i].data = dpos
	lda	MAP_TEMP_SPR_DAT_L
	sta	SPRINFO_STRUCT_DATA_L,y
	sta	SPRINFO_STRUCT_DATA_L+$0800,y
	lda	MAP_TEMP_SPR_DAT_H
	sta	SPRINFO_STRUCT_DATA_H,y
	sta	SPRINFO_STRUCT_DATA_H+$0800,y

	; next rotations
	inc	REG_RAM_BANK
	dex
	bpl	@loop_rset

	; next frame
	iny
	cpy	MAP_TEMP_SPR_FRAME
	bcc	@loop_dset

	; num_sprites = frame
	lda	MAP_TEMP_SPR_FRAME
	sta	MAP_TEMP_SPR_IDX

	rts

@load_wspr:
	;; TODO
	lda	#111
	jmp	load_error

@tspr_roto:
	; columns
	lda	MAP_TEMP_SPR_COLS_L
	sta	SPRINFO_STRUCT_COLS_H+$0800,x
	lda	MAP_TEMP_SPR_COLS_H
	sta	SPRINFO_STRUCT_COLS_B+$0800,x

	; si->width = sprh->width
	lda	MAP_TEMP_SPRH_WIDTH
	sta	SPRINFO_STRUCT_WIDTH+$0800,x

	; si->height = sprh->height
	lda	MAP_TEMP_SPRH_HEIGHT
	sta	SPRINFO_STRUCT_HEIGHT+$0800,x

	; si->ox = sprh->ox
	lda	MAP_TEMP_SPRH_OX
	sta	SPRINFO_STRUCT_OX+$0800,x

	; si->oy = sprh->oy
	lda	MAP_TEMP_SPRH_OY
	sta	SPRINFO_STRUCT_OY+$0800,x

	jmp	@tspr_next

@tspr_rote:
	; columns
	lda	MAP_TEMP_SPR_COLS_L
	sta	SPRINFO_STRUCT_COLS_H,x
	lda	MAP_TEMP_SPR_COLS_H
	sta	SPRINFO_STRUCT_COLS_B,x

	; si->width = sprh->width
	lda	MAP_TEMP_SPRH_WIDTH
	sta	SPRINFO_STRUCT_WIDTH,x

	; si->height = sprh->height
	lda	MAP_TEMP_SPRH_HEIGHT
	sta	SPRINFO_STRUCT_HEIGHT,x

	; si->ox = sprh->ox
	lda	MAP_TEMP_SPRH_OX
	sta	SPRINFO_STRUCT_OX,x

	; si->oy = sprh->oy
	lda	MAP_TEMP_SPRH_OY
	sta	SPRINFO_STRUCT_OY,x

	jmp	@tspr_next


;;;
; check reserve space in WRAM
;;;
; A - block count; 0 = 256

get_wram:
	; check count
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

