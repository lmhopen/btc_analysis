/**
 * @file crosshair.h
 * @brief 十字线交互模块
 * 
 * 该模块负责：
 * - 鼠标事件处理
 * - 十字线绑制
 * - 信息框显示
 */

#ifndef CROSSHAIR_H
#define CROSSHAIR_H

#include "data.h"
#include "calculate.h"
#include "chart.h"

/* ========== 数据结构 ========== */

/**
 * @brief 十字线状态
 */
typedef struct {
    bool enabled;               // 是否启用
    int x_index;                // 当前X索引
    double y_price;             // 当前Y价格
    
    // 鼠标状态
    Vector2 mouse_pos;          // 鼠标位置
    bool in_chart;              // 鼠标是否在图表区域内
    
    // 双击检测
    double last_click_time;     // 上次点击时间
    int click_count;            // 点击计数
} Crosshair;

/**
 * @brief 信息框数据
 */
typedef struct {
    char date[20];
    char open[32];
    char high[32];
    char low[32];
    char close[32];
    char center[32];
    char upper_0618[32];
    char lower_0618[32];
    char upper_1_0[32];
    char lower_1_0[32];
    char upper_2_0[32];
    char lower_2_0[32];
    char deviation[32];
} InfoBox;

/* ========== 函数声明 ========== */

/**
 * @brief 创建十字线
 * @return 十字线指针
 */
Crosshair* crosshair_create(void);

/**
 * @brief 释放十字线
 * @param ch 十字线指针
 */
void crosshair_free(Crosshair* ch);

/**
 * @brief 更新十字线状态
 * @param ch 十字线指针
 * @param coords 坐标转换器
 */
void crosshair_update(Crosshair* ch, const ChartCoords* coords);

/**
 * @brief 绘制十字线
 * @param ch 十字线指针
 * @param coords 坐标转换器
 */
void crosshair_draw(const Crosshair* ch, const ChartCoords* coords);

/**
 * @brief 绘制信息框
 * @param ch 十字线指针
 * @param dataset 数据集
 * @param channel 回归通道
 * @param coords 坐标转换器
 */
void crosshair_draw_info(const Crosshair* ch,
                         const Dataset* dataset,
                         const RegressionChannel* channel,
                         const ChartCoords* coords);

/**
 * @brief 获取当前索引的信息框数据
 * @param index 数据索引
 * @param dataset 数据集
 * @param channel 回归通道
 * @param info 信息框输出
 */
void crosshair_get_info(int index,
                        const Dataset* dataset,
                        const RegressionChannel* channel,
                        InfoBox* info);

/**
 * @brief 处理鼠标事件
 * @param ch 十字线指针
 * @param coords 坐标转换器
 */
void crosshair_handle_mouse(Crosshair* ch, const ChartCoords* coords);

/**
 * @brief 检测双击
 * @param ch 十字线指针
 * @return 是否为双击
 */
bool crosshair_check_double_click(Crosshair* ch);

#endif /* CROSSHAIR_H */
