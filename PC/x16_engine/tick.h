
#define NUM_TICKERS	128

enum
{
	// this is direct map to THMODE_*
	TICKER_FUNC_DUMMY = 0,
	TICKER_FUNC_ANIM = 2,
	TICKER_FUNC_MOVE = 4,
	TICKER_FUNC_THING = 6,
	TICKER_FUNC_PLAYER = 8,
	//
	TFUNC_FREE_SLOT = 0xFF,
};

typedef struct
{
	uint8_t func;
	uint8_t next;
	uint8_t prev;
	uint8_t type;
} ticker_base_t;

typedef struct
{
	ticker_base_t base;
	uint8_t buf[64 - sizeof(ticker_base_t)];
} ticker_dummy_t;

//

extern ticker_dummy_t ticker[NUM_TICKERS];
extern uint8_t tick_idx;

//

void tick_clear();
void tick_run();
uint32_t tick_add();
void tick_del(uint32_t idx);

//

static inline uint8_t ticker_idx(void *ptr)
{
	return (ticker_dummy_t*)ptr - ticker;
}
