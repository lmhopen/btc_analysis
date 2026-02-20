/**
 * @file chart.h
 * @brief 图形绑制模块 - 使用raylib绑制K线图和通道线
 * 
 * 该模块负责：
 * - 窗口初始化和管理
 * - K线图绑制
 * - 通道线绑制
 * - 坐标轴和标签
 * - 对数坐标转换
 */

#ifndef CHART_H
#define CHART_H

#include "data.h"
#include "calculate.h"
#include "raylib.h"

/* ========== 常量定义 ========== */
#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900
#define CHART_MARGIN_LEFT 100
#define CHART_MARGIN_RIGHT 140
#define CHART_MARGIN_TOP 80
#define CHART_MARGIN_BOTTOM 80
#define SUB_CHART_HEIGHT 200
#define SUB_CHART_GAP 50

/* ========== 颜色定义 ========== */
#define COLOR_BG         ((Color){ 20, 20, 30, 255 })
#define COLOR_GRID       ((Color){ 50, 50, 60, 100 })
#define COLOR_TEXT       ((Color){ 200, 200, 200, 255 })
#define COLOR_TEXT_HIGHLIGHT ((Color){ 255, 255, 100, 255 })
#define COLOR_CANDLE_UP  ((Color){ 220, 50, 50, 255 })
#define COLOR_CANDLE_DOWN ((Color){ 50, 180, 50, 255 })
#define COLOR_CENTER     ((Color){ 255, 100, 100, 255 })
#define COLOR_SIGMA_0618 ((Color){ 135, 206, 250, 255 })  // lightblue
#define COLOR_SIGMA_1_0  ((Color){ 0, 180, 180, 200 })
#define COLOR_SIGMA_1_5  ((Color){ 50, 205, 50, 255 })     // green
#define COLOR_SIGMA_2_0  ((Color){ 148, 0, 211, 255 })     // purple
#define COLOR_CROSSHAIR  ((Color){ 180, 180, 180, 200 })
#define COLOR_INFOBOX    ((Color){ 255, 255, 200, 240 })

/* ========== 数据结构 ========== */

/**
 * @brief 图表坐标转换器
 */
typedef struct {
    // 主图区域
    Rectangle main_rect;
    // 副图区域
    Rectangle sub_rect;
    
    // 数据范围
    int data_count;
    double price_min;
    double price_max;
    double dev_min;
    double dev_max;
    
    // 对数坐标标志
    bool use_log_scale;
} ChartCoords;

/**
 * @brief 图表状态
 */
typedef struct {
    // 显示模式
    bool show_log_scale;        // 对数坐标
    bool show_linear_scale;     // 线性坐标
    bool show_sub_chart;        // 显示副图
    
    // 视图参数
    int view_start;             // 可见区域起始索引
    int view_count;             // 可见数据数量
    
    // 字体
    Font font;
    int font_size;
} ChartState;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化图表
 * @return 图表状态指针
 */
ChartState* chart_init(void);

/**
 * @brief 关闭图表
 * @param state 图表状态指针
 */
void chart_close(ChartState* state);

/**
 * @brief 计算图表坐标
 * @param dataset 数据集
 * @param channel 回归通道
 * @param use_log 是否使用对数坐标
 * @return 坐标转换器
 */
ChartCoords chart_calc_coords(const Dataset* dataset, 
                               const RegressionChannel* channel,
                               bool use_log);

/**
 * @brief 绑制完整图表
 * @param state 图表状态
 * @param dataset 数据集
 * @param channel 回归通道
 * @param coords 坐标转换器
 */
void chart_draw(ChartState* state, 
                const Dataset* dataset,
                const RegressionChannel* channel,
                const ChartCoords* coords);

/**
 * @brief 绑制K线
 * @param canvas 绘制目标
 * @param dataset 数据集
 * @param coords 坐标转换器
 * @param start_index 起始索引
 * @param count 数量
 */
void chart_draw_candles(const Dataset* dataset,
                        const ChartCoords* coords,
                        int start_index, int count);

/**
 * @brief 绑制通道线
 * @param channel 回归通道
 * @param coords 坐标转换器
 * @param count 数量
 */
void chart_draw_channel_lines(const RegressionChannel* channel,
                               const ChartCoords* coords,
                               int count);

/**
 * @brief 绑制副图（偏离度）
 * @param channel 回归通道
 * @param coords 坐标转换器
 * @param count 数量
 */
void chart_draw_sub_chart(const RegressionChannel* channel,
                          const ChartCoords* coords,
                          int count);

/**
 * @brief 绑制坐标轴和标签
 * @param dataset 数据集
 * @param coords 坐标转换器
 */
void chart_draw_axes(const Dataset* dataset, const ChartCoords* coords);

/**
 * @brief 绘制标题和信息
 * @param state 图表状态
 * @param dataset 数据集
 * @param channel 回归通道
 */
void chart_draw_title_info(ChartState* state,
                           const Dataset* dataset,
                           const RegressionChannel* channel);

/* ========== 坐标转换函数 ========== */

/**
 * @brief 数据索引转X坐标
 * @param index 数据索引
 * @param coords 坐标转换器
 * @return X坐标（像素）
 */
float chart_index_to_x(int index, const ChartCoords* coords);

/**
 * @brief 价格转Y坐标（主图）
 * @param price 价格
 * @param coords 坐标转换器
 * @return Y坐标（像素）
 */
float chart_price_to_y(double price, const ChartCoords* coords);

/**
 * @brief 偏离度转Y坐标（副图）
 * @param deviation 偏离度
 * @param coords 坐标转换器
 * @return Y坐标（像素）
 */
float chart_deviation_to_y(double deviation, const ChartCoords* coords);

/**
 * @brief X坐标转数据索引
 * @param x X坐标（像素）
 * @param coords 坐标转换器
 * @return 数据索引
 */
int chart_x_to_index(float x, const ChartCoords* coords);

/**
 * @brief Y坐标转价格
 * @param y Y坐标（像素）
 * @param coords 坐标转换器
 * @return 价格
 */
double chart_y_to_price(float y, const ChartCoords* coords);

/* ========== 辅助绘制函数 ========== */

/**
 * @brief 绘制网格
 * @param coords 坐标转换器
 */
void chart_draw_grid(const ChartCoords* coords);

/**
 * @brief 绘制矩形
 */
void chart_draw_rect(float x, float y, float w, float h, Color color, float alpha);

/**
 * @brief 绘制文本（带背景）
 */
void chart_draw_text_box(const char* text, int x, int y, int font_size, 
                         Color text_color, Color bg_color);

/**
 * @brief 保存图表为PNG图片（使用RenderTexture避免DPI问题）
 * @param dataset 数据集
 * @param channel 回归通道
 * @param use_log 是否使用对数坐标
 * @param filename 输出文件名
 * @return 成功返回true
 */
bool chart_save_to_file(const Dataset* dataset,
                        const RegressionChannel* channel,
                        bool use_log,
                        const char* filename);

#endif /* CHART_H */
