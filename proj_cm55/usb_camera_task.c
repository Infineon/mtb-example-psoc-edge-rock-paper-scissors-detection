/*******************************************************************************
* File Name        : usb_camera_task.c
*
* Description      : This file implements the USB Webcam functions.
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

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cybsp.h"
#include "cy_pdl.h"
/* FreeRTOS header file */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "USBH.h"
#include "USBH_Util.h"
#include "USBH_VIDEO.h"
#include <stdio.h>
#include "definitions.h"
#include "lcd_task.h"
#include "usb_camera_task.h"
#include "time_utils.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* USB task configuration macros */
#define USBH_TASK_STACK_SIZE_BYTES          ( 8192U )

/* USB device configuration macros */
#define USB_DEVICE_CONNECTED                ( 1U )
#define USB_DEVICE_DISCONNECTED             ( 0U )
#define USB_DISCONNECT_EVENT                ( 0xFFU )

/* Frame processing macros */
#define FRAME_TIMEOUT_MS                    ( 10000U )  /* 10 seconds */
#define DEVICE_INIT_TIMEOUT_MS              ( 5000.0f )   /* 5 seconds */
#define TERMINAL_ENUM_DELAY_MS              ( 10U )
#define FRAME_DISCOVERY_DELAY_MS            ( 10U )

/* Error handling macros */
#define USB_STREAM_ERROR_THRESHOLD          ( 10U )

/* Buffer management macros */
#define FRAME_BYTES_RESET                   ( 0U )
#define QUEUE_GET_TIMEOUT_MS                ( 100U )

#define USB_DEVICE_DISCONNECT_WAIT_MS       (50U)
#define USB_DEFAULT_FRAME_INTERVAL_IDX      (1U)
#define USB_SECONDS_TO_100NS                (10000000U)

#define INITIAL_VAL                         (0)

/*******************************************************************************
* Global Variables
*******************************************************************************/
static TaskHandle_t usbh_main_task_handle;
static TaskHandle_t usbh_isr_task_handle;

static QueueHandle_t video_mail_box;
static QueueHandle_t device_state_mailbox;

static U8 avideo_dev_Indexes[MAX_VIDEO_INTERFACES] __attribute__((used));
static char _ac[128];
static char ac_mb_event[1] __attribute__((used));
static USBH_NOTIFICATION_HOOK   Hook;

uint8_t _device_connected;
uint8_t lastBuffer;
bool logitech_camera_enabled = 0;
bool point3mp_camera_enabled = 0;
bool twomp_camera_enabled = 0;
bool camera_not_supported = 0;

/* Frame monitoring variables */
float last_frame_time = 0;
float last_check_time = 0;
static int stream_err_cnt;
float frame_timeout_ms = 10000; /* 10 seconds without frames is considered stalled */

cy_queue_t _NewFrameMailBox;

video_buffer_t    image_buff[NUM_IMAGE_BUFFERS];

/*******************************************************************************
* Functions
*******************************************************************************/

/*******************************************************************************
* Function Name: USBH_Task_Wrapper
********************************************************************************
* Description:
* Task wrapper function for the USB host task. This function is responsible for
* invoking the USBH_Task function, which performs the actual USB host operations
* The function takes a void pointer as an argument, but it is not used
*
* Input Arguments:
*   void *arg - Unused argument
*
*
*
* Return Value:
*   None
*
********************************************************************************/
void USBH_Task_Wrapper(void *arg) {
    (void)arg;
    USBH_Task();
}

/*******************************************************************************
* Function Name: USBH_Task_Wrapper_new
********************************************************************************
* Description:
* Task wrapper function for the USB host interrupt service task. This function
* is responsible for invoking the USBH_ISRTask function, which performs the actual
* USB host interrupt service operations. The function takes a void pointer as an
* argument, but it is not used.
*
* Input Arguments:
*   void *arg - Unused argument
*
*
*
* Return Value:
*   None
*
********************************************************************************/
void USBH_Task_Wrapper_new(void *arg) {
    (void)arg;
    USBH_ISRTask();
}

/*******************************************************************************
* Function Name: cb_Onadd_remove_device
********************************************************************************
* Description:
*   Callback function invoked when a USB device is added or removed. Executes in
*   the context of the USBH_Task and handles device connection/disconnection events
*   by updating buffers, logging events, and signaling tasks. Non-blocking.
*
* Input Arguments:
*   void *pContext - Context pointer (not used)
*   U8 DevIndex - Index of the device
*   USBH_DEVICE_EVENT Event - Event type (add or remove)
*
* Return Value:
*   None
*
*******************************************************************************/
static void cb_Onadd_remove_device ( void *pContext, U8 DevIndex, USBH_DEVICE_EVENT Event )
{
    (void) pContext;

    switch ( Event )
    {
        case USBH_DEVICE_EVENT_ADD :
            _device_connected = USB_DEVICE_CONNECTED;
            USBH_Logf_Application("**** VIDEO Device added [%d]\n", DevIndex);
            for (int i = INITIAL_VAL; i < NUM_IMAGE_BUFFERS; i++)
            {
                image_buff[i].BufReady = INITIAL_VAL;
                image_buff[i].NumBytes = INITIAL_VAL;
            }
            lastBuffer = INITIAL_VAL;
            __DMB();
           xQueueSend(video_mail_box, &DevIndex, INITIAL_VAL);

            break;
         case USBH_DEVICE_EVENT_REMOVE :
            _device_connected = USB_DEVICE_DISCONNECTED;
            USBH_Logf_Application("**** VIDEO Device removed [%d]\n", DevIndex);

            /* Reset buffer states */
            for (int i = 0; i < NUM_IMAGE_BUFFERS; i++) 
            {
                image_buff[i].BufReady = INITIAL_VAL;
                image_buff[i].NumBytes = INITIAL_VAL;
            }
            __DMB();
            /* Signal main task about disconnection */
            U8 disconnectEvent = USB_DISCONNECT_EVENT;
            xQueueSend(device_state_mailbox, &disconnectEvent, INITIAL_VAL);

            xSemaphoreGive(model_semaphore);
            stream_err_cnt = INITIAL_VAL;
            break;
        default :
            break; /* Should never happen */
    }
}

CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: usb_cb_on_data
********************************************************************************
* Description:
*   Callback function invoked when video data is received from a USB video device.
*   Manages frame buffering, error handling, and stream state for video data
*   processing. Updates buffer states, signals semaphores, and handles frame
*   completion or errors.
*
* Input Arguments:
*   USBH_VIDEO_DEVICE_HANDLE hDevice - Handle to the video device (not used)
*   USBH_VIDEO_STREAM_HANDLE hStream - Handle to the video stream
*   USBH_STATUS Status - Status of the data transfer
*   const U8 *pData - Pointer to the received data
*   unsigned NumBytes - Number of bytes received
*   U32 Flags - Flags indicating frame status
*   void *pUserDataContext - User context pointer (not used)
*
* Return Value:
*   None
*
********************************************************************************/
static void usb_cb_on_data (
        USBH_VIDEO_DEVICE_HANDLE hDevice,
        USBH_VIDEO_STREAM_HANDLE hStream,
        USBH_STATUS Status,
        const U8    *pData,
        unsigned    NumBytes,
        U32         Flags,
        void        *pUserDataContext )
{
    (void) pData;
    (void) hDevice;
    (void) pUserDataContext;

    static size_t   _frame_bytes = INITIAL_VAL;
    static uint8_t  _throw_away_frame = INITIAL_VAL;
    static uint8_t  _current_Buffer = INITIAL_VAL;
    static uint16_t _frame_count = INITIAL_VAL;
    USBH_STATUS Status1;
    I8 IsStreamStopped;
    BaseType_t result;

    /* Check if USB is ready to transmit */
    if ( Status == USBH_STATUS_SUCCESS )
    {
            stream_err_cnt = 0;

        bool    endOfFrame = (Flags & USBH_UVC_END_OF_FRAME) == USBH_UVC_END_OF_FRAME;


        if( !point3mp_camera_enabled )
        {
            if(!logitech_camera_enabled)
                _throw_away_frame |= (_frame_count % FRAMES_TO_SKIP);
            else
                _throw_away_frame |= (_frame_count % FRAMES_TO_SKIP_LOGITECH);
        }

        if ( _frame_bytes + NumBytes > CAMERA_BUFFER_SIZE ) {
            printf( "[USB Camera] Buffer overflow: %u (%6u)\r\n", _current_Buffer, _frame_bytes + NumBytes );
            _throw_away_frame = 1;
            _frame_bytes = FRAME_BYTES_RESET;
        }

        if ( _throw_away_frame ) 
        {
            if ( endOfFrame ) 
            {
                _frame_count++;
                _frame_bytes = FRAME_BYTES_RESET;
                _throw_away_frame = image_buff[_current_Buffer].BufReady;
            }
            USBH_VIDEO_Ack(hStream);
            return;
        }

        /* Copy data into the current buffer. */
      
        memcpy( ((uint8_t *)usb_yuv_frames[_current_Buffer].memory) + _frame_bytes, pData,NumBytes );

        
        _frame_bytes += NumBytes;
        /* Switch buffers when a buffer is full or when we register an end-of-frame. */
        if ( endOfFrame )
        {
            _frame_count++;
            

            if ( _frame_bytes == CAMERA_BUFFER_SIZE )
             { 
                /* processing of the previous frame is DONE, no ready buffer, and wait for a new ready frame */
                lastBuffer = _current_Buffer;
                image_buff[_current_Buffer].BufReady = 1;
                image_buff[_current_Buffer].NumBytes = _frame_bytes;

                __DMB();

                /* Switch to next buffer */
                _current_Buffer = (_current_Buffer + 1) % NUM_IMAGE_BUFFERS;
                /* If there is only ONE buffer, throw away next frames until processing is completed */
                _throw_away_frame = image_buff[_current_Buffer].BufReady;

                if(!logitech_camera_enabled)
                {
                    result = xSemaphoreTake(usb_semaphore, portMAX_DELAY);
                    if (result != pdPASS)
                    {
                        printf("[USB Camera] USB Semaphore take failed\r\n");
                    }
                }
                else
                {
                    xSemaphoreGive(usb_semaphore);
                }
            }
            _frame_bytes = FRAME_BYTES_RESET;
        }

        USBH_VIDEO_Ack(hStream);
        
    } 
    else
    {
        stream_err_cnt++;
        if (stream_err_cnt >= USB_STREAM_ERROR_THRESHOLD)
        {
             Status1 = USBH_STATUS_DEVICE_ERROR; /* Indicate failure */
        }
        else
        {
            if ((Status != USBH_STATUS_DEVICE_REMOVED))
            {
               Status1 = USBH_VIDEO_GetStreamState(hStream, &IsStreamStopped);
               if (Status1 == USBH_STATUS_SUCCESS)
                {
                 if (IsStreamStopped)
                  {
                     Status1 = USBH_VIDEO_RestartStream(hStream);
                   if (Status1== USBH_STATUS_SUCCESS)
                    {
                     USBH_Logf_Application("usb_cb_on_data: Restarted video stream after packet error");
                    }
                    else
                    {
                     USBH_Logf_Application("usb_cb_on_data: USBH_VIDEO_RestartStream failed %s", USBH_GetStatusStr(Status));
                    }
                }
                else
                {
                   USBH_Warnf_Application("usb_cb_on_data: stream_err_cnt %d, but stream is running", stream_err_cnt);

                }
               }
                else
                {
                 USBH_Logf_Application("usb_cb_on_data: USBH_VIDEO_GetStreamState failed %s", USBH_GetStatusStr(Status));
                 }
            }
           }

        /* If the consecutive error count reaches max,
            the device was disconnected,
            or the stream could not be restarted - remove the video device. */

           if ((Status == USBH_STATUS_DEVICE_REMOVED) ||
               (Status1  != USBH_STATUS_SUCCESS))
            {
             USBH_Logf_Application("usb_cb_on_data device removed or max conseq. errors exceeded");

             xQueueSend(device_state_mailbox, (uint8_t*)0xFF, 0);
             stream_err_cnt = 0;
            }

        /* Reset frame state on error but don't change buffer states */
        _frame_bytes = FRAME_BYTES_RESET;
        _throw_away_frame = 0;
        USBH_VIDEO_Ack(hStream);
        return;
               
    }
}
CY_SECTION_ITCM_END

/*******************************************************************************
* Function Name: get_frame_intervals
********************************************************************************
* Description:
*   Retrieves and formats frame interval information for a video frame into a
*   string buffer for logging or display purposes.
*
* Input Arguments:
*   USBH_VIDEO_FRAME_INFO *pFrame - Pointer to the frame information structure
*
* Return Value:
*   None
*
*******************************************************************************/
static void get_frame_intervals ( USBH_VIDEO_FRAME_INFO *pFrame )
{
    char ac[32];
    unsigned i;

    USBH_MEMSET(_ac, 0, sizeof(_ac));
    USBH_MEMSET(ac, 0, sizeof(ac));
    for ( i = 0; i < pFrame->bFrameIntervalType; i++ )
    {
        if ( i == 0 )
        {
            SEGGER_snprintf(ac, sizeof(ac), "%lu", pFrame->u.dwFrameInterval[i]);
        }
        else
        {
            SEGGER_snprintf(ac, sizeof(ac), ", %lu", pFrame->u.dwFrameInterval[i]);
        }
        strncat(_ac, ac, sizeof(_ac) - (strlen(_ac) + 1));
    }
}

/*******************************************************************************
* Function Name: term_type2str
********************************************************************************
* Description:
*   Converts a terminal type code to a human-readable string representation.
*
* Input Arguments:
*   U8 Type - Terminal type code
*
* Return Value:
*   const char* - String representation of the terminal type
*
********************************************************************************/
static const char* term_type2str ( U8 Type )
{
    switch ( Type )
    {
        case USBH_VIDEO_VC_INPUT_TERMINAL :
            return "Input";
        case USBH_VIDEO_VC_OUTPUT_TERMINAL :
            return "Output";
        case USBH_VIDEO_VC_SELECTOR_UNIT :
            return "Selector";
        case USBH_VIDEO_VC_PROCESSING_UNIT :
            return "Processing";
        case USBH_VIDEO_VC_EXTENSION_UNIT :
            return "Extension";
        default :
            return "Unknown";
    }
}


/*******************************************************************************
* Function Name: format_type2str
********************************************************************************
* Description:
*   Converts a video format type code to a human-readable string representation.
*
* Input Arguments:
*   U8 Type - Video format type code
*
* Return Value:
*   const char* - String representation of the video format type
*
********************************************************************************/
static const char* format_type2str ( U8 Type )
{
    switch ( Type )
    {
        case USBH_VIDEO_VS_FORMAT_UNCOMPRESSED :
            return "Uncompressed";
        case USBH_VIDEO_VS_FORMAT_MJPEG :
            return "MJPEG";
        case USBH_VIDEO_VS_FORMAT_FRAME_BASED :
            return "H.264";
        default :
            return "Unknown";
    }
}

/*******************************************************************************
* Function Name: frame_type2str
********************************************************************************
* Description:
*   Converts a video frame type code to a human-readable string representation.
*
* Input Arguments:
*   U8 Type - Video frame type code
*
* Return Value:
*   const char* - String representation of the video frame type
*
*******************************************************************************/
static const char* frame_type2str ( U8 Type )
{
    switch ( Type )
    {
        case USBH_VIDEO_VS_FRAME_UNCOMPRESSED :
            return "Uncompressed";
        case USBH_VIDEO_VS_FRAME_MJPEG :
            return "MJPEG";
        case USBH_VIDEO_VS_FRAME_FRAME_BASED :
            return "H.264";
        default :
            return "Unknown";
    }
}

/*******************************************************************************
* Function Name: print_Input_term_info
********************************************************************************
* Description:
*   Logs information about an input terminal, specifically for camera terminals.
*
* Input Arguments:
*   USBH_VIDEO_TERM_UNIT_INFO *pTermInfo - Pointer to the terminal information structure
*
* Return Value:
*   None
*
********************************************************************************/
static void print_Input_term_info ( USBH_VIDEO_TERM_UNIT_INFO *pTermInfo )
{
    if ( pTermInfo->u.Terminal.TerminalType != USBH_VIDEO_ITT_CAMERA )
    {
        USBH_Logf_Application("  Input terminal type 0x%x is not handled...", pTermInfo->u.Terminal.TerminalType);
    }
    else
    {
        USBH_Logf_Application("  Associated terminal ID %d", pTermInfo->u.Terminal.u.CameraTerm.bAssocTerminal);
        USBH_Logf_Application(
                "  Objective focal length min %d",
                pTermInfo->u.Terminal.u.CameraTerm.wObjectiveFocalLengthMin);
        USBH_Logf_Application(
                "  Objective focal length max %d",
                pTermInfo->u.Terminal.u.CameraTerm.wObjectiveFocalLengthMax);
        USBH_Logf_Application("  Ocular focal length %d", pTermInfo->u.Terminal.u.CameraTerm.wOcularFocalLength);
        USBH_Logf_Application("  Control Size %d", pTermInfo->u.Terminal.u.CameraTerm.bControlSize);
    }
}

/*******************************************************************************
* Function Name: print_output_term_info
********************************************************************************
* Description:
*   Logs information about an output terminal, specifically for streaming terminals.
*
* Input Arguments:
*   USBH_VIDEO_TERM_UNIT_INFO *pTermInfo - Pointer to the terminal information structure
*
* Return Value:
*   None
*
********************************************************************************/
static void print_output_term_info ( USBH_VIDEO_TERM_UNIT_INFO *pTermInfo )
{
    if ( pTermInfo->u.Terminal.TerminalType != USBH_VIDEO_TT_STREAMING )
    {
        USBH_Logf_Application("  Output terminal type 0x%x is not handled...", pTermInfo->u.Terminal.TerminalType);
    }
    else
    {
        USBH_Logf_Application("  Associated terminal ID %d", pTermInfo->u.Terminal.u.OutputTerm.bAssocTerminal);
        USBH_Logf_Application("  Source ID %d", pTermInfo->u.Terminal.u.OutputTerm.bSourceID);
    }
}

/*******************************************************************************
* Function Name: print_selector_unit_info
********************************************************************************
* Description:
*   Logs information about a selector unit, including its input pins.
*
* Input Arguments:
*   USBH_VIDEO_TERM_UNIT_INFO *pTermInfo - Pointer to the terminal information structure
*
* Return Value:
*   None
*
********************************************************************************/
static void print_selector_unit_info ( USBH_VIDEO_TERM_UNIT_INFO *pTermInfo )
{
    unsigned i;

    USBH_Logf_Application("  Unit ID %d", pTermInfo->bTermUnitID);
    USBH_Logf_Application("  Number of pins %d", pTermInfo->u.SelectUnit.bNrInPins);
    for ( i = 0; i < pTermInfo->u.SelectUnit.bNrInPins; i++ )
    {
        USBH_Logf_Application("  Input pin: %d", pTermInfo->u.SelectUnit.baSourceID[i]);
    }
}

/*******************************************************************************
* Function Name: print_processing_unit_info
********************************************************************************
* Description:
*   Logs information about a processing unit, including its source and control details.
*
* Input Arguments:
*   USBH_VIDEO_TERM_UNIT_INFO *pTermInfo - Pointer to the terminal information structure
*
* Return Value:
*   None
*
********************************************************************************/
static void print_processing_unit_info ( USBH_VIDEO_TERM_UNIT_INFO *pTermInfo )
{
    USBH_Logf_Application("  Unit ID %d", pTermInfo->bTermUnitID);
    USBH_Logf_Application("  Source ID %d", pTermInfo->u.ProcessUnit.bSourceID);
    USBH_Logf_Application("  Max multiplier %d", pTermInfo->u.ProcessUnit.wMaxMultiplier);
    USBH_Logf_Application("  Control size %d", pTermInfo->u.ProcessUnit.bControlSize);
}

/*******************************************************************************
* Function Name: print_extension_unit_info
*******************************************************************************
* Description:
*   Logs information about an extension unit, including its GUID, pins, and controls.
*
* Input Arguments:
*   USBH_VIDEO_TERM_UNIT_INFO *pTermInfo - Pointer to the terminal information structure
*
* Return Value:
*   None
*
*******************************************************************************/
static void print_extension_unit_info ( USBH_VIDEO_TERM_UNIT_INFO *pTermInfo )
{
    USBH_Logf_Application("  Unit ID %d", pTermInfo->bTermUnitID);
    USBH_Logf_Application("  Unit GUID 0x%x...", pTermInfo->u.ExtUnit.guidExtensionCode[0]);
    USBH_Logf_Application("  Number of pins %d", pTermInfo->u.ExtUnit.bNrInPins);
    USBH_Logf_Application("  Controls size %d", pTermInfo->u.ExtUnit.bControlSize);
}

/*******************************************************************************
* Function Name: print_term_unit_info
********************************************************************************
* Description:
*   Logs information about a terminal or unit based on its type, delegating to
*   specific print functions for different unit types. Adds a small delay to
*   prevent flooding terminal output.
*
* Input Arguments:
*   USBH_VIDEO_TERM_UNIT_INFO *pTermInfo - Pointer to the terminal or unit information structure
*
* Return Value:
*   None
*
********************************************************************************/
static void print_term_unit_info ( USBH_VIDEO_TERM_UNIT_INFO *pTermInfo )
{
    USBH_Logf_Application("  Terminal/Unit ID %d type %s", pTermInfo->bTermUnitID, term_type2str(pTermInfo->Type));
    switch ( pTermInfo->Type )
    {
        case USBH_VIDEO_VC_INPUT_TERMINAL :
            print_Input_term_info(pTermInfo);
            break;
        case USBH_VIDEO_VC_OUTPUT_TERMINAL :
            print_output_term_info(pTermInfo);
            break;
        case USBH_VIDEO_VC_SELECTOR_UNIT :
            print_selector_unit_info(pTermInfo);
            break;
        case USBH_VIDEO_VC_PROCESSING_UNIT :
            print_processing_unit_info(pTermInfo);
            break;
        case USBH_VIDEO_VC_EXTENSION_UNIT :
            print_extension_unit_info(pTermInfo);
            break;
        default :
            USBH_Logf_Application("  Type %d is not handled...", pTermInfo->Type);
            break;
    }
    USBH_Logf_Application("---------------");
    vTaskDelay(pdMS_TO_TICKS(TERMINAL_ENUM_DELAY_MS)); /* Small delay so that we do not flood terminal output */
}

/*******************************************************************************
* Function Name: On_dev_ready
********************************************************************************
* Description:
*   Handles the initialization and configuration of a USB video device when it is
*   ready. Enumerates terminals, formats, and frames, configures the video stream,
*   and manages frame reception until the device is disconnected or an error occurs.
*
* Input Arguments:
*   U8 DevIndex - Index of the video device
*
* Return Value:
*   None
*
********************************************************************************/
static void On_dev_ready ( U8 DevIndex )
{
    USBH_VIDEO_INPUT_HEADER_INFO InputHeaderInfo;
    USBH_VIDEO_DEVICE_HANDLE hDevice;
    USBH_VIDEO_INTERFACE_INFO IfaceInfo;
    USBH_VIDEO_TERM_UNIT_INFO TermInfo;
    USBH_VIDEO_STREAM_CONFIG StreamInfo;
    USBH_VIDEO_COLOR_INFO ColorInfo;
    USBH_VIDEO_STREAM_HANDLE Stream;
    USBH_VIDEO_FORMAT_INFO Format;
    USBH_VIDEO_FRAME_INFO Frame;
    USBH_STATUS Status;
    unsigned RequestedFrameIntervalIdx;
    unsigned RequestedFormatIdx;
    unsigned RequestedFrameIdx;
    unsigned NumFrameDescriptors;
    unsigned Found;
    unsigned i;
    unsigned j;
    unsigned k;
    int r;
    U8 MBEvent;
    U32 FrameIntervalfrmvidpid;

    /* Open the device, the device index is retrieved from the notification callback */
    RequestedFormatIdx = 0;
    RequestedFrameIdx = 0;
    RequestedFrameIntervalIdx = 0;
    Found = 0;

    memset( &hDevice, 0, sizeof(hDevice) );
    memset( &Stream, 0, sizeof(Stream) );
    /* Add timeout tracking for device initialization */
    float timeout_start_time = ifx_time_get_ms_f();
    float timeout_ms = DEVICE_INIT_TIMEOUT_MS; /* 5 seconds timeout */

    Status = USBH_VIDEO_Open(DevIndex, &hDevice);
    if ( Status != USBH_STATUS_SUCCESS )
    {
        USBH_Logf_Application("USBH_VIDEO_Open returned with error: %s", USBH_GetStatusStr(Status));
        return;
    }

    Status = USBH_VIDEO_GetInterfaceInfo(hDevice, &IfaceInfo);
    if ( Status != USBH_STATUS_SUCCESS )
    {
        USBH_Logf_Application("USBH_VIDEO_GetInterfaceInfo returned with error: %s", USBH_GetStatusStr(Status));
        return;
    }
    USBH_Logf_Application("====================================================================");
    USBH_Logf_Application("Vendor  Id = 0x%0.4X", IfaceInfo.VendorId);
    USBH_Logf_Application("Product Id = 0x%0.4X", IfaceInfo.ProductId);

    if((LOGI_TECH_C920_VID == IfaceInfo.VendorId) && (LOGI_TECH_C920_PID == IfaceInfo.ProductId))
    {
        camera_not_supported = 0;
        FrameIntervalfrmvidpid = FRAME_INTERVAL_1;
        logitech_camera_enabled = 1;
        point3mp_camera_enabled = 0;
        twomp_camera_enabled = 0;
    }
    else if((HBV_CAM_0P3_VID == IfaceInfo.VendorId) && (HBV_CAM_0P3_PID == IfaceInfo.ProductId))
    {
        camera_not_supported = 0;
        FrameIntervalfrmvidpid = FRAME_INTERVAL_2;
        logitech_camera_enabled = 0;
        point3mp_camera_enabled = 1;
        twomp_camera_enabled = 0;
    }
    else if((HBV_CAM_2P0_VID == IfaceInfo.VendorId) && (HBV_CAM_2P0_PID == IfaceInfo.ProductId))
    {
        camera_not_supported = 0;
        FrameIntervalfrmvidpid = FRAME_INTERVAL;
        logitech_camera_enabled = 0;
        point3mp_camera_enabled = 0;
        twomp_camera_enabled = 1;
    }
    else
    {
        xSemaphoreGive(model_semaphore);
        camera_not_supported = 1;
        return;
    }

    /* List all terminals/units */
    for ( i = 0; i < IfaceInfo.NumTermUnits; i++ )
    {
        /* Check if we've been waiting too long for initialization */
        if ((ifx_time_get_ms_f() - timeout_start_time) > timeout_ms)
        {
            USBH_Logf_Application("Timeout waiting for device initialization during terminal enumeration. \
                                    Resetting...");
            Found = 0;
            goto cleanup; /* Exit and cleanup */
        }

        Status = USBH_VIDEO_GetTermUnitInfo( hDevice, i, &TermInfo );
        if ( Status == USBH_STATUS_SUCCESS )
        {
            print_term_unit_info(&TermInfo);

        } 
        else 
        {
            USBH_Logf_Application("USBH_VIDEO_GetTermUnitInfo returned with error: %s", USBH_GetStatusStr(Status));
        }
    }


    /* This sets the text information fields and the webcam frame window visible. */

    memset( &IfaceInfo, 0, sizeof(IfaceInfo) );
    if ( Status != USBH_STATUS_SUCCESS && Status != USBH_STATUS_NOT_FOUND )
    {
        USBH_Logf_Application("USBH_VIDEO_GetFirstTermUnitInfo returned with error: %s", USBH_GetStatusStr(Status));
    } 
    else
    {

        /* Configure the VIDEO device */

        Status = USBH_VIDEO_GetInputHeader( hDevice, &InputHeaderInfo );
        if ( Status != USBH_STATUS_SUCCESS )
        {
            USBH_Logf_Application("USBH_VIDEO_GetInputHeader returned with error: %s", USBH_GetStatusStr(Status));
        } 
        else
        {
            USBH_Logf_Application(
                    "Video Interface with index %d (%d formats reported, still capture method %d):",
                    DevIndex,
                    InputHeaderInfo.bNumFormats,
                    InputHeaderInfo.bStillCaptureMethod);

            /* Iterate over all formats */

            for ( i = 0; i < InputHeaderInfo.bNumFormats; i++ )
            {
                /* Check if we've been waiting too long for initialization */
                if ((ifx_time_get_ms_f() - timeout_start_time) > timeout_ms)
                {
                    USBH_Logf_Application("Timeout waiting for device initialization during format discovery. \
                                            Resetting...");
                    Found = 0;
                    break;
                }

                Status = USBH_VIDEO_GetFormatInfo(hDevice, i, &Format);
                if ( Status != USBH_STATUS_SUCCESS )
                {
                    USBH_Logf_Application(
                            "USBH_VIDEO_GetFormatInfo returned with error: %s",
                            USBH_GetStatusStr(Status));
                }
                else
                {
                    switch ( Format.FormatType )
                    {
                        case USBH_VIDEO_VS_FORMAT_UNCOMPRESSED :
                            NumFrameDescriptors = Format.u.UncompressedFormat.bNumFrameDescriptors;
                            break;
                        case USBH_VIDEO_VS_FORMAT_MJPEG :
                            NumFrameDescriptors = Format.u.MJPEG_Format.bNumFrameDescriptors;
                            break;
                        case USBH_VIDEO_VS_FORMAT_FRAME_BASED :
                            NumFrameDescriptors = Format.u.H264_Format.bNumFrameDescriptors;
                            break;
                        default :
                            USBH_Logf_Application("  Format type %d is not supported!:", Format.FormatType);
                            Status = USBH_STATUS_ERROR;
                    }

                    /* If we have a valid format type - proceed with parsing */

                    if ( Status == USBH_STATUS_SUCCESS )
                    {
                        USBH_Logf_Application("  Format Index %d, type %s:", i, format_type2str(Format.FormatType));

                        /* Check if the format has a color matching descriptor */

                        Status = USBH_VIDEO_GetColorMatchingInfo(hDevice, i, &ColorInfo);
                        if ( Status != USBH_STATUS_SUCCESS )
                        {
                            USBH_Logf_Application("  No color matching descriptor (%s)", USBH_GetStatusStr(Status));
                        }
                        else
                        {
                            USBH_Logf_Application(
                                    "  Color matching descriptor: bColorPrimaries 0x%x, "
                                    "bTransferCharacteristics 0x%x, "
                                    "bMatrixCoefficients 0x%x",
                                    ColorInfo.bColorPrimaries,
                                    ColorInfo.bTransferCharacteristics,
                                    ColorInfo.bMatrixCoefficients);
                        }

                        /* Iterate over all frame types */

                        for ( j = 0; j < NumFrameDescriptors; j++ )
                        {
                            /* Check timeout during frame discovery */
                            if ((ifx_time_get_ms_f() - timeout_start_time) > timeout_ms)
                            {
                                USBH_Logf_Application("Timeout waiting for device initialization during frame discovery."
                                                        "Resetting...");
                                Found = 0;
                                break; /* Exit frame discovery loop */
                            }

                            Status = USBH_VIDEO_GetFrameInfo(hDevice, i, j, &Frame);
                            if ( Status != USBH_STATUS_SUCCESS )
                            {
                                USBH_Logf_Application(
                                        "USBH_VIDEO_GetFrameInfo returned with error: %s",
                                        USBH_GetStatusStr(Status));
                            }
                            else
                            {
                                if ( Frame.bFrameIntervalType == 0 )
                                {
                                    USBH_Logf_Application(
                                            "    Frame Index %d, type %s, x %d, y %d, %d min frame interval, "
                                            "%d max frame interval, %d interval step",
                                            j,
                                            frame_type2str(Frame.FrameType),
                                            Frame.wWidth,
                                            Frame.wHeight,
                                            Frame.bFrameIntervalType,
                                            Frame.u.dwMinFrameInterval,
                                            Frame.u.dwMaxFrameInterval,
                                            Frame.u.dwFrameIntervalStep);
                                }
                                else
                                {
                                    get_frame_intervals(&Frame);
                                    USBH_Logf_Application(
                                            "    Frame Index %d, type %s, x %d, y %d, intervals (%d): { %s }",
                                            j,
                                            frame_type2str(Frame.FrameType),
                                            Frame.wWidth,
                                            Frame.wHeight,
                                            Frame.bFrameIntervalType,
                                            _ac);

                                    /* Try to find a setting which matches our defines */

                                    if ( Format.FormatType == FORMAT )
                                    {
                                        if ( (Frame.wHeight == CAMERA_HEIGHT) && (Frame.wWidth == CAMERA_WIDTH) )
                                        {
                                            for ( k = 0; k < Frame.bFrameIntervalType; k++ )
                                            {
                                                if ( Frame.u.dwFrameInterval[k] == FrameIntervalfrmvidpid )
                                                {
                                                    if ( Found != 1u ) 
                                                    {
                                                        RequestedFormatIdx = i;
                                                        RequestedFrameIdx = j;
                                                        RequestedFrameIntervalIdx = k + USB_DEFAULT_FRAME_INTERVAL_IDX;
                                                        Found = 1;
                                                        USBH_Logf_Application(
                                                            "--->Found requested setting (%dx%d @ %d FPS): "
                                                            "Format Index %d, Frame Index %d, Frame Interval Index %d",
                                                            CAMERA_WIDTH,
                                                            CAMERA_HEIGHT,
                                                            USB_SECONDS_TO_100NS / Frame.u.dwFrameInterval[k],
                                                            RequestedFormatIdx,
                                                            RequestedFrameIdx,
                                                            RequestedFrameIntervalIdx);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                /* Small delay so that we do not flood terminal output */
                                vTaskDelay(pdMS_TO_TICKS(FRAME_DISCOVERY_DELAY_MS)); 
                            }
                        }
                    }
                }
            }

            /* If we didn't find an exact format match, try to use first available format */
            if (Found != 1u && InputHeaderInfo.bNumFormats > 0)
            {
                USBH_Logf_Application(
                    "WARNING: Couldn't find exact requested format. "
                    "Using first available format instead.");
                /* Fall back to first available format if any exist */
                RequestedFormatIdx = 0;
                RequestedFrameIdx = 0;
                RequestedFrameIntervalIdx = USB_DEFAULT_FRAME_INTERVAL_IDX; /* First interval */
                Found = 1;
            }
        }

        /* Receive frame data */
        memset(&StreamInfo, 0, sizeof(USBH_VIDEO_STREAM_CONFIG));
        StreamInfo.Flags = 0;
        if ( Found == 1u )
        {
            StreamInfo.FormatIdx       = RequestedFormatIdx;
            StreamInfo.FrameIdx        = RequestedFrameIdx;
            StreamInfo.FrameIntervalIdx = RequestedFrameIntervalIdx;
            StreamInfo.pfDataCallback   = usb_cb_on_data;
            lastBuffer = 0;
            last_check_time = ifx_time_get_ms_f();
            xQueueReset(device_state_mailbox);
            Status = USBH_VIDEO_OpenStream( hDevice, &StreamInfo, &Stream );
            if ( Status != USBH_STATUS_SUCCESS )
            {
                USBH_Logf_Application("USBH_VIDEO_OpenStream returned with error: %s", USBH_GetStatusStr(Status));
            }
            else
            {

                /* Wait until the device has been disconnected */

                for ( ;; )
                {
                    /* Check for stalled camera frames */
                    float current_time = ifx_time_get_ms_f();
                    if ((current_time - last_check_time) > 1000)
                    { 
                        /* Check once per second */
                        last_check_time = current_time;

#ifdef ENABLE_USB_WEBCAM_TIMEOUT
                        if (last_frame_time > 0 &&
                            (current_time - last_frame_time) > frame_timeout_ms)
                        {
                            printf("[ERROR] No frames received for %.2f seconds, camera is stalled", 
                                    frame_timeout_ms/1000.0);
                            /* Force camera reset */
                            break;
                        }
#endif /* ENABLE_USB_WEBCAM_TIMEOUT */
                    }
                    /* checking if the queue operation was successful */
                    r = xQueueReceive(device_state_mailbox, &MBEvent, pdMS_TO_TICKS(100));
                    if ( r == pdTRUE )
                    {
                        if (MBEvent == 0xff)
                        {
                            USBH_Logf_Application("USBH_VIDEO_OpenStream breaking - Device was disconnected");
                            break;
                        }
                        else if (MBEvent == 0xfe)
                        {
                            USBH_Logf_Application("USBH_VIDEO_OpenStream breaking - Transfer status error");
                            break;
                        }
                        else
                        {
                            USBH_Logf_Application("USBH_VIDEO_OpenStream received unknown error code: 0x%02x", MBEvent);

                            if(_device_connected)
                                MBEvent = 0xfe;
                            break;
                        }
                    }
                    else
                    {
                }
            }
        }
        }
        else
        {

            /* Display error text until webcam is removed */

            while ( _device_connected )
            {
                USBH_OS_Delay(USB_DEVICE_DISCONNECT_WAIT_MS);
            }
        }
    }

    cleanup:

        /*  Close the device */

        USBH_Logf_Application("Closing video device...");
        USBH_VIDEO_Close(hDevice);
}


/*******************************************************************************
* Function Name: cm55_usb_webcam_task
********************************************************************************
* Description:
*   Main task for handling USB webcam operations. Initializes USB host and video
*   subsystems, creates tasks for USB handling, sets up mailboxes, and processes
*   device readiness events from the video mailbox.
*
* Input Arguments:
*   void *arg - Task argument (not used)
*
* Return Value:
*   None
*
*******************************************************************************/
void cm55_usb_webcam_task( void *arg )
{
    const uint32_t  task_stack_size_bytes = USBH_TASK_STACK_SIZE_BYTES;
    U8              DevIndex;

    USBH_Init();
    xTaskCreate(USBH_Task_Wrapper, "USBH_Task", task_stack_size_bytes / sizeof(StackType_t), NULL, TASK_PRIO_USBH_MAIN, &usbh_main_task_handle);
    xTaskCreate(USBH_Task_Wrapper_new, "USBH_isr", task_stack_size_bytes / sizeof(StackType_t), NULL, TASK_PRIO_USBH_ISR, &usbh_isr_task_handle);

   video_mail_box = xQueueCreate(SEGGER_COUNTOF(avideo_dev_Indexes), sizeof(U8));
    configASSERT(video_mail_box != NULL);
   device_state_mailbox = xQueueCreate(sizeof(ac_mb_event)/sizeof(U8), sizeof(U8));
    configASSERT(device_state_mailbox != NULL);
    USBH_VIDEO_Init();
    USBH_VIDEO_AddNotification( &Hook, cb_Onadd_remove_device, NULL );

    while ( true ) 
    {

        if ( xQueueReceive(video_mail_box, &DevIndex, pdMS_TO_TICKS(QUEUE_GET_TIMEOUT_MS)) == pdTRUE )
        {
            On_dev_ready(DevIndex);
        }
    }
}

/* [] END OF FILE */