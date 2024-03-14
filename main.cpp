#include <iostream>
#include <chrono>
#include <random>
#include "opencv2/opencv.hpp"

#include "common/utils/file_process.hpp"
#include "dvpp_resize.h"

cv::Mat BGR2YUV_NV12(const cv::Mat &src)
{
    auto src_h = src.rows;
    auto src_w = src.cols;
    cv::Mat dst(src_h * 1.5, src_w, CV_8UC1);
    cv::cvtColor(src, dst, cv::COLOR_BGR2YUV_I420);  // I420: YYYY...UU...VV...

    auto n_y = src_h * src_w;
    auto n_uv = n_y / 2;
    auto n_u = n_y / 4;
    std::vector<uint8_t> uv(n_uv);
    std::copy(dst.data+n_y, dst.data+n_y+n_uv, uv.data());
    for (auto i = 0; i < n_u; i++) {
        dst.data[n_y + 2*i] = uv[i];            // U
        dst.data[n_y + 2*i + 1] = uv[n_u + i];  // V
    }
    return dst;
}

int get_random(int min, int max)
{
    std::random_device seed;//硬件生成随机数种子
    std::ranlux48 engine(seed());//利用种子生成随机数引擎
    std::uniform_int_distribution<> distrib(min, max);//设置随机数范围，并为均匀分布
    return distrib(engine);//随机数
}

int main(int argc, const char *argv[])
{
    if (argc != 9)
    {
        std::cout << "Usage: ./main img_list_file batch_size des_width des_height num_loop yuv420sp_nv12_resize fix_scale crop_size" << std::endl;
        return -1;
    }
    int32_t deviceId;
    aclrtContext context;
    aclrtStream stream;
    aclrtRunMode run_mode;

    aclError ret = aclInit(nullptr);
    if (ret != ACL_SUCCESS)
    {
        std::printf("acl init failed\n");
        return -1;
    }
    std::printf("acl init success\n");

    // open device
    ret = aclrtSetDevice(deviceId);
    if (ret != ACL_SUCCESS)
    {
        std::printf("acl open device %d failed\n", deviceId);
        return -1;
    }

    std::printf("open device %d success\n", deviceId);
    // create context (set current)
    ret = aclrtCreateContext(&context, deviceId);
    if (ret != ACL_SUCCESS)
    {
        std::printf("acl create context failed\n");
        return -1;
    }
    std::printf("create context success\n");

    // create stream
    ret = aclrtCreateStream(&stream);
    if (ret != ACL_SUCCESS)
    {
        std::printf("acl create stream failed\n");
        return -1;
    }
    std::printf("create stream success\n");

    ret = aclrtGetRunMode(&run_mode);
    if (ret != ACL_SUCCESS) {
        std::printf("acl get run mode failed\n");
        return -1;
    }

    std::vector<std::string> img_list;
    alg_utils::get_all_line_from_txt(argv[1], img_list);
    int batch_size = std::atoi(argv[2]);
    int des_width = std::atoi(argv[3]);
    int des_height = std::atoi(argv[4]);

    int yuv420sp_nv12_resize = std::atoi(argv[6]);
    int fix_scale = std::atoi(argv[7]);
    int crop_size = std::atoi(argv[8]);

    DvppResize dvppResize;
    if(!dvppResize.HasInit())
    {
        DVPPResizeInitConfig dvppResizeInitConfig;
        dvppResizeInitConfig.context = context;
        dvppResizeInitConfig.stream = stream;
        dvppResizeInitConfig.input_format = 1 == yuv420sp_nv12_resize ? 1 : 13;
        dvppResizeInitConfig.batch_size = batch_size;
        dvppResizeInitConfig.resized_width = des_width;
        dvppResizeInitConfig.resized_height = des_height;
        dvppResizeInitConfig.is_fix_scale_resize = fix_scale;
        dvppResizeInitConfig.is_symmetry_padding = 0;
        dvppResizeInitConfig.resize_scale_factor = 1.0f;
        dvppResize.Init(&dvppResizeInitConfig);
    }

    std::vector<void*> src_buffers(batch_size);
    std::vector<DVPPImageData> src_imgs(batch_size);
    uint32_t num_loop = std::atoi(argv[5]);
    std::vector<RectInt> rects;
    rects.resize(batch_size);

    for (int idx = 0; idx < batch_size; ++idx)
    {
        // if the input yuv is from JPEGD, 128*16 alignment on 310, 64*16 alignment on 310P
        // if the input yuv is from VDEC, it shoud be aligned to 16*2
        // alloc device memory && copy data from host to device
        cv::Mat tmp = cv::imread(img_list[idx], cv::IMREAD_COLOR);
        int img_height = tmp.rows;
        int img_width = tmp.cols;
        cv::Mat img;
        cv::resize(tmp, img, {ALIGN_UP16(img_width),ALIGN_UP2(img_height)});
        cv::Mat img_new;
        if(0 == yuv420sp_nv12_resize)
        {
            std::cout << img.cols << " " << img.rows << std::endl;
            img_new = img.clone();
            src_imgs[idx].width = img_new.cols; // 1920
            src_imgs[idx].height = img_new.rows; // 1080
//            src_img.alignWidth = ALIGN_UP128(img.cols); // 1920
//            src_img.alignHeight = ALIGN_UP16(img.rows); // 1088
            src_imgs[idx].alignWidth = ALIGN_UP16(src_imgs[idx].width) * 3; // 1920
            src_imgs[idx].alignHeight = ALIGN_UP2(src_imgs[idx].height); // 1080
            src_imgs[idx].size = src_imgs[idx].alignWidth * src_imgs[idx].alignHeight;
        }
        else
        {
            std::cout << img.cols << " " << img.rows << std::endl;
            img_new = BGR2YUV_NV12(img);
            src_imgs[idx].width = img_new.cols; // 1920
            src_imgs[idx].height = img_new.rows / 1.5; // 1080
//            src_img.alignWidth = ALIGN_UP128(img.cols); // 1920
//            src_img.alignHeight = ALIGN_UP16(img.rows); // 1088
            src_imgs[idx].alignWidth = ALIGN_UP16(src_imgs[idx].width); // 1920
            src_imgs[idx].alignHeight = ALIGN_UP2(src_imgs[idx].height); // 1080
            std::cout << src_imgs[idx].width << " " << src_imgs[idx].height << " " << src_imgs[idx].alignWidth << " " << src_imgs[idx].alignHeight << std::endl;
            src_imgs[idx].size = YUV420SP_SIZE(src_imgs[idx].alignWidth, src_imgs[idx].alignHeight);
        }
        if(crop_size <= 0)
        {
            rects[idx].xmin = 0;
            rects[idx].xmax = src_imgs[idx].width - 1;
            rects[idx].ymin = 0;
            rects[idx].ymax = src_imgs[idx].height - 1;
        }
        else
        {
            rects[idx].xmin = get_random(200, 900);
            rects[idx].xmax = rects[idx].xmin + crop_size;
            rects[idx].ymin = get_random(400, 600);
            rects[idx].ymax = rects[idx].ymin + crop_size;
        }

        aclError aclRet = acldvppMalloc(&src_buffers[idx], src_imgs[idx].size);
        if (aclRet != ACL_SUCCESS)
        {
            std::printf("malloc device data buffer failed, aclRet is %d\n", aclRet);
            return -1;
        }

        aclRet = aclrtMemcpy(src_buffers[idx], src_imgs[idx].size, img_new.data, src_imgs[idx].size, ACL_MEMCPY_HOST_TO_DEVICE);
        if (aclRet != ACL_SUCCESS)
        {
            std::printf("Copy data to device failed, aclRet is %d\n", aclRet);
            acldvppFree(src_buffers[idx]);
            return -1;
        }
        src_imgs[idx].data = static_cast<uint8_t*>(src_buffers[idx]);
    }

    std::chrono::time_point<std::chrono::system_clock> startTP = std::chrono::system_clock::now();
    for (int idx = 0; idx < num_loop; ++idx)
    {
        dvppResize.Process(src_imgs.data(), rects.data(), src_imgs.size());
    }
    std::chrono::time_point<std::chrono::system_clock> finishTP1 = std::chrono::system_clock::now();
    std::printf("Dvpp %d resize time = %ld us\n", num_loop, std::chrono::duration_cast<std::chrono::microseconds>(finishTP1 - startTP).count());
    std::printf("Dvpp average resize time = %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(finishTP1 - startTP).count() / num_loop);

    for (int idx = 0; idx < batch_size; ++idx)
    {
        DVPPImageData des_img;
        dvppResize.Get(des_img, idx);

        // alloc device memory && copy data from device to host
        std::vector<uint8_t> out_data(des_img.size);
        aclError aclRet = aclrtMemcpy(out_data.data(), des_img.size, des_img.data, des_img.size, ACL_MEMCPY_DEVICE_TO_HOST);
        if (aclRet != ACL_SUCCESS)
        {
            std::printf("Copy data to host failed, aclRet is %d\n", aclRet);
            return -1;
        }

        cv::Mat des_mat(des_img.height, des_img.width, CV_8UC3, out_data.data(), des_img.alignWidth);
        cv::imwrite("./res/dvpp_resize_output" + std::to_string(idx) + ".jpg", des_mat);
    }

    for (int idx = 0; idx < batch_size; ++idx)
    {
        if(src_buffers[idx])
        {
            acldvppFree(src_buffers[idx]);
            src_buffers[idx] = nullptr;
        }
    }

    if(dvppResize.HasInit())
    {
        dvppResize.DestroyResource();
    }

    if (stream)
    {
        ret = aclrtDestroyStream(stream);
        if (ret != ACL_SUCCESS)
        {
            std::printf("destroy stream failed\n");
        }
        stream = nullptr;
    }
    std::printf("end to destroy stream\n");

    if (context)
    {
        ret = aclrtDestroyContext(context);
        if (ret != ACL_SUCCESS)
        {
            std::printf("destroy context failed\n");
        }
        context = nullptr;
    }
    std::printf("end to destroy context\n");

    ret = aclrtResetDevice(deviceId);
    if (ret != ACL_SUCCESS)
    {
        std::printf("reset device failed\n");
    }
    std::printf("end to reset device is %d\n", deviceId);

    ret = aclFinalize();
    if (ret != ACL_SUCCESS)
    {
        std::printf("finalize acl failed\n");
    }
    std::printf("end to finalize acl\n");

    return 0;
}
