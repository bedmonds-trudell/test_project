#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(app);

#define SW0_NODE	DT_ALIAS(sw0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,{0});
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,{0});

int main(void) {
	gpio_pin_configure_dt(&button, GPIO_INPUT);
  gpio_pin_configure_dt(&led, GPIO_OUTPUT);

  while(1) {
    int val = gpio_pin_get_dt(&button);
    gpio_pin_set_dt(&led, val);
    k_msleep(10);
  }

  return 0;
}