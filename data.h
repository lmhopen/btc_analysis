/**
 * @file data.h
 * @brief 比特币数据结构和CSV解析模块
 * 
 * 该模块负责：
 * - 定义K线数据结构
 * - CSV文件解析
 * - 数据集管理
 */

#ifndef DATA_H
#define DATA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ========== 常量定义 ========== */
#define MAX_DATE_LEN 11
#define MAX_LINE_LEN 1024
#define INITIAL_CAPACITY 200
#define MAX_PATH_LEN 512

/* ========== 数据结构 ========== */

/**
 * @brief 单条K线数据
 */
typedef struct {
    char date[MAX_DATE_LEN];    // 日期 "YYYY-MM-DD"
    int year;                   // 年份
    int month;                  // 月份
    double open;                // 开盘价
    double high;                // 最高价
    double low;                 // 最低价
    double close;               // 收盘价
    double volume;              // 成交量
    double quote_volume;        // 成交额
} Candle;

/**
 * @brief 数据集
 */
typedef struct {
    Candle* candles;            // K线数组
    int count;                  // 数据条数
    int capacity;               // 数组容量
} Dataset;

/**
 * @brief 日期范围筛选器
 */
typedef struct {
    int start_year;             // 起始年份
    int start_month;            // 起始月份
    int end_year;               // 结束年份
    int end_month;              // 结束月份
} DateFilter;

/* ========== 函数声明 ========== */

/**
 * @brief 创建空数据集
 * @return 新数据集指针
 */
Dataset* dataset_create(void);

/**
 * @brief 释放数据集
 * @param dataset 数据集指针
 */
void dataset_free(Dataset* dataset);

/**
 * @brief 从CSV文件加载数据
 * @param filepath CSV文件路径
 * @return 数据集指针，失败返回NULL
 */
Dataset* dataset_load_csv(const char* filepath);

/**
 * @brief 按日期范围筛选数据
 * @param src 源数据集
 * @param filter 日期筛选器
 * @return 新数据集指针
 */
Dataset* dataset_filter_by_date(const Dataset* src, const DateFilter* filter);

/**
 * @brief 按起始年份筛选数据
 * @param src 源数据集
 * @param start_year 起始年份
 * @return 新数据集指针
 */
Dataset* dataset_filter_from_year(const Dataset* src, int start_year);

/**
 * @brief 打印数据集信息
 * @param dataset 数据集指针
 */
void dataset_print_info(const Dataset* dataset);

/**
 * @brief 解析日期字符串
 * @param date_str 日期字符串 "YYYY-MM-DD"
 * @param year 年份输出
 * @param month 月份输出
 * @return 成功返回0
 */
int parse_date(const char* date_str, int* year, int* month);

/**
 * @brief 获取数据集时间范围
 * @param dataset 数据集指针
 * @param start_year 起始年份输出
 * @param end_year 结束年份输出
 */
void dataset_get_date_range(const Dataset* dataset, int* start_year, int* end_year);

/**
 * @brief 获取最高价
 * @param dataset 数据集指针
 * @return 最高价
 */
double dataset_get_highest(const Dataset* dataset);

/**
 * @brief 获取最低价
 * @param dataset 数据集指针
 * @return 最低价
 */
double dataset_get_lowest(const Dataset* dataset);

#endif /* DATA_H */
