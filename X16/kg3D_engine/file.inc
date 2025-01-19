
.import file_load_wram
.import file_open_rd
.import file_close
.import file_read_8_wram
.import file_read_16_wram
.import file_read_16_vram
.import file_restore_handle

; X is file handle
file_set_handle = k_CHKIN

; A is file handle
file_close_raw = k_CLOSE
