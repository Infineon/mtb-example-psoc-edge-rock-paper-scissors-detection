/*******************************************************************************
* File Name        : oob_command_handler.h
*
* Description      : Protocol definitions and public APIs for the Out-Of-Band 
*                    (OOB) UART command parser and minimal response handling.
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

#ifndef OOB_COMMAND_HANDLER_H
#define OOB_COMMAND_HANDLER_H

#ifdef TENXER_UART

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define OOB_REQ_START_BYTE              0xCC
#define OOB_RSP_START_BYTE              0xAA

#define OOB_RSP_ACK                     0xFA
#define OOB_RSP_NAK                     0xFB

#define OOB_SEQ_MIN                     0x01
#define OOB_SEQ_MAX                     0xFF

#define OOB_REQ_MIN_LEN_NO_PAYLOAD      0x03 /* Seq(1)+App(1)+Op(1) */
#define OOB_REQ_MAX_PAYLOAD             (64u)

#define APP_CMD_SEND_DATA_LEN           0x04
#define APP_CMD_RESET_VALUE             0x00
#define APP_CMD_START_BYTE_INDEX        0x00
#define APP_CMD_FRAME_INDEX             0x01
#define APP_CMD_SEQUENCE_NO_INDEX       0x02
#define APP_CMD_APP_IDENTIFIER_INDEX    0x03
#define APP_CMD_COMMAND_OP_CODE_INDEX   0x04

#define APP_CMD_SEND_DATA_VALUE         0x02
#define APP_CMD_EVENT_RSP_VALUE         0xBB
#define APP_CMD_MAX_NO_SEND_EVENT_VALUE 0x03
#define APP_CMD_MAX_NO_VALUE            0xFF
#define APP_CMD_SEND_EVT_DELAY_IN_MS    100

/* Application Identifiers */
#define OOB_APP_GLOBAL                  0x00
#define OOB_APP_EDGE_DEMO_CAROUSEL      0x01
#define OOB_APP_MUSIC_PLAYER            0x02
#define OOB_APP_FACE_ID                 0x03
#define OOB_APP_RPS                     0x04
#define OOB_APP_VIDEO_PLAYER            0x05
#define OOB_APP_RPS_DETECTION           0x06
#define OOB_ACTIVE_APP_EDGE_DEMO_CAROUSEL

/* Global (AppID 0x00) */
#define WHO_AM_I                        0xDD
#define BACK_TO_CAROUSEL                0xEE

/* Edge Demo (AppID 0x01) */
#define ED_HOME_SWIPE_LEFT              0x01
#define ED_HOME_SWIPE_RIGHT             0x02
#define ED_HOME_SELECT_OOB              0x03
#define ED_HOME_GFXTOUCH_MODE           0x04
#define ED_HOME_VOICE_MODE              0x05
#define ED_HOME_GESTURE_MODE            0x06
#define ED_OOBINFO_CLOSE                0x07
#define ED_OOBINFO_PLAY_OOB             0x08

/* Music Player (AppID 0x02) */
#define MP_HOME_TOGGLE_PLAY_PAUSE       0x01
#define MP_HOME_NEXT                    0x02
#define MP_HOME_PREV                    0x03
#define MP_HOME_TOGGLE_SHUFFLE          0x04
#define MP_HOME_TOGGLE_REPEAT           0x05
#define MP_HOME_VOL_UP_ONE_STEP         0x06
#define MP_HOME_VOL_DOWN_ONE_STEP       0x07
#define MP_HOME_INFO                    0x08
#define MP_HOME_INFO_CLOSE              0x09
#define MP_HOME_SHOW_TRACK_LIST         0x0A
#define MP_TLIST_BACK_TO_PLAYER         0x0B
#define MP_TLIST_TOGGLE_PLAY_PAUSE_T1   0x0C
#define MP_TLIST_TOGGLE_PLAY_PAUSE_T2   0x0D
#define MP_TLIST_TOGGLE_PLAY_PAUSE_T3   0x0E
#define MP_TLIST_TOGGLE_PLAY_PAUSE_T4   0x0F
#define MP_TLIST_TOGGLE_PLAY_PAUSE_T5   0x10

/* Face ID (AppID 0x03) */
#define FID_HOME_START_ENROLLMENT       0x01
#define FID_HOME_CANCLE_ENROLLMENT      0x02
#define FID_HOME_CLEAR_ENROLLED_USERS   0x03

/* Task Configuration */
#define OOB_UART_TASK_NAME              ("TenXer UART Task")
#define OOB_UART_TASK_STACK_SIZE        (configMINIMAL_STACK_SIZE)
#define OOB_UART_TASK_PRIORITY          (5U)


/*******************************************************************************
* Global variable
*******************************************************************************/
/* The application sets this to indicate current app context. */
extern volatile uint8_t g_oob_current_app_id;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
/* Polling task: parses frames and sends feedback responses. */
void oob_uart_cmd_task(void *pvParameters);

/* Builds and sends the response frame for a given Seq/App/Opcode and status. */
void oob_cmd_send_response(uint8_t seq, uint8_t app, uint8_t opcode, bool success);

/* Navigates to the home screen. */
void go_to_home_screen(void);

/* De-initializes the UART interface. */
void uart_de_init( void );

/* Lite version of response function: sends only 4 bytes  */
void oob_cmd_send_response_lite(uint8_t seq, uint8_t app, uint8_t opcode, bool success);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /*TENXER_UART*/

#endif /* OOB_COMMAND_HANDLER_H */

/* [] END OF FILE */