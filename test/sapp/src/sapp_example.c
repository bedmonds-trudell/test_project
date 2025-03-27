#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include "sapp.h"

LOG_MODULE_REGISTER(sapp_example);

#define TIME_SCHED_DELAY_LOOP_MS   100
#define DEMO_MAX_WORK_COUNT        40
#define CBOR_TX_BUF_SIZE           148
#define UCLOGGE_PLOT_NAME_SIZE     12

typedef enum {
  STATE_SETUP = 0,
  STATE_LOOP,
} STATE;
// !! NOTE: in order to use LOG_*("{enum: <>}%d"),
//          enum declaration name must be unique !!

/**
 * Private Context
*/
typedef struct {
  // add fields as required
  int work_count;
  int x_tics;
  bool enable;
  const struct shell *shell;
  STATE state;
  int plot_number;
  char plot_name[UCLOGGE_PLOT_NAME_SIZE];
} Ctx;

static Ctx ctx = {
  // init fields as required
  .plot_number = 0,
  .work_count = 0,
  .x_tics = 0,
  .enable = false,
  .state = STATE_SETUP,
};

/**
 * Arduino-like setup() callback
 * - run once every time the app is enabled from the SHELL
*/
static int setup(void) {
  ctx.work_count = 0;
  ctx.enable = true;
  ctx.state = STATE_SETUP;

  LOG_INF("State %d", ctx.state);
  return 0;
}

/**
 * Arduino-like loop() callback
 * - run via scheduler after TIME_SCHED_DELAY_LOOP_MS
 * - return zero or negative value
 *             - will stop future calls to loop()
 *             - SHELL enable to restart
 *          positive value
 *             - represents the milliseconds delay to poll loop()
 *             - value can change during the life of sapp
 *             - recommended value should generally be >= 10
 *
 * - WARNING do not block loop() for long time (>= ~100ms),
 *   as this will starve other threads
*/
static int loop(void) {
  // Finally time to work, the real (fake) work is done here...
  if (!ctx.enable) return -1;  // exit loop() by SHELL disable command
  if (ctx.work_count == 0) {
    ctx.state = STATE_LOOP;
    LOG_INF("State %d", ctx.state);
  }

  ctx.work_count++;

  // show the user my work... generally shell or log, but not both
  shell_print(ctx.shell, "working %d/%d", ctx.work_count, DEMO_MAX_WORK_COUNT);
  if ((ctx.work_count % 10) == 0) {  // log every 10th count
    LOG_INF("working %d/%d", ctx.work_count, DEMO_MAX_WORK_COUNT);
  }

  if (ctx.work_count >= DEMO_MAX_WORK_COUNT) return 0;  // done?
  return TIME_SCHED_DELAY_LOOP_MS;
}

/**
 * SHELL Commands and handlers
 * - within a SHELL command, use getopt to parse args
 *
 * cmd_enable
 * - do not modify, all apps enabled the same way
*/
static int cmd_enable(const struct shell *shell, size_t argc, char **argv) {
  int c=0;
  while ((c = getopt(argc, argv, "hed")) != -1) {
    switch (c) {
    case 'e':
      // !! NOTE: the sapp name should be unique !!
      sapp_start("sapp_example", setup, loop);
      ctx.shell = shell;  // cache shell handle for worker output
      LOG_INF("shell enabled");
      break;

    case 'd':
      ctx.enable = false;
      LOG_INF("shell disabled");
      break;

    case 'h':
      shell_print(shell, "Usage: enable <-e|-d>");
      shell_print(shell, "       -e enable");
      shell_print(shell, "       -d disable");
      break;

    default:
      //LOG_ERROR("Invalid args: %s", *argv);  not required Zephry prints an error
      shell_print(shell, "ERROR: invalid args: %s", *argv);
      break;
    }
  }
	return 0;
}

/**
 * cmd_status - demo, can be removed
*/
static int cmd_status(const struct shell *shell, size_t argc, char **argv) {
    LOG_INF("working %d/%d", ctx.work_count, DEMO_MAX_WORK_COUNT);
    shell_print(shell, "working %d/%d", ctx.work_count, DEMO_MAX_WORK_COUNT);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sapp_example_cmds,
    // common enable/disable, do not modify
    SHELL_CMD_ARG(enable, NULL,
        "enable (or disable) Example app"
        "usage:\n"
        "$ sapp_example enable <-e|-d>",
        cmd_enable, 2, 0),

    /**
     * Add app commands here...
    */
    SHELL_CMD_ARG(status, NULL,   // demo, can be removed
        "show work status"
        "usage:\n"
        "$ sapp_example status",
        cmd_status, 0, 0),

    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sapp_example, &sapp_example_cmds, "sapp_example commands", NULL);