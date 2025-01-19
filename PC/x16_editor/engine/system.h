
// DEBUG
#define HAX_VIDEO_WIDTH	1024
#define HAX_VIDEO_HEIGHT	768
#define FAR_PLANE	16384.0f

//

extern uint32_t screen_width;
extern uint32_t screen_height;
extern float screen_aspect;
extern float screen_scale[2];

extern uint32_t gametick;
extern uint_fast8_t system_was_busy;

extern void (*system_draw)();
extern void (*system_input)();
extern void (*system_tick)(uint32_t);
extern void (*system_text)(uint8_t*,uint32_t);

//

int system_init(int argc, void **argv);
void system_deinit();
void system_finish_draw();
int system_loop();
void system_mouse_grab(int grab);
void system_clear_events();
void system_stop(uint32_t code);

// DEBUG
uint64_t get_app_tick();
