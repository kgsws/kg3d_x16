
#define TICK_MAX_MOVE	32767

//

extern uint32_t leveltime;

extern struct kge_ticcmd_s tick_local_cmd;

extern struct linked_list_s tick_list_normal;

//

struct linked_list_s;

//

void tick_make_cmd(kge_ticcmd_t *cmd, kge_thing_t *th);
kge_ticker_t *tick_create(uint32_t, struct linked_list_s*);
void tick_cleanup();
void tick_go();

