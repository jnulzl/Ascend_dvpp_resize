#include <iostream>
#include <chrono>
#include "dvpp_resize.h"
#include "opencv2/opencv.hpp"

void swapYUV_I420toNV12(unsigned char *i420bytes, unsigned char *nv12bytes, int width, int height)
{
    int nLenY = width * height;
    int nLenU = nLenY / 4;

    memcpy(nv12bytes, i420bytes, width * height);

    for (int i = 0; i < nLenU; i++)
    {
        nv12bytes[nLenY + 2 * i] = i420bytes[nLenY + i];                    // U
        nv12bytes[nLenY + 2 * i + 1] = i420bytes[nLenY + nLenU + i];        // V
    }
}

void BGR2YUV_nv12(cv::Mat src, cv::Mat &dst)
{
    int w_img = src.cols;
    int h_img = src.rows;
    dst = cv::Mat(h_img * 1.5, w_img, CV_8UC1, cv::Scalar(0));
    cv::Mat src_YUV_I420(h_img * 1.5, w_img, CV_8UC1, cv::Scalar(0));  //YUV_I420
    cvtColor(src, src_YUV_I420, cv::COLOR_BGR2YUV_I420);
    swapYUV_I420toNV12(src_YUV_I420.data, dst.data, w_img, h_img);
}


int main(int argc, const char *argv[])
{
    if (argc != 3)
    {
        std::cout << "Usage: ./main <image_path> <num_loop>" << std::endl;
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

    DvppResize dvppResize(stream, 640, 360);

    std::string img_path = argv[1];
    uint32_t num_loop = std::atoi(argv[2]);

    cv::Mat img = cv::imread(img_path, 1);
    cv::Mat yuv420_sp;
    BGR2YUV_nv12(img, yuv420_sp);

//    cv::Mat saveImg;
//    cv::cvtColor(yuv420_sp, saveImg, cv::COLOR_YUV2BGR_NV12);
//    cv::imwrite("./input.jpg", saveImg);

    // if the input yuv is from JPEGD, 128*16 alignment on 310, 64*16 alignment on 310P
    // if the input yuv is from VDEC, it shoud be aligned to 16*2
    // alloc device memory && copy data from host to device
    ImageData src_img;
    src_img.width = img.cols; // 1920
    src_img.height = img.rows; // 1080
//    src_img.alignWidth = ALIGN_UP128(img.cols); // 1920
//    src_img.alignHeight = ALIGN_UP16(img.rows); // 1088
    src_img.alignWidth = ALIGN_UP16(img.cols) * 3; // 1920
    src_img.alignHeight = ALIGN_UP2(img.rows); // 1080
    src_img.size = src_img.alignWidth * src_img.alignHeight;//YUV420SP_SIZE(src_img.alignWidth, src_img.alignHeight);
    std::printf("img.width = %d, img.height = %d\n", img.cols, img.rows);
    std::printf("img.alignWidth = %d, img.alignHeight = %d\n", src_img.alignWidth, src_img.alignHeight);
    std::printf("img.size = %u\n", src_img.size);

    void *src_buffer = nullptr;
    aclError aclRet = acldvppMalloc(&src_buffer, src_img.size);
    if (aclRet != ACL_SUCCESS)
    {
        std::printf("malloc device data buffer failed, aclRet is %d\n", aclRet);
        return -1;
    }

    aclRet = aclrtMemcpy(src_buffer, src_img.size, img.data, src_img.size, ACL_MEMCPY_HOST_TO_DEVICE);
    if (aclRet != ACL_SUCCESS)
    {
        std::printf("Copy data to device failed, aclRet is %d\n", aclRet);
        acldvppFree(src_buffer);
        return -1;
    }

    src_img.data = static_cast<uint8_t*>(src_buffer);

    ImageData des_img;
    std::chrono::time_point<std::chrono::system_clock> startTP = std::chrono::system_clock::now();
    for (int idx = 0; idx < num_loop; ++idx)
    {
        dvppResize.Process(des_img, src_img);
    }
    std::chrono::time_point<std::chrono::system_clock> finishTP1 = std::chrono::system_clock::now();
    std::printf("Dvpp %d resize time = %ld us\n", num_loop, std::chrono::duration_cast<std::chrono::microseconds>(finishTP1 - startTP).count());
    std::printf("Dvpp average resize time = %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(finishTP1 - startTP).count() / num_loop);

    std::printf("des_width = %d, des_height = %d\n", des_img.width, des_img.height);
    std::printf("des_aligned_width = %d, des_aligned_height = %d\n", des_img.alignWidth, des_img.alignHeight);
    std::printf("des_size = %d\n", des_img.size);

    // alloc device memory && copy data from device to host
    cv::Mat des_mat(des_img.height, des_img.width, CV_8UC3);
    aclRet = aclrtMemcpy(des_mat.data, des_img.size, des_img.data, des_img.size, ACL_MEMCPY_DEVICE_TO_HOST);
    if (aclRet != ACL_SUCCESS)
    {
        std::printf("Copy data to host failed, aclRet is %d\n", aclRet);
        return -1;
    }

    cv::imwrite("./output.jpg", des_mat);

    if(src_buffer)
    {
        acldvppFree(src_buffer);
        src_buffer = nullptr;
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
