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

#endif /* NETWORK_H */
