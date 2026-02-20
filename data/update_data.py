"""
比特币数据更新脚本
从Binance API获取最新月线数据，与本地数据合并并保存
参考 D:\openskills\BTC\menu.py 的更新逻辑
"""

import os
import sys

script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

DATA_FILE = 'btc_historical_monthly.csv'
API_URL = 'https://api.binance.com/api/v3/klines'

def update_data():
    """更新数据"""
    print("=" * 60)
    print("更新比特币数据")
    print("=" * 60)
    print("\n正在从Binance API获取最新数据...")
    
    import requests
    import pandas as pd
    
    # 读取现有数据
    df_existing = pd.read_csv(DATA_FILE)
    df_existing['time'] = pd.to_datetime(df_existing['time'])
    last_date = df_existing['time'].max()
    
    print(f"\n现有数据最后日期: {last_date.strftime('%Y-%m-%d')}")
    
    # 获取新数据
    params = {
        "symbol": "BTCUSDT",
        "interval": "1M",
        "limit": 100
    }
    
    response = requests.get(API_URL, params=params, timeout=30)
    data = response.json()
    
    # 处理数据
    new_data = []
    for item in data:
        date = pd.to_datetime(item[0], unit='ms')
        new_data.append({
            'time': date,
            'Open': float(item[1]),
            'High': float(item[2]),
            'Low': float(item[3]),
            'Close': float(item[4]),
            'Volume': float(item[5]),
            'Quote_Volume': float(item[7])
        })
    
    df_new = pd.DataFrame(new_data)
    
    # 获取年月用于比较
    last_year_month = (last_date.year, last_date.month)
    api_latest_row = df_new.loc[df_new['time'].idxmax()]
    api_year_month = (api_latest_row['time'].year, api_latest_row['time'].month)
    
    # 检查是否有新月份的数据
    if api_year_month > last_year_month:
        # 有新月份的数据
        new_months_data = df_new[
            (df_new['time'].dt.year > last_date.year) | 
            ((df_new['time'].dt.year == last_date.year) & (df_new['time'].dt.month > last_date.month))
        ]
        df_combined = pd.concat([df_existing, new_months_data], ignore_index=True)
        df_combined = df_combined.drop_duplicates(subset=['time'], keep='last')
        df_combined = df_combined.sort_values('time')
        
        # 保存
        df_combined.to_csv(DATA_FILE, index=False)
        
        print(f"\n更新成功!")
        print(f"新增数据: {len(new_months_data)} 条")
        print(f"最新日期: {df_combined['time'].max().strftime('%Y-%m-%d')}")
        print(f"最新价格: ${df_combined['Close'].iloc[-1]:,.2f}")
        
    elif api_year_month == last_year_month:
        # 当月数据已存在，更新价格
        current_row = df_existing[df_existing['time'] == last_date].iloc[0]
        new_row = api_latest_row
        
        print(f"\n检测到当月数据（{last_date.strftime('%Y-%m-%d')}）")
        print(f"当前收盘价: ${current_row['Close']:,.2f}")
        print(f"API最新价:  ${new_row['Close']:,.2f}")
        
        if current_row['Close'] != new_row['Close']:
            print(f"价格有变化（差值: ${new_row['Close'] - current_row['Close']:,.2f}）")
        
        # 强制更新当月数据
        print(f"\n正在更新当月数据...")
        
        # 替换现有数据中的当月记录
        df_existing = df_existing[df_existing['time'] != last_date]
        
        # 创建更新后的当月数据行
        updated_row = pd.DataFrame([{
            'time': last_date,
            'Open': new_row['Open'],
            'High': new_row['High'],
            'Low': new_row['Low'],
            'Close': new_row['Close'],
            'Volume': new_row['Volume'],
            'Quote_Volume': new_row['Quote_Volume']
        }])
        
        df_combined = pd.concat([df_existing, updated_row], ignore_index=True)
        df_combined = df_combined.sort_values('time')
        
        # 保存
        df_combined.to_csv(DATA_FILE, index=False)
        
        print(f"\n当月数据已更新!")
        print(f"最新日期: {df_combined['time'].max().strftime('%Y-%m-%d')}")
        print(f"最新收盘价: ${df_combined['Close'].iloc[-1]:,.2f}")
        print(f"最高价: ${new_row['High']:,.2f}")
        print(f"最低价: ${new_row['Low']:,.2f}")
    else:
        print(f"\n数据已是最新，无需更新")
    
    print("=" * 60)

if __name__ == '__main__':
    try:
        update_data()
    except Exception as e:
        print(f"\n更新失败: {e}")
        import traceback
        traceback.print_exc()
