"""
比特币链上指标数据更新脚本
从 Bitcoin Magazine Pro 获取 MVRV Z-Score 和 CVDD 数据
使用 Playwright 浏览器自动化提取 Plotly 图表数据

支持增量更新：每次运行时读取现有CSV最后日期，只追加新数据

输出文件:
  - data/mvrv_zscore.csv  (time,zscore)
  - data/mvrv_zscore.txt  (yyyy-mm-dd,zscore)
  - data/cvdd.csv         (time,cvdd)
  - data/cvdd.txt         (yyyy-mm-dd,cvdd)
"""

import os
import sys
import asyncio

script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

MVRV_ZSCORE_CSV = 'mvrv_zscore.csv'
MVRV_ZSCORE_TXT = 'mvrv_zscore.txt'
CVDD_CSV = 'cvdd.csv'
CVDD_TXT = 'cvdd.txt'

MVRV_URL = 'https://www.bitcoinmagazinepro.com/charts/mvrv-zscore/'
CVDD_URL = 'https://www.bitcoinmagazinepro.com/charts/cvdd/'


def merge_and_save(new_pairs, csv_path, txt_path, header):
    """
    增量合并并保存。
    new_pairs: 从网页获取的完整数据列表 [(date_str, value), ...]
    会读取现有CSV，只追加新数据，然后去重排序写入。
    """
    # 读取现有数据（如果存在）
    existing = {}  # date -> value
    if os.path.exists(csv_path):
        try:
            with open(csv_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
            # 跳过表头
            for line in lines[1:]:
                line = line.strip()
                if not line:
                    continue
                parts = line.split(',', 1)
                if len(parts) == 2:
                    d, v = parts[0].strip(), parts[1].strip()
                    if d:
                        existing[d[:10]] = v
        except Exception as e:
            print(f"  [警告] 读取 {csv_path} 失败，将全量更新: {e}")
            existing = {}

    if existing:
        last_existing = max(existing.keys())
        print(f"  现有数据截止: {last_existing} ({len(existing)} 条)")
    else:
        print(f"  无现有数据，将全量写入")
        last_existing = None

    # 合并新数据（过滤掉已存在的日期）
    added = 0
    for date_str, val in new_pairs:
        if val is None or val == '':
            continue
        key = date_str[:10]
        if key not in existing:
            existing[key] = str(val)
            added += 1

    if added == 0 and existing:
        print(f"  数据已是最新，无需更新")
        return len(existing)

    # 按日期排序
    sorted_items = sorted(existing.items(), key=lambda x: x[0])

    # 写入 CSV (with header)
    with open(csv_path, 'w', encoding='utf-8') as f:
        f.write(header + '\n')
        for date_str, val in sorted_items:
            f.write(f"{date_str},{val}\n")

    # 写入 TXT (yyyy-mm-dd,value per line, no header)
    with open(txt_path, 'w', encoding='utf-8') as f:
        for date_str, val in sorted_items:
            f.write(f"{date_str},{val}\n")

    print(f"  新增 {added} 条记录")
    print(f"  最新日期: {sorted_items[-1][0]}")
    print(f"  写入 {len(sorted_items)} 条到: {csv_path}")
    print(f"  写入 {len(sorted_items)} 条到: {txt_path}")
    return len(sorted_items)


async def fetch_mvrv_zscore():
    """从 MVRV Z-Score 页面获取 Z-Score 数据"""
    from playwright.async_api import async_playwright

    print("\n--- 获取 MVRV Z-Score 数据 ---")
    async with async_playwright() as p:
        browser = await p.chromium.launch(
            headless=True,
            args=['--ignore-certificate-errors', '--no-sandbox']
        )
        page = await browser.new_page(viewport={'width': 1280, 'height': 800})

        try:
            await page.goto(MVRV_URL, wait_until='networkidle', timeout=60000)
            await page.wait_for_selector('.js-plotly-plot', timeout=30000)
            await asyncio.sleep(5)

            result = await page.evaluate('''() => {
                var plotDiv = document.querySelector('.js-plotly-plot');
                if (!plotDiv || !plotDiv._fullData) return {error: 'no data'};
                var data = plotDiv._fullData;

                var zscoreTrace = null;
                for (var i = 0; i < data.length; i++) {
                    if (data[i].name === 'Z-Score') zscoreTrace = data[i];
                }
                if (!zscoreTrace) return {error: 'no Z-Score trace'};

                var pairs = [];
                var x = zscoreTrace.x;
                var y = zscoreTrace.y;
                for (var i = 0; i < x.length && i < y.length; i++) {
                    if (y[i] !== null && y[i] !== undefined) {
                        var d = new Date(x[i]);
                        var dateStr = d.getFullYear() + '-' +
                            String(d.getMonth() + 1).padStart(2, '0') + '-' +
                            String(d.getDate()).padStart(2, '0');
                        pairs.push([dateStr, y[i]]);
                    }
                }
                return {pairs: pairs, total: pairs.length};
            }''')

            if 'error' in result:
                print(f"  [失败] {result['error']}")
                return None

            print(f"  从网页获取 {result['total']} 条")
            return result['pairs']

        except Exception as e:
            print(f"  [失败] {e}")
            return None
        finally:
            await browser.close()


async def fetch_cvdd():
    """从 CVDD 页面获取 CVDD 数据"""
    from playwright.async_api import async_playwright

    print("\n--- 获取 CVDD 数据 ---")
    async with async_playwright() as p:
        browser = await p.chromium.launch(
            headless=True,
            args=['--ignore-certificate-errors', '--no-sandbox']
        )
        page = await browser.new_page(viewport={'width': 1280, 'height': 800})

        try:
            await page.goto(CVDD_URL, wait_until='networkidle', timeout=60000)
            await page.wait_for_selector('.js-plotly-plot', timeout=30000)
            await asyncio.sleep(5)

            result = await page.evaluate('''() => {
                var plotDiv = document.querySelector('.js-plotly-plot');
                if (!plotDiv || !plotDiv._fullData) return {error: 'no data'};
                var data = plotDiv._fullData;

                var cvddTrace = null;
                for (var i = 0; i < data.length; i++) {
                    if (data[i].name === 'CVDD') cvddTrace = data[i];
                }
                if (!cvddTrace) return {error: 'no CVDD trace'};

                var pairs = [];
                var x = cvddTrace.x;
                var y = cvddTrace.y;
                for (var i = 0; i < x.length && i < y.length; i++) {
                    if (y[i] !== null && y[i] !== undefined) {
                        var d = new Date(x[i]);
                        var dateStr = d.getFullYear() + '-' +
                            String(d.getMonth() + 1).padStart(2, '0') + '-' +
                            String(d.getDate()).padStart(2, '0');
                        pairs.push([dateStr, y[i]]);
                    }
                }
                return {pairs: pairs, total: pairs.length};
            }''')

            if 'error' in result:
                print(f"  [失败] {result['error']}")
                return None

            print(f"  从网页获取 {result['total']} 条")
            return result['pairs']

        except Exception as e:
            print(f"  [失败] {e}")
            return None
        finally:
            await browser.close()


def update_indicators():
    """更新所有指标数据（增量更新）"""
    print("=" * 60)
    print("更新比特币链上指标数据 (增量更新)")
    print("=" * 60)

    # 1. 获取 MVRV Z-Score
    zscore_pairs = asyncio.run(fetch_mvrv_zscore())
    if zscore_pairs and len(zscore_pairs) > 0:
        # 通达信兼容处理：负数取绝对值，再缩小100倍；正数保持原样
        for i in range(len(zscore_pairs)):
            date_str, val = zscore_pairs[i]
            val = float(val)
            if val < 0:
                val = abs(val) / 100.0
            zscore_pairs[i] = [date_str, val]
        merge_and_save(zscore_pairs, MVRV_ZSCORE_CSV, MVRV_ZSCORE_TXT,
                       "time,zscore")
    else:
        print("  [警告] MVRV Z-Score 数据获取失败，保留现有文件")

    print()

    # 2. 获取 CVDD
    cvdd_pairs = asyncio.run(fetch_cvdd())
    if cvdd_pairs and len(cvdd_pairs) > 0:
        merge_and_save(cvdd_pairs, CVDD_CSV, CVDD_TXT,
                       "time,cvdd")
    else:
        print("  [警告] CVDD 数据获取失败，保留现有文件")

    print("\n" + "=" * 60)
    print("指标数据更新完成")
    print("=" * 60)


if __name__ == '__main__':
    try:
        update_indicators()
    except Exception as e:
        print(f"\n更新失败: {e}")
        import traceback
        traceback.print_exc()
    try:
        input("\n按回车键退出...")
    except EOFError:
        pass  # 非交互模式（如通过C的system()调用）下静默退出
