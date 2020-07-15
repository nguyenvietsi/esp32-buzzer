#ifndef _SHIFTREGISTER_H_
#define _SHIFTREGISTER_H_

#if 1
/* output definition at the shift register  */
 enum{
	ESR_BATMONI =0,
	ESR_BMA_CSB,
	ESR_SIGFOX_CSN,
	ESR_GPS_CSN,
	ESR_SIGFOX_RESET,
	ESR_GPX_PWR,
	ESR_SIGFOX_POWER,
	//ESR_NOTUSED_1_Q7,/* RFU */
	ESR_BUZZER_BOOST,/* RFU */
	ESR_NOTUSED_2_Q0,
	ESR_RED_LED,
	ESR_BLUE_LED,
	ESR_GPS_RESET,
	ESR_NOTUSED_2_Q4,/* RFU */
	ESR_NOTUSED_2_Q5,/* RFU */
	ESR_NOTUSED_2_Q6,/* RFU */
	ESR_NOTUSED_2_Q7,/* RFU */
	ESR_MAX
};
#endif
extern unsigned char bSRdata[];

void shiftregisterInit();
void updateSR();

#endif /*_SHIFTREGISTER_H_*/
