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
EXT_FILE_HANDLE = 3

VRAM_TEXTURE_START = $0A000
VRAM_TEXTURE_END = $1C000

; name hash in temp storage
MAP_TEMP_NHASH_0 = TAB_TEMP_DATA + 0
MAP_TEMP_NHASH_1 = TAB_TEMP_DATA + 1
MAP_TEMP_NHASH_2 = TAB_TEMP_DATA + 2
MAP_TEMP_NHASH_3 = TAB_TEMP_DATA + 3

; variant hash in temp storage
MAP_TEMP_VHASH_0 = TAB_TEMP_DATA + 4
MAP_TEMP_VHASH_1 = TAB_TEMP_DATA + 5
MAP_TEMP_VHASH_2 = TAB_TEMP_DATA + 6
MAP_TEMP_VHASH_3 = TAB_TEMP_DATA + 7

; light bits in temp storage
MAP_TEMP_LIGHTS = TAB_TEMP_DATA + 4

; plane bright map in temp storage
MAP_TEMP_PLANE_BRIGHT8 = TAB_TEMP_DATA + 128

; plane variant hash in temp storage
MAP_TEMP_PLANE_VHASH_0 = TAB_TEMP_DATA + 128
MAP_TEMP_PLANE_VHASH_1 = TAB_TEMP_DATA + 129
MAP_TEMP_PLANE_VHASH_2 = TAB_TEMP_DATA + 130
MAP_TEMP_PLANE_VHASH_3 = TAB_TEMP_DATA + 131

; plane bright map in temp storage
MAP_TEMP_PLANE4_BRIGHT = TAB_TEMP_DATA + 132

; plane color map in temp storage
MAP_TEMP_PLANE4_COLORMAP = TAB_TEMP_DATA + 134

; plane effect in temp storage
MAP_TEMP_PLANE4_EFFECT = TAB_TEMP_DATA + 150

; expanded plane bright map in temp storage
MAP_TEMP_PLANE4_BRIGHTMAP = TAB_TEMP_DATA + 154

; wall variant hash in temp storage
MAP_TEMP_WALL_VHASH_0 = TAB_TEMP_DATA + 128
MAP_TEMP_WALL_VHASH_1 = TAB_TEMP_DATA + 129
MAP_TEMP_WALL_VHASH_2 = TAB_TEMP_DATA + 130
MAP_TEMP_WALL_VHASH_3 = TAB_TEMP_DATA + 131

; wall variant info in temp storage
MAP_TEMP_WALL_HTYPE = TAB_TEMP_DATA + 132

; wall animation info in temp storage
MAP_TEMP_WALL_ANIM_C = TAB_TEMP_DATA + 133
MAP_TEMP_WALL_ANIM_T = TAB_TEMP_DATA + 134
MAP_TEMP_WALL_ANIM_O = TAB_TEMP_DATA + 135

; thing sprite variant info in temp storage
MAP_TEMP_SPRITE_FRM = TAB_TEMP_DATA + 128
MAP_TEMP_SPRITE_ROT = TAB_TEMP_DATA + 129
MAP_TEMP_SPRITE_OX = TAB_TEMP_DATA + 130
MAP_TEMP_SPRITE_OY = TAB_TEMP_DATA + 131
MAP_TEMP_SPRITE_WIDTH = TAB_TEMP_DATA + 132
MAP_TEMP_SPRITE_HEIGHT = TAB_TEMP_DATA + 133

; structure temp storage
MAP_TEMP_DATA_STRUCT = TAB_TEMP_DATA + 512
MAP_TEMP_STACK = TAB_TEMP_DATA + 512 + 64 + 32

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

; loaded plane effect
MAP_TEMP_PLANE8_EFFECT = $B000

;;;
; info
;;;

MAP_HEAD__SIZE = $20
MAP_HEAD__VERSION = 27
MAP_CMAP__SIZE = 4
MAP_PLANE__SIZE = 5
MAP_WALL__SIZE = 5
MAP_TEXTURE__SIZE = 8
MAP_PLAYER_START__SIZE = 10
MAP_THING__SIZE = 14

PLANE_HEAD_SIZE = 2
PLANE_DATA8_SIZE = 4100	; 64 * 64 + 4
PLANE_DATA4_SIZE = 2048
PLANE_BRIGHT8_SIZE = 32
PLANE_VARIANT_SIZE = 26

WALL_HEAD_SIZE = 2
WALL_VARIANT_SIZE = 8

TSPR_HEAD_SIZE = 2
TSPR_VARIANT_SIZE = 6

WSPR_HEAD_SIZE = 2
WSPR_VARIANT_SIZE = 5

;;;
; stuff
;;;

MAP_TEMP_IMG_IDX_L = G_LEVEL_TICK_L
MAP_TEMP_IMG_IDX_H = G_LEVEL_TICK_H
MAP_TEMP_SPR_IDX = TICK_CMD_BITS_L
MAP_TEMP_WPN_IDX = BACKSEC_PTR_I
MAP_TEMP_TH_TYPE = TICK_CMD_BITS_H
MAP_TEMP_COUNT_THING = TICK_CMD_ANGLE
MAP_TEMP_COUNT_EBLOCKS = G_LEVEL_TICK_M
MAP_TEMP_COUNT_LIGHTS = TICK_CMD_PITCH
MAP_TEMP_START_COUNT = G_TICKER_IDX
MAP_LOAD_WRAM_L = FUNC_ARG_L
MAP_LOAD_WRAM_H = FUNC_ARG_H
MAP_LOAD_TEX_TMP = SECTOR_PTR_I

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
	lda	#$A0
	sta	MAP_LOAD_WRAM_L
	lda	#$04
	sta	MAP_LOAD_WRAM_H
	stz	MAP_TEMP_SPR_IDX
	stz	MAP_TEMP_WPN_IDX
	stz	MAP_TEMP_IMG_IDX_L
	stz	MAP_TEMP_IMG_IDX_H

	; reset sprite remaps
	lda	#$FF
	ldy	#$7F
@loop_reset:
	sta	TAB_SPRITE_REMAP,y
	dey
	bpl	@loop_reset

	; tsprite info bank
	lda	#BANK_TSPR_INFO
	sta	REG_RAM_BANK

	; clear sprite rotation flag
	lda	#0
	tay
@loop_rclr:
	sta	SPRINFO_STRUCT_ROTATE,y
	dey
	bne	@loop_rclr

	; thing load stack pointer
	; (to prevent sprite load recursion)
	tsx
	dex
	dex
	stx	MAP_TEMP_STACK

	; check logo sprite
	ldy	TAB_MENU_LOGO
	bmi	:+

	; name is already set
	jsr	load_sprite_wpn

	; apply remap
	ldx	TAB_MENU_LOGO
	lda	TAB_SPRITE_REMAP,x
	sta	TAB_MENU_LOGO
:
	; precache base things
	lda	#THING_WEAPON_FIRST
	sta	MAP_TEMP_TH_TYPE
@loop_preload:
	jsr	load_thing_sprites
	inc	MAP_TEMP_TH_TYPE
	bpl	@loop_preload

	; code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	; save sprite remaps
	lda	#$FF
	ldy	#$7F
@loop_save:
	lda	TAB_SPRITE_REMAP,y
	sta	TAB_PRELOAD_REMAP,y
	dey
	bpl	@loop_save

	; save precached values
	lda	MAP_TEMP_SPR_IDX
	sta	TAB_PRELOAD_SPR_IDX
	lda	MAP_TEMP_WPN_IDX
	sta	TAB_PRELOAD_WPN_IDX
	lda	MAP_TEMP_IMG_IDX_L
	sta	TAB_PRELOAD_IMG_IDX+0
	lda	MAP_TEMP_IMG_IDX_H
	sta	TAB_PRELOAD_IMG_IDX+1
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
	stz	MAP_LOAD_TEX

	; use precached values
	lda	TAB_PRELOAD_SPR_IDX
	sta	MAP_TEMP_SPR_IDX
	lda	TAB_PRELOAD_WPN_IDX
	sta	MAP_TEMP_WPN_IDX
	lda	TAB_PRELOAD_IMG_IDX+0
	sta	MAP_TEMP_IMG_IDX_L
	lda	TAB_PRELOAD_IMG_IDX+1
	sta	MAP_TEMP_IMG_IDX_H
	lda	TAB_PRELOAD_WRAM+0
	sta	MAP_LOAD_WRAM_L
	lda	TAB_PRELOAD_WRAM+1
	sta	MAP_LOAD_WRAM_H

	; code bank
	lda	#BANK_EXTRA_CODE
	sta	REG_RAM_BANK

	; reset GFX remaps
	ldy	#$7F
@loop_remap:
	lda	TAB_PRELOAD_REMAP,y
	sta	TAB_SPRITE_REMAP,y
	lda	#$FF
	sta	TAB_TEXTURE_REMAP,y
	dey
	bpl	@loop_remap

	; tsprite info bank
	lda	#BANK_TSPR_INFO
	sta	REG_RAM_BANK

	; clear sprite rotation flag
	lda	#0
	ldy	MAP_TEMP_SPR_IDX
@loop_rclr:
	sta	SPRINFO_STRUCT_ROTATE,y
	iny
	bne	@loop_rclr

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

	; name
	ldx	#(type_sky-txt_types)
	jsr	copy_name_type

	; generate file name
	lda	MAP_HEAD_HASH_SKY+0
	sta	MAP_TEMP_NHASH_0
	lda	MAP_HEAD_HASH_SKY+1
	sta	MAP_TEMP_NHASH_1
	lda	MAP_HEAD_HASH_SKY+2
	sta	MAP_TEMP_NHASH_2
	lda	MAP_HEAD_HASH_SKY+3
	sta	MAP_TEMP_NHASH_3
	jsr	make_hash

	; sky pointer
	lda	MAP_LOAD_WRAM_H
	asl
	asl
	asl
	ldx	MAP_LOAD_WRAM_L
	ora	TAB_BANK,x
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	TAB_FILE_DEST+1

	; load light from file
	stz	TAB_FILE_DEST+0
	lda	#(file_name_end-file_name)
	ldx	#<file_name
	ldy	#>file_name
	jsr	file_load_wram
	bcc	:+

	lda	#3
	jmp	load_error
:
	; store sky WRAM index
	lda	MAP_LOAD_WRAM_L
	sta	TAB_SKY_BASE+0
	lda	MAP_LOAD_WRAM_H
	sta	TAB_SKY_BASE+1

	; advance WRAM index
	inc	MAP_LOAD_WRAM_H

	; restore handle
	jsr	file_restore_handle

@after_sky:

	;;;;
	; lights

	ldx	#(type_light-txt_types)
	jsr	copy_name_type

	; generate white light
	lda	#0
@loop_white:
	tay
	sta	TAB_LIGHTMAPS,y
	dea
	bne	@loop_white

	; load other lights
	lda	#1
	sta	MAP_LOAD_IDX
@loop_light:
	lda	MAP_LOAD_IDX
	cmp	MAP_TEMP_COUNT_LIGHTS
	beq	@end_light

	; load info
	lda	#MAP_CMAP__SIZE
	ldx	#<TAB_TEMP_DATA
	ldy	#>TAB_TEMP_DATA
	jsr	file_read_8_wram
	bcc	:+

@err_light:
	; error
	lda	#4
	jmp	load_error
:
	; generate file name
	jsr	make_hash

	; get destination
	clc
	lda	MAP_LOAD_IDX
	adc	#>TAB_LIGHTMAPS
	sta	TAB_FILE_DEST+1
	stz	TAB_FILE_DEST+0

	; load light from file
	lda	#(file_name_end-file_name)
	ldx	#<file_name
	ldy	#>file_name
	jsr	file_load_wram
	bcs	@err_light

	; restore handle
	jsr	file_restore_handle

	; next
	inc	MAP_LOAD_IDX
	bra	@loop_light
@end_light:

	;;;;
	; planes

	ldx	#(type_plane-txt_types)
	jsr	copy_name_type

	; reset VERA
	lda	#$04
	sta	VERA_CTRL
	stz	VERA_FX_CTRL

	; reset index
	stz	MAP_LOAD_P4
	lda	#(VRAM_TEXTURE_END-VRAM_TEXTURE_START)/2048
	sta	MAP_LOAD_P8

	; load planes
	stz	MAP_LOAD_IDX
@loop_plane:
	lda	MAP_LOAD_IDX
	cmp	MAP_HEAD_COUNT_PTEX
	beq	@end_plane

	; check VRAM space
	lda	MAP_LOAD_P4
	and	#$FE
	cmp	MAP_LOAD_P8
	beq	@err_plane

	; load info
	lda	#MAP_PLANE__SIZE
	ldx	#<TAB_TEMP_DATA
	ldy	#>TAB_TEMP_DATA
	jsr	file_read_8_wram
	bcc	:+

@err_plane:
	; error
	lda	#5
	jmp	load_error
:
	; check texture count
	lda	MAP_LOAD_TEX
	bmi	@skip_plane_load

	; generate file name
	jsr	make_hash

	; open plane file
	lda	#EXT_FILE_HANDLE
	sta	TAB_FILE_HANDLE
	lda	#(file_name_end-file_name)
	ldx	#<file_name
	ldy	#>file_name
	jsr	file_open_rd

	; read header
	lda	#PLANE_HEAD_SIZE
	ldx	#<MAP_PLANE_BASE
	ldy	#>MAP_PLANE_BASE
	jsr	file_read_8_wram
	bcs	@skip_plane

	; load plane(s)
	lda	MAP_PLANE_VCNT
	bmi	:+
	jsr	load_plane4
	bra	:++
:
	jsr	load_plane8
:
@skip_plane:
	; close plane file
	jsr	file_close

	; restore handle
	ldx	#MAP_FILE_HANDLE
	jsr	file_set_handle

@skip_plane_load:
	; next
	inc	MAP_LOAD_IDX
	bra	@loop_plane
@end_plane:

	;;;;
	; walls

	ldx	#(type_wall-txt_types)
	jsr	copy_name_type

	; load walls
	stz	MAP_LOAD_IDX
@loop_wall:
	lda	MAP_LOAD_IDX
	cmp	MAP_HEAD_COUNT_WTEX
	beq	@end_wall

	; load info
	lda	#MAP_WALL__SIZE
	ldx	#<TAB_TEMP_DATA
	ldy	#>TAB_TEMP_DATA
	jsr	file_read_8_wram
	bcc	:+

	; error
	lda	#6
	jmp	load_error
:
	; check texture count
	lda	MAP_LOAD_TEX
	bmi	@skip_wall_load

	; generate file name
	jsr	make_hash

	; open wall file
	lda	#EXT_FILE_HANDLE
	sta	TAB_FILE_HANDLE
	lda	#(file_name_end-file_name)
	ldx	#<file_name
	ldy	#>file_name
	jsr	file_open_rd

	; read header
	lda	#WALL_HEAD_SIZE
	ldx	#<MAP_WALL_DLEN
	ldy	#>MAP_WALL_DLEN
	jsr	file_read_8_wram
	bcs	:+

	; load wall(s)
	jsr	load_wall
:
	; close wall file
	jsr	file_close

	; restore handle
	ldx	#MAP_FILE_HANDLE
	jsr	file_set_handle

@skip_wall_load:
	; next
	inc	MAP_LOAD_IDX
	bra	@loop_wall
@end_wall:

	;;;;
	; textures

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; load textures
	stz	MAP_LOAD_IDX
@loop_tex:
	lda	MAP_LOAD_IDX
	cmp	MAP_HEAD_COUNT_TEXTURES
	beq	@end_tex

	; load info
	lda	#MAP_TEXTURE__SIZE
	ldx	#<TAB_TEMP_DATA
	ldy	#>TAB_TEMP_DATA
	jsr	file_read_8_wram
	bcc	:+

	; error
	lda	#7
	jmp	load_error
:
	; find this texture
	lda	MAP_LOAD_TEX
	beq	@not_found
	ldy	#0
@loop_xet:
	; check name hash
	lda	TEXINFO_STRUCT_NHASH_0,y
	cmp	MAP_TEMP_NHASH_0
	bne	:+
	lda	TEXINFO_STRUCT_NHASH_1,y
	cmp	MAP_TEMP_NHASH_1
	bne	:+
	lda	TEXINFO_STRUCT_NHASH_2,y
	cmp	MAP_TEMP_NHASH_2
	bne	:+
	lda	TEXINFO_STRUCT_NHASH_3,y
	cmp	MAP_TEMP_NHASH_3
	bne	:+

	; check variant hash
	lda	TEXINFO_STRUCT_VHASH_0,y
	cmp	MAP_TEMP_VHASH_0
	bne	:+
	lda	TEXINFO_STRUCT_VHASH_1,y
	cmp	MAP_TEMP_VHASH_1
	bne	:+
	lda	TEXINFO_STRUCT_VHASH_2,y
	cmp	MAP_TEMP_VHASH_2
	bne	:+
	lda	TEXINFO_STRUCT_VHASH_3,y
	cmp	MAP_TEMP_VHASH_3
	bne	:+

	; found
	tya
	bra	@do_remap
:
	; next
	iny
	cpy	MAP_LOAD_TEX
	bne	@loop_xet

@not_found:
	; not found
	lda	#$FF

@do_remap:
	; store remap
	ldx	MAP_LOAD_IDX
	sta	TAB_TEXTURE_REMAP,x

	; next
	inc	MAP_LOAD_IDX
	bra	@loop_tex
@end_tex:

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

	; check WRAM space
	clc
	lda	MAP_LOAD_WRAM_L
	sta	MAP_LOAD_TMP_L
	adc	MAP_TEMP_COUNT_EBLOCKS
	lda	MAP_LOAD_WRAM_H
	sta	MAP_LOAD_TMP_H
	adc	#0
	cmp	#$20
	bcc	:+

	; error
	lda	#11
	jmp	load_error
:
	; advance WRAM pointer
	clc
	lda	MAP_TEMP_COUNT_EBLOCKS
	adc	MAP_LOAD_WRAM_L
	sta	MAP_LOAD_WRAM_L
	bcc	:+
	inc	MAP_LOAD_WRAM_H
:
	; get destination pointer
	lda	MAP_LOAD_TMP_H
	asl
	asl
	asl
	ldx	MAP_LOAD_TMP_L
	ora	TAB_BANK,x
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

	; set thing pitch
	lda	#$80
	sta	THING_STRUCT_PITCH,x

	; next thing
	dec	MAP_TEMP_COUNT_THING
	beq	:+
	jmp	@loop_thing
:
	; close file
	lda	#MAP_FILE_HANDLE
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
; load 4 bit planes
;;;

load_plane4:
	; map temp bank
	lda	#BANK_MAP_TEMP
	sta	REG_RAM_BANK

	; load texture data
	stz	TAB_FILE_DEST+0
	lda	#$A0
	sta	TAB_FILE_DEST+1
	lda	#<PLANE_DATA4_SIZE
	sta	TAB_FILE_BUF_SIZE+0
	lda	#>PLANE_DATA4_SIZE
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_wram
	bcc	:+

	; error
	rts
:
	; dcsel = 2, addrsel = 0
	lda	#$04
	sta	VERA_CTRL

	; 4-bit mode
	lda	#%00000100
	sta	VERA_FX_CTRL

	; ADDR0 = VRAM_TEXTURE_START + (vram_4bpp >> 1) * 4096, increment = 1
	stz	VERA_ADDRx_L
	lda	MAP_LOAD_P4
	lsr
	clc
	adc	#$0A
	tax
	lda	TAB_SWAP,x
	and	#$F0
	sta	VERA_ADDRx_M
	lda	TAB_SWAP,x
	and	#$0F
	ora	#%00010000
	sta	VERA_ADDRx_H

	; add nibble address
	lda	MAP_LOAD_P4
	asl
	and	#%00000010
	ora	VERA_ADDRx_H
	sta	VERA_ADDRx_H

	; check mode
	and	#%00000010
	bne	:+

	; storing to HI nibble
	lda	#$8D	; STA opcode
	sta	@opcodes+0
	lda	#$8E	; STX opcode
	sta	@opcodes+3
	; colormap generator A
	lda	#<generate_cmap_16_a
	sta	@cgen_jsr+1
	lda	#>generate_cmap_16_a
	sta	@cgen_jsr+2
	bra	:++
:
	; storing to LO nibble
	lda	#$8E	; STX opcode
	sta	@opcodes+0
	lda	#$8D	; STA opcode
	sta	@opcodes+3
	; colormap generator B
	lda	#<generate_cmap_16_b
	sta	@cgen_jsr+1
	lda	#>generate_cmap_16_b
	sta	@cgen_jsr+2
:
	; reset pointer
	lda	#$A0
	sta	@loop_lo+2

	; load texture data
	lda	#8
	sta	MAP_LOAD_XDI
@loop_hi:
	ldy	#0
@loop_lo:
	ldx	$A000,y
	lda	TAB_SWAP,x
@opcodes:
	sta	VERA_DATA0	; stx
	stx	VERA_DATA0	; sta
	iny
	bne	@loop_lo
	inc	@loop_lo+2
	dec	MAP_LOAD_XDI
	bne	@loop_hi

	; reset VERA
	stz	VERA_FX_CTRL

	; increment data index
	inc	MAP_LOAD_P4

	; create variants
@loop_variant:
	; check texture count
	lda	MAP_LOAD_TEX
	bpl	:+

	; out of textures
	rts
:
	; load variant info
	lda	#PLANE_VARIANT_SIZE
	ldx	#<MAP_TEMP_PLANE_VHASH_0
	ldy	#>MAP_TEMP_PLANE_VHASH_0
	jsr	file_read_8_wram
	bcc	:+

	; read failed
	rts
:
	; copy light info
	lda	MAP_TEMP_LIGHTS
	sta	MAP_LOAD_TMP_LIGHTS

	; check for fullbright
	lda	MAP_TEMP_PLANE4_BRIGHT+0
	ina
	bne	:+
	lda	MAP_TEMP_PLANE4_BRIGHT+1
	ina
	bne	:+

	; only white light info
	lda	#$01
	sta	MAP_LOAD_TMP_LIGHTS
:
	;; new texture
	ldy	MAP_LOAD_TEX

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; name hash
	jsr	copy_name_hash

	; variant hash
	lda	MAP_TEMP_PLANE_VHASH_0
	sta	TEXINFO_STRUCT_VHASH_0,y
	lda	MAP_TEMP_PLANE_VHASH_1
	sta	TEXINFO_STRUCT_VHASH_1,y
	lda	MAP_TEMP_PLANE_VHASH_2
	sta	TEXINFO_STRUCT_VHASH_2,y
	lda	MAP_TEMP_PLANE_VHASH_3
	sta	TEXINFO_STRUCT_VHASH_3,y

	; variant effect
	lda	MAP_TEMP_PLANE4_EFFECT+0
	sta	TEXINFO_STRUCT_EFFECT_0,y
	lda	MAP_TEMP_PLANE4_EFFECT+1
	sta	TEXINFO_STRUCT_EFFECT_1,y
	lda	MAP_TEMP_PLANE4_EFFECT+2
	sta	TEXINFO_STRUCT_EFFECT_2,y
	lda	MAP_TEMP_PLANE4_EFFECT+3
	sta	TEXINFO_STRUCT_EFFECT_3,y

	; type
	lda	#$F0
	sta	TEXINFO_STRUCT_TYPE,y

	; tile map base
	lda	MAP_PLANE_BASE
	sta	TEXINFO_STRUCT_P_TILE_MAP,y

	; tile data base
	lda	MAP_LOAD_P4
	dea
	and	#$FE
	clc
	adc	#VRAM_TEXTURE_START/2048
	asl
	asl
	sta	TEXINFO_STRUCT_P_TILE_DATA,y

	;; colormap info

	; prepare light pointer
	lda	#>TEXINFO_STRUCT_P_COLORMAP
	sta	TEXTURE_PTR_H
	lda	MAP_LOAD_TEX
	sta	TEXTURE_PTR_L
	ldy	#$80

	; set all lights to default
	ldx	MAP_TEMP_COUNT_LIGHTS
@loop_dflt:
	lda	MAP_LOAD_WRAM_L
	sta	(TEXTURE_PTR_L)
	lda	MAP_LOAD_WRAM_H
	sta	(TEXTURE_PTR_L),y
	inc	TEXTURE_PTR_H
	dex
	bne	@loop_dflt

	; prepare light pointer
	lda	#>TEXINFO_STRUCT_P_COLORMAP
	sta	TEXTURE_PTR_H

	; generate light maps
	lda	#0
	sta	MAP_LOAD_XDI
@loop_light:
	; check if this light is used
	ror	MAP_LOAD_TMP_LIGHTS
	bcc	@skip_light

	; store colormap index for this light
	ldy	#$80
	lda	MAP_LOAD_WRAM_L
	sta	(TEXTURE_PTR_L)
	lda	MAP_LOAD_WRAM_H
	sta	(TEXTURE_PTR_L),y

	; copy brightmap
	lda	MAP_TEMP_PLANE4_BRIGHT+0
	sta	MAP_LOAD_TMP_BRIGHT_0
	lda	MAP_TEMP_PLANE4_BRIGHT+1
	sta	MAP_LOAD_TMP_BRIGHT_1

	; expand brightmap
	ldx	#$F0
@loop_expand:
	ror	MAP_LOAD_TMP_BRIGHT_1
	ror	MAP_LOAD_TMP_BRIGHT_0

	; store expanded byte
	ror
	sta	MAP_TEMP_PLANE4_BRIGHTMAP-$F0,x

	; next
	inx
	bne	@loop_expand

	; generate this light
	clc
	lda	MAP_LOAD_XDI
	adc	#>TAB_LIGHTMAPS
	sta	HI_PTR_H

	; colormap pointer (destination)
	ldx	MAP_LOAD_WRAM_L
	lda	MAP_LOAD_WRAM_H
	asl
	asl
	asl
	ora	TAB_BANK,x
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	COLORMAP_H

@cgen_jsr:
	jsr	generate_cmap_16_a	; modified address

	; increment WRAM index
	inc	MAP_LOAD_WRAM_L
	bne	:+
	inc	MAP_LOAD_WRAM_H
:
	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

@skip_light:
	inc	TEXTURE_PTR_H
	inc	MAP_LOAD_XDI
	lda	MAP_LOAD_XDI
	cmp	MAP_TEMP_COUNT_LIGHTS
	bne	@loop_light

	; add this texture
	inc	MAP_LOAD_TEX

	; next
	dec	MAP_PLANE_VCNT
	beq	:+
	jmp	@loop_variant
:
@end_variant:
	rts

;;;
; load 8 bit plane
;;;

load_plane8:
	; decrement data index
	dec	MAP_LOAD_P8
	dec	MAP_LOAD_P8

	; map temp bank
	lda	#BANK_MAP_TEMP
	sta	REG_RAM_BANK

	; load texture data
	stz	TAB_FILE_DEST+0
	lda	#$A0
	sta	TAB_FILE_DEST+1
	lda	#<PLANE_DATA8_SIZE
	sta	TAB_FILE_BUF_SIZE+0
	lda	#>PLANE_DATA8_SIZE
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_wram
	bcc	:+

	; error
	rts
:
	; ADDR0 = VRAM_TEXTURE_START + (vram_8bpp >> 1) * 4096, increment = 1
	stz	VERA_ADDRx_L
	lda	MAP_LOAD_P8
	lsr
	clc
	adc	#$0A
	tax
	lda	TAB_SWAP,x
	and	#$F0
	sta	VERA_ADDRx_M
	lda	TAB_SWAP,x
	and	#$0F
	ora	#%00010000
	sta	VERA_ADDRx_H

	; reset pointer
	lda	#$A0
	sta	@loop_lo+2

	; load texture data
	ldx	#16
@loop_hi:
	ldy	#0
@loop_lo:
	lda	$A000,y
	sta	VERA_DATA0
	iny
	bne	@loop_lo
	inc	@loop_lo+2
	dex
	bne	@loop_hi

	; copy effect
	lda	MAP_TEMP_PLANE8_EFFECT+0
	sta	MAP_TEMP_PLANE4_EFFECT+0
	lda	MAP_TEMP_PLANE8_EFFECT+1
	sta	MAP_TEMP_PLANE4_EFFECT+1
	lda	MAP_TEMP_PLANE8_EFFECT+2
	sta	MAP_TEMP_PLANE4_EFFECT+2
	lda	MAP_TEMP_PLANE8_EFFECT+3
	sta	MAP_TEMP_PLANE4_EFFECT+3

	;; new texture
	ldy	MAP_LOAD_TEX

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; name hash
	jsr	copy_name_hash

	; variant hash
	lda	#0
	sta	TEXINFO_STRUCT_VHASH_0,y
	sta	TEXINFO_STRUCT_VHASH_1,y
	sta	TEXINFO_STRUCT_VHASH_2,y
	sta	TEXINFO_STRUCT_VHASH_3,y

	; variant effect
	lda	MAP_TEMP_PLANE4_EFFECT+0
	sta	TEXINFO_STRUCT_EFFECT_0,y
	lda	MAP_TEMP_PLANE4_EFFECT+1
	sta	TEXINFO_STRUCT_EFFECT_1,y
	lda	MAP_TEMP_PLANE4_EFFECT+2
	sta	TEXINFO_STRUCT_EFFECT_2,y
	lda	MAP_TEMP_PLANE4_EFFECT+3
	sta	TEXINFO_STRUCT_EFFECT_3,y

	; type
	lda	#$FF
	sta	TEXINFO_STRUCT_TYPE,y

	; tile map base
	lda	MAP_PLANE_BASE
	sta	TEXINFO_STRUCT_P_TILE_MAP,y

	; tile data base
	clc
	lda	MAP_LOAD_P8
	adc	#VRAM_TEXTURE_START/2048
	asl
	asl
	sta	TEXINFO_STRUCT_P_TILE_DATA,y

	; done
	inc	MAP_LOAD_TEX
	rts

;;;
; generate colormap for 4bpp planes
;;;

gen_single_color:
	; get color
	and	#$0F
	tax
	lda	MAP_TEMP_PLANE4_COLORMAP,x
	tay

	; default to fullbright color
	sta	(COLORMAP_L)

	; check fullbright flag
	lda	MAP_TEMP_PLANE4_BRIGHTMAP,x
	bmi	:+

	; get light remap
	lda	(HI_PTR_L),y
	sta	MAP_LOAD_LIGHT_COL

	; set light remap
	lda	MAP_LOAD_LIGHT_COL
	sta	(COLORMAP_L)
:
	; next
	inc	COLORMAP_L

	rts

generate_cmap_16_a:
	stz	COLORMAP_L
@loop_main:
	; get index
	ldx	COLORMAP_L
	lda	TAB_SWAP,x

	; get color
	jsr	gen_single_color

	; next
	bne	@loop_main

	rts

generate_cmap_16_b:
	stz	COLORMAP_L
@loop_main:
	; get index
	lda	COLORMAP_L

	; get color
	jsr	gen_single_color

	; next
	bne	@loop_main

	rts

;;;
; load wall
;;;

load_wall:
	; map temp bank
	lda	#BANK_MAP_TEMP
	sta	REG_RAM_BANK

	; load pixel data (first half)
	stz	TAB_FILE_DEST+0
	lda	#$A0
	sta	TAB_FILE_DEST+1
	stz	TAB_FILE_BUF_SIZE+0
	lda	MAP_WALL_DLEN
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_wram
	bcc	:+

	; error
	rts
:
	; load pixel data (second half)
	stz	TAB_FILE_BUF_SIZE+0
	lda	MAP_WALL_DLEN
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_wram
	bcc	:+

	; error
	rts
:
	; check fullbright flag
	lda	MAP_WALL_VCNT
	bpl	:+

	; only white light info
	lda	#$01
	sta	MAP_TEMP_LIGHTS
:
	; copy light info
	lda	MAP_TEMP_LIGHTS
	sta	MAP_LOAD_TMP_LIGHTS

	; copy WRAM pointer
	lda	MAP_LOAD_WRAM_L
	sta	MAP_LOAD_TMP_L
	lda	MAP_LOAD_WRAM_H
	sta	MAP_LOAD_TMP_H

	; go trough lights
	stz	MAP_LOAD_XDI
@loop_light:
	; check if this light is used
	ror	MAP_LOAD_TMP_LIGHTS
	bcc	@skip_light

	; advance WRAM pointer (temp)
	lda	MAP_WALL_DLEN
	beq	:+
	clc
	lda	MAP_LOAD_TMP_L
	adc	MAP_WALL_DLEN
	sta	MATH_VAR_L
:
	lda	MAP_LOAD_TMP_H
	adc	#0
	sta	MATH_VAR_H

	; check WRAM space
	cmp	#$20
	bcc	:+

	; out of WRAM space, discard everything
	rts
:
	; convert data for this light
	jsr	convert_pixels

	; use advanced WRAM pointer
	lda	MATH_VAR_L
	sta	MAP_LOAD_TMP_L
	lda	MATH_VAR_H
	sta	MAP_LOAD_TMP_H

@skip_light:
	; next
	inc	MAP_LOAD_XDI
	lda	MAP_LOAD_XDI
	cmp	MAP_TEMP_COUNT_LIGHTS
	bne	@loop_light

	; copy texture idx
	lda	MAP_LOAD_TEX
	sta	MAP_LOAD_TEX_TMP

	; go trough variants
	lda	#$80
	trb	MAP_WALL_VCNT
@loop_variant:
	; check WRAM space
	lda	MAP_LOAD_TMP_H
	cmp	#$20
	bcc	:+

	; out of WRAM space, discard everything
	rts
:
	; check texture count
	lda	MAP_LOAD_TEX_TMP
	bpl	:+
	jmp	@end_variant
:
	; load variant info
	lda	#WALL_VARIANT_SIZE
	ldx	#<MAP_TEMP_WALL_VHASH_0
	ldy	#>MAP_TEMP_WALL_VHASH_0
	jsr	file_read_8_wram
	bcc	:+

	; read failed
	rts
:
	;; new texture
	ldy	MAP_LOAD_TEX_TMP

	; texture bank
	lda	#BANK_TEXTURES
	sta	REG_RAM_BANK

	; name hash
	jsr	copy_name_hash

	; variant hash
	lda	MAP_TEMP_WALL_VHASH_0
	sta	TEXINFO_STRUCT_VHASH_0,y
	lda	MAP_TEMP_WALL_VHASH_1
	sta	TEXINFO_STRUCT_VHASH_1,y
	lda	MAP_TEMP_WALL_VHASH_2
	sta	TEXINFO_STRUCT_VHASH_2,y
	lda	MAP_TEMP_WALL_VHASH_3
	sta	TEXINFO_STRUCT_VHASH_3,y

	; animation
	lda	#0
	sta	TEXINFO_STRUCT_EFFECT_0,y
	lda	MAP_TEMP_WALL_ANIM_C
	beq	:+

	; activate
	sta	TEXINFO_STRUCT_EFFECT_2,y
	lda	MAP_TEMP_WALL_ANIM_T
	sta	TEXINFO_STRUCT_EFFECT_1,y
	lda	MAP_TEMP_WALL_ANIM_O
	sta	TEXINFO_STRUCT_EFFECT_3,y
	lda	#4
	sta	TEXINFO_STRUCT_EFFECT_0,y
:
	; type + height
	lda	MAP_TEMP_WALL_HTYPE
	sta	TEXINFO_STRUCT_TYPE,y

	; store texture column
	lda	MAP_LOAD_TMP_L
	sta	TEXINFO_STRUCT_W_COLS_L,y
	lda	MAP_LOAD_TMP_H
	sta	TEXINFO_STRUCT_W_COLS_H,y

	; reset light destination pointers
	lda	#>TEXINFO_STRUCT_W_DATA_LH
	sta	HI_PTR_H
	sta	TEXTURE_PTR_H
	lda	#$80
	sta	TEXTURE_PTR_L

	; copy WRAM pointer (original)
	lda	MAP_LOAD_WRAM_L
	sta	MATH_VAR_L
	lda	MAP_LOAD_WRAM_H
	sta	MATH_VAR_H

	; copy light info
	lda	MAP_TEMP_LIGHTS
	sta	MAP_LOAD_TMP_LIGHTS

	; go trough lights
	ldx	#0
@loop_light_ptr:
	; store base light offset
	lda	MAP_LOAD_WRAM_L
	sta	(HI_PTR_L),y
	lda	MAP_LOAD_WRAM_H
	sta	(TEXTURE_PTR_L),y

	; check if this light is used
	ror	MAP_LOAD_TMP_LIGHTS
	bcc	@skip_light_ptr

	; store current light offset
	lda	MATH_VAR_L
	sta	(HI_PTR_L),y
	lda	MATH_VAR_H
	sta	(TEXTURE_PTR_L),y

	; advance WRAM pointer (temp)
	lda	MAP_WALL_DLEN
	beq	:+
	clc
	lda	MATH_VAR_L
	adc	MAP_WALL_DLEN
	sta	MATH_VAR_L
	bcc	:++
:
	inc	MATH_VAR_H
:
@skip_light_ptr:
	; increment light poiners
	inc	HI_PTR_H
	inc	TEXTURE_PTR_H

	; next
	inx
	cpx	MAP_TEMP_COUNT_LIGHTS
	bne	@loop_light_ptr

	; get destination pointer and bank
	lda	MAP_LOAD_TMP_H
	asl
	asl
	asl
	ldx	MAP_LOAD_TMP_L
	ora	TAB_BANK,x
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	MAP_LOAD_XDI

	; load columns (lo)
	tay
	ldx	#$00
	lda	#$80
	jsr	file_read_8_wram

	; load columns (hi)
	lda	#$80
	tax
	ldy	MAP_LOAD_XDI
	jsr	file_read_8_wram
	bcc	:+

	; read failed
	rts
:
	; advance WRAM pointer
	inc	MAP_LOAD_TMP_L
	bne	:+
	inc	MAP_LOAD_TMP_H
:
	; accept texture
	inc	MAP_LOAD_TEX_TMP

	; next
	dec	MAP_WALL_VCNT
	beq	@end_variant
	jmp	@loop_variant
@end_variant:

	; use new WRAM pointer
	lda	MAP_LOAD_TMP_L
	sta	MAP_LOAD_WRAM_L
	lda	MAP_LOAD_TMP_H
	sta	MAP_LOAD_WRAM_H

	; use new texture idx
	lda	MAP_LOAD_TEX_TMP
	sta	MAP_LOAD_TEX

	rts

;;;
; load sprite (thing or weapon)
;;;

load_sprite:
	phy

	; thing info bank
	lda	#BANK_THING_INFO
	sta	REG_RAM_BANK

	; generate sprite file name
	lda	THTYPE_SHASH_0,y
	sta	MAP_TEMP_NHASH_0
	lda	THTYPE_SHASH_1,y
	sta	MAP_TEMP_NHASH_1
	lda	THTYPE_SHASH_2,y
	sta	MAP_TEMP_NHASH_2
	lda	THTYPE_SHASH_3,y
	sta	MAP_TEMP_NHASH_3
	jsr	make_hash

	; check type
	ply
	cpy	TAB_THG_SPRLNK
	bcc	load_sprite_thg

	;; WEAPON SPRITE
load_sprite_wpn:

	; mark as loaded
	lda	MAP_TEMP_WPN_IDX
	sta	TAB_SPRITE_REMAP,y

	; suffix
	ldx	#(type_weapon-txt_types)
	jsr	copy_name_type

	; open sprite file
	lda	#EXT_FILE_HANDLE
	sta	TAB_FILE_HANDLE
	lda	#(file_name_end-file_name)
	ldx	#<file_name
	ldy	#>file_name
	jsr	file_open_rd

	; read header
	lda	#WSPR_HEAD_SIZE
	ldx	#<MAP_WALL_DLEN
	ldy	#>MAP_WALL_DLEN
	jsr	file_read_8_wram
	bcs	:+

	; actually load the sprite
	jsr	load_wspr
:
	; close file
	jsr	file_close

	; restore handle
	ldx	#MAP_FILE_HANDLE
	jsr	file_set_handle

	rts

	;; THING SPRITE
load_sprite_thg:

	; mark as loaded
	lda	MAP_TEMP_SPR_IDX
	sta	TAB_SPRITE_REMAP,y

	; suffix
	ldx	#(type_sprite-txt_types)
	jsr	copy_name_type

	; open sprite file
	lda	#EXT_FILE_HANDLE
	sta	TAB_FILE_HANDLE
	lda	#(file_name_end-file_name)
	ldx	#<file_name
	ldy	#>file_name
	jsr	file_open_rd

	; read header
	lda	#TSPR_HEAD_SIZE
	ldx	#<MAP_WALL_DLEN
	ldy	#>MAP_WALL_DLEN
	jsr	file_read_8_wram
	bcs	:+

	; actually load the sprite
	jsr	load_tspr
:
	; close file
	jsr	file_close

	; restore handle
	ldx	#MAP_FILE_HANDLE
	jsr	file_set_handle

	rts

;;;
; load thing sprite
;;;

load_tspr:
	; check frame count
	lda	MAP_TEMP_SPR_IDX
	ina
	bne	:+

	; too many frames
	rts
:
	; check image count
	lda	MAP_TEMP_IMG_IDX_H
	cmp	#4
	bcc	:+

	; too many images
	rts
:
	jsr	load_sprite_pixels
	bcc	:+

	; error
	rts
:
	; top_frame = 0
	stz	MAP_LOAD_TOP_FRAME

	; go trough variants
	lda	MAP_WALL_VCNT
	sta	MAP_LOAD_VIDX
@loop_variant:
	; check WRAM space
	lda	MAP_LOAD_TMP_H
	cmp	#$20
	bcc	:+

	; out of WRAM space
	rts
:
	; load variant info
	lda	#TSPR_VARIANT_SIZE
	ldx	#<MAP_TEMP_SPRITE_FRM
	ldy	#>MAP_TEMP_SPRITE_FRM
	jsr	file_read_8_wram
	bcc	:+

	; read failed
	rts
:
	; tsprite info bank
	lda	#BANK_TSPR_INFO
	sta	REG_RAM_BANK

	; if(info->frm > top_frame)
	lda	MAP_TEMP_SPRITE_FRM
	cmp	MAP_LOAD_TOP_FRAME
	bcc	:+

	; top_frame = info->frm
	sta	MAP_LOAD_TOP_FRAME
:
	; destination sprite
	clc
	adc	MAP_TEMP_SPR_IDX
	tay

	; limit check
	ina
	bne	:+

	; too many sprites
	rts
:
	; if(info->rot)
	lda	MAP_TEMP_SPRITE_ROT
	and	#$07
	beq	:+

	; si->rotate = 1
	sta	SPRINFO_STRUCT_ROTATE,y
:
	; destination rotation
	ora	#$A0
	sta	HI_PTR_H

	; save rotation frame
	lda	MAP_TEMP_IMG_IDX_L
	sta	(HI_PTR_L),y

	; save rotation bank
	lda	#$08
	tsb	HI_PTR_H
	lda	MAP_TEMP_IMG_IDX_H
	ora	#BANK_SPRITE_GFX
	sta	(HI_PTR_L),y

	; sprite image bank
	sta	REG_RAM_BANK

	; get sprite frame
	ldx	MAP_TEMP_IMG_IDX_L

	; frm->width = info->width
	lda	MAP_TEMP_SPRITE_WIDTH
	sta	SPRFRM_STRUCT_WIDTH,x

	; frm->height = info->height
	lda	MAP_TEMP_SPRITE_HEIGHT
	sta	SPRFRM_STRUCT_HEIGHT,x

	; frm->ox = info->ox
	lda	MAP_TEMP_SPRITE_OX
	sta	SPRFRM_STRUCT_OX,x

	; frm->oy = info->oy
	lda	MAP_TEMP_SPRITE_OY
	sta	SPRFRM_STRUCT_OY,x

	; frm->offs_cols = offs_cols
	lda	MAP_LOAD_TMP_L
	sta	SPRFRM_STRUCT_OFFS_COLS_L,x
	lda	MAP_LOAD_TMP_H
	sta	SPRFRM_STRUCT_OFFS_COLS_H,x

	; frm->offs_data = offset
	lda	MAP_LOAD_WRAM_L
	sta	SPRFRM_STRUCT_OFFS_DATA_L,x
	lda	MAP_LOAD_WRAM_H
	sta	SPRFRM_STRUCT_OFFS_DATA_H,x

	; num_sframes++
	inc	MAP_TEMP_IMG_IDX_L
	bne	:+
	inc	MAP_TEMP_IMG_IDX_H
:
	;; load columns

	; get destination pointer and bank
	lda	MAP_LOAD_TMP_H
	asl
	asl
	asl
	ldx	MAP_LOAD_TMP_L
	ora	TAB_BANK,x
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	MAP_LOAD_XDI

	; load columns (lo)
	tay
	ldx	#$00
	lda	#$80
	jsr	file_read_8_wram

	; load columns (hi)
	lda	#$80
	ldx	#$80
	ldy	MAP_LOAD_XDI
	jsr	file_read_8_wram
	bcc	:+

	; read failed
	rts
:
	; advance WRAM pointer
	inc	MAP_LOAD_TMP_L
	bne	:+
	inc	MAP_LOAD_TMP_H
:
	; next variant
	dec	MAP_LOAD_VIDX
	beq	:+
	jmp	@loop_variant
:
	; num_sprites += top_frame + 1
	sec
	lda	MAP_TEMP_SPR_IDX
	adc	MAP_LOAD_TOP_FRAME
	sta	MAP_TEMP_SPR_IDX

	; use new WRAM pointer
	lda	MAP_LOAD_TMP_L
	sta	MAP_LOAD_WRAM_L
	lda	MAP_LOAD_TMP_H
	sta	MAP_LOAD_WRAM_H

	rts

;;;
; load weapon sprite
;;;

load_wspr:
	; check frame count
	lda	MAP_TEMP_WPN_IDX
	bmi	@error

	; top_frame = 0
	stz	MAP_LOAD_TOP_FRAME

	; load pixels
	jsr	load_sprite_pixels
	bcc	:+
@error:
	rts
:
	; wsprite frame bank
	lda	#BANK_WSPR_INFO
	sta	REG_RAM_BANK

	; go trough variants
@loop_variant:
	; read part count
	jsr	k_ACPTR
	ina
	asl
	asl
	pha

	; read parts
	ldx	#<MAP_TEMP_DATA_STRUCT
	ldy	#>MAP_TEMP_DATA_STRUCT
	jsr	file_read_8_wram
	pla
	bcs	@error

	; mark last part
	tay
	lda	#$FF
	sta	MAP_TEMP_DATA_STRUCT,y

	; limit part bytes to copy
	cpy	#64
	bcc	:+
	dey
:
	; if(info->frm > top_frame)
	lda	MAP_TEMP_DATA_STRUCT+1
	cmp	MAP_LOAD_TOP_FRAME
	bcc	:+

	; top_frame = info->frm
	sta	MAP_LOAD_TOP_FRAME
:
	; destination pointer
	clc
	adc	MAP_TEMP_WPN_IDX
	bmi	@error
	sta	TMP_PTR_H
	stz	TMP_PTR_L
	lsr	TMP_PTR_H
	ror	TMP_PTR_L
	lsr	TMP_PTR_H
	ror	TMP_PTR_L
	lda	#$A0
	tsb	TMP_PTR_H

	; update data start
	clc
	lda	MAP_TEMP_DATA_STRUCT+0
	adc	MAP_LOAD_WRAM_L
	sta	MAP_TEMP_DATA_STRUCT+0
	lda	#0
	adc	MAP_LOAD_WRAM_H
	sta	MAP_TEMP_DATA_STRUCT+1

	; copy to WRAM
@loop_copy:
	lda	MAP_TEMP_DATA_STRUCT,y
	sta	(TMP_PTR_L),y
	dey
	bpl	@loop_copy

	; next
	dec	MAP_WALL_VCNT
	bne	@loop_variant

	; add frames
	lda	MAP_LOAD_TOP_FRAME
	ina
	clc
	adc	MAP_TEMP_WPN_IDX
	sta	MAP_TEMP_WPN_IDX

	; use new WRAM pointer
	lda	MAP_LOAD_TMP_L
	sta	MAP_LOAD_WRAM_L
	lda	MAP_LOAD_TMP_H
	sta	MAP_LOAD_WRAM_H

	rts

;;;
; load pixels from sprite
;;;

load_sprite_pixels:
	; advance WRAM pointer (temp)
	lda	MAP_WALL_DLEN
	beq	:+
	clc
	lda	MAP_LOAD_WRAM_L
	adc	MAP_WALL_DLEN
	sta	MAP_LOAD_TMP_L
:
	lda	MAP_LOAD_WRAM_H
	adc	#0
	sta	MAP_LOAD_TMP_H

	; check WRAM space
	cmp	#$20
	bcc	:+

	; error
	rts
:
	; get destination pointer
	lda	MAP_LOAD_WRAM_H
	asl
	asl
	asl
	ldx	MAP_LOAD_WRAM_L
	ora	TAB_BANK,x
	sta	REG_RAM_BANK
	txa
	and	#$1F
	ora	#$A0
	sta	TAB_FILE_DEST+1

	; load pixel data
	stz	TAB_FILE_DEST+0
	stz	TAB_FILE_BUF_SIZE+0
	lda	MAP_WALL_DLEN
	sta	TAB_FILE_BUF_SIZE+1
	jsr	file_read_16_wram

	rts

;;;
; convert wall or sprite pixels
;;;
; MAP_LOAD_XDI is light number
; MAP_WALL_DLEN is number of blocks (0 is 256)
; MAP_LOAD_TMP_* is destination WRAM block
; source data are loaded at BANK_MAP_TEMP

convert_pixels:
	; get light pointer
	clc
	lda	MAP_LOAD_XDI
	adc	#>TAB_LIGHTMAPS
	sta	PCONV_PTR_H

	; get source pointer
	lda	#BANK_MAP_TEMP
	sta	TMP_PTR_I
	lda	#$A0
	sta	TMP_PTR_H

	; get destination pointer
	lda	MAP_LOAD_TMP_H
	asl
	asl
	asl
	ldx	MAP_LOAD_TMP_L
	ora	TAB_BANK,x
	sta	COLORMAP_B
	txa
	and	#$1F
	ora	#$A0
	sta	COLORMAP_H

	; loop trough blocks
	ldx	MAP_WALL_DLEN
@loop_block:
	; loop trough range
	ldy	#0
@loop_range:
	; source bank
	lda	TMP_PTR_I
	sta	REG_RAM_BANK

	; load pixel color
	lda	(TMP_PTR_L)
	sta	PCONV_PTR_L

	; load pixel brightness
	inc	TMP_PTR_L
	lda	(TMP_PTR_L)
	bne	@skip_remap

	; get light remap
	lda	(PCONV_PTR_L)
	sta	PCONV_PTR_L

@skip_remap:
	; destination bank
	lda	COLORMAP_B
	sta	REG_RAM_BANK

	; store pixel
	lda	PCONV_PTR_L
	sta	(COLORMAP_L),y

	; increment source
	inc	TMP_PTR_L
	bne	:+
	inc	TMP_PTR_H
	lda	TMP_PTR_H
	cmp	#$C0
	bcc	:+
	inc	TMP_PTR_I
	lda	#$A0
	sta	TMP_PTR_H
:
	; next (range)
	iny
	bne	@loop_range

	; increment destination
	inc	COLORMAP_H
	lda	COLORMAP_H
	cmp	#$C0
	bcc	:+
	inc	COLORMAP_B
	lda	#$A0
	sta	COLORMAP_H
:
	; next (block)
	dex
	bne	@loop_block

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
; set file suffix
;;;

copy_name_type:
	lda	txt_types,x
	sta	file_name+14
	lda	txt_types+1,x
	sta	file_name+15
	lda	txt_types+2,x
	sta	file_name+16
	rts

;;;
; copy texture name hash
;;;

copy_name_hash:
	lda	MAP_TEMP_NHASH_0
	sta	TEXINFO_STRUCT_NHASH_0,y
	lda	MAP_TEMP_NHASH_1
	sta	TEXINFO_STRUCT_NHASH_1,y
	lda	MAP_TEMP_NHASH_2
	sta	TEXINFO_STRUCT_NHASH_2,y
	lda	MAP_TEMP_NHASH_3
	sta	TEXINFO_STRUCT_NHASH_3,y
	rts

;;;
; make hash
;;;
; turn uint32_t at TAB_TEMP_DATA into file name

make_hash:
	ldy	#0
	ldx	#3
@loop:
	lda	TAB_TEMP_DATA,x
	lsr
	lsr
	lsr
	lsr
	jsr	get_hex
	sta	file_name+5,y
	iny

	lda	TAB_TEMP_DATA,x
	and	#$0F
	jsr	get_hex
	sta	file_name+5,y
	iny

	dex
	bpl	@loop

	rts

;;;
; map loading error
;;;
; this function restores stack pointer - use 'jmp'
; called when map loading fails
; A is error code

load_error:
	; TODO
	sta DBG_BYTE1
	bra *

	; restore stack pointer
	ldx	TAB_TEMP_STACK
	txs

	; close all files
	lda	#EXT_FILE_HANDLE
	jsr	file_close_raw
	lda	#MAP_FILE_HANDLE
	jsr	file_close_raw

	rts

;;;
; strings
;;;

map_magic:
	.byte	"kgMapX16"

txt_types:
type_light:
	.byte	"LIT"
type_plane:
	.byte	"PLN"
type_wall:
	.byte	"WAL"
type_sprite:
	.byte	"THS"
type_weapon:
	.byte	"WPS"
type_sky:
	.byte	"SKY"

file_name:
	.byte	"DATA/F8845BD5.WPS"
file_name_end:

;;; TESTING ONLY

filename_map:
	.byte	"DATA/DEFAULT.MAP"
filename_map_end:

