# 编译方法
## 导出工具链
    . ./export_toolchain.sh
## 创建并进入编译目录
    mkdir build && cd build
## 生成编译脚本
    cmake .. #Release
    或
    cmake -DCMAKE_BUILD_TYPE=Debug .. #Debug
## 编译
    cmake --build . -j
编译生成文件在 build/jlink