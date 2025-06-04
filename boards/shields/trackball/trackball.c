#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>

#define DT_DRV_COMPAT gpio_keys

#define ACCELERATION_TIMEOUT_MS 200 // Timeout in milliseconds


struct trackball_config {
    struct gpio_dt_spec left;
    struct gpio_dt_spec right;
    struct gpio_dt_spec up;
    struct gpio_dt_spec down;
    struct gpio_dt_spec push;
};

static struct {
    int64_t last_event_time;
    int consecutive_triggers;
} acceleration_state = {0, 0};

struct trackball_data {
    const struct device *dev;
    // storage for last event time
    int64_t last_event_time_up;
    int64_t last_event_time_down;
    int64_t last_event_time_left;
    int64_t last_event_time_right;

};

static struct trackball_data trackball_data = {
    .last_event_time_up = 0,
    .last_event_time_down = 0,
    .last_event_time_left = 0,
    .last_event_time_right = 0,
};

static int16_t calculate_step_size(int64_t* last_event_time) {
    int64_t current_time = k_uptime_get();
    int64_t elapsed_time = current_time - *last_event_time;
    int16_t acceleration =0;

    if (elapsed_time > ACCELERATION_TIMEOUT_MS) {
        // Reset acceleration if timeout has passed
        acceleration = 1;
    }
    else if (elapsed_time <= ACCELERATION_TIMEOUT_MS && elapsed_time > 50) {
        // If the time since the last event is less than the timeout and we have not triggered yet,
        // we reset the consecutive triggers to 0.
        acceleration = 2;
    }
    else if (elapsed_time <= 50 && elapsed_time > 10) {
        // If the time since the last event is less than 50ms, we increase the consecutive triggers
        acceleration = 5;
    }
    else {
        // If the time since the last event is less than 10ms, we increase the consecutive triggers
        acceleration = 15;
    }


    *last_event_time = current_time;

    return CONFIG_ZMK_TRACKBALL_STEP_WIDTH * acceleration_state.consecutive_triggers;
}

static void trackball_trigger_handler_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t x_movement = 0;
    int16_t y_movement = -(calculate_step_size(&trackball_data.last_event_time_up));

    printk("trackball up triggered, step size: %d\n", y_movement);

    zmk_hid_mouse_movement_set(x_movement, y_movement);
    zmk_endpoints_send_mouse_report();
    zmk_hid_mouse_movement_set(0, 0);
}

static void trackball_trigger_handler_down(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t x_movement = 0;
    int16_t y_movement = (calculate_step_size(&trackball_data.last_event_time_down));

    printk("trackball up triggered\n");

    // Send mouse movement event via ZMK HID
    //zmk_hid_mouse_movement_report(x_movement, y_movement);
    zmk_hid_mouse_movement_set(x_movement, y_movement);
    zmk_endpoints_send_mouse_report();
    zmk_hid_mouse_movement_set(0, 0);
}

static void trackball_trigger_handler_right(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t y_movement = 0;
    int16_t x_movement = (calculate_step_size(&trackball_data.last_event_time_right));

    printk("trackball up triggered\n");

    // Send mouse movement event via ZMK HID
    //zmk_hid_mouse_movement_report(x_movement, y_movement);
    zmk_hid_mouse_movement_set(x_movement, y_movement);
    zmk_endpoints_send_mouse_report();
    zmk_hid_mouse_movement_set(0, 0);
}

static void trackball_trigger_handler_left(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t y_movement = 0;
    int16_t x_movement = -(calculate_step_size(&trackball_data.last_event_time_left));

    printk("trackball up triggered\n");

    // Send mouse movement event via ZMK HID
    //zmk_hid_mouse_movement_report(x_movement, y_movement);
    zmk_hid_mouse_movement_set(x_movement, y_movement);
    zmk_endpoints_send_mouse_report();
    zmk_hid_mouse_movement_set(0, 0);
}

static void trackball_push_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    // Handle push button event if needed
    // For example, you could send a click event or toggle a mode
    printk("trackball push button pressed\n");

    zmk_hid_mouse_button_press(0);
zmk_endpoints_send_mouse_report();
zmk_hid_mouse_button_release(0);
zmk_endpoints_send_mouse_report();
}

static int trackball_init(const struct device *dev)
{
    const struct trackball_config *config = dev->config;

    printk("trackball initializing\n");

    if (!device_is_ready(config->left.port) || !device_is_ready(config->right.port) ||
        !device_is_ready(config->up.port) || !device_is_ready(config->down.port)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&config->left, GPIO_INPUT | GPIO_PULL_DOWN | GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_configure_dt(&config->right, GPIO_INPUT | GPIO_PULL_DOWN | GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_configure_dt(&config->up, GPIO_INPUT | GPIO_PULL_DOWN | GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_configure_dt(&config->down, GPIO_INPUT | GPIO_PULL_DOWN | GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_configure_dt(&config->push, GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_TO_ACTIVE);

    static struct gpio_callback left_cb_data;
    static struct gpio_callback right_cb_data;
    static struct gpio_callback up_cb_data;
    static struct gpio_callback down_cb_data;
    static struct gpio_callback push_cb_data;

    gpio_init_callback(&left_cb_data, trackball_trigger_handler_left, BIT(config->left.pin));
    gpio_add_callback(config->left.port, &left_cb_data);
    gpio_pin_interrupt_configure_dt(&config->left, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&right_cb_data, trackball_trigger_handler_right, BIT(config->right.pin));
    gpio_add_callback(config->right.port, &right_cb_data);
    gpio_pin_interrupt_configure_dt(&config->right, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&up_cb_data, trackball_trigger_handler_up, BIT(config->up.pin));
    gpio_add_callback(config->up.port, &up_cb_data);
    gpio_pin_interrupt_configure_dt(&config->up, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&down_cb_data, trackball_trigger_handler_down, BIT(config->down.pin));
    gpio_add_callback(config->down.port, &down_cb_data);
    gpio_pin_interrupt_configure_dt(&config->down, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&push_cb_data, trackball_push_handler, BIT(config->push.pin));
    gpio_add_callback(config->push.port, &push_cb_data);
    gpio_pin_interrupt_configure_dt(&config->push, GPIO_INT_EDGE_TO_ACTIVE);

    return 0;
}

#define TRACKBALL_INIT(n)                                      \
static const struct trackball_config trackball_config_##n = { \
        .left = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), left), gpios),               \
        .right = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), right), gpios),             \
        .up = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), up), gpios),                   \
        .down = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), down), gpios),               \
        .push = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), push), gpios), \
};                                                            \
static struct trackball_data trackball_data_##n;              \
DEVICE_DT_INST_DEFINE(n, trackball_init, NULL,                \
                      &trackball_data_##n,                    \
                      &trackball_config_##n, POST_KERNEL,\
                      CONFIG_SENSOR_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(TRACKBALL_INIT)

