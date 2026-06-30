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
import socket

script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

MVRV_ZSCORE_CSV = 'mvrv_zscore.csv'
MVRV_ZSCORE_TXT = 'mvrv_zscore.txt'
CVDD_CSV = 'cvdd.csv'
CVDD_TXT = 'cvdd.txt'

MVRV_URL = 'https://www.bitcoinmagazinepro.com/charts/mvrv-zscore/'
CVDD_URL = 'https://www.bitcoinmagazinepro.com/charts/cvdd/'


def detect_proxy():
    """检测系统代理配置，返回 Playwright 可用的 proxy_server 字符串，或 None"""
    # 1. 优先读取环境变量
    for env_var in ['HTTPS_PROXY', 'https_proxy', 'HTTP_PROXY', 'http_proxy', 'ALL_PROXY', 'all_proxy']:
        val = os.environ.get(env_var)
        if val:
            # 去掉协议前缀，保留 host:port
            proxy = val.strip().rstrip('/')
            for prefix in ['https://', 'HTTP://', 'http://']:
                if proxy.startswith(prefix):
                    proxy = proxy[len(prefix):]
            print(f"  [代理] 从 {env_var} 检测到: {proxy}")
            return proxy

    # 2. 检测常见本地代理端口（Clash / V2Ray / Sing-box 等）
    common_ports = [7890, 7891, 1080, 10808, 10809, 8080]
    for port in common_ports:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(0.3)
        try:
            if s.connect_ex(('127.0.0.1', port)) == 0:
                proxy_str = f'127.0.0.1:{port}'
                print(f"  [代理] 检测到本地代理: {proxy_str}")
                s.close()
                return proxy_str
            s.close()
        except:
            s.close()

    print("  [代理] 未检测到代理配置，将直连")
    return None


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


STEALTH_JS = '''
// 隐藏 headless 浏览器特征
Object.defineProperty(navigator, 'webdriver', { get: () => undefined });
Object.defineProperty(navigator, 'plugins', { get: () => [1, 2, 3, 4, 5] });
Object.defineProperty(navigator, 'languages', { get: () => ['en-US', 'en'] });
window.chrome = { runtime: {} };
// 覆盖权限查询
const originalQuery = navigator.permissions.query;
navigator.permissions.query = (params) => (
    params.name === 'notifications'
        ? Promise.resolve({ state: 'denied' })
        : originalQuery(params)
);
'''


async def fetch_with_playwright(url, trace_name, trace_key):
    """通用函数：用 CloakBrowser 访问页面，提取指定 Plotly trace 数据

    使用 CloakBrowser (stealth_args=True) 绕过 Cloudflare 防护，
    持久化浏览器 profile 提高成功率。
    """
    from cloakbrowser import launch_persistent_context_async

    proxy_server = detect_proxy()

    # 持久化浏览器 profile 目录（复用 cookie/session，提高绕过成功率）
    user_data_dir = os.path.join(script_dir, '.browser_profile')
    os.makedirs(user_data_dir, exist_ok=True)

    for attempt in range(2):
        try:
            opts = {
                "headless": True,
                "stealth_args": True,
                "viewport": {"width": 1920, "height": 1080},
            }
            if proxy_server:
                opts["proxy"] = {"server": f"http://{proxy_server}"}

            context = await launch_persistent_context_async(user_data_dir, **opts)

            # 注入隐身 JS（补充 cloakbrowser 的 stealth_args）
            await context.add_init_script(STEALTH_JS)

            pages = context.pages
            page = pages[0] if pages else await context.new_page()

            try:
                # 先等 DOM，再等网络空闲（分开等，给 Cloudflare 挑战更多时间）
                await page.goto(url, wait_until='domcontentloaded', timeout=60000)
                await page.wait_for_load_state('networkidle', timeout=90000)
                await asyncio.sleep(5)

                try:
                    await page.wait_for_selector('.js-plotly-plot',
                                                 state='attached', timeout=30000)
                except:
                    # 没找到则尝试移除付费墙
                    await page.evaluate('''
                        document.querySelectorAll('.paywall, .paywall-msg-box')
                            .forEach(el => el.remove());
                    ''')
                    await asyncio.sleep(3)
                    try:
                        await page.wait_for_selector('.js-plotly-plot',
                                                     state='attached', timeout=15000)
                    except:
                        try:
                            await page.screenshot(path=os.path.join(script_dir, 'debug.png'))
                            print(f"  [调试] 页面截图已保存到 debug.png")
                        except:
                            pass
                        return None

                await asyncio.sleep(3)

                result = await page.evaluate(f'''() => {{
                    var plotDiv = document.querySelector('.js-plotly-plot');
                    if (!plotDiv || !plotDiv._fullData) return {{error: 'no data'}};
                    var data = plotDiv._fullData;

                    var targetTrace = null;
                    for (var i = 0; i < data.length; i++) {{
                        if (data[i].name === '{trace_key}') targetTrace = data[i];
                    }}
                    if (!targetTrace) return {{error: 'no {trace_key} trace'}};

                    var pairs = [];
                    var x = targetTrace.x;
                    var y = targetTrace.y;
                    for (var i = 0; i < x.length && i < y.length; i++) {{
                        if (y[i] !== null && y[i] !== undefined) {{
                            var d = new Date(x[i]);
                            var dateStr = d.getFullYear() + '-' +
                                String(d.getMonth() + 1).padStart(2, '0') + '-' +
                                String(d.getDate()).padStart(2, '0');
                            pairs.push([dateStr, y[i]]);
                        }}
                    }}
                    return {{pairs: pairs, total: pairs.length}};
                }}''')

                if 'error' in result:
                    print(f"  [失败] {result['error']}")
                    return None

                print(f"  从网页获取 {result['total']} 条")
                return result['pairs']

            except Exception as e:
                err_str = str(e)
                if attempt == 0 and proxy_server and ('ERR_PROXY' in err_str or 'TIMED' in err_str.upper()):
                    print(f"  [重试] 代理 {proxy_server} 不可用，将直连重试...")
                    proxy_server = None
                    continue
                print(f"  [失败] {e}")
                return None
            finally:
                try:
                    await context.close()
                except:
                    pass

        except Exception as outer_e:
            if attempt == 0:
                print(f"  [重试] 第 1 次失败，10 秒后重试... ({outer_e})")
                await asyncio.sleep(10)
                continue
            print(f"  [失败] {outer_e}")
            return None

    return None


async def fetch_mvrv_zscore():
    print("\n--- 获取 MVRV Z-Score 数据 ---")
    return await fetch_with_playwright(MVRV_URL, 'MVRV Z-Score', 'Z-Score')


async def fetch_cvdd():
    print("\n--- 获取 CVDD 数据 ---")
    return await fetch_with_playwright(CVDD_URL, 'CVDD', 'CVDD')


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
