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

#define RGBU8_IMAGE_SIZE(width, height) ((width) * (height) * 3)
#define YUV420SP_SIZE(width, height) ((width) * (height) * 3 / 2)

#define ALIGN_UP(num, align) (((num) + (align) - 1) & ~((align) - 1))
#define ALIGN_UP2(num) ALIGN_UP(num, 2)
#define ALIGN_UP16(num) ALIGN_UP(num, 16)
#define ALIGN_UP64(num) ALIGN_UP(num, 64)
#define ALIGN_UP128(num) ALIGN_UP(num, 128)

struct ImageData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t alignWidth = 0;
    uint32_t alignHeight = 0;
    uint32_t size = 0;
    uint8_t* data;
};

class DvppResize {
public:
    /**
    * @brief Constructor
    * @param [in] stream: stream
    */
    DvppResize(aclrtStream &stream, uint32_t batch_size, uint32_t resized_width, uint32_t resized_height);

    /**
    * @brief Destructor
    */
    ~DvppResize();

    /**
    * @brief dvpp process
    * @return result
    */
    int Process(ImageData* srcImage, int img_num);

    int Get(ImageData& resizedImage, int index);

    void DestroyResource();

private:
    int InitResizeInputDesc(ImageData& inputImage, int index);

    int InitResizeOutputDesc();

private:
    std::vector<uint32_t> src_widths_;
    std::vector<uint32_t> src_heights_;

    aclrtStream stream_;
    acldvppChannelDesc *g_dvppChannelDesc_;
    acldvppResizeConfig *g_resizeConfig_;

    acldvppBatchPicDesc *g_vpcBatchInputDesc_; // vpc input desc
    acldvppBatchPicDesc *g_vpcBatchOutputDesc_; // vpc output desc

    std::vector<void *> g_vpcBatchOutBufferDev_;  // input pic dev buffer
    uint32_t g_vpcOutBufferSize_;  // vpc output size

    uint32_t g_resizeWidth_;
    uint32_t g_resizeHeight_;
    acldvppPixelFormat g_format_;

    int  g_batch_size_;
    std::vector<uint32_t> g_roiNums_;
    std::vector<acldvppRoiConfig*> g_cropArea_;
};

#endif // _PICTURE_INC_DVPP_RESIZE_H