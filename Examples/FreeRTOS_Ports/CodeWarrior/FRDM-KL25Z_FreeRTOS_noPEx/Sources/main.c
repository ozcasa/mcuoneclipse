/*
 * main implementation: use this 'C' sample to create your own application
 *
 */
#include "derivative.h" /* include peripheral declarations */
#include "FreeRTOS.h"

#define RED         (18)
#define RED_SHIFT   (1<<RED)

#define RED_OFF     (GPIOB_PSOR = RED_SHIFT)
#define RED_ON      (GPIOB_PCOR = RED_SHIFT)
#define RED_TOGGLE  (GPIOB_PTOR = RED_SHIFT)

static void InitLED(void) {
  /* Turn on clock to PortB module */
  SIM_SCGC5 |= SIM_SCGC5_PORTB_MASK;

  /* Set the PTB18 pin multiplexer to GPIO mode */
  PORTB_PCR18 = PORT_PCR_MUX(1);

  /* Set the initial output state to high */
  GPIOB_PSOR |= RED_SHIFT;

  /* Set the pins direction to output */
  GPIOB_PDDR |= RED_SHIFT;
}

static void NegLED(void) {
  RED_TOGGLE;
}

static portTASK_FUNCTION(MainTask, pvParameters) {
  (void)pvParameters; /* parameter not used */
  for(;;) {
    NegLED();
    vTaskDelay(250/portTICK_RATE_MS);
  }
}

int main(void) {
  InitLED();
  if (xTaskCreate(
        MainTask,  /* pointer to the task */
        (signed char *)"Main", /* task name for kernel awareness debugging */
        configMINIMAL_STACK_SIZE, /* task stack size */
        (void*)NULL, /* optional task startup argument */
        tskIDLE_PRIORITY,  /* initial priority */
        (xTaskHandle*)NULL /* optional task handle to create */
      ) != pdPASS) {
    /*lint -e527 */
    for(;;){}; /* error! probably out of memory */
    /*lint +e527 */
  }
  vTaskStartScheduler(); /* does not return */
	return 0;
}
