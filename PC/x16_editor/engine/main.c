#include "inc.h"
#include "system.h"

int main(int argc, void **argv)
{
	if(system_init(argc, argv))
		return 1;

	argc = system_loop();

	system_deinit();

	return argc;
}

