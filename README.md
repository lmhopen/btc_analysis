# Bitcoin Analysis Toolkit - C Edition

A high-performance Bitcoin price analysis tool written in pure C, featuring K-line charts with linear regression channels.

## Features

- **CSV Data Loading**: Load historical Bitcoin price data
- **Linear Regression Channel**: Calculate and display 9 channel lines (Center, ±0.618σ, ±1.0σ, ±1.5σ, ±2.0σ)
- **K-Line Charts**: Candlestick visualization (red=up, green=down)
- **Log/Linear Scale**: Support for both logarithmic and linear price scales
- **Interactive Crosshair**: Double-click to enable stock-software-style crosshair
- **Real-time Data Display**: View OHLC and channel values at cursor position

## Requirements

- **GCC** or compatible C compiler
- **raylib** library (version 4.0+)

### Installing raylib

**Windows:**
1. Download from https://github.com/raysan5/raylib/releases
2. Extract to `C:\raylib`
3. Or adjust `RAYLIB_PATH` in Makefile

**Linux (Ubuntu/Debian):**
```bash
sudo apt install libraylib-dev
```

**macOS:**
```bash
brew install raylib
```

## Building

```bash
# Build
make

# Build with debug symbols
make debug

# Build and run
make run

# Clean build files
make clean
```

## Usage

```bash
./btc_analysis
```

### Menu Options

1. **Linear Scale Chart (2017+)**: K-line with regression channel, linear Y-axis
2. **Log Scale Chart (2017+)**: K-line with regression channel, logarithmic Y-axis
3. **Full History Chart**: All historical data with log scale
4. **Data Summary**: View data statistics

### Chart Controls

- **Double-click**: Toggle crosshair on/off
- **Move mouse**: View price and indicator data
- **ESC**: Close chart and return to menu

## Project Structure

```
BTC-C/
├── src/
│   ├── main.c          # Main program and menu
│   ├── data.c/h        # CSV parsing and data structures
│   ├── calculate.c/h   # Math calculations (regression, std dev)
│   ├── chart.c/h       # Chart drawing with raylib
│   └── crosshair.c/h   # Crosshair interaction
├── data/
│   └── btc_historical_monthly.csv
├── Makefile
└── README.md
```

## Comparison with Python Version

| Feature | Python | C |
|---------|--------|---|
| Memory Usage | ~100MB+ | ~5-10MB |
| Startup Time | ~2-3s | Instant |
| CPU Usage | Moderate | Minimal |
| Dependencies | pandas, numpy, matplotlib | raylib only |
| Binary Size | N/A | ~200KB |

## Technical Details

### Regression Calculation

- **Logarithmic regression**: Uses least squares method in log(price) space
- **Linear regression**: Follows TongDaXin formula for compatibility
- **Standard deviation**: Population standard deviation (ddof=0)

### Coordinate Transformation

- **Log scale**: Uses log10 transformation for Y-axis
- **Index to pixel**: Linear mapping with proper margins
- **Price to pixel**: Accounts for log/linear mode

## License

MIT License

## Original Python Version

See `D:\openskills\BTC\` for the original Python implementation.
