/*************************************************************************************************/
/*!
 *  \file   tipc_api.c
 *
 *  \brief  Time profile client.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *
 *  Copyright (c) 2011 Wicentric, Inc., all rights reserved.
 *  Wicentric confidential and proprietary.
 *
 *  IMPORTANT.  Your use of this file is governed by a Software License Agreement
 *  ("Agreement") that must be accepted in order to download or otherwise receive a
 *  copy of this file.  You may not use or copy this file for any purpose other than
 *  as described in the Agreement.  If you do not agree to all of the terms of the
 *  Agreement do not use this file and delete all copies in your possession or control;
 *  if you do not have a copy of the Agreement, you must contact Wicentric, Inc. prior
 *  to any use, copying or further distribution of this software.
 */
/*************************************************************************************************/
#ifndef TIPC_API_H
#define TIPC_API_H

#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Current Time service enumeration of handle indexes of characteristics to be discovered */
enum
{
  TIPC_CTS_CT_HDL_IDX,      /*! Current time */
  TIPC_CTS_CT_CCC_HDL_IDX,  /*! Current time client characteristic configuration descriptor */
  TIPC_CTS_LTI_HDL_IDX,     /*! Local time information */
  TIPC_CTS_RTI_HDL_IDX,     /*! Reference time information */
  TIPC_CTS_HDL_LIST_LEN     /*! Handle list length */
};  

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     TipcCtsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Current Time service.  Parameter 
 *          pHdlList must point to an array of length TIPC_CTS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void TipcCtsDiscover(dmConnId_t connId, uint16_t *pHdlList);

/*************************************************************************************************/
/*!
 *  \fn     TipcCtsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length TIPC_CTS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t TipcCtsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg);

#ifdef __cplusplus
};
#endif

#endif /* TIPC_API_H */
