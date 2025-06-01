#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>

#define DT_DRV_COMPAT gpio_keys

#define BASE_STEP_SIZE CONFIG_ZMK_TRACKBALL_STEP_WIDTH
#define ACCELERATION_FACTOR 2
#define ACCELERATION_TIMEOUT_MS 100 // Timeout in milliseconds


struct trackball_config {
    struct gpio_dt_spec left;
    struct gpio_dt_spec right;
    struct gpio_dt_spec up;
    struct gpio_dt_spec down;
};

static struct {
    int64_t last_event_time;
    int consecutive_triggers;
} acceleration_state = {0, 0};

struct trackball_data {
    const struct device *dev;
    // storage for cursor position
    int16_t x_position;
    int16_t y_position;
    // storage for accelation values
    int16_t x_acceleration;
    int16_t y_acceleration;
};

static int16_t calculate_step_size(int16_t base_step) {
    int64_t current_time = k_uptime_get();
    int64_t elapsed_time = current_time - acceleration_state.last_event_time;

    if (elapsed_time > ACCELERATION_TIMEOUT_MS) {
        // Reset acceleration if timeout has passed
        acceleration_state.consecutive_triggers = 0;
    }

    acceleration_state.last_event_time = current_time;

    // Increase step size if consecutive triggers exceed threshold
    if (acceleration_state.consecutive_triggers > 0) {
        return base_step * acceleration_state.consecutive_triggers;
    }

    return base_step;
}

static void trackball_trigger_handler_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t x_movement = 0;
    int16_t y_movement = BASE_STEP_SIZE;

    acceleration_state.consecutive_triggers++;

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
    int16_t y_movement = -BASE_STEP_SIZE);

    acceleration_state.consecutive_triggers++;

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
    int16_t x_movement = BASE_STEP_SIZE;

    acceleration_state.consecutive_triggers++;

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
    int16_t x_movement = -BASE_STEP_SIZE;

    acceleration_state.consecutive_triggers++;

    printk("trackball up triggered\n");

    // Send mouse movement event via ZMK HID
    //zmk_hid_mouse_movement_report(x_movement, y_movement);
    zmk_hid_mouse_movement_set(x_movement, y_movement);
    zmk_endpoints_send_mouse_report();
    zmk_hid_mouse_movement_set(0, 0);
}

// worker thread por cyclic pushing mouse data
// static void trackball_worker_handler(struct k_work *work)
// {
//     struct trackball_data *data = CONTAINER_OF(work, struct trackball_data, work);
//     const struct device *dev = data->dev;

//     // while loop for cyclic task
//     while (1) {
//         // if mouse data not zero we can report it
//         if (data->x_position != 0 || data->y_position != 0) {
//             zmk_hid_mouse_movement_set(data->x_position, data->y_position);
//             zmk_endpoints_send_mouse_report();
//             data->x_position = 0;
//             data->y_position = 0;
//             // reset also the reported data
//             zmk_hid_mouse_movement_set(0, 0);

            
//         }
// }

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

    static struct gpio_callback left_cb_data;
    static struct gpio_callback right_cb_data;
    static struct gpio_callback up_cb_data;
    static struct gpio_callback down_cb_data;

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

    // Initialize the worker thread
    //k_work_init(&data->work, trackball_worker_handler);

    return 0;
}

#define TRACKBALL_INIT(n)                                      \
static const struct trackball_config trackball_config_##n = { \
        .left = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), left), gpios),               \
        .right = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), right), gpios),             \
        .up = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), up), gpios),                   \
        .down = GPIO_DT_SPEC_GET(DT_CHILD(DT_NODELABEL(trackball), down), gpios),               \
};                                                            \
static struct trackball_data trackball_data_##n;              \
DEVICE_DT_INST_DEFINE(n, trackball_init, NULL,                \
                      &trackball_data_##n,                    \
                      &trackball_config_##n, POST_KERNEL,\
                      CONFIG_SENSOR_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(TRACKBALL_INIT)

