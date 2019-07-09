/*
 * Copyright (c) 2019, Erich Styger
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "platform.h"
#include "Stepper.h"
#include "McuA3967.h"
#include "McuWait.h"
#include "McuRTOS.h"

#define STEPPER_CONFIG_DO_UINT_TEST             (0)
#define STEPPER_CONFIG_DO_SLEEP_IF_NOT_MOVING   (1)  /* reduce current if not moving */

#define PINS_STEPPER1_RST_GPIO      GPIOB
#define PINS_STEPPER1_RST_PORT      PORTB
#define PINS_STEPPER1_RST_PIN       0U

#define PINS_STEPPER1_ENABLE_GPIO   GPIOB
#define PINS_STEPPER1_ENABLE_PORT   PORTB
#define PINS_STEPPER1_ENABLE_PIN    1U

#define PINS_STEPPER1_MS2_GPIO      GPIOB
#define PINS_STEPPER1_MS2_PORT      PORTB
#define PINS_STEPPER1_MS2_PIN       2U

#define PINS_STEPPER1_SLEEP_GPIO    GPIOB
#define PINS_STEPPER1_SLEEP_PORT    PORTB
#define PINS_STEPPER1_SLEEP_PIN     3U

#define PINS_STEPPER1_MS1_GPIO      GPIOB
#define PINS_STEPPER1_MS1_PORT      PORTB
#define PINS_STEPPER1_MS1_PIN       16U

#define PINS_STEPPER1_STEP_GPIO     GPIOB
#define PINS_STEPPER1_STEP_PORT     PORTB
#define PINS_STEPPER1_STEP_PIN      17U

#define PINS_STEPPER1_DIR_GPIO      GPIOB
#define PINS_STEPPER1_DIR_PORT      PORTB
#define PINS_STEPPER1_DIR_PIN       18U

typedef struct {
  bool isSleeping;
  bool isForward;
  int32_t pos;
  McuA3967_Handle_t stepper;
  struct {
    uint32_t speed;
    int32_t steps;
  } move;
} STEPPER_Motor;

static STEPPER_Motor stepper0;

#if STEPPER_CONFIG_
static void UnitTest(McuA3967_Handle_t stepper) {
  bool inSleep, dir, enable, reset;
  uint8_t micro;

  enable = McuA3967_GetEnable(stepper);
  if (enable) { /* shall not enabled initially */
    for(;;) {} /* error */
  }
  McuA3967_SetEnable(stepper, true);
  enable = McuA3967_GetEnable(stepper);
  if (!enable) { /* should be enabled now */
    for(;;) {} /* error */
  }
  McuA3967_SetEnable(stepper, false); /* disable it again */

  reset = McuA3967_GetReset(stepper);
  if (!reset) { /* shall be in reset initially */
    for(;;) {} /* error */
  }
  McuA3967_SetReset(stepper, false);
  reset = McuA3967_GetReset(stepper);
  if (reset) { /* shall be in reset now */
    for(;;) {} /* error */
  }
  McuA3967_SetReset(stepper, true); /* put back in reset */

  inSleep = McuA3967_GetSleep(stepper);
  if (!inSleep) { /* shall be in sleep */
    for(;;) {} /* error */
  }
  McuA3967_SetSleep(stepper, !inSleep);
  inSleep = McuA3967_GetSleep(stepper);
  if (inSleep) { /* shall be not in sleep now */
    for(;;) {} /* error */
  }
  McuA3967_SetSleep(stepper, true); /* put back in sleep */

  dir = McuA3967_GetDirection(stepper);
  if (dir) { /* initially false */
    for(;;) {} /* error */
  }
  McuA3967_SetDirection(stepper, true);
  dir = McuA3967_GetDirection(stepper);
  if (!dir) { /* must be true now */ /* somehow fails???????? */
    for(;;) {} /* error */
  }

  micro = McuA3967_GetMicroStepping(stepper);
  if (micro!=1) { /* should be in full step initially */
    for(;;) {}
  }
  McuA3967_SetMicroStepping(stepper, 2);
  micro = McuA3967_GetMicroStepping(stepper);
  if (micro!=2) { /* should be in half stepping now */
    for(;;) {}
  }
  McuA3967_SetMicroStepping(stepper, 4);
  micro = McuA3967_GetMicroStepping(stepper);
  if (micro!=4) { /* should be in quarter stepping now */
    for(;;) {}
  }
  McuA3967_SetMicroStepping(stepper, 8);
  micro = McuA3967_GetMicroStepping(stepper);
  if (micro!=8) { /* should be in 1/8 stepping now */
    for(;;) {}
  }

  McuA3967_SetMicroStepping(stepper, 1); /* set back to full step */
  micro = McuA3967_GetMicroStepping(stepper);
  if (micro!=1) { /* should be in half step now */
    for(;;) {}
  }
}
#endif

#if STEPPER_CONFIG_
static void DoStepping(McuA3967_Handle_t stepper) {
  bool forward = true;
  bool wait = true;
  int i;

  McuA3967_SetDirection(stepper, forward);
  for(i=0;i<100;i++) {
    McuA3967_MakeStep(stepper);
    McuWait_Waitms(25);
  }
}
#endif

static void StepperTask(void *pv) {
  for(;;) {
    if (stepper0.move.steps!=0) {
#if STEPPER_CONFIG_DO_SLEEP_IF_NOT_MOVING
      if (stepper0.isSleeping) {
        stepper0.isSleeping = false;
        McuA3967_SetSleep(stepper0.stepper, stepper0.isSleeping); /* wake up from sleep */
      }
#endif
      if (stepper0.move.steps<0 && stepper0.isForward) {
        stepper0.isForward = false;
        McuA3967_SetDirection(stepper0.stepper, stepper0.isForward);
      } else if (stepper0.move.steps>0 && !stepper0.isForward) {
        stepper0.isForward = true;
        McuA3967_SetDirection(stepper0.stepper, stepper0.isForward);
      }
      /* do step */
      McuA3967_SetStep(stepper0.stepper, true); /* set HIGH */
      vTaskDelay(1);
      McuA3967_SetStep(stepper0.stepper, false); /* set LOW */
      /* update position */
      if (stepper0.move.steps>0) { /* forward */
        stepper0.move.steps--;
        stepper0.pos++;
      } else { /* backward */
        stepper0.move.steps++;
        stepper0.pos--;
      }
#if STEPPER_CONFIG_DO_SLEEP_IF_NOT_MOVING
      if (stepper0.move.steps==0 && !stepper0.isSleeping) { /* end of stepping */
        stepper0.isSleeping = true;
        McuA3967_SetSleep(stepper0.stepper, stepper0.isSleeping); /* go to sleep to reduce current consumption */
      }
#endif
    }
    vTaskDelay(1);
  }
}

#if PL_CONFIG_USE_SHELL
static uint8_t PrintStatus(const McuShell_StdIOType *io) {
  uint8_t buf[32];

  McuShell_SendStatusStr((unsigned char*)"Stepper", (unsigned char*)"\r\n", io->stdOut);

  McuUtility_strcpy(buf, sizeof(buf), (unsigned char*)"pos: ");
  McuUtility_strcatNum32s(buf, sizeof(buf), stepper0.pos);
  if (stepper0.isForward) {
    McuUtility_strcat(buf, sizeof(buf), (unsigned char*)", FW");
  } else {
    McuUtility_strcat(buf, sizeof(buf), (unsigned char*)", BW");
  }
  McuUtility_strcat(buf, sizeof(buf), (unsigned char*)"\r\n");

  McuShell_SendStatusStr((unsigned char*)"  stepper0", buf, io->stdOut);
  (void)McuA3967_PrintStepperStatus(stepper0.stepper, (unsigned char*)"Stepper 0", io);
  return ERR_OK;
}

static uint8_t PrintHelp(const McuShell_StdIOType *io) {
  McuShell_SendHelpStr((unsigned char*)"Stepper", (unsigned char*)"Group of Stepper commands\r\n", io->stdOut);
  McuShell_SendHelpStr((unsigned char*)"  help|status", (unsigned char*)"Print help or status information\r\n", io->stdOut);
  McuShell_SendHelpStr((unsigned char*)"  go <m> <step> <speed>", (unsigned char*)"Move motor (0, ...) with number of steps at given speed\r\n", io->stdOut);
  McuShell_SendHelpStr((unsigned char*)"  microstep <m> <step>", (unsigned char*)"Set micro steps: 1, 2, 4 or 8\r\n", io->stdOut);
  return ERR_OK;
}

uint8_t STEPPER_ParseCommand(const unsigned char* cmd, bool *handled, const McuShell_StdIOType *io) {
  uint8_t res = ERR_OK;
  int32_t val;
  uint8_t motor;
  const unsigned char *p;

  if (McuUtility_strcmp((char*)cmd, McuShell_CMD_HELP) == 0
    || McuUtility_strcmp((char*)cmd, "Stepper help") == 0) {
    *handled = TRUE;
    return PrintHelp(io);
  } else if (   (McuUtility_strcmp((char*)cmd, McuShell_CMD_STATUS)==0)
             || (McuUtility_strcmp((char*)cmd, "Stepper status") == 0)
            )
  {
    *handled = TRUE;
    res = PrintStatus(io);
  } else if (McuUtility_strncmp((char*)cmd, "Stepper microstep ", sizeof("Stepper microstep ")-1) == 0) {
    p = cmd + sizeof("Stepper microstep ")-1;

    res = McuUtility_xatoi(&p, &val);
    if (res!=ERR_OK || val<0) {
      return ERR_FAILED;
    }
    motor = val;

    res = McuUtility_xatoi(&p, &val);
    if (res!=ERR_OK || !(val==1 || val==2 || val==4 || val==8)) {
      return ERR_FAILED;
    }
    McuA3967_SetMicroStepping(stepper0.stepper, val);
    *handled = TRUE;
    res = ERR_OK;
  } else if (McuUtility_strncmp((char*)cmd, "Stepper go ", sizeof("Stepper go ")-1) == 0) {
    int32_t steps;
    uint32_t speed;

    p = cmd + sizeof("Stepper go ")-1;
    res = McuUtility_xatoi(&p, &val);
    if (res!=ERR_OK || val<0) {
      return ERR_FAILED;
    }
    motor = val;
    res = McuUtility_xatoi(&p, &steps);
    if (res!=ERR_OK) {
      return ERR_FAILED;
    }
    res = McuUtility_xatoi(&p, &val);
    if (res!=ERR_OK && val<0) {
      return ERR_FAILED;
    }
    speed =  val;
    if (motor==0) {
      stepper0.move.speed = speed;
      stepper0.move.steps = steps;
    }
    *handled = TRUE;
    res = ERR_OK;
  }
  return res;
}
#endif /* PL_CONFIG_USE_SHELL */

void STEPPER_Deinit(void){
}

void STEPPER_Init(void) {
  McuA3967_Config_t config;

  CLOCK_EnableClock(kCLOCK_PortB);

  McuA3967_GetDefaultConfig(&config);

  config.rst.gpio = PINS_STEPPER1_RST_GPIO;
  config.rst.port = PINS_STEPPER1_RST_PORT;
  config.rst.pin = PINS_STEPPER1_RST_PIN;

  config.enable.gpio = PINS_STEPPER1_ENABLE_GPIO;
  config.enable.port = PINS_STEPPER1_ENABLE_PORT;
  config.enable.pin = PINS_STEPPER1_ENABLE_PIN;

  config.slp.gpio = PINS_STEPPER1_SLEEP_GPIO;
  config.slp.port = PINS_STEPPER1_SLEEP_PORT;
  config.slp.pin = PINS_STEPPER1_SLEEP_PIN;

  config.ms1.gpio = PINS_STEPPER1_MS1_GPIO;
  config.ms1.port = PINS_STEPPER1_MS1_PORT;
  config.ms1.pin = PINS_STEPPER1_MS1_PIN;

  config.ms2.gpio = PINS_STEPPER1_MS2_GPIO;
  config.ms2.port = PINS_STEPPER1_MS2_PORT;
  config.ms2.pin = PINS_STEPPER1_MS2_PIN;

  config.dir.gpio = PINS_STEPPER1_DIR_GPIO;
  config.dir.port = PINS_STEPPER1_DIR_PORT;
  config.dir.pin = PINS_STEPPER1_DIR_PIN;

  config.step.gpio = PINS_STEPPER1_STEP_GPIO;
  config.step.port = PINS_STEPPER1_STEP_PORT;
  config.step.pin = PINS_STEPPER1_STEP_PIN;

  stepper0.stepper = McuA3967_InitHandle(&config);

#if STEPPER_CONFIG_
  UnitTest();
#endif

  /* enable device */
  McuA3967_SetReset(stepper0.stepper, false); /* take out of reset */
  McuA3967_SetEnable(stepper0.stepper, true); /* enable device */
  stepper0.isSleeping = true;
  McuA3967_SetSleep(stepper0.stepper, stepper0.isSleeping); /* turn on sleep mode */
  stepper0.isForward = true;
  McuA3967_SetDirection(stepper0.stepper, stepper0.isForward);
  stepper0.pos = 0;
  stepper0.move.speed = 0;
  stepper0.move.steps = 0;
#if STEPPER_CONFIG_
  DoStepping();
#endif
  if (xTaskCreate(
      StepperTask,  /* pointer to the task */
      "Stepper", /* task name for kernel awareness debugging */
      300/sizeof(StackType_t), /* task stack size */
      (void*)NULL, /* optional task startup argument */
      tskIDLE_PRIORITY+2,  /* initial priority */
      (TaskHandle_t*)NULL /* optional task handle to create */
    ) != pdPASS) {
     for(;;){} /* error! probably out of memory */
  }

}
