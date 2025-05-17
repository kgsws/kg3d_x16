
;; functions

k_i2c_batch_read = $FEB4
k_i2c_batch_write = $FEB7

k_BSAVE = $FEBA

k_i2c_read_byte = $FEC6
k_i2c_write_byte = $FEC9

k_MACPTR = $FF44

k_joystick_scan = $FF53
k_joystick_get = $FF56

k_screen_set_charset = $FF62

k_mouse_config = $FF68

k_PRIMM = $FF7D

k_MEMTOP = $FF99

k_SCNKEY = $FF9F

k_ACPTR = $FFA5

k_extapi = $FEAB

k_CHROUT = $FFD2

k_LOAD = $FFD5

k_GETIN = $FFE4

k_SETLFS = $FFBA

k_SETNAM = $FFBD

k_OPEN = $FFC0
k_CLOSE = $FFC3

k_CHKIN = $FFC6

;; extended

e_ps2data_fetch = $08
e_ps2data_raw = $09

;; variables

k_CINV = $0314
k_KEYHDL = $032E

;; debugger

.macro  dbg
	.byte	$DB
.endmacro

.macro	dbg_framerate
	lda	$9FB8
	sta	DBG_BYTE0
	lda	$9FB9
	sta	DBG_BYTE0
	lda	$9FBA
	sta	DBG_BYTE1
	stz	$9FB8
.endmacro

.macro	dbg_rate_clr
	stz	$9FB8
.endmacro

DBG_BYTE0 = $9FB9
DBG_BYTE1 = $9FBA
DBG_VERA = $9FB1

