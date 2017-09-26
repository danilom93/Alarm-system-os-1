#include  	<os.h>
#include  	<math.h>
#include  	<board.h>
#include  	<bsp_ser.h>
#include  	<app_cfg.h>
#include  	<cpu_core.h>
#include  	<lib_math.h>
#include  	<system_MK64F12.h>
#include	<fsl_gpio_common.h>
#include  	<fsl_os_abstraction.h>

#include 	"fsl_interrupt_manager.h"

//define
#define 	UART_BAUD	9600
#define 	LED_RED		1
#define 	LED_GREEN	2
#define 	LED_BLUE	3
#define 	FREQ_10HZ	100
#define 	FREQ_20HZ	50
#define 	FREQ_0HZ	3

//variables
static 		OS_TCB 		appStartTaskTCB;
static 		OS_TCB 		ledControlTaskTCB;

static 		CPU_STK 	appStartTaskStk[APP_CFG_TASK_START_STK_SIZE];
static 		CPU_STK 	ledControlTaskStartStk[APP_CFG_TASK_START_STK_SIZE];

volatile 	uint16_t 	adcValue = 0;

volatile	int 		currentLed;
volatile 	int			currentFreq;

//tasks
static 		void 		appStartTask(void *p_arg);
static 		void 		ledControlTask(void *p_arg);

//interrupt handlers
			void 		adcInterruptHandler(void);
			void 		portCInterruptHandler(void);
			void 		portAInterruptHandler(void);

//uart functions
			void 		uartInit();
			void 		uartPrint(char *str);
			void 		uartPutChar(char ch);

//adc functions
			void 		adcInit(void);
			void 		adcStart(void);

//generic functions

			void 		getLedPeriod(int *led, int *period);

/*
 *
 * *******************************MAIN******************************************
 *
 */

int  main(void){

    OS_ERR   err;

	#if (CPU_CFG_NAME_EN == DEF_ENABLED)
    	CPU_ERR  cpu_err;
	#endif

    hardware_init();
    GPIO_DRV_Init(switchPins, ledPins);

    adcInit();
    uartInit();

	#if (CPU_CFG_NAME_EN == DEF_ENABLED)
    	CPU_NameSet((CPU_CHAR *)"MK64FN1M0VMD12", (CPU_ERR  *)&cpu_err);
	#endif

    OSA_Init();                                                 /* Init uC/OS-III.                          */

    // ISR button
    INT_SYS_InstallHandler(PORTC_IRQn, portCInterruptHandler);
    INT_SYS_InstallHandler(PORTA_IRQn, portAInterruptHandler);
    // ISR ADC
    INT_SYS_EnableIRQ(ADC0_IRQn);
    INT_SYS_InstallHandler(ADC0_IRQn, adcInterruptHandler);


    OSTaskCreate(	&appStartTaskTCB,                              /* Create the start task                 */
                    "App Task Start",
					appStartTask,
                    0u,
                    APP_CFG_TASK_START_PRIO,
                    &appStartTaskStk[0u],
                    (APP_CFG_TASK_START_STK_SIZE / 10u),
                    APP_CFG_TASK_START_STK_SIZE,
                    0u,
                    0u,
                    0u,
                    (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
                    &err);

    OSTaskCreate(	&ledControlTaskTCB,                              /* Create the start task               */
                    "Led Control Task",
					ledControlTask,
                    0u,
                    APP_CFG_TASK_START_PRIO,
                    &ledControlTaskStartStk[0u],
                    (APP_CFG_TASK_START_STK_SIZE / 10u),
                    APP_CFG_TASK_START_STK_SIZE,
                    0u,
                    0u,
                    0u,
                    (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
                    &err);
    OSA_Start();                                                /* Start multitasking (i.e. give control to uC/OS-III). */

    while (DEF_ON){                                            /* Should Never Get Here				        */

    }
}

//task
static void appStartTask(void *p_arg){

	OS_ERR   err;
	char str[40] = {0};
	int tmp = 0;
	float read = 0;

	CPU_Init();

	while(DEF_TRUE){

		adcStart();
		uartPrint("I am enabling the ADC module\n");
		OSTimeDlyHMSM(0u, 0u, 0u, 100u, OS_OPT_TIME_HMSM_STRICT, &err);
		getLedPeriod(&currentLed, &currentFreq);
	}
}
static void ledControlTask(void *p_arg){

	OS_ERR   err;

	while(DEF_TRUE){

		if(currentFreq == FREQ_0HZ){

			uartPrint("VIN > 3V\n");
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 0);
			OSTimeDlyHMSM(0u, 0u, 0u, 100u, OS_OPT_TIME_HMSM_STRICT, &err);
		}else{

			switch(currentLed){

				case LED_GREEN:

					uartPrint("0V < VIN < 1V\n");
					GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_GREEN);
					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);
					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);
					break;
				case LED_BLUE:

					uartPrint("1V < VIN < 2V\n");
					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);
					GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_BLUE);
					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);
					break;
				case LED_RED:

					uartPrint("2V < VIN < 3V\n");
					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);
					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);
					GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_RED);
					break;
				default:

					break;
			}
			OSTimeDlyHMSM(0u, 0u, 0u, currentFreq, OS_OPT_TIME_HMSM_STRICT, &err);
		}
	}
}

//ISR handler
void portCInterruptHandler(void){

  static uint32_t ifsr;
  uint32_t portBaseAddr = g_portBaseAddr[GPIO_EXTRACT_PORT(kGpioSW1)];

  CPU_CRITICAL_ENTER();         						// enter critical section (disable interrupts)

  OSIntEnter();        								 	// notify to scheduler the beginning of an ISR ("This allows C/OS-III to keep track of interrupt nesting")

  ifsr = PORT_HAL_GetPortIntFlag(portBaseAddr);         // get intr flag reg related to port C

  if((ifsr & 0x40u)){

      GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_BLUE);
      uartPrint("You pressed button 1\n");
      GPIO_DRV_ClearPinIntFlag(kGpioSW1);
  }

  CPU_CRITICAL_EXIT();  // renable interrupts

  OSIntExit();          /* notify to scheduler the end of an ISR ("determines if a higher priority task is ready-to-run.
                          If so, the interrupt returns to the higher priority task instead of the interrupted task.") */
}
void portAInterruptHandler(void){

  static uint32_t ifsr;          // interrupt flag status register
  uint32_t portBaseAddr = g_portBaseAddr[GPIO_EXTRACT_PORT(kGpioSW2)];

  CPU_CRITICAL_ENTER();         // enter critical section (disable interrupts)

  OSIntEnter();         // notify to scheduler the beginning of an ISR ("This allows C/OS-III to keep track of interrupt nesting")

  ifsr = PORT_HAL_GetPortIntFlag(portBaseAddr);

  if((ifsr & 0x10u)){

      GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_RED);             // turn on/off led
      uartPrint("You pressed button 2\n");
      GPIO_DRV_ClearPinIntFlag(kGpioSW2);       // clear int flag related to SW1 to avoid recalling the handler
  }
  if((ifsr & 0x02u)){

	  GPIOE_PTOR |= 0x4000000;
	  uartPrint("You pressed button 3\n");
      GPIO_DRV_ClearPinIntFlag(kGpioSW3);       // clear int flag related to SW1 to avoid recalling the handler
    }

  CPU_CRITICAL_EXIT();  // renable interrupts

  OSIntExit();          /* notify to scheduler the end of an ISR ("determines if a higher priority task is ready-to-run.
                          If so, the interrupt returns to the higher priority task instead of the interrupted task.") */
}
void adcInterruptHandler(void){

	char str[40] = {0};

	CPU_CRITICAL_ENTER();         // enter critical section (disable interrupts)

	OSIntEnter();         // notify to scheduler the beginning of an ISR ("This allows C/OS-III to keep track of interrupt nesting")

	adcValue = ADC0_RA;
	snprintf(str, 40*sizeof(char), "Hello, I am here:  %d\n", adcValue);
	uartPrint(str);

	CPU_CRITICAL_EXIT();  // renable interrupts

	OSIntExit();
}

//function uart
void uartInit(){

	uint16_t 	ubd;
	uint8_t 	temp;

	//C3 --> UART1_RX
	PORT_HAL_SetMuxMode(PORTC_BASE,3u,kPortMuxAlt3);
	__asm("nop");
	    //C4 --> UART1_TX
	PORT_HAL_SetMuxMode(PORTC_BASE,4u,kPortMuxAlt3);
	__asm("nop");

    SIM_SCGC4 |= SIM_SCGC4_UART1_MASK;      //enable uart clock

	UART1_C2 &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK ); //turn off module
    UART1_C1 = 0;
	ubd = (uint16_t)((120000*1000)/(UART_BAUD * 16));
	temp = UART1_BDH & ~(UART_BDH_SBR(0x1F));
	UART1_BDH = temp | (((ubd & 0x1F00) >> 8));
	UART1_BDL = (uint8_t)(ubd & UART_BDL_SBR_MASK);
	UART1_C2 |= UART_C2_TE_MASK;   //enable transmission
}
void uartPutChar(char ch){

	//while transmission register is not free
	while(!(UART1_S1 & UART_S1_TDRE_MASK));
	//send data
	UART1_D = (uint8_t)ch;
}
void uartPrint(char *str){

	//while char is different to null
	while(*str){

		uartPutChar(*str++);
	}
}

//function adc
void adcInit(void){

	SIM_SCGC6 |= SIM_SCGC6_ADC0_MASK;
	ADC0_CFG1 |= ADC_CFG1_MODE(3);
	ADC0_SC1A |= ADC_SC1_ADCH(31);
}
void adcStart(void){

	ADC0_SC1A = 0x4C;
}

void getLedPeriod(int *led, int *period){

	if(adcValue < 9929){

		*led 	= LED_GREEN;
		*period = FREQ_10HZ;
	}else if(adcValue >=9929 && adcValue < 19858){

		*led 	= LED_GREEN;
		*period = FREQ_20HZ;
	}else if(adcValue >= 19858 && adcValue < 29787){

		*led 	= LED_BLUE;
		*period = FREQ_10HZ;
	}else if(adcValue >= 29787 && adcValue < 39716){

		*led 	= LED_BLUE;
		*period = FREQ_20HZ;
	}else if(adcValue >= 39716 && adcValue < 49645){

		*led 	= LED_RED;
		*period = FREQ_10HZ;
	}else if(adcValue >= 49645 && adcValue < 59574){

		*led 	= LED_RED;
		*period = FREQ_20HZ;
	}else{

		*led 	= LED_RED;
		*period = FREQ_0HZ;
	}
}
