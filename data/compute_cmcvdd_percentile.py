"""
计算 C-CVDD 数值的历史百分位
读取 btc_close_minus_cvdd.txt 中的每个 (date, value)，
对于每个数值，计算其在全部历史数据中的百分位排名，
输出 TXT 文件格式: YYYY-MM-DD,百分位(0~100)

百分位含义：数值在历史上比百分之多少的日子要高。
例如 95 表示该值比历史上 95% 的日子都高（极度高估）。
例如 5  表示该值比历史上 95% 的日子都低（极度低估）。

相同数值的百分位相同（取该值在排序中最后一次出现的排名）。
支持增量检测：每次运行时检查输出文件的最新日期，
如果输入数据没有更新则跳过。
"""

import os
import sys

script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

INPUT_FILE = 'btc_close_minus_cvdd.txt'
OUTPUT_FILE = 'btc_cmcvdd_percentile.txt'


def get_last_line_date(filepath):
    """快速读取TXT文件的最后一行日期"""
    if not os.path.exists(filepath):
        return None
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        if not lines:
            return None
        last_line = lines[-1].strip()
        parts = last_line.split(',')
        if parts:
            return parts[0].strip()[:10]
    except Exception:
        pass
    return None


def needs_update():
    """检查是否需要重新计算"""
    in_last = get_last_line_date(INPUT_FILE)
    out_last = get_last_line_date(OUTPUT_FILE)

    if in_last is None:
        print("  [检测] 输入文件不存在，跳过")
        return False

    if out_last is None:
        print("  [检测] 输出文件不存在，需要计算")
        return True

    if out_last < in_last:
        print(f"  [检测] 有新数据: 输入截止 {in_last}，输出截止 {out_last}")
        return True

    print(f"  [检测] 数据已是最新（{out_last}），无需计算")
    return False


def compute_percentile(values):
    """
    计算每个值在历史数据中的百分位排名（0~100）。
    相同值的百分位相同：取该值在排序中最后一次出现的排名。
    百分位 = rank / (n - 1) * 100，最小值为 0，最大值为 100。
    """
    n = len(values)
    if n == 0:
        return []
    if n == 1:
        return [50.0]

    # 排序带原始索引
    sorted_pairs = sorted(enumerate(values), key=lambda x: x[1])

    percentiles = [0.0] * n

    # 用双指针处理相同值：相同值的所有索引共享最后一条的排名
    i = 0
    while i < n:
        j = i
        # 找到所有相同值
        while j < n and abs(sorted_pairs[j][1] - sorted_pairs[i][1]) < 1e-12:
            j += 1
        # 这些相同值的排名统一为 (j - 1)（即最后一条的排名）
        rank = j - 1
        pct = rank / (n - 1) * 100.0
        for k in range(i, j):
            orig_idx = sorted_pairs[k][0]
            percentiles[orig_idx] = pct
        i = j

    return percentiles


def main():
    if not needs_update():
        sys.exit(0)

    print("=" * 60)
    print("计算 C-CVDD 历史百分位")
    print("=" * 60)

    # 1. 读取数据
    dates = []
    values = []
    with open(INPUT_FILE, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(',')
            if len(parts) >= 2:
                dates.append(parts[0].strip())
                values.append(float(parts[1].strip()))

    print(f"读取数据: {len(dates)} 条")
    print(f"  范围: {dates[0]} ~ {dates[-1]}")

    if not values:
        print("[失败] 没有数据")
        sys.exit(1)

    # 2. 计算百分位
    percentiles = compute_percentile(values)

    # 3. 输出文件
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        for date, pct in zip(dates, percentiles):
            f.write(f"{date},{pct:.2f}\n")

    # 4. 统计信息
    print(f"\n写入 {len(dates)} 条到: {OUTPUT_FILE}")
    print(f"百分位范围: {min(percentiles):.2f} ~ {max(percentiles):.2f}")

    # 找出当前最新值的百分位
    last_pct = percentiles[-1]
    print(f"最新值（{dates[-1]}）的百分位: {last_pct:.2f}")

    # 找最高/最低百分位对应的日期
    max_idx = percentiles.index(max(percentiles))
    min_idx = percentiles.index(min(percentiles))
    print(f"最高百分位: {dates[max_idx]} -> {percentiles[max_idx]:.2f}")
    print(f"最低百分位: {dates[min_idx]} -> {percentiles[min_idx]:.2f}")

    # 检查是否有相同值的记录
    unique_count = len(set(round(v, 4) for v in values))
    print(f"唯一值数量: {unique_count}（共 {len(values)} 条）")

    print("=" * 60)


if __name__ == '__main__':
    main()
