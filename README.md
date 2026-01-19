# libsyllable

リアルタイム音節検出・アクセント推定ライブラリ

## 概要

音声信号から音節境界とアクセント（強勢）を検出するC言語ライブラリ。
Multi-Feature Fusion アーキテクチャを採用し、以下の特徴量を組み合わせて高精度な検出を実現：

- **PeakRate**: 包絡線の立ち上がり速度
- **Spectral Flux**: スペクトルの時間変化量
- **High-Frequency Energy**: 高周波エネルギー（子音・破裂音検出）
- **MFCC Delta**: MFCCの時間差分（音素境界検出）
- **Wavelet Transform**: ウェーブレット変換によるトランジェント検出
- **Zero Frequency Filter (ZFF)**: 有声/無声判定とF0推定

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

### 基本情報
| フィールド | 型 | 説明 |
|-----------|-----|------|
| `timestamp_samples` | uint64_t | オンセットのサンプルインデックス |
| `time_seconds` | double | 検出時刻（秒） |

### レガシー特徴量
| フィールド | 型 | 説明 |
|-----------|-----|------|
| `peak_rate` | float | 包絡線の立ち上がり速度 |
| `pr_slope` | float | PeakRate の傾斜 |
| `energy` | float | 音節エネルギー |
| `f0` | float | 基本周波数 (Hz) |
| `delta_f0` | float | 周囲との F0 差 |
| `duration_s` | float | 推定持続時間（秒） |

### Multi-Feature 特徴量 (NEW)
| フィールド | 型 | 説明 |
|-----------|-----|------|
| `spectral_flux` | float | オンセット時のSpectral Flux値 |
| `high_freq_energy` | float | オンセット時の高周波エネルギー |
| `mfcc_delta` | float | オンセット時のMFCC変化量 |
| `wavelet_score` | float | オンセット時のWaveletトランジェントスコア |
| `fusion_score` | float | 統合検出スコア（0-1+） |
| `onset_type` | SyllableOnsetType | オンセットタイプ（VOICED/UNVOICED/MIXED） |

### 顕著性・アクセント
| フィールド | 型 | 説明 |
|-----------|-----|------|
| `prominence_score` | float | 顕著性スコア（0.0〜1.0+, コンテキスト相対） |
| `is_accented` | int | 1: 強調, 0: 非強調 |

## 設定パラメータ

`SyllableConfig` で調整可能:

### 基本設定
| パラメータ | デフォルト | 説明 |
|-----------|-----------|------|
| `sample_rate` | (必須) | 入力サンプルレート |
| `min_syllable_dist_ms` | 100 | 最小音節間隔 (ms) |
| `context_size` | 2 | 顕著性計算に使う前後音節数 |

### PeakRate 設定
| パラメータ | デフォルト | 説明 |
|-----------|-----------|------|
| `peak_rate_band_min` | 500.0 | バンドパスフィルタ最小周波数 (Hz) |
| `peak_rate_band_max` | 3200.0 | バンドパスフィルタ最大周波数 (Hz) |
| `threshold_peak_rate` | 0.0003 | 検出閾値の下限 |
| `adaptive_peak_rate_k` | 4.0 | 適応閾値の係数 |
| `adaptive_peak_rate_tau_ms` | 500.0 | 適応統計の時定数 (ms) |

### 特徴量有効化
| パラメータ | デフォルト | 説明 |
|-----------|-----------|------|
| `enable_spectral_flux` | 1 | Spectral Flux検出を有効化 |
| `enable_high_freq_energy` | 1 | 高周波エネルギー追跡を有効化 |
| `enable_mfcc_delta` | 1 | MFCC Delta検出を有効化 |
| `enable_wavelet` | 1 | Wavelet Transform検出を有効化 |
| `enable_agc` | 1 | 自動ゲイン制御を有効化 |

### Feature Fusion 重み
| パラメータ | デフォルト | 説明 |
|-----------|-----------|------|
| `weight_peak_rate` | 0.25 | PeakRate の重み |
| `weight_spectral_flux` | 0.20 | Spectral Flux の重み |
| `weight_high_freq` | 0.15 | 高周波エネルギーの重み |
| `weight_mfcc_delta` | 0.10 | MFCC Delta の重み |
| `weight_wavelet` | 0.20 | Wavelet Transform の重み |
| `weight_voiced_bonus` | 0.10 | 有声ボーナスの重み |

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
