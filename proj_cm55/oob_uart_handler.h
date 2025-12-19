/*******************************************************************************
* File Name        : oob_uart_handler.h
*
* Description      : Public API for the Out-Of-Band (OOB) UART handler utilities.
*
* Related Document : See README.md
*
********************************************************************************
 * (c) 2025, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 * This software, associated documentation and materials ("Software") is
 * owned by Infineon Technologies AG or one of its affiliates ("Infineon")
 * and is protected by and subject to worldwide patent protection, worldwide
 * copyright laws, and international treaty provisions. Therefore, you may use
 * this Software only as provided in the license agreement accompanying the
 * software package from which you obtained this Software. If no license
 * agreement applies, then any use, reproduction, modification, translation, or
 * compilation of this Software is prohibited without the express written
 * permission of Infineon.
 *
 * Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
 * IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
 * THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
 * SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
 * Infineon reserves the right to make changes to the Software without notice.
 * You are responsible for properly designing, programming, and testing the
 * functionality and safety of your intended application of the Software, as
 * well as complying with any legal requirements related to its use. Infineon
 * does not guarantee that the Software will be free from intrusion, data theft
 * or loss, or other breaches ("Security Breaches"), and Infineon shall have
 * no liability arising out of any Security Breaches. Unless otherwise
 * explicitly approved by Infineon, the Software may not be used in any
 * application where a failure of the Product or any consequences of the use
 * thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

#ifndef OOB_UART_HANDLER_H
#define OOB_UART_HANDLER_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
/* UART interrupt service routine. */
void uart_isr( void );

/* Returns number of bytes currently available in the UART RX FIFO. */
uint32_t is_rcv_data_available( void );

/* Reads a single byte from the UART RX FIFO. */
char read_single_byte_data( void );

/* Sends a single byte over UART. */
void send_one_byte_data( char send_char );

/* Initializes the UART hardware and enables the UART/interrupt. */
void init_uart( void );

/* Transmits a buffer of data over UART. */
void send_data_buffer( uint8_t *psend_data_buf, uint8_t data_len );

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* OOB_UART_HANDLER_H */

/* [] END OF FILE */
