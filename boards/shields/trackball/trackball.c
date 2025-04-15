#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>

#define DT_DRV_COMPAT gpio_keys

struct trackball_config {
    struct gpio_dt_spec left;
    struct gpio_dt_spec right;
    struct gpio_dt_spec up;
    struct gpio_dt_spec down;
};

struct trackball_data {
    const struct device *dev;
};

static void trackball_trigger_handler_up(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;

    int16_t x_movement = 0;
    int16_t y_movement = 100;

    printk("trackball up triggered\n");

    // Send mouse movement event via ZMK HID
    //zmk_hid_mouse_movement_report(x_movement, y_movement);
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
    int16_t y_movement = -100;

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

    int16_t x_movement = 100;
    int16_t y_movement = 0;

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

    int16_t x_movement = -100;
    int16_t y_movement = 0;

    printk("trackball up triggered\n");

    // Send mouse movement event via ZMK HID
    //zmk_hid_mouse_movement_report(x_movement, y_movement);
    zmk_hid_mouse_movement_set(x_movement, y_movement);
    zmk_endpoints_send_mouse_report();
    zmk_hid_mouse_movement_set(0, 0);
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

