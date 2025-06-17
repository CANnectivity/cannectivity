/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
#include <zephyr/sys_clock.h>

#include "cannectivity.h"

LOG_MODULE_REGISTER(led, CONFIG_CANNECTIVITY_LOG_LEVEL);

/* LED ticks */
#define LED_TICK_MS        50U
#define LED_TICKS_ACTIVITY 2U
#define LED_TICKS_IDENTIFY 20U

/* LED finite-state machine states */
enum led_state {
	LED_STATE_NORMAL,
	LED_STATE_NORMAL_STARTED,
	LED_STATE_NORMAL_STOPPED,
	LED_STATE_IDENTIFY,
};

/* LED finite-state machine events */
enum led_event {
	LED_EVENT_TICK,
	LED_EVENT_CHANNEL_IDENTIFY_ON,
	LED_EVENT_CHANNEL_IDENTIFY_OFF,
	LED_EVENT_CHANNEL_STARTED,
	LED_EVENT_CHANNEL_STOPPED,
	LED_EVENT_CHANNEL_ACTIVITY_RX,
	LED_EVENT_CHANNEL_ACTIVITY_TX,
};

/* Activity types */
enum led_activity {
	LED_ACTIVITY_RX = 0U,
	LED_ACTIVITY_TX,
	LED_ACTIVITY_COUNT,
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
	bool started;
	struct led_dt_spec state_led;
	int identify_ticks;
	k_timepoint_t activity[LED_ACTIVITY_COUNT];
	int ticks[LED_ACTIVITY_COUNT];
	struct led_dt_spec activity_led[LED_ACTIVITY_COUNT];
};

/* Devicetree accessor macros */
#define CHANNEL_LED_DT_SPEC_GET(node_id)                                                           \
	{                                                                                          \
		.state_led = LED_DT_SPEC_GET_OR(DT_PHANDLE(node_id, state_led), {0}),              \
		.activity_led[0] = LED_DT_SPEC_GET_OR(DT_PHANDLE_BY_IDX(node_id, activity_leds, 0),\
						      {0}),                                        \
		.activity_led[1] = LED_DT_SPEC_GET_OR(DT_PHANDLE_BY_IDX(node_id, activity_leds, 1),\
						      {0}),                                        \
	}

#define CHANNEL_LED0_DT_SPEC_GET()                                                                 \
	{                                                                                          \
		.state_led = LED_DT_SPEC_GET(DT_ALIAS(led0)),                                      \
	}

/* Array of LED finite-state machine channel contexts */
struct led_ctx led_channel_ctx[] = {
#if CANNECTIVITY_DT_HAS_CHANNEL
	CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(CHANNEL_LED_DT_SPEC_GET, (,))
#else /* CANNECTIVITY_DT_HAS_CHANNEL */
	CHANNEL_LED0_DT_SPEC_GET()
#endif /* !CANNECTIVITY_DT_HAS_CHANNEL */
};

/* Helper macros */
#define LED_CTX_HAS_STATE_LED(_led_ctx)                                                            \
	(_led_ctx->state_led.dev != NULL)
#define LED_CTX_HAS_ACTIVITY_LED(_led_ctx)                                                         \
	(_led_ctx->activity_led[LED_ACTIVITY_RX].dev != NULL)
#define LED_CTX_HAS_DUAL_ACTIVITY_LEDS(_led_ctx)                                                   \
	(_led_ctx->activity_led[LED_ACTIVITY_TX].dev != NULL)

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

static void led_indicate_state(struct led_ctx *lctx, bool state)
{
	int err;

	if (LED_CTX_HAS_STATE_LED(lctx)) {
		if (state) {
			err = led_on_dt(&lctx->state_led);
		} else {
			err = led_off_dt(&lctx->state_led);
		}
		if (err != 0) {
			LOG_ERR("failed to turn %s channel %u state LED (err %d)",
				state ? "on" : "off", lctx->ch, err);
		}
	}
}

static void led_indicate_activity(struct led_ctx *lctx, enum led_activity type, bool activity)
{
	struct led_dt_spec *led = NULL;
	int value = activity;
	int err;

	switch (type) {
	case LED_ACTIVITY_RX:
		if (LED_CTX_HAS_ACTIVITY_LED(lctx)) {
			led = &lctx->activity_led[LED_ACTIVITY_RX];
		}
		break;

	case LED_ACTIVITY_TX:
		if (LED_CTX_HAS_DUAL_ACTIVITY_LEDS(lctx)) {
			led = &lctx->activity_led[LED_ACTIVITY_TX];
		} else if (LED_CTX_HAS_ACTIVITY_LED(lctx)) {
			led = &lctx->activity_led[LED_ACTIVITY_RX];
		}

		break;

	default:
		__ASSERT_NO_MSG(false);
		break;
	}

	if (led == NULL && lctx->started && LED_CTX_HAS_STATE_LED(lctx)) {
		led = &lctx->state_led;
		value = !value;
	}

	if (led != NULL) {
		if (value) {
			err = led_on_dt(led);
		} else {
			err = led_off_dt(led);
		}
		if (err != 0) {
			LOG_ERR("failed to turn %s channel %u activity LED (err %d)",
				value ? "on" : "off", lctx->ch, err);
		}
	}
}

static enum smf_state_result led_state_normal_run(void *obj)
{
	struct led_ctx *lctx = obj;

	switch (lctx->event) {
	case LED_EVENT_CHANNEL_IDENTIFY_ON:
		smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_IDENTIFY]);
		break;
	default:
		return SMF_EVENT_PROPAGATE;
	}

	return SMF_EVENT_HANDLED;
}

static void led_state_normal_stopped_entry(void *obj)
{
	struct led_ctx *lctx = obj;

	if (lctx->started) {
		smf_set_state(SMF_CTX(obj), &led_states[LED_STATE_NORMAL_STARTED]);
		return;
	}

	led_indicate_state(lctx, false);
	led_indicate_activity(lctx, LED_ACTIVITY_RX, false);
	led_indicate_activity(lctx, LED_ACTIVITY_TX, false);
}

static enum smf_state_result led_state_normal_stopped_run(void *obj)
{
	struct led_ctx *lctx = obj;

	switch (lctx->event) {
	case LED_EVENT_CHANNEL_STARTED:
		lctx->started = true;
		smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_NORMAL_STARTED]);
		break;
	default:
		return SMF_EVENT_PROPAGATE;
	}

	return SMF_EVENT_HANDLED;
}

static void led_state_normal_started_entry(void *obj)
{
	struct led_ctx *lctx = obj;

	led_indicate_state(lctx, true);
	led_indicate_activity(lctx, LED_ACTIVITY_RX, false);
	led_indicate_activity(lctx, LED_ACTIVITY_TX, false);
}

static enum smf_state_result led_state_normal_started_run(void *obj)
{
	struct led_ctx *lctx = obj;
	int i;

	switch (lctx->event) {
	case LED_EVENT_TICK:
		for (i = 0; i < ARRAY_SIZE(lctx->ticks); i++) {
			if (lctx->ticks[i] != 0U) {
				lctx->ticks[i]--;
				if (lctx->ticks[i] == (LED_TICKS_ACTIVITY / 2U)) {
					led_indicate_activity(lctx, i, true);
				} else if (lctx->ticks[i] == 0U) {
					led_indicate_activity(lctx, i, false);
				}
			}
		}
		break;
	case LED_EVENT_CHANNEL_STOPPED:
		lctx->started = false;
		smf_set_state(SMF_CTX(lctx), &led_states[LED_STATE_NORMAL_STOPPED]);
		break;
	case LED_EVENT_CHANNEL_ACTIVITY_TX:
		lctx->ticks[LED_ACTIVITY_TX] = LED_TICKS_ACTIVITY;
		break;
	case LED_EVENT_CHANNEL_ACTIVITY_RX:
		lctx->ticks[LED_ACTIVITY_RX] = LED_TICKS_ACTIVITY;
		break;
	default:
		return SMF_EVENT_PROPAGATE;
	}

	return SMF_EVENT_HANDLED;
}

static void led_state_identify_entry(void *obj)
{
	struct led_ctx *lctx = obj;

	lctx->identify_ticks = LED_TICKS_IDENTIFY;

	led_indicate_state(lctx, true);
	led_indicate_activity(lctx, LED_ACTIVITY_RX, true);
	led_indicate_activity(lctx, LED_ACTIVITY_TX, true);
}

static enum smf_state_result led_state_identify_run(void *obj)
{
	struct led_ctx *lctx = obj;
	struct led_dt_spec *leds[3];
	int err;
	int i;

	switch (lctx->event) {
	case LED_EVENT_TICK:
		leds[0] = &lctx->state_led;
		leds[1] = &lctx->activity_led[LED_ACTIVITY_RX];
		leds[2] = &lctx->activity_led[LED_ACTIVITY_TX];

		lctx->identify_ticks--;

		if (lctx->identify_ticks == LED_TICKS_IDENTIFY / 2U) {
			for (i = 0; i < ARRAY_SIZE(leds); i++) {
				if (leds[i]->dev != NULL) {
					err = led_off_dt(leds[i]);
					if (err != 0) {
						LOG_ERR("failed to turn channel %u LED %d off"
							"(err %d)", lctx->ch, i, err);
					}
				}
			}
		} else if (lctx->identify_ticks == 0U) {
			for (i = 0; i < ARRAY_SIZE(leds); i++) {
				if (leds[i]->dev != NULL) {
					err = led_on_dt(leds[i]);
					if (err != 0) {
						LOG_ERR("failed to turn channel %u LED %d on "
							"(err %d)", lctx->ch, i, err);
					}
				}
			}

			lctx->identify_ticks = LED_TICKS_IDENTIFY;
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
		return SMF_EVENT_PROPAGATE;
	}

	return SMF_EVENT_HANDLED;
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

int cannectivity_led_event(const struct device *dev, uint16_t ch, enum gs_usb_event event,
			   void *user_data)
{
	led_event_t led_event;
	struct led_ctx *lctx;
	int err;

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (ch >= ARRAY_SIZE(led_channel_ctx)) {
		LOG_ERR("event for non-existing channel %u", ch);
		return -EINVAL;
	}

	lctx = &led_channel_ctx[ch];

	switch (event) {
	case GS_USB_EVENT_CHANNEL_STARTED:
		LOG_DBG("channel %u started", ch);
		led_event = LED_EVENT_CHANNEL_STARTED;
		break;
	case GS_USB_EVENT_CHANNEL_STOPPED:
		LOG_DBG("channel %u stopped", ch);
		led_event = LED_EVENT_CHANNEL_STOPPED;
		break;
	case GS_USB_EVENT_CHANNEL_ACTIVITY_RX:
		__fallthrough;
	case GS_USB_EVENT_CHANNEL_ACTIVITY_TX:
		/* Low-pass filtering of RX/TX activity events is combined if no TX LED */
		led_event = LED_EVENT_CHANNEL_ACTIVITY_RX;
		int idx = LED_ACTIVITY_RX;

		if (event == GS_USB_EVENT_CHANNEL_ACTIVITY_TX) {
			led_event = LED_EVENT_CHANNEL_ACTIVITY_TX;

			if (LED_CTX_HAS_DUAL_ACTIVITY_LEDS(lctx)) {
				idx = LED_ACTIVITY_TX;
			}
		}

		if (!sys_timepoint_expired(lctx->activity[idx])) {
			goto skipped;
		}

		lctx->activity[idx] = sys_timepoint_calc(K_MSEC(LED_TICK_MS * LED_TICKS_ACTIVITY));
		break;
	case GS_USB_EVENT_CHANNEL_IDENTIFY_ON:
		LOG_DBG("identify channel %u on", ch);
		led_event = LED_EVENT_CHANNEL_IDENTIFY_ON;
		break;
	case GS_USB_EVENT_CHANNEL_IDENTIFY_OFF:
		LOG_DBG("identify channel %u off", ch);
		led_event = LED_EVENT_CHANNEL_IDENTIFY_OFF;
		break;
	default:
		/* Unsupported event */
		goto skipped;
	}

	err = led_event_enqueue(ch, led_event);
	if (err != 0) {
		LOG_ERR("failed to enqueue LED event for channel %u (err %d)", ch, err);
	}

skipped:
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
	int i;

	for (ch = 0; ch < ARRAY_SIZE(led_channel_ctx); ch++) {
		lctx = &led_channel_ctx[ch];
		lctx->ch = ch;

		for (i = 0; i < ARRAY_SIZE(lctx->activity); i++) {
			lctx->activity[i] = sys_timepoint_calc(K_NO_WAIT);
		}

		if (LED_CTX_HAS_STATE_LED(lctx)) {
			if (!led_is_ready_dt(&lctx->state_led)) {
				LOG_ERR("state LED for channel %u not ready", ch);
				return -ENODEV;
			}
		}

		for (i = 0; i < ARRAY_SIZE(lctx->activity_led); i++) {
			if (lctx->activity_led[i].dev != NULL) {
				if (!led_is_ready_dt(&lctx->activity_led[i])) {
					LOG_ERR("activity LED %d for channel %u not ready", i, ch);
					return -ENODEV;
				}
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
