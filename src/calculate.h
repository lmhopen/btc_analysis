/**
 * @file calculate.h
 * @brief 数学计算模块 - 线性回归、标准差、通道线计算
 * 
 * 该模块负责：
 * - 对数线性回归计算
 * - 标准差计算
 * - 通道线计算（9条线）
 * - 偏离度计算
 */

#ifndef CALCULATE_H
#define CALCULATE_H

#include "data.h"
#include <math.h>

/* ========== 常量定义 ========== */
#define SIGMA_0618 0.618
#define SIGMA_1_0  1.0
#define SIGMA_1_5  1.5
#define SIGMA_2_0  2.0

/* ========== 数据结构 ========== */

/**
 * @brief 回归通道数据
 */
typedef struct {
    // 回归参数
    double slope;               // 斜率 (对数空间)
    double intercept;           // 截距 (对数空间)
    double std_dev;             // 标准差 (对数空间)
    
    // 通道线数组（动态分配）
    double* center_line;        // 中心线
    double* upper_0618;         // +0.618σ
    double* lower_0618;         // -0.618σ
    double* upper_1_0;          // +1.0σ
    double* lower_1_0;          // -1.0σ
    double* upper_1_5;          // +1.5σ
    double* lower_1_5;          // -1.5σ
    double* upper_2_0;          // +2.0σ
    double* lower_2_0;          // -2.0σ
    
    // OHLC4数组
    double* ohlc4;              // OHLC4值
    double* log_ohlc4;          // 对数OHLC4值
    
    // 偏离度数组
    double* deviation;          // 偏离度（标准差倍数）
    
    int count;                  // 数据点数量
} RegressionChannel;

/* ========== 函数声明 ========== */

/**
 * @brief 创建回归通道结构
 * @param count 数据点数量
 * @return 回归通道指针
 */
RegressionChannel* channel_create(int count);

/**
 * @brief 释放回归通道结构
 * @param channel 回归通道指针
 */
void channel_free(RegressionChannel* channel);

/**
 * @brief 计算对数线性回归通道
 * @param dataset 数据集
 * @return 回归通道指针
 */
RegressionChannel* channel_calculate_log(const Dataset* dataset);

/**
 * @brief 计算线性回归通道（通达信方式）
 * @param dataset 数据集
 * @return 回归通道指针
 */
RegressionChannel* channel_calculate_linear(const Dataset* dataset);

/**
 * @brief 获取指定索引的偏离度描述
 * @param channel 回归通道
 * @param index 数据索引
 * @return 偏离度描述字符串
 */
const char* channel_get_deviation_label(const RegressionChannel* channel, int index);

/**
 * @brief 打印回归参数
 * @param channel 回归通道指针
 */
void channel_print_params(const RegressionChannel* channel);

/**
 * @brief 打印最后一个数据点的通道值
 * @param channel 回归通道指针
 * @param dataset 数据集
 */
void channel_print_last_values(const RegressionChannel* channel, const Dataset* dataset);

/* ========== 数学辅助函数 ========== */

/**
 * @brief 计算数组平均值
 * @param arr 数组
 * @param count 元素数量
 * @return 平均值
 */
double math_mean(const double* arr, int count);

/**
 * @brief 计算数组标准差（总体标准差）
 * @param arr 数组
 * @param count 元素数量
 * @return 标准差
 */
double math_std(const double* arr, int count);

/**
 * @brief 计算线性回归斜率（最小二乘法）
 * @param x X数组
 * @param y Y数组
 * @param count 元素数量
 * @param intercept 输出截距
 * @return 斜率
 */
double math_linear_regression(const double* x, const double* y, int count, double* intercept);

/**
 * @brief 安全的对数计算（避免log(0)）
 * @param x 输入值
 * @return 对数值
 */
double math_safe_log(double x);

/**
 * @brief 安全的指数计算
 * @param x 输入值
 * @return 指数值
 */
double math_safe_exp(double x);

#endif /* CALCULATE_H */
