/* Low-power housekeeping: park unused peripherals, status LED. */
#ifndef SNOWGAUGE_POWER_H
#define SNOWGAUGE_POWER_H

#include <stdint.h>

/* Put the QSPI flash into deep power-down etc. Call once at boot. */
int power_init(void);

/* Short green LED pulse (blocking). Costs ~1 mA for the duration only. */
void led_pulse(uint32_t ms);

#endif /* SNOWGAUGE_POWER_H */
