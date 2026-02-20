#!/bin/bash
# 使用w64devkit工具链编译BTC分析程序

RAYLIB_PATH=/c/raylib/w64devkit/x86_64-w64-mingw32

# 创建obj目录
mkdir -p obj

echo "编译 data.c ..."
gcc -Wall -O2 -std=c99 -Isrc -I$RAYLIB_PATH/include -c src/data.c -o obj/data.o

echo "编译 calculate.c ..."
gcc -Wall -O2 -std=c99 -Isrc -I$RAYLIB_PATH/include -c src/calculate.c -o obj/calculate.o

echo "编译 chart.c ..."
gcc -Wall -O2 -std=c99 -Isrc -I$RAYLIB_PATH/include -c src/chart.c -o obj/chart.o

echo "编译 crosshair.c ..."
gcc -Wall -O2 -std=c99 -Isrc -I$RAYLIB_PATH/include -c src/crosshair.c -o obj/crosshair.o

echo "编译 main.c ..."
gcc -Wall -O2 -std=c99 -Isrc -I$RAYLIB_PATH/include -c src/main.c -o obj/main.o

echo "链接 ..."
gcc obj/data.o obj/calculate.o obj/chart.o obj/crosshair.o obj/main.o \
    -o btc_analysis.exe \
    -L$RAYLIB_PATH/lib \
    -lraylib -lopengl32 -lgdi32 -lwinmm -lm

echo ""
echo "编译完成！"
echo "运行 ./btc_analysis.exe 启动程序"
