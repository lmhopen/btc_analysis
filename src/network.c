/**
 * @file network.c
 * @brief 数据更新模块 - 调用Python脚本从Binance API获取数据
 */

#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 公共函数实现 ========== */

Dataset* update_data(const char* filepath) {
    printf("\n============================================================\n");
    printf("数据更新\n");
    printf("============================================================\n");
    
    int result = system("python data\\update_data.py");
    
    if (result == 0) {
        printf("数据更新成功!\n");
    } else {
        printf("数据更新失败 (返回码: %d)\n", result);
        printf("请确保已安装: pip install pandas requests\n");
    }
    
    printf("============================================================\n\n");
    
    Dataset* dataset = dataset_load_csv(filepath);
    
    if (dataset) {
        printf("加载数据: %d 条记录\n", dataset->count);
        if (dataset->count > 0) {
            printf("最新日期: %s\n", dataset->candles[dataset->count-1].date);
            printf("最新收盘价: $%.2f\n", dataset->candles[dataset->count-1].close);
        }
    }
    
    return dataset;
}
