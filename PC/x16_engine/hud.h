
typedef struct
{
	// order is defined in game ASM and UI buttons
	union
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
	} menu_color;
	union
	{
		uint8_t raw[4];
		struct
		{
			uint8_t info;
			uint8_t msg;
			uint8_t hp;
			uint8_t ammo;
		};
	} stat_color;
	struct
	{
		uint8_t info;
		uint8_t bar_style;
		uint8_t bar_pos[2];
	} stat_cfg;
} hud_cfg_t;

//

extern hud_cfg_t hud_cfg;

//

void hud_draw();
