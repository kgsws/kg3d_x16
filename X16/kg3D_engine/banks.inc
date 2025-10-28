
; code
BANK_EXTRA_CODE = 1
BANK_SWAP_CODE = 2	; [2 banks]

; thing info
BANK_THING_INFO = 4

; palette
BANK_PALETTE = 5

; graphics
; these also contain one-time pre-loaded stuff
BANK_TSPR_INFO = 6
BANK_WSPR_INFO = 7

; GRAPHICS overlap
; map loading requires up to 16 banks of temp data
; this is only needed before map layout is loaded
; 8 banks overlap run-time game data
; 8 banks overlap map layout data
BANK_MAP_TEMP = 8	; [16 banks]

; things
; holds 128 slots, slot 0 is used for local player weapon
BANK_TICKER_STRUCT = 8

; things in sectors
; 256 sectors, each can hold 31 things
; last slot is reserved for thing count
BANK_SECTOR_THINGS = 9

; render sprites
BANK_DRAW_SPRITES = 10

; render masked walls
BANK_DRAW_MASKED = 11

; map sectors
BANK_SECTORS = 12

; textures
BANK_TEXTURES = 13

;; 2 game banks available here

; MAP DATA overlap
; walls are loaded in up to 16 banks
; walls use only 4096 bytes per bank (first half)
; second half of each bank is used for other stuff

; map walls
BANK_MAPWALLS = 16	; [16 banks] must be divisible by 16

; player starts
BANK_PLAYER_START = BANK_MAPWALLS + 0	; loaded from map file

; sectors in things
; index 0 to 127 is used for 'thing id' [thingsec]
; index 128 to 255 is used for 'offset in table above' [thingces]
BANK_THING_SECTORS = BANK_MAPWALLS + 7

; thing states
BANK_THING_STATES = BANK_MAPWALLS + 8	; [8 banks] must be divisible by 8, overlaps with 'sprite images'

; sprite images
BANK_SPRITE_GFX = BANK_MAPWALLS + 12	; [4 banks] must be divisible by 4, overlaps with 'thing states'

; math
BANK_TABLES = 32
BANK_IDIV_L = 32
BANK_TSCALE = 33
BANK_TAN = 34
BANK_YDEPTH_L = 34
BANK_ATAN = 35
BANK_A2X = 36
BANK_RANDOM = 36
BANK_PLANE_X = 36
BANK_Y_CENTER = 36

; video
BANK_VIDOFFS = 36

; print decimal value
BANK_PRINTINT = 36

; sector portals
BANK_PORTALS = 36

;;;; BANK 37+ contains game data

