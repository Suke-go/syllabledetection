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

## プロジェクト構成

```
syllabledetection/
├── src/
│   ├── syllable_detector.c    # メイン検出器
│   └── dsp/                   # DSPモジュール
│       ├── agc.c/h            # 自動ゲイン制御
│       ├── spectral_flux.c/h  # スペクトラルフラックス
│       ├── mfcc.c/h           # MFCC特徴量
│       ├── wavelet.c/h        # ウェーブレット変換
│       └── high_freq_energy.c/h
├── include/
│   └── syllable_detector.h    # 公開API
├── extern/
│   └── kissfft/               # FFTライブラリ (submodule)
├── experiments/
│   └── realtime_prominence/   # リアルタイム実験
│       ├── web_demo/          # ブラウザデモ (Wasm)
│       └── paper/             # 論文原稿
├── wrappers/                  # 言語バインディング
│   ├── python/
│   └── csharp/
└── docs/                      # ドキュメント
```

## ビルド

### 前提条件

```bash
# サブモジュールの取得
git submodule update --init --recursive
```

### ネイティブビルド

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Windows環境では `build/Release/syllable.dll` と `syllable.lib` が生成される。

### WebAssembly ビルド

詳細な手順は [experiments/realtime_prominence/README.md](experiments/realtime_prominence/README.md#wasm-ビルド) を参照。

```bash
cd experiments/realtime_prominence
# Emscripten SDK のセットアップ後
emcc [source files] -o wasm_build/syllable.js  # 詳細はREADME参照
```

## 使い方

### C/C++ (オフラインモード)

```c
#include "syllable_detector.h"

SyllableConfig config = syllable_default_config(16000);
SyllableDetector *detector = syllable_create(&config);

float audio[1024];
SyllableEvent events[64];
int count = syllable_process(detector, audio, 1024, events, 64);

for (int i = 0; i < count; i++) {
    printf("%.3f秒: スコア %.2f\n", events[i].time_seconds, events[i].prominence_score);
}

syllable_destroy(detector);
```

### C/C++ (リアルタイムモード) - NEW

```c
#include "syllable_detector.h"

SyllableConfig config = syllable_default_config(48000);
SyllableDetector *detector = syllable_create(&config);

// リアルタイムモード有効化 + キャリブレーション開始
syllable_set_realtime_mode(detector, 1);

// キャリブレーション中は無音を入力 (約2秒)
while (syllable_is_calibrating(detector)) {
    // 無音または環境ノイズを処理
    syllable_process(detector, silence_buffer, 512, events, 64);
}

// 検出開始
while (recording) {
    int count = syllable_process(detector, audio_buffer, 512, events, 64);
    for (int i = 0; i < count; i++) {
        // prominence検出時の処理
    }
}

syllable_destroy(detector);
```

### Web (Wasm)

```javascript
const detector = new ProminenceDetectorWasm();
await detector.init();

// RTモード有効 (自動でキャリブレーション開始)
detector.start();

// prominence検出時のコールバック
detector.onProminence = (event) => {
    console.log(`Score: ${event.fusion_score}`);
};
```

## リアルタイムモード API

| 関数 | 説明 |
|------|------|
| `syllable_set_realtime_mode(d, enable)` | RTモード有効/無効化 |
| `syllable_recalibrate(d)` | キャリブレーション再開 |
| `syllable_is_calibrating(d)` | キャリブレーション中か確認 |

### リアルタイムモードの特徴

- **オンラインキャリブレーション**: 2秒間で環境ノイズを学習
- **SNRベース閾値**: $\theta_k = P_{95}(f_k) \times 10^{\text{SNR}/10}$
- **幾何平均Fusion**: 複数特徴量の同時超過を要求し、ノイズに強い
- **レイテンシ**: 20ms以下
- **メモリ**: 500KB以下

## 設定パラメータ

| パラメータ | デフォルト | 説明 |
|-----------|-----------|------|
| `sample_rate` | (必須) | 入力サンプルレート |
| `realtime_mode` | 0 | リアルタイムモード有効化 |
| `calibration_duration_ms` | 2000 | キャリブレーション期間 (ms) |
| `snr_threshold_db` | 6.0 | SNR閾値 (dB) |
| `min_syllable_dist_ms` | 200 | 最小音節間隔 (ms) |

## 実験・デモ

### Web Demo

```bash
cd experiments/realtime_prominence/web_demo
python -m http.server 8000
# ブラウザで http://localhost:8000 を開く
```

### 論文

- `paper/interspeech_prominence.tex`: Interspeech 2026 論文
- `paper/visceral_resonance.tex`: Human Augmentation デモ論文

## 依存関係

- [KissFFT](https://github.com/mborgerding/kissfft) - BSD License (submodule)
- C99 コンパイラ
- CMake 3.10+

## ライセンス

MIT License
