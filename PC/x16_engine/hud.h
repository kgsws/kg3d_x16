
typedef union
{
	uint8_t raw[6];
	struct
	{
		uint8_t title;
		uint8_t item;
		uint8_t group;
		uint8_t select;
		uint8_t extra;
		uint8_t exsel;
	};
} hud_menu_color_t;

typedef union
{
	uint8_t raw[4];
	struct
	{
		uint8_t info;
		uint8_t msg;
		uint8_t hp;
		uint8_t ammo;
	};
} hud_stat_color_t;

typedef struct
{
	// order is defined in game ASM and UI buttons
	hud_menu_color_t menu_color;
	hud_stat_color_t stat_color;
	struct
	{
		uint8_t info_x;
		uint8_t info_y;
		uint8_t hp_x[3];
		uint8_t am_x[3];
		uint8_t bar_y;
		uint8_t space;
		uint8_t digits;
	} style;
} hud_export_t;

//

extern hud_export_t hud_info;
extern uint32_t hud_update;

//

void hud_draw();
