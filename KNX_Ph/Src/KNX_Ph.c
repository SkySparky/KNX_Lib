/**
  ******************************************************************************
  * @file       KNX_Ph.c
  * @author     MA Dingjie
  * @version    V0.0.5
  * @date       20-July-2016
  * @brief      KNX Physical Layer supervisor.
  *             This file provides functions to manage following functionalities:
  *              + Initialization functions
  *              + Send and Receive messages functions
  *              + State functions
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "KNX_Ph.h"
#include "KNX_Aux.h"
#include "KNX_TPUart.h"
#include "cola.h"
#include "debug.h"

/** @defgroup KNX_PH KNX Physical Layer
  * @{
  */

/** @defgroup KNX_PH_Sup KNX Physical Layer Supervisor
  * @{
  */

/* Private variables ---------------------------------------------------------*/
/** @defgroup KNX_PH_Sup_Private_Variables KNX_Ph_Sup Private Variables
  * @{
  */
/** \brief Current state of KNX physical layer. */
static PH_Status_t KNX_PH_STATE;
/** \brief State Debug message sent in \ref Cola_Debug. */
static unsigned char KNX_PH_STATE_DEBUGMSG[] = "[KNX PH]KNX_PH_STATE changed to XX.\r\n";
/** \brief ::KNX_PH_STATE_DEBUGMSG digits indice. */
#define KNX_PH_STATE_DEBUGMSG_INDICE ((uint8_t)32)
/** \brief Sending Debug message sent in \ref Cola_Debug. */
static unsigned char KNX_PH_SEND_DEBUGMSG[] = "[KNX PH]Message sent: /XX/\r\n";
/** \brief ::KNX_PH_SEND_DEBUGMSG digits indice. */
#define KNX_PH_SEND_DEBUGMSG_INDICE ((uint8_t)23)
/** \brief Receiving Debug message sent in \ref Cola_Debug. */
static unsigned char KNX_PH_RECEIVE_DEBUGMSG[] = "[KNX PH]Message received: [XX]\r\n";
/** \brief ::KNX_PH_RECEIVE_DEBUGMSG digits indice. */
#define KNX_PH_RECEIVE_DEBUGMSG_INDICE ((uint8_t)27)
/** \brief Error Debug message sent in \ref Cola_Debug. */
static unsigned char KNX_PH_ERROR_DEBUGMSG[] = "[KNX PH]Error code: 0xXX\r\n";
/** \brief ::KNX_PH_ERROR_DEBUGMSG digits indice. */
#define KNX_PH_ERROR_DEBUGMSG_INDICE ((uint8_t)22)

/** \brief Cola defined in \ref Debug */
t_cola colaDebug;
/**
  * @}
  */

/* Private function prototypes -----------------------------------------------*/
/** @defgroup KNX_PH_Sup_Private_Functions KNX_Ph_Sup Private Functions
  * @{
  */
static uint8_t  request2data(PH_Request_t request);
static void     KNX_Ph_SetState(PH_Status_t state);
static void     KNX_Ph_DebugMessage(uint8_t data, DEBUG_Type_t type);
/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/
/** @defgroup KNX_PH_Sup_Exported_Functions KNX Physical Layer Supervisor Exported Functions
  * @{
  */

/** @defgroup KNX_PH_Sup_Exported_Functions_Group1 Initialization Functions
  * @{
  */

/**
  * @brief      Initialize the \ref KNX_PH module.
  * @retval     Error code, See \ref PH_Error_Code.
  */
uint8_t KNX_Ph_Init(void)
{  
  /** Set state to ::PH_NOINIT */
  KNX_Ph_SetState(PH_NOINIT);
  
  /** Initialize the timer. */
  KNX_InitTimer();
  /** Initialize TPUart. */
  if(KNX_PH_TPUart_init() == TPUart_ERROR)
  {
    KNX_Ph_DebugMessage(PH_ERROR_INIT, ERROR_DEBUG);
    
    /** \b If TPUart initialization failed, return ::PH_ERROR_INIT */
    return PH_ERROR_INIT;
  }
      
  return PH_ERROR_NONE;
}

/**
  * @}
  */

/** @defgroup KNX_PH_Sup_Exported_Functions_Group2 Send/Receive Functions
  * @{
  */

/**
  * @brief      Send a request.
  * @param      request: a request, see ::PH_Request_t.
  * @param      timeout: timeout duration.
  * @retval     Error code, See \ref PH_Error_Code.
  */
uint8_t KNX_Ph_Send(PH_Request_t request, uint32_t timeout)
{
  uint8_t data;
  
  /** Convert the request to \c uint8_t data. */
  data = request2data(request);
  
  /** \b If the request is not valid, return ::PH_ERROR_REQUEST. */
  if(data == U_None)
  {
    return PH_ERROR_REQUEST;
  }
    
  KNX_StartTimer(timeout);
  /** Try to send the data */
  while(KNX_GetTimerState() != TIMER_TIMEOUT)
  {
    if(KNX_PH_TPUart_Send(&data, 1) == TPUart_OK)
    {
      KNX_Ph_DebugMessage(data, SEND_DEBUG);
      
      KNX_ResetTimer();
      /** \b If succeeded, return ::PH_ERROR_NONE. */
      return PH_ERROR_NONE;
    }
  }
  
  KNX_ResetTimer();
  KNX_Ph_DebugMessage(PH_ERROR_TIMEOUT, ERROR_DEBUG);

  /** \b If timeout, return ::PH_ERROR_TIMEOUT. */
  return PH_ERROR_TIMEOUT;
}

/**
  * @brief      Wait for a response with timeout.
  * @param      res: response got.
  * @param      timeout: timeout duration.
  * @retval     Error code, See \ref PH_Error_Code.
  */
uint8_t KNX_Ph_WaitFor(uint8_t res, uint32_t timeout)
{
  uint8_t data;
  
  KNX_StartTimer(timeout);
  /** Try to receive the data */
  while(KNX_GetTimerState() != TIMER_TIMEOUT)
  {
    if(KNX_PH_TPUart_Receive(&data, 1) == TPUart_OK)
    {      
      /** \b If data received is the response expected. */
      if(data == res)
      {
        KNX_ResetTimer();
        KNX_Ph_DebugMessage(data, RECEIVE_DEBUG);

        /** Return ::PH_ERROR_NONE. */
        return PH_ERROR_NONE;
      }
    }
  }
  
  KNX_ResetTimer();
  KNX_Ph_DebugMessage(PH_ERROR_TIMEOUT, ERROR_DEBUG);

  /** \b If timeout, return ::PH_ERROR_TIMEOUT. */
  return PH_ERROR_RESPONSE;
}

/**
  * @brief      Wait for a type of response with mask and timeout.
  * @param      res: response got.
  * @param      resMask: mask of the response expected.
  * @param      timeout: timeout duration.
  * @retval     Error code, See \ref PH_Error_Code.
  */
uint8_t KNX_Ph_WaitForWithMask(uint8_t *res, uint8_t resMask, uint32_t timeout)
{
  uint8_t data;
  
  KNX_StartTimer(timeout);
  /** Try to receive the data */
  while(KNX_GetTimerState() != TIMER_TIMEOUT)
  {
    if(KNX_PH_TPUart_Receive(&data, 1) == TPUart_OK)
    {      
      /** \b If data received is the type of response expected. */
      if((data & resMask) == resMask)
      {
        KNX_ResetTimer();
        *res = data;
        KNX_Ph_DebugMessage(data, RECEIVE_DEBUG);

        /** Return ::PH_ERROR_NONE. */
        return PH_ERROR_NONE;
      }
    }
  }
  
  KNX_ResetTimer();
  KNX_Ph_DebugMessage(PH_ERROR_TIMEOUT, ERROR_DEBUG);

  /** \b If timeout, return ::PH_ERROR_TIMEOUT. */
  return PH_ERROR_RESPONSE;
}
/**
  * @}
  */

/** @defgroup KNX_PH_Sup_Exported_Functions_Group3 KNX PH Services
* @{
*/

/**
  * @brief      Request to reset the \ref KNX_PH module and waiting for the
  *             ::Reset_indication.
  * @retval     Error code, See \ref PH_Error_Code.
  */
uint8_t KNX_Ph_Reset_request(void)
{
  uint8_t ret;
  
  /** Send ::Ph_Reset request. */
  ret = KNX_Ph_Send(Ph_Reset, KNX_DEFAULT_TIMEOUT);
  if(ret != PH_ERROR_NONE)
  {
    /** \b If encounter a problem, return ::PH_ERROR_REQUEST  */
    return PH_ERROR_REQUEST;
  }
    
  /** Waiting for the ::Reset_indication. */
  ret = KNX_Ph_WaitFor(Reset_indication, KNX_DEFAULT_TIMEOUT);
  if(ret == PH_ERROR_NONE)
  {
    /** \b If receive the response, set state to ::PH_RESET. */
    KNX_Ph_SetState(PH_RESET);

    /** Return ::PH_ERROR_NONE. */
    return PH_ERROR_NONE;
  }
  else
  {
    /** Else return ::PH_ERROR_RESPONSE. */
    return PH_ERROR_TIMEOUT;
  }
}

/**
  * @brief      Requests the internal communication state from the TP-UART-IC
  *             and waiting for the ::State_indication.
  * @retval     Error code, See \ref PH_Error_Code.
  */
uint8_t KNX_Ph_State_request(void)
{
  uint8_t ret, res;
  
  /** Send ::Ph_Reset request. */
  ret = KNX_Ph_Send(Ph_State, KNX_DEFAULT_TIMEOUT);
  if(ret != PH_ERROR_NONE)
  {
    /** \b If encounter a problem, return ::PH_ERROR_REQUEST  */
    return PH_ERROR_REQUEST;
  }
    
  /** Waiting for the ::State_indication. */
  ret = KNX_Ph_WaitForWithMask(&res, State_indication_mask, KNX_DEFAULT_TIMEOUT);
  if(ret == PH_ERROR_NONE)
  {
    if(res == State_indication)
    {
      /** \b If receive the response, set state to ::PH_NORMAL. */
      KNX_Ph_SetState(PH_NORMAL);
      
      /** Return ::PH_ERROR_NONE. */
      return PH_ERROR_NONE;
    }
    
    KNX_Ph_DebugMessage(PH_ERROR_STATE, ERROR_DEBUG);
    /** Else return ::PH_ERROR_STATE. */
    return PH_ERROR_STATE;
  }
  else
  {
    /** Else return ::PH_ERROR_RESPONSE. */
    return PH_ERROR_TIMEOUT;
  }
}
/**
  * @}
  */

/** @defgroup KNX_PH_Sup_Exported_Functions_Group4 KNX PH Supervisor State Function
  * @{
  */

/**
 *  @brief      Getter of the status of the physical layer. 
 *  @retval     Physical Layer's status: ::PH_Status_t.
 */
PH_Status_t KNX_Ph_GetState(void)
{
  return KNX_PH_STATE;
}
/**
  * @}
  */

/**
  * @}
  */

/* Private function ----------------------------------------------------------*/
/** @addtogroup KNX_PH_Sup_Private_Functions
  * @{
  */

/**
 *  @brief      Convert a request to a data. 
 *  @param      request: the request in ::PH_Request_t.
 *  @retval     The 8 bits data.
 */
static uint8_t  request2data(PH_Request_t request)
{
  switch(request)
  {
    case Ph_Reset:
      return 0x01U;
    case Ph_State:
      return 0x02U;
    case Ph_ActivateBusmon:
      return 0x05U;
    case Ph_ProductID:
      return 0x20U;
    case Ph_ActivateBusyMode:
      return 0x21U;
    case Ph_ResetBusyMode:
      return 0x22U;
    case Ph_SetAddress:
      return 0x28U;
    default:
      return 0x00U;
  }
}

/**
 *  @brief      Set the status of Physical layer. 
 *  @param      state: the state.
 */
static void     KNX_Ph_SetState(PH_Status_t state)
{
  /** Change the ::KNX_PH_STATE to \b state */
  KNX_PH_STATE = state;
  /** Send the debug message that the status has changed */
  KNX_Ph_DebugMessage(state, STATE_DEBUG);
}

/**
 *  @brief      Send the debug message to indicate the change of the state. 
 *  @param      data: the data or error code.
 *  @param      type: the type of the message, see ::DEBUG_Type_t.
 */
static void KNX_Ph_DebugMessage(uint8_t data, DEBUG_Type_t type)
{ 
  switch(type)
  {
    case STATE_DEBUG:
      int2text(data, &KNX_PH_STATE_DEBUGMSG[KNX_PH_STATE_DEBUGMSG_INDICE]);
      cola_guardar(&colaDebug, KNX_PH_STATE_DEBUGMSG);
      break;
    case SEND_DEBUG:
      int2text(data, &KNX_PH_SEND_DEBUGMSG[KNX_PH_SEND_DEBUGMSG_INDICE]);
      cola_guardar(&colaDebug, KNX_PH_SEND_DEBUGMSG);
      break;
    case RECEIVE_DEBUG:
      int2text(data, &KNX_PH_RECEIVE_DEBUGMSG[KNX_PH_RECEIVE_DEBUGMSG_INDICE]);
      cola_guardar(&colaDebug, KNX_PH_RECEIVE_DEBUGMSG);
      break;
    default:
      int2text(data, &KNX_PH_ERROR_DEBUGMSG[KNX_PH_ERROR_DEBUGMSG_INDICE]);
      cola_guardar(&colaDebug, KNX_PH_ERROR_DEBUGMSG);
  }
}
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */