/**
 * @file crosshair.c
 * @brief 十字线交互模块实现
 */

#include "crosshair.h"
#include <time.h>

/* ========== 常量定义 ========== */
#define DOUBLE_CLICK_INTERVAL 0.3  // 双击间隔（秒）
#define INFO_BOX_WIDTH 180
#define INFO_BOX_LINE_HEIGHT 16
#define INFO_BOX_PADDING 8

/* ========== 函数实现 ========== */

Crosshair* crosshair_create(void) {
    Crosshair* ch = (Crosshair*)malloc(sizeof(Crosshair));
    if (ch == NULL) {
        return NULL;
    }
    
    ch->enabled = true;   // 默认开启十字光标，双击可切换
    ch->x_index = 0;
    ch->y_price = 0.0;
    ch->mouse_pos = (Vector2){ 0, 0 };
    ch->in_chart = false;
    ch->last_click_time = 0.0;
    ch->click_count = 0;
    
    return ch;
}

void crosshair_free(Crosshair* ch) {
    if (ch != NULL) {
        free(ch);
    }
}

void crosshair_update(Crosshair* ch, const ChartCoords* coords) {
    if (ch == NULL || coords == NULL) {
        return;
    }
    
    // 获取鼠标位置
    ch->mouse_pos = GetMousePosition();
    
    // 检查鼠标是否在图表区域内（主图+所有副图）
    float chart_left = coords->main_rect.x;
    float chart_right = coords->main_rect.x + coords->main_rect.width;
    float chart_top = coords->main_rect.y;
    float chart_bottom = coords->main_rect.y + coords->main_rect.height;
    if (coords->sub_rect.height > 0) {
        chart_bottom = coords->sub_rect.y + coords->sub_rect.height;
    }
    if (coords->mvrv_rect.height > 0) {
        chart_bottom = coords->mvrv_rect.y + coords->mvrv_rect.height;
    }
    if (coords->cmcvdd_rect.height > 0) {
        chart_bottom = coords->cmcvdd_rect.y + coords->cmcvdd_rect.height;
    }
    ch->in_chart = (ch->mouse_pos.x >= chart_left &&
                    ch->mouse_pos.x <= chart_right &&
                    ch->mouse_pos.y >= chart_top &&
                    ch->mouse_pos.y <= chart_bottom);
    
    // 处理双击
    if (crosshair_check_double_click(ch)) {
        ch->enabled = !ch->enabled;
        printf("十字线: %s\n", ch->enabled ? "开启" : "关闭");
    }
    
    // 更新十字线位置
    if (ch->enabled && ch->in_chart) {
        ch->x_index = chart_x_to_index(ch->mouse_pos.x, coords);
        ch->y_price = chart_y_to_price(ch->mouse_pos.y, coords);
    }
}

bool crosshair_check_double_click(Crosshair* ch) {
    if (ch == NULL) {
        return false;
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        double current_time = GetTime();
        
        if (current_time - ch->last_click_time < DOUBLE_CLICK_INTERVAL) {
            ch->click_count++;
            if (ch->click_count >= 2) {
                ch->click_count = 0;
                ch->last_click_time = 0;
                return true;
            }
        } else {
            ch->click_count = 1;
        }
        
        ch->last_click_time = current_time;
    }
    
    return false;
}

void crosshair_draw(const Crosshair* ch, const ChartCoords* coords) {
    if (ch == NULL || !ch->enabled || !ch->in_chart) {
        return;
    }
    
    float x = ch->mouse_pos.x;
    // 竖线：从主图顶部到底部
    float top_y = coords->main_rect.y;
    float bottom_y = coords->main_rect.y + coords->main_rect.height;
    // 如果有偏离度副图，延伸到底部
    if (coords->sub_rect.height > 0) {
        bottom_y = coords->sub_rect.y + coords->sub_rect.height;
    }
    // 如果有 MVRV 副图，继续延伸
    if (coords->mvrv_rect.height > 0) {
        bottom_y = coords->mvrv_rect.y + coords->mvrv_rect.height;
    }
    // 如果有 C-CVDD 副图，继续延伸到底部
    if (coords->cmcvdd_rect.height > 0) {
        bottom_y = coords->cmcvdd_rect.y + coords->cmcvdd_rect.height;
    }
    DrawLine(x, top_y, x, bottom_y, COLOR_CROSSHAIR);
    
    float y = ch->mouse_pos.y;
    DrawLine(coords->main_rect.x, y,
             coords->main_rect.x + coords->main_rect.width, y,
             COLOR_CROSSHAIR);
    
    int screen_h = GetScreenHeight();
    int font_size = screen_h * 0.014;
    
    char price_label[32];
    if (ch->y_price >= 1000) {
        sprintf(price_label, "$%.0f", ch->y_price);
    } else if (ch->y_price >= 1) {
        sprintf(price_label, "$%.2f", ch->y_price);
    } else {
        sprintf(price_label, "$%.4f", ch->y_price);
    }
    
    int label_width = MeasureText(price_label, font_size) + 10;
    int label_x = coords->main_rect.x + coords->main_rect.width + 5;
    int label_y = (int)y - font_size/2 - 2;
    
    DrawRectangle(label_x, label_y, label_width, font_size + 6, ((Color){ 50, 50, 200, 220 }));
    DrawText(price_label, label_x + 5, label_y + 3, font_size, WHITE);
}

void crosshair_get_info_ex(int index,
                           const Dataset* dataset,
                           const RegressionChannel* channel,
                           const IndicatorSeries* mvrv,
                           const IndicatorSeries* cmcvdd,
                           InfoBox* info) {
    if (index < 0 || index >= dataset->count || info == NULL) {
        return;
    }
    
    const Candle* c = &dataset->candles[index];
    
    strcpy(info->date, c->date);
    sprintf(info->open, "Open: $%.2f", c->open);
    sprintf(info->high, "High: $%.2f", c->high);
    sprintf(info->low, "Low: $%.2f", c->low);
    sprintf(info->close, "Close: $%.2f", c->close);
    
    if (channel != NULL) {
        sprintf(info->center, "Center: $%.2f", channel->center_line[index]);
        sprintf(info->upper_0618, "+0.618s: $%.2f", channel->upper_0618[index]);
        sprintf(info->lower_0618, "-0.618s: $%.2f", channel->lower_0618[index]);
        sprintf(info->upper_1_0, "+1.0s: $%.2f", channel->upper_1_0[index]);
        sprintf(info->lower_1_0, "-1.0s: $%.2f", channel->lower_1_0[index]);
        sprintf(info->upper_2_0, "+2.0s: $%.2f", channel->upper_2_0[index]);
        sprintf(info->lower_2_0, "-2.0s: $%.2f", channel->lower_2_0[index]);
        sprintf(info->deviation, "Deviation: %+.2fs", channel->deviation[index]);
    }
    
    // 指标值
    double val;
    if (mvrv != NULL && indicator_get_value(mvrv, c->date, &val)) {
        sprintf(info->mvrv, "MVRV: %.4f", val);
    } else {
        sprintf(info->mvrv, "MVRV: N/A");
    }
    if (cmcvdd != NULL && indicator_get_value(cmcvdd, c->date, &val)) {
        sprintf(info->close_minus_cvdd, "C-CVDD: %.2f%%", val);
    } else {
        sprintf(info->close_minus_cvdd, "C-CVDD: N/A");
    }
}

void crosshair_get_info(int index,
                        const Dataset* dataset,
                        const RegressionChannel* channel,
                        InfoBox* info) {
    crosshair_get_info_ex(index, dataset, channel, NULL, NULL, info);
}

void crosshair_draw_info_ex(const Crosshair* ch,
                           const Dataset* dataset,
                           const RegressionChannel* channel,
                           const IndicatorSeries* mvrv,
                           const IndicatorSeries* cmcvdd,
                           const ChartCoords* coords) {
    if (ch == NULL || !ch->enabled || !ch->in_chart) {
        return;
    }
    
    InfoBox info;
    memset(&info, 0, sizeof(info));
    crosshair_get_info_ex(ch->x_index, dataset, channel, mvrv, cmcvdd, &info);
    
    int screen_h = GetScreenHeight();
    
    // 用更大的字体，统一黑色
    int font_size = screen_h * 0.018;
    int line_height = font_size + 6;
    int box_width = screen_h * 0.16;
    int box_padding = 10;
    
    int box_x = coords->main_rect.x + 15;
    int box_y = coords->main_rect.y + coords->main_rect.height * 0.25;
    int total_lines = 16;
    bool has_mvrv = mvrv != NULL && mvrv->count > 0;
    bool has_cmcvdd = cmcvdd != NULL && cmcvdd->count > 0;
    if (has_mvrv || has_cmcvdd) total_lines += 3;
    
    int box_height = total_lines * line_height + box_padding * 2;
    Color box_bg = { 255, 255, 230, 245 }; // 米白底
    Color border_color = { 80, 80, 80, 200 };
    
    DrawRectangle(box_x, box_y, box_width, box_height, box_bg);
    DrawRectangleLines(box_x, box_y, box_width, box_height, border_color);
    
    int text_y = box_y + box_padding;
    DrawText(info.date, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    
    DrawLine(box_x + 5, text_y, box_x + box_width - 5, text_y, border_color);
    text_y += 6;
    
    DrawText(info.open, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.high, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.low, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.close, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    
    DrawLine(box_x + 5, text_y, box_x + box_width - 5, text_y, border_color);
    text_y += 6;
    
    DrawText(info.center, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.upper_0618, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.lower_0618, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.upper_1_0, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.lower_1_0, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.upper_2_0, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    DrawText(info.lower_2_0, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    
    DrawLine(box_x + 5, text_y, box_x + box_width - 5, text_y, border_color);
    text_y += 6;
    
    DrawText(info.deviation, box_x + box_padding, text_y, font_size, BLACK);
    text_y += line_height;
    
    if (has_mvrv || has_cmcvdd) {
        DrawLine(box_x + 5, text_y, box_x + box_width - 5, text_y, border_color);
        text_y += 6;
        
        DrawText(info.mvrv, box_x + box_padding, text_y, font_size, BLACK);
        text_y += line_height;
        DrawText(info.close_minus_cvdd, box_x + box_padding, text_y, font_size, BLACK);
        text_y += line_height;
    }
}

void crosshair_draw_info(const Crosshair* ch,
                         const Dataset* dataset,
                         const RegressionChannel* channel,
                         const ChartCoords* coords) {
    crosshair_draw_info_ex(ch, dataset, channel, NULL, NULL, coords);
}
