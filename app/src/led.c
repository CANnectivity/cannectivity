/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
#include <zephyr/sys_clock.h>

#include "cannectivity.h"

LOG_MODULE_REGISTER(led, CONFIG_CANNECTIVITY_LOG_LEVEL);

/* LED ticks */
#define LED_TICK_MS        50U
#define LED_TICKS_ACTIVITY 2U
#define LED_TICKS_IDENTIFY 10U

/* LED finite-state machine states */
enum led_state {
	LED_STATE_NORMAL,
	LED_STATE_NORMAL_STARTED,
	LED_STATE_NORMAL_STOPPED,
	LED_STATE_NORMAL_ACTIVITY,
	LED_STATE_IDENTIFY,
};

/* LED finite-state machine events */
enum led_event {
	LED_EVENT_TICK,
	LED_EVENT_CHANNEL_IDENTIFY_ON,
	LED_EVENT_CHANNEL_IDENTIFY_OFF,
	LED_EVENT_CHANNEL_STARTED,
	LED_EVENT_CHANNEL_STOPPED,
	LED_EVENT_CHANNEL_ACTIVITY,
};

/* LED finite-state machine event data type */
typedef uint32_t led_event_t;

/* LED finite-state machine per-channel context */
struct led_ctx {
	struct smf_ctx ctx;
	char eventq_buf[sizeof(led_event_t) * CONFIG_CANNECTIVITY_LED_EVENT_MSGQ_SIZE];
	struct k_msgq eventq;
	led_event_t event;
	uint16_t ch;
	k_timepoint_t activity;
	int ticks;
	bool started;
	struct gpio_dt_spec state_led;
	struct gpio_dt_spec activity_led;
};

/* Devicetree accessor macros */
#define CHANNEL_LED_GPIO_DT_SPEC_GET(node_id)                                                      \
	{                                                                                          \
		.state_led = GPIO_DT_SPEC_GET_OR(node_id, state_gpios, {0}),                       \
		.activity_led = GPIO_DT_SPEC_GET_OR(node_id, activity_gpios, {0}),                 \
	}

#define CHANNEL_LED0_GPIO_DT_SPEC_GET()                                                            \
	{                                                                                          \
		.state_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),                              \
	}

/* Array of LED finite-state machine channel contexts */
struct led_ctx led_channel_ctx[] = {
#if CANNECTIVITY_DT_HAS_CHANNEL
	CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(CHANNEL_LED_GPIO_DT_SPEC_GET, (,))
#else /* CANNECTIVITY_DT_HAS_CHANNEL */
	CHANNEL_LED0_GPIO_DT_SPEC_GET()
#endif /* !CANNECTIVITY_DT_HAS_CHANNEL */
};

/* Forward declarations */
static const struct smf_state led_states[];
static void led_tick(struct k_timer *timer);

/* Variables */
static K_TIMER_DEFINE(led_tick_timer, led_tick, NULL);
static struct k_thread led_thread_data;
static K_THREAD_STACK_DEFINE(led_thread_stack, CONFIG_CANNECTIVITY_LED_THREAD_STACK_SIZE);
struct k_poll_event led_poll_events[ARRAY_SIZE(led_channel_ctx)];

BUILD_ASSERT(ARRAY_SIZE(led_channel_ctx) == ARRAY_SIZE(led_poll_events));

static int led_event_enqueue(uint16_t ch, led_event_t event)
{
	struct led_ctx *lctx;
	int err;

	if (ch >= ARRAY_SIZE(led_channel_ctx)) {
		LOG_ERR("event %u for non-existing channel %u", event, ch);
		return -EINVAL;
	}

	lctx = &led_channel_ctx[ch];
	err = k_msgq_put(&lctx->eventq, &event, K_NO_WAIT);
	if (err != 0) {
		LOG_ERR("failed to enqueue event %u for channel %u (err %d)", event, ch, err);
	}

	return 0;
}

static void led_set(struct led_ctx *lctx, bool state, bool activity)
{
	int err;

	if (lctx->state_led.port != NULL) {
		err = gpio_pin_set_dt(&lctx->state_led, state);
		if (err != 0) {
			LOG_ERR("failed to turn %s channel %u state LED (err %d)",
				state ? "on" : "off", lctx->ch, err);
		}
	}

	if (lctx->activity_led.port != NULL) {
		err = gpio_pin_set_dt(&lctx->activity_led, activity);
		if (err != 0) {
			LOG_ERR("failed to turn %s channel %u activity LED (err %d)",
				activity ? "on" : "off", lctx->ch, err);
		}
	}
}

static void led_toggle(struct led_ctx *lctx)
{
	int err;

	if (lctx->state_led.port != NULL) {
		err = gpio_pin_toggle_dt(&lctx->state_led);
		if (err != 0) {
			LOG_ERR("failed to toggle channel %u state LED (err %d)", lctx->ch, err);
		}
	}

	if (lctx->activity_led.port != NULL) {
		err = gpio_pin_toggle_dt(&lctx->activity_led);
		if (err != 0) {
			LOG_ERR("failed to toggle channel %u activity LED (err %d)", lctx->ch, err);
		}
	}
}

static void led_state_normal_run(void *obj)
{
	struct led_ctx *lctx = obj;

	switch (lctx->event) {
	case LED_EVENT_CHANNEL_IDENTIFY_ON:
		smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_IDENTIFY]);
		break;
	default:
		/* Event ignored */
	};
}

static void led_state_normal_stopped_entry(void *obj)
{
	struct led_ctx *lctx = obj;

	if (lctx->started) {
		smf_set_state(SMF_CTX(obj), &led_states[LED_STATE_NORMAL_STARTED]);
		return;
	}

	led_set(lctx, false, false);
}

static void led_state_normal_stopped_run(void *obj)
{
	struct led_ctx *lctx = obj;

	switch (lctx->event) {
	case LED_EVENT_TICK:
		smf_set_handled(SMF_CTX(lctx));
		break;
	case LED_EVENT_CHANNEL_STARTED:
		lctx->started = true;
		smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_NORMAL_STARTED]);
		break;
	default:
		/* Event ignored */
	};
}

static void led_state_normal_started_entry(void *obj)
{
	struct led_ctx *lctx = obj;

	led_set(lctx, true, false);
}

static void led_state_normal_started_run(void *obj)
{
	struct led_ctx *lctx = obj;

	switch (lctx->event) {
	case LED_EVENT_TICK:
		smf_set_handled(SMF_CTX(lctx));
		break;
	case LED_EVENT_CHANNEL_STOPPED:
		lctx->started = false;
		smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_NORMAL_STOPPED]);
		break;
	case LED_EVENT_CHANNEL_ACTIVITY:
		smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_NORMAL_ACTIVITY]);
		break;
	default:
		/* Event ignored */
	};
}

static void led_state_normal_activity_entry(void *obj)
{
	struct led_ctx *lctx = obj;

	lctx->ticks = LED_TICKS_ACTIVITY;
}

static void led_state_normal_activity_run(void *obj)
{
	struct led_ctx *lctx = obj;

	switch (lctx->event) {
	case LED_EVENT_TICK:
		lctx->ticks--;

		if (lctx->ticks == (LED_TICKS_ACTIVITY / 2U)) {
			if (lctx->activity_led.port != NULL) {
				led_set(lctx, true, true);
			} else {
				led_set(lctx, false, false);
			}

			smf_set_handled(SMF_CTX(lctx));
		} else if (lctx->ticks == 0U) {
			smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_NORMAL]);
		}
		break;
	case LED_EVENT_CHANNEL_STARTED:
		lctx->started = true;
		break;
	case LED_EVENT_CHANNEL_STOPPED:
		lctx->started = false;
		break;
	default:
		/* Event ignored */
	}
}

static void led_state_identify_entry(void *obj)
{
	struct led_ctx *lctx = obj;

	lctx->ticks = LED_TICKS_IDENTIFY;

	led_set(lctx, true, false);
}

static void led_state_identify_run(void *obj)
{
	struct led_ctx *lctx = obj;

	switch (lctx->event) {
	case LED_EVENT_TICK:
		if (--lctx->ticks == 0U) {
			led_toggle(lctx);
			lctx->ticks = LED_TICKS_IDENTIFY;
		}
		break;
	case LED_EVENT_CHANNEL_STARTED:
		lctx->started = true;
		break;
	case LED_EVENT_CHANNEL_STOPPED:
		lctx->started = false;
		break;
	case LED_EVENT_CHANNEL_IDENTIFY_OFF:
		smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_NORMAL]);
		break;
	default:
		/* Event ignored */
	};
}

/* clang-format off */
static const struct smf_state led_states[] = {
	[LED_STATE_NORMAL] = SMF_CREATE_STATE(NULL,
					      led_state_normal_run,
					      NULL,
					      NULL,
					      &led_states[LED_STATE_NORMAL_STOPPED]),
	[LED_STATE_NORMAL_STOPPED] = SMF_CREATE_STATE(led_state_normal_stopped_entry,
						      led_state_normal_stopped_run,
						      NULL,
						      &led_states[LED_STATE_NORMAL],
						      NULL),
	[LED_STATE_NORMAL_STARTED] = SMF_CREATE_STATE(led_state_normal_started_entry,
						      led_state_normal_started_run,
						      NULL,
						      &led_states[LED_STATE_NORMAL],
						      NULL),
	[LED_STATE_NORMAL_ACTIVITY] = SMF_CREATE_STATE(led_state_normal_activity_entry,
						       led_state_normal_activity_run,
						       NULL,
						       &led_states[LED_STATE_NORMAL],
						       NULL),
	[LED_STATE_IDENTIFY] = SMF_CREATE_STATE(led_state_identify_entry,
						led_state_identify_run,
						NULL,
						NULL,
						NULL),
};
/* clang-format on */

static void led_tick(struct k_timer *timer)
{
	uint16_t ch;
	int err;

	ARG_UNUSED(timer);

	for (ch = 0; ch < ARRAY_SIZE(led_channel_ctx); ch++) {
		err = led_event_enqueue(ch, LED_EVENT_TICK);
		if (err != 0) {
			LOG_WRN("failed to enqueue LED tick event for channel %u (err %d)", ch,
				err);
		}
	}
}

int cannectivity_led_identify(const struct device *dev, uint16_t ch, bool identify, void *user_data)
{
	led_event_t event;
	int err;

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_DBG("identify channel %u %s", ch, identify ? "on" : "off");

	if (identify) {
		event = LED_EVENT_CHANNEL_IDENTIFY_ON;
	} else {
		event = LED_EVENT_CHANNEL_IDENTIFY_OFF;
	}

	err = led_event_enqueue(ch, event);
	if (err != 0) {
		LOG_ERR("failed to enqueue identify %s event for channel %u (err %d)",
			event == LED_EVENT_CHANNEL_IDENTIFY_ON ? "on" : "off", ch, err);
	}

	return 0;
}

int cannectivity_led_state(const struct device *dev, uint16_t ch, bool started, void *user_data)
{
	led_event_t event;
	int err;

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_DBG("channel %u %s", ch, started ? "started" : "stopped");

	if (started) {
		event = LED_EVENT_CHANNEL_STARTED;
	} else {
		event = LED_EVENT_CHANNEL_STOPPED;
	}

	err = led_event_enqueue(ch, event);
	if (err != 0) {
		LOG_ERR("failed to enqueue channel %s event for channel %u (err %d)",
			event == LED_EVENT_CHANNEL_STOPPED ? "stopped" : "started", ch, err);
	}

	return 0;
}

int cannectivity_led_activity(const struct device *dev, uint16_t ch, void *user_data)
{
	struct led_ctx *lctx;
	int err;

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (ch >= ARRAY_SIZE(led_channel_ctx)) {
		LOG_ERR("activity event for non-existing channel %u", ch);
		return -EINVAL;
	}

	lctx = &led_channel_ctx[ch];

	/* low-pass filter activity events */
	if (sys_timepoint_expired(lctx->activity)) {
		lctx->activity = sys_timepoint_calc(K_MSEC(LED_TICK_MS * LED_TICKS_ACTIVITY));

		err = led_event_enqueue(ch, LED_EVENT_CHANNEL_ACTIVITY);
		if (err != 0) {
			LOG_ERR("failed to enqueue channel activity event for channel %u (err %d)",
				ch, err);
		}
	}

	return 0;
}

static void led_thread(void *arg1, void *arg2, void *arg3)
{
	struct led_ctx *lctx;
	led_event_t event;
	uint16_t ch;
	int err;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	for (ch = 0; ch < ARRAY_SIZE(led_channel_ctx); ch++) {
		lctx = &led_channel_ctx[ch];

		smf_set_initial(SMF_CTX(lctx), &led_states[LED_STATE_NORMAL]);
	}

	while (true) {
		err = k_poll(led_poll_events, ARRAY_SIZE(led_poll_events), K_FOREVER);
		if (err == 0) {
			for (ch = 0; ch < ARRAY_SIZE(led_poll_events); ch++) {
				lctx = &led_channel_ctx[ch];

				if (led_poll_events[ch].state != K_POLL_STATE_MSGQ_DATA_AVAILABLE) {
					continue;
				}

				err = k_msgq_get(&lctx->eventq, &event, K_NO_WAIT);
				if (err == 0) {
					lctx->event = event;

					err = smf_run_state(SMF_CTX(lctx));
					if (err != 0) {
						break;
					}
				}

				led_poll_events[ch].state = K_POLL_STATE_NOT_READY;
			}
		}
	}

	LOG_ERR("LED finite-state machine terminated (err %d)", err);
}

int cannectivity_led_init(void)
{
	struct led_ctx *lctx;
	uint16_t ch;
	int err;

	for (ch = 0; ch < ARRAY_SIZE(led_channel_ctx); ch++) {
		lctx = &led_channel_ctx[ch];

		lctx->ch = ch;
		lctx->activity = sys_timepoint_calc(K_NO_WAIT);

		if (lctx->state_led.port != NULL) {
			if (!gpio_is_ready_dt(&lctx->state_led)) {
				LOG_ERR("state LED for channel %u not ready", ch);
				return -ENODEV;
			}

			err = gpio_pin_configure_dt(&lctx->state_led, GPIO_OUTPUT_INACTIVE);
			if (err != 0) {
				LOG_ERR("failed to configure channel %d state LED GPIO (err %d)",
					ch, err);
				return err;
			}
		}

		if (lctx->activity_led.port != NULL) {
			if (!gpio_is_ready_dt(&lctx->activity_led)) {
				LOG_ERR("activity LED for channel %u not ready", ch);
				return -ENODEV;
			}

			err = gpio_pin_configure_dt(&lctx->activity_led, GPIO_OUTPUT_INACTIVE);
			if (err != 0) {
				LOG_ERR("failed to configure channel %d activity LED GPIO (err %d)",
					ch, err);
				return err;
			}
		}

		k_msgq_init(&lctx->eventq, lctx->eventq_buf, sizeof(led_event_t),
			    CONFIG_CANNECTIVITY_LED_EVENT_MSGQ_SIZE);

		k_poll_event_init(&led_poll_events[ch], K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
				  K_POLL_MODE_NOTIFY_ONLY, &lctx->eventq);
	}

	k_thread_create(&led_thread_data, led_thread_stack, K_THREAD_STACK_SIZEOF(led_thread_stack),
			led_thread, NULL, NULL, NULL, CONFIG_CANNECTIVITY_LED_THREAD_PRIO, 0,
			K_NO_WAIT);
	k_thread_name_set(&led_thread_data, "led");

	k_timer_start(&led_tick_timer, K_MSEC(LED_TICK_MS), K_MSEC(LED_TICK_MS));

	return 0;
}
