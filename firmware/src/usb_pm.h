/* USB device enable/disable driven by VBUS presence (saves ~1.8 mA when unplugged). */
#ifndef SNOWGAUGE_USB_PM_H
#define SNOWGAUGE_USB_PM_H

int usb_pm_init(void);

#endif /* SNOWGAUGE_USB_PM_H */
