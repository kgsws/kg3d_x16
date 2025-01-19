
; code
BANK_EXTRA_CODE = 1
BANK_SWAP_CODE = 2

; math
BANK_TABLES = 59
BANK_IDIV_L = 59
BANK_TSCALE = 60
BANK_TAN = 61
BANK_YDEPTH_L = 61
BANK_ATAN = 62
BANK_A2X = 63
BANK_RANDOM = 63
BANK_PLANE_X = 63
BANK_Y_CENTER = 63

; sector portals
BANK_PORTALS = 63

; thing info and states
; these are loaded from a single file
BANK_THING_INFO = 57
BANK_THING_STATES = 58

; palette
BANK_PALETTE = 56

; sprites
BANK_DRAW_SPRITES = 55

; masked walls
BANK_DRAW_MASKED = 54

; player starts
BANK_PLAYER_START = BANK_THING_STATES

; things
; holds 256 thing slots, slot 0 is invalid
BANK_THING_STRUCT = 53
BANK_THING_POS = BANK_TSPR_INFO

; things in sectors
BANK_SECTOR_THINGS = 52

; sectors in things
BANK_THING_SECTORS = 51

; loaded map stuff
BANK_MAPDATA = 48	; [4 banks] must be divisible by 4
BANK_TEXTURES = 39
BANK_TSPR_INFO = 38
BANK_WSPR_INFO = 37
BANK_SPRITE_FRM = 36
BANK_SECTORS = 35

; map loading
BANK_MAP_TEMP = 16	; [16 banks] must be divisible by 16
