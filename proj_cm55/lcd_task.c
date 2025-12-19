/*******************************************************************************
* File Name        : lcd_task.c
*
* Description      : This file implements the LCD display modules.
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
#include "retarget_io_init.h"
#if defined(HOME_BUTTON)
#include "home_btn_img.h"
#endif
#include "no_camera_img.h"
#include "camera_not_supported_img.h"
#include "mtb_disp_dsi_waveshare_4p3.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "FreeRTOSConfig.h"
#include "definitions.h"
#include "usb_camera_task.h"
#include "lcd_task.h"
#include "inference_task.h"
#include "mtb_ctp_ft5406.h"
#include "ifx_gui_render.h"
#include "font_16x36.h"
#include "ifx_image_utils.h"
#include "time_utils.h"
#include "object_detection_structs.h"
#include "mtb_ml_gen/model_info.h" /* import CLASS_STRING_LIST and its values */
#include "lcd_graphics.h"

#if defined(MTB_SHARED_MEM)
#include "shared_mem.h"
#endif

/*******************************************************************************
* Macros
*******************************************************************************/
/* Task and interrupt priority macros */
#define I2C_CONTROLLER_IRQ_PRIORITY         (4U)
#define GPU_INT_PRIORITY                    (3U)
#define DC_INT_PRIORITY                     (3U)

/* Display configuration macros */
#define COLOR_DEPTH                         (16U)
#define BITS_PER_PIXEL                      (8U)

/* GPU buffer size macros */
#define DEFAULT_GPU_CMD_BUFFER_SIZE         ((64U) * (1024U))
#define GPU_TESSELLATION_BUFFER_SIZE        ((DISPLAY_H) * 128U)
#define FRAME_BUFFER_SIZE                   ((DISPLAY_W) * (DISPLAY_H) * ((COLOR_DEPTH) / (BITS_PER_PIXEL)))
#define VGLITE_HEAP_SIZE                    (((FRAME_BUFFER_SIZE) * (3U)) + \
                                             (((DEFAULT_GPU_CMD_BUFFER_SIZE) + (GPU_TESSELLATION_BUFFER_SIZE)) * \
                                                (NUM_IMAGE_BUFFERS)) + \
                                             ((CAMERA_BUFFER_SIZE) * (NUM_IMAGE_BUFFERS + 1U)))

/* GPU memory configuration macros */
#define GPU_MEM_BASE                        (0x0U)
#define TICK_VAL                            (1U)

/* Color definition macros */
#define BLACK_COLOR                         (0x00000000U)
#define RESET_VAL                           (0U)
#define INITIAL_VAL                         (0)

/* Box drawing macros */
#define OUTER_BOX_X_POS                     (0U)
#define OUTER_BOX_Y_POS                     (0U)
#define OUTER_BOX_HEIGHT                    (512U)
#define OUTER_BOX_WIDTH                     (512U)
#define BOX_X_POS                           (0U)
#define BOX_Y_POS                           (0U)
#define BOX_HEIGHT                          (100U)
#define BOX_WIDTH                           (100U)

/* Touch configuration macros */
#define TOUCH_TIMER_PERIOD_MS               (200U)
#define TOUCH_DEBOUNCE_DELAY_MS             (2U)

/* Image positioning macros */
#define NO_CAMERA_IMG_X_POS                 ((MTB_DISP_WAVESHARE_4P3_HOR_RES / 2U) - ((NO_CAMERA_IMG_WIDTH / 2U) + 10U))
#define NO_CAMERA_IMG_Y_POS                 ((MTB_DISP_WAVESHARE_4P3_VER_RES / 2U) - (NO_CAMERA_IMG_HEIGHT / 2U))
#define CAMERA_NOT_SUPPORTED_IMG_X_POS      ((MTB_DISP_WAVESHARE_4P3_HOR_RES / 2U) - ((CAMERA_NOT_SUPPORTED_IMG_WIDTH / \
                                                                                        2U) + 10U))
#define CAMERA_NOT_SUPPORTED_IMG_Y_POS      ((MTB_DISP_WAVESHARE_4P3_VER_RES / 2U) - (CAMERA_NOT_SUPPORTED_IMG_HEIGHT / \
                                                                                        2U))

/* Display buffer configuration macros */
#define DISPLAY_BUFFER_COUNT                (3U)
#define DISPLAY_BUFFER_INDEX_0              (0U)

/* Timing and delay macros */
#define USB_ENUMERATION_DELAY_MS            (1500U)
#define FRAME_SWITCH_DELAY_MS               (1U)

/* Path data indices for rectangle drawing */
#define PATHDATA_TOP_LEFT_X                 (1U)
#define PATHDATA_TOP_LEFT_Y                 (2U)
#define PATHDATA_TOP_RIGHT_X                (4U)
#define PATHDATA_TOP_RIGHT_Y                (5U)
#define PATHDATA_BOTTOM_RIGHT_X             (7U)
#define PATHDATA_BOTTOM_RIGHT_Y             (8U)
#define PATHDATA_BOTTOM_LEFT_X              (10U)
#define PATHDATA_BOTTOM_LEFT_Y              (11U)
#define PATHDATA_CLOSE_TOP_LEFT_X           (13U)
#define PATHDATA_CLOSE_TOP_LEFT_Y           (14U)

/* Color indices for bounding boxes */
#define COLOR_GREEN_INDEX                   (0U)
#define COLOR_BLACK_INDEX                   (1U)
#define COLOR_RED_INDEX                     (2U)
#define COLOR_BLUE_INDEX                    (3U)

/* Number of supported classes (for color cycling) */
#define OBJECT_DETECTION_CLASS_COUNT        (7u)

/* Color values for bounding boxes */
#define COLOR_RED_R                         (227U)
#define COLOR_RED_G                         (66U)
#define COLOR_RED_B                         (52U)
#define COLOR_BLUE_R                        (8U)
#define COLOR_BLUE_G                        (24U)
#define COLOR_BLUE_B                        (168U)
#define COLOR_GREEN_R                       (0U)
#define COLOR_GREEN_G                       (255U)
#define COLOR_GREEN_B                       (0U)
#define COLOR_BLACK_R                       (0U)
#define COLOR_BLACK_G                       (0U)
#define COLOR_BLACK_B                       (0U)

/* GUI text offsets */
#define GUI_TEXT_X_OFFSET                   (8U)
#define GUI_TEXT_Y_OFFSET                   (36U)

/* Performance metrics display position */
#define PERF_METRICS_X_POS                  (10U)
#define PERF_METRICS_Y_POS                  (440U)

/* Buffer loop iteration limits */
#define BUFFER_RESET_LOOP_LIMIT             (NUM_IMAGE_BUFFERS - 1U)
#define NO_CAMERA_DISPLAY_LOOP_LIMIT        (2U)
#define CAMERA_NOT_SUPPORTED_LOOP_LIMIT     (2U)

/* Bytes per pixel for mirroring */
#define BYTES_PER_PIXEL                     (2U)

/* UI formatting */
#define CONFIDENCE_PERCENT_MULT             (100.0f)

#define VALUE_ONE                           (1U)
#define VALUE_TWO                           (2U)
#define VALUE_THREE                         (3U)

#define COLOR_SHIFT_RED                     (16u)
#define COLOR_SHIFT_GREEN                   (8u)

#define OOB_SHARED_DATA_RESET_TO_HOME       (2u)      
#define OOB_SHARED_DATA_BOOT_ADDR_NONE      (0xFFFFFFFFu)
#define TOUCH_DISPLAY_EDGE_OFFSET           (32u)        
#define TOUCH_DISPLAY_MAX_INDEX             (1u)

#define HOME_BUTTON_TOUCH_TOLERANCE         (10u)

#define DIVIDE_HALF                         (0.5f)

#define HOME_BUTTON_MARGIN_RIGHT_PX         (40u)     
#define HOME_BUTTON_MARGIN_TOP_PX           (10u)     
#define HOME_BUTTON_X_OFFSET_FOR_ICON       (100u)    

/* Display resolution (for calculations) */
#define DISPLAY_WIDTH_PX                    (832u)
#define COLOR_COUNT                         (4)

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* USB race condition recovery mode watchdog variables */
static float last_successful_frame_time = 0;
static int recovery_attempts = 0;
static GFXSS_Type *base = (GFXSS_Type*) GFXSS;

#if defined(HOME_BUTTON)
static uint16_t home_btn_x_pos;
static uint16_t home_btn_y_pos;
#endif

static vg_lite_matrix_t matrix;
static volatile bool s_lvgl_initialized = false;
static int display_offset_x = 0, display_offset_y = 0;

static int16_t pathData[] = {
    2, BOX_WIDTH, BOX_HEIGHT,        /* Move to (minX, minY) */
    4, OUTER_BOX_WIDTH, BOX_HEIGHT,  /* Line from (minX, minY) to (maxX, minY) */
    4, OUTER_BOX_WIDTH, OUTER_BOX_HEIGHT, /* Line from (maxX, minY) to (maxX, maxY) */
    4, BOX_WIDTH, OUTER_BOX_HEIGHT,  /* Line from (maxX, maxY) to (minX, maxY) */
    4, BOX_WIDTH, BOX_HEIGHT,        /* Line from (minX, maxY) to (minX, minY) */
    0,
};

static vg_lite_path_t path = {
    {OUTER_BOX_X_POS, OUTER_BOX_Y_POS, /* left,top */
    CAMERA_WIDTH, CAMERA_HEIGHT},      /* right,bottom */
    VG_LITE_HIGH,                      /* quality */
    VG_LITE_S16,                       /* format */
    {0},                               /* uploaded */
    sizeof(pathData),                  /* path length */
    pathData,                          /* path data */
    1                                  /* path changed */
};

/* 4 colors: green, black, red, blue */
static uint8_t color_r[4] = {COLOR_GREEN_R, COLOR_BLACK_R, COLOR_RED_R, COLOR_BLUE_R};
static uint8_t color_g[4] = {COLOR_GREEN_G, COLOR_BLACK_G, COLOR_RED_G, COLOR_BLUE_G};
static uint8_t color_b[4] = {COLOR_GREEN_B, COLOR_BLACK_B, COLOR_RED_B, COLOR_BLUE_B};

/* scale factor from the camera image to the display */
float scale_Cam2Disp;

CY_SECTION(".cy_socmem_data")
__attribute__((
    aligned(64))) int8_t bgr888_int8[(IMAGE_HEIGHT) * (IMAGE_WIDTH) * 3] = {0}; /* BGR888 integer buffer */

/* Contiguous memory for VGLite heap */
CY_SECTION(".cy_xip")
__attribute((used)) uint8_t contiguous_mem[VGLITE_HEAP_SIZE];

volatile void *vglite_heap_base = &contiguous_mem;   /* VGLite heap base address */
volatile bool fb_pending = false;                   /* Framebuffer pending flag */
volatile bool button_debouncing = false;
volatile uint32_t button_debounce_timestamp = 0; 
volatile float time_start1;


/*******************************************************************************
 * Global Variables - Shared memory variable
 *******************************************************************************/
#if defined(MTB_SHARED_MEM)
oob_shared_data_t oob_shared_data_ns;
#endif

cy_stc_gfx_context_t gfx_context;

vg_lite_buffer_t *renderTarget;
vg_lite_buffer_t usb_yuv_frames[NUM_IMAGE_BUFFERS];
vg_lite_buffer_t bgr565;

/* double display frame buffers */
vg_lite_buffer_t display_buffer[DISPLAY_BUFFER_COUNT];

/*******************************************************************************
 * Global Variables - I2C Controller Configuration
 *******************************************************************************/
cy_stc_scb_i2c_context_t i2c_controller_context;     /* I2C controller context */
mtb_ctp_ft5406_config_t ft5406_config =              /* Touch controller configuration */
{
    .i2c_base = CYBSP_I2C_CONTROLLER_HW,             /* I2C base hardware */
    .i2c_context =&i2c_controller_context            /* Pointer to I2C context */
};

/*******************************************************************************
 * Global Variables - Interrupt Configurations
 *******************************************************************************/
cy_stc_sysint_t dc_irq_cfg =                                   /* DC Interrupt Configuration */
{
    .intrSrc      = gfxss_interrupt_dc_IRQn,                   /* DC interrupt source */
    .intrPriority = DC_INT_PRIORITY                            /* DC interrupt priority */
};

cy_stc_sysint_t gpu_irq_cfg =                                  /* GPU Interrupt Configuration */
{
    .intrSrc      = gfxss_interrupt_gpu_IRQn,                  /* GPU interrupt source */
    .intrPriority = GPU_INT_PRIORITY                           /* GPU interrupt priority */
};

cy_stc_sysint_t i2c_controller_irq_cfg =                       /* I2C Controller Interrupt Configuration */
{
    .intrSrc      = CYBSP_I2C_CONTROLLER_IRQ,                  /* I2C controller interrupt source */
    .intrPriority = I2C_CONTROLLER_IRQ_PRIORITY,                /* I2C controller interrupt priority */
};

/*******************************************************************************
* Functions
*******************************************************************************/

CY_SECTION_ITCM_BEGIN
/*******************************************************************************
* Function Name: mirrorImage
********************************************************************************
* Description: Mirrors an image horizontally by swapping pixels from left to right
*              in the provided buffer. The function assumes a fixed bytes-per-pixel
*              value of 2 (e.g., for RGB565 format) and operates on a buffer with
*              dimensions defined by CAMERA_WIDTH and CAMERA_HEIGHT.
* Parameters:
*   - buffer: Pointer to the vg_lite_buffer_t structure containing the image data
*
* Return:
*   None
********************************************************************************/
void mirrorImage(vg_lite_buffer_t *buffer)
{
    uint8_t temp[BYTES_PER_PIXEL];
    uint8_t *start, *end;
    int m, n;

    for (m = INITIAL_VAL; m < CAMERA_HEIGHT; m++) 
    {
        start = buffer->memory + m * (CAMERA_WIDTH * BYTES_PER_PIXEL);
        end = start + (CAMERA_WIDTH - 1) * BYTES_PER_PIXEL;

        for (n = INITIAL_VAL; n < CAMERA_WIDTH / 2; n++) 
        {
            for (int i = INITIAL_VAL; i < BYTES_PER_PIXEL; i++) 
            {
                temp[i] = start[i];
            }

            for (int i = INITIAL_VAL; i < BYTES_PER_PIXEL; i++) 
            {
                start[i] = end[i];
            }

            for (int i = INITIAL_VAL; i < BYTES_PER_PIXEL; i++) 
            {
                end[i] = temp[i];
            }

            start += BYTES_PER_PIXEL;
            end -= BYTES_PER_PIXEL;
        }
    }
}
CY_SECTION_ITCM_END

/*******************************************************************************
* Function Name: cleanup
********************************************************************************
* Description: Deallocates resources and frees memory used by the VGLite
*              graphics library. This function should be called to ensure proper
*              cleanup of VGLite resources when they are no longer needed.
* Parameters:
*   None
*
* Return:
*   None
********************************************************************************/
static void cleanup(void) 
{
    /* Deallocate all the resource and free up all the memory */
    vg_lite_close();
}

CY_SECTION_ITCM_BEGIN
/*******************************************************************************
 * Function Name: draw
 *******************************************************************************
 * Summary:
 *  Captures a frame from the USB camera, converts YUYV422 to BGR565, optionally
 *  mirrors it, scales to display resolution (800x600), and converts to BGR888
 *  (int8_t, offset -128) for ML inference. Measures timing for each stage.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  int8_t* : Pointer to the processed BGR888 (int8_t) image buffer (256x240)
 *******************************************************************************/
int8_t *draw(void) 
{
    vg_lite_error_t error;
    volatile uint32_t time_draw_start = ifx_time_get_ms_f();
    /* find a ready buffer */
    uint32_t    workBuffer = lastBuffer;

    while ( image_buff[workBuffer].BufReady == 0 ) 
    {
        for ( int32_t ii = 0; ii < NUM_IMAGE_BUFFERS; ii++ )
            if ( image_buff[(workBuffer + ii) % NUM_IMAGE_BUFFERS].BufReady == 1 ) 
            {
                workBuffer = (workBuffer + ii) % NUM_IMAGE_BUFFERS;
                break;
            }
        vTaskDelay(pdMS_TO_TICKS(FRAME_SWITCH_DELAY_MS));
    }


    /* reset all other buffers to available for input from camera */
    for (int32_t ii = 1; ii <= BUFFER_RESET_LOOP_LIMIT; ii++)
        image_buff[(workBuffer + ii) % NUM_IMAGE_BUFFERS].BufReady = RESET_VAL;

    /* convert 320x240 YUYV 422 image into 320*240 BGR565 (scale:1) */
    volatile uint32_t    time_draw_1 = ifx_time_get_ms_f();
    error = vg_lite_blit( &bgr565, &usb_yuv_frames[workBuffer],
                            NULL,                       /* identity matrix */
                            VG_LITE_BLEND_NONE,
                            0,
                            VG_LITE_FILTER_POINT );
    if ( error ) 
    {
        printf("\r\nvg_lite_blit() (320x240 YUYV 422 ==> 320*240 BGR565) "
               "returned error %d\r\n",
               error);
        cleanup();
        CY_ASSERT(0);
    }

    vg_lite_finish();

    if (!point3mp_camera_enabled)
    {
        mirrorImage(&bgr565);
    }
    /* convert 320x240 BGR565 image into 800x600 BGR565 (scale:2.5) */
    volatile uint32_t time_draw_3 = ifx_time_get_ms_f();
    error = vg_lite_blit(renderTarget, &bgr565,
                         &matrix,
                         VG_LITE_BLEND_NONE,
                         0, VG_LITE_FILTER_POINT);
    if (error) 
    {
        printf("\r\nvg_lite_blit() (320x240 BGR565 ==> 800x600 display BGR565) "
               "returned error %d\r\n",
               error);
        cleanup();
        CY_ASSERT(0);
    }
    vg_lite_finish();

    /* Clear USB buffer */
    image_buff[workBuffer].NumBytes = RESET_VAL;
    image_buff[workBuffer].BufReady = RESET_VAL;

    /* Convert 320x240 BGR565 image into 256x240 BGR888 - 128 */
    volatile uint32_t time_draw_5 = ifx_time_get_ms_f();
    ifx_image_conv_RGB565_to_RGB888_i8(bgr565.memory, CAMERA_WIDTH,
                                       CAMERA_HEIGHT, bgr888_int8, IMAGE_WIDTH,
                                       IMAGE_HEIGHT);

    volatile uint32_t time_draw_end = ifx_time_get_ms_f();
    /* performance measures: time */
    extern float Prep_Wait_Buf, Prep_YUV422_to_bgr565, Prep_bgr565_to_Disp,
        Prep_RGB565_to_RGB888;
    Prep_Wait_Buf = time_draw_1 - time_draw_start;
    Prep_YUV422_to_bgr565 = time_draw_3 - time_draw_1;
    Prep_bgr565_to_Disp = time_draw_5 - time_draw_3;
    Prep_RGB565_to_RGB888 = time_draw_end - time_draw_5;

    return bgr888_int8;
}
CY_SECTION_ITCM_END

/*******************************************************************************
* Function Name: dc_irq_handler
********************************************************************************
* Summary: This is the display controller I2C interrupt handler.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
static void dc_irq_handler(void)
{
    fb_pending = false;
    Cy_GFXSS_Clear_DC_Interrupt(base, &gfx_context);
}

/*******************************************************************************
* Function Name: dc_gpu_irq_handlerirq_handler
********************************************************************************
* Summary: This is the GPU interrupt handler.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
static void gpu_irq_handler(void)
{
    Cy_GFXSS_Clear_GPU_Interrupt(base, &gfx_context);
    vg_lite_IRQHandler();
}

/*******************************************************************************
* Function Name: draw_rectangle
*******************************************************************************
*
* Summary:
*  Draws a rectangle on the render target using the VGLite graphics library.
*
* Parameters:
*  color  - The color of the rectangle in 32-bit format.
*  min_x  - The x-coordinate of the top-left corner of the rectangle.
*  min_y  - The y-coordinate of the top-left corner of the rectangle.
*  max_x  - The x-coordinate of the bottom-right corner of the rectangle.
*  max_y  - The y-coordinate of the bottom-right corner of the rectangle.
*
* Return:
*  None
*
*
*******************************************************************************/
void draw_rectangle(uint32_t color, int16_t min_x, int16_t min_y, int16_t max_x,
                    int16_t max_y)
{
    pathData[PATHDATA_TOP_LEFT_X] = min_x;
    pathData[PATHDATA_TOP_LEFT_Y] = min_y; /* top-left */
    pathData[PATHDATA_TOP_RIGHT_X] = max_x;
    pathData[PATHDATA_TOP_RIGHT_Y] = min_y; /* top-right */
    pathData[PATHDATA_BOTTOM_RIGHT_X] = max_x;
    pathData[PATHDATA_BOTTOM_RIGHT_Y] = max_y; /* bottom-right */
    pathData[PATHDATA_BOTTOM_LEFT_X] = min_x;
    pathData[PATHDATA_BOTTOM_LEFT_Y] = max_y; /* bottom-left */
    pathData[PATHDATA_CLOSE_TOP_LEFT_X] = min_x;
    pathData[PATHDATA_CLOSE_TOP_LEFT_Y] = min_y; /* top-left */

    vg_lite_draw(renderTarget, &path, VG_LITE_FILL_EVEN_ODD, &matrix,
                 VG_LITE_BLEND_SRC_OVER, color);
}

/*******************************************************************************
* Function Name: update_bounding_box_data
********************************************************************************
* Summary:
* Updates and draws bounding boxes on the render target based on object 
* detection predictions, including class labels and confidence scores. Sets 
* appropriate colors for different class IDs.
*
* Parameters:
*  vg_lite_buffer_t *renderTarget: Pointer to the render target buffer
*  prediction_OD_t *prediction: Pointer to the object detection prediction data
*
* Return:
*  void
*
*******************************************************************************/
void update_bounding_box_data( vg_lite_buffer_t *renderTarget, prediction_OD_t  *prediction )
{

    for ( int32_t i = INITIAL_VAL; i < prediction->count; i++ )
    {
        int32_t jj = i << VALUE_TWO;
        int32_t id = prediction->class_id[i];
        int32_t cid = (id >= INITIAL_VAL) ? (id % COLOR_COUNT) : COLOR_GREEN_INDEX;

        uint32_t xmin = (uint32_t)( prediction->bbox_int16[jj]     * scale_Cam2Disp ) + display_offset_x;
        uint32_t ymin = (uint32_t)( prediction->bbox_int16[jj + VALUE_ONE] * scale_Cam2Disp ) + display_offset_y;
        uint32_t xmax = (uint32_t)( prediction->bbox_int16[jj + VALUE_TWO] * scale_Cam2Disp ) + display_offset_x;
        uint32_t ymax = (uint32_t)( prediction->bbox_int16[jj + VALUE_THREE] * scale_Cam2Disp ) + display_offset_y;

        if (COLOR_BLUE_INDEX == cid)
        {
            ifx_lcd_set_FGcolor(COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B);
        }
        else if (COLOR_BLACK_INDEX == cid)
        {
            ifx_lcd_set_FGcolor(COLOR_BLACK_R, COLOR_BLACK_G, COLOR_BLACK_B);
        }
        else if (COLOR_RED_INDEX == cid)
        {
            ifx_lcd_set_FGcolor(COLOR_RED_R, COLOR_RED_G, COLOR_RED_B);
        }

        ifx_lcd_draw_Rect(xmin, ymin, xmax, ymax);

        if (id >= INITIAL_VAL)
        {
            ifx_set_bg_color((color_r[cid] << COLOR_SHIFT_RED) | (color_g[cid] << COLOR_SHIFT_GREEN) | color_b[cid]);
            ifx_print_to_buffer(xmin + GUI_TEXT_X_OFFSET, ymin - GUI_TEXT_Y_OFFSET, "%s, %.2f", CLASS_STRING_LIST[id], 
                                prediction->conf[i] * CONFIDENCE_PERCENT_MULT);
        }
        ifx_draw_buffer(renderTarget->memory);
    }
}

/*******************************************************************************
* Function Name: update_model_inference_time
********************************************************************************
* Summary:
* Draws performance metrics (model inference time) on the render target at a 
* fixed position using a specific background color.
*
* Parameters:
*  vg_lite_buffer_t *renderTarget: Pointer to the render target buffer
*  float num: Model inference time in milliseconds
*
* Return:
*  void
*
*******************************************************************************/
void update_model_inference_time( vg_lite_buffer_t *renderTarget, float num  )
{
    ifx_set_bg_color((color_r[COLOR_BLUE_INDEX] << COLOR_SHIFT_RED) | (color_g[COLOR_BLUE_INDEX] << COLOR_SHIFT_GREEN) | color_b[COLOR_BLUE_INDEX]);
    ifx_print_to_buffer(PERF_METRICS_X_POS, PERF_METRICS_Y_POS, "%s %.1f %s", "Model", num, "ms");
    ifx_draw_buffer(renderTarget->memory);
}

/*******************************************************************************
* Function Name: vg_lite_error_exit
*******************************************************************************
*
* Summary:
*  Handles VGLite error conditions by printing an error message with the status
*  code, calling the cleanup function to deallocate resources, and triggering an
*  assertion to halt execution.
*
* Parameters:
*  msg           - Pointer to a character string describing the error context.
*  vglite_status - VGLite error code indicating the specific error condition.
*
* Return:
*  None
*
*******************************************************************************/
void vg_lite_error_exit(char *msg, vg_lite_error_t vglite_status)
{
    printf("%s %d\r\n", msg, vglite_status);
    cleanup();
    CY_ASSERT(RESET_VAL);
}

#if defined(HOME_BUTTON) || defined(TENXER_UART)
/*******************************************************************************
 * Function Name: mcu_soft_reset
 *******************************************************************************
 * Summary:
 *  Initiates a soft reset of the MCU. Prints reset info, updates shared memory
 *  (if enabled) to signal boot mode, clears the display, closes VG-Lite, and
 *  triggers NVIC system reset.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None (does not return — system resets)
 *******************************************************************************/
void mcu_soft_reset(void)
{
    printf("[INFO] Going to home screen, triggering reset...\r\n");
#if defined(MTB_SHARED_MEM)
    oob_shared_data_ns.is_valid = OOB_SHARED_DATA_RESET_TO_HOME;
    oob_shared_data_ns.app_boot_address = OOB_SHARED_DATA_BOOT_ADDR_NONE;
    shared_mem_write(&oob_shared_data_ns);
#endif
    vg_lite_clear(renderTarget, NULL, BLACK_COLOR);
    vg_lite_close();
    __NVIC_SystemReset();


}
#endif

/*******************************************************************************
 * Function Name: disp_i2c_controller_interrupt
 ********************************************************************************
 * Summary:
 *  I2C controller ISR which invokes Cy_SCB_I2C_Interrupt to perform I2C
 *transfer as controller.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void i2c_controller_interrupt(void)
{ 
    Cy_SCB_I2C_Interrupt(CYBSP_I2C_CONTROLLER_HW, &i2c_controller_context);
}
#if defined(HOME_BUTTON)
/*******************************************************************************
* Function Name: touch_read_timer_cb
*******************************************************************************
*
* Summary:
*  Timer callback function for reading touch events from the touch controller.
*  Processes single-touch events, applies debouncing, and checks if the touch
*  coordinates fall within the specified area. If a valid touch is detected
*  action is taken care
*
* Parameters:
*  xTimer - Handle to the FreeRTOS timer that triggered the callback.
*
* Return:
*  None
*
*******************************************************************************/
static void touch_read_timer_cb(TimerHandle_t xTimer)
{
    cy_en_scb_i2c_status_t i2c_status = CY_SCB_I2C_SUCCESS;
    static int touch_x = INITIAL_VAL;
    static int touch_y = INITIAL_VAL;
    volatile int x_val = INITIAL_VAL;
    volatile int y_val = INITIAL_VAL;
    mtb_ctp_touch_event_t touch_event = MTB_CTP_TOUCH_UP;

    i2c_status = mtb_ctp_ft5406_get_single_touch(&touch_event, &touch_x, &touch_y);

    if ((CY_SCB_I2C_SUCCESS == i2c_status) &&
       ((MTB_CTP_TOUCH_DOWN == touch_event) || 
       (MTB_CTP_TOUCH_CONTACT == touch_event)))
    {
        if (!button_debouncing)
        {
            /* Set the debouncing flag */
            button_debouncing = true;

            /* Record the current timestamp */
            button_debounce_timestamp = (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
        }

        if (button_debouncing && 
        (((xTaskGetTickCount() * portTICK_PERIOD_MS)) - 
        button_debounce_timestamp >= 
        TOUCH_DEBOUNCE_DELAY_MS * portTICK_PERIOD_MS))
        {
            button_debouncing = false;
            x_val = ((ifx_lcd_get_Display_Width() - TOUCH_DISPLAY_MAX_INDEX) - TOUCH_DISPLAY_EDGE_OFFSET) - touch_x;
            y_val = (ifx_lcd_get_Display_Height() - TOUCH_DISPLAY_MAX_INDEX) - touch_y;

            if ((x_val >= (home_btn_x_pos - HOME_BUTTON_TOUCH_TOLERANCE)) && (x_val <= (home_btn_x_pos + HOME_BUTTON_WIDTH)) &&
            (y_val >= (home_btn_y_pos - HOME_BUTTON_TOUCH_TOLERANCE)) && (y_val <= (home_btn_y_pos + HOME_BUTTON_HEIGHT)))
            {
                mcu_soft_reset();
            }
        }
    }
}

/*******************************************************************************
* Function Name: touch_input_dev_init
********************************************************************************
* Summary:
* Initializes the touch input device using the CTP FT5406 touch controller and
* creates a periodic timer to read touch inputs. Starts the timer to trigger 
* the touch_read_timer_cb callback function periodically.
*
* Parameters:
*  void
*
* Return:
*  void
*
********************************************************************************/
static void touch_input_dev_init(void)
{
    cy_en_scb_i2c_status_t i2c_status = CY_SCB_I2C_SUCCESS;
    TimerHandle_t touch_timer_handle;

    i2c_status = mtb_ctp_ft5406_init(&ft5406_config);

    if (CY_SCB_I2C_SUCCESS != i2c_status)
    {
        printf("[ERROR] Touch driver initialization failed: %d\r\n", i2c_status);
        CY_ASSERT(RESET_VAL);
    }

    touch_timer_handle = xTimerCreate("touch_read_timer",
                                      pdMS_TO_TICKS(TOUCH_TIMER_PERIOD_MS),
                                      pdTRUE,
                                      (void*)RESET_VAL,
                                      touch_read_timer_cb);

    if (touch_timer_handle != NULL)
    {
        xTimerStart(touch_timer_handle, RESET_VAL);
    }
    else
    {
        printf("[ERROR] Touch read timer creation failed\r\n");
        CY_ASSERT(RESET_VAL);
    }
}
#endif
/*******************************************************************************
* Function Name: VG_switch_frame
*******************************************************************************
*
* Summary:
*  Switches the video/graphics layer frame buffer and manages buffer swapping for
*  display rendering. Signals the USB semaphore based on camera enable status.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void VG_switch_frame(void)
{
    static int current_buffer = DISPLAY_BUFFER_INDEX_0;
    BaseType_t result;

    /* Sets Video/Graphics layer buffer address and transfer the frame buffer to
     * DC */
    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t *)renderTarget->address,
                             &gfx_context);
    __DMB();

    /* Swap buffers */
    current_buffer ^= TICK_VAL;
    renderTarget = &display_buffer[current_buffer];

    __DMB();

    if (!logitech_camera_enabled)
    {
        xSemaphoreGive(usb_semaphore);
    } 
    else
    {
        result = xSemaphoreTake(usb_semaphore, portMAX_DELAY);
        if (result != pdPASS)
        {
            printf("[USB Camera] USB Semphore set failed\r\n");
        }
    }
}

/*******************************************************************************
* Function Name: init_buffer_system
*******************************************************************************
*
* Summary:
*  Initializes the buffer system by resetting image buffer states, clearing buffer
*  counters, and resetting timeout tracking variables.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void init_buffer_system(void)
{
    printf("[LCD_TASK] Initializing buffer system\n");

    /* Ensure clean startup state */
    for (int i = 0; i < NUM_IMAGE_BUFFERS; i++)
    {
        image_buff[i].BufReady = RESET_VAL;
        image_buff[i].NumBytes = RESET_VAL;
    }
    __DMB();

    lastBuffer = RESET_VAL;
    __DMB();

    /* Reset timeout tracking */
    last_successful_frame_time = 0;
    recovery_attempts = RESET_VAL;

    printf("[LCD_TASK] Buffer system initialized\n");
}

/*******************************************************************************
* Function Name: cm55_ns_gfx_task
*******************************************************************************
*
* Summary:
*  Initializes and manages the graphics subsystem, including display controller,
*  I2C, VGLite, and buffer systems. Handles rendering of camera frames and 
*  detection status
*
* Parameters:
*  arg - Unused task argument
*
* Return:
*  None
*
*******************************************************************************/
void cm55_ns_gfx_task(void *arg)
{
    CY_UNUSED_PARAMETER(arg);
    vg_lite_error_t     vglite_status = VG_LITE_SUCCESS;
    cy_en_gfx_status_t  status = CY_GFX_SUCCESS;

    cy_en_sysint_status_t sysint_status = CY_SYSINT_SUCCESS;
    cy_en_scb_i2c_status_t i2c_result = CY_SCB_I2C_SUCCESS;

    /* Set frame buffer address to the GFXSS configuration structure */
    GFXSS_config.dc_cfg->gfx_layer_config->buffer_address    = (gctADDRESS *) vglite_heap_base;
    GFXSS_config.dc_cfg->gfx_layer_config->uv_buffer_address = (gctADDRESS *) vglite_heap_base;

    /* Initializes the graphics subsystem according to the configuration */
    status = Cy_GFXSS_Init(base, &GFXSS_config, &gfx_context);
    if ( CY_GFX_SUCCESS != status ) 
    {
           printf("Graphics subsystem initialization failed: Cy_GFXSS_Init() returned error %d\r\n", status);
           CY_ASSERT(RESET_VAL);
    }

    /* setup Display Controller */
    sysint_status = Cy_SysInt_Init(&dc_irq_cfg, dc_irq_handler);

    if (CY_SYSINT_SUCCESS != sysint_status)
    {
        printf("Error in registering DC interrupt: %d\r\n", sysint_status);
        CY_ASSERT(0);
    }

    NVIC_EnableIRQ(GFXSS_DC_IRQ);        /* Enable interrupt in NVIC. */

    Cy_GFXSS_Clear_DC_Interrupt(base, &gfx_context);

    /* Initialize GFX GPU interrupt */
    sysint_status = Cy_SysInt_Init(&gpu_irq_cfg, gpu_irq_handler);

    if (CY_SYSINT_SUCCESS != sysint_status)
    {
        printf("Error in registering GPU interrupt: %d\r\n", sysint_status);
        CY_ASSERT(RESET_VAL);
    }

    /* Enable GPU interrupt */
    Cy_GFXSS_Enable_GPU_Interrupt(base);

    /* Enable GFX GPU interrupt in NVIC. */
    NVIC_EnableIRQ(GFXSS_GPU_IRQ);

    /* Initialize the I2C in controller mode. */
    i2c_result = Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW,
                &CYBSP_I2C_CONTROLLER_config, 
                &i2c_controller_context);

    if (CY_SCB_I2C_SUCCESS != i2c_result)
    {
        printf("I2C controller initialization failed !!\n");
        CY_ASSERT(RESET_VAL);
    }

    /* Initialize the I2C interrupt */
    sysint_status = Cy_SysInt_Init(&i2c_controller_irq_cfg,
                                   &i2c_controller_interrupt);

    if (CY_SYSINT_SUCCESS != sysint_status)
    {
        printf("I2C controller interrupt initialization failed\r\n");
        CY_ASSERT(RESET_VAL);
    }

    /* Enable the I2C interrupts. */
    NVIC_EnableIRQ(i2c_controller_irq_cfg.intrSrc);

    /* Enable the I2C */
    Cy_SCB_I2C_Enable(CYBSP_I2C_CONTROLLER_HW);

    i2c_result = mtb_disp_waveshare_4p3_init(CYBSP_I2C_CONTROLLER_HW,
                                             &i2c_controller_context);

    if (CY_SCB_I2C_SUCCESS != i2c_result)
    {
        printf("Waveshare 4.3-Inch display init failed with status = %u\r\n", 
            (unsigned int) i2c_result);
        CY_ASSERT(RESET_VAL);
    }

    vg_module_parameters_t vg_params;
    vg_params.register_mem_base = (uint32_t) GFXSS_GFXSS_GPU_GCNANO;
    vg_params.gpu_mem_base[INITIAL_VAL] = GPU_MEM_BASE;
    vg_params.contiguous_mem_base[INITIAL_VAL] = (volatile void*) vglite_heap_base;
    vg_params.contiguous_mem_size[INITIAL_VAL] = VGLITE_HEAP_SIZE;

    /* Init VGLite */
    vg_lite_init_mem(&vg_params);

    /* Initialize the draw */
    vglite_status = vg_lite_init(DISPLAY_W, DISPLAY_H);
    if ( VG_LITE_SUCCESS != vglite_status )
    {
        vg_lite_error_exit("vg_lite engine init failed: vg_lite_init() "
            "returned error %d\r\n",
            vglite_status);
    }

    /* setup double display-frame-buffers */
    for ( int32_t ii = INITIAL_VAL; ii < VALUE_TWO; ii++ ) 
    {
        display_buffer[ii].width  = DISPLAY_W;
        display_buffer[ii].height = DISPLAY_H;
        display_buffer[ii].format = VG_LITE_BGR565;
        vglite_status = vg_lite_allocate(&display_buffer[ii]);
        if ( VG_LITE_SUCCESS != vglite_status ) 
        {
            vg_lite_error_exit(
                "display_buffer[] allocation failed in vglite space: "
                "vg_lite_allocate() returned error %d\r\n",
                vglite_status);
        }
    }
    renderTarget = &display_buffer[DISPLAY_BUFFER_INDEX_0];

    /* Allocate the camera buffers */
    for ( int32_t i = INITIAL_VAL; i < NUM_IMAGE_BUFFERS; i++ ) 
    {
        usb_yuv_frames[i].width  = CAMERA_WIDTH;
        usb_yuv_frames[i].height = CAMERA_HEIGHT;
        usb_yuv_frames[i].format = VG_LITE_YUYV;
        usb_yuv_frames[i].image_mode = VG_LITE_NORMAL_IMAGE_MODE;
        vglite_status = vg_lite_allocate(&usb_yuv_frames[i]);
        if ( VG_LITE_SUCCESS != vglite_status ) 
        {
            vg_lite_error_exit(
                    "camera buffers allocation failed in vglite space: "
                    "vg_lite_allocate() returned error %d\r\n",
                    vglite_status);
        }
    }

    /* Allocate the work camera buffer */
    bgr565.width  = CAMERA_WIDTH;
    bgr565.height = CAMERA_HEIGHT;
    bgr565.format = VG_LITE_BGR565;
    bgr565.image_mode = VG_LITE_NORMAL_IMAGE_MODE;
    vglite_status = vg_lite_allocate(&bgr565);
    if ( VG_LITE_SUCCESS != vglite_status ) 
    {
        vg_lite_error_exit(
            "work camera image bgr565 allocation failed in vglite space: "
            "vg_lite_allocate() returned error %d\r\n",
            vglite_status);
        }

    /* Clear the buffer with White */
    vglite_status = vg_lite_clear(renderTarget, NULL, BLACK_COLOR);
    if ( VG_LITE_SUCCESS != vglite_status ) 
    {
        vg_lite_error_exit(
            "Clear failed: vg_lite_clear() returned error %d\r\n",
            vglite_status);
    }

    /* Define the transformation matrix that will be applied from the camera 
     * image to the display */
    vg_lite_identity(&matrix);

    /* scale factor from the camera image to the display */
    float scale_Cam2Disp_x = (float)(DISPLAY_W) / (float)CAMERA_WIDTH;
    float scale_Cam2Disp_y = (float)(DISPLAY_H) / (float)CAMERA_HEIGHT;
    scale_Cam2Disp = max(scale_Cam2Disp_x, scale_Cam2Disp_y);
    vg_lite_scale(scale_Cam2Disp, scale_Cam2Disp, &matrix);

    /* Move the scaled-frame to the display center */
    float translate_x = ((DISPLAY_W) / scale_Cam2Disp - CAMERA_WIDTH)  * DIVIDE_HALF;
    float translate_y = (DISPLAY_H / scale_Cam2Disp - CAMERA_HEIGHT) * DIVIDE_HALF;
    vg_lite_translate( translate_x, translate_y, &matrix );

    /* Move the scaled-frame to the display center */
    display_offset_x = ((DISPLAY_W) - scale_Cam2Disp * IMAGE_WIDTH)   / VALUE_TWO;
    display_offset_y = (DISPLAY_H - scale_Cam2Disp * CAMERA_HEIGHT) / VALUE_TWO;


    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) renderTarget->address, &gfx_context);

    vg_lite_flush();
    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) renderTarget->address, &gfx_context);

    /* Delay for USB enumeration to complete before rendering data to the display */
    vTaskDelay(pdMS_TO_TICKS(USB_ENUMERATION_DELAY_MS));

#if defined(HOME_BUTTON)
    touch_input_dev_init();

     home_btn_x_pos = ifx_lcd_get_Display_Width() - HOME_BUTTON_WIDTH - HOME_BUTTON_MARGIN_RIGHT_PX;
     home_btn_y_pos = HOME_BUTTON_MARGIN_TOP_PX;
#endif
    /* Initialize buffer states - clear any startup artifacts */
    for (int i = INITIAL_VAL; i < NUM_IMAGE_BUFFERS; i++)
    {
        image_buff[i].BufReady = RESET_VAL;
        image_buff[i].NumBytes = RESET_VAL;
    }
    __DMB();

    init_buffer_system();

    for ( ;; )
    {

        if(!_device_connected)
        {
            int i = RESET_VAL;
            while (i < NO_CAMERA_DISPLAY_LOOP_LIMIT)
            {
                i++;
                __DMB();

                vg_lite_finish();
                Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) renderTarget->address, &gfx_context);

                __DMB();

                /* Clear the buffer with Black */
                vglite_status = vg_lite_clear(renderTarget, NULL, BLACK_COLOR);
                if ( VG_LITE_SUCCESS != vglite_status )
                {
                    vg_lite_error_exit("Clear failed: vg_lite_clear() returned error %d\r\n", vglite_status);
                }

                __DMB();

                ifx_lcd_display_Rect( NO_CAMERA_IMG_X_POS , NO_CAMERA_IMG_Y_POS,  (uint8_t *) no_camera_img_map, 
                                      NO_CAMERA_IMG_WIDTH, NO_CAMERA_IMG_HEIGHT );
#if defined(HOME_BUTTON)
                ifx_lcd_display_Rect(home_btn_x_pos, home_btn_y_pos, (uint8_t *) home_btn_img_map, HOME_BUTTON_WIDTH, HOME_BUTTON_HEIGHT);
#endif
                /* Sets Video/Graphics layer buffer address and transfer the frame buffer to DC */
                Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) renderTarget->address, &gfx_context);

                __DMB();
            }
        }
        else
        {

            if(camera_not_supported)
            {
                int i = RESET_VAL;
                while (i < CAMERA_NOT_SUPPORTED_LOOP_LIMIT)
                {
                    i++;
                    __DMB();

                    vg_lite_finish();
                    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) renderTarget->address, &gfx_context);

                    __DMB();

                    /* Clear the buffer with Black */
                    vglite_status = vg_lite_clear(renderTarget, NULL, BLACK_COLOR);
                    if ( VG_LITE_SUCCESS != vglite_status )
                    {

                        vg_lite_error_exit("Clear failed: vg_lite_clear() returned error %d\r\n", vglite_status);
                    }

                    __DMB();

                    ifx_lcd_display_Rect( CAMERA_NOT_SUPPORTED_IMG_X_POS , CAMERA_NOT_SUPPORTED_IMG_Y_POS,  
                                        (uint8_t *) camera_not_supported_img_map, CAMERA_NOT_SUPPORTED_IMG_WIDTH, 
                                        CAMERA_NOT_SUPPORTED_IMG_HEIGHT );
#if defined(HOME_BUTTON)
                    ifx_lcd_display_Rect(home_btn_x_pos, home_btn_y_pos, (uint8_t *) home_btn_img_map, HOME_BUTTON_WIDTH, HOME_BUTTON_HEIGHT);
#endif
                    /* Sets Video/Graphics layer buffer address and transfer the frame buffer to DC */
                    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) renderTarget->address, &gfx_context);

                    __DMB();
                }

                while(_device_connected);

            }
        }

        if ( xSemaphoreTake(model_semaphore, portMAX_DELAY) == pdTRUE )
        {
            if(_device_connected)
            {
#if defined(HOME_BUTTON)
                ifx_lcd_display_Rect( (DISPLAY_WIDTH_PX - HOME_BUTTON_X_OFFSET_FOR_ICON), HOME_BUTTON_MARGIN_TOP_PX,  (uint8_t *) home_btn_img_map, HOME_BUTTON_WIDTH, HOME_BUTTON_HEIGHT );
#endif
                /* Update box */
                update_bounding_box_data( renderTarget, &Prediction );
                update_model_inference_time( renderTarget, Inference_time);

                VG_switch_frame();
             }
        }
    }
}

/* [] END OF FILE */