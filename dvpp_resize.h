/*
* Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef _PICTURE_INC_DVPP_RESIZE_H
#define _PICTURE_INC_DVPP_RESIZE_H

#include <vector>
#include <cstdint>
#include "acl/acl.h"
#include "acl/ops/acl_dvpp.h"
#include "data_type.h"

#define RGBU8_IMAGE_SIZE(width, height) ((width) * (height) * 3)
#define YUV420SP_SIZE(width, height) ((width) * (height) * 3 / 2)

#define ALIGN_UP(num, align) (((num) + (align) - 1) & ~((align) - 1))
#define ALIGN_UP2(num) ALIGN_UP(num, 2)
#define ALIGN_UP16(num) ALIGN_UP(num, 16)
#define ALIGN_UP64(num) ALIGN_UP(num, 64)
#define ALIGN_UP128(num) ALIGN_UP(num, 128)

typedef struct{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t alignWidth = 0;
    uint32_t alignHeight = 0;
    uint32_t size = 0;
    uint8_t* data;
} DVPPImageData ;

typedef struct{
    aclrtContext context;
    aclrtStream stream;
    uint32_t input_format;
    uint32_t batch_size;
    uint32_t resized_width;
    uint32_t resized_height;
    uint32_t is_fix_scale_resize = 1;  //yolov6 && rtmpose: 1
    uint32_t is_symmetry_padding = 1;  //rtmpose: 1
    float resize_scale_factor = 1.0f; //rtmpose: 1.25f
    char reserve[8];
}DVPPResizeInitConfig;

class DvppResize {
public:
    /**
    * @brief Constructor
    * @param [in] stream: stream
    */
    DvppResize();

    void Init(const DVPPResizeInitConfig* dvppResizeInitConfig);

    /**
    * @brief Destructor
    */
    ~DvppResize();

    /**
    * @brief dvpp process
    * @return result
    */
    int Process(const DVPPImageData* srcImage, const  RectInt* rois, int img_num);

    int Get(DVPPImageData& resizedImage, int index) const;

    int GetHostData(DVPPImageData& resizedImage, int index);

    inline bool HasInit() const
    {
        return has_init_over_;
    }

    const uint8_t* GetOutputDevicePtr() const;

    void DestroyResource();

private:
    int InitResizeInputDesc(const DVPPImageData& inputImage, int index);

    int InitResizeOutputDesc();

    void ProcessFullImage(const DVPPImageData* srcImage, int img_num);

    void ProcessSubImage(const DVPPImageData* srcImage, const RectInt* rois, int img_num);

private:
    DVPPResizeInitConfig dvppResizeInitConfig_;

    std::vector<uint32_t> src_widths_;
    std::vector<uint32_t> src_heights_;

    acldvppChannelDesc *g_dvppChannelDesc_;
    acldvppResizeConfig *g_resizeConfig_;

    acldvppBatchPicDesc *g_vpcBatchInputDesc_; // vpc input desc
    acldvppBatchPicDesc *g_vpcBatchOutputDesc_; // vpc output desc

    void* g_vpcBatchOutBufferDev_;  // input pic dev buffer
    uint32_t g_vpcOutBufferSize_;  // vpc output size

    acldvppPixelFormat g_format_;
    bool has_init_over_;

    std::vector<uint32_t> g_roiNums_;
    std::vector<acldvppRoiConfig*> g_cropArea_;
    std::vector<acldvppRoiConfig*> g_pasteArea_;

    // copy data from device to host
    std::vector<uint8_t> out_host_data_;
};

#endif // _PICTURE_INC_DVPP_RESIZE_H