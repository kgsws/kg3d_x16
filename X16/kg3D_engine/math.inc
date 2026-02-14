
.import math_setup_vera
.import math_scale_down
.import math_shift_down
.import	math_idiv
.import	math_idiv_ns
.import	math_atan
.import math_p2a
.import math_p2d
.import math_ang8
.import math_u16
.import math_intclip
.import math_txtint

.macro  math_read_sin	dest_l, dest_h
	lda	TAB_SIN_L,x
	sta	dest_l
	lda	TAB_SIN_H,x
	sta	dest_h
.endmacro

.macro  math_read_cos	dest_l, dest_h
	lda	TAB_COS_L,x
	sta	dest_l
	lda	TAB_COS_H,x
	sta	dest_h
.endmacro

.macro  math_read_x2a	dest_l, dest_h
	lda	TAB_X2A_L,x
	sta	dest_l
	lda	TAB_X2A_H,x
	sta	dest_h
.endmacro

.macro  math_read_htan	dest_l, dest_h
	lda	TAB_HTAN_L,x
	sta	dest_l
	lda	TAB_HTAN_H,x
	sta	dest_h
.endmacro
