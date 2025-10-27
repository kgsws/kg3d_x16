
; code
BANK_EXTRA_CODE = 1
BANK_SWAP_CODE = 2	; [2 banks]

; things
; holds 128 slots, slot 0 is used for local player weapon
BANK_TICKER_STRUCT = 4

; things in sectors
; 256 sectors, each can hold 31 things
; last slot is reserved for thing count
BANK_SECTOR_THINGS = 5

; thing info and states
; these are loaded from a single file
BANK_THING_INFO = 6
BANK_THING_STATES = 7

; palette
BANK_PALETTE = 8

; sprites
BANK_DRAW_SPRITES = 9

; masked walls
BANK_DRAW_MASKED = 10

; graphics
BANK_TEXTURES = 11
BANK_TSPR_INFO = 12
BANK_WSPR_INFO = 13
BANK_SPRITE_FRM = 14

; sectors
BANK_SECTORS = 15

; map data (walls)
BANK_MAPDATA = 16	; [16 banks] must be divisible by 16

; map data (other)
BANK_PLAYER_START = BANK_MAPDATA + 0

; sectors in things (in map data)
; index 0 to 127 is used for 'thing id' [thingsec]
; index 128 to 255 is used for 'offset in table above' [thingces]
BANK_THING_SECTORS = BANK_MAPDATA + 15

; graphics loading
BANK_MAP_TEMP = BANK_MAPDATA	; [16 banks]

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

;;;; BANK 40+ contains texture data

