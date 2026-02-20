/**
 * @file calculate.c
 * @brief 数学计算模块实现
 */

#include "calculate.h"
#include <float.h>

/* ========== 数学辅助函数实现 ========== */

double math_mean(const double* arr, int count) {
    if (arr == NULL || count <= 0) {
        return 0.0;
    }
    
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += arr[i];
    }
    
    return sum / count;
}

double math_std(const double* arr, int count) {
    if (arr == NULL || count <= 0) {
        return 0.0;
    }
    
    double mean = math_mean(arr, count);
    double sum_sq = 0.0;
    
    for (int i = 0; i < count; i++) {
        double diff = arr[i] - mean;
        sum_sq += diff * diff;
    }
    
    // 总体标准差 (ddof=0)
    return sqrt(sum_sq / count);
}

double math_linear_regression(const double* x, const double* y, int count, double* intercept) {
    if (x == NULL || y == NULL || count <= 1 || intercept == NULL) {
        *intercept = 0.0;
        return 0.0;
    }
    
    double x_mean = math_mean(x, count);
    double y_mean = math_mean(y, count);
    
    double numerator = 0.0;
    double denominator = 0.0;
    
    for (int i = 0; i < count; i++) {
        double x_diff = x[i] - x_mean;
        numerator += x_diff * (y[i] - y_mean);
        denominator += x_diff * x_diff;
    }
    
    if (fabs(denominator) < DBL_EPSILON) {
        *intercept = y_mean;
        return 0.0;
    }
    
    double slope = numerator / denominator;
    *intercept = y_mean - slope * x_mean;
    
    return slope;
}

double math_safe_log(double x) {
    if (x <= 0) {
        return log(DBL_MIN);
    }
    return log(x);
}

double math_safe_exp(double x) {
    if (x > 700) {  // 避免溢出
        return DBL_MAX;
    }
    if (x < -700) {
        return DBL_MIN;
    }
    return exp(x);
}

/* ========== 回归通道函数实现 ========== */

RegressionChannel* channel_create(int count) {
    if (count <= 0) {
        return NULL;
    }
    
    RegressionChannel* ch = (RegressionChannel*)malloc(sizeof(RegressionChannel));
    if (ch == NULL) {
        return NULL;
    }
    
    // 分配所有数组
    ch->center_line = (double*)calloc(count, sizeof(double));
    ch->upper_0618 = (double*)calloc(count, sizeof(double));
    ch->lower_0618 = (double*)calloc(count, sizeof(double));
    ch->upper_1_0 = (double*)calloc(count, sizeof(double));
    ch->lower_1_0 = (double*)calloc(count, sizeof(double));
    ch->upper_1_5 = (double*)calloc(count, sizeof(double));
    ch->lower_1_5 = (double*)calloc(count, sizeof(double));
    ch->upper_2_0 = (double*)calloc(count, sizeof(double));
    ch->lower_2_0 = (double*)calloc(count, sizeof(double));
    ch->ohlc4 = (double*)calloc(count, sizeof(double));
    ch->log_ohlc4 = (double*)calloc(count, sizeof(double));
    ch->deviation = (double*)calloc(count, sizeof(double));
    
    // 检查分配是否成功
    if (ch->center_line == NULL || ch->upper_0618 == NULL || 
        ch->lower_0618 == NULL || ch->upper_1_0 == NULL ||
        ch->lower_1_0 == NULL || ch->upper_1_5 == NULL ||
        ch->lower_1_5 == NULL || ch->upper_2_0 == NULL ||
        ch->lower_2_0 == NULL || ch->ohlc4 == NULL ||
        ch->log_ohlc4 == NULL || ch->deviation == NULL) {
        channel_free(ch);
        return NULL;
    }
    
    ch->count = count;
    ch->slope = 0.0;
    ch->intercept = 0.0;
    ch->std_dev = 0.0;
    
    return ch;
}

void channel_free(RegressionChannel* channel) {
    if (channel == NULL) {
        return;
    }
    
    free(channel->center_line);
    free(channel->upper_0618);
    free(channel->lower_0618);
    free(channel->upper_1_0);
    free(channel->lower_1_0);
    free(channel->upper_1_5);
    free(channel->lower_1_5);
    free(channel->upper_2_0);
    free(channel->lower_2_0);
    free(channel->ohlc4);
    free(channel->log_ohlc4);
    free(channel->deviation);
    free(channel);
}

RegressionChannel* channel_calculate_log(const Dataset* dataset) {
    if (dataset == NULL || dataset->count == 0) {
        return NULL;
    }
    
    int n = dataset->count;
    RegressionChannel* ch = channel_create(n);
    if (ch == NULL) {
        return NULL;
    }
    
    // 创建X数组（0, 1, 2, ..., n-1）
    double* x = (double*)malloc(n * sizeof(double));
    if (x == NULL) {
        channel_free(ch);
        return NULL;
    }
    
    for (int i = 0; i < n; i++) {
        x[i] = (double)i;
    }
    
    // 计算OHLC4和对数OHLC4
    for (int i = 0; i < n; i++) {
        const Candle* c = &dataset->candles[i];
        ch->ohlc4[i] = (c->open + c->high + c->low + c->close) / 4.0;
        ch->log_ohlc4[i] = math_safe_log(ch->ohlc4[i]);
    }
    
    // 计算对数空间的线性回归
    ch->slope = math_linear_regression(x, ch->log_ohlc4, n, &ch->intercept);
    
    // 计算标准差
    ch->std_dev = math_std(ch->log_ohlc4, n);
    
    // 计算通道线
    for (int i = 0; i < n; i++) {
        double log_center = ch->intercept + ch->slope * x[i];
        
        ch->center_line[i] = math_safe_exp(log_center);
        ch->upper_0618[i] = math_safe_exp(log_center + SIGMA_0618 * ch->std_dev);
        ch->lower_0618[i] = math_safe_exp(log_center - SIGMA_0618 * ch->std_dev);
        ch->upper_1_0[i] = math_safe_exp(log_center + SIGMA_1_0 * ch->std_dev);
        ch->lower_1_0[i] = math_safe_exp(log_center - SIGMA_1_0 * ch->std_dev);
        ch->upper_1_5[i] = math_safe_exp(log_center + SIGMA_1_5 * ch->std_dev);
        ch->lower_1_5[i] = math_safe_exp(log_center - SIGMA_1_5 * ch->std_dev);
        ch->upper_2_0[i] = math_safe_exp(log_center + SIGMA_2_0 * ch->std_dev);
        ch->lower_2_0[i] = math_safe_exp(log_center - SIGMA_2_0 * ch->std_dev);
        
        const Candle* c = &dataset->candles[i];
        double log_close = math_safe_log(c->close);
        ch->deviation[i] = (log_close - log_center) / ch->std_dev;
    }
    
    free(x);
    return ch;
}

RegressionChannel* channel_calculate_linear(const Dataset* dataset) {
    if (dataset == NULL || dataset->count == 0) {
        return NULL;
    }
    
    int n = dataset->count;
    RegressionChannel* ch = channel_create(n);
    if (ch == NULL) {
        return NULL;
    }
    
    // 创建X数组（通达信风格：1, 2, 3, ..., n）
    double* x = (double*)malloc(n * sizeof(double));
    if (x == NULL) {
        channel_free(ch);
        return NULL;
    }
    
    for (int i = 0; i < n; i++) {
        x[i] = (double)(i + 1);
    }
    
    // 计算OHLC4
    for (int i = 0; i < n; i++) {
        const Candle* c = &dataset->candles[i];
        ch->ohlc4[i] = (c->open + c->high + c->low + c->close) / 4.0;
        ch->log_ohlc4[i] = ch->ohlc4[i];
    }
    
    double x_mean = (n + 1.0) / 2.0;
    double ohlc4_mean = math_mean(ch->ohlc4, n);
    
    double numerator = 0.0;
    double denominator = 0.0;
    for (int i = 0; i < n; i++) {
        double x_diff = x[i] - x_mean;
        numerator += x_diff * (ch->ohlc4[i] - ohlc4_mean);
        denominator += x_diff * x_diff;
    }
    ch->slope = numerator / denominator;
    ch->intercept = ohlc4_mean;
    
    ch->std_dev = math_std(ch->ohlc4, n);
    
    double last_x = x[n - 1];
    double last_center = ohlc4_mean + (last_x - x_mean) * ch->slope;
    
    for (int i = 0; i < n; i++) {
        double offset = (x[i] - last_x) * ch->slope;
        
        ch->center_line[i] = last_center + offset;
        ch->upper_0618[i] = last_center + SIGMA_0618 * ch->std_dev + offset;
        ch->lower_0618[i] = last_center - SIGMA_0618 * ch->std_dev + offset;
        ch->upper_1_0[i] = last_center + SIGMA_1_0 * ch->std_dev + offset;
        ch->lower_1_0[i] = last_center - SIGMA_1_0 * ch->std_dev + offset;
        ch->upper_1_5[i] = last_center + SIGMA_1_5 * ch->std_dev + offset;
        ch->lower_1_5[i] = last_center - SIGMA_1_5 * ch->std_dev + offset;
        ch->upper_2_0[i] = last_center + SIGMA_2_0 * ch->std_dev + offset;
        ch->lower_2_0[i] = last_center - SIGMA_2_0 * ch->std_dev + offset;
        
        const Candle* c = &dataset->candles[i];
        ch->deviation[i] = (c->close - ch->center_line[i]) / ch->std_dev;
    }
    
    free(x);
    return ch;
}

const char* channel_get_deviation_label(const RegressionChannel* channel, int index) {
    if (channel == NULL || index < 0 || index >= channel->count) {
        return "Unknown";
    }
    
    double dev = channel->deviation[index];
    
    if (dev > 2.0) {
        return "Extreme Overbought (+2s)";
    } else if (dev > 1.5) {
        return "Overbought (+1.5s~+2s)";
    } else if (dev > 1.0) {
        return "Strong (+1s~+1.5s)";
    } else if (dev > 0.618) {
        return "Bullish (+0.618s~+1s)";
    } else if (dev > -0.618) {
        return "Neutral (-0.618s~+0.618s)";
    } else if (dev > -1.0) {
        return "Bearish (-1s~-0.618s)";
    } else if (dev > -1.5) {
        return "Weak (-1.5s~-1s)";
    } else if (dev > -2.0) {
        return "Oversold (-2s~-1.5s)";
    } else {
        return "Extreme Oversold (-2s)";
    }
}

void channel_print_params(const RegressionChannel* channel) {
    if (channel == NULL) {
        printf("通道数据为空\n");
        return;
    }
    
    printf("\n========== 回归参数 ==========\n");
    printf("斜率 (Slope): %.6f\n", channel->slope);
    printf("截距 (Intercept): %.2f\n", channel->intercept);
    printf("标准差 (Std Dev): %.4f\n", channel->std_dev);
    printf("数据点数: %d\n", channel->count);
    printf("==============================\n");
}

void channel_print_last_values(const RegressionChannel* channel, const Dataset* dataset) {
    if (channel == NULL || dataset == NULL || channel->count == 0) {
        return;
    }
    
    int last = channel->count - 1;
    
    printf("\n========== 最后数据点通道值 ==========\n");
    printf("收盘价: $%.2f\n", dataset->candles[last].close);
    printf("中心线: $%.2f\n", channel->center_line[last]);
    printf("+2.0s: $%.2f\n", channel->upper_2_0[last]);
    printf("-2.0s: $%.2f\n", channel->lower_2_0[last]);
    printf("偏离度: %+.2fs (%s)\n", 
           channel->deviation[last],
           channel_get_deviation_label(channel, last));
    printf("======================================\n");
}
