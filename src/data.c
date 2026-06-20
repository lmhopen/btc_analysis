/**
 * @file data.c
 * @brief CSV解析和数据管理实现
 */

#include "data.h"
#include <ctype.h>

/* ========== 指标序列常量 ========== */
#define INDICATOR_INITIAL_CAPACITY 200
#define INDICATOR_LINE_LEN 64

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

/* ========== 周期聚合函数 ========== */

/**
 * @brief 获取给定年月所属的季度 (1-4)
 */
static int month_to_quarter(int month) {
    return (month - 1) / 3 + 1;
}

/**
 * @brief 获取季度起始月份 (1, 4, 7, 10)
 */
static int quarter_start_month(int quarter) {
    return (quarter - 1) * 3 + 1;
}

/**
 * @brief 计算两个日期的周期键值
 * 月线: year*100 + month  (如 202401)
 * 季线: year*100 + quarter (如 20241)
 */
static int period_key(const Candle* c, PeriodType period) {
    if (period == PERIOD_MONTHLY) {
        return c->year * 100 + c->month;
    } else if (period == PERIOD_QUARTERLY) {
        return c->year * 10 + month_to_quarter(c->month);
    }
    return c->year * 10000 + c->month * 100;  // daily: unique per day
}

const char* period_to_string(PeriodType period) {
    switch (period) {
        case PERIOD_DAILY: return "日线";
        case PERIOD_MONTHLY: return "月线";
        case PERIOD_QUARTERLY: return "季线";
        default: return "未知";
    }
}

static void set_candle_date_by_period(Candle* c, int year, int month, PeriodType period) {
    if (period == PERIOD_MONTHLY) {
        snprintf(c->date, MAX_DATE_LEN, "%04d-%02d-01", year, month);
    } else if (period == PERIOD_QUARTERLY) {
        int qm = quarter_start_month(month_to_quarter(month));
        snprintf(c->date, MAX_DATE_LEN, "%04d-%02d-01", year, qm);
    }
}

Dataset* dataset_aggregate_to_period(const Dataset* src, PeriodType period) {
    if (src == NULL || src->count == 0) return NULL;
    if (period == PERIOD_DAILY) {
        // 日线直接返回副本
        Dataset* result = dataset_create();
        if (result == NULL) return NULL;
        for (int i = 0; i < src->count; i++) {
            if (result->count >= result->capacity) {
                int nc = result->capacity * 2;
                Candle* tmp = (Candle*)realloc(result->candles, sizeof(Candle) * nc);
                if (tmp == NULL) { dataset_free(result); return NULL; }
                result->candles = tmp;
                result->capacity = nc;
            }
            result->candles[result->count++] = src->candles[i];
        }
        return result;
    }
    
    Dataset* result = dataset_create();
    if (result == NULL) return NULL;
    
    int current_key = -1;
    int aggr_count = 0;
    Candle aggr;
    memset(&aggr, 0, sizeof(aggr));
    
    for (int i = 0; i < src->count; i++) {
        const Candle* c = &src->candles[i];
        int key = period_key(c, period);
        
        if (key != current_key && current_key != -1) {
            // 保存上一个聚合周期
            if (result->count >= result->capacity) {
                int nc = result->capacity * 2;
                Candle* tmp = (Candle*)realloc(result->candles, sizeof(Candle) * nc);
                if (tmp == NULL) { dataset_free(result); return NULL; }
                result->candles = tmp;
                result->capacity = nc;
            }
            result->candles[result->count++] = aggr;
            memset(&aggr, 0, sizeof(aggr));
            aggr_count = 0;
        }
        
        if (aggr_count == 0) {
            // 第一个交易日
            aggr = *c;
            set_candle_date_by_period(&aggr, c->year, c->month, period);
        } else {
            // 更新
            aggr.high = (c->high > aggr.high) ? c->high : aggr.high;
            aggr.low = (c->low < aggr.low) ? c->low : aggr.low;
            aggr.close = c->close;
            aggr.volume += c->volume;
            aggr.quote_volume += c->quote_volume;
        }
        aggr_count++;
        current_key = key;
    }
    
    // 保存最后一个周期
    if (aggr_count > 0) {
        if (result->count >= result->capacity) {
            int nc = result->capacity * 2;
            Candle* tmp = (Candle*)realloc(result->candles, sizeof(Candle) * nc);
            if (tmp == NULL) { dataset_free(result); return NULL; }
            result->candles = tmp;
            result->capacity = nc;
        }
        result->candles[result->count++] = aggr;
    }
    
    return result;
}

/* ========== 指标序列函数实现 ========== */

IndicatorSeries* indicator_create(void) {
    IndicatorSeries* ind = (IndicatorSeries*)malloc(sizeof(IndicatorSeries));
    if (ind == NULL) return NULL;
    
    ind->dates = (char**)malloc(sizeof(char*) * INDICATOR_INITIAL_CAPACITY);
    ind->values = (double*)malloc(sizeof(double) * INDICATOR_INITIAL_CAPACITY);
    if (ind->dates == NULL || ind->values == NULL) {
        free(ind->dates);
        free(ind->values);
        free(ind);
        return NULL;
    }
    
    ind->count = 0;
    ind->capacity = INDICATOR_INITIAL_CAPACITY;
    return ind;
}

void indicator_free(IndicatorSeries* ind) {
    if (ind == NULL) return;
    if (ind->dates) {
        for (int i = 0; i < ind->count; i++) {
            free(ind->dates[i]);
        }
        free(ind->dates);
    }
    free(ind->values);
    free(ind);
}

IndicatorSeries* indicator_load_txt(const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    if (fp == NULL) {
        fprintf(stderr, "警告: 无法打开指标文件 %s\n", filepath);
        return NULL;
    }
    
    IndicatorSeries* ind = indicator_create();
    if (ind == NULL) {
        fclose(fp);
        return NULL;
    }
    
    char line[INDICATOR_LINE_LEN];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';
        
        if (line[0] == '\0') continue;
        
        char date[MAX_DATE_LEN];
        double value;
        if (sscanf(line, "%10[^,],%lf", date, &value) != 2) continue;
        
        // 扩容
        if (ind->count >= ind->capacity) {
            int new_cap = ind->capacity * 2;
            char** new_dates = (char**)realloc(ind->dates, sizeof(char*) * new_cap);
            double* new_values = (double*)realloc(ind->values, sizeof(double) * new_cap);
            if (new_dates == NULL || new_values == NULL) {
                if (new_dates) ind->dates = new_dates;
                if (new_values) ind->values = new_values;
                break;
            }
            ind->dates = new_dates;
            ind->values = new_values;
            ind->capacity = new_cap;
        }
        
        ind->dates[ind->count] = strdup(date);
        ind->values[ind->count] = value;
        ind->count++;
    }
    
    fclose(fp);
    return ind;
}

bool indicator_get_value(const IndicatorSeries* ind, const char* date, double* out_value) {
    if (ind == NULL || date == NULL || out_value == NULL) return false;
    for (int i = 0; i < ind->count; i++) {
        if (strcmp(ind->dates[i], date) == 0) {
            *out_value = ind->values[i];
            return true;
        }
    }
    return false;
}

double indicator_get_min(const IndicatorSeries* ind) {
    if (ind == NULL || ind->count == 0) return 0.0;
    double min_val = ind->values[0];
    for (int i = 1; i < ind->count; i++) {
        if (ind->values[i] < min_val) min_val = ind->values[i];
    }
    return min_val;
}

double indicator_get_max(const IndicatorSeries* ind) {
    if (ind == NULL || ind->count == 0) return 0.0;
    double max_val = ind->values[0];
    for (int i = 1; i < ind->count; i++) {
        if (ind->values[i] > max_val) max_val = ind->values[i];
    }
    return max_val;
}
