/* Application-level state shared between main and the shell. */
#ifndef SNOWGAUGE_APP_H
#define SNOWGAUGE_APP_H

#include <stdint.h>

void app_set_auto_period(uint32_t seconds);
uint32_t app_get_auto_period(void);

#endif /* SNOWGAUGE_APP_H */
