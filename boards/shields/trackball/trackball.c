#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>

#define DT_DRV_COMPAT gpio_keys

#define ACCELERATION_TIMEOUT_MS 200 // Timeout in milliseconds
#define MAX_ACCELERATION 24


struct trackball_config {
    struct gpio_dt_spec left;
    struct gpio_dt_spec right;
    struct gpio_dt_spec up;
    struct gpio_dt_spec down;
    struct gpio_dt_spec push;
};

struct trackball_motion_state {
    int64_t last_event_time;
    int64_t filtered_interval;
    bool initialized;
};

struct trackball_data {
    const struct device *dev;
    struct trackball_motion_state x_motion;
    struct trackball_motion_state y_motion;
};

static struct trackball_data trackball_data;
static atomic_t pending_x_movement;
static atomic_t pending_y_movement;

static int16_t calculate_step_size(struct trackball_motion_state *state) {
    int64_t current_time = k_uptime_get();
    if (!state->initialized) {
        state->last_event_time = current_time;
        state->filtered_interval = ACCELERATION_TIMEOUT_MS;
        state->initialized = true;
    } else {
        int64_t elapsed_time = current_time - state->last_event_time;
        state->last_event_time = current_time;

        if (elapsed_time < 0) {
            elapsed_time = 0;
        }

        if (elapsed_time >= ACCELERATION_TIMEOUT_MS) {
            state->filtered_interval = ACCELERATION_TIMEOUT_MS;
        } else {
            state->filtered_interval =
                (3 * state->filtered_interval + elapsed_time + 2) / 4;
        }
    }

    int64_t scale_numerator = ACCELERATION_TIMEOUT_MS +
                              (MAX_ACCELERATION - 1) *
                                  (ACCELERATION_TIMEOUT_MS - state->filtered_interval);
    int64_t step_size = ((int64_t)CONFIG_ZMK_TRACKBALL_STEP_WIDTH * scale_numerator +
                         ACCELERATION_TIMEOUT_MS / 2) /
                        ACCELERATION_TIMEOUT_MS;
    if (step_size > INT16_MAX) {
        step_size = INT16_MAX;
    }

    return step_size;
}

static int16_t clamp_mouse_movement(atomic_val_t movement) {
    if (movement > INT16_MAX) {
        return INT16_MAX;
    }
    if (movement < INT16_MIN) {
        return INT16_MIN;
    }
    return movement;
}

static void trackball_mouse_work_handler(struct k_work *work) {
    (void)work;

    int16_t x_movement = clamp_mouse_movement(atomic_set(&pending_x_movement, 0));
    int16_t y_movement = clamp_mouse_movement(atomic_set(&pending_y_movement, 0));
    if (x_movement == 0 && y_movement == 0) {
        return;
    }

    zmk_hid_mouse_movement_set(x_movement, y_movement);
    zmk_endpoint_send_mouse_report();
    zmk_hid_mouse_movement_set(0, 0);
}

K_WORK_DEFINE(trackball_mouse_work, trackball_mouse_work_handler);

static void queue_mouse_movement(int16_t x_movement, int16_t y_movement) {
    atomic_add(&pending_x_movement, x_movement);
    atomic_add(&pending_y_movement, y_movement);
    k_work_submit(&trackball_mouse_work);
}

static void trackball_trigger_handler_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t x_movement = 0;
    int16_t y_movement = -calculate_step_size(&trackball_data.y_motion);

    queue_mouse_movement(x_movement, y_movement);
}

static void trackball_trigger_handler_down(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t x_movement = 0;
    int16_t y_movement = calculate_step_size(&trackball_data.y_motion);

    queue_mouse_movement(x_movement, y_movement);
}

static void trackball_trigger_handler_right(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t y_movement = 0;
    int16_t x_movement = calculate_step_size(&trackball_data.x_motion);

    queue_mouse_movement(x_movement, y_movement);
}

static void trackball_trigger_handler_left(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t y_movement = 0;
    int16_t x_movement = -calculate_step_size(&trackball_data.x_motion);

    queue_mouse_movement(x_movement, y_movement);
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
    zmk_endpoint_send_mouse_report();
    zmk_hid_mouse_button_release(0);
    zmk_endpoint_send_mouse_report();
}

static int trackball_init(const struct device *dev)
{
    const struct trackball_config *config = dev->config;

    printk("trackball initializing\n");

    if (!device_is_ready(config->left.port) || !device_is_ready(config->right.port) ||
        !device_is_ready(config->up.port) || !device_is_ready(config->down.port)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&config->left, GPIO_INPUT | GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_configure_dt(&config->right, GPIO_INPUT | GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_configure_dt(&config->up, GPIO_INPUT | GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_configure_dt(&config->down, GPIO_INPUT | GPIO_INT_EDGE_TO_ACTIVE);
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

