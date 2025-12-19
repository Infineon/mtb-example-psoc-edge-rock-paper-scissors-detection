/*******************************************************************************
* File Name        : object_detection_structs.h
*
* Description      : This is the header file of object detection utility C 
*                    structs.
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

#ifndef _DETECTION_H_
#define _DETECTION_H_

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include "cy_result.h"
#include "definitions.h"
#pragma once

/*******************************************************************************
* Macros
*******************************************************************************/
#ifndef max
#define max(a, b)                             ((a) > (b) ? (a) : (b))
#define min(a, b)                             ((a) < (b) ? (a) : (b))
#endif  // max

#define QUANTIZE( r, zero_point, scale )      ((int)((r) / (scale) + 0.5f) + (zero_point))
#define DEQUANTIZE( q, zero_point, scale )    (((int)(q) - (zero_point)) * (scale))

/******************************************************************************
 * Typedefs
 *****************************************************************************/

/******************************************************************************
* \struct ImageProperty_t
*
* \brief
* Input image structure.
*
* \details
* Detailed description of the struct's purpose and usage (optional)
*
*******************************************************************************/
/**
 * Detection option structure
 */
typedef struct detection_opt_st
{
    uint8_t    *image_buf;
    float      image_width;                /* Original width of image */
    float      image_height;               /* Original height of image */
    float      threshold_score;            /* Threshold for confidence score */
    float      threshold_IOU;              /* Threshold for IoU (intersection over union) in NMS */
    int32_t    max_detections;             /* Maximum number of predictions */
} detection_opt_t;

typedef struct model_detail {
    int8_t *    data;
    size_t      size;
    int *       dim;            /* MTB ML's type */
    int         dim_len;
    int         zero;
    float       scale;
} model_detail_t;


/* Final output variables */
typedef struct {
    int32_t     count;
    int16_t     bbox_int16[Max_detections * 4];
    float       conf[Max_detections];
    uint8_t     class_id[Max_detections];
} prediction_OD_t;

/*******************************************************************************
* Global Variables
*******************************************************************************/
extern detection_opt_t Opt; /* Declare Opt as extern */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* _DETECTION_H_ */

/* [] END OF FILE */