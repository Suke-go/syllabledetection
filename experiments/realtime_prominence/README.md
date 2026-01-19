# Real-time Prominence Detection Experiments

syllabledetection ライブラリを使用した、リアルタイムプロミネンス検出の実験プロジェクト。

## 概要

このディレクトリには以下が含まれます：

- **リアルタイム検出アルゴリズム** - 幾何平均Fusion + オンラインキャリブレーション
- **Web Demo** - ブラウザ上でリアルタイム検出を体験
- **論文原稿** - Interspeech 2026 / Human Augmentation Conference

## ディレクトリ構成

```
experiments/realtime_prominence/
├── README.md
├── CMakeLists.txt
├── build_wasm.bat       # WebAssembly ビルドスクリプト
├── src/
│   └── realtime_sim.c   # WAVファイルシミュレータ
├── web_demo/            # ブラウザデモ
│   ├── index.html
│   ├── js/
│   │   ├── demo.js
│   │   ├── prominence-detector.js       # Pure JS実装
│   │   └── prominence-detector-wasm.js  # Wasm版
│   ├── css/
│   └── syllable.wasm    # ビルド済みWasm
├── paper/               # 論文原稿
│   ├── interspeech_prominence.tex
│   └── visceral_resonance.tex
└── config/
    └── default.json
```

## Web Demo

### 起動方法

```bash
cd web_demo
python -m http.server 8000
# または
start_server.bat
```

ブラウザで http://localhost:8000 を開く。

### 機能

- マイク入力からリアルタイムでprominenceを検出
- 検出時に視覚・聴覚フィードバック
- 2秒間の自動キャリブレーション

### 技術仕様

| 項目 | 値 |
|------|-----|
| サンプルレート | 48kHz |
| バッファサイズ | 256 samples (5.3ms) |
| レイテンシ | < 20ms |
| Wasm サイズ | 48KB |

## Wasm ビルド

### 前提条件

1. [Emscripten SDK](https://emscripten.org/) をインストール
2. `emsdk` ディレクトリをプロジェクトルートの親に配置

### ビルド手順

```bash
# Windows
build_wasm.bat

# 出力
# wasm_build/syllable.js
# wasm_build/syllable.wasm
```

### エクスポート関数

| 関数 | 説明 |
|------|------|
| `syllable_create` | 検出器作成 |
| `syllable_process` | 音声処理 |
| `syllable_set_realtime_mode` | RTモード設定 |
| `syllable_recalibrate` | キャリブレーション |
| `syllable_is_calibrating` | キャリブレーション状態 |
| `syllable_destroy` | 検出器破棄 |

## 論文

### Interspeech 2026

`paper/interspeech_prominence.tex`

**タイトル**: Lightweight Real-Time Acoustic Prominence Detection for Explainable Pronunciation Feedback

**概要**: CAPT向けの軽量リアルタイムpromience検出。オンラインキャリブレーションと幾何平均Fusionにより、20ms以下のレイテンシでBURNC F1=0.68を達成。

### Visceral Resonance (Human Augmentation)

`paper/visceral_resonance.tex`

**タイトル**: Visceral Resonance: Augmenting Speech Listening with Prominence-Synchronized Abdominal EMS

**概要**: リアルタイムprominence検出と腹部EMS同期による内臓感覚的リスニング体験の拡張。

## アルゴリズム

### 特徴量 (6種)

1. **Spectral Flux** - オンセット検出
2. **Peak Rate** - 母音立ち上がり
3. **High-Frequency Energy** - 子音検出
4. **MFCC Delta** - 音色変化
5. **Wavelet** - マルチスケールトランジェント
6. **Voicing Confidence** - 有声判定

### オンラインキャリブレーション

```
θ_k = P_95(f_k) × 10^(SNR/10)
```

2秒間のノイズフロアから95パーセンタイルを推定し、SNR (デフォルト 6dB) で閾値を設定。

### 幾何平均Fusion

```
S = σ(max(∏r_k^(1/n), max(r_k)/2))
```

閾値を超えた特徴量の幾何平均を計算。単一特徴量のノイズスパイクに強い。

## ネイティブビルド

```bash
# 親プロジェクトのビルド
cd ../..
mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=ON
cmake --build . --config Release

# ライブラリのコピー
cd ../experiments/realtime_prominence
mkdir lib
cp ../../build/Release/syllable.* lib/

# このプロジェクトのビルド
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## 依存関係

- syllable ライブラリ（親プロジェクト）
- KissFFT（extern/kissfft、submodule）
- Emscripten SDK（Wasmビルド用）
