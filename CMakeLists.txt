cmake_minimum_required(VERSION 3.25)
project(DVPP_Resize_V1)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_SYSTEM_NAME "Linux") # Linux Android
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS " -fPIC -Wno-deprecated-declarations ${CMAKE_CXX_FLAGS}")
add_definitions(-DENABLE_DVPP_INTERFACE)

message(STATUS "Operate System : " ${CMAKE_SYSTEM_NAME})
message(STATUS "Compiler ID : " ${CMAKE_CXX_COMPILER_ID})
message(STATUS "Compiler Version : " ${CMAKE_CXX_COMPILER_VERSION})

if(NOT BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON)
endif()

##############THIRD_PARTY##############
if(NOT THIRD_PARTY)
    set(THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)
endif()

##############Ascend##############
if(NOT ASCEND_TOOLKIT)
    set(ASCEND_TOOLKIT /home/jnulzl/Ascend/ascend-toolkit/6.3.RC2/aarch64-linux)
endif()

set(ASCEND_INCLUDE ${ASCEND_TOOLKIT}/include)
message(STATUS "env INC_PATH: ${ASCEND_INCLUDE}")

set(ASCEND_LIB_PATH ${ASCEND_TOOLKIT}/devlib/aarch64)
message(STATUS "env LIB_PATH: ${ASCEND_LIB_PATH}")

if(NOT OUTPUT_DIRECTORY)
    set(OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME})
endif()

if(NOT LIB_MAJOR_VERSION)
    set(LIB_MAJOR_VERSION 1)
endif()

if(NOT LIB_MINOR_VERSION)
    set(LIB_MINOR_VERSION 0)
endif()

include(cmake/cmake_utils.cmake)
string(TIMESTAMP COMPILE_TIME %Y%m%d)
#git号
get_git_hash(GIT_HASH)
message(STATUS "Latest git hash is : " ${GIT_HASH})
#git分支
get_git_branch(GIT_BRANCH)

###############OpenCV##############
if(NOT OpenCV_DIR)
    set(OpenCV_DIR ${THIRD_PARTY}/opencv455)
endif()
message(STATUS "OpenCV_DIR :  " ${OpenCV_DIR})
set(OpenCV_INCLUDE
        ${OpenCV_DIR}/include
        ${OpenCV_DIR}/include/opencv2
        ${OpenCV_DIR}/include/opencv4
        ${OpenCV_DIR}/include/opencv4/opencv2)
set(OpenCV_LIB_PATH
        ${OpenCV_DIR}/lib
        ${OpenCV_DIR}/lib64
        ${OpenCV_DIR}/lib/x86_64-linux-gnu
        ${OpenCV_DIR}/lib/aarch64-linux-gnu
        )

file(MAKE_DIRECTORY ${OUTPUT_DIRECTORY})
set(EXECUTABLE_OUTPUT_PATH ${OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${EXECUTABLE_OUTPUT_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${EXECUTABLE_OUTPUT_PATH})

set(INCLUDE_PATH ${INCLUDE_PATH}
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/common
        ${ASCEND_INCLUDE}
        ${OpenCV_INCLUDE}
        )
set(LIB_PATH ${LIB_PATH}
        ${ASCEND_LIB_PATH}
        ${OpenCV_LIB_PATH}
        )

include_directories(${INCLUDE_PATH})
link_directories(${LIB_PATH})

set(DVPP_RESIZE_LIB_NAME dvpp_resize)
set(src_all ${CMAKE_CURRENT_SOURCE_DIR}/dvpp_resize.cpp)

if (BUILD_SHARED_LIBS)
    add_library(${DVPP_RESIZE_LIB_NAME} SHARED ${src_all})
    set_target_properties(${DVPP_RESIZE_LIB_NAME} PROPERTIES VERSION ${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${GIT_HASH} SOVERSION ${LIB_MAJOR_VERSION})
else()
    set(src_all ${src_all})
    add_library(${DVPP_RESIZE_LIB_NAME} STATIC ${src_all})
endif ()
target_link_libraries(${DVPP_RESIZE_LIB_NAME}
        PRIVATE
        ascendcl
        acl_dvpp
        )

add_executable(dvpp_resize_demo main.cpp)

target_link_options(dvpp_resize_demo  PRIVATE
        #        -Wl,--no-undefined
        #        -Wl,--no-allow-shlib-undefined
        -Wl,--warn-unresolved-symbols
        )

target_link_libraries(dvpp_resize_demo
            PRIVATE
            ${DVPP_RESIZE_LIB_NAME}
            ascendcl
            acl_dvpp
            opencv_core
            opencv_imgproc
            opencv_imgcodecs
            opencv_highgui
        )