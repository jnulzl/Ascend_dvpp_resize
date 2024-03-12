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
#include "alg_define.h"

DvppResize::DvppResize()
        : stream_(nullptr), g_dvppChannelDesc_(nullptr),
          g_resizeConfig_(nullptr), g_vpcBatchInputDesc_(nullptr), g_vpcBatchOutputDesc_(nullptr),
          g_vpcOutBufferSize_(0), has_init_over_(false)
{

}

void DvppResize::Init(aclrtStream &stream, uint32_t input_format, uint32_t fix_scale_resize,
                      uint32_t batch_size, uint32_t resized_width, uint32_t resized_height)
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

    g_resizeWidth_ = resized_width;
    g_resizeHeight_ = resized_height;

    // PIXEL_FORMAT_BGR_888 : 13, PIXEL_FORMAT_YUV_SEMIPLANAR_420 : 1
    // g_format_ = static_cast<acldvppPixelFormat>(PIXEL_FORMAT_BGR_888);
    g_format_ = static_cast<acldvppPixelFormat>(input_format);

    g_resize_fix_scale_ = fix_scale_resize;

    g_batch_size_ = batch_size;
    g_vpcBatchInputDesc_ = acldvppCreateBatchPicDesc(g_batch_size_);
    if (!g_vpcBatchInputDesc_)
    {
        AIALG_ERROR("acldvppCreateBatchPicDesc g_vpcBatchInputDesc_ failed\n");
        return;
    }

    g_vpcBatchOutBufferDev_ = nullptr;
    if (1 != InitResizeOutputDesc())
    {
        AIALG_ERROR("InitResizeOutputDesc failed\n");
        return;
    }

    src_widths_.resize(g_batch_size_, 0);
    src_heights_.resize(g_batch_size_, 0);
    g_roiNums_.resize(g_batch_size_, 1);
    g_cropArea_.resize(g_batch_size_, nullptr);
    g_pasteArea_.resize(g_batch_size_,nullptr);
    has_init_over_ = true;
    AIALG_PRINT("Init success\n");
}

DvppResize::~DvppResize()
{
    AIALG_PRINT("destory success\n");
}

void DvppResize::DestroyResource()
{
    if (g_vpcBatchInputDesc_)
    {
        acldvppDestroyBatchPicDesc(g_vpcBatchInputDesc_);
        g_vpcBatchInputDesc_ = nullptr;
    }
    if (g_vpcBatchOutputDesc_)
    {
        acldvppDestroyBatchPicDesc(g_vpcBatchOutputDesc_);
        g_vpcBatchOutputDesc_ = nullptr;
    }
    if (g_vpcBatchOutBufferDev_)
    {
        acldvppFree(g_vpcBatchOutBufferDev_);
        g_vpcBatchOutBufferDev_ = nullptr;
    }
    for (int idx = 0; idx < g_batch_size_; ++idx)
    {
        if (!g_cropArea_.empty() && g_cropArea_[idx])
        {
            acldvppDestroyRoiConfig(g_cropArea_[idx]);
            g_cropArea_[idx] = nullptr;
        }
        if (!g_pasteArea_.empty() && g_pasteArea_[idx])
        {
            acldvppDestroyRoiConfig(g_pasteArea_[idx]);
            g_pasteArea_[idx] = nullptr;
        }
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

int DvppResize::InitResizeInputDesc(DVPPImageData &inputImage, int index)
{
    // if the input yuv is from JPEGD, 128*16 alignment on 310, 64*16 alignment on 310P
    // if the input yuv is from VDEC, it shoud be aligned to 16*2
//    uint32_t alignWidth = ALIGN_UP128(inputImage.width);
//    uint32_t alignHeight = ALIGN_UP16(inputImage.height);
    uint32_t alignWidthStride;
    uint32_t alignHeightStride;
    uint32_t inputBufferSize;
    if(PIXEL_FORMAT_BGR_888 == g_format_)
    {
        alignWidthStride = ALIGN_UP16(inputImage.width) * 3;
        alignHeightStride = ALIGN_UP2(inputImage.height);
        inputBufferSize = alignWidthStride * alignHeightStride;//YUV420SP_SIZE(alignWidth, alignHeight);
    }
    else
    {
        alignWidthStride = ALIGN_UP16(inputImage.width);
        alignHeightStride = ALIGN_UP2(inputImage.height);
        inputBufferSize = YUV420SP_SIZE(alignWidthStride, alignHeightStride);
    }

    uint32_t inputWidth = inputImage.width;
    uint32_t inputHeight = inputImage.height;
    uint32_t n = 2;
    if (inputImage.width % n != 0)
        inputWidth--;
    if (inputImage.height % n != 0)
        inputHeight--;

    acldvppPicDesc *vpcInputDesc = acldvppGetPicDesc(g_vpcBatchInputDesc_, index);
    acldvppSetPicDescFormat(vpcInputDesc, g_format_);
    acldvppSetPicDescWidth(vpcInputDesc, inputWidth);
    acldvppSetPicDescHeight(vpcInputDesc, inputHeight);
    acldvppSetPicDescWidthStride(vpcInputDesc, alignWidthStride);
    acldvppSetPicDescHeightStride(vpcInputDesc, alignHeightStride);
    acldvppSetPicDescSize(vpcInputDesc, inputBufferSize);
    return 1;
}

int DvppResize::InitResizeOutputDesc()
{
    g_vpcBatchOutputDesc_ = acldvppCreateBatchPicDesc(g_batch_size_);
    if (!g_vpcBatchOutputDesc_)
    {
        AIALG_ERROR("acldvppCreateBatchPicDesc g_vpcBatchOutputDesc_ failed\n");
        return 0;
    }

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

    aclError aclRet = acldvppMalloc(&g_vpcBatchOutBufferDev_, g_batch_size_ * g_vpcOutBufferSize_);
    if (aclRet != ACL_SUCCESS)
    {
        AIALG_ERROR("acldvppMalloc g_vpcOutBufferDev_ failed, aclRet = %d\n", aclRet);
        return 0;
    }
    for (int bs = 0; bs < g_batch_size_; ++bs)
    {
        acldvppPicDesc *vpcOutputDesc = acldvppGetPicDesc(g_vpcBatchOutputDesc_, bs);
        acldvppSetPicDescData(vpcOutputDesc, reinterpret_cast<uint8_t*>(g_vpcBatchOutBufferDev_) + bs * g_vpcOutBufferSize_);
        acldvppSetPicDescFormat(vpcOutputDesc, PIXEL_FORMAT_BGR_888);
        acldvppSetPicDescWidth(vpcOutputDesc, resizeOutWidth);
        acldvppSetPicDescHeight(vpcOutputDesc, resizeOutHeight);
        acldvppSetPicDescWidthStride(vpcOutputDesc, resizeOutWidthStride);
        acldvppSetPicDescHeightStride(vpcOutputDesc, resizeOutHeightStride);
        acldvppSetPicDescSize(vpcOutputDesc, g_vpcOutBufferSize_);
    }
    return 1;
}

int DvppResize::Process(DVPPImageData* srcImage, int img_num)
{
    if(img_num != g_batch_size_)
    {
        AIALG_ERROR("img_num must equal batch_size, img_num = %d, img_num = %d\n", img_num, g_batch_size_);
        return 0;
    }

    for (int idx = 0; idx < g_batch_size_; ++idx)
    {
        acldvppPicDesc *vpcInputDesc = acldvppGetPicDesc(g_vpcBatchInputDesc_, idx);
        acldvppSetPicDescData(vpcInputDesc, srcImage[idx].data);
        if (src_widths_[idx] != srcImage[idx].width || src_heights_[idx] != srcImage[idx].height)
        {
            src_widths_[idx] = srcImage[idx].width;
            src_heights_[idx] = srcImage[idx].height;

            if (!g_cropArea_.empty() && g_cropArea_[idx])
            {
                acldvppDestroyRoiConfig(g_cropArea_[idx]);
                g_cropArea_[idx] = nullptr;
            }
            g_cropArea_[idx] = acldvppCreateRoiConfig(0, src_widths_[idx] % 2 ? src_widths_[idx] - 2 : src_widths_[idx] - 1,
                                                      0, src_heights_[idx] % 2 ? src_heights_[idx] - 2 : src_heights_[idx] - 1);
            if (!g_cropArea_[idx])
            {
                AIALG_ERROR("acldvppCreateRoiConfig cropArea_ failed");
                return 0;
            }
            InitResizeInputDesc(srcImage[idx], idx);

            float r = 1.0f * std::max(g_resizeHeight_, g_resizeWidth_) / std::max(src_heights_[idx], src_widths_[idx]);
            int net_input_new_width = static_cast<int>(src_widths_[idx] * r);
            int net_input_new_height = static_cast<int>(src_heights_[idx] * r);
            if (!g_pasteArea_.empty() && g_pasteArea_[idx])
            {
                acldvppDestroyRoiConfig(g_pasteArea_[idx]);
                g_pasteArea_[idx] = nullptr;
            }

            // left offset must aligned to 16
            int x = 0;
            // x = (g_resizeWidth_ - net_input_new_width) / 2; // 左右对称补0
            x = x < 0 ? 0 : x;
            x = ALIGN_UP16(x);
            int x_max = g_resizeWidth_ - 1;
            if(0 != g_resize_fix_scale_)
            {
                x_max = x + net_input_new_width;
                x_max = x_max >  g_resizeWidth_ ? g_resizeWidth_ - 1 : x_max;
            }
            x_max = x_max % 2 ? x_max : x_max - 1;

            int y = 0;
            // y = (g_resizeHeight_ - net_input_new_height) / 2; //上下对称补0
            y = y % 2 ? y - 1 : y - 2;
            y = y < 0 ? 0 : y;
            int y_max = g_resizeHeight_ - 1;
            if(0 != g_resize_fix_scale_)
            {
                y_max = y + net_input_new_height;
                y_max = y_max >  g_resizeHeight_ ? g_resizeHeight_ - 1 : y_max;
            }
            y_max = y_max % 2 ? y_max : y_max - 1;
            g_pasteArea_[idx] = acldvppCreateRoiConfig(x, x_max,
                                                       y, y_max);
            if (!g_pasteArea_[idx])
            {
                AIALG_ERROR("acldvppCreateRoiConfig g_pasteArea_ failed");
                return 0;
            }
        }
    }
    aclError aclRet = acldvppVpcBatchCropResizePasteAsync(g_dvppChannelDesc_, g_vpcBatchInputDesc_,
                                                          g_roiNums_.data(), g_batch_size_,
                                                          g_vpcBatchOutputDesc_, g_cropArea_.data(), g_pasteArea_.data(),
                                                          g_resizeConfig_, stream_);
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
    return 1;
}

int DvppResize::Get(DVPPImageData &resizedImage, int index) const
{
    resizedImage.width = g_resizeWidth_;
    resizedImage.height = g_resizeHeight_;
    resizedImage.alignWidth = ALIGN_UP16(g_resizeWidth_) * 3;
    resizedImage.alignHeight = ALIGN_UP2(g_resizeHeight_);
    resizedImage.size = g_vpcOutBufferSize_;
    resizedImage.data = reinterpret_cast<uint8_t*>(g_vpcBatchOutBufferDev_) + index * g_vpcOutBufferSize_;
    return 1;
}

const uint8_t* DvppResize::GetOutputDevicePtr() const
{
    return static_cast<const uint8_t*>(g_vpcBatchOutBufferDev_);
}