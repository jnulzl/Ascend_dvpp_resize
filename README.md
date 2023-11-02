# 华为Ascend310P3芯片图像等比例缩放

## 依赖软件

- aarch64 版本的OpenCV

- g++ 或 aarch64-target-linux-gnu-g++

- 带有Ascend310P3的华为服务器

- [CANN社区版](https://www.hiascend.com/zh/software/cann/community)

- [CANN社区版文档](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/70RC1alpha003/overview/index.html)

参考CANN官方文档配置完环境(安装`Ascend-cann-toolkit`和`Ascend-cann-amct`)后进行以下操作。

## 编译 

```shell
mkdir build
cmake ../ # .... 查看CMakeLists.txt根据实际情况修改
make VERBOSE=1 -j4
./dvpp_resize                                   
	Usage: ./main img_list_file batch_size des_width des_height num_loop
```

本仓库实现了BGR图形的等比例缩放，如下所示

- 缩放前

  ![dog](D:\codes\Ascend\Deploy\dvpp_resize\imgs\dog.jpg)


- 缩放后

  ![dvpp_resize_output0](D:\codes\Ascend\Deploy\dvpp_resize\imgs\dvpp_resize_output0.jpg)


## 说明及注意事项

### 1、关于`dvpp`的详细使用可参考[图像/视频/音频数据处理](https://www.hiascend.com/document/detail/zh/canncommercial/63RC1/inferapplicationdev/aclcppdevg/aclcppdevg_000038.html)

### 2、输入BGR/RGB的图像宽度(width)必须是16的整数倍，高度(height)必须是偶数

### 3、关于输入图像格式说明见[VPC功能说明V1](https://www.hiascend.com/document/detail/zh/canncommercial/63RC1/inferapplicationdev/aclcppdevg/aclcppdevg_03_0172.html)和[VPC功能说明V2](https://www.hiascend.com/document/detail/zh/canncommercial/63RC1/inferapplicationdev/aclcppdevg/aclcppdevg_03_0350.html)

### 4、[Ascend_samples](https://github.com/Ascend/samples)