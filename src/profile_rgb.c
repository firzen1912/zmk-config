/*
 * Automatic Bluetooth-profile RGB and connection-state indication for ZMK.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <dt-bindings/zmk/rgb.h>
#include <zmk/behavior.h>
#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/position_state_changed.h>

LOG_MODULE_REGISTER(helix_profile_rgb, CONFIG_ZMK_LOG_LEVEL);

#define RGB_EFFECT_SOLID 0
#define RGB_EFFECT_BREATHE 1
#define RGB_MIN_ANIMATION_SPEED 1
#define RGB_MAX_ANIMATION_SPEED 5

struct profile_color {
    uint16_t hue;
    uint8_t saturation;
    uint8_t brightness;
};

/*
 * ZMK HSB values: hue 0-359 degrees, saturation/brightness 0-100 percent.
 * Blue and red use pure-primary hues with identical saturation and brightness,
 * so the red channel peak for profile 2 equals the blue channel peak for
 * profile 1.
 */
static const struct profile_color profile_colors[] = {
    {0, 0, 35},     /* Profile 0: white */
    {240, 100, 35}, /* Profile 1: pure blue */
    {0, 100, 35},   /* Profile 2: pure red; same intensity as profile 1 blue */
    {285, 100, 35}, /* Profile 3: purple */
    {120, 100, 35}, /* Profile 4: green */
};

static int invoke_rgb_behavior(uint32_t command, uint32_t value) {
    const struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(rgb_ug)),
        .param1 = command,
        .param2 = value,
    };

    const struct zmk_behavior_binding_event event = {
        .layer = 0,
        .position = 0,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    return zmk_behavior_invoke_binding(&binding, event, true);
}

static int force_slowest_animation_speed(void) {
    /*
     * ZMK exposes relative speed commands but no absolute speed command.
     * Four decrements force every valid speed (1-5) to the minimum value.
     */
    for (int i = RGB_MIN_ANIMATION_SPEED; i < RGB_MAX_ANIMATION_SPEED; i++) {
        const int err = invoke_rgb_behavior(RGB_SPD_CMD, 0);
        if (err < 0) {
            LOG_ERR("Failed to reduce RGB animation speed (%d)", err);
            return err;
        }
    }

    return 0;
}

static int apply_profile_indicator(uint8_t profile_index) {
    if (profile_index >= ARRAY_SIZE(profile_colors)) {
        LOG_WRN("Bluetooth profile index %u is outside the color table", profile_index);
        return -ERANGE;
    }

    const struct profile_color *color = &profile_colors[profile_index];
    const bool connected = zmk_ble_active_profile_is_connected();
    const uint8_t effect = connected ? RGB_EFFECT_SOLID : RGB_EFFECT_BREATHE;
    int err;

    if (!connected) {
        err = force_slowest_animation_speed();
        if (err < 0) {
            return err;
        }
    }

    err = invoke_rgb_behavior(
        RGB_COLOR_HSB_CMD,
        RGB_COLOR_HSB_VAL(color->hue, color->saturation, color->brightness));
    if (err < 0) {
        LOG_ERR("Failed to set RGB color for profile %u (%d)", profile_index, err);
        return err;
    }

    err = invoke_rgb_behavior(RGB_EFS_CMD, effect);
    if (err < 0) {
        LOG_ERR("Failed to select RGB effect %u (%d)", effect, err);
        return err;
    }

    err = invoke_rgb_behavior(RGB_ON_CMD, 0);
    if (err < 0) {
        LOG_ERR("Failed to turn RGB underglow on (%d)", err);
        return err;
    }

    LOG_INF("Bluetooth profile %u: HSB %u/%u/%u, %s", profile_index, color->hue,
            color->saturation, color->brightness, connected ? "connected/solid" : "waiting/fading");
    return 0;
}

static int profile_changed_listener(const zmk_event_t *event_header) {
    const struct zmk_ble_active_profile_changed *event =
        as_zmk_ble_active_profile_changed(event_header);

    if (event != NULL) {
        /* This event is raised for profile selection and connection changes. */
        (void)apply_profile_indicator(event->index);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(helix_profile_rgb, profile_changed_listener);
ZMK_SUBSCRIPTION(helix_profile_rgb, zmk_ble_active_profile_changed);

static void initial_profile_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    const int profile_index = zmk_ble_active_profile_index();
    if (profile_index >= 0) {
        (void)apply_profile_indicator((uint8_t)profile_index);
    } else {
        LOG_WRN("Unable to read active Bluetooth profile (%d)", profile_index);
    }
}

K_WORK_DELAYABLE_DEFINE(initial_profile_work, initial_profile_work_handler);

static int helix_profile_rgb_init(void) {
    /* Let BLE, split transport, and the LED strip finish initializing first. */
    (void)k_work_schedule(&initial_profile_work, K_MSEC(1500));
    return 0;
}

SYS_INIT(helix_profile_rgb_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
