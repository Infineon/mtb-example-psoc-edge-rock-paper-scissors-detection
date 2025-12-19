/*******************************************************************************
* File Name        : main.c
*
* Description      : This source file contains the main routine for non-secure
*                    application running on CM55 CPU.
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
#include <stdint.h>
#include "cy_time.h"
#include "cybsp.h"
#include "retarget_io_init.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "definitions.h"
#include "object_detection_lib.h"
#include "lcd_task.h"
#include "time_utils.h"
#include "cycfg_qspi_memslot.h"
#include "mtb_serial_memory.h"
#if defined(TENXER_UART)
#include "oob_command_handler.h"
#endif /*TENXER_UART*/

/*******************************************************************************
* Macros
*******************************************************************************/
/* The timeout value in microsecond used to wait for core to be booted */
#define SMIF_INIT_TIMEOUT_USEC             (1000U) /* SMIF init timeout */

#define GFX_TASK_NAME                      ("CM55 Gfx Task")
#define GFX_TASK_STACK_SIZE                (10U * 1024U)
#define GFX_TASK_PRIORITY                  (configMAX_PRIORITIES - 5U)

#define USB_WEBCAM_TASK_NAME               ("CM55 USB Webcam Task")
#define USB_WEBCAM_TASK_STACK_SIZE         (20U * 1024U)
#define USB_WEBCAM_TASK_PRIORITY           (configMAX_PRIORITIES - 3U)

#define INFERENCE_TASK_NAME                ("CM55 Inference Task")
#define INFERENCE_TASK_STACK_SIZE          (64U * 1024U)
#define INFERENCE_TASK_PRIORITY            (configMAX_PRIORITIES - 4U)

/* Semaphore initialization */

#define SEMAPHORE_INITIAL_COUNT            (0U)

/* Enabling or disabling a MCWDT requires a wait time of upto 2 CLK_LF cycles
 * to come into effect. This wait time value will depend on the actual CLK_LF
 * frequency set by the BSP.
 */
#define LPTIMER_1_WAIT_TIME_USEC            (62U)

/* Define the LPTimer interrupt priority number. '1' implies highest priority.
 */
#define APP_LPTIMER_INTERRUPT_PRIORITY      (1U)
/*******************************************************************************
* Global Variables
*******************************************************************************/
static TaskHandle_t usb_webcam_thread;
static TaskHandle_t gfx_thread;
static TaskHandle_t inference_thread;
#if defined(TENXER_UART)
static TaskHandle_t uart_cmd_thread;
#endif /*TENXER_UART*/
static mtb_serial_memory_t serial_memory_obj;
static cy_stc_smif_mem_context_t smif_mem_context;
static cy_stc_smif_mem_info_t smif_mem_info;

SemaphoreHandle_t model_semaphore;
SemaphoreHandle_t usb_semaphore;
TimerHandle_t countdown_timer;
/* LPTimer HAL object */
static mtb_hal_lptimer_t lptimer_obj;
/* RTC HAL object */
static mtb_hal_rtc_t rtc_obj;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void check_status(char *message, uint32_t status);
void cm55_ns_gfx_task(void *arg);
void cm55_usb_webcam_task(void *arg);
void cm55_inference_task(void *arg);
void check_status(char *message, uint32_t status);

/*******************************************************************************
* Function Name: vApplicationStackOverflowHook
********************************************************************************
* Summary:
*   Hook function called when a stack overflow is detected for a task. Triggers
*   an assertion to halt execution.
*
* Parameters:
*   TaskHandle_t xTask - Handle of the task that overflowed (not used)
*   char *pcTaskName - Name of the task that overflowed (not used)
*
* Return:
*   None
*
*******************************************************************************/
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) 
{
    CY_UNUSED_PARAMETER(xTask);
    CY_UNUSED_PARAMETER(pcTaskName);
    configASSERT(0);
}

/*******************************************************************************
* Function Name: vApplicationMallocFailedHook
********************************************************************************
* Summary:
*   Hook function called when a memory allocation failure is detected. Triggers
*   an assertion to halt execution.
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/
void vApplicationMallocFailedHook(void) 
{

    configASSERT(0);
}

/*******************************************************************************
 * Function Name: check_status
 ********************************************************************************
 * Summary:
 *  Prints the message, indicates the non-zero status by turning the LED on, and
 *  asserts the non-zero status.
 *
 * Parameters:
 *  message - message to print if status is non-zero.
 *  status - status for evaluation.
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void check_status(char *message, uint32_t status) {
    if (status) {
        printf("\n\r====================================================\n\r");
        printf("\n\rFAIL: %s\n\r", message);
        printf("Error Code: 0x%x\n\r", (int)status);
        printf("\n\r====================================================\n\r");

        /* On failure, turn the LED ON */
        Cy_GPIO_Set(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
        while (true);
    }
}

/*******************************************************************************
* Function Name: setup_clib_support
********************************************************************************
* Summary:
*    1. This function configures and initializes the Real-Time Clock (RTC)).
*    2. It then initializes the RTC HAL object to enable CLIB support library
*       to work with the provided Real-Time Clock (RTC) module.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void setup_clib_support(void)
{
    /* RTC Initialization is done in CM33 non-secure project */

    /* Initialize the ModusToolbox CLIB support library */
    mtb_clib_support_init(&rtc_obj);
}

/*******************************************************************************
* Function Name: lptimer_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for LPTimer instance.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void lptimer_interrupt_handler(void)
{
    mtb_hal_lptimer_process_interrupt(&lptimer_obj);
}

/*******************************************************************************
* Function Name: setup_tickless_idle_timer
********************************************************************************
* Summary:
*    1. This function first configures and initializes an interrupt for LPTimer.
*    2. Then it initializes the LPTimer HAL object to be used in the RTOS
*       tickless idle mode implementation to allow the device enter deep sleep
*       when idle task runs. LPTIMER_1 instance is configured for CM55 CPU.
*    3. It then passes the LPTimer object to abstraction RTOS library that
*       implements tickless idle mode
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void setup_tickless_idle_timer(void)
{
    /* Interrupt configuration structure for LPTimer */
    cy_stc_sysint_t lptimer_intr_cfg =
    {
        .intrSrc = CYBSP_CM55_LPTIMER_1_IRQ,
        .intrPriority = APP_LPTIMER_INTERRUPT_PRIORITY
    };

    /* Initialize the LPTimer interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status =
                                    Cy_SysInt_Init(&lptimer_intr_cfg,
                                                    lptimer_interrupt_handler);

    /* LPTimer interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(lptimer_intr_cfg.intrSrc);

    /* Initialize the MCWDT block */
    cy_en_mcwdt_status_t mcwdt_init_status =
                                    Cy_MCWDT_Init(CYBSP_CM55_LPTIMER_1_HW,
                                                &CYBSP_CM55_LPTIMER_1_config);

    /* MCWDT initialization failed. Stop program execution. */
    if(CY_MCWDT_SUCCESS != mcwdt_init_status)
    {
        handle_app_error();
    }

    /* Enable MCWDT instance */
    Cy_MCWDT_Enable(CYBSP_CM55_LPTIMER_1_HW,
                    CY_MCWDT_CTR_Msk,
                    LPTIMER_1_WAIT_TIME_USEC);

    /* Setup LPTimer using the HAL object and desired configuration as defined
     * in the device configurator. */
    cy_rslt_t result = mtb_hal_lptimer_setup(&lptimer_obj,
                                            &CYBSP_CM55_LPTIMER_1_hal_config);

    /* LPTimer setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Pass the LPTimer object to abstraction RTOS library that implements
     * tickless idle mode
     */
    cyabs_rtos_set_lptimer(&lptimer_obj);
}

/*******************************************************************************
 * Function Name: smif_ospi_psram_init
 ********************************************************************************
 * Summary:
 *  Initialize SMIF for PSRAM (QSPI Core 1) and set-up serial memory interface
 *
 * Parameters:
 *  none
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void smif_ospi_psram_init(void) {
    cy_rslt_t result;

    /* Disable SMIF Block for reconfiguration. */
    Cy_SMIF_Disable(CYBSP_SMIF_CORE_1_PSRAM_HW);

    /* Initialize SMIF-1 Peripheral. */
    result =
        Cy_SMIF_Init((CYBSP_SMIF_CORE_1_PSRAM_hal_config.base),
                     (CYBSP_SMIF_CORE_1_PSRAM_hal_config.config),
                     SMIF_INIT_TIMEOUT_USEC, &smif_mem_context.smif_context);

    check_status("Cy_SMIF_Init failed", result);

    /* Configure Data Select Option for SMIF-1 */
    Cy_SMIF_SetDataSelect(CYBSP_SMIF_CORE_1_PSRAM_hal_config.base,
                          smif1BlockConfig.memConfig[0]->slaveSelect,
                          smif1BlockConfig.memConfig[0]->dataSelect);

    /* Enable the SMIF_CORE_1 block. */
    Cy_SMIF_Enable(CYBSP_SMIF_CORE_1_PSRAM_hal_config.base,
                   &smif_mem_context.smif_context);

    /* Set-up serial memory. */
    result = mtb_serial_memory_setup(
        &serial_memory_obj, MTB_SERIAL_MEMORY_CHIP_SELECT_2,
        CYBSP_SMIF_CORE_1_PSRAM_hal_config.base,
        CYBSP_SMIF_CORE_1_PSRAM_hal_config.clock, &smif_mem_context,
        &smif_mem_info, &smif1BlockConfig);

    check_status("serial memory setup failed", result);
}

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 *  This is the main function for CM55 non-secure application.
 *    1. It initializes the device and board peripherals.
 *    2. It creates the FreeRTOS application task 'cm55_ns_gfx_task'.
 *    3. It starts the RTOS task scheduler.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 *******************************************************************************/
int main(void) {
    cy_rslt_t result;
    BaseType_t task_status;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0); /* Board init failed. Stop program execution */
    }

    /* Setup CLIB support library. */
    setup_clib_support();

    /* Initialize retarget-io middleware */
    init_retarget_io();

    ifx_time_start();

    /* Setup the LPTimer instance for CM55*/
    setup_tickless_idle_timer();

    /* Initialize PSRAM and set-up serial memory */
    smif_ospi_psram_init();

    check_status("smif_ospi_psram_init error", (uint32_t)result);

    /* Enable XIP mode for the SMIF memory slot associated with the PSRAM. */
    result = mtb_serial_memory_enable_xip(&serial_memory_obj, true);
    check_status("mtb_serial_memory_enable_xip: failed", result);

    /* Enable write for the SMIF memory slot associated with the PSRAM. */
    result = mtb_serial_memory_set_write_enable(&serial_memory_obj, true);
    check_status("mtb_serial_memory_set_write_enable: failed", result);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("\r\n***************************************************************"
           "**********************\r\n");
    printf("PSOC Edge MCU: Rock Paper Scissors detection demo with USB Camera\r\n");
    printf("Build Version: %d.%d.%d\r\n", MAJOR_VERSION, MINOR_VERSION,
           PATCH_VERSION);
    printf("Build Date: %s\r\n", __DATE__);
    printf("Build Time: %s\r\n", __TIME__);
    printf("Following cameras are supported:\r\n");
    printf("1. HBVCAM OV7675 0.3MP Camera: "
           "https://www.hbvcamera.com/0-3mp-pixel-usb-cameras/"
           "hbvcam-ov7675-0.3mp-mini-laptop-camera-module.html\r\n");
    printf("2. Logitech C920 HD Pro Webcam: "
           "https://www.logitech.com/en-ch/shop/p/c920-pro-hd-webcam\r\n");
    printf(
        "3. HBVCAM OS02F10 2MP Camera: "
        "https://www.hbvcamera.com/2-mega-pixel-usb-cameras/"
        "2mp-1080p-auto-focus-hd-usb-camera-module-for-atm-machine.html\r\n");
    printf("\r\n***************************************************************"
           "**********************\r\n");

    /***********************************************************************************/
     /* Create Semaphores*/
    /***********************************************************************************/

    usb_semaphore = xSemaphoreCreateCounting(NUM_IMAGE_BUFFERS, SEMAPHORE_INITIAL_COUNT);
    if (usb_semaphore == NULL)
    {
        handle_app_error();
    }

    model_semaphore = xSemaphoreCreateCounting(NUM_IMAGE_BUFFERS, SEMAPHORE_INITIAL_COUNT);
    if (model_semaphore == NULL) 
    {
        handle_app_error();
    }
    
    /***********************************************************************************/
     /* Create tasks */
    /***********************************************************************************/

     /* Create GFX task */
    task_status = xTaskCreate(cm55_ns_gfx_task, GFX_TASK_NAME,
                   GFX_TASK_STACK_SIZE / sizeof(StackType_t), NULL,
                   GFX_TASK_PRIORITY, &gfx_thread);
    if (task_status != pdPASS) 
    {
        handle_app_error(); /* Task creation failed*/
    }

    /* Create Inference task */
    task_status = xTaskCreate(cm55_inference_task, INFERENCE_TASK_NAME,
                    INFERENCE_TASK_STACK_SIZE / sizeof(StackType_t), NULL,
                    INFERENCE_TASK_PRIORITY, &inference_thread);
    if (task_status != pdPASS) 
    {
        handle_app_error(); /* Task creation failed*/
    }

    /* Create Usb_Webcam task */
    task_status = xTaskCreate(cm55_usb_webcam_task, USB_WEBCAM_TASK_NAME,
                    USB_WEBCAM_TASK_STACK_SIZE / sizeof(StackType_t), NULL,
                    USB_WEBCAM_TASK_PRIORITY, &usb_webcam_thread);
    if (task_status != pdPASS)
    {
        handle_app_error(); /* Task creation failed*/
    }

#if defined(TENXER_UART)
    /* Create TenXer UART command task */
    task_status = xTaskCreate(oob_uart_cmd_task, "OOB_UART_TASK_NAME",
                    OOB_UART_TASK_STACK_SIZE / sizeof(StackType_t), NULL,
                    OOB_UART_TASK_PRIORITY, &uart_cmd_thread);
    if (task_status != pdPASS)
    {
        handle_app_error(); /* Task creation failed*/
    }
#endif /*TENXER_UART*/

    /* Enable global interrupts */
    __enable_irq();
    /* Start the RTOS Scheduler */
    vTaskStartScheduler();

    /* Should never get here! */
    /* Halt the CPU if scheduler exits */
    handle_app_error();

    return 0;
}

/* [] END OF FILE */
