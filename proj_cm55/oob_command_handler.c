/*******************************************************************************
* File Name        : oob_command_handler.c
*
* Description      : This file implements the Out-Of-Band (OOB) UART command
*                    parser and response handler. It processes incoming UART
*                    frames, extracts command information, and sends
*                    appropriate responses/events.
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
#ifdef TENXER_UART
/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "oob_uart_handler.h"
#include "oob_command_handler.h"
#include "lcd_task.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define OOB_UART_BUFFER_SIZE_MAX        64   /* Max UART buffer size : 1+1+(3+48)=~64 bytes */
#define OOB_UART_BUFFER_SIZE_MIN        16   /* Min UART buffer size: 1+1+(3+8)=~16 bytes */
#define OOB_UART_TASK_DELAY_MS          (50U)   /* Task delay to keep CPU cool */
#define OOB_MCU_RESET_DELAY_MS          (1U)    /* Delay before and after UART de-init during MCU reset */
#define OOB_RSP_FRAME_LENGTH            (0x02U) /* Response frame length for lite responses */

/*******************************************************************************
* Global Variables
*******************************************************************************/
/*The application should set this to the active app context (0x01..0x05).*/
volatile uint8_t g_oob_current_app_id = OOB_APP_RPS_DETECTION; /*default application*/

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void mcu_soft_reset(void);

/*******************************************************************************
* Functions
*******************************************************************************/

/*******************************************************************************
* Function Name: send_carousel_event
********************************************************************************
* Summary:
* Sends a sequence of carousel demo events to the host over UART, cycling
* through predefined event values with delays between transmissions.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void send_carousel_event(void)
{
    uint8_t send_cmd_data[APP_CMD_SEND_DATA_LEN];
    uint8_t event_send_value;
    send_cmd_data[APP_CMD_START_BYTE_INDEX] = OOB_RSP_START_BYTE;
    send_cmd_data[APP_CMD_FRAME_INDEX] = APP_CMD_SEND_DATA_VALUE;
    send_cmd_data[APP_CMD_SEQUENCE_NO_INDEX] = APP_CMD_RESET_VALUE;
    send_cmd_data[APP_CMD_APP_IDENTIFIER_INDEX] = APP_CMD_EVENT_RSP_VALUE;
    for (event_send_value = APP_CMD_RESET_VALUE; event_send_value < APP_CMD_MAX_NO_SEND_EVENT_VALUE; event_send_value++)
    {
        send_data_buffer(send_cmd_data, APP_CMD_SEND_DATA_LEN);
        vTaskDelay(pdMS_TO_TICKS(APP_CMD_SEND_EVT_DELAY_IN_MS));
    }
}

/*******************************************************************************
* Function Name: oob_cmd_send_response_lite
********************************************************************************
* Summary:
* Forms and sends a standard response frame (ACK/NAK) for a received command.
* Special-cases WHO_AM_I and BACK_TO_CAROUSEL for global app ID responses.
*
* Parameters:
*  seq    : Sequence number from the received command
*  app    : Application identifier
*  opcode : Opcode of the received command
*  success: true if command handled successfully, false otherwise
*
* Return:
*  void
*
*******************************************************************************/
void oob_cmd_send_response_lite(uint8_t seq, uint8_t app, uint8_t opcode, bool success)
{
    /*Decide payload byte*/
    uint8_t resp_byte = success ? OOB_RSP_ACK : OOB_RSP_NAK;

    /*Special case: global WHO_AM_I / BACK_TO_CAROUSEL return current app id*/
    if ((app == OOB_APP_GLOBAL) && (opcode == WHO_AM_I || opcode == BACK_TO_CAROUSEL))
    {
        resp_byte = (uint8_t)g_oob_current_app_id;
    }

    Cy_SCB_UART_Put(CYBSP_TENXER_UART_HW, (uint32_t)OOB_RSP_START_BYTE);
    Cy_SCB_UART_Put(CYBSP_TENXER_UART_HW, (uint32_t)OOB_RSP_FRAME_LENGTH);    
    Cy_SCB_UART_Put(CYBSP_TENXER_UART_HW, (uint32_t)seq);
    Cy_SCB_UART_Put(CYBSP_TENXER_UART_HW, (uint32_t)resp_byte);
}

/*******************************************************************************
* Function Name: oob_cmd_send_response
********************************************************************************
* Summary:
* Forms and sends a standard response frame (ACK/NAK) for a received command.
* Special-cases WHO_AM_I and BACK_TO_CAROUSEL for global app ID responses.
*
* Parameters:
*  seq    : Sequence number from the received command
*  app    : Application identifier
*  opcode : Opcode of the received command
*  success: true if command handled successfully, false otherwise
*
* Return:
*  void
*
*******************************************************************************/
void oob_cmd_send_response(uint8_t seq, uint8_t app, uint8_t opcode, bool success)
{
    uint8_t send_rep_data[APP_CMD_SEND_DATA_LEN];
    send_rep_data[APP_CMD_START_BYTE_INDEX] = OOB_RSP_START_BYTE;
    send_rep_data[APP_CMD_FRAME_INDEX] = APP_CMD_SEND_DATA_VALUE;
    send_rep_data[APP_CMD_SEQUENCE_NO_INDEX] = seq;

    /*Decide payload byte*/
    uint8_t resp_byte = success ? OOB_RSP_ACK : OOB_RSP_NAK;

    /*Special case: global WHO_AM_I / BACK_TO_CAROUSEL return current app id*/
    if ((app == OOB_APP_GLOBAL) && (opcode == WHO_AM_I || opcode == BACK_TO_CAROUSEL))
    {
        resp_byte = (uint8_t)g_oob_current_app_id;
    }
    send_rep_data[APP_CMD_APP_IDENTIFIER_INDEX] = resp_byte;
    send_data_buffer(send_rep_data, APP_CMD_SEND_DATA_LEN);
}

/*******************************************************************************
* Function Name: oob_uart_cmd_task
********************************************************************************
* Summary:
* Polling UART receive task. Accumulates bytes into a frame buffer, validates
* frames, parses fields (sequence, app, opcode, payload), dispatches actions
* based on the active application, and sends feedback to the host.
*
* Parameters:
*  pvparameters: FreeRTOS task parameter (unused)
*
* Return:
*  void
*
*******************************************************************************/
void oob_uart_cmd_task(void *pvparameters)
{
    uint8_t rx_char;
    size_t uart_available_chunk;
    uint8_t rx_frame_buff[OOB_UART_BUFFER_SIZE_MAX];
    uint8_t rx_index = APP_CMD_RESET_VALUE;          /*buffered so far*/
    uint8_t rx_want = APP_CMD_RESET_VALUE;           /*total wire bytes expected: 2 + Len*/

    uint8_t frame_len;
    uint8_t cmd_seq;
    uint8_t app_name;
    uint8_t app_opcode;
    bool frame_ok = true;

    init_uart();
    for (;;)
    {
        uart_available_chunk = is_rcv_data_available();

        if (uart_available_chunk)
        {
            rx_char = Cy_SCB_UART_Get(CYBSP_TENXER_UART_HW);

            if (APP_CMD_RESET_VALUE == rx_index)
            {
                if (OOB_REQ_START_BYTE == rx_char)
                {
                    rx_frame_buff[rx_index++] = rx_char;      /* Start */
                }
                /* else ignore until Start */
            }
            else if (APP_CMD_FRAME_INDEX == rx_index)
            {
                rx_frame_buff[rx_index++] = rx_char;             /* Len (excludes Start & Len) */
                /* expected wire size = Start(1) + Len(1) + Len */
                rx_want = (uint8_t)(APP_CMD_SEQUENCE_NO_INDEX + rx_char);

                /* Quick sanity: must at least hold Seq+App+Opcode */
                if (rx_char < OOB_REQ_MIN_LEN_NO_PAYLOAD || rx_want > OOB_UART_BUFFER_SIZE_MAX)
                {
                    /* invalid length; resync */
                    rx_index = APP_CMD_RESET_VALUE;
                    rx_want = APP_CMD_RESET_VALUE;
                }
            }
            else
            {
                if (rx_index < OOB_UART_BUFFER_SIZE_MAX)
                    {
                    rx_frame_buff[rx_index++] = rx_char;
                }
                if (rx_want && rx_index == rx_want)
                {
                    /* ---- Validate frame ---- */
                    frame_len = rx_frame_buff[APP_CMD_FRAME_INDEX];           /* excludes start & len */
                    cmd_seq = rx_frame_buff[APP_CMD_SEQUENCE_NO_INDEX];
                    app_name = rx_frame_buff[APP_CMD_APP_IDENTIFIER_INDEX];
                    app_opcode  = rx_frame_buff[APP_CMD_COMMAND_OP_CODE_INDEX];

                    /* payload length derived from len: len = 3 + payload_len */
                    uint8_t payload_len = (frame_len >= APP_CMD_APP_IDENTIFIER_INDEX) ? (uint8_t)(frame_len - \
                                        APP_CMD_APP_IDENTIFIER_INDEX) : APP_CMD_MAX_NO_VALUE;

                    /* Basic checks */
                    if (rx_frame_buff[APP_CMD_RESET_VALUE] != OOB_REQ_START_BYTE)
                    {
                        frame_ok = false;
                    }
                    else if (cmd_seq == APP_CMD_RESET_VALUE)
                    {
                        frame_ok = false;
                    }
                    else if (payload_len > OOB_REQ_MAX_PAYLOAD)
                    {
                        frame_ok = false;
                    }
                    else
                    {
                        frame_ok = true;
                    }
                        
                    /* Compare against current app context if AppID != 0x00 */
                    if (frame_ok && app_name != OOB_APP_GLOBAL && app_name != (uint8_t)g_oob_current_app_id)
                         frame_ok = false;
                    /* Opcode checks per AppID */
                    if (frame_ok)
                    {
                        frame_ok = true; /* default to false, only set true for valid command */
                        switch (app_name)
                        {
                            case OOB_APP_GLOBAL:
                                if(app_opcode == WHO_AM_I)
                                {
                                    frame_ok = true;
                                    oob_cmd_send_response_lite(cmd_seq, app_name, app_opcode, 
                                                               frame_ok);
                                }
                                else if(app_opcode == BACK_TO_CAROUSEL)
                                {
                                    frame_ok = true;
                                    oob_cmd_send_response_lite(cmd_seq, app_name, app_opcode, 
                                                               frame_ok);
                                    vTaskDelay(pdMS_TO_TICKS(OOB_MCU_RESET_DELAY_MS));
                                    uart_de_init();
                                    vTaskDelay(pdMS_TO_TICKS(OOB_MCU_RESET_DELAY_MS));
                                    mcu_soft_reset();
                                }
                                break;

                           
                            default:
                                frame_ok = false;
                                oob_cmd_send_response_lite(cmd_seq, app_name, app_opcode, 
                                                           frame_ok);
                                break;
                        }
                    }
                    else
                    {
                        frame_ok = false;
                        oob_cmd_send_response_lite(cmd_seq, app_name, app_opcode, frame_ok);
                    }

                    /* Reset for next frame */
                    rx_index = APP_CMD_RESET_VALUE;
                    rx_want = APP_CMD_RESET_VALUE;
                }
                /* Overflow guard */
                if (rx_index >= OOB_UART_BUFFER_SIZE_MAX)
                {
                    rx_index = APP_CMD_RESET_VALUE;
                    rx_want = APP_CMD_RESET_VALUE;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(OOB_UART_TASK_DELAY_MS)); /* keep CPU cool, same cadence you use */
    }
}
#endif /*TENXER_UART*/
/* [] END OF FILE */