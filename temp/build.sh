#!/bin/bash

echo "清理之前的构建文件..."
rm -rf build

echo "创建构建目录..."
mkdir build
cd build

echo "运行CMake配置..."
cmake ..

if [ $? -eq 0 ]; then
    echo "CMake配置成功！"
    echo "开始编译..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
    
    if [ $? -eq 0 ]; then
        echo "编译成功！"
        echo "可执行文件位置: $(pwd)/ffmpegdemo"
    else
        echo "编译失败！"
        exit 1
    fi
else
    echo "CMake配置失败！"
    exit 1
fi
