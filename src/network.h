/**
 * @file network.h
 * @brief 数据更新模块
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "data.h"

/**
 * @brief 更新数据（调用Python脚本）
 * @param filepath CSV文件路径
 * @return 数据集指针
 */
Dataset* update_data(const char* filepath);

/**
 * @brief 从CSV指定行开始，读取前6列并导出到txt文件
 * @param csv_path CSV文件路径
 * @param output_path 输出txt文件路径
 * @param start_line 起始行号（从1开始计）
 */
void export_csv_slice(const char* csv_path, const char* output_path, int start_line);

/**
 * @brief 更新链上指标数据（MVRV Z-Score, CVDD）
 * 调用 update_indicators.py 脚本
 */
void update_indicators(void);

/**
 * @brief 计算 BTC 收盘价 - CVDD 差值
 * 调用 compute_close_minus_cvdd.py 脚本
 * 有数据更新才计算，无更新则跳过
 */
void compute_close_minus_cvdd(void);

/**
 * @brief 计算 C-CVDD 历史百分位
 * 调用 compute_cmcvdd_percentile.py 脚本
 * 有数据更新才计算，无更新则跳过
 */
void compute_cmcvdd_percentile(void);

#endif /* NETWORK_H */
