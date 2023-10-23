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

#include <iostream>
#include "acl/acl.h"
#include "dvpp_resize.h"
#include "common/alg_define.h"

DvppResize::DvppResize(aclrtStream &stream, uint32_t width, uint32_t height)
        : stream_(stream), g_dvppChannelDesc_(nullptr),
          g_resizeConfig_(nullptr), g_vpcInputDesc_(nullptr), g_vpcOutputDesc_(nullptr),
          g_vpcOutBufferDev_(nullptr), g_vpcOutBufferSize_(0)
{
    g_dvppChannelDesc_ = acldvppCreateChannelDesc();
    if (!g_dvppChannelDesc_)
    {
        AIALG_ERROR("acldvppCreateChannelDesc failed\n");
        return;
    }

    aclError aclRet = acldvppCreateChannel(g_dvppChannelDesc_);
    if (aclRet != ACL_SUCCESS)
    {
        AIALG_ERROR("acldvppCreateChannel failed, aclRet = %d\n", aclRet);
        return;
    }

    g_resizeConfig_ = acldvppCreateResizeConfig();
    if (!g_resizeConfig_)
    {
        AIALG_ERROR("Dvpp resize init failed for create config failed\n");
        return;
    }

    aclRet = acldvppSetResizeConfigInterpolation(g_resizeConfig_, 2);
    if (aclRet != ACL_SUCCESS)
    {
        AIALG_ERROR("acldvppSetResizeConfigInterpolation failed, aclRet = %d\n", aclRet);
        return;
    }

    src_width_ = 0;
    src_height_ = 0;

    g_resizeWidth_ = width;
    g_resizeHeight_ = height;

    g_format_ = static_cast<acldvppPixelFormat>(PIXEL_FORMAT_BGR_888);

    g_vpcInputDesc_ = acldvppCreatePicDesc();
    if (!g_vpcInputDesc_)
    {
        AIALG_ERROR("acldvppCreatePicDesc g_vpcInputDesc_ failed\n");
        return;
    }

    if (1 != InitResizeOutputDesc())
    {
        AIALG_ERROR("InitResizeOutputDesc failed\n");
        return;
    }
    AIALG_PRINT("Init success\n");
}

DvppResize::~DvppResize()
{
    AIALG_PRINT("destory success\n");
}

void DvppResize::DestroyResource()
{
    if (g_vpcInputDesc_)
    {
        acldvppDestroyPicDesc(g_vpcInputDesc_);
        g_vpcInputDesc_ = nullptr;
    }
    if (g_vpcOutputDesc_)
    {
        acldvppDestroyPicDesc(g_vpcOutputDesc_);
        g_vpcOutputDesc_ = nullptr;
    }
    if(g_vpcOutBufferDev_)
    {
        acldvppFree(g_vpcOutBufferDev_);
        g_vpcOutBufferDev_ = nullptr;
    }
    if (g_resizeConfig_)
    {
        acldvppDestroyResizeConfig(g_resizeConfig_);
        g_resizeConfig_ = nullptr;
    }
    if (g_dvppChannelDesc_)
    {
        aclError aclRet = acldvppDestroyChannel(g_dvppChannelDesc_);
        if (aclRet != ACL_SUCCESS)
        {
            AIALG_ERROR("acldvppDestroyChannel failed, aclRet = %d\n", aclRet);
        }
        aclRet = acldvppDestroyChannelDesc(g_dvppChannelDesc_);
        if (aclRet != ACL_SUCCESS)
        {
            AIALG_ERROR("acldvppDestroyChannelDesc failed, aclRet = %d\n", aclRet);
        }
        g_dvppChannelDesc_ = nullptr;
    }
}
int DvppResize::InitResizeInputDesc(ImageData &inputImage)
{
    // if the input yuv is from JPEGD, 128*16 alignment on 310, 64*16 alignment on 310P
    // if the input yuv is from VDEC, it shoud be aligned to 16*2
//    uint32_t alignWidth = ALIGN_UP128(inputImage.width);
//    uint32_t alignHeight = ALIGN_UP16(inputImage.height);
    uint32_t alignWidthStride = ALIGN_UP16(inputImage.width) * 3;
    uint32_t alignHeightStride = ALIGN_UP2(inputImage.height);
    uint32_t inputBufferSize = alignWidthStride * alignHeightStride;//YUV420SP_SIZE(alignWidth, alignHeight);

    uint32_t inputWidth = inputImage.width;
    uint32_t inputHeight = inputImage.height;
    uint32_t n = 2;
    if (inputImage.width % n != 0)
        inputWidth--;
    if (inputImage.height % n != 0)
        inputHeight--;

    acldvppSetPicDescFormat(g_vpcInputDesc_, g_format_);
    acldvppSetPicDescWidth(g_vpcInputDesc_, inputWidth);
    acldvppSetPicDescHeight(g_vpcInputDesc_, inputHeight);
    acldvppSetPicDescWidthStride(g_vpcInputDesc_, alignWidthStride);
    acldvppSetPicDescHeightStride(g_vpcInputDesc_, alignHeightStride);
    acldvppSetPicDescSize(g_vpcInputDesc_, inputBufferSize);
    return 1;
}

int DvppResize::InitResizeOutputDesc()
{
    int resizeOutWidth = g_resizeWidth_;
    int resizeOutHeight = g_resizeHeight_;
    int resizeOutWidthStride = ALIGN_UP16(resizeOutWidth) * 3;
    int resizeOutHeightStride = ALIGN_UP2(resizeOutHeight);
    if (resizeOutWidthStride == 0 || resizeOutHeightStride == 0)
    {
        AIALG_ERROR("InitResizeOutputDesc AlignmentHelper failed\n");
        return 0;
    }

    g_vpcOutBufferSize_ = resizeOutWidthStride * resizeOutHeightStride;//YUV420SP_SIZE(resizeOutWidthStride, resizeOutHeightStride);
    aclError aclRet = acldvppMalloc(&g_vpcOutBufferDev_, g_vpcOutBufferSize_);
    if (aclRet != ACL_SUCCESS)
    {
        AIALG_ERROR("acldvppMalloc g_vpcOutBufferDev_ failed, aclRet = %d\n", aclRet);
        return 0;
    }

    g_vpcOutputDesc_ = acldvppCreatePicDesc();
    if (!g_vpcOutputDesc_)
    {
        AIALG_ERROR("acldvppCreatePicDesc g_vpcOutputDesc_ failed\n");
        return 0;
    }

    acldvppSetPicDescData(g_vpcOutputDesc_, g_vpcOutBufferDev_);
    acldvppSetPicDescFormat(g_vpcOutputDesc_, g_format_);
    acldvppSetPicDescWidth(g_vpcOutputDesc_, resizeOutWidth);
    acldvppSetPicDescHeight(g_vpcOutputDesc_, resizeOutHeight);
    acldvppSetPicDescWidthStride(g_vpcOutputDesc_, resizeOutWidthStride);
    acldvppSetPicDescHeightStride(g_vpcOutputDesc_, resizeOutHeightStride);
    acldvppSetPicDescSize(g_vpcOutputDesc_, g_vpcOutBufferSize_);

    return 1;
}

int DvppResize::Process(ImageData &resizedImage, ImageData &srcImage)
{
    acldvppSetPicDescData(g_vpcInputDesc_, srcImage.data);
    if(src_width_ != srcImage.width || src_height_ != srcImage.height)
    {
        src_width_ = srcImage.width;
        src_height_ = srcImage.height;
        InitResizeInputDesc(srcImage);
    }
    // resize pic
    aclError aclRet = acldvppVpcResizeAsync(g_dvppChannelDesc_, g_vpcInputDesc_,
                                            g_vpcOutputDesc_, g_resizeConfig_, stream_);
    if (aclRet != ACL_SUCCESS)
    {
        AIALG_ERROR("acldvppVpcResizeAsync failed, aclRet = %d\n", aclRet);
        return 0;
    }

    aclRet = aclrtSynchronizeStream(stream_);
    if (aclRet != ACL_SUCCESS)
    {
        AIALG_ERROR("resize aclrtSynchronizeStream failed, aclRet = %d\n", aclRet);
        return 0;
    }

    resizedImage.width = g_resizeWidth_;
    resizedImage.height = g_resizeHeight_;
    resizedImage.alignWidth = ALIGN_UP16(g_resizeWidth_) * 3;
    resizedImage.alignHeight = ALIGN_UP2(g_resizeHeight_);
    resizedImage.size = g_vpcOutBufferSize_;
    resizedImage.data = reinterpret_cast<uint8_t *>(g_vpcOutBufferDev_);
    return 1;
}
