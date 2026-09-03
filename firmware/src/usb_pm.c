/*
 * The board's CDC ACM console (subsys/usb/device_next/app/cdc_acm_serial.c)
 * registers and initialises the USB device at boot. Enabling it, however,
 * makes udc_nrf request the HFXO and switch the USBD peripheral on
 * regardless of VBUS, which costs ~1.8 mA forever when running on battery.
 * CONFIG_CDC_ACM_SERIAL_ENABLE_AT_BOOT is therefore off and this module
 * enables the device only while VBUS is present.
 */

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>
#include <hal/nrf_power.h>
#include <errno.h>

#include "usb_pm.h"

LOG_MODULE_REGISTER(usb_pm, CONFIG_LOG_DEFAULT_LEVEL);

static struct usbd_context *usb_ctx;

static void usb_msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
	int ret;

	switch (msg->type) {
	case USBD_MSG_VBUS_READY:
		ret = usbd_enable(ctx);
		if (ret && ret != -EALREADY) {
			LOG_ERR("usbd_enable failed (%d)", ret);
		}
		break;
	case USBD_MSG_VBUS_REMOVED:
		ret = usbd_disable(ctx);
		if (ret && ret != -EALREADY) {
			LOG_ERR("usbd_disable failed (%d)", ret);
		}
		break;
	default:
		break;
	}
}

int usb_pm_init(void)
{
	int ret;

	/* The board's context is file-static; pick it up from its section. */
	STRUCT_SECTION_FOREACH(usbd_context, ctx) {
		usb_ctx = ctx;
		break;
	}
	if (usb_ctx == NULL) {
		LOG_ERR("no USB device context");
		return -ENODEV;
	}

	ret = usbd_msg_register_cb(usb_ctx, usb_msg_cb);
	if (ret) {
		LOG_ERR("usbd_msg_register_cb failed (%d)", ret);
		return ret;
	}

	/* VBUS may already be present at boot; the READY event was sent
	 * before our callback existed, so check the hardware directly.
	 */
	if (nrf_power_usbregstatus_vbusdet_get(NRF_POWER)) {
		ret = usbd_enable(usb_ctx);
		if (ret && ret != -EALREADY) {
			LOG_ERR("usbd_enable at boot failed (%d)", ret);
			return ret;
		}
	}
	return 0;
}
