#if 0   //IO¿Ú


/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */

#include "device.h"
#include "gpio_api.h"   // mbed
#include "main.h"

#define GPIO_LED_PIN       PC_5
#define GPIO_PUSHBT_PIN    PC_4


void delay(unsigned int cnt)
{
  while(cnt--)
	for(int i=0;i<1000000;i++)
		asm(" nop");
}
/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
//int main_app(IN u16 argc, IN u8 *argv[])
void main(void)
{
    gpio_t gpio_led;
    gpio_t gpio_btn;
    printf("here is coming\n");

    // Init LED control pin
    gpio_init(&gpio_led, GPIO_LED_PIN);
    gpio_dir(&gpio_led, PIN_OUTPUT);    // Direction: Output
    gpio_mode(&gpio_led, PullNone);     // No pull

    // Initial Push Button pin
    gpio_init(&gpio_btn, GPIO_PUSHBT_PIN);
    gpio_dir(&gpio_btn, PIN_INPUT);     // Direction: Input
    gpio_mode(&gpio_btn, PullUp);       // Pull-High

    while(1){
      
      gpio_write(&gpio_led, 0);
      delay(1);
//      printf("here is high\n");
      gpio_write(&gpio_led, 1);
//      printf("here is low\n");
      delay(1);
/*      
        if (gpio_read(&gpio_btn)) {
            // turn off LED
            gpio_write(&gpio_led, 0);
        }
        else {
            // turn on LED
            gpio_write(&gpio_led, 1);
        }
*/
    }
}




#endif

#if  1  //original

#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include <example_entry.h>

extern void console_init(void);
extern xTaskHandle g_client_task;  
extern xTaskHandle g_server_task;

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{
	if ( rtl_cryptoEngine_init() != 0 ) {
		DiagPrintf("crypto engine init failed\r\n");
	}

	/* Initialize log uart and at command service */
	console_init();	

	/* pre-processor of application example */
	pre_example_entry();

	/* wlan intialization */
#if defined(CONFIG_WIFI_NORMAL) && defined(CONFIG_NETWORK)
	wlan_network();
#endif

	/* Execute application example */
	example_entry();
        
#if 1    //xjx-test 
            
        #define BSD_STACK_SIZE_TEST		256
//        if(xTaskCreate(TcpClientHandler_test, "tcp_client", BSD_STACK_SIZE_TEST, NULL, tskIDLE_PRIORITY + 1 + PRIORITIE_OFFSET, &g_client_task) != pdPASS)
//		printf("\n\rTCP ERROR: Create tcp client task failed.");
        
        if(xTaskCreate(TcpServerHandler_test, "tcp_server", BSD_STACK_SIZE_TEST, NULL, tskIDLE_PRIORITY + 1 + PRIORITIE_OFFSET, &g_server_task) != pdPASS)
			printf("\n\rTCP ERROR: Create tcp server task failed.");
#endif
    	/*Enable Schedule, Start Kernel*/
#if defined(CONFIG_KERNEL) && !TASK_SCHEDULER_DISABLED
	#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
	#endif
#else
	RtlConsolTaskRom(NULL);
#endif
        
       
}
#endif


#if 0   //socket--Client

#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include <example_entry.h>
void main(void)
{
  while(1);
}

#endif