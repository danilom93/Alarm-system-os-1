#ifndef  APPH
#define  APPH

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

/*	Uncomment this define to compile the second solution													*/
//#define 	TIMER_INTERRUPT

#define 	FREQ_10HZ	50
#define 	FREQ_20HZ	25
#define 	FREQ_0HZ	0

/*
 * 	Task prototypes
 */

static 		void 		adcTask(void *p_arg);

#ifndef TIMER_INTERRUPT

static 		void 		ledControlTask(void *p_arg);

#endif

/*
 * 	Interrupt handler prototypes
 */
			void 		adcInterruptHandler(void);

#ifdef TIMER_INTERRUPT

			void 		timerInterruptHandler(void);
#endif

/*
 * 	Function prototypes
 */
			void 		adcInit(void);
			void 		adcStart(void);
			void 		computeLedPeriod(void);
#endif
