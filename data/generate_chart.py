"""
比特币图表生成脚本
生成K线+回归通道图表并保存为PNG图片
"""

import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import matplotlib
matplotlib.use('Agg')

script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

DATA_FILE = 'btc_historical_monthly.csv'
OUTPUT_FILE = 'btc_chart.png'

def generate_chart():
    """生成图表"""
    print("正在生成图表...")
    
    # 读取数据
    df = pd.read_csv(DATA_FILE)
    df['time'] = pd.to_datetime(df['time'])
    
    # 筛选2017年以后的数据
    df = df[df['time'] >= '2017-01-01'].reset_index(drop=True)
    
    print(f"数据条数: {len(df)}")
    print(f"时间范围: {df['time'].min().strftime('%Y-%m')} 至 {df['time'].max().strftime('%Y-%m')}")
    
    # 计算OHLC4
    OHLC4 = (df['Open'] + df['High'] + df['Low'] + df['Close']) / 4
    
    # 线性回归参数
    N = len(df)
    X = np.arange(1, N+1)
    X_mean = np.mean(X)
    OHLC4_mean = np.mean(OHLC4)
    SL = np.sum((X - X_mean) * (OHLC4 - OHLC4_mean)) / np.sum((X - X_mean) ** 2)
    VSTD = np.std(OHLC4, ddof=0)
    
    # 通道线
    回归线 = OHLC4_mean + (X - (N + 1) / 2) * SL
    最后一个回归值 = 回归线[-1]
    
    中心线 = 最后一个回归值 + SL * (X - N)
    line_0618_p = 最后一个回归值 + 0.618 * VSTD + SL * (X - N)
    line_0618_m = 最后一个回归值 - 0.618 * VSTD + SL * (X - N)
    line_10_p = 最后一个回归值 + 1.0 * VSTD + SL * (X - N)
    line_10_m = 最后一个回归值 - 1.0 * VSTD + SL * (X - N)
    line_15_p = 最后一个回归值 + 1.5 * VSTD + SL * (X - N)
    line_15_m = 最后一个回归值 - 1.5 * VSTD + SL * (X - N)
    line_20_p = 最后一个回归值 + 2.0 * VSTD + SL * (X - N)
    line_20_m = 最后一个回归值 - 2.0 * VSTD + SL * (X - N)
    
    # 偏离度
    df['偏离度'] = (df['Close'] - 中心线) / VSTD
    
    # 创建图表
    fig, (ax, ax_dev) = plt.subplots(2, 1, figsize=(16, 10), gridspec_kw={'height_ratios': [3, 1]})
    
    # 绘制K线
    for i in range(len(df)):
        open_p = df['Open'].iloc[i]
        high_p = df['High'].iloc[i]
        low_p = df['Low'].iloc[i]
        close_p = df['Close'].iloc[i]
        
        color = 'red' if close_p >= open_p else 'green'
        body_bottom = open_p if close_p >= open_p else close_p
        body_height = abs(close_p - open_p)
        if body_height < open_p * 0.001:
            body_height = open_p * 0.005
        
        rect = Rectangle((i-0.4, body_bottom), 0.8, body_height, 
                         facecolor=color, edgecolor=color, alpha=0.8)
        ax.add_patch(rect)
        ax.plot([i, i], [low_p, high_p], color=color, linewidth=1)
    
    # 绘制通道线
    ax.plot(X-1, 中心线, '-', color='red', linewidth=2, label='Center')
    ax.plot(X-1, line_0618_p, '-', color='lightblue', linewidth=1, alpha=0.8)
    ax.plot(X-1, line_0618_m, '-', color='lightblue', linewidth=1, label='±0.618σ', alpha=0.8)
    ax.plot(X-1, line_10_p, '-', color=(0, 0.7, 0.7), linewidth=1, alpha=0.8)
    ax.plot(X-1, line_10_m, '-', color=(0, 0.7, 0.7), linewidth=1, label='±1.0σ', alpha=0.8)
    ax.plot(X-1, line_15_p, '-', color='green', linewidth=1.5, alpha=0.8)
    ax.plot(X-1, line_15_m, '-', color='green', linewidth=1.5, label='±1.5σ', alpha=0.8)
    ax.plot(X-1, line_20_p, '-', color='purple', linewidth=2, alpha=0.8)
    ax.plot(X-1, line_20_m, '-', color='purple', linewidth=2, label='±2.0σ', alpha=0.8)
    
    # X轴标签
    year_positions = []
    year_labels = []
    for i in range(len(df)):
        if df['time'].iloc[i].month == 1:
            year_positions.append(i)
            year_labels.append(str(df['time'].iloc[i].year))
    ax.set_xticks(year_positions)
    ax.set_xticklabels(year_labels)
    
    # 标记当前价格
    last_idx = len(df) - 1
    last_price = df['Close'].iloc[-1]
    ax.scatter([last_idx], [last_price], color='red', s=150, zorder=15)
    
    deviation = df['偏离度'].iloc[-1]
    ax.annotate(f'Current: ${last_price:,.0f}\nDeviation: {deviation:+.2f}σ', 
                xy=(last_idx, last_price), 
                xytext=(last_idx-10, last_price*1.1),
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.9),
                fontsize=10)
    
    ax.set_xlim(-2, len(df)+2)
    ax.set_ylim(0, df['High'].max() * 1.1)
    ax.set_ylabel('Price (USD)', fontsize=12)
    ax.set_title('Bitcoin Monthly K-Line + Linear Regression Channel (2017-Present)', fontsize=14)
    ax.grid(True, alpha=0.3)
    ax.legend(loc='upper left', fontsize=9)
    
    # 副图：偏离度
    ax_dev.plot(range(len(df)), df['偏离度'], linewidth=1, color='white')
    ax_dev.axhline(y=0, color='red', linestyle='--', linewidth=1)
    ax_dev.axhline(y=1, color='cyan', linestyle='--', linewidth=0.8)
    ax_dev.axhline(y=-1, color='cyan', linestyle='--', linewidth=0.8)
    ax_dev.axhline(y=2, color='purple', linestyle='--', linewidth=0.8)
    ax_dev.axhline(y=-2, color='purple', linestyle='--', linewidth=0.8)
    ax_dev.set_xticks(year_positions)
    ax_dev.set_xticklabels(year_labels)
    ax_dev.set_ylim(-3, 3)
    ax_dev.set_xlabel('Year', fontsize=12)
    ax_dev.set_ylabel('Deviation (σ)', fontsize=12)
    ax_dev.grid(True, alpha=0.3)
    ax_dev.set_facecolor('#14141e')
    
    ax.set_facecolor('#14141e')
    fig.patch.set_facecolor('#14141e')
    ax.tick_params(colors='white')
    ax_dev.tick_params(colors='white')
    ax.xaxis.label.set_color('white')
    ax.yaxis.label.set_color('white')
    ax_dev.xaxis.label.set_color('white')
    ax_dev.yaxis.label.set_color('white')
    ax.title.set_color('white')
    
    plt.tight_layout()
    plt.savefig(OUTPUT_FILE, dpi=150, facecolor='#14141e', bbox_inches='tight')
    plt.close()
    
    print(f"\n图表已保存: {OUTPUT_FILE}")
    print(f"最新价格: ${last_price:,.2f}")
    print(f"偏离度: {deviation:+.2f}σ")

if __name__ == '__main__':
    try:
        generate_chart()
    except Exception as e:
        print(f"生成失败: {e}")
        import traceback
        traceback.print_exc()
