/*! *********************************************************************************
* Copyright 2022 NXP
* All rights reserved.
*
* \file UART_Helper.h
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

#ifndef __UART_HELPER_H__
#define __UART_HELPER_H__

/*! *********************************************************************************
*************************************************************************************
* Public macros
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */
uint8_t u8AHI_UartReadData(uint8_t u8Uart);
uint16_t u16AHI_UartReadRxFifoLevel(uint8_t u8Uart);
void vAHI_UartDisable(uint8_t u8Uart);
void vAHI_UartEnable(uint8_t u8Uart);

#endif /* __UART_HELPER_H__ */
