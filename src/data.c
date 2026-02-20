/**
 * @file data.c
 * @brief CSV解析和数据管理实现
 */

#include "data.h"
#include <ctype.h>

/* ========== 内部辅助函数 ========== */

/**
 * @brief 去除字符串首尾空白
 */
static char* trim(char* str) {
    char* end;
    
    // 去除前导空白
    while (isspace((unsigned char)*str)) {
        str++;
    }
    
    if (*str == '\0') {
        return str;
    }
    
    // 去除尾部空白
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    
    return str;
}

/**
 * @brief 解析CSV行
 */
static int parse_csv_line(char* line, char* tokens[], int max_tokens) {
    int count = 0;
    char* token = strtok(line, ",\n\r");
    
    while (token != NULL && count < max_tokens) {
        tokens[count++] = trim(token);
        token = strtok(NULL, ",\n\r");
    }
    
    return count;
}

/* ========== 公共函数实现 ========== */

Dataset* dataset_create(void) {
    Dataset* ds = (Dataset*)malloc(sizeof(Dataset));
    if (ds == NULL) {
        return NULL;
    }
    
    ds->candles = (Candle*)malloc(sizeof(Candle) * INITIAL_CAPACITY);
    if (ds->candles == NULL) {
        free(ds);
        return NULL;
    }
    
    ds->count = 0;
    ds->capacity = INITIAL_CAPACITY;
    
    return ds;
}

void dataset_free(Dataset* dataset) {
    if (dataset != NULL) {
        if (dataset->candles != NULL) {
            free(dataset->candles);
        }
        free(dataset);
    }
}

Dataset* dataset_load_csv(const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    if (fp == NULL) {
        fprintf(stderr, "错误: 无法打开文件 %s\n", filepath);
        return NULL;
    }
    
    Dataset* ds = dataset_create();
    if (ds == NULL) {
        fclose(fp);
        return NULL;
    }
    
    char line[MAX_LINE_LEN];
    char* tokens[10];
    int line_num = 0;
    
    // 跳过标题行
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        dataset_free(ds);
        return NULL;
    }
    line_num++;
    
    // 读取数据行
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_num++;
        
        int token_count = parse_csv_line(line, tokens, 10);
        if (token_count < 7) {
            fprintf(stderr, "警告: 第%d行数据不完整，跳过\n", line_num);
            continue;
        }
        
        // 检查是否需要扩容
        if (ds->count >= ds->capacity) {
            int new_capacity = ds->capacity * 2;
            Candle* new_candles = (Candle*)realloc(ds->candles, 
                                                    sizeof(Candle) * new_capacity);
            if (new_candles == NULL) {
                fprintf(stderr, "错误: 内存分配失败\n");
                fclose(fp);
                dataset_free(ds);
                return NULL;
            }
            ds->candles = new_candles;
            ds->capacity = new_capacity;
        }
        
        // 填充数据
        Candle* c = &ds->candles[ds->count];
        strncpy(c->date, tokens[0], MAX_DATE_LEN - 1);
        c->date[MAX_DATE_LEN - 1] = '\0';
        
        parse_date(c->date, &c->year, &c->month);
        
        c->open = atof(tokens[1]);
        c->high = atof(tokens[2]);
        c->low = atof(tokens[3]);
        c->close = atof(tokens[4]);
        c->volume = atof(tokens[5]);
        c->quote_volume = (token_count > 6) ? atof(tokens[6]) : 0.0;
        
        ds->count++;
    }
    
    fclose(fp);
    printf("成功加载 %d 条数据\n", ds->count);
    
    return ds;
}

int parse_date(const char* date_str, int* year, int* month) {
    if (date_str == NULL || year == NULL || month == NULL) {
        return -1;
    }
    
    // 格式: "YYYY-MM-DD"
    if (sscanf(date_str, "%d-%d-%*d", year, month) == 2) {
        return 0;
    }
    
    return -1;
}

Dataset* dataset_filter_by_date(const Dataset* src, const DateFilter* filter) {
    if (src == NULL || filter == NULL) {
        return NULL;
    }
    
    Dataset* result = dataset_create();
    if (result == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < src->count; i++) {
        const Candle* c = &src->candles[i];
        
        // 检查日期范围
        int in_range = 1;
        
        if (c->year < filter->start_year || 
            (c->year == filter->start_year && c->month < filter->start_month)) {
            in_range = 0;
        }
        
        if (filter->end_year > 0) {
            if (c->year > filter->end_year || 
                (c->year == filter->end_year && c->month > filter->end_month)) {
                in_range = 0;
            }
        }
        
        if (in_range) {
            // 扩容检查
            if (result->count >= result->capacity) {
                int new_capacity = result->capacity * 2;
                Candle* new_candles = (Candle*)realloc(result->candles,
                                                        sizeof(Candle) * new_capacity);
                if (new_candles == NULL) {
                    dataset_free(result);
                    return NULL;
                }
                result->candles = new_candles;
                result->capacity = new_capacity;
            }
            
            result->candles[result->count++] = *c;
        }
    }
    
    return result;
}

Dataset* dataset_filter_from_year(const Dataset* src, int start_year) {
    DateFilter filter = {
        .start_year = start_year,
        .start_month = 1,
        .end_year = 0,
        .end_month = 0
    };
    return dataset_filter_by_date(src, &filter);
}

void dataset_print_info(const Dataset* dataset) {
    if (dataset == NULL) {
        printf("数据集为空\n");
        return;
    }
    
    printf("\n========== 数据集信息 ==========\n");
    printf("数据条数: %d\n", dataset->count);
    
    if (dataset->count > 0) {
        int start_year, end_year;
        dataset_get_date_range(dataset, &start_year, &end_year);
        printf("时间范围: %d 至 %d\n", start_year, end_year);
        printf("最新收盘价: $%.2f\n", dataset->candles[dataset->count - 1].close);
        printf("历史最高价: $%.2f\n", dataset_get_highest(dataset));
        printf("历史最低价: $%.2f\n", dataset_get_lowest(dataset));
    }
    printf("================================\n\n");
}

void dataset_get_date_range(const Dataset* dataset, int* start_year, int* end_year) {
    if (dataset == NULL || dataset->count == 0) {
        *start_year = 0;
        *end_year = 0;
        return;
    }
    
    *start_year = dataset->candles[0].year;
    *end_year = dataset->candles[dataset->count - 1].year;
}

double dataset_get_highest(const Dataset* dataset) {
    if (dataset == NULL || dataset->count == 0) {
        return 0.0;
    }
    
    double highest = dataset->candles[0].high;
    for (int i = 1; i < dataset->count; i++) {
        if (dataset->candles[i].high > highest) {
            highest = dataset->candles[i].high;
        }
    }
    
    return highest;
}

double dataset_get_lowest(const Dataset* dataset) {
    if (dataset == NULL || dataset->count == 0) {
        return 0.0;
    }
    
    double lowest = dataset->candles[0].low;
    for (int i = 1; i < dataset->count; i++) {
        if (dataset->candles[i].low < lowest) {
            lowest = dataset->candles[i].low;
        }
    }
    
    return lowest;
}
