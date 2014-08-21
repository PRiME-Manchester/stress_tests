#include "spin1_api.h"
#define TIMER_TICK_PERIOD 10000

void app_init(void);

void c_main()
{
	spin1_set_timer_tick(TIMER_TICK_PERIOD);

	app_init();
}

void app_init(void)	
{
	uint i;

	i = 0;
	while(1)
	{
		i = i+1;
		if (i==10000)
			i=0;
	}

}