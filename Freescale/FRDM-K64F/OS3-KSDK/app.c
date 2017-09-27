
#include "KDS/APP/app.h"

/*
 * 	Global variables
 */

static 		OS_TCB 		adcTaskTCB;
static 		CPU_STK 	adcTaskStk[APP_CFG_TASK_START_STK_SIZE];

#ifndef TIMER_INTERRUPT

static 		OS_TCB 		ledControlTaskTCB;
static 		CPU_STK 	ledControlTaskStartStk[APP_CFG_TASK_START_STK_SIZE];

#endif

static  	OS_SEM      sem;

#ifndef TIMER_INTERRUPT

static  	OS_SEM      startSem;

#endif

volatile 	uint16_t 	adcValue = 0;
volatile	uint32_t	currentLed = 0;
volatile 	uint8_t		currentFreq = 0;

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

	#if (CPU_CFG_NAME_EN == DEF_ENABLED)
    	CPU_NameSet((CPU_CHAR *)"MK64FN1M0VMD12", (CPU_ERR  *)&cpu_err);
	#endif

    /*	Initialize the hardware																				*/
    hardware_init();
    GPIO_DRV_Init(switchPins, ledPins);

    /*	Initialize the ADC module and enable the interrupt mode												*/
    adcInit();
    INT_SYS_EnableIRQ(ADC0_IRQn);
    INT_SYS_InstallHandler(ADC0_IRQn, adcInterruptHandler);

    /*	If TIMER_INTERRUPT is defined 																		*/
#ifdef TIMER_INTERRUPT

    /*	Enable the interrupt mode																			*/
    INT_SYS_EnableIRQ(PIT1_IRQn);
    INT_SYS_InstallHandler(PIT1_IRQn, timerInterruptHandler);

    /*	Enable PIT module clock																				*/
    SIM_SCGC6 |= SIM_SCGC6_PIT_MASK;

    /*	Enable PIT module																					*/
    PIT_MCR = 0x00;

    /*	Enable interrupt mode for the counter 1																*/
    PIT_TCTRL1 |= 0x02;
#endif

    /* Init uC/OS-III.                          															*/
    OSA_Init();

    /* Create a semaphore for the ADC task        															*/
    OSSemCreate(&sem, "Sem", 0u, &err);

    /* If isn't defined TIMER_INTERRUPT	     																*/
#ifndef TIMER_INTERRUPT

    /* Create a semaphore to manage which task has to start before											*/
    OSSemCreate(&startSem, "Sem start", 0, &err);
#endif

    /* Create the ADC task			                   														*/
    OSTaskCreate(	&adcTaskTCB,
                    "App Task Start",
					adcTask,
                    0u,
                    APP_CFG_TASK_START_PRIO,
                    &adcTaskStk[0u],
                    (APP_CFG_TASK_START_STK_SIZE / 10u),
                    APP_CFG_TASK_START_STK_SIZE,
                    0u,
                    0u,
                    0u,
                    (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
                    &err);

    /* If isn't defined TIMER_INTERRUPT	     																*/
#ifndef TIMER_INTERRUPT

    /* Create a task to manage leds blinking     															*/
    OSTaskCreate(	&ledControlTaskTCB,
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
#endif

    /* Start multitasking (i.e. give control to uC/OS-III).													*/
    OSA_Start();

    /* Should Never Get Here				        														*/
    while (DEF_ON){
    }
}

/*
 * 		ADC task
 */
static void adcTask(void *p_arg){

	OS_ERR 		err;
	uint8_t 	lastFreq = 1;

#ifndef TIMER_INTERRUPT

	uint8_t flag = 0;
#endif

	CPU_Init();

	while(DEF_TRUE){

		/*	Enable the ADC module																			*/
		adcStart();

		/*	Pend the semaphore																				*/
		OSSemPend(&sem, 0u, OS_OPT_PEND_BLOCKING, 0u,&err);

		#ifndef TIMER_INTERRUPT

		/*	If it is the first time																			*/
		if(flag == 0){

			/* Post the sempahore to enable the LED task 													*/
			OSSemPost(&startSem, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED, &err);
			flag = 1;
		}

		#endif

		/*	This function is used to calculate which led has to blink and its blink
		 * rate depending of the voltage read from ADC														*/
		computeLedPeriod();

		/*	If the current frequency is different from the last frequency									*/
		if(currentFreq != lastFreq){

			lastFreq = currentFreq;

			/*	If isn't define TIMER_INTERRUPT																*/
			#ifndef TIMER_INTERRUPT

				/*	If the frequency is 0Hz the red led has to be fixed										*/
				if(currentFreq == FREQ_0HZ){

					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);
					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);
					GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 0);

				}else{

					/*	Turn off other leds that must not blink												*/
					switch(currentLed){

						case BOARD_GPIO_LED_GREEN:

							GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);
							GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);
							break;
						case BOARD_GPIO_LED_BLUE:

							GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);
							GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);
							break;
						case BOARD_GPIO_LED_RED:

							GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);
							GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);
							break;
						default:

							break;
					}
				}
			}
			/* If is defined TIMER_INTERRUPT																*/
			#else

				/*	Stop PIT module to change interrupt period												*/
				PIT_TCTRL1 = 0x02;

				/*	Turn off all LEDs																		*/
				GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);
				GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);
				GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);

				/*	Switch the current frequency to set the right period									*/
				switch(currentFreq){

					case FREQ_0HZ:

						/*	If the frequency is 0Hz the red led has to be fixed								*/
						GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);
						GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);
						GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 0);
						break;
					case FREQ_10HZ:

						/*	If the frequency is 10Hz the interrupt has to occur
						 * every 50ms so the register has to be set to 3.000.000							*/
						PIT_LDVAL1 = 0x2DC6C0;
						/*	Turn on PIT module																*/
						PIT_TCTRL1 |= 0x01;
						break;
					case FREQ_20HZ:

						/*	If the frequency is 20Hz the interrupt has to occur
						 * every 25ms so the register has to be set to 1.500.000							*/
						PIT_LDVAL1 = 0x16E360;
						/*	Turn on PIT module																*/
						PIT_TCTRL1 |= 0x01;
						break;
				}
			}
		#endif
	}
}

/* If isn't defined TIMER_INTERRUPT	     																	*/
#ifndef TIMER_INTERRUPT

/*
 * 	LED control task
 */
static void ledControlTask(void *p_arg){

	OS_ERR   err;

	/*	Pend the semaphore startSem and wait until that it will be unlocked from the ADC task				*/
	OSSemPend(&startSem, 0u, OS_OPT_PEND_BLOCKING, 0u,&err);

	/*	Infinite loop																						*/
	while(DEF_TRUE){

		/* If some led has to blink																			*/
		if(currentFreq != FREQ_0HZ){

			/*	Toggle the led																				*/
			GPIO_DRV_TogglePinOutput(currentLed);

			/*	Delay for blinking																			*/
			OSTimeDlyHMSM(0u, 0u, 0u, currentFreq, OS_OPT_TIME_HMSM_STRICT, &err);
		}
	}
}
#endif

/*
 * 	ADC interrupt handler
 */
void adcInterruptHandler(void){

	OS_ERR   err;

	/* enter critical section (disable interrupts) 															*/
	CPU_CRITICAL_ENTER();

	/*	notify to scheduler the beginning of an ISR 														*/
	OSIntEnter();

	/*	read the ADC value																					*/
	adcValue = ADC0_RA;

	/* 	post the semaphore for adc task 																	*/
	OSSemPost(&sem, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED, &err);

	/*	renable interrupts 																					*/
	CPU_CRITICAL_EXIT();

	/*	notify to scheduler the end of an ISR																*/
	OSIntExit();
}

#ifdef TIMER_INTERRUPT

void timerInterruptHandler(void){

	/* enter critical section (disable interrupts) 															*/
	CPU_CRITICAL_ENTER();

	/*	notify to scheduler the beginning of an ISR 														*/
	OSIntEnter();

	/*	If the interrupt comes from PIT 1																	*/
	if(PIT_TFLG1){

		/*	Toggle the led																					*/
		GPIO_DRV_TogglePinOutput(currentLed);

		/*	Reset the interrupt flag																		*/
		PIT_TFLG1 = 0x01;
	}
	/*	renable interrupts 																					*/
	CPU_CRITICAL_EXIT();

	/*	notify to scheduler the end of an ISR																*/
	OSIntExit();
}

#endif

/*
 * 	ADC Init
 */
void adcInit(void){

	/*	Enable ADC module clock																				*/
	SIM_SCGC6 |= SIM_SCGC6_ADC0_MASK;
	/*	Set the ADC module																					*/
	ADC0_CFG1 |= ADC_CFG1_MODE(3);
	/*	Turn off the module																					*/
	ADC0_SC1A |= ADC_SC1_ADCH(31);
}

/*
 * ADC Start
 */
void adcStart(void){

	/*	Start the module with AD12																			*/
	ADC0_SC1A = 0x4C;
}

/*
 * 	Compute Led period
 */
void computeLedPeriod(){

	if(adcValue < 9929){

		currentLed 	= BOARD_GPIO_LED_GREEN;
		currentFreq = FREQ_10HZ;
	}else if(adcValue >=9929 && adcValue < 19858){

		currentLed = BOARD_GPIO_LED_GREEN;
		currentFreq = FREQ_20HZ;
	}else if(adcValue >= 19858 && adcValue < 29787){

		currentLed= BOARD_GPIO_LED_BLUE;
		currentFreq = FREQ_10HZ;
	}else if(adcValue >= 29787 && adcValue < 39716){

		currentLed = BOARD_GPIO_LED_BLUE;
		currentFreq = FREQ_20HZ;
	}else if(adcValue >= 39716 && adcValue < 49645){

		currentLed	= BOARD_GPIO_LED_RED;
		currentFreq = FREQ_10HZ;
	}else if(adcValue >= 49645 && adcValue < 59574){

		currentLed = BOARD_GPIO_LED_RED;
		currentFreq = FREQ_20HZ;
	}else{

		currentLed = BOARD_GPIO_LED_RED;
		currentFreq = FREQ_0HZ;
	}
}
