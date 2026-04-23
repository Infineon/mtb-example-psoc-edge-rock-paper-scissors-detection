/*******************************************************************************
* File Name        : oob_uart_handler.c
*
* Description      : This file implements the Out-Of-Band (OOB) UART command
*                    parser and response handler. It initializes UART,
*                    handles interrupts, and provides basic TX/RX utilities.
*
* Related Document : See README.md
*
********************************************************************************
* (c) 2025-2026, Infineon Technologies AG, or an affiliate of Infineon
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

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include "cybsp.h"
#include "cy_pdl.h"
#include "oob_uart_handler.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define TENXER_UART_INTR_NUM                  ((IRQn_Type) CYBSP_TENXER_UART_IRQ)
#define TENXER_UART_INTERRUPT_PRIORITY        (4U)

/*******************************************************************************
* Global variable
*******************************************************************************/
cy_stc_scb_uart_context_t tTenxer_uart_Context;

/*******************************************************************************
* Functions
*******************************************************************************/

/*******************************************************************************
* Function Name: uart_isr
********************************************************************************
* Summary:
* UART interrupt service routine that forwards handling to the PDL UART ISR
* for the configured UART hardware and context.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void uart_isr( void )
{
  Cy_SCB_UART_Interrupt(CYBSP_TENXER_UART_HW, &tTenxer_uart_Context);
}

/*******************************************************************************
* Function Name: init_uart
********************************************************************************
* Summary:
* Initializes the UART block, configures and enables its interrupt, and
* enables the UART for operation.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void init_uart( void )
 {
   /* Configure UART to operate */
   (void) Cy_SCB_UART_Init(CYBSP_TENXER_UART_HW, &CYBSP_TENXER_UART_config, &tTenxer_uart_Context);

   /* Populate configuration structure (code specific for CM4) */
   cy_stc_sysint_t uartIntrConfig =
   {
       .intrSrc      = TENXER_UART_INTR_NUM,
       .intrPriority = TENXER_UART_INTERRUPT_PRIORITY,
   };
   /* Hook interrupt service routine and enable interrupt */
   (void) Cy_SysInt_Init(&uartIntrConfig, &uart_isr);
   NVIC_EnableIRQ(TENXER_UART_INTR_NUM);

   __enable_irq();
   /* Enable UART to operate */
   Cy_SCB_UART_Enable(CYBSP_TENXER_UART_HW);
 }

/*******************************************************************************
* Function Name: is_rcv_data_available
********************************************************************************
* Summary:
* Returns the number of bytes currently available in the UART RX FIFO.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: Number of bytes available to read
*
*******************************************************************************/
uint32_t is_rcv_data_available( void )
{
    return Cy_SCB_UART_GetNumInRxFifo(CYBSP_TENXER_UART_HW);
}

/*******************************************************************************
* Function Name: read_single_byte_data
********************************************************************************
* Summary:
* Reads and returns a single byte from the UART RX FIFO.
*
* Parameters:
*  void
*
* Return:
*  char: The byte read from RX FIFO
*
*******************************************************************************/
char read_single_byte_data( void )
{
    return Cy_SCB_UART_Get(CYBSP_TENXER_UART_HW);
}

/*******************************************************************************
* Function Name: send_one_byte_data
********************************************************************************
* Summary:
* Sends a single byte by writing it to the UART TX FIFO.
*
* Parameters:
*  send_char: Byte to transmit
*
* Return:
*  void
*
*******************************************************************************/
void send_one_byte_data( char send_char )
{
    Cy_SCB_UART_Put(CYBSP_TENXER_UART_HW, send_char);
}

/*******************************************************************************
* Function Name: send_data_buffer
********************************************************************************
* Summary:
* Transmits a buffer of bytes over UART using the PDL transmit API.
*
* Parameters:
*  psend_data_buf: Pointer to data buffer to transmit
*  data_len      : Number of bytes to transmit
*
* Return:
*  void
*
*******************************************************************************/
void send_data_buffer( uint8_t *psend_data_buf, uint8_t data_len )
{
    Cy_SCB_UART_Transmit(CYBSP_TENXER_UART_HW, psend_data_buf, data_len, &tTenxer_uart_Context);
}

/*******************************************************************************
* Function Name: uart_de_init
********************************************************************************
* Summary:
* Disables and de-initializes the UART peripheral, freeing associated resources.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void uart_de_init( void )
{
  Cy_SCB_UART_Disable(CYBSP_TENXER_UART_HW, &tTenxer_uart_Context);
  Cy_SCB_UART_DeInit(CYBSP_TENXER_UART_HW);

}

/* [] END OF FILE */