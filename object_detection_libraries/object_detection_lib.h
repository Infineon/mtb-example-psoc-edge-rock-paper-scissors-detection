/*******************************************************************************
* File Name        : object_detection_lib.h
*
* Description      : This is the header file of detection utility.
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

#ifndef _OBJECT_DETECTION_H_
#define _OBJECT_DETECTION_H_

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "object_detection_structs.h"
#include "cy_utils.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "mtb_ml_utils.h"
#include "mtb_ml_common.h"
#include "mtb_ml.h"

/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/*******************************************************************************
 * Function Name: ifx_object_detect_init
 *******************************************************************************
 * Summary:
 *  Initializes the MTB ML model and links the model input buffer to the model
 *  input tensor using provided input/output data structures.
 *
 * Parameters:
 *  model_bin   : Pointer to the model binary structure
 *  input_data  : Pointer to model input configuration
 *  output_data : Pointer to model output configuration
 *
 * Return:
 *  cy_rslt_t : MTB_ML_RESULT_SUCCESS on success, MTB_ML_RESULT_BAD_ARG on invalid input
 *******************************************************************************/
cy_rslt_t ifx_object_detect_init(mtb_ml_model_bin_t  *model_bin, model_detail_t *input_data, 
                                    model_detail_t *output_data);

/*******************************************************************************
 * Function Name: ifx_object_detect_deinit
 *******************************************************************************
 * Summary:
 *  Deinitializes the neural network model runtime and frees all dynamically
 *  allocated memory. Should be called once at cleanup.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  cy_rslt_t : MTB_ML_RESULT_SUCCESS on success
 *******************************************************************************/
cy_rslt_t ifx_object_detect_deinit();

/*******************************************************************************
 * Function Name: ifx_object_detect_preprocess
 *******************************************************************************
 * Summary:
 *  Preprocesses the raw image buffer (int8_t) and updates the model input buffer
 *  for inference.
 *
 * Parameters:
 *  image_buf_int8 : Pointer to raw image data (int8_t format)
 *  input_data     : Pointer to model input configuration
 *
 * Return:
 *  cy_rslt_t : Result of preprocessing operation
 *******************************************************************************/
cy_rslt_t ifx_object_detect_preprocess(int8_t *image_buf_int8,
                                       model_detail_t *input_data);

/*******************************************************************************
 * Function Name: ifx_object_detect_inference
 *******************************************************************************
 * Summary:
 *  Executes inference on the preprocessed input data using the loaded MTB ML model.
 *
 * Parameters:
 *  processed_data : Pointer to preprocessed input data buffer
 *
 * Return:
 *  cy_rslt_t : MTB_ML_RESULT_SUCCESS on success,
 *              MTB_ML_RESULT_BAD_ARG on invalid input,
 *              MTB_ML_RESULT_INFERENCE_ERROR on failure
 *******************************************************************************/
cy_rslt_t ifx_object_detect_inference(int8_t *processed_data);

/*******************************************************************************
 * Function Name: ifx_object_detect_postprocess
 *******************************************************************************
 * Summary:
 *  Applies postprocessing (e.g., NMS, filtering) to raw model outputs and
 *  populates the prediction structure with refined detections.
 *
 * Parameters:
 *  opt          : Pointer to detection configuration options
 *  prediction   : Pointer to output prediction structure
 *  output_data  : Pointer to model output data containing raw detections
 *
 * Return:
 *  int32_t : Number of valid detections after postprocessing
 *******************************************************************************/
int32_t ifx_object_detect_postprocess(const detection_opt_t *opt,
                                      prediction_OD_t *prediction,
                                      model_detail_t *output_data);

#endif /* _OBJECT_DETECTION_H_ */

/* [] END OF FILE */