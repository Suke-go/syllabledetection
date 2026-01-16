# libsyllable

リアルタイム音節検出・アクセント推定ライブラリ

## 概要

音声信号から音節境界とアクセント（強勢）を検出するC言語ライブラリ。
PeakRate法とZero Frequency Filter (ZFF) を組み合わせたハイブリッド手法を採用。

## ビルド

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Windows環境では `build/Release/syllable.dll` と `syllable.lib` が生成される。

## 使い方

### C/C++

```c
#include "syllable_detector.h"

// 初期化
SyllableConfig config = syllable_default_config(16000);  // サンプルレート指定
SyllableDetector *detector = syllable_create(&config);

// 処理（モノラル float 配列を渡す）
float audio[1024];
SyllableEvent events[64];
int count = syllable_process(detector, audio, 1024, events, 64);

// 結果を使う
for (int i = 0; i < count; i++) {
    printf("%.3f秒: スコア %.2f\n", events[i].time_seconds, events[i].prominence_score);
}

// 終了時
int remaining = syllable_flush(detector, events, 64);
syllable_destroy(detector);
```

### Python

```python
from wrappers.python.syllable import SyllableDetector

detector = SyllableDetector("path/to/syllable.dll", sample_rate=16000)

# 処理
results = detector.process_block(audio_float_list)
for r in results:
    print(f"{r['time']:.3f}s: score={r['score']:.2f}")

# 終了
remaining = detector.flush()
```

### C# / Unity

`wrappers/csharp/SyllableDetector.cs` をプロジェクトに追加。
`syllable.dll` を `Assets/Plugins` に配置。

```csharp
using SyllableDetection;

var detector = new SyllableDetector(16000);
var events = new SyllableEvent[64];

int count = detector.Process(audioData, events);
for (int i = 0; i < count; i++) {
    Debug.Log($"{events[i].time_seconds:F3}s: {events[i].prominence_score:F2}");
}

detector.Dispose();
```

## 出力データ構造

`SyllableEvent` の各フィールド:

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `time_seconds` | double | 検出時刻（秒） |
| `peak_rate` | float | 包絡線の立ち上がり速度 |
| `pr_slope` | float | PeakRate の傾斜 |
| `energy` | float | 音節エネルギー |
| `f0` | float | 基本周波数 (Hz) |
| `delta_f0` | float | 周囲との F0 差 |
| `prominence_score` | float | 顕著性スコア（1.0 が平均、高いほど強調） |
| `is_accented` | int | 1: 強調, 0: 非強調 |

## 設定パラメータ

`SyllableConfig` で調整可能:

| パラメータ | デフォルト | 説明 |
|-----------|-----------|------|
| `sample_rate` | (必須) | 入力サンプルレート |
| `threshold_peak_rate` | 0.0003 | 検出閾値の下限 |
| `adaptive_peak_rate_k` | 4.0 | 適応閾値の係数 |
| `min_syllable_dist_ms` | 100 | 最小音節間隔 (ms) |
| `context_size` | 2 | 顕著性計算に使う前後音節数 |

## 配布パッケージ作成

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package_dist.ps1
```

`dist/` に以下が生成される:
- `bin/syllable.dll`
- `lib/syllable.lib`
- `include/syllable_detector.h`
- `wrappers/`

## ライセンス

MIT License
