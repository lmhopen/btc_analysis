"""
比特币日线数据更新脚本
从 Gate.io API 获取 BTC 日线数据（备用: OKX）
Gate.io 国内可直接访问，支持从2017年至今全量历史数据分页拉取
"""

import os
import sys
import time
import requests
import urllib3
import pandas as pd

script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

DATA_FILE = 'btc_historical_daily.csv'
GATEIO_BASE = 1483228800  # 2017-01-01 00:00:00 UTC


def _request_with_ssl_fallback(url, params, timeout=30):
    """Try verify=True first, fall back to verify=False on SSLError.
    Also bypass system proxy to avoid interference from proxy software."""
    proxies = {"http": None, "https": None}
    try:
        return requests.get(url, params=params, timeout=timeout, verify=True, proxies=proxies)
    except requests.exceptions.SSLError:
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        return requests.get(url, params=params, timeout=timeout, verify=False, proxies=proxies)


def fetch_gateio_daily(from_ts, to_ts=None, limit=1000, max_retries=2):
    url = 'https://api.gateio.ws/api/v4/spot/candlesticks'
    params = {
        'currency_pair': 'BTC_USDT',
        'interval': '1d',
        'limit': limit,
        'from': str(from_ts)
    }
    if to_ts:
        params['to'] = str(to_ts)

    for attempt in range(max_retries + 1):
        try:
            response = _request_with_ssl_fallback(url, params, timeout=30)
            if response.status_code != 200:
                print(f"  [失败] HTTP {response.status_code}: {response.text[:200]}")
                if attempt < max_retries:
                    wait = (attempt + 1) * 3
                    print(f"  等待 {wait} 秒重试 ({attempt+1}/{max_retries})...")
                    time.sleep(wait)
                continue

            items = response.json()
            if not isinstance(items, list) or len(items) == 0:
                print("  [失败] 数据为空")
                return None

            rows = []
            for item in items:
                ts = pd.to_datetime(int(item[0]), unit='s')
                rows.append({
                    'time': ts,
                    'Open': float(item[5]),
                    'High': float(item[3]),
                    'Low': float(item[4]),
                    'Close': float(item[2]),
                    'Volume': float(item[6]),       # 成交量(基础货币BTC)
                    'Quote_Volume': float(item[1])  # 成交额(计价货币USDT)
                })

            return pd.DataFrame(rows)

        except requests.exceptions.Timeout:
            print(f"  [超时] 请求超时 (attempt {attempt+1}/{max_retries+1})")
            if attempt < max_retries:
                wait = (attempt + 1) * 5
                print(f"  等待 {wait} 秒重试...")
                time.sleep(wait)
            else:
                print("  [失败] 已达最大重试次数")
                return None
        except (requests.exceptions.ConnectionError, OSError, requests.exceptions.RequestException) as e:
            print(f"  [连接错误] {e}")
            if attempt < max_retries:
                wait = (attempt + 1) * 5
                print(f"  等待 {wait} 秒重试...")
                time.sleep(wait)
            else:
                print("  [失败] 已达最大重试次数")
                return None


def fetch_gateio_all():
    print("\n--- 从 Gate.io 分页获取 2017 年至今日线 ---")
    all_dfs = []
    from_ts = GATEIO_BASE
    batch = 0
    BATCH_RANGE = 365 * 86400

    now_ts = int(pd.Timestamp.now().timestamp())

    while from_ts < now_ts:
        batch += 1
        to_ts = min(from_ts + BATCH_RANGE, now_ts)
        print(f"  批次 {batch}: {pd.to_datetime(from_ts, unit='s').strftime('%Y-%m-%d')} ~ "
              f"{pd.to_datetime(to_ts, unit='s').strftime('%Y-%m-%d')}")

        df = fetch_gateio_daily(from_ts=from_ts, to_ts=to_ts, limit=1000)
        if df is None:
            print("  [!] 请求失败，尝试切换到 OKX...")
            return None

        all_dfs.append(df)
        print(f"    -> 获取 {len(df)} 条")

        from_ts = to_ts

    if not all_dfs:
        return None

    result = pd.concat(all_dfs, ignore_index=True)
    result = result.drop_duplicates(subset=['time'], keep='last')
    result = result.sort_values('time').reset_index(drop=True)
    return result


def fetch_okx_daily(after_ts=None, limit=300):
    url = 'https://www.okx.com/api/v5/market/candles'
    params = {'instId': 'BTC-USDT', 'bar': '1D', 'limit': limit}
    if after_ts:
        params['after'] = str(int(after_ts))

    response = _request_with_ssl_fallback(url, params, timeout=30)
    if response.status_code != 200:
        return None

    data = response.json()
    if data.get('code') != '0' or not data.get('data'):
        return None

    rows = []
    for item in data['data']:
        rows.append({
            'time': pd.to_datetime(int(item[0]), unit='ms'),
            'Open': float(item[1]),
            'High': float(item[2]),
            'Low': float(item[3]),
            'Close': float(item[4]),
            'Volume': float(item[5]),
            'Quote_Volume': float(item[6])
        })

    return pd.DataFrame(rows)


def fill_from_okx(df):
    print("\n--- 用 OKX 补充最新数据 ---")
    okx_dfs = [df]
    after = None

    for i in range(5):
        print(f"  批次 {i+1}")
        okx_df = fetch_okx_daily(after_ts=after)
        if okx_df is None:
            print("  [!] 请求失败")
            break

        okx_dfs.append(okx_df)
        print(f"    -> {okx_df['time'].min().strftime('%Y-%m-%d')} ~ {okx_df['time'].max().strftime('%Y-%m-%d')}")

        oldest = okx_df['time'].min()
        if oldest <= df['time'].max():
            print("  已覆盖现有数据范围，结束")
            break

        after = int(oldest.timestamp() * 1000)

    result = pd.concat(okx_dfs, ignore_index=True)
    result = result.drop_duplicates(subset=['time'], keep='last')
    result = result.sort_values('time').reset_index(drop=True)
    return result


def incremental_update(df_existing):
    print("\n--- 增量更新 ---")
    last_date = df_existing['time'].max()
    print(f"现有数据截止: {last_date.strftime('%Y-%m-%d')}")

    from_ts = int(last_date.timestamp())
    df = fetch_gateio_daily(from_ts=from_ts, limit=30)
    if df is None:
        return None

    df_new = df[df['time'] > last_date]
    if len(df_new) == 0:
        print("  数据已是最新，无需更新")
        return df_existing

    result = pd.concat([df_existing, df_new], ignore_index=True)
    result = result.drop_duplicates(subset=['time'], keep='last')
    result = result.sort_values('time').reset_index(drop=True)

    print(f"  新增 {len(df_new)} 条数据")
    print(f"  最新日期: {result['time'].max().strftime('%Y-%m-%d')}")
    return result


def update_data():
    print("=" * 60)
    print("更新比特币日线数据 (2017年至今)")
    print("=" * 60)

    if os.path.exists(DATA_FILE):
        df_existing = pd.read_csv(DATA_FILE)
        df_existing['time'] = pd.to_datetime(df_existing['time'])
        print(f"\n现有数据: {len(df_existing)} 条")
        print(f"时间范围: {df_existing['time'].min().strftime('%Y-%m-%d')} ~ "
              f"{df_existing['time'].max().strftime('%Y-%m-%d')}")

        has_gap = df_existing['time'].min() > pd.Timestamp('2017-01-10')

        if has_gap:
            print("\n数据不完整（缺2017年起数据），重新全量拉取...")
        else:
            result = incremental_update(df_existing)
            if result is not None:
                result.to_csv(DATA_FILE, index=False)
                print(f"\n更新完成! 共 {len(result)} 条")
                print(f"最新日期: {result['time'].max().strftime('%Y-%m-%d')}")
                print(f"最新收盘价: ${result['Close'].iloc[-1]:,.2f}")
                print("=" * 60)
                return
            print("\n增量更新失败，尝试全量拉取...")
    else:
        print(f"\n全新数据文件: {DATA_FILE}")

    df = fetch_gateio_all()

    if df is not None:
        if df['time'].max() < pd.Timestamp.now() - pd.Timedelta(days=3):
            print(f"\nGate.io 数据最新到 {df['time'].max().strftime('%Y-%m-%d')}")
            print("用 OKX 补充最新数据...")
            df = fill_from_okx(df)
    else:
        print("\nGate.io 失败，尝试 OKX...")
        dfs = []
        after = None
        for i in range(12):
            okx_df = fetch_okx_daily(after_ts=after)
            if okx_df is None:
                break
            dfs.append(okx_df)
            oldest = okx_df['time'].min()
            if oldest <= pd.Timestamp('2017-01-01'):
                break
            after = int(oldest.timestamp() * 1000)

        if dfs:
            df = pd.concat(dfs, ignore_index=True)
        else:
            print("\n[失败] 所有数据源均不可用")
            return

    if df is None:
        print("\n[失败] 无法获取数据")
        return

    df = df[df['time'] >= pd.Timestamp('2017-01-01')]
    df = df.sort_values('time').reset_index(drop=True)
    df.to_csv(DATA_FILE, index=False)

    print(f"\n写入文件: {DATA_FILE}")
    print(f"共条数: {len(df)}")
    print(f"时间范围: {df['time'].min().strftime('%Y-%m-%d')} ~ {df['time'].max().strftime('%Y-%m-%d')}")
    print(f"最新收盘价: ${df['Close'].iloc[-1]:,.2f}")
    print("=" * 60)


if __name__ == '__main__':
    try:
        update_data()
    except Exception as e:
        print(f"\n更新失败: {e}")
        import traceback
        traceback.print_exc()
