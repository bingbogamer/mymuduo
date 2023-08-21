cmake_minimum_required(VERSION 2.5)
project(mymuduo)

# mymuduo 最终编译成 so动态库（不是可执行程序，而是提供给用户调用的网络库），设置动态库的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)  # 放在源码目录的 lib目录下

# 设置调试信息  启动 C++11语言标准（旧编译器）
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 定义参与编译的 源文件
aux_source_directory(. SRC_LIST)
# 编译生成动态库 mymuduo
add_library(mymuduo SHARED ${SRC_LIST})