/**
 * @file main.c
 * @brief 比特币分析工具 - C语言版本
 * 
 * 功能：
 * - 从Binance API获取数据或加载本地CSV
 * - 计算线性回归通道
 * - 显示K线图和通道线
 * - 支持十字线交互
 * - 自动保存图表为PNG
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "data.h"
#include "calculate.h"
#include "chart.h"
#include "crosshair.h"
#include "network.h"

/* ========== 常量定义 ========== */
#define DATA_FILE "data/btc_historical_monthly.csv"
#define DATA_FILE_ALT "../data/btc_historical_monthly.csv"
#define DATA_FILE_PYTHON "D:/openskills/BTC/btc_historical_monthly.csv"
#define OUTPUT_IMAGE "btc_chart.png"

/* ========== 全局变量 ========== */
static bool g_auto_update = true;
static bool g_save_image = true;

/* ========== 菜单函数 ========== */

void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void print_header(void) {
    printf("============================================================\n");
    printf("           Bitcoin Analysis Toolkit - C Edition\n");
    printf("============================================================\n\n");
}

void print_menu(void) {
    print_header();
    printf("[Chart Analysis]\n");
    printf("  1. Bitcoin K-Line + Regression Channel (2017+, Linear Scale)\n");
    printf("     - 9 channel lines: Center, +/-0.618s, +/-1.0s, +/-1.5s, +/-2.0s\n");
    printf("     - Crosshair enabled (double-click to toggle)\n\n");
    
    printf("  2. Bitcoin K-Line + Regression Channel (2017+, Log Scale)\n");
    printf("     - Channel lines are straight on log scale\n");
    printf("     - Best for long-term trend analysis\n\n");
    
    printf("  3. Bitcoin Full History K-Line + Channel (2010-2026)\n");
    printf("     - All historical data\n");
    printf("     - Log scale display\n\n");
    
    printf("[Other]\n");
    printf("  4. View Data Summary\n");
    printf("  0. Exit\n\n");
    printf("============================================================\n");
}

void print_data_summary(const Dataset* dataset) {
    printf("\n============================================================\n");
    printf("Data Summary\n");
    printf("============================================================\n");
    
    if (dataset == NULL || dataset->count == 0) {
        printf("No data loaded.\n");
    } else {
        printf("\nMonthly Data:\n");
        printf("   Records: %d\n", dataset->count);
        
        int start_year, end_year;
        dataset_get_date_range(dataset, &start_year, &end_year);
        printf("   Range: %d to %d\n", start_year, end_year);
        printf("   Latest Close: $%.2f\n", dataset->candles[dataset->count - 1].close);
        printf("   All-Time High: $%.2f\n", dataset_get_highest(dataset));
        printf("   All-Time Low: $%.2f\n", dataset_get_lowest(dataset));
    }
    
    printf("============================================================\n");
}

/* ========== 图表显示函数 ========== */

void show_chart(Dataset* full_dataset, int start_year, bool use_log_scale) {
    // 筛选数据
    Dataset* dataset = (start_year > 0) ? 
        dataset_filter_from_year(full_dataset, start_year) : full_dataset;
    
    if (dataset == NULL || dataset->count == 0) {
        printf("Error: No data available.\n");
        return;
    }
    
    printf("\nLoading %d data points (%d to present)...\n", 
           dataset->count, start_year > 0 ? start_year : 2010);
    
    // 计算回归通道
    RegressionChannel* channel = use_log_scale ? 
        channel_calculate_log(dataset) : channel_calculate_linear(dataset);
    
    if (channel == NULL) {
        printf("Error: Failed to calculate regression channel.\n");
        if (dataset != full_dataset) dataset_free(dataset);
        return;
    }
    
    channel_print_params(channel);
    channel_print_last_values(channel, dataset);
    
    // 初始化图表
    ChartState* chart = chart_init();
    if (chart == NULL) {
        printf("Error: Failed to initialize chart.\n");
        channel_free(channel);
        if (dataset != full_dataset) dataset_free(dataset);
        return;
    }
    
    printf("\n============================================================\n");
    printf("Chart Controls:\n");
    printf("  - Double-click: Toggle crosshair on/off\n");
    printf("  - Move mouse: View price and indicator data\n");
    printf("  - Resize window: Drag window corners\n");
    printf("  - Press ESC: Close chart and return to menu\n");
    printf("============================================================\n\n");
    
    ChartCoords coords = {0};
    Crosshair* crosshair = crosshair_create();
    if (crosshair == NULL) {
        printf("Warning: Failed to create crosshair.\n");
    }
    
    int last_width = 0;
    int last_height = 0;
    bool saved_image = false;
    int frame_count = 0;
    bool coords_initialized = false;
    bool auto_save_done = false;
    
    while (!WindowShouldClose()) {
        int current_width = GetScreenWidth();
        int current_height = GetScreenHeight();
        
        if (!coords_initialized || current_width != last_width || current_height != last_height) {
            coords = chart_calc_coords(dataset, channel, use_log_scale);
            last_width = current_width;
            last_height = current_height;
            coords_initialized = true;
        }
        
        if (crosshair != NULL) {
            crosshair_update(crosshair, &coords);
        }
        
        if (g_save_image && !saved_image && frame_count == 3) {
            if (chart_save_to_file(dataset, channel, use_log_scale, OUTPUT_IMAGE)) {
                printf("\nChart auto-saved to: %s\n", OUTPUT_IMAGE);
            } else {
                printf("\nFailed to save chart.\n");
            }
            saved_image = true;
            auto_save_done = true;
        }
        
        if (IsKeyPressed(KEY_S) && g_save_image && saved_image && !auto_save_done && frame_count > 10) {
            if (chart_save_to_file(dataset, channel, use_log_scale, OUTPUT_IMAGE)) {
                printf("\nChart saved to: %s\n", OUTPUT_IMAGE);
            } else {
                printf("\nFailed to save chart.\n");
            }
        }
        
        BeginDrawing();
        
        chart_draw(chart, dataset, channel, &coords);
        
        if (crosshair != NULL) {
            crosshair_draw(crosshair, &coords);
            crosshair_draw_info(crosshair, dataset, channel, &coords);
        }
        
        int font_help = last_height * 0.012;
        if (font_help < 10) font_help = 14;
        DrawText("Press S to save | ESC to close | Double-click for crosshair | Resize to adjust", 
                 10, last_height - font_help - 10, font_help, ((Color){ 150, 150, 150, 255 }));
        
        EndDrawing();
        
        frame_count++;
    }
    
    crosshair_free(crosshair);
    chart_close(chart);
    channel_free(channel);
    if (dataset != full_dataset) dataset_free(dataset);
    
    printf("\nChart closed.\n");
}

/* ========== 数据加载函数 ========== */

Dataset* load_data(void) {
    Dataset* dataset = NULL;
    
    const char* paths[] = {
        DATA_FILE,
        DATA_FILE_ALT,
        DATA_FILE_PYTHON,
        NULL
    };
    
    for (int i = 0; paths[i] != NULL; i++) {
        printf("Trying: %s\n", paths[i]);
        dataset = dataset_load_csv(paths[i]);
        if (dataset != NULL) {
            printf("Data loaded successfully from: %s\n", paths[i]);
            break;
        }
    }
    
    return dataset;
}

Dataset* load_or_update_data(bool auto_update) {
    if (auto_update) {
        return update_data(DATA_FILE);
    }
    return load_data();
}

/* ========== 主函数 ========== */

int main(int argc, char* argv[]) {
    printf("============================================================\n");
    printf("         Bitcoin Analysis Toolkit - C Edition\n");
    printf("============================================================\n\n");
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-update") == 0) {
            g_auto_update = false;
        }
        if (strcmp(argv[i], "--no-save") == 0) {
            g_save_image = false;
        }
    }
    
    printf("Loading data");
    if (g_auto_update) {
        printf(" (with network update)...\n");
    } else {
        printf(" (local only)...\n");
    }
    
    Dataset* dataset = load_or_update_data(g_auto_update);
    
    if (dataset == NULL) {
        printf("\nError: Failed to load data file.\n");
        printf("Please ensure 'btc_historical_monthly.csv' is in the data/ directory.\n");
        printf("Or run with network enabled to fetch data from Binance API.\n");
        return 1;
    }
    
    dataset_print_info(dataset);
    
    int choice = 0;
    char input[16];
    
    while (1) {
        clear_screen();
        print_menu();
        
        printf("Enter choice (0-4): ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        choice = atoi(input);
        
        switch (choice) {
            case 0:
                printf("\nGoodbye!\n");
                dataset_free(dataset);
                return 0;
                
            case 1:
                // 2017年至今，线性坐标
                show_chart(dataset, 2017, false);
                break;
                
            case 2:
                // 2017年至今，对数坐标
                show_chart(dataset, 2017, true);
                break;
                
            case 3:
                // 全部历史，对数坐标
                show_chart(dataset, 0, true);
                break;
                
            case 4:
                // 数据概况
                print_data_summary(dataset);
                printf("\nPress Enter to continue...");
                getchar();
                break;
                
            default:
                printf("\nInvalid choice: %d\n", choice);
                printf("Please enter a number between 0 and 4.\n");
                break;
        }
    }
    
    dataset_free(dataset);
    return 0;
}
