/**
 * @file chart.h
 * @brief 图形绘制模块 - 使用raylib绘制K线图和通道线
 * 
 * 该模块负责：
 * - 窗口初始化和管理
 * - K线图绘制
 * - 通道线绘制
 * - 坐标轴和标签
 * - 对数坐标转换
 * - 键盘缩放（方向键上/下控制显示数据条数）
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

/* 缩放: 最少显示5条数据, 每次按键增减10条 */
#define MIN_VIEW_COUNT 5
#define ZOOM_STEP 10

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
#define COLOR_MVRV       ((Color){ 255, 165, 0, 255 })     // orange
#define COLOR_CLOSE_MINUS_CVDD ((Color){ 0, 255, 200, 255 }) // teal

/* ========== 数据结构 ========== */

/**
 * @brief 图表坐标转换器
 */
typedef struct {
    // 主图区域
    Rectangle main_rect;
    // 副图区域（偏离度）
    Rectangle sub_rect;
    // MVRV Z-Score 副图
    Rectangle mvrv_rect;
    // Close-minus-CVDD 副图
    Rectangle cmcvdd_rect;
    
    // 数据范围
    int data_count;             // 显示的数据条数（= view_count）
    int view_start;             // 在全量数据中的起始索引
    double price_min;
    double price_max;
    double dev_min;
    double dev_max;
    
    // MVRV 数据范围
    double mvrv_min;
    double mvrv_max;
    // Close-minus-CVDD 数据范围
    double cmcvdd_min;
    double cmcvdd_max;
    
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
    int view_count;             // 当前显示的数据条数（从最新开始）
    
    // 字体
    Font font;
    int font_size;
    
    // 周期切换
    PeriodType current_period;  // 当前显示周期
    Rectangle btn_daily;        // 日线按钮区域
    Rectangle btn_monthly;      // 月线按钮区域
    Rectangle btn_quarterly;    // 季线按钮区域
    int period_switch_request;  // 请求切换的周期（-1表示无请求）
    
    // 指标数据（由外部加载，图表只引用不释放）
    IndicatorSeries* mvrv;         // MVRV Z-Score
    IndicatorSeries* cmcvdd;       // Close-minus-CVDD
} ChartState;

/* ========== 函数声明 ========== */

ChartState* chart_init(void);
void chart_close(ChartState* state);

ChartCoords chart_calc_coords(const Dataset* dataset, 
                               const RegressionChannel* channel,
                               bool use_log,
                               const IndicatorSeries* mvrv,
                               const IndicatorSeries* cmcvdd,
                               int view_count);

void chart_draw(ChartState* state, 
                const Dataset* dataset,
                const RegressionChannel* channel,
                const ChartCoords* coords);

void chart_draw_candles(const Dataset* dataset,
                        const ChartCoords* coords,
                        int start_index, int count);

void chart_draw_channel_lines(const RegressionChannel* channel,
                               const ChartCoords* coords,
                               int start_index, int count);

void chart_draw_sub_chart(const RegressionChannel* channel,
                          const ChartCoords* coords,
                          int start_index, int count);

void chart_draw_axes(const Dataset* dataset, const ChartCoords* coords);

void chart_draw_title_info(ChartState* state,
                           const Dataset* dataset,
                           const RegressionChannel* channel,
                           const ChartCoords* coords);

void chart_draw_period_buttons(ChartState* state);

int chart_handle_period_click(ChartState* state, Vector2 mouse_pos);

void chart_zoom_in(ChartState* state);
void chart_zoom_out(ChartState* state);

void chart_draw_mvrv_subchart(const ChartCoords* coords,
                              const IndicatorSeries* mvrv,
                              const Dataset* dataset,
                              int start_index, int count);

void chart_draw_cmcvdd_subchart(const ChartCoords* coords,
                                const IndicatorSeries* cmcvdd,
                                const Dataset* dataset,
                                int start_index, int count);

/* ========== 坐标转换函数 ========== */

float chart_index_to_x(int index, const ChartCoords* coords);
float chart_price_to_y(double price, const ChartCoords* coords);
float chart_deviation_to_y(double deviation, const ChartCoords* coords);
int chart_x_to_index(float x, const ChartCoords* coords);
double chart_y_to_price(float y, const ChartCoords* coords);

/* ========== 辅助绘制函数 ========== */

void chart_draw_grid(const ChartCoords* coords);
void chart_draw_rect(float x, float y, float w, float h, Color color, float alpha);
void chart_draw_text_box(const char* text, int x, int y, int font_size, 
                         Color text_color, Color bg_color);

bool chart_save_to_file(const Dataset* dataset,
                        const RegressionChannel* channel,
                        bool use_log,
                        const char* filename,
                        const IndicatorSeries* mvrv,
                        const IndicatorSeries* cmcvdd,
                        int view_count);

#endif /* CHART_H */
