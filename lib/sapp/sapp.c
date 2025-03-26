#include "sapp.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <string.h>

LOG_MODULE_REGISTER(sapp);

#define CONFIG_SAPP_STACK_SIZE (1024)
#define CONFIG_SAPP_THREAD_PRIORITY (2)
#define MAX_SAPPS                  10U      // max number of sapps running
#define TIMEOUT_MUTEX_MS           10

typedef enum {
  STATE_STARTUP = 0, // initial start state
  STATE_DISABLED,    // not running, waiting to be (re)enabled
  STATE_SETUP,       // call client setup callback
  STATE_LOOP,        // call client loop callback until non-zero return
  STATE_FINI,        // shutdown, go back to STATE_DISABLED
} STATE;

static const char* state_strings[] = { // for shell prints
  "STATE_STARTUP", "STATE_DISABLED", "STATE_SETUP", "STATE_LOOP", "STATE_FINI",
};

typedef struct {
  const char*       name;
  struct k_timer    timer;
  struct k_work     work;
  int               delay_ms;
  STATE             state;
  int (*setup)(void);
  int (*loop)(void);
} Sapp;

typedef struct {
  bool            init_done;
  struct k_mutex  lock;
  Sapp            sapps[MAX_SAPPS];
  uint16_t        next_app_idx;

  K_KERNEL_STACK_MEMBER(stack_area, CONFIG_SAPP_STACK_SIZE);
  struct k_work_q work_q;
} Ctx;

static Ctx ctx = {
  .init_done = false,
  .next_app_idx = 0,
};

static void timer_handler(struct k_timer* timer_id) {
  Sapp* sapp = CONTAINER_OF(timer_id, Sapp, timer);
  k_work_submit_to_queue(&ctx.work_q, &sapp->work);
}

static void work_handler(struct k_work* work_id) {
  int ret = 0;
  Sapp* sapp = CONTAINER_OF(work_id, Sapp, work);
  int current_delay_ms = sapp->delay_ms;
  STATE current_state = sapp->state;
  
  switch(sapp->state) {
    case STATE_STARTUP:
      // Do startup stuff. Called once ever.
      sapp->state = STATE_SETUP;
      break;
    
    case STATE_DISABLED: // We get here after sapp_start is called again
      sapp->state = STATE_SETUP;
    
    case STATE_SETUP:
      ret = sapp->setup();
      if (ret < 0) {
        sapp->state = STATE_FINI;
      } else if (ret == 0) {
        sapp->state = STATE_DISABLED;
      } else {
        sapp->state = STATE_LOOP;
        sapp->delay_ms = ret;
      }
      break;
    
    case STATE_LOOP:
      ret = sapp->loop();
      if (ret < 0) {
        sapp->state = STATE_FINI;
      } else if (ret == 0) {
        sapp->state = STATE_DISABLED;
      } else {
        sapp->delay_ms = ret;
      }
      break;
    
    case STATE_FINI:
      // Do any shutdown work.
      sapp->state = STATE_DISABLED;
      break;
    
    default:
      LOG_ERR("Unknown state %d", sapp->state);
      sapp->state = STATE_DISABLED;
  }
  
  if (current_state != sapp->state) {  // show state changes only
    LOG_INF("%s {enum:STATE}%d -> {enum:STATE}%d, delay %d",
    sapp->name, current_state, sapp->state, sapp->delay_ms);
  }
  
  if (sapp->state == STATE_FINI || sapp->state == STATE_DISABLED) {
    k_timer_stop(&sapp->timer);
    return;
  }
  
  if (sapp->delay_ms != current_delay_ms) {
    k_timer_start(&sapp->timer, K_MSEC(sapp->delay_ms), K_MSEC(sapp->delay_ms));
  }
}

/**
 * _init (lazy init)
 * - init mutex, clear/set sapps array
*/
static int init(void)
{
  int rv = k_mutex_init(&ctx.lock);
  if (rv != 0) {
    LOG_ERR("failed to create mutex");
    return -ENOMEM;
  }

  // init all sapps
  memset(&ctx.sapps, 0, sizeof(ctx.sapps));
  for (unsigned int i = 0; i < MAX_SAPPS; i++) {
    ctx.sapps[i].state = STATE_STARTUP;
    ctx.sapps[i].setup = NULL;
    ctx.sapps[i].loop = NULL;
  }

  k_work_queue_init(&ctx.work_q);

  k_work_queue_start( &ctx.work_q,
                      ctx.stack_area,
                      K_THREAD_STACK_SIZEOF(ctx.stack_area),
                      CONFIG_SAPP_THREAD_PRIORITY,
                      NULL);

  if (k_thread_name_set(&ctx.work_q.thread, "SAPP WQ") != 0) {
    // Couldn't set thread name
  }

  ctx.init_done = true;
  return 0;
}

/**
 * _get_sapp_idx
 * - return index for storing sapp, or idx of sapp if currently stored
*/
static int get_sapp_idx(const char *name) {
  // check if sapp is already in array
  for(unsigned int i = 0; i < MAX_SAPPS; i++) {
    if (ctx.sapps[i].name == NULL) {
      break;  // empty entry end of list
    }
    if (ctx.sapps[i].name == name) {
      LOG_INF("found %s at idx %d", name, i);  // TODO: make DEBUG
      return i;
    }
  }
  // did not find entry for app, store at next available
  unsigned int app_idx = ctx.next_app_idx;
  LOG_INF("new %s at idx %d", name, app_idx);  // TODO: make DEBUG
  ctx.next_app_idx++;
  return app_idx;
}


int sapp_start(const char *name, int (*setup)(void), int (*loop)(void))
{
  int rv = 0;
  rv = k_mutex_lock(&ctx.lock, K_MSEC(TIMEOUT_MUTEX_MS));
  if (rv != 0) {
    LOG_ERR("failed to take lock");
    return -EACCES;
  }

  if (ctx.next_app_idx >= MAX_SAPPS) {
    LOG_ERR("Too many sapps running already");
    int rc = k_mutex_unlock(&ctx.lock);
    ARG_UNUSED(rc);
    return -EAGAIN;
  }

  int app_idx = get_sapp_idx(name);

  // check if sapp is in a state to be restarted
  // NOTE: this depends on STATE_STARTUP == 0 by initialized _ctx.sapps[] to zeros
  if (!( (ctx.sapps[app_idx].state == STATE_DISABLED) ||
         (ctx.sapps[app_idx].state == STATE_STARTUP)) ) {
    LOG_ERR("attempt to restart sapp while in busy state {enum:sapp_lib_state_e}%d", ctx.sapps[app_idx].state);
    int rc = k_mutex_unlock(&ctx.lock);
    ARG_UNUSED(rc);
    return -EBUSY;
  }

  Sapp sapp = {
    .work     = Z_WORK_INITIALIZER(work_handler),
    .name     = name,
    .setup    = setup,
    .loop     = loop,
    .delay_ms = 0,
    .state = STATE_STARTUP,
  };
  ctx.sapps[app_idx] = sapp;

  k_timer_init(&ctx.sapps[app_idx].timer, timer_handler, NULL);

  LOG_INF("starting sapp %s at idx %d", name, app_idx);
  if (k_work_submit_to_queue(&ctx.work_q, &ctx.sapps[app_idx].work) < 0) {
    LOG_ERR("Couldn't push sapp work");
  }

  rv = k_mutex_unlock(&ctx.lock);
  ARG_UNUSED(rv);
  return 0;
}

/**
 * cmd_status
 * - shows the status of registered sapps
 * TODO: show/track time stats of loop() calls per sapp
*/
static int cmd_status(const struct shell *shell, size_t argc, char **argv)
{
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);

  shell_print(shell, "Number sapps registered: %d of %d allowed", ctx.next_app_idx, MAX_SAPPS);
  for(unsigned int i = 0; i < MAX_SAPPS; i++) {
    if (ctx.sapps[i].name == NULL) {
      break;  // empty entry end of list
    }
    shell_print(shell, "%20s, %15s, delay %d ms", ctx.sapps[i].name, state_strings[ctx.sapps[i].state], ctx.sapps[i].delay_ms);
  }
	return 0;
}

SYS_INIT(init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

SHELL_STATIC_SUBCMD_SET_CREATE(sapp_cmds,

  SHELL_CMD_ARG(status, NULL,
    "show sapp status"
    "usage:\n"
    "$ sapp status",
    cmd_status, 0, 0),

  SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sapp, &sapp_cmds, "sapp commands", NULL);