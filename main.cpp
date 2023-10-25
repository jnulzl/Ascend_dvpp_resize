#include <iostream>
#include <chrono>
#include "opencv2/opencv.hpp"

#include "common/utils/file_process.hpp"
#include "dvpp_resize.h"

int main(int argc, const char *argv[])
{
    if (argc != 6)
    {
        std::cout << "Usage: ./main img_list_file batch_size des_width des_height num_loop" << std::endl;
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

    DvppResize dvppResize;
    dvppResize.Init(stream, batch_size, des_width, des_height);

    std::vector<void*> src_buffers(batch_size);
    std::vector<ImageData> src_imgs(batch_size);
    uint32_t num_loop = std::atoi(argv[5]);

    for (int idx = 0; idx < batch_size; ++idx)
    {
        cv::Mat img = cv::imread(img_list[idx], 1);
        // if the input yuv is from JPEGD, 128*16 alignment on 310, 64*16 alignment on 310P
        // if the input yuv is from VDEC, it shoud be aligned to 16*2
        // alloc device memory && copy data from host to device
        src_imgs[idx].width = img.cols; // 1920
        src_imgs[idx].height = img.rows; // 1080
//    src_img.alignWidth = ALIGN_UP128(img.cols); // 1920
//    src_img.alignHeight = ALIGN_UP16(img.rows); // 1088
        src_imgs[idx].alignWidth = ALIGN_UP16(img.cols) * 3; // 1920
        src_imgs[idx].alignHeight = ALIGN_UP2(img.rows); // 1080
        src_imgs[idx].size = src_imgs[idx].alignWidth * src_imgs[idx].alignHeight;//YUV420SP_SIZE(src_img.alignWidth, src_img.alignHeight);
        size_t src_img_byte_size = img.cols * img.rows * 3;

        aclError aclRet = acldvppMalloc(&src_buffers[idx], src_imgs[idx].size);
        if (aclRet != ACL_SUCCESS)
        {
            std::printf("malloc device data buffer failed, aclRet is %d\n", aclRet);
            return -1;
        }

        aclRet = aclrtMemcpy(src_buffers[idx], src_imgs[idx].size, img.data, src_img_byte_size, ACL_MEMCPY_HOST_TO_DEVICE);
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
        dvppResize.Process(src_imgs.data(), src_imgs.size());
    }
    std::chrono::time_point<std::chrono::system_clock> finishTP1 = std::chrono::system_clock::now();
    std::printf("Dvpp %d resize time = %ld us\n", num_loop, std::chrono::duration_cast<std::chrono::microseconds>(finishTP1 - startTP).count());
    std::printf("Dvpp average resize time = %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(finishTP1 - startTP).count() / num_loop);

    for (int idx = 0; idx < batch_size; ++idx)
    {
        ImageData des_img;
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

    dvppResize.DestroyResource();

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
