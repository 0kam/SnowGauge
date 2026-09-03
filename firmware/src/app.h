/* Application-level state shared between main and the shell. */
#ifndef SNOWGAUGE_APP_H
#define SNOWGAUGE_APP_H

#include <stdint.h>
#include <stdbool.h>

#include "measure.h"
#include "record.h"

void app_set_auto_period(uint32_t seconds);
uint32_t app_get_auto_period(void);

/*
 * Run one measurement cycle and store it as a record (LittleFS + mirror).
 * manual = true marks shell/BLE-triggered records. *m / *r may be NULL.
 * Returns the storage result (0 = stored) or the measurement error.
 */
int app_measure_and_store(bool manual, struct measurement *m, struct record *r);

#endif /* SNOWGAUGE_APP_H */
