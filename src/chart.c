/**
 * @file chart.c
 * @brief 图形绘制模块实现 - 使用raylib
 */

#include "chart.h"
#include "crosshair.h"
#include <math.h>

/* ========== 内部辅助函数 ========== */

static double log_transform(double value, bool use_log) {
    if (use_log && value > 0) {
        return log10(value);
    }
    return value;
}

static double log_inverse(double value, bool use_log) {
    if (use_log) {
        return pow(10.0, value);
    }
    return value;
}

/**
 * @brief 获取可见数据切片中的最低/最高价
 */
static void visible_price_range(const Dataset* dataset, int start, int count,
                                double* out_min, double* out_max) {
    *out_min = dataset->candles[start].low;
    *out_max = dataset->candles[start].high;
    for (int i = 1; i < count; i++) {
        const Candle* c = &dataset->candles[start + i];
        if (c->low  < *out_min) *out_min = c->low;
        if (c->high > *out_max) *out_max = c->high;
    }
}

/* ========== 坐标转换函数实现 ========== */

float chart_index_to_x(int index, const ChartCoords* coords) {
    float chart_width = coords->main_rect.width;
    float x_per_bar = chart_width / coords->data_count;
    return coords->main_rect.x + index * x_per_bar + x_per_bar / 2;
}

float chart_price_to_y(double price, const ChartCoords* coords) {
    double log_price = log_transform(price, coords->use_log_scale);
    double log_min = log_transform(coords->price_min, coords->use_log_scale);
    double log_max = log_transform(coords->price_max, coords->use_log_scale);
    
    double range = log_max - log_min;
    if (range < 0.0001) range = 0.0001;
    
    double normalized = (log_max - log_price) / range;
    return coords->main_rect.y + normalized * coords->main_rect.height;
}

float chart_deviation_to_y(double deviation, const ChartCoords* coords) {
    double range = coords->dev_max - coords->dev_min;
    if (range < 0.0001) range = 0.0001;
    
    double normalized = (coords->dev_max - deviation) / range;
    return coords->sub_rect.y + normalized * coords->sub_rect.height;
}

int chart_x_to_index(float x, const ChartCoords* coords) {
    float chart_width = coords->main_rect.width;
    float x_per_bar = chart_width / coords->data_count;
    
    int display_idx = (int)((x - coords->main_rect.x) / x_per_bar);
    
    if (display_idx < 0) display_idx = 0;
    if (display_idx >= coords->data_count) display_idx = coords->data_count - 1;
    
    return coords->view_start + display_idx;
}

double chart_y_to_price(float y, const ChartCoords* coords) {
    double log_min = log_transform(coords->price_min, coords->use_log_scale);
    double log_max = log_transform(coords->price_max, coords->use_log_scale);
    double range = log_max - log_min;
    
    double normalized = (coords->main_rect.y + coords->main_rect.height - y) / coords->main_rect.height;
    double log_price = log_min + normalized * range;
    
    return log_inverse(log_price, coords->use_log_scale);
}

/* ========== 图表函数实现 ========== */

ChartState* chart_init(void) {
    ChartState* state = (ChartState*)malloc(sizeof(ChartState));
    if (state == NULL) {
        return NULL;
    }
    
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Bitcoin Analysis");
    
    SetWindowPosition(100, 100);
    
    SetTargetFPS(60);
    
    state->show_log_scale = true;
    state->show_linear_scale = false;
    state->show_sub_chart = true;
    state->view_count = 0;
    state->font_size = 16;
    
    state->font = GetFontDefault();
    
    state->current_period = PERIOD_DAILY;
    state->period_switch_request = -1;
    state->btn_daily = (Rectangle){0,0,0,0};
    state->btn_monthly = (Rectangle){0,0,0,0};
    state->btn_quarterly = (Rectangle){0,0,0,0};
    
    return state;
}

void chart_close(ChartState* state) {
    if (state != NULL) {
        CloseWindow();
        free(state);
    }
}

void chart_zoom_in(ChartState* state) {
    state->view_count -= ZOOM_STEP;
    if (state->view_count < MIN_VIEW_COUNT) {
        state->view_count = MIN_VIEW_COUNT;
    }
}

void chart_zoom_out(ChartState* state) {
    /* view_count 上限由 main.c 根据当前数据集大小控制 */
    state->view_count += ZOOM_STEP;
}

ChartCoords chart_calc_coords(const Dataset* dataset, 
                               const RegressionChannel* channel,
                               bool use_log,
                               const IndicatorSeries* mvrv,
                               const IndicatorSeries* cmcvdd,
                               int view_count) {
    ChartCoords coords;
    (void)channel;
    
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    
    int margin_left = screen_w * 0.05;
    int margin_right = screen_w * 0.07;
    int margin_top = screen_h * 0.06;
    int margin_bottom = screen_h * 0.04;
    
    bool has_mvrv = (mvrv != NULL && mvrv->count > 0);
    bool has_cmcvdd = (cmcvdd != NULL && cmcvdd->count > 0);
    
    int sub_height = screen_h * 0.16;
    int sub_gap = screen_h * 0.02;
    
    int mvrv_height = has_mvrv ? (screen_h * 0.10) : 0;
    int mvrv_gap = has_mvrv ? (screen_h * 0.02) : 0;
    int cmcvdd_height = has_cmcvdd ? (screen_h * 0.10) : 0;
    int cmcvdd_gap = has_cmcvdd ? (screen_h * 0.02) : 0;
    
    coords.main_rect.x = margin_left;
    coords.main_rect.y = margin_top;
    coords.main_rect.width = screen_w - margin_left - margin_right;
    coords.main_rect.height = screen_h - margin_top - margin_bottom 
                              - sub_height - sub_gap
                              - mvrv_height - mvrv_gap
                              - cmcvdd_height - cmcvdd_gap;
    
    coords.sub_rect.x = margin_left;
    coords.sub_rect.y = coords.main_rect.y + coords.main_rect.height + sub_gap;
    coords.sub_rect.width = coords.main_rect.width;
    coords.sub_rect.height = sub_height;
    
    coords.mvrv_rect.x = margin_left;
    coords.mvrv_rect.y = coords.sub_rect.y + coords.sub_rect.height + mvrv_gap;
    coords.mvrv_rect.width = coords.main_rect.width;
    coords.mvrv_rect.height = mvrv_height;
    
    coords.cmcvdd_rect.x = margin_left;
    coords.cmcvdd_rect.y = coords.mvrv_rect.y + coords.mvrv_rect.height + cmcvdd_gap;
    coords.cmcvdd_rect.width = coords.main_rect.width;
    coords.cmcvdd_rect.height = cmcvdd_height;
    
    coords.data_count = view_count;
    coords.view_start = dataset->count - view_count;
    if (coords.view_start < 0) coords.view_start = 0;
    
    coords.use_log_scale = use_log;
    
    /* 根据可见数据切片计算价格范围 */
    {
        double vmin, vmax;
        visible_price_range(dataset, coords.view_start, coords.data_count, &vmin, &vmax);
        double range = vmax - vmin;
        if (range < 0.0001) range = 0.0001;
        coords.price_min = vmin - range * 0.1;
        coords.price_max = vmax + range * 0.1;
    }
    
    coords.dev_min = -3.0;
    coords.dev_max = 3.0;
    
    if (has_mvrv) {
        coords.mvrv_min = indicator_get_min(mvrv);
        coords.mvrv_max = indicator_get_max(mvrv);
        double mvrv_range = coords.mvrv_max - coords.mvrv_min;
        if (mvrv_range < 0.0001) mvrv_range = 0.0001;
        coords.mvrv_min -= mvrv_range * 0.1;
        coords.mvrv_max += mvrv_range * 0.1;
    } else { coords.mvrv_min = 0; coords.mvrv_max = 1; }
    
    if (has_cmcvdd) {
        coords.cmcvdd_min = indicator_get_min(cmcvdd);
        coords.cmcvdd_max = indicator_get_max(cmcvdd);
        double cmcvdd_range = coords.cmcvdd_max - coords.cmcvdd_min;
        if (cmcvdd_range < 0.0001) cmcvdd_range = 0.0001;
        coords.cmcvdd_min -= cmcvdd_range * 0.1;
        coords.cmcvdd_max += cmcvdd_range * 0.1;
    } else { coords.cmcvdd_min = 0; coords.cmcvdd_max = 1; }
    
    return coords;
}

void chart_draw_grid(const ChartCoords* coords) {
    int h_lines = 8;
    for (int i = 0; i <= h_lines; i++) {
        float y = coords->main_rect.y + i * coords->main_rect.height / h_lines;
        DrawLine(coords->main_rect.x, y, 
                 coords->main_rect.x + coords->main_rect.width, y,
                 COLOR_GRID);
    }
    
    int v_lines = 10;
    for (int i = 0; i <= v_lines; i++) {
        float x = coords->main_rect.x + i * coords->main_rect.width / v_lines;
        DrawLine(x, coords->main_rect.y,
                 x, coords->main_rect.y + coords->main_rect.height,
                 COLOR_GRID);
    }
    
    if (coords->sub_rect.height > 0) {
        for (int i = 0; i <= 4; i++) {
            float y = coords->sub_rect.y + i * coords->sub_rect.height / 4;
            DrawLine(coords->sub_rect.x, y,
                     coords->sub_rect.x + coords->sub_rect.width, y,
                     COLOR_GRID);
        }
    }
}

void chart_draw_candles(const Dataset* dataset,
                        const ChartCoords* coords,
                        int start_index, int count) {
    if (dataset == NULL || coords == NULL || count <= 0) {
        return;
    }
    
    BeginScissorMode((int)coords->main_rect.x, (int)coords->main_rect.y, 
                     (int)coords->main_rect.width, (int)coords->main_rect.height);
    
    float chart_width = coords->main_rect.width;
    float x_per_bar = chart_width / count;
    float bar_width = x_per_bar * 0.7;
    if (bar_width < 2) bar_width = 2;
    if (bar_width > 15) bar_width = 15;
    
    for (int i = 0; i < count && (start_index + i) < dataset->count; i++) {
        int idx = start_index + i;
        const Candle* c = &dataset->candles[idx];
        
        float x = chart_index_to_x(i, coords);
        
        float y_open = chart_price_to_y(c->open, coords);
        float y_close = chart_price_to_y(c->close, coords);
        float y_high = chart_price_to_y(c->high, coords);
        float y_low = chart_price_to_y(c->low, coords);
        
        Color color = (c->close >= c->open) ? COLOR_CANDLE_UP : COLOR_CANDLE_DOWN;
        
        DrawLine(x, y_high, x, y_low, color);
        
        float body_top = (y_open < y_close) ? y_open : y_close;
        float body_height = fabs(y_close - y_open);
        if (body_height < 1) body_height = 1;
        
        Rectangle body = { x - bar_width/2, body_top, bar_width, body_height };
        DrawRectangleRec(body, color);
    }
    
    EndScissorMode();
}

void chart_draw_channel_lines(const RegressionChannel* channel,
                               const ChartCoords* coords,
                               int start_index, int count) {
    if (channel == NULL || coords == NULL || count <= 0) {
        return;
    }
    
    BeginScissorMode((int)coords->main_rect.x, (int)coords->main_rect.y, 
                     (int)coords->main_rect.width, (int)coords->main_rect.height);
    
    /* 中心线 */
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        float y1 = chart_price_to_y(channel->center_line[start_index + i - 1], coords);
        float y2 = chart_price_to_y(channel->center_line[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_CENTER);
    }
    
    /* +/-0.618s */
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        float y1 = chart_price_to_y(channel->upper_0618[start_index + i - 1], coords);
        float y2 = chart_price_to_y(channel->upper_0618[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_0618);
        y1 = chart_price_to_y(channel->lower_0618[start_index + i - 1], coords);
        y2 = chart_price_to_y(channel->lower_0618[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_0618);
    }
    
    /* +/-1.0s */
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        float y1 = chart_price_to_y(channel->upper_1_0[start_index + i - 1], coords);
        float y2 = chart_price_to_y(channel->upper_1_0[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_1_0);
        y1 = chart_price_to_y(channel->lower_1_0[start_index + i - 1], coords);
        y2 = chart_price_to_y(channel->lower_1_0[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_1_0);
    }
    
    /* +/-1.5s */
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        float y1 = chart_price_to_y(channel->upper_1_5[start_index + i - 1], coords);
        float y2 = chart_price_to_y(channel->upper_1_5[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_1_5);
        y1 = chart_price_to_y(channel->lower_1_5[start_index + i - 1], coords);
        y2 = chart_price_to_y(channel->lower_1_5[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_1_5);
    }
    
    /* +/-2.0s */
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        float y1 = chart_price_to_y(channel->upper_2_0[start_index + i - 1], coords);
        float y2 = chart_price_to_y(channel->upper_2_0[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_2_0);
        y1 = chart_price_to_y(channel->lower_2_0[start_index + i - 1], coords);
        y2 = chart_price_to_y(channel->lower_2_0[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_2_0);
    }
    
    EndScissorMode();
}

void chart_draw_sub_chart(const RegressionChannel* channel,
                          const ChartCoords* coords,
                          int start_index, int count) {
    if (channel == NULL || coords == NULL || count <= 0) {
        return;
    }
    
    /* 参考线 */
    float y_zero = chart_deviation_to_y(0, coords);
    DrawLine(coords->sub_rect.x, y_zero,
             coords->sub_rect.x + coords->sub_rect.width, y_zero,
             COLOR_CENTER);
    
    float y_p0618 = chart_deviation_to_y(0.618, coords);
    float y_m0618 = chart_deviation_to_y(-0.618, coords);
    DrawLine(coords->sub_rect.x, y_p0618,
             coords->sub_rect.x + coords->sub_rect.width, y_p0618, COLOR_SIGMA_0618);
    DrawLine(coords->sub_rect.x, y_m0618,
             coords->sub_rect.x + coords->sub_rect.width, y_m0618, COLOR_SIGMA_0618);
    
    float y_p1 = chart_deviation_to_y(1.0, coords);
    float y_m1 = chart_deviation_to_y(-1.0, coords);
    DrawLine(coords->sub_rect.x, y_p1,
             coords->sub_rect.x + coords->sub_rect.width, y_p1, COLOR_SIGMA_1_0);
    DrawLine(coords->sub_rect.x, y_m1,
             coords->sub_rect.x + coords->sub_rect.width, y_m1, COLOR_SIGMA_1_0);
    
    float y_p2 = chart_deviation_to_y(2.0, coords);
    float y_m2 = chart_deviation_to_y(-2.0, coords);
    DrawLine(coords->sub_rect.x, y_p2,
             coords->sub_rect.x + coords->sub_rect.width, y_p2, COLOR_SIGMA_2_0);
    DrawLine(coords->sub_rect.x, y_m2,
             coords->sub_rect.x + coords->sub_rect.width, y_m2, COLOR_SIGMA_2_0);
    
    /* 偏离度曲线 */
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        float y1 = chart_deviation_to_y(channel->deviation[start_index + i - 1], coords);
        float y2 = chart_deviation_to_y(channel->deviation[start_index + i], coords);
        DrawLine(x1, y1, x2, y2, WHITE);
    }
}

void chart_draw_axes(const Dataset* dataset, const ChartCoords* coords) {
    int screen_h = GetScreenHeight();
    int font_size = screen_h * 0.014;
    
    int price_labels = 8;
    for (int i = 0; i <= price_labels; i++) {
        float y = coords->main_rect.y + i * coords->main_rect.height / price_labels;
        double price = chart_y_to_price(y, coords);
        
        char label[32];
        if (price >= 1000) {
            sprintf(label, "$%.0f", price);
        } else if (price >= 1) {
            sprintf(label, "$%.2f", price);
        } else {
            sprintf(label, "$%.4f", price);
        }
        
        DrawText(label, 10, (int)y - font_size/2, font_size, COLOR_TEXT);
    }
    
    int last_year = 0;
    int end_idx = coords->view_start + coords->data_count;
    if (end_idx > dataset->count) end_idx = dataset->count;
    for (int i = coords->view_start; i < end_idx; i++) {
        const Candle* c = &dataset->candles[i];
        if (c->month == 1 && c->year != last_year) {
            float x = chart_index_to_x(i - coords->view_start, coords);
            char label[16];
            sprintf(label, "%d", c->year);
            DrawText(label, (int)x - 20, 
                     (int)(coords->main_rect.y + coords->main_rect.height + 8), 
                     font_size, COLOR_TEXT);
            last_year = c->year;
        }
    }
    
    char label[16];
    sprintf(label, "+2s");
    DrawText(label, 10, (int)chart_deviation_to_y(2.0, coords) - font_size/2, font_size - 2, COLOR_TEXT);
    sprintf(label, "0");
    DrawText(label, 10, (int)chart_deviation_to_y(0, coords) - font_size/2, font_size - 2, COLOR_TEXT);
    sprintf(label, "-2s");
    DrawText(label, 10, (int)chart_deviation_to_y(-2.0, coords) - font_size/2, font_size - 2, COLOR_TEXT);
}

void chart_draw_title_info(ChartState* state,
                           const Dataset* dataset,
                           const RegressionChannel* channel,
                           const ChartCoords* coords) {
    if (dataset == NULL || dataset->count == 0) {
        return;
    }
    
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    
    int font_large = screen_h * 0.022;
    int font_medium = screen_h * 0.016;
    
    const char* title = "Bitcoin K-Line + Regression Channel";
    DrawText(title, screen_w / 2 - MeasureText(title, font_large) / 2, 15, font_large, COLOR_TEXT_HIGHLIGHT);
    
    int legend_x = screen_w * 0.05 + 15;
    int legend_y = screen_h * 0.06 + 15;
    int line_h = screen_h * 0.022;
    
    char info[256];
    /* 显示当前可见范围最后一根K线的通道值 */
    int last = coords->view_start + coords->data_count - 1;
    if (last >= dataset->count) last = dataset->count - 1;
    
    sprintf(info, "Center: $%.0f", channel->center_line[last]);
    DrawText(info, legend_x, legend_y, font_medium, COLOR_CENTER);
    
    sprintf(info, "+/-0.618s: $%.0f / $%.0f", channel->upper_0618[last], channel->lower_0618[last]);
    DrawText(info, legend_x, legend_y + line_h, font_medium, COLOR_SIGMA_0618);
    
    sprintf(info, "+/-1.0s: $%.0f / $%.0f", channel->upper_1_0[last], channel->lower_1_0[last]);
    DrawText(info, legend_x, legend_y + line_h * 2, font_medium, COLOR_SIGMA_1_0);
    
    sprintf(info, "+/-2.0s: $%.0f / $%.0f", channel->upper_2_0[last], channel->lower_2_0[last]);
    DrawText(info, legend_x, legend_y + line_h * 3, font_medium, COLOR_SIGMA_2_0);
    
    sprintf(info, "Visible: %d bars | Current: $%.0f | Dev: %+.2fs",
            coords->data_count,
            dataset->candles[last].close,
            channel->deviation[last]);
    DrawText(info, screen_w - MeasureText(info, font_medium) - 20, legend_y, font_medium, COLOR_TEXT_HIGHLIGHT);
}

void chart_draw_mvrv_subchart(const ChartCoords* coords,
                              const IndicatorSeries* mvrv,
                              const Dataset* dataset,
                              int start_index, int count) {
    if (coords->mvrv_rect.height <= 0) return;
    
    BeginScissorMode((int)coords->mvrv_rect.x, (int)coords->mvrv_rect.y,
                     (int)coords->mvrv_rect.width, (int)coords->mvrv_rect.height);
    
    for (int i = 0; i <= 4; i++) {
        float y = coords->mvrv_rect.y + i * coords->mvrv_rect.height / 4;
        DrawLine(coords->mvrv_rect.x, y,
                 coords->mvrv_rect.x + coords->mvrv_rect.width, y,
                 COLOR_GRID);
    }
    
    int font_size = GetScreenHeight() * 0.012;
    char label[32];
    sprintf(label, "MVRV: %.4f", coords->mvrv_max);
    DrawText(label, coords->mvrv_rect.x + 4, (int)coords->mvrv_rect.y + 2, font_size, COLOR_MVRV);
    sprintf(label, "%.4f", coords->mvrv_min);
    DrawText(label, coords->mvrv_rect.x + 4,
             (int)(coords->mvrv_rect.y + coords->mvrv_rect.height - 18), font_size, COLOR_MVRV);
    
    double norm = (0.0 - coords->mvrv_min) / (coords->mvrv_max - coords->mvrv_min);
    float y0 = coords->mvrv_rect.y + (1.0f - (float)norm) * coords->mvrv_rect.height;
    Color zero_color = { 100, 100, 120, 180 };
    DrawLine(coords->mvrv_rect.x, (int)y0,
             coords->mvrv_rect.x + coords->mvrv_rect.width, (int)y0, zero_color);
    
    DrawLine((int)coords->mvrv_rect.x, (int)coords->mvrv_rect.y,
             (int)(coords->mvrv_rect.x + coords->mvrv_rect.width), (int)coords->mvrv_rect.y,
             ((Color){80, 80, 100, 200}));
    
    if (mvrv != NULL && mvrv->count > 0) {
        int last_drawn = -1;
        for (int i = 0; i < count; i++) {
            int idx = start_index + i;
            if (idx >= dataset->count) break;
            double val;
            if (indicator_get_value(mvrv, dataset->candles[idx].date, &val)) {
                float x = chart_index_to_x(i, coords);
                double norm = (val - coords->mvrv_min) / (coords->mvrv_max - coords->mvrv_min);
                float y = coords->mvrv_rect.y + (1.0 - norm) * coords->mvrv_rect.height;
                if (last_drawn >= 0) {
                    float prev_x = chart_index_to_x(last_drawn, coords);
                    double prev_val;
                    indicator_get_value(mvrv, dataset->candles[start_index + last_drawn].date, &prev_val);
                    double prev_norm = (prev_val - coords->mvrv_min) / (coords->mvrv_max - coords->mvrv_min);
                    float prev_y = coords->mvrv_rect.y + (1.0 - prev_norm) * coords->mvrv_rect.height;
                    DrawLine(prev_x, prev_y, x, y, COLOR_MVRV);
                }
                last_drawn = i;
            }
        }
    }
    
    EndScissorMode();
}

void chart_draw_cmcvdd_subchart(const ChartCoords* coords,
                                const IndicatorSeries* cmcvdd,
                                const Dataset* dataset,
                                int start_index, int count) {
    if (coords->cmcvdd_rect.height <= 0) return;
    
    BeginScissorMode((int)coords->cmcvdd_rect.x, (int)coords->cmcvdd_rect.y,
                     (int)coords->cmcvdd_rect.width, (int)coords->cmcvdd_rect.height);
    
    for (int i = 0; i <= 4; i++) {
        float y = coords->cmcvdd_rect.y + i * coords->cmcvdd_rect.height / 4;
        DrawLine(coords->cmcvdd_rect.x, y,
                 coords->cmcvdd_rect.x + coords->cmcvdd_rect.width, y,
                 COLOR_GRID);
    }
    
    int font_size = GetScreenHeight() * 0.012;
    char label[32];
    sprintf(label, "C-CVDD: %.0f", coords->cmcvdd_max);
    DrawText(label, coords->cmcvdd_rect.x + 4, (int)coords->cmcvdd_rect.y + 2, font_size, COLOR_CLOSE_MINUS_CVDD);
    sprintf(label, "%.0f", coords->cmcvdd_min);
    DrawText(label, coords->cmcvdd_rect.x + 4,
             (int)(coords->cmcvdd_rect.y + coords->cmcvdd_rect.height - 18), font_size, COLOR_CLOSE_MINUS_CVDD);
    
    double norm = (0.0 - coords->cmcvdd_min) / (coords->cmcvdd_max - coords->cmcvdd_min);
    float y0 = coords->cmcvdd_rect.y + (1.0f - (float)norm) * coords->cmcvdd_rect.height;
    Color zero_color = { 100, 100, 120, 180 };
    DrawLine(coords->cmcvdd_rect.x, (int)y0,
             coords->cmcvdd_rect.x + coords->cmcvdd_rect.width, (int)y0, zero_color);
    
    DrawLine((int)coords->cmcvdd_rect.x, (int)coords->cmcvdd_rect.y,
             (int)(coords->cmcvdd_rect.x + coords->cmcvdd_rect.width), (int)coords->cmcvdd_rect.y,
             ((Color){80, 80, 100, 200}));
    
    if (cmcvdd != NULL && cmcvdd->count > 0) {
        int last_drawn = -1;
        for (int i = 0; i < count; i++) {
            int idx = start_index + i;
            if (idx >= dataset->count) break;
            double val;
            if (indicator_get_value(cmcvdd, dataset->candles[idx].date, &val)) {
                float x = chart_index_to_x(i, coords);
                double norm = (val - coords->cmcvdd_min) / (coords->cmcvdd_max - coords->cmcvdd_min);
                float y = coords->cmcvdd_rect.y + (1.0 - norm) * coords->cmcvdd_rect.height;
                if (last_drawn >= 0) {
                    float prev_x = chart_index_to_x(last_drawn, coords);
                    double prev_val;
                    indicator_get_value(cmcvdd, dataset->candles[start_index + last_drawn].date, &prev_val);
                    double prev_norm = (prev_val - coords->cmcvdd_min) / (coords->cmcvdd_max - coords->cmcvdd_min);
                    float prev_y = coords->cmcvdd_rect.y + (1.0 - prev_norm) * coords->cmcvdd_rect.height;
                    DrawLine(prev_x, prev_y, x, y, COLOR_CLOSE_MINUS_CVDD);
                }
                last_drawn = i;
            }
        }
    }
    
    EndScissorMode();
}

void chart_draw(ChartState* state, 
                const Dataset* dataset,
                const RegressionChannel* channel,
                const ChartCoords* coords) {
    ClearBackground(COLOR_BG);
    chart_draw_grid(coords);
    chart_draw_channel_lines(channel, coords, coords->view_start, coords->data_count);
    chart_draw_candles(dataset, coords, coords->view_start, coords->data_count);
    chart_draw_sub_chart(channel, coords, coords->view_start, coords->data_count);
    chart_draw_mvrv_subchart(coords, state->mvrv, dataset, coords->view_start, coords->data_count);
    chart_draw_cmcvdd_subchart(coords, state->cmcvdd, dataset, coords->view_start, coords->data_count);
    chart_draw_axes(dataset, coords);
    chart_draw_title_info(state, dataset, channel, coords);
}

/* ========== PNG 保存 ========== */

static void chart_draw_to_texture(const Dataset* dataset,
                                   const RegressionChannel* channel,
                                   bool use_log,
                                   int width, int height,
                                   const IndicatorSeries* mvrv,
                                   const IndicatorSeries* cmcvdd,
                                   int view_count) {
    ChartCoords coords;
    (void)channel;
    
    int margin_left = width * 0.05;
    int margin_right = width * 0.07;
    int margin_top = height * 0.06;
    int margin_bottom = height * 0.04;
    
    bool has_mvrv = (mvrv != NULL && mvrv->count > 0);
    bool has_cmcvdd = (cmcvdd != NULL && cmcvdd->count > 0);
    int sub_height = height * 0.16;
    int sub_gap = height * 0.02;
    int mvrv_height = has_mvrv ? (height * 0.10) : 0;
    int mvrv_gap = has_mvrv ? (height * 0.02) : 0;
    int cmcvdd_height = has_cmcvdd ? (height * 0.10) : 0;
    int cmcvdd_gap = has_cmcvdd ? (height * 0.02) : 0;
    
    coords.main_rect.x = margin_left;
    coords.main_rect.y = margin_top;
    coords.main_rect.width = width - margin_left - margin_right;
    coords.main_rect.height = height - margin_top - margin_bottom
                              - sub_height - sub_gap
                              - mvrv_height - mvrv_gap
                              - cmcvdd_height - cmcvdd_gap;
    
    coords.sub_rect.x = margin_left;
    coords.sub_rect.y = coords.main_rect.y + coords.main_rect.height + sub_gap;
    coords.sub_rect.width = coords.main_rect.width;
    coords.sub_rect.height = sub_height;
    
    coords.mvrv_rect.x = margin_left;
    coords.mvrv_rect.y = coords.sub_rect.y + coords.sub_rect.height + mvrv_gap;
    coords.mvrv_rect.width = coords.main_rect.width;
    coords.mvrv_rect.height = mvrv_height;
    
    coords.cmcvdd_rect.x = margin_left;
    coords.cmcvdd_rect.y = coords.mvrv_rect.y + coords.mvrv_rect.height + cmcvdd_gap;
    coords.cmcvdd_rect.width = coords.main_rect.width;
    coords.cmcvdd_rect.height = cmcvdd_height;
    
    coords.data_count = view_count;
    coords.view_start = dataset->count - view_count;
    if (coords.view_start < 0) coords.view_start = 0;
    coords.use_log_scale = use_log;
    
    {
        double vmin, vmax;
        visible_price_range(dataset, coords.view_start, coords.data_count, &vmin, &vmax);
        double range = vmax - vmin;
        if (range < 0.0001) range = 0.0001;
        coords.price_min = vmin - range * 0.1;
        coords.price_max = vmax + range * 0.1;
    }
    
    coords.dev_min = -3.0;
    coords.dev_max = 3.0;
    
    if (has_mvrv) {
        coords.mvrv_min = indicator_get_min(mvrv);
        coords.mvrv_max = indicator_get_max(mvrv);
        double mr = coords.mvrv_max - coords.mvrv_min;
        if (mr < 0.0001) mr = 0.0001;
        coords.mvrv_min -= mr * 0.1;
        coords.mvrv_max += mr * 0.1;
    } else { coords.mvrv_min = 0; coords.mvrv_max = 1; }
    
    if (has_cmcvdd) {
        coords.cmcvdd_min = indicator_get_min(cmcvdd);
        coords.cmcvdd_max = indicator_get_max(cmcvdd);
        double cr = coords.cmcvdd_max - coords.cmcvdd_min;
        if (cr < 0.0001) cr = 0.0001;
        coords.cmcvdd_min -= cr * 0.1;
        coords.cmcvdd_max += cr * 0.1;
    } else { coords.cmcvdd_min = 0; coords.cmcvdd_max = 1; }
    
    ClearBackground(COLOR_BG);
    chart_draw_grid(&coords);
    chart_draw_channel_lines(channel, &coords, coords.view_start, coords.data_count);
    chart_draw_candles(dataset, &coords, coords.view_start, coords.data_count);
    chart_draw_sub_chart(channel, &coords, coords.view_start, coords.data_count);
    chart_draw_mvrv_subchart(&coords, mvrv, dataset, coords.view_start, coords.data_count);
    chart_draw_cmcvdd_subchart(&coords, cmcvdd, dataset, coords.view_start, coords.data_count);
    chart_draw_axes(dataset, &coords);
    
    int font_large = height * 0.022;
    int font_medium = height * 0.016;
    
    const char* title = "Bitcoin K-Line + Regression Channel";
    DrawText(title, width / 2 - MeasureText(title, font_large) / 2, 15, font_large, COLOR_TEXT_HIGHLIGHT);
    
    int legend_x = width * 0.05 + 15;
    int legend_y = height * 0.06 + 15;
    int line_h = height * 0.022;
    
    char info[256];
    int last = coords.view_start + coords.data_count - 1;
    if (last >= dataset->count) last = dataset->count - 1;
    
    sprintf(info, "Center: $%.0f", channel->center_line[last]);
    DrawText(info, legend_x, legend_y, font_medium, COLOR_CENTER);
    
    sprintf(info, "+/-0.618s: $%.0f / $%.0f", channel->upper_0618[last], channel->lower_0618[last]);
    DrawText(info, legend_x, legend_y + line_h, font_medium, COLOR_SIGMA_0618);
    
    sprintf(info, "+/-1.0s: $%.0f / $%.0f", channel->upper_1_0[last], channel->lower_1_0[last]);
    DrawText(info, legend_x, legend_y + line_h * 2, font_medium, COLOR_SIGMA_1_0);
    
    sprintf(info, "+/-2.0s: $%.0f / $%.0f", channel->upper_2_0[last], channel->lower_2_0[last]);
    DrawText(info, legend_x, legend_y + line_h * 3, font_medium, COLOR_SIGMA_2_0);
    
    sprintf(info, "Visible: %d bars | Current: $%.0f | Dev: %+.2fs",
            coords.data_count,
            dataset->candles[last].close, channel->deviation[last]);
    DrawText(info, width - MeasureText(info, font_medium) - 20, legend_y, font_medium, COLOR_TEXT_HIGHLIGHT);
}

void chart_draw_period_buttons(ChartState* state) {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int btn_w = 60;
    int btn_h = 26;
    int gap = 6;
    int font_size = screen_h * 0.015;
    
    int start_x = screen_w - (btn_w * 3 + gap * 2) - 30;
    int start_y = 15;
    
    state->btn_daily = (Rectangle){ start_x, start_y, btn_w, btn_h };
    state->btn_monthly = (Rectangle){ start_x + btn_w + gap, start_y, btn_w, btn_h };
    state->btn_quarterly = (Rectangle){ start_x + (btn_w + gap) * 2, start_y, btn_w, btn_h };
    
    const char* labels[] = {"日线", "月线", "季线"};
    Rectangle rects[] = {state->btn_daily, state->btn_monthly, state->btn_quarterly};
    PeriodType types[] = {PERIOD_DAILY, PERIOD_MONTHLY, PERIOD_QUARTERLY};
    
    for (int i = 0; i < 3; i++) {
        bool is_active = (state->current_period == types[i]);
        Color bg = is_active ? ((Color){60, 100, 200, 220}) : ((Color){40, 40, 50, 200});
        Color border = is_active ? ((Color){100, 150, 255, 255}) : ((Color){80, 80, 100, 200});
        
        DrawRectangleRec(rects[i], bg);
        DrawRectangleLinesEx(rects[i], 1.5f, border);
        
        int tw = MeasureText(labels[i], font_size);
        DrawText(labels[i], 
                 (int)(rects[i].x + (rects[i].width - tw) / 2),
                 (int)(rects[i].y + (rects[i].height - font_size) / 2),
                 font_size, COLOR_TEXT);
    }
}

int chart_handle_period_click(ChartState* state, Vector2 mouse_pos) {
    if (CheckCollisionPointRec(mouse_pos, state->btn_daily))
        return PERIOD_DAILY;
    if (CheckCollisionPointRec(mouse_pos, state->btn_monthly))
        return PERIOD_MONTHLY;
    if (CheckCollisionPointRec(mouse_pos, state->btn_quarterly))
        return PERIOD_QUARTERLY;
    return -1;
}

bool chart_save_to_file(const Dataset* dataset,
                        const RegressionChannel* channel,
                        bool use_log,
                        const char* filename,
                        const IndicatorSeries* mvrv,
                        const IndicatorSeries* cmcvdd,
                        int view_count) {
    if (dataset == NULL || channel == NULL || filename == NULL) {
        return false;
    }
    
    int width = 1600;
    int height = 900;
    
    RenderTexture2D target = LoadRenderTexture(width, height);
    if (target.id == 0) {
        printf("Error: Failed to create render texture.\n");
        return false;
    }
    
    BeginTextureMode(target);
    chart_draw_to_texture(dataset, channel, use_log, width, height, mvrv, cmcvdd, view_count);
    EndTextureMode();
    
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    
    bool success = ExportImage(image, filename);
    
    UnloadImage(image);
    UnloadRenderTexture(target);
    
    return success;
}
