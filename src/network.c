/**
 * @file network.c
 * @brief 数据更新模块 - 调用Python脚本从Binance API获取数据
 */

#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ========== 内部辅助函数 ========== */

/**
 * @brief 去除字符串首尾空白
 */
static char* trim_str(char* str) {
    char* end;
    
    while (isspace((unsigned char)*str)) {
        str++;
    }
    
    if (*str == '\0') {
        return str;
    }
    
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    
    return str;
}

/**
 * @brief 从CSV第80行开始，读取前6列并导出到txt文件
 */
void export_csv_slice(const char* csv_path, const char* output_path, int start_line) {
    FILE* fp_in = fopen(csv_path, "r");
    if (fp_in == NULL) {
        printf("错误: 无法打开 %s\n", csv_path);
        return;
    }
    
    FILE* fp_out = fopen(output_path, "w");
    if (fp_out == NULL) {
        printf("错误: 无法创建 %s\n", output_path);
        fclose(fp_in);
        return;
    }
    
    char line[1024];
    int line_num = 0;
    int export_count = 0;
    
    while (fgets(line, sizeof(line), fp_in) != NULL) {
        line_num++;
        if (line_num < start_line) continue;
        
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;
        
        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        
        char* tokens[6];
        int token_count = 0;
        char* ptr = line_copy;
        
        while (token_count < 6) {
            while (*ptr && isspace((unsigned char)*ptr)) ptr++;
            if (*ptr == '\0') break;
            tokens[token_count++] = ptr;
            while (*ptr && *ptr != ',') ptr++;
            if (*ptr == ',') { *ptr = '\0'; ptr++; }
        }
        if (token_count < 6) continue;
        
        for (int i = 0; i < 6; i++) {
            tokens[i] = trim_str(tokens[i]);
        }
        
        for (int i = 0; i < 6; i++) {
            fprintf(fp_out, "%s", tokens[i]);
            if (i < 5) fprintf(fp_out, ",");
        }
        fprintf(fp_out, "\n");
        export_count++;
    }
    
    fclose(fp_in);
    fclose(fp_out);
    
    printf("已导出从第%d行开始的 %d 条记录到: %s\n", start_line, export_count, output_path);
}

/* ========== 公共函数实现 ========== */

Dataset* update_data(const char* filepath) {
    printf("\n============================================================\n");
    printf("数据更新\n");
    printf("============================================================\n");
    
    int result = system("python data\\update_data.py");
    
    if (result == 0) {
        printf("数据更新成功!\n");
    } else {
        printf("数据更新失败 (返回码: %d)\n", result);
        printf("请确保已安装: pip install pandas requests\n");
    }
    
    printf("============================================================\n\n");
    
    Dataset* dataset = dataset_load_csv(filepath);
    
    if (dataset) {
        printf("加载数据: %d 条记录\n", dataset->count);
        if (dataset->count > 0) {
            printf("最新日期: %s\n", dataset->candles[dataset->count-1].date);
            printf("最新收盘价: $%.2f\n", dataset->candles[dataset->count-1].close);
        }
    }
    
    return dataset;
}

/* ========== 指标数据更新 ========== */

void update_indicators(void) {
    printf("\n============================================================\n");
    printf("更新链上指标数据 (MVRV Z-Score, CVDD)\n");
    printf("============================================================\n");
    
    int result = system("python data\\update_indicators.py");
    
    if (result == 0) {
        printf("指标数据更新成功!\n");
    } else {
        printf("指标数据更新失败 (返回码: %d)\n", result);
        printf("请确保已安装: pip install playwright && python -m playwright install chromium\n");
    }
    
    printf("============================================================\n\n");
}

/* ========== BTC 收盘价 - CVDD 差值计算 ========== */

void compute_close_minus_cvdd(void) {
    printf("\n============================================================\n");
    printf("计算 BTC 收盘价 - CVDD 差值\n");
    printf("============================================================\n");
    
    int result = system("python data\\compute_close_minus_cvdd.py");
    
    if (result == 0) {
        printf("差值计算完成 (或数据已是最新，无需计算)\n");
    } else {
        printf("差值计算失败 (返回码: %d)\n", result);
    }
    
    printf("============================================================\n\n");
}

void compute_cmcvdd_percentile(void) {
    printf("\n============================================================\n");
    printf("计算 C-CVDD 历史百分位\n");
    printf("============================================================\n");
    
    int result = system("python data\\compute_cmcvdd_percentile.py");
    
    if (result == 0) {
        printf("百分位计算完成 (或数据已是最新，无需计算)\n");
    } else {
        printf("百分位计算失败 (返回码: %d)\n", result);
    }
    
    printf("============================================================\n\n");
}

