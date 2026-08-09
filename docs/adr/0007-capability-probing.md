# ADR-0007: アルファ対応と可逆性はメモリ上の往復で実測する

- 状態: 承認
- 日付: 2026-08-09

---

## 背景

`docs/spec-core.md` §3 の `FormatCapability` は次の 5 つの真偽値を持つ。

```cpp
bool canDecode, canEncode, supportsAlpha, supportsQuality, isLossless;
```

このうち 3 つは Qt の API から直接得られる。

| フィールド | 取得元 |
|---|---|
| `canDecode` | `QImageReader::supportedImageFormats()` |
| `canEncode` | `QImageWriter::supportedImageFormats()` |
| `supportsQuality` | `QImageWriter::supportsOption(QImageIOHandler::Quality)` |

**残る `supportsAlpha` と `isLossless` に相当する API は Qt に存在しない。**
`QImageIOHandler::ImageOption` の全列挙を確認した（Qt 6.8 公式ドキュメント）。

```
Size, ClipRect, ScaledSize, ScaledClipRect, Description, CompressionRatio, Gamma,
Quality, Name, SubType, IncrementalReading, Endianness, Animation, BackgroundColor,
ImageFormat, SupportedSubTypes, OptimizedWrite, ProgressiveScanWrite, ImageTransformation
```

アルファ対応の有無も可逆性も、この列挙には無い。
一方 `CLAUDE.md` は「対応フォーマットはハードコードせず、実行時に
`QImageReader` / `QImageWriter` から能力表を生成する」と定めており、
形式名と性質の対応表をコードに書くことは禁じられている。

## 選択肢

### A. メモリ上の往復で実測する

固定パターンの小さな画像を `QBuffer` へ書き出し、読み戻して性質を判定する。

### B. `FormatId.hpp` の例外枠に対応表を置く

フォーマット名の文字列リテラルが許される唯一の場所に
`(名前 → alpha, lossless)` の表を持つ。単純で高速。

### C. 2 フィールドを仕様から外す

`docs/spec-core.md` §3 から削る。ただし §4 のアルファ表は
「出力形式がアルファ非対応」の判定を必要とするため、別の手段が要る。

## 決定

**A を採る。`buildFromQt()` がメモリ上の往復で実測する。**

理由。

1. **`CLAUDE.md` の「ハードコードせず実行時に生成する」に合致する。**
   B は形式名と性質の対応をコードに固定することになり、この原則に正面から反する。
   Phase 3 で追加コーデックを導入したとき、B では手動登録が要るが、
   A は何もしなくても新しい形式を正しく分類する。
2. **実測で全形式が正しく分類できることを確認した**（下記）。
3. **core の禁止事項に触れない。** `QBuffer` はメモリ上のバッファであり、
   ファイルアクセスではない。固定パターンを使うため乱数も時刻も使わない。

### 判定方法

| フィールド | 探針 | 判定 |
|---|---|---|
| `supportsAlpha` | 2×2 の `Format_ARGB32`。1 画素を完全透明にする | 往復後もその画素の alpha が 0 なら true |
| `isLossless` | 8×8 の `Format_RGB32`。1 画素ごとに白黒が入れ替わる高周波パターン | 往復後に全画素が完全一致すれば true |

高周波パターンを使うのは、非可逆コーデックの周波数変換が
市松模様を原理的に再現できないため。なだらかな画像では非可逆でも一致しうる。

### 実測結果（Qt 6.11.1 / macOS 14 arm64）

```
fmt    quality  alpha  lossless
bmp    no       no     lossless
heic   yes      no     lossy
icns   no       yes    lossless
ico    no       yes    lossless
jpeg   yes      no     lossy
png    yes      yes    lossless
tiff   no       yes    lossless
webp   yes      yes    lossy
xpm    no       yes    lossless
```

いずれも実態と一致する。

## 帰結

- **書き出せない形式（`canEncode == false`）は往復できないため判定不能。
  `supportsAlpha` / `isLossless` / `supportsQuality` をいずれも `false` に固定する。**
  この環境では `gif` / `pdf` / `svg` / `svgz` / `tga` が該当する。
  `convert()` のアルファ判定は**出力形式**、すなわち書き出し可能な形式にしか使わないため、
  実害はない。**「false」は「その性質が無い」ではなく「判定していない」を意味する。**
  この違いを利用者に見せる場面が出たら、この ADR を書き直す。
- **`buildFromQt()` は形式数 × 2 回の往復を行う。** 小さな画像のため
  実測では無視できるコストだが、**繰り返し呼ばない。**
  `docs/spec-core.md` §3 のとおり明示的に 1 度生成して注入する。
- **`isLossless` の定義は「この探針画像が完全一致で往復すること」である。**
  白黒 2 値のパターンを使うため、色を表現できない形式（`pbm` / `xbm` 等）も
  `lossless` と判定される。これらはその色モデルの範囲では実際に可逆であり、
  誤りではないが、**任意の入力に対する可逆性を意味しない。**
- Phase 3 で追加コーデックを導入した場合、**コード変更なしに能力表へ反映される**
  （`docs/phases.md` §4 Phase 3 の受け入れ基準）。この ADR の方式はその要件を満たす。
- 探針画像は固定パターンであり、`buildFromQt()` の結果は同じ環境で常に同じになる。
