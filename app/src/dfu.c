/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys_clock.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "cannectivity.h"

LOG_MODULE_REGISTER(dfu, CONFIG_CANNECTIVITY_LOG_LEVEL);

/* DFU button poll timing */
#define DFU_BUTTON_POLL_HZ 5
#define DFU_BUTTON_POLL_INTERVAL_MS (MSEC_PER_SEC / DFU_BUTTON_POLL_HZ)
#define DFU_BUTTON_POLL_TOTAL (CONFIG_CANNECTIVITY_DFU_BUTTON_HOLD_TIME * DFU_BUTTON_POLL_HZ)

/* DFU button and LED devicetree nodes */
#define DFU_LED_NODE DT_ALIAS(mcuboot_led0)
#define DFU_BUTTON_NODE DT_ALIAS(mcuboot_button0)

#ifdef CONFIG_CANNECTIVITY_DFU_LED
struct led_dt_spec dfu_led = LED_DT_SPEC_GET(DFU_LED_NODE);
#endif /* CONFIG_CANNECTIVITY_DFU_LED */

#ifdef CONFIG_CANNECTIVITY_DFU_BUTTON
struct gpio_dt_spec dfu_button = GPIO_DT_SPEC_GET(DFU_BUTTON_NODE, gpios);
static struct gpio_callback dfu_button_cb;
static struct k_work_delayable dfu_button_work;
static struct k_sem dfu_button_sem;

static void dfu_button_poll(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	int err;

	err = gpio_pin_get_dt(&dfu_button);
	if (err < 0) {
		LOG_ERR("failed to get DFU button state (err %d)", err);
		goto done;
	}

	if (err > 0) {
#ifdef CONFIG_CANNECTIVITY_DFU_LED
		if (k_sem_count_get(&dfu_button_sem) % 2U == 0U) {
			err = led_on_dt(&dfu_led);
		} else {
			err = led_off_dt(&dfu_led);
		}
		if (err != 0) {
			LOG_ERR("failed to toggle DFU LED (err %d)", err);
			goto done;
		}
#endif /* CONFIG_CANNECTIVITY_DFU_LED */

		k_sem_give(&dfu_button_sem);
		if (k_sem_count_get(&dfu_button_sem) >= DFU_BUTTON_POLL_TOTAL) {
			LOG_INF("rebooting");
			sys_reboot(SYS_REBOOT_COLD);
		}

		k_work_reschedule(dwork, K_MSEC(DFU_BUTTON_POLL_INTERVAL_MS));
		return;
	}

done:
#ifdef CONFIG_CANNECTIVITY_DFU_LED
	err = led_off_dt(&dfu_led);
	if (err != 0) {
		LOG_ERR("failed to turn off DFU LED (err %d)", err);
		return;
	}
#endif /* CONFIG_CANNECTIVITY_DFU_LED */
}

static void dfu_button_interrupt(const struct device *port, struct gpio_callback *cb,
				 gpio_port_pins_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_sem_reset(&dfu_button_sem);
	k_work_reschedule(&dfu_button_work, K_NO_WAIT);
}

static int dfu_button_init(void)
{
	int err;

	err = k_sem_init(&dfu_button_sem, 0, K_SEM_MAX_LIMIT);
	if (err != 0) {
		LOG_ERR("failed to initialize DFU button semaphore (err %d)", err);
		return err;
	}

	if (!gpio_is_ready_dt(&dfu_button)) {
		LOG_ERR("DFU button device not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&dfu_button, GPIO_INPUT);
	if (err != 0) {
		LOG_ERR("failed to configure DFU button (err %d)", err);
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&dfu_button, GPIO_INT_EDGE_TO_ACTIVE);
	if (err != 0) {
		LOG_ERR("failed to configure DFU button interrupt (err %d)", err);
		return err;
	}

	k_work_init_delayable(&dfu_button_work, dfu_button_poll);

	gpio_init_callback(&dfu_button_cb, dfu_button_interrupt, BIT(dfu_button.pin));
	err = gpio_add_callback_dt(&dfu_button, &dfu_button_cb);
	if (err != 0) {
		LOG_ERR("failed to add DFU button callback (err %d)", err);
		return err;
	}

	return 0;
}
#endif /* CONFIG_CANNECTIVITY_DFU_BUTTON */

#ifdef CONFIG_CANNECTIVITY_DFU_LED
static int dfu_led_init(void)
{
	if (!led_is_ready_dt(&dfu_led)) {
		LOG_ERR("DFU LED device not ready");
		return -ENODEV;
	}

	return 0;
}
#endif /* CONFIG_CANNECTIVITY_DFU_LED */

int cannectivity_dfu_init(void)
{
	int err;

	/*
	 * Confirm updated image if running under MCUboot booloader. This could be done on
	 * successful USB enumeration instead, but that could cause unwanted image reverts on
	 * e.g. self-powered development boards.
	 */
	if (!boot_is_img_confirmed()) {
		err = boot_write_img_confirmed();
		if (err != 0) {
			LOG_ERR("failed to confirm image (err %d)", err);
			return err;
		}

		LOG_INF("image confirmed");
	}

#ifdef CONFIG_CANNECTIVITY_DFU_LED
	err = dfu_led_init();
	if (err != 0) {
		return err;
	}
#endif /* CONFIG_CANNECTIVITY_DFU_LED */

#ifdef CONFIG_CANNECTIVITY_DFU_BUTTON
	err = dfu_button_init();
	if (err != 0) {
		return err;
	}
#endif /* CONFIG_CANNECTIVITY_DFU_BUTTON */

	return 0;
}
