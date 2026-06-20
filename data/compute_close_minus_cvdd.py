"""
计算比特币收盘价相对于 CVDD 的偏离百分比
读取 btc_historical_daily.csv 和 cvdd.csv，
对重叠日期的每个日期计算 (Close - CVDD) / Close * 100，
输出 TXT 文件格式: YYYY-MM-DD,百分比

正值表示价格高于CVDD，负值表示价格低于CVDD。

支持增量检测：每次运行时检查输出文件的最新日期，
如果输入数据有更新则重新计算，否则跳过。
"""

import os
import csv
import sys

script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

BTC_FILE = 'btc_historical_daily.csv'
CVDD_FILE = 'cvdd.csv'
OUTPUT_TXT = 'btc_close_minus_cvdd.txt'


def get_last_line_date(filepath, has_header=True):
    """快速读取CSV/TXT文件的最后一行日期"""
    if not os.path.exists(filepath):
        return None
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        if has_header and len(lines) > 1:
            last_line = lines[-1].strip()
        elif not has_header and len(lines) > 0:
            last_line = lines[-1].strip()
        else:
            return None
        parts = last_line.split(',')
        if parts:
            return parts[0].strip()[:10]
    except Exception:
        pass
    return None


def needs_update():
    """检查是否需要重新计算"""
    # 获取输出文件的最新日期
    out_last_date = get_last_line_date(OUTPUT_TXT, has_header=False)

    # 检查输出文件是否为旧格式（绝对差值，非百分比）
    # 用最后一行检测：近期BTC价格数万，旧格式差值 > 1000，百分比 < 200
    if os.path.exists(OUTPUT_TXT):
        try:
            with open(OUTPUT_TXT, 'r', encoding='utf-8') as f:
                lines = f.readlines()
            if lines:
                last_line = lines[-1].strip()
                parts = last_line.split(',')
                if len(parts) >= 2:
                    val = abs(float(parts[1]))
                    if val > 1000:
                        print("  [检测] 输出文件为旧格式（绝对差值），需要重新计算为百分比")
                        os.remove(OUTPUT_TXT)
                        return True
        except Exception:
            pass

    # 获取 BTC 和 CVDD 的最新日期
    btc_last_date = get_last_line_date(BTC_FILE, has_header=True)
    cvdd_last_date = get_last_line_date(CVDD_FILE, has_header=True)

    if btc_last_date is None or cvdd_last_date is None:
        return True  # 输入文件缺失，需要尝试

    # 取两个输入中较早的那天（交集边界取决于较短的数据源）
    common_last = min(btc_last_date, cvdd_last_date)

    if out_last_date is None:
        print("  [检测] 输出文件不存在，需要计算")
        return True

    if out_last_date < common_last:
        print(f"  [检测] 有新数据: 输出截止 {out_last_date}，输入截止 {common_last}")
        return True

    print(f"  [检测] 数据已是最新（{out_last_date}），无需计算")
    return False


def main():
    # 先检测是否需要更新
    if not needs_update():
        sys.exit(0)  # 无需更新，正常退出

    print("=" * 60)
    print("计算 BTC 收盘价相对于 CVDD 的偏离百分比")
    print("=" * 60)

    # 1. 读取 BTC 日线数据（只取 Close）
    btc_close = {}
    with open(BTC_FILE, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        next(reader)  # 跳过表头
        for row in reader:
            if len(row) >= 5:
                date = row[0].strip()
                close = float(row[4].strip())
                btc_close[date] = close
    print(f"BTC 日线数据: {len(btc_close)} 条")
    print(f"  范围: {min(btc_close.keys())} ~ {max(btc_close.keys())}")

    # 2. 读取 CVDD 数据
    cvdd = {}
    with open(CVDD_FILE, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        next(reader)  # 跳过表头
        for row in reader:
            if len(row) >= 2:
                date = row[0].strip()[:10]
                val = float(row[1].strip())
                cvdd[date] = val
    print(f"CVDD 数据: {len(cvdd)} 条")
    print(f"  范围: {min(cvdd.keys())} ~ {max(cvdd.keys())}")

    # 3. 取交集，计算差值
    common_dates = sorted(set(btc_close.keys()) & set(cvdd.keys()))
    print(f"\n重叠日期: {len(common_dates)} 条")
    print(f"  范围: {common_dates[0]} ~ {common_dates[-1]}")

    if not common_dates:
        print("[失败] 没有重叠的日期数据")
        sys.exit(1)

    # 4. 计算百分比并输出 TXT 文件
    # 负值取绝对值再除以100（缩小负数影响）
    pct_min = float('inf')
    pct_max = float('-inf')
    neg_count = 0
    with open(OUTPUT_TXT, 'w', encoding='utf-8') as f:
        for date in common_dates:
            close = btc_close[date]
            if close == 0:
                pct = 0.0
            else:
                pct = (close - cvdd[date]) / close * 100.0
            if pct < 0:
                pct = abs(pct) / 100.0
                neg_count += 1
            if pct < pct_min: pct_min = pct
            if pct > pct_max: pct_max = pct
            f.write(f"{date},{pct:.4f}\n")

    print(f"\n写入 {len(common_dates)} 条到: {OUTPUT_TXT}")
    print(f"百分比范围: {pct_min:.2f}% ~ {pct_max:.2f}%")
    first_close = btc_close[common_dates[0]]
    last_close = btc_close[common_dates[-1]]
    first_pct = (first_close - cvdd[common_dates[0]]) / first_close * 100
    last_pct = (last_close - cvdd[common_dates[-1]]) / last_close * 100
    print(f"第一行: {common_dates[0]},{first_pct:.2f}%")
    print(f"最后一行: {common_dates[-1]},{last_pct:.2f}%")
    print("=" * 60)


if __name__ == '__main__':
    main()


