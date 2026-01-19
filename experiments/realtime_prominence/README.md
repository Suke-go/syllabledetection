# Real-time Prominence Detection Experiments

syllabledetection ライブラリを使用した、リアルタイムプロミネンス検出の実験用プロジェクト。

## ディレクトリ構成

```
experiments/realtime_prominence/
├── README.md           # このファイル
├── CMakeLists.txt      # スタンドアロンビルド設定
├── src/
│   └── realtime_sim.c  # WAVファイルシミュレータ
├── lib/                # syllable ライブラリのコピー（ビルド後）
├── include/            # ヘッダーのコピー
├── config/
│   └── default.json    # デフォルトパラメータ
├── data/               # テスト用音声ファイル
└── output/             # 実験結果出力
```

## セットアップ

### 1. syllabledetection ライブラリのビルド

```bash
cd ../../   # syllabledetection ルートへ
mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=ON
cmake --build . --config Release
```

### 2. ライブラリのコピー

```powershell
# Windows
Copy-Item ..\..\build\Release\syllable.dll lib\
Copy-Item ..\..\build\Release\syllable.lib lib\
```

```bash
# Linux/macOS
cp ../../build/libsyllable.so lib/  # or .dylib
```

### 3. このプロジェクトのビルド

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## 使い方

### WAVファイルをリアルタイム風に処理
```bash
./realtime_sim ../data/test.wav
```

### 高速処理（シミュレーションなし）
```bash
./realtime_sim ../data/test.wav --fast
```

### 速度調整
```bash
./realtime_sim ../data/test.wav --speed 2.0
```

## 出力形式

```
[12.34s] PROMINENCE | Score: 0.82 | PR:0.75 SF:0.88 HF:0.45 | VOICED
  -> Feedback: Good prominence - well stressed!
```

### フィールド説明

| フィールド | 説明 |
|-----------|------|
| Score | 融合スコア (0-1) |
| PR | Peak Rate（母音立ち上がり） |
| SF | Spectral Flux（スペクトル変化） |
| HF | High Frequency Energy（高周波エネルギー） |
| Type | VOICED / UNVOICED / MIXED |

## 依存関係

- syllable ライブラリ（親プロジェクトからコピー）
- C99 コンパイラ
