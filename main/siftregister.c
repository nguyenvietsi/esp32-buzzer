#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"


#include "shiftregister.h"


/* pin definition */
#define DGPIO_SHIFT_IN 25
//#define DGPIO_SHIFT_OE 26
#define DGPIO_SHIFT_STCP 27
#define DGPIO_SHIFT_SHCP 22
#define DGPIO_SHIFT_SHMR 4
#define DGPIO_SHIFT_PWR 2

/* pin definition for control of shift register */
#define DATAPIN		DGPIO_SHIFT_IN		// 74HC595‚ÌDS‚Ö
#define LATCHPIN	DGPIO_SHIFT_STCP	// 74HC595‚ÌST_CP‚Ö
#define CLOCKPIN	DGPIO_SHIFT_SHCP	// 74HC595‚ÌSH_CP‚Ö


/* shift register test.                        */
/* I will change this array to the bit fields.. */
unsigned char bSRdata[ESR_MAX]={
		1,/* ESR_BATMONI =0 */
		1,/* ESR_BMA_CSB */
		1,/* ESR_SIGFOX_CSN */
		1,/* ESR_GPS_CSN */
		1,/* ESR_SIGFOX_RESET */
		1,/* ESR_GPX_PWR */
		1,/* ESR_SIGFOX_POWER */
		0,/* ESR_NOTUSED_1_Q7 */
		0,/* ESR_NOTUSED_2_Q0 */
		0,/* ESR_RED_LED */
		0,/* ESR_BLUE_LED */
		1,/* ESR_GPS_RESET */
		0,/* ESR_NOTUSED_2_Q4 */
		0,/* ESR_NOTUSED_2_Q5 */
		0,/* ESR_NOTUSED_2_Q6 */
		0,/* ESR_NOTUSED_2_Q7 */
};



void updateSR(){

	int iii;

	gpio_set_level(LATCHPIN, 0);
    for( iii = 0; iii < ESR_MAX; iii++){
    	gpio_set_level(DATAPIN, bSRdata[ESR_MAX-iii-1]);
    	gpio_set_level(CLOCKPIN, 1);
    	gpio_set_level(CLOCKPIN, 0);

    }
    gpio_set_level(LATCHPIN, 1);

    return;
}


void shiftregisterInit(){

	   gpio_config_t io_conf;
	    //disable interrupt
	    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	    //set as output mode
	    io_conf.mode = GPIO_MODE_OUTPUT;
	    //bit mask of the pins that you want to set,e.g.GPIO18/19
	    io_conf.pin_bit_mask =
	    		(1ULL<<DGPIO_SHIFT_PWR )|
				(1ULL<<DGPIO_SHIFT_IN )|
	//			(1ULL<<DGPIO_SHIFT_OE )|
				(1ULL<<DGPIO_SHIFT_STCP )|
				(1ULL<<DGPIO_SHIFT_SHCP )|
				(1ULL<<DGPIO_SHIFT_SHMR )
				;

	    //disable pull-down mode
	    io_conf.pull_down_en = 0;
	    //disable pull-up mode
	    io_conf.pull_up_en = 0;
	    //configure GPIO with the given settings
	    gpio_config(&io_conf);


	    gpio_set_level(LATCHPIN, 1);
//	    gpio_set_level(DGPIO_SHIFT_OE, 0);
	    gpio_set_level(DGPIO_SHIFT_SHMR, 1);
	    gpio_set_level(DGPIO_SHIFT_PWR, 1);/* shift-register on */

	    //delaysub(100);
	    /* set shift-register */
	    gpio_set_level(CLOCKPIN, 0);
	    updateSR();/* init shift regiser */


	    return;


}
