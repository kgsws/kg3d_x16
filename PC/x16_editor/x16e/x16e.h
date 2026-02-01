
#define X16_DEFAULT_NAME	"default"

#define X16_DIR_DATA	"data"
#define X16_DIR_DEFS	"defs"
#define X16_DIR_MAPS	"maps"
#define X16_DIR_EXPORT	"export"

#define X16_PATH_DEFS	X16_DIR_DATA PATH_SPLIT_STR X16_DIR_DEFS
#define X16_PATH_MAPS	X16_DIR_DATA PATH_SPLIT_STR X16_DIR_MAPS
#define X16_PATH_EXPORT	X16_DIR_DATA PATH_SPLIT_STR X16_DIR_EXPORT

//

extern uint_fast8_t x16e_enable_logo;
extern uint8_t x16e_logo_data[160 * 120];

//

uint32_t x16_init();
uint32_t x16e_line_angle(float y, float x);
void x16_export_map();

