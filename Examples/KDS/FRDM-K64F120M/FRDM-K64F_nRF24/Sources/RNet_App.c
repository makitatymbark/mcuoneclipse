/**
 * \file
 * \brief This is main application file
 * \author (c) 2014 Erich Styger, http://mcuoneclipse.com/
 * \note MIT License (http://opensource.org/licenses/mit-license.html)
 *
 * This module implements the application part of the program.
 */

#include "RNet_App.h"
#include "RStack.h"
#include "RApp.h"
#include "FRTOS1.h"
#include "RPHY.h"
#include "Radio.h"
#include "LED1.h"
#include "LED2.h"
#include "LED3.h"
#include "KEY1.h"

typedef enum {
  RNETA_INITIAL, /* initialization state */
  RNETA_POWERUP, /* powered up the transceiver */
  RNETA_TX_RX /* ready to send and receive data */
} RNETA_State;

static RNETA_State appState = RNETA_INITIAL;

static uint8_t RNETA_HandleRxMessage(RAPP_MSG_Type type, uint8_t size, uint8_t *data, RNWK_ShortAddrType srcAddr, bool *handled, RPHY_PacketDesc *packet) {
  (void)srcAddr;
  (void)packet;
  switch(type) {
    case RAPP_MSG_TYPE_PING: /* <type><size><data */
      *handled = TRUE;
      /* to be defined: do something with the ping, e.g blink a LED */
      LED2_On(); /* green LED blink */
      vTaskDelay(pdMS_TO_TICKS(20));
      LED2_Off();
      return ERR_OK;

    case RAPP_MSG_TYPE_BUTTON:
      *handled = TRUE;
      /* to be defined: do something with the button message, e.g blink a LED */
      LED1_On(); /* red LED blink */
      FRTOS1_vTaskDelay(pdMS_TO_TICKS(20));
      LED1_Off();
      return ERR_OK;

    default:
      break;
  } /* switch */
  return ERR_OK;
}

static const RAPP_MsgHandler handlerTable[] = 
{
  RNETA_HandleRxMessage,
  NULL /* sentinel */
};

static void RadioPowerUp(void) {
  /* need to ensure that we wait at least 100 ms (I use 150 ms here) after power-on of the transceiver */
  TickType_t xTime;
  
  xTime = xTaskGetTickCount();
  if (xTime<(pdMS_TO_TICKS(150))) {
    /* not powered for 100 ms: wait until we can access the radio transceiver */
    xTime = pdMS_TO_TICKS(150)-xTime; /* remaining ticks to wait */
    FRTOS1_vTaskDelay(xTime);
  }
  (void)RADIO_PowerUp(); /* enable the transceiver */
}

static void Process(void) {
  for(;;) {
    switch(appState) {
    case RNETA_INITIAL:
      appState = RNETA_POWERUP;
      continue;
      
    case RNETA_POWERUP:
      RadioPowerUp();
      appState = RNETA_TX_RX;
      break;
      
    case RNETA_TX_RX:
      (void)RADIO_Process();
      break;
  
    default:
      break;
    } /* switch */
    break; /* break for loop */
  } /* for */
}

static void RNetTask(void *pvParameters) {
  uint32_t cntr;
  uint8_t msgCntr;
  TickType_t tickCnt, prevTickCnt;

  (void)pvParameters; /* not used */
  if (RAPP_SetThisNodeAddr(RNWK_ADDR_BROADCAST)!=ERR_OK) { /* set a default address */
    for(;;); /* "ERR: Failed setting node address" */
  }
  cntr = 0; /* initialize LED counter */
  msgCntr = 0; /* initialize message counter */
  appState = RNETA_INITIAL; /* initialize state machine state */
  tickCnt = xTaskGetTickCount();
  for(;;) {
    Process(); /* process state machine */
    KEY1_ScanKeys();
    cntr++;
    if (cntr==500) { /* with an RTOS 10 ms/100 Hz tick rate, this is every 5 second */
      LED3_On(); /* blink blue LED for 20 ms */
      RAPP_SendPayloadDataBlock(&msgCntr, sizeof(msgCntr), RAPP_MSG_TYPE_PING, RNWK_ADDR_BROADCAST, RPHY_PACKET_FLAGS_NONE);
      msgCntr++;
      cntr = 0;
      vTaskDelay(pdMS_TO_TICKS(20));
      LED3_Off(); /* blink blue LED */
    }
    FRTOS1_vTaskDelay(pdMS_TO_TICKS(10));
  } /* for */
}

void RNETA_KeyPressed(uint8_t key) {
  LED1_On();
  LED2_On();
  LED3_On();
  RAPP_SendPayloadDataBlock(&key, sizeof(key), RAPP_MSG_TYPE_BUTTON, RNWK_ADDR_BROADCAST, RPHY_PACKET_FLAGS_NONE);
  vTaskDelay(pdMS_TO_TICKS(10));
  LED1_Off();
  LED2_Off();
  LED3_Off();
}

void RNETA_Init(void) {
  RSTACK_Init(); /* initialize stack */
  if (RAPP_SetMessageHandlerTable(handlerTable)!=ERR_OK) { /* assign application message handler */
    for(;;); /* "ERR: failed setting message handler!" */
  }
  if (xTaskCreate(
        RNetTask,  /* pointer to the task */
        "RNet", /* task name for kernel awareness debugging */
        configMINIMAL_STACK_SIZE, /* task stack size */
        (void*)NULL, /* optional task startup argument */
        tskIDLE_PRIORITY,  /* initial priority */
        (TaskHandle_t*)NULL /* optional task handle to create */
      ) != pdPASS) {
    /*lint -e527 */
    for(;;){}; /* error! probably out of memory */
    /*lint +e527 */
  }
}

