# 設計仕様書

## 1. 全体構成

### 1.1 Multi-Feature Fusion アーキテクチャ (v2.0)

```
入力音声 x[n]
    │
    ├──▶ [バンドパスフィルタ] ──▶ [包絡追従] ──▶ [PeakRate計算] ─────┐
    │                                                               │
    ├──▶ [ZFF] ──▶ [有声判定・F0推定] ─────────────────────────────┤
    │                                                               │
    ├──▶ [FFT] ──▶ [Spectral Flux] ────────────────────────────────┤
    │              (スペクトル変化検出)                              │
    │                                                               │
    ├──▶ [ハイパスフィルタ 2kHz] ──▶ [高周波エネルギー] ───────────┼──▶ [Feature Fusion]
    │                               (子音・破裂音検出)               │         │
    │                                                               │    [重み付け合成]
    └──▶ [FFT] ──▶ [Mel Filter] ──▶ [DCT] ──▶ [MFCC Delta] ────────┘         │
                                            (音素境界検出)                     ▼
                                                                        [状態機械]
                                                                    (Voiced/Unvoiced対応)
                                                                             │
                                                                        [顕著性計算]
                                                                             │
                                                                      SyllableEvent[]
```

### 1.2 Feature Fusion 重み配分

| 特徴量 | デフォルト重み | 役割 |
|--------|---------------|------|
| PeakRate | 0.30 | 包絡線立ち上がり（従来手法） |
| Spectral Flux | 0.25 | スペクトル変化（無声音も検出可） |
| High-Freq Energy | 0.20 | 摩擦音・破裂音のバースト |
| MFCC Delta | 0.15 | 音素境界の検出 |
| Voiced Bonus | 0.10 | 有声区間の加算ボーナス |


---

## 2. DSPモジュール

### 2.1 バンドパスフィルタ (biquad.c)

2次 IIR バンドパスフィルタにより 500–3200 Hz を抽出。この帯域は音節核（母音）の第1・第2フォルマント情報を多く含む [1]。

**伝達関数**:

$$
H(z) = \frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}{1 + a_1 z^{-1} + a_2 z^{-2}}
$$

**係数計算** (中心周波数 $f_c$、Q値 $Q$、サンプルレート $f_s$):

$$
\omega_0 = \frac{2\pi f_c}{f_s}, \quad \alpha = \frac{\sin(\omega_0)}{2Q}
$$

$$
b_0 = \alpha, \quad b_1 = 0, \quad b_2 = -\alpha
$$

$$
a_0 = 1 + \alpha, \quad a_1 = -2\cos(\omega_0), \quad a_2 = 1 - \alpha
$$

各係数は $a_0$ で正規化される。設計式は Audio EQ Cookbook [2] に基づく。

**差分方程式**:

$$
y[n] = b_0 x[n] + b_1 x[n-1] + b_2 x[n-2] - a_1 y[n-1] - a_2 y[n-2]
$$

---

### 2.2 包絡追従 (envelope.c)

Attack/Release 非対称の指数平滑による振幅包絡追跡。オーディオコンプレッサー設計で広く使用される標準手法 [3]。

**時定数からの係数計算**:

$$
\alpha_{\text{attack}} = 1 - \exp\left(-\frac{1}{f_s \cdot \tau_{\text{attack}}}\right)
$$

$$
\alpha_{\text{release}} = 1 - \exp\left(-\frac{1}{f_s \cdot \tau_{\text{release}}}\right)
$$

ここで $\tau_{\text{attack}} = 5\,\text{ms}$、$\tau_{\text{release}} = 20\,\text{ms}$。

**更新式**:

$$
e[n] = 
\begin{cases}
e[n-1] + \alpha_{\text{attack}} \cdot (|x[n]| - e[n-1]) & \text{if } |x[n]| > e[n-1] \\
e[n-1] + \alpha_{\text{release}} \cdot (|x[n]| - e[n-1]) & \text{otherwise}
\end{cases}
$$

---

### 2.3 PeakRate 計算

包絡の正の時間微分（半波整流）。Oganian & Chang [4] は、このランドマークが大脳皮質聴覚野における音節符号化と高い相関を持つことを示した。

$$
\text{PeakRate}[n] = \max\bigl(0,\, e[n] - e[n-1]\bigr)
$$

**PR_slope**: 音節開始から PeakRate ピークまでの傾き

$$
\text{PR\_slope} = \frac{\max(\text{PeakRate})}{\Delta t_{\text{rise}}}
$$

ここで $\Delta t_{\text{rise}}$ はオンセットからピークまでの時間（秒）。

---

### 2.4 Zero Frequency Filter (zff.c)

Murty & Yegnanarayana [5] による声門閉鎖点 (GCI) 検出手法。有声/無声判定と F0 推定に使用。

**リーキー二重積分**:

$$
I_1[n] = \lambda \cdot I_1[n-1] + x[n]
$$

$$
I_2[n] = \lambda \cdot I_2[n-1] + I_1[n]
$$

リーク係数 $\lambda = 0.999$（DC 発散防止）。

**トレンド除去** (移動平均):

$$
\bar{I}_2[n] = \frac{1}{W} \sum_{k=0}^{W-1} I_2[n-k]
$$

$$
z[n] = I_2[n] - \bar{I}_2[n]
$$

窓幅 $W$ はサンプルレートと `zff_trend_window_ms` から決定。

**ゼロクロス検出**:

$$
\text{is\_epoch}[n] = 
\begin{cases}
1 & \text{if } z[n-1] < 0 \land z[n] \geq 0 \\
0 & \text{otherwise}
\end{cases}
$$

**F0 推定**:

$$
F_0 = \frac{f_s}{\Delta n_{\text{epoch}}}
$$

$\Delta n_{\text{epoch}}$ は連続する GCI 間のサンプル数。

---

## 3. 検出ロジック

### 3.1 適応閾値

Welford [6] のオンラインアルゴリズムで統計量を更新。

**平均の逐次更新**:

$$
\mu_{n} = \mu_{n-1} + \alpha \cdot (x_n - \mu_{n-1})
$$

**分散の逐次更新**:

$$
\sigma^2_{n} = (1 - \alpha) \cdot \left( \sigma^2_{n-1} + \alpha \cdot (x_n - \mu_{n-1})^2 \right)
$$

時定数 $\tau = 500\,\text{ms}$ から:

$$
\alpha = 1 - \exp\left(-\frac{1}{f_s \cdot \tau}\right)
$$

**閾値計算**:

$$
\theta = \max\bigl(\theta_{\text{floor}},\, \mu + k \cdot \sigma\bigr)
$$

デフォルト: $\theta_{\text{floor}} = 0.0003$、$k = 4.0$。

---

### 3.2 ヒステリシス

シュミットトリガー [7] の原理に基づき、チャタリング防止のため ON/OFF で異なる閾値を使用。

$$
\theta_{\text{on}} = \theta \times h_{\text{on}}, \quad \theta_{\text{off}} = \theta \times h_{\text{off}}
$$

デフォルト: $h_{\text{on}} = 1.2$、$h_{\text{off}} = 0.8$。

### 3.3 状態機械

| 状態 | 遷移条件 |
|------|---------|
| IDLE → ONSET_RISING | $\text{PeakRate} > \theta_{\text{on}} \land \text{is\_voiced}$ |
| ONSET_RISING → NUCLEUS | $\text{PeakRate} < 0.5 \times \text{PeakRate}_{\max}$ |
| NUCLEUS → COOLDOWN | $\lnot\text{is\_voiced} \lor E < E_{\text{thresh}}$ |
| COOLDOWN → IDLE | $t > t_{\min}$ (デフォルト 100 ms) |

---

## 4. 顕著性スコア

英語の強勢知覚に関する研究 [8] に基づき、複数の音響特徴を統合。

### 4.1 コンテキスト計算

$i$ 番目の音節に対し、前後 $C$ 個（デフォルト $C=2$）の音節を参照。

$$
\bar{E} = \frac{1}{|\mathcal{N}|} \sum_{j \in \mathcal{N}} E_j, \quad \mathcal{N} = \{i-C, \ldots, i-1, i+1, \ldots, i+C\}
$$

同様に $\bar{P}$（PeakRate）、$\bar{D}$（Duration）、$\bar{S}$（PR_slope）を計算。

### 4.2 ΔF0 の計算

周囲 F0 の中央値との差:

$$
\Delta F_0^{(i)} = F_0^{(i)} - \text{median}\bigl(\{F_0^{(j)} : j \in \mathcal{N}\}\bigr)
$$

### 4.3 スコア計算

$$
s_E = \frac{E_i}{\bar{E} + \epsilon}, \quad
s_P = \frac{P_i}{\bar{P} + \epsilon}, \quad
s_D = \frac{D_i}{\bar{D} + \epsilon}, \quad
s_S = \frac{S_i}{\bar{S} + \epsilon}
$$

F0 ボーナス（上昇ピッチは強調を示唆）:

$$
b_{F_0} = \text{clamp}\left(\frac{\Delta F_0}{50}, 0, 1\right)
$$

**総合スコア**:

$$
\text{Score} = w_E \cdot s_E + w_P \cdot s_P + w_D \cdot s_D + w_S \cdot s_S + w_F \cdot (1 + b_{F_0})
$$

デフォルト重み: $w_E = w_P = w_D = w_S = w_F = 0.2$

**アクセント判定**:

$$
\text{is\_accented} = 
\begin{cases}
1 & \text{if Score} > 1.2 \\
0 & \text{otherwise}
\end{cases}
$$

---

## 5. 汎用性

本ライブラリの信号処理パイプラインは以下の設計原則により高い汎用性を持つ。

### 5.1 パラメータの可変性

| パラメータ | 調整範囲 | 応用例 |
|-----------|---------|--------|
| `peak_rate_band_min/max` | 任意の Hz | 音楽オンセット検出、環境音分析 |
| `sample_rate` | 8000–48000+ | 電話音声からハイレゾまで |
| `zff_trend_window_ms` | 5–50 ms | 低音声・高音声への適応 |
| `adaptive_peak_rate_k` | 2.0–8.0 | 感度調整 |

### 5.2 言語・信号非依存性

- 音響特徴のみに依存し、言語辞書や音素モデルを使用しない
- 周期的なイベント検出であれば音声以外にも適用可能
  - 心拍・脈波センサー
  - 機械振動モニタリング
  - 動物音声（帯域調整で鳥類・海洋哺乳類等に対応）

### 5.3 計算効率

- 全処理が $O(n)$ で完結
- FFT はスペクトル特徴量（Spectral Flux、MFCC Delta）のみで使用、ホップベース更新で効率化
- 固定メモリフットプリント（動的確保は初期化時のみ）
- SIMD 最適化オプション対応（`simd_utils.h`）

---

## 6. 参考文献

[1] Stevens, K. N. (1998). *Acoustic Phonetics*. MIT Press.

[2] Bristow-Johnson, R. (2005). Audio EQ Cookbook. https://www.w3.org/2011/audio/audio-eq-cookbook.html

[3] Giannoulis, D., Massberg, M., & Zölzer, U. (2012). Digital dynamic range compressor design—A tutorial and analysis. *Journal of the Audio Engineering Society*, 60(6), 399-408.

[4] Oganian, Y., & Chang, E. F. (2019). A speech envelope landmark for syllable encoding in human superior temporal gyrus. *Science Advances*, 5(11), eaay6279.

[5] Murty, K. S. R., & Yegnanarayana, B. (2008). Epoch extraction from speech signals. *IEEE Transactions on Audio, Speech, and Language Processing*, 16(8), 1602-1613.

[6] Welford, B. P. (1962). Note on a method for calculating corrected sums of squares and products. *Technometrics*, 4(3), 419-420.

[7] Schmitt, O. H. (1938). A thermionic trigger. *Journal of Scientific Instruments*, 15(1), 24.

[8] Fry, D. B. (1958). Experiments in the perception of stress. *Language and Speech*, 1(2), 126-152.
