/*
 * Battery voltage via the 1M + 1M divider on A0.
 *
 * PCB v1.2 hangs the divider on VBAT_SW, so the sensor rail must be on
 * while reading (the node sits at 0 V otherwise). Vbat = 2 x node voltage.
 */
#ifndef SNOWGAUGE_BATTERY_H
#define SNOWGAUGE_BATTERY_H

#include <stdint.h>

int battery_init(void);

/*
 * Read the battery voltage in mV. Returns 0, -EBUSY if the sensor rail is
 * off, or a negative errno from the ADC.
 */
int battery_read_mv(uint16_t *vbat_mv);

#endif /* SNOWGAUGE_BATTERY_H */
