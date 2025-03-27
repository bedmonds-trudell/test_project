#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/gpio.h>

enum LED_STATE { LED_OFF, LED_ON, LED_BLINKING };

struct led {
  const struct gpio_dt_spec device;
  enum LED_STATE target_state;
};

static struct led l1 = {
    .device = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios),
    .target_state = LED_OFF,
};

static struct led l2 = {
    .device = GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios),
    .target_state = LED_OFF,
};

static struct led l3 = {
    .device = GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios),
    .target_state = LED_OFF,
};

static struct led l4 = {
  .device = GPIO_DT_SPEC_GET(DT_NODELABEL(led3), gpios),
  .target_state = LED_OFF,
};

static void set_led_state(struct led* l) {
  switch (l->target_state) {
    case LED_OFF:
      gpio_pin_set_dt(&l->device, 0);
      break;
    case LED_ON:
      gpio_pin_set_dt(&l->device, 1);
      break;
    case LED_BLINKING:
      gpio_pin_toggle_dt(&l->device);
      break;
  }
}

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

int main(void)
{
	gpio_pin_configure_dt(&l1.device, GPIO_OUTPUT);
  gpio_pin_configure_dt(&l2.device, GPIO_OUTPUT);
  gpio_pin_configure_dt(&l3.device, GPIO_OUTPUT);
  gpio_pin_configure_dt(&l4.device, GPIO_OUTPUT);

	if (led.port) {
		while (1) {
			set_led_state(&l1);
      set_led_state(&l2);
      set_led_state(&l3);
      set_led_state(&l4);
      k_msleep(500);
		}
	}
	return 0;
}

static void set_led_state_by_color(enum LED_STATE state, char* led_num) {
  if (strcmp(led_num, "1") == 0) {
    l1.target_state = state;
  } else if (strcmp(led_num, "2") == 0) {
    l2.target_state = state;
  } else if (strcmp(led_num, "3") == 0) {
    l3.target_state = state;
  } else if (strcmp(led_num, "4") == 0) {
    l4.target_state = state;
  }
}

// callback for the turn on subcommand
static int handle_turn_on(const struct shell* shell, size_t argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    shell_print(shell, "Turning on LED %s.", argv[i]);
    set_led_state_by_color(LED_ON, argv[i]);
  }
  return 0;
}

// callback for the turn off subcommand
static int handle_turn_off(const struct shell* shell, size_t argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    shell_print(shell, "Turning off LED %s.", argv[i]);
    set_led_state_by_color(LED_OFF, argv[i]);
  }
  return 0;
}

// callback for the blink subcommand
static int handle_blink(const struct shell* shell, size_t argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    shell_print(shell, "Blinking LED %s.", argv[i]);
    set_led_state_by_color(LED_BLINKING, argv[i]);
  }
  return 0;
}

// level 2 commands
SHELL_STATIC_SUBCMD_SET_CREATE(led_names,
                               SHELL_CMD(1, NULL, "LED 1.", NULL),
                               SHELL_CMD(2, NULL, "LED 2.", NULL),
                               SHELL_CMD(3, NULL, "LED 3.", NULL),
                               SHELL_CMD(4, NULL, "LED 4.", NULL),
                               SHELL_SUBCMD_SET_END);

// level 1 commands
SHELL_STATIC_SUBCMD_SET_CREATE(
    led_actions,
    SHELL_CMD(on, &led_names, "Turn the LED on.", handle_turn_on),
    SHELL_CMD(off, &led_names, "Turn the LED off.", handle_turn_off),
    SHELL_CMD(blink, &led_names, "Make the LED blink.", handle_blink),
    SHELL_SUBCMD_SET_END);

// Root level 0 command
SHELL_CMD_REGISTER(led, &led_actions, "Command to change LED states.", NULL);