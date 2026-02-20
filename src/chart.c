/**
 * @file chart.c
 * @brief 图形绑制模块实现 - 使用raylib
 */

#include "chart.h"
#include "crosshair.h"
#include <math.h>

/* ========== 内部辅助函数 ========== */

/**
 * @brief 对数坐标转换
 */
static double log_transform(double value, bool use_log) {
    if (use_log && value > 0) {
        return log10(value);
    }
    return value;
}

/**
 * @brief 对数坐标逆转换
 */
static double log_inverse(double value, bool use_log) {
    if (use_log) {
        return pow(10.0, value);
    }
    return value;
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
    
    int index = (int)((x - coords->main_rect.x) / x_per_bar);
    
    if (index < 0) index = 0;
    if (index >= coords->data_count) index = coords->data_count - 1;
    
    return index;
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
    state->view_start = 0;
    state->view_count = 0;
    state->font_size = 16;
    
    state->font = GetFontDefault();
    
    return state;
}

void chart_close(ChartState* state) {
    if (state != NULL) {
        CloseWindow();
        free(state);
    }
}

ChartCoords chart_calc_coords(const Dataset* dataset, 
                               const RegressionChannel* channel,
                               bool use_log) {
    ChartCoords coords;
    
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    
    int margin_left = screen_w * 0.05;
    int margin_right = screen_w * 0.07;
    int margin_top = screen_h * 0.06;
    int margin_bottom = screen_h * 0.05;
    int sub_height = screen_h * 0.18;
    int sub_gap = screen_h * 0.03;
    
    coords.main_rect.x = margin_left;
    coords.main_rect.y = margin_top;
    coords.main_rect.width = screen_w - margin_left - margin_right;
    coords.main_rect.height = screen_h - margin_top - margin_bottom - sub_height - sub_gap;
    
    coords.sub_rect.x = margin_left;
    coords.sub_rect.y = coords.main_rect.y + coords.main_rect.height + sub_gap;
    coords.sub_rect.width = coords.main_rect.width;
    coords.sub_rect.height = sub_height;
    
    coords.data_count = dataset->count;
    coords.use_log_scale = use_log;
    
    coords.price_min = dataset_get_lowest(dataset) * 0.7;
    coords.price_max = dataset_get_highest(dataset) * 1.3;
    
    coords.dev_min = -3.0;
    coords.dev_max = 3.0;
    
    return coords;
}

void chart_draw_grid(const ChartCoords* coords) {
    // 主图网格
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
    
    // 副图网格
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
                               int count) {
    if (channel == NULL || coords == NULL || count <= 0) {
        return;
    }
    
    BeginScissorMode((int)coords->main_rect.x, (int)coords->main_rect.y, 
                     (int)coords->main_rect.width, (int)coords->main_rect.height);
    
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        
        float y1 = chart_price_to_y(channel->center_line[i - 1], coords);
        float y2 = chart_price_to_y(channel->center_line[i], coords);
        
        DrawLine(x1, y1, x2, y2, COLOR_CENTER);
    }
    
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        
        float y1, y2;
        
        y1 = chart_price_to_y(channel->upper_0618[i - 1], coords);
        y2 = chart_price_to_y(channel->upper_0618[i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_0618);
        
        y1 = chart_price_to_y(channel->lower_0618[i - 1], coords);
        y2 = chart_price_to_y(channel->lower_0618[i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_0618);
    }
    
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        
        float y1, y2;
        
        y1 = chart_price_to_y(channel->upper_1_0[i - 1], coords);
        y2 = chart_price_to_y(channel->upper_1_0[i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_1_0);
        
        y1 = chart_price_to_y(channel->lower_1_0[i - 1], coords);
        y2 = chart_price_to_y(channel->lower_1_0[i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_1_0);
    }
    
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        
        float y1, y2;
        
        y1 = chart_price_to_y(channel->upper_1_5[i - 1], coords);
        y2 = chart_price_to_y(channel->upper_1_5[i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_1_5);
        
        y1 = chart_price_to_y(channel->lower_1_5[i - 1], coords);
        y2 = chart_price_to_y(channel->lower_1_5[i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_1_5);
    }
    
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        
        float y1, y2;
        
        y1 = chart_price_to_y(channel->upper_2_0[i - 1], coords);
        y2 = chart_price_to_y(channel->upper_2_0[i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_2_0);
        
        y1 = chart_price_to_y(channel->lower_2_0[i - 1], coords);
        y2 = chart_price_to_y(channel->lower_2_0[i], coords);
        DrawLine(x1, y1, x2, y2, COLOR_SIGMA_2_0);
    }
    
    EndScissorMode();
}

void chart_draw_sub_chart(const RegressionChannel* channel,
                          const ChartCoords* coords,
                          int count) {
    if (channel == NULL || coords == NULL || count <= 0) {
        return;
    }
    
    // 绘制参考线
    float y_zero = chart_deviation_to_y(0, coords);
    DrawLine(coords->sub_rect.x, y_zero,
             coords->sub_rect.x + coords->sub_rect.width, y_zero,
             COLOR_CENTER);
    
    float y_p0618 = chart_deviation_to_y(0.618, coords);
    float y_m0618 = chart_deviation_to_y(-0.618, coords);
    DrawLine(coords->sub_rect.x, y_p0618,
             coords->sub_rect.x + coords->sub_rect.width, y_p0618,
             COLOR_SIGMA_0618);
    DrawLine(coords->sub_rect.x, y_m0618,
             coords->sub_rect.x + coords->sub_rect.width, y_m0618,
             COLOR_SIGMA_0618);
    
    float y_p1 = chart_deviation_to_y(1.0, coords);
    float y_m1 = chart_deviation_to_y(-1.0, coords);
    DrawLine(coords->sub_rect.x, y_p1,
             coords->sub_rect.x + coords->sub_rect.width, y_p1,
             COLOR_SIGMA_1_0);
    DrawLine(coords->sub_rect.x, y_m1,
             coords->sub_rect.x + coords->sub_rect.width, y_m1,
             COLOR_SIGMA_1_0);
    
    float y_p2 = chart_deviation_to_y(2.0, coords);
    float y_m2 = chart_deviation_to_y(-2.0, coords);
    DrawLine(coords->sub_rect.x, y_p2,
             coords->sub_rect.x + coords->sub_rect.width, y_p2,
             COLOR_SIGMA_2_0);
    DrawLine(coords->sub_rect.x, y_m2,
             coords->sub_rect.x + coords->sub_rect.width, y_m2,
             COLOR_SIGMA_2_0);
    
    // 绘制偏离度曲线
    for (int i = 1; i < count; i++) {
        float x1 = chart_index_to_x(i - 1, coords);
        float x2 = chart_index_to_x(i, coords);
        
        float y1 = chart_deviation_to_y(channel->deviation[i - 1], coords);
        float y2 = chart_deviation_to_y(channel->deviation[i], coords);
        
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
    for (int i = 0; i < dataset->count; i++) {
        const Candle* c = &dataset->candles[i];
        if (c->month == 1 && c->year != last_year) {
            float x = chart_index_to_x(i, coords);
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
                           const RegressionChannel* channel) {
    if (dataset == NULL || dataset->count == 0) {
        return;
    }
    
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    
    int font_large = screen_h * 0.022;
    int font_medium = screen_h * 0.016;
    
    const char* title = "Bitcoin Monthly K-Line + Linear Regression Channel";
    DrawText(title, screen_w / 2 - MeasureText(title, font_large) / 2, 15, font_large, COLOR_TEXT_HIGHLIGHT);
    
    int legend_x = screen_w * 0.05 + 15;
    int legend_y = screen_h * 0.06 + 15;
    int line_h = screen_h * 0.022;
    
    char info[256];
    int last = dataset->count - 1;
    
    sprintf(info, "Center: $%.0f", channel->center_line[last]);
    DrawText(info, legend_x, legend_y, font_medium, COLOR_CENTER);
    
    sprintf(info, "+/-0.618s: $%.0f / $%.0f", channel->upper_0618[last], channel->lower_0618[last]);
    DrawText(info, legend_x, legend_y + line_h, font_medium, COLOR_SIGMA_0618);
    
    sprintf(info, "+/-1.0s: $%.0f / $%.0f", channel->upper_1_0[last], channel->lower_1_0[last]);
    DrawText(info, legend_x, legend_y + line_h * 2, font_medium, COLOR_SIGMA_1_0);
    
    sprintf(info, "+/-2.0s: $%.0f / $%.0f", channel->upper_2_0[last], channel->lower_2_0[last]);
    DrawText(info, legend_x, legend_y + line_h * 3, font_medium, COLOR_SIGMA_2_0);
    
    sprintf(info, "Current: $%.0f | Deviation: %+.2fs",
            dataset->candles[last].close,
            channel->deviation[last]);
    DrawText(info, screen_w - MeasureText(info, font_medium) - 20, legend_y, font_medium, COLOR_TEXT_HIGHLIGHT);
}

void chart_draw(ChartState* state, 
                const Dataset* dataset,
                const RegressionChannel* channel,
                const ChartCoords* coords) {
    ClearBackground(COLOR_BG);
    chart_draw_grid(coords);
    chart_draw_channel_lines(channel, coords, coords->data_count);
    chart_draw_candles(dataset, coords, 0, coords->data_count);
    chart_draw_sub_chart(channel, coords, coords->data_count);
    chart_draw_axes(dataset, coords);
    chart_draw_title_info(state, dataset, channel);
}

static void chart_draw_to_texture(const Dataset* dataset,
                                   const RegressionChannel* channel,
                                   bool use_log,
                                   int width, int height) {
    ChartCoords coords;
    
    int margin_left = width * 0.05;
    int margin_right = width * 0.07;
    int margin_top = height * 0.06;
    int margin_bottom = height * 0.05;
    int sub_height = height * 0.18;
    int sub_gap = height * 0.03;
    
    coords.main_rect.x = margin_left;
    coords.main_rect.y = margin_top;
    coords.main_rect.width = width - margin_left - margin_right;
    coords.main_rect.height = height - margin_top - margin_bottom - sub_height - sub_gap;
    
    coords.sub_rect.x = margin_left;
    coords.sub_rect.y = coords.main_rect.y + coords.main_rect.height + sub_gap;
    coords.sub_rect.width = coords.main_rect.width;
    coords.sub_rect.height = sub_height;
    
    coords.data_count = dataset->count;
    coords.use_log_scale = use_log;
    coords.price_min = dataset_get_lowest(dataset) * 0.7;
    coords.price_max = dataset_get_highest(dataset) * 1.3;
    coords.dev_min = -3.0;
    coords.dev_max = 3.0;
    
    ClearBackground(COLOR_BG);
    chart_draw_grid(&coords);
    chart_draw_channel_lines(channel, &coords, coords.data_count);
    chart_draw_candles(dataset, &coords, 0, coords.data_count);
    chart_draw_sub_chart(channel, &coords, coords.data_count);
    chart_draw_axes(dataset, &coords);
    
    int font_large = height * 0.022;
    int font_medium = height * 0.016;
    
    const char* title = "Bitcoin Monthly K-Line + Linear Regression Channel";
    DrawText(title, width / 2 - MeasureText(title, font_large) / 2, 15, font_large, COLOR_TEXT_HIGHLIGHT);
    
    int legend_x = width * 0.05 + 15;
    int legend_y = height * 0.06 + 15;
    int line_h = height * 0.022;
    
    char info[256];
    int last = dataset->count - 1;
    
    sprintf(info, "Center: $%.0f", channel->center_line[last]);
    DrawText(info, legend_x, legend_y, font_medium, COLOR_CENTER);
    
    sprintf(info, "+/-0.618s: $%.0f / $%.0f", channel->upper_0618[last], channel->lower_0618[last]);
    DrawText(info, legend_x, legend_y + line_h, font_medium, COLOR_SIGMA_0618);
    
    sprintf(info, "+/-1.0s: $%.0f / $%.0f", channel->upper_1_0[last], channel->lower_1_0[last]);
    DrawText(info, legend_x, legend_y + line_h * 2, font_medium, COLOR_SIGMA_1_0);
    
    sprintf(info, "+/-2.0s: $%.0f / $%.0f", channel->upper_2_0[last], channel->lower_2_0[last]);
    DrawText(info, legend_x, legend_y + line_h * 3, font_medium, COLOR_SIGMA_2_0);
    
    sprintf(info, "Current: $%.0f | Deviation: %+.2fs",
            dataset->candles[last].close, channel->deviation[last]);
    DrawText(info, width - MeasureText(info, font_medium) - 20, legend_y, font_medium, COLOR_TEXT_HIGHLIGHT);
}

bool chart_save_to_file(const Dataset* dataset,
                        const RegressionChannel* channel,
                        bool use_log,
                        const char* filename) {
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
    chart_draw_to_texture(dataset, channel, use_log, width, height);
    EndTextureMode();
    
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    
    bool success = ExportImage(image, filename);
    
    UnloadImage(image);
    UnloadRenderTexture(target);
    
    return success;
}
