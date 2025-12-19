/******************************************************************************
* File Name        : lcd_task.h
*
* Description      : This is the header file of LCD display functions.
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
 * Header Guards
 *******************************************************************************/
#ifndef _LCD_TASK_H_
#define _LCD_TASK_H_

/*******************************************************************************
 * Included Headers
 *******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "semphr.h"
#include "vg_lite.h"
#include "vg_lite_platform.h"
#include "object_detection_lib.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define NUM_IMAGE_BUFFERS                 (2U)

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern vg_lite_buffer_t *renderTarget;
extern vg_lite_buffer_t usb_yuv_frames[];
extern SemaphoreHandle_t model_semaphore;    /* usb_semaphore */ 
extern int8_t bgr888_int8[];
extern prediction_OD_t Prediction;
extern volatile float Inference_time;
extern SemaphoreHandle_t usb_semaphore;
extern uint8_t _device_connected;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
int8_t * draw( void );
void update_bounding_box_data ( vg_lite_buffer_t *renderTarget, prediction_OD_t  *prediction );

void ifx_image_conv_RGB565_to_RGB888_i8( uint8_t *src_bgr565, int width, int height,
                                        int8_t *dst_rgb888_i8, int dst_width, int dst_height );
uint32_t ifx_lcd_get_Display_Width( void );
uint32_t ifx_lcd_get_Display_Height( void );
void ifx_lcd_display_Rect( uint16_t x0, uint16_t y0, uint8_t *image, uint16_t width, uint16_t height );

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _LCD_TASK_H_ */

/* [] END OF FILE */