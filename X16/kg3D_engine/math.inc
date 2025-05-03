
.import math_setup_vera
.import math_scale_down
.import math_shift_down
.import	math_idiv
.import	math_idiv_ns
.import	math_atan
.import math_p2a

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

.macro  math_lsr4_u16	src_l, src_h
	ldx	src_l
	lda	TAB_SWAP,x
	and	#$0F
	sta	MATH_TEMP
	ldx	src_h
	lda	TAB_SWAP,x
	and	#$F0
	ora	MATH_TEMP
.endmacro

