
; code
BANK_EXTRA_CODE = 1
BANK_SWAP_CODE = 2	; [2 banks]

; thing info
BANK_THING_INFO = 4

; loaded from GFX
BAMK_GFX_INFO = 5
BANK_PALETTE = 6

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

; visibility table
BANK_VISTAB = 14

; MAP DATA overlap
; walls are loaded in up to 16 banks
; walls use only 4096 bytes per bank (first half)
; second half of each bank is used for other stuff

; map walls
BANK_MAPWALLS = 16	; [16 banks] must be divisible by 16; this is used as a bit in 'poscheck.htype'

; player starts
BANK_PLAYER_START = BANK_MAPWALLS + 0	; loaded from map file

; sectors in things
; index 0 to 127 is used for 'thing id' [thingsec]
; index 128 to 255 is used for 'offset in sector things table' [thingces]
BANK_THING_SECTORS = BANK_MAPWALLS + 7

; thing states
BANK_THING_STATES = BANK_MAPWALLS + 8	; [8 banks] must be divisible by 8

; tables T1
BANK_TABLES_T1 = 32
BANK_RCODE = BANK_TABLES_T1 + 0
BANK_IDIV_L = BANK_TABLES_T1 + 1
BANK_TSCALE = BANK_TABLES_T1 + 2
BANK_TAN = BANK_TABLES_T1 + 3
BANK_YDEPTH_L = BANK_TABLES_T1 + 3
BANK_ATAN = BANK_TABLES_T1 + 4
BANK_A2X = BANK_TABLES_T1 + 5
BANK_RANDOM = BANK_TABLES_T1 + 5
BANK_PLANE_X = BANK_TABLES_T1 + 5
BANK_Y_CENTER = BANK_TABLES_T1 + 5
BANK_VIDOFFS = BANK_TABLES_T1 + 5
BANK_PRINTINT = BANK_TABLES_T1 + 5

; REMOVE
BANK_PORTALS = BANK_TABLES_T1 + 5

;;;; BANK 38+ contains game data

