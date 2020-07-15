////////////////////////////////////////////////////////////////////////////////////////////////
/**
 @file		menu.c
 @brief 	control console menu
 @author	Green House  Azuma
*/
/////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdio.h"
#include "stdbool.h"
#include "string.h"
#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/ledc.h"
#include "shiftregister.h"




#define DMENU_TIMEOUT_AUTO (300*60000)
#define DMENU_TIMEOUT_NONE (0x7fffffff)


#define DMENU_VAL_MAX 10 
#define DMENU_ACTION_MAX 20 
#define DMENU_LINEBUF_MAX 256

#define DMYSTAT 1

static void writeFlash(){}/* dummy */
static bool console_getc(uint8_t* pc1, uint32_t time){
	bool bRet = false;
	int iret = 0;
	iret = read(STDIN_FILENO,pc1,1);
	if(iret>0){
		bRet = true;
	}
	return bRet;
}
static void console_putc(uint8_t pc1){
	write(STDOUT_FILENO,&pc1,1);
}
static void console_puts(char* pc1){
	write(STDOUT_FILENO,pc1,strlen((char*)pc1));
}
static uint32_t HAL_GetTick(){
	return xTaskGetTickCount();
}
/********************************************************************/
/* GLOBAL VARIABLE						    */
/********************************************************************/

/********************************************************************/
/* LOCAL VARIABLE						    */
/********************************************************************/
/* menu ID list */
typedef enum{
  EMENUID_NONE, 
  EMENUID_TOPMENU,
  EMENUID_FREQ,
  EMENUID_DUTY,
  EMENUID_RING,
  EMENUID_INTERVAL,
  EMENUID_BOOST,
  EMENUID_MAX
}EMenuID;

typedef enum{
  EMENU_INPUTTYPE_NONE,
  EMENU_INPUTTYPE_STRING,
  EMENU_INPUTTYPE_DECIMAL,
  EMENU_INPUTTYPE_HEX,
  EMENU_INPUTTYPE_MAX
}EMenuInputType;

typedef enum{
  EMENU_VAL_NONE,
  EMENU_VAL_INT8,        /* 8bit     */
  EMENU_VAL_INT16,       /* 16bit    */
  EMENU_VAL_INT32,       /* 32bit    */
  EMENU_VAL_STR,         /* string   */
  EMENU_VAL_BOOL_ONOFF,  /* on/off   */
  EMENU_VAL_FLOAT,       /* float    */
  EMENU_VAL_CALLBACK,    /* for using a specific type */
  EMENU_VAL_HEX_ARRY,
  EMENU_VAL_MAX
}EMenuValType;

typedef enum{
  EMENU_ACTION_NONE,
  EMENU_ACTION_DEVICESTART,
  EMENU_ACTION_NEXTMENU,
  EMENU_ACTION_SETPARAM_INT8,
  EMENU_ACTION_SETPARAM_INT16,
  EMENU_ACTION_SETPARAM_INT32,
  EMENU_ACTION_SETPARAM_FLOAT,
  EMENU_ACTION_SETPARAM_HEX_ARRY,
  EMENU_ACTION_CALLBACK,/* to do a specific action */
  EMENU_ACTION_DISPMSG,
  EMENU_ACTION_BUZZERSTOP,
  EMENU_ACTION_MAX
}EMenuAction;



typedef struct{
  EMenuAction action;/* action type */
  void* pAction;     /* action parameter */
  void* pNext;       /* next pointer     */
  uint32_t exparam;
}SMenuAction;


typedef struct{
    EMenuInputType inputtype;
    float min;
    float max;
    SMenuAction action;
}SMenuInput;

typedef struct{
  const char* const fmt; /* NULL means the indicator of the end.  */
  struct{
    EMenuValType eType;
    void* pVal;
    uint32_t exparam;
  }val;
  
}SMenuDisp;


/* The chache to debug a buzzer */
typedef struct{
	uint32_t freq;
	uint32_t duty;
	uint32_t ring;
	uint32_t interval;
	bool boost;
}SMenuCache;

static SMenuCache stCacheM = {5000,8,60,60,true};
static SMenuCache stCacheD = {5000,8,60,60,true};

enum{
	EBUZZER_IDLE,
	EBUZZER_RING,
	EBUZZER_REST,
	EBUZZER_MAX
};

#define DDISPVAL_NONE {EMENU_VAL_NONE,NULL}
/* callback functions for menu */
typedef EMenuAction (* menuActionCB(SMenuAction *action));
static const SMenuAction stMenuActionTopMenu = {EMENU_ACTION_NEXTMENU,(void*)EMENUID_TOPMENU,NULL};

/* start up message */

/* LoRaWAN menu */
static const SMenuDisp dispLoRaWANMenu[] = {
  {" 1.Frequency              : %d(Hz)\r\n",{EMENU_VAL_INT32,(void*)&stCacheM.freq}},
  {" 2.Duty rate              : 1/%d\r\n",{EMENU_VAL_INT32,(void*)&stCacheM.duty}},
  {" 3.Ringing time(sec)      : %d(sec)\r\n",{EMENU_VAL_INT32,(void*)&stCacheM.ring}},
  {" 4.Interval time(sec)     : %d(sec)\r\n",{EMENU_VAL_INT32,(void*)&stCacheM.interval}},
  {" 5.Stop buzzer\r\n",DDISPVAL_NONE},
  {" 6.Boost     : %s\r\n",{EMENU_VAL_BOOL_ONOFF,(void*)&stCacheM.boost}},
  {" 0.Start/Restart\r\n",DDISPVAL_NONE},
  {"InputNumber:",DDISPVAL_NONE},
  {NULL,{EMENU_VAL_NONE,NULL}}/* end code */
};

static const char strInputNumber[] = "\rInput Number:";

static const SMenuInput inputLoRaWANMenu[] = {
   {EMENU_INPUTTYPE_STRING,   0,   0, {EMENU_ACTION_NEXTMENU, (void*)EMENUID_TOPMENU,  NULL}},
   {EMENU_INPUTTYPE_DECIMAL,  0,   0, {EMENU_ACTION_DEVICESTART, (void*)strInputNumber,  NULL}},
   {EMENU_INPUTTYPE_DECIMAL,  1,   1, {EMENU_ACTION_NEXTMENU, (void*)EMENUID_FREQ,  NULL}},
   {EMENU_INPUTTYPE_DECIMAL,  2,   2, {EMENU_ACTION_NEXTMENU, (void*)EMENUID_DUTY,  NULL}},
   {EMENU_INPUTTYPE_DECIMAL,  3,   3, {EMENU_ACTION_NEXTMENU, (void*)EMENUID_RING,  NULL}},
   {EMENU_INPUTTYPE_DECIMAL,  4,   4, {EMENU_ACTION_NEXTMENU, (void*)EMENUID_INTERVAL,  NULL}},
   {EMENU_INPUTTYPE_DECIMAL,  5,   5, {EMENU_ACTION_BUZZERSTOP, (void*)strInputNumber,  NULL}},
   {EMENU_INPUTTYPE_DECIMAL,  6,   6, {EMENU_ACTION_NEXTMENU, (void*)EMENUID_BOOST,  NULL}},

   {EMENU_INPUTTYPE_NONE}
};


static const SMenuDisp dispFreqMenu[] = {
  {" 1.Frequency              : %d(Hz)\r\n",{EMENU_VAL_INT32,(void*)&stCacheM.freq}},
  {"InputNumber:",DDISPVAL_NONE},
  {NULL,{EMENU_VAL_NONE,NULL}}/* end code */
};
static const SMenuInput inputFreqMenu[] = {
   {EMENU_INPUTTYPE_STRING,   0,   0, {EMENU_ACTION_NEXTMENU,(void*)EMENUID_TOPMENU,NULL}},        
   {EMENU_INPUTTYPE_DECIMAL,  1,   65535, {EMENU_ACTION_SETPARAM_INT32,(void*)&stCacheM.freq, (void*)&stMenuActionTopMenu}},
   {EMENU_INPUTTYPE_NONE}
};

static const SMenuDisp dispDutyMenu[] = {
  {" 2.Duty rate              : 1/%d\r\n",{EMENU_VAL_INT32,(void*)&stCacheM.duty}},
  {"InputNumber:",DDISPVAL_NONE},
  {NULL,{EMENU_VAL_NONE,NULL}}/* end code */
};

static const SMenuInput inputDutyMenu[] = {
  {EMENU_INPUTTYPE_STRING,   0,   0, {EMENU_ACTION_NEXTMENU,(void*)EMENUID_TOPMENU,NULL}},
  {EMENU_INPUTTYPE_DECIMAL,  2,   65535, {EMENU_ACTION_SETPARAM_INT32,(void*)&stCacheM.duty, (void*)&stMenuActionTopMenu}},
  {EMENU_INPUTTYPE_NONE}
};

static const SMenuDisp dispRingMenu[] = {
  {" 3.Ringing time(sec)      : %d(sec)\r\n",{EMENU_VAL_INT32,(void*)&stCacheM.ring}},
  {"InputNumber:",DDISPVAL_NONE},
  {NULL,{EMENU_VAL_NONE,NULL}}/* end code */
};

static const SMenuInput inputRingMenu[] = {
  {EMENU_INPUTTYPE_STRING,   0,   0, {EMENU_ACTION_NEXTMENU,(void*)EMENUID_TOPMENU,NULL}},
  {EMENU_INPUTTYPE_DECIMAL,  0,   65535, {EMENU_ACTION_SETPARAM_INT32,(void*)&stCacheM.ring, (void*)&stMenuActionTopMenu}},
  {EMENU_INPUTTYPE_NONE}
};


static const SMenuDisp dispIntervalMenu[] = {
  {" 4.Interval time(sec)     : %d(sec)\r\n",{EMENU_VAL_INT32,(void*)&stCacheM.interval}},
  {"InputNumber:",DDISPVAL_NONE},
  {NULL,{EMENU_VAL_NONE,NULL}}/* end code */
};

static const SMenuInput inputIntervalMenu[] = {
  {EMENU_INPUTTYPE_STRING,   0,   0, {EMENU_ACTION_NEXTMENU,(void*)EMENUID_TOPMENU,NULL}},
  {EMENU_INPUTTYPE_DECIMAL,  0,   65535, {EMENU_ACTION_SETPARAM_INT32,(void*)&stCacheM.interval, (void*)&stMenuActionTopMenu}},
  {EMENU_INPUTTYPE_NONE}
};

static const SMenuDisp dispBoostMenu[] = {
  {" 6.Boost     : %s\r\n",{EMENU_VAL_BOOL_ONOFF,(void*)&stCacheM.boost}},
  {"InputNumber(on/off=1/0):",DDISPVAL_NONE},
  {NULL,{EMENU_VAL_NONE,NULL}}/* end code */
};

static const SMenuInput inputBoostMenu[] = {
  {EMENU_INPUTTYPE_STRING,   0,   0, {EMENU_ACTION_NEXTMENU,(void*)EMENUID_TOPMENU,NULL}},
  {EMENU_INPUTTYPE_DECIMAL,  0,   1, {EMENU_ACTION_SETPARAM_INT8,(void*)&stCacheM.boost, (void*)&stMenuActionTopMenu}},
  {EMENU_INPUTTYPE_NONE}
};


/* definition for the list to show menus */
typedef struct {
  struct{
    uint32_t timeout; /* msec */
    SMenuDisp* fmt;
  }disp;
  struct{
      SMenuInput *arInput;
  }input;
  struct{
    bool hiddenoutput;
  }other;
  struct{
    uint32_t rfu;
  }rfu;

}SMenuInfo;



static const SMenuInfo arMenu[EMENUID_MAX] = {

  {   {0},{0},{0} },/* NONE*/
  {{ DMENU_TIMEOUT_NONE,(SMenuDisp*)dispLoRaWANMenu }, {(SMenuInput*)inputLoRaWANMenu}, {false } },/* EMENUID_TOPMENU */
  {{ DMENU_TIMEOUT_NONE,(SMenuDisp*)dispFreqMenu  }, {(SMenuInput*)inputFreqMenu}, { false }  },/* EMENUID_FREQ */
  {{ DMENU_TIMEOUT_NONE,(SMenuDisp*)dispDutyMenu  }, {(SMenuInput*)inputDutyMenu}, { false }  },/* EMENUID_DUTY */
  {{ DMENU_TIMEOUT_NONE,(SMenuDisp*)dispRingMenu  }, {(SMenuInput*)inputRingMenu}, { false }  },/* EMENUID_RING */
  {{ DMENU_TIMEOUT_NONE,(SMenuDisp*)dispIntervalMenu  }, {(SMenuInput*)inputIntervalMenu}, { false }  },/* EMENUID_INTERVAL */
  {{ DMENU_TIMEOUT_NONE,(SMenuDisp*)dispBoostMenu  }, {(SMenuInput*)inputBoostMenu}, { false }  },/* EMENUID_INTERVAL */

};


/*  work  area */
static struct{
  EMenuID menuID;
  uint32_t tick_s;/* tick from show a current screen */
  struct{
    uint8_t buf[DMENU_LINEBUF_MAX];
    uint16_t itt;
  }line;
  uint32_t val;
  uint8_t dispbuf[512];
  uint8_t hexbuf[128];
  struct{
	 uint8_t stat;
	 uint32_t tick;
  }bz;
}stMenuWork;
uint32_t melody_fred=2000;
static ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num  = LEDC_TIMER_0,
    .bit_num    = 2,
    .freq_hz    = 2000
};

static ledc_channel_config_t ledc_channel = {
    .channel    = LEDC_CHANNEL_0,
    .gpio_num   = 5,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 2
};


/********************************************************************/
/* LOCAL FUNCTIONS						    */
/********************************************************************/
static void menuDisp(EMenuID id);
static SMenuInfo* getMenuInfo(EMenuID id);
static EMenuAction menuAction();
static bool menustart();

static void buzzerInit();
static void buzzerMelody();
static void buzzerOn();
static void buzzerOff();
static void sound(uint32_t freq,uint32_t duration);


/******************************************************************************/
/* make console menu start                                                    */
/******************************************************************************/
static bool menustart(){
  uint8_t getc;
  uint8_t bEsc = 0;
  EMenuAction eAction;
  SMenuAction stMenuAction;
  SMenuInfo *pMenuInfo;
  bool bEnd = false;
  
  //console_puts("\r\n");
    pMenuInfo = getMenuInfo(stMenuWork.menuID);
//  FOR_MSECWAIT_S(tick_s){/* main loop */
    if(console_getc(&getc,0)){
      if(bEsc){
        bEsc--;
      }else if(getc == 0x1b){
        bEsc = 2;
      }else{
//        if(stMenuWork.line.itt == 0 && getc == '\r'){
//           /* ignore  */
//           continue;
//        }        
        //if(stCache.info1.general.echoback){
    	if(true){
          if(pMenuInfo && pMenuInfo->other.hiddenoutput && getc != '\r'
             && getc != 0x08)
          {
            console_putc('*');
             
          }else{
            console_putc(getc);/* echo back */
          }
        }

        
        if(getc == '\r'){

          console_putc('\n');
          stMenuWork.line.buf[stMenuWork.line.itt]= NULL;/* null terminate */
          eAction = menuAction(&stMenuAction);
          stMenuWork.tick_s = HAL_GetTick();
          if( eAction == EMENU_ACTION_DEVICESTART){
        	  buzzerOff();

        	console_puts("\r\nStart!\r\n");
            stCacheD = stCacheM;
            if(stCacheD.boost){
    			bSRdata[ESR_BUZZER_BOOST] = 1;
    			//bSRdata[ESR_RED_LED] = 1;
    			console_puts("\r\nBoost on!\r\n");
            }else{
            	bSRdata[ESR_BUZZER_BOOST] = 0;
            	//bSRdata[ESR_RED_LED] = 0;

            	console_puts("\r\nBoost off!\r\n");
            }

            updateSR();
            buzzerOn();
            stMenuWork.bz.stat = 	EBUZZER_RING;

          }else if( eAction == EMENU_ACTION_BUZZERSTOP){
            console_puts("\r\nStop!\r\n");
            buzzerOff();
            stMenuWork.bz.stat = 	EBUZZER_IDLE;

#if 0

          }else if(eAction == EMENU_ACTION_FACTORYTESTSTART){
            stLocalInfo.bFactoryTest = true;
            console_puts("\r\nFactory Test!\r\n");
#endif
          
          }else if(eAction == EMENU_ACTION_NEXTMENU){
            menuDisp((EMenuID)(uint32_t)stMenuAction.pAction);
            pMenuInfo = getMenuInfo(stMenuWork.menuID);
          }
          stMenuWork.line.itt = 0;
         
        }else if(getc == 0x08){
          if(stMenuWork.line.itt)stMenuWork.line.itt--;
        }else{
          stMenuWork.line.buf[stMenuWork.line.itt++] = getc;
          if(stMenuWork.line.itt>=DMENU_LINEBUF_MAX){
            /* length over */
            console_puts("lengthover\r");
            stMenuWork.line.itt = 0;
            //menuDisp()
          }
        }
        
      }
      
//    }
#if 0
      if(FOR_MSECWAIT_IsE(stMenuWork.tick_s,pMenuInfo->disp.timeout)){
      console_puts("\r\nStart!\r\n");
      bEnd = true;
    }
#endif
  }
  
  return bEnd; /* end of menu */
}

/******************************************************************************/
/* show menu                                                                  */
/******************************************************************************/
static void menuDisp(EMenuID id){
  SMenuInfo* pMenuInfo;
  SMenuDisp* pMenuDisp;
  //uint32_t arVal[DMENU_VAL_MAX];
  uint32_t val = 0;
  uint8_t i;
  const char strOn[] = "On";
  const char strOff[] = "Off";  
  const char strOTAA[] = "OTAA";
  const char strABP[] = "ABP";
  
  
  pMenuInfo = getMenuInfo(id);
  pMenuDisp = pMenuInfo->disp.fmt;

  console_puts("\r\n");/* CRLF */
  if(pMenuInfo){
    stMenuWork.menuID = id;
    stMenuWork.tick_s = HAL_GetTick();
  
    while(pMenuDisp && pMenuDisp->fmt){
      if(pMenuDisp->val.eType == EMENU_VAL_NONE){
        val = 0;
      }
      else if(pMenuDisp->val.eType == EMENU_VAL_INT8){
        val = *((uint8_t*)pMenuDisp->val.pVal);
      }
      else if(pMenuDisp->val.eType == EMENU_VAL_INT16){
        val = *((uint16_t*)pMenuDisp->val.pVal);
      }
      else if(pMenuDisp->val.eType == EMENU_VAL_INT32){
        val = *((uint32_t*)pMenuDisp->val.pVal);
      }
      else if(pMenuDisp->val.eType == EMENU_VAL_STR){
        val = (uint32_t)pMenuDisp->val.pVal;
      }
      else if(pMenuDisp->val.eType == EMENU_VAL_BOOL_ONOFF){
        val = (uint32_t)((*(uint8_t*)pMenuDisp->val.pVal==0)?strOff:strOn);
      }
#if 0
      else if(pMenuDisp->val.eType == EMENU_VAL_OTAA_ABP){
        val = (uint32_t)((*(uint8_t*)pMenuDisp->val.pVal==0)?strOTAA:strABP);
      }
#endif
      else if(pMenuDisp->val.eType == EMENU_VAL_CALLBACK){
        /* pending*/
      }
      else if(pMenuDisp->val.eType == EMENU_VAL_HEX_ARRY){
        for(i=0;i<pMenuDisp->val.exparam;i++){
          if(i>=32){
            break;/* failsafe */
          }
          sprintf((char*)&stMenuWork.hexbuf[i*2],"%02x",((uint8_t*)pMenuDisp->val.pVal)[i]);
        }
        val = (uint32_t)stMenuWork.hexbuf;
      }
      
      /* create caption */
     // if(pMenuDisp->val.eType != EMENU_VAL_NONE){
          sprintf((char*)stMenuWork.dispbuf,pMenuDisp->fmt,val);
          console_puts((char*)stMenuWork.dispbuf);
      //}
      pMenuDisp++;
    }

                                                   
    
  }
  
  
  
  
  return;
}
/******************************************************************************/
/* get menu information                                                       */
/******************************************************************************/
SMenuInfo* getMenuInfo(EMenuID id_a){
  SMenuInfo* pRet = NULL;
  if(id_a < EMENUID_MAX){/* check parmeter */
    pRet = (SMenuInfo*)&arMenu[id_a];
  }
  return pRet;
}


static EMenuAction menuActionSub(SMenuAction *pAction_a){
  SMenuAction *pAction = pAction_a;
  uint8_t hexstr[3];
  uint16_t hexidx = 0;
  uint16_t hexbufidx = 0;
  uint32_t hex;
  
  uint16_t i;
  uint8_t getc;
  bool isHex = false;
  bool berror = false;
  
  if(pAction){
    while(1){
      if(pAction->action == EMENU_ACTION_CALLBACK && pAction->pAction){
        ((menuActionCB *)pAction->pAction)(pAction);
      }else if(pAction->action == EMENU_ACTION_NEXTMENU  && pAction->pAction){
        /* do nothing */
      }else if(pAction->action == EMENU_ACTION_SETPARAM_INT8  && pAction->pAction){
        *(uint8_t *)pAction->pAction = (uint8_t)stMenuWork.val;
        writeFlash();
      }else if(pAction->action == EMENU_ACTION_SETPARAM_INT16  && pAction->pAction){
        *(uint16_t *)pAction->pAction = (uint16_t)stMenuWork.val;
        writeFlash();
      }else if(pAction->action == EMENU_ACTION_SETPARAM_INT32  && pAction->pAction){
        *(uint32_t *)pAction->pAction = (uint32_t)stMenuWork.val;
        writeFlash();
      }else if(pAction->action == EMENU_ACTION_SETPARAM_FLOAT  && pAction->pAction){
        *(float *)pAction->pAction = stMenuWork.val;
        writeFlash();
      }else if (pAction->action == EMENU_ACTION_DISPMSG  && pAction->pAction){
        console_puts((char*)pAction->pAction);
      
      }else if(pAction->action == EMENU_ACTION_SETPARAM_HEX_ARRY  && pAction->pAction){
        hexidx = 0;
        hexbufidx = 0;
        for(i=0;i<strlen((char*)stMenuWork.line.buf);i++){
          getc = stMenuWork.line.buf[i];
          if(getc == 0x20){
            /* space is ignored */
            continue;
          }
          isHex = false;
          /* check code */
          if(getc>='0' && getc <= '9'){
            isHex = true;
          }else if(getc>='a' && getc <= 'f'){
            isHex = true;
          }else if(getc>='A' && getc <= 'F'){
            isHex = true;
          }
          if(isHex){
            hexstr[hexidx++] = getc; /* add ascii to buf for atoi */
            if(hexidx >= 2){
              if(1 == sscanf((char*)hexstr,"%02x",&hex)){
                stMenuWork.hexbuf[hexbufidx++] = hex;
              }
              hexidx = 0;
            }
          }else{
            console_puts("parameter error \r\n");
            berror = true;
            break;
          }
        }
        if(berror == false && pAction->exparam == hexbufidx){
          /* copy */
          memcpy(pAction->pAction,stMenuWork.hexbuf,pAction->exparam);
          writeFlash();
        }
                
      }
      
 
      
      if(pAction->pNext){/*  is there next pointer? */
        *pAction = *(SMenuAction *)pAction->pNext;
      }else{
        break;
      }
    }
    
   
    return pAction_a->action;
  }else{
    return EMENU_ACTION_NONE;
  }
}

/******************************************************************************/
/* menu action process                                                        */
/******************************************************************************/
static EMenuAction menuAction(SMenuAction *pRetAct){
//  SMenuAction *pAction;
  static SMenuAction stAction;
  SMenuInfo* pMenuInfo;
  uint16_t linelen;
  float floatval;
  uint8_t i;
  
  memset(&stAction,0,sizeof(stAction));
  pMenuInfo = getMenuInfo(stMenuWork.menuID);
  if(pMenuInfo){
    for(i=0;i<DMENU_ACTION_MAX;i++){
      
      if(pMenuInfo->input.arInput[i].inputtype == EMENU_INPUTTYPE_STRING){
        linelen = strlen((char*)stMenuWork.line.buf);/* get length */
        if(pMenuInfo->input.arInput[i].min <= linelen &&  /* check length */ 
           pMenuInfo->input.arInput[i].max >= linelen){
             stAction = pMenuInfo->input.arInput[i].action;
             menuActionSub(&stAction);
             *pRetAct = stAction;
             break;
        }
          
      }
 
      else if(pMenuInfo->input.arInput[i].inputtype == EMENU_INPUTTYPE_DECIMAL){
        if(1== sscanf((char*)stMenuWork.line.buf,"%f",&floatval)){
          if(pMenuInfo->input.arInput[i].min <= floatval &&  /* check min/max */ 
             pMenuInfo->input.arInput[i].max >= floatval){
               stMenuWork.val= floatval;/* save value */
               stAction = pMenuInfo->input.arInput[i].action;
               menuActionSub(&stAction);
               *pRetAct = stAction;
               break;
          }
        }
      }
      else if(pMenuInfo->input.arInput[i].inputtype == EMENU_INPUTTYPE_NONE){
        console_puts("Invalid Parameter\r\n");
        menuDisp(stMenuWork.menuID);
        break;
      }

    }
  }
  return stAction.action;
  
}


/******************************************************************************/
/* passcode process                                                           */
/******************************************************************************/
static EMenuAction menuActionPasscode(SMenuAction* pAction){
#if 0
	SSettings *pCache;
  char codebuf[16];
 

  pCache = getCache();
  sprintf(codebuf,"%04d",pCache->info1.general.passcode);

  if(strcmp(masterPasscode,(char*)stMenuWork.line.buf) == 0 || strcmp((char*)stMenuWork.line.buf,codebuf)==0){
    /* passcode matching */
    pAction->action =   EMENU_ACTION_NEXTMENU;
    pAction->pAction =   (void*)EMENUID_TOPMENU;
    
    }else{
    pAction->action =  EMENU_ACTION_NONE;
  }
  
  return pAction->action;
#endif
  return EMENU_ACTION_NEXTMENU;
}

/******************************************************************************/
/* factory reset process                                                           */
/******************************************************************************/
static EMenuAction menuActionFactoryReset(SMenuAction* pAction){
#if 0
  SSettings *pCache;
  SSettings *pDefault;
 
  pCache = getCache();
  pDefault = getDefaultSettings();
  *pCache = *pDefault;
  writeFlash();
  
  pAction->action =   EMENU_ACTION_NEXTMENU;
  pAction->pAction =   (void*)EMENUID_TOPMENU;

  
  return pAction->action;
#endif
  return EMENU_ACTION_NEXTMENU;
}
static void buzzerproc(){
	if(stMenuWork.bz.stat == EBUZZER_RING){
		buzzerMelody();
		if(((HAL_GetTick() - stMenuWork.bz.tick)*portTICK_PERIOD_MS) >= (stCacheD.ring * 1000) ){
			console_puts("\r\nOFFFFFFFFFFFFFFF!\r\n");
			stMenuWork.bz.stat = EBUZZER_REST;
			stMenuWork.bz.tick = HAL_GetTick();
			buzzerOff();
		}

	}else if(stMenuWork.bz.stat == EBUZZER_REST){
		if(((HAL_GetTick() - stMenuWork.bz.tick)*portTICK_PERIOD_MS) >= (stCacheD.interval * 1000) ){
			stMenuWork.bz.stat = EBUZZER_RING;
			stMenuWork.bz.tick = HAL_GetTick();
			buzzerOn();
		}

	}
}

/********************************************************/
/* menu task in init                                    */
/********************************************************/
void usermainInitMenu()
{
	shiftregisterInit();
  memset(&stMenuWork,0,sizeof(stMenuWork));
  menuDisp(EMENUID_TOPMENU);
}
/********************************************************/
/* menu task in loop                                    */
/********************************************************/
uint32_t usermainLoopMenu(bool bInit){
  uint32_t ret = DMYSTAT;
  bool bEnd = false;


  bEnd = menustart();/* menu task */
  buzzerproc();
  if(bEnd){
    ret = 0;
  }
  
  return ret;

}

static void buzzerMelody(){
	 // uint8_t i;
	  float sinVal;
	  int toneVal;
	  int getFreq;
	  //stMenuWork.bz.tick = HAL_GetTick();
	  getFreq = melody_fred;
	 // duty = stCacheD.duty;
	// for(i=0;i<360;i+= 20){
	  sinVal = sin(0 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(10 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(20 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(30 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(40 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(50 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(60 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(70 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(80 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(90 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(100 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(110 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(120 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(130 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(140 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(150 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(160 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(170 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	  sinVal = sin(180 * (3.14 / 180));
	  toneVal = getFreq + sinVal * 2500;
	  sound(toneVal, 10);
	 //}
}

static void buzzerOn(){

	stMenuWork.bz.tick = HAL_GetTick();
	melody_fred = stCacheD.freq;
	ledc_timer.duty_resolution = stCacheD.duty;
    ledc_timer_config(&ledc_timer);
	ledc_channel_config(&ledc_channel);
	buzzerMelody(stCacheD.duty);
	ledc_stop(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0,0);
}
static void buzzerOff(){
	ledc_stop(LEDC_HIGH_SPEED_MODE,LEDC_CHANNEL_0,0);
}
static void sound(uint32_t freq,uint32_t duration) {
	ledc_timer.freq_hz    = freq;
	ledc_timer_config(&ledc_timer);
	ledc_channel.hpoint     = 0;
	ledc_channel_config(&ledc_channel);
	vTaskDelay(duration/portTICK_PERIOD_MS);
}
