# ADR-0006: `FormatId` の正規化と別名の吸収

- 状態: 承認
- 日付: 2026-08-09

---

## 背景

`docs/spec-core.md` §2.1 は `FormatId` を強い型付き文字列と定め、
`QString` ⇄ `FormatId` の変換関数を `FormatId.hpp` にのみ置くと決めている。
しかし**変換時に正規化を行うかどうかを書いていない。**

`FormatId::operator==` は defaulted、すなわち `QString` の完全一致比較である。
正規化しないと次が起きる。

- 利用者入力や拡張子から作った `FormatId{"PNG"}` が、Qt 由来の `FormatId{"png"}` と一致しない
- 同じ形式を指す `FormatId{"jpg"}` と `FormatId{"jpeg"}` が別物になる

いずれも `CapabilityTable::find()` の取りこぼしになる。

Phase 1 の T2 実装時、まず空白除去と小文字化のみを入れて「指示書に根拠が無い判断」として
報告したところ、**正規化を行うこと、および別名を吸収することの指示を得た。**

### 実測した Qt の挙動（Qt 6.11.1 / macOS 14 arm64）

```
READ : bmp cur gif heic heif icns ico jfif jp2 jpeg jpg pbm pdf pgm png ppm
       svg svgz tga tif tiff wbmp webp xbm xpm
WRITE: bmp cur heic heif icns ico jfif jp2 jpeg jpg pbm pgm png ppm tif tiff
       wbmp webp xbm xpm
MIME : ... image/jpeg ... image/tiff ... image/heic image/heif ...
```

Qt は `jpeg` / `jpg` / `jfif` を**別々の名前として報告するが、MIME はいずれも `image/jpeg`**。
`tif` / `tiff` も同様に `image/tiff`。
一方 `heic` と `heif` は `image/heic` と `image/heif` で**別の MIME**。

## 選択肢

### A. 正規化しない

指示書の記述に忠実だが、上記の取りこぼしが残る。

### B. 空白除去と小文字化のみ

大文字問題は解決するが、`jpg` と `jpeg` の分裂は残る。

### C. 空白除去 + 小文字化 + 別名の吸収

すべて解決する。どの名前を別名とみなすかの基準が要る。

## 決定

**C を採る。`formatIdFromString()` は次の順で正規化する。**

1. 前後の空白を落とす
2. 小文字へ畳む
3. 別名を代表名へ畳む

**別名を畳む基準は「Qt が同一の MIME タイプを報告すること」。**
名前の見た目や一般的な慣習では判断しない。実測できる事実に基づく。

現時点の別名表。

| 別名 | 代表名 | 根拠 |
|---|---|---|
| `jpg` | `jpeg` | ともに `image/jpeg` |
| `jfif` | `jpeg` | ともに `image/jpeg` |
| `tif` | `tiff` | ともに `image/tiff` |

**畳まないもの（意図的）**

| 名前 | 理由 |
|---|---|
| `heic` / `heif` | MIME が `image/heic` と `image/heif` で異なる |
| `svg` / `svgz` | MIME が `image/svg+xml` と `image/svg+xml-compressed` で異なる（svgz は圧縮形式） |

## 帰結

- **別名表は `src/core/FormatId.hpp` にのみ置く。**
  ここは「フォーマット名の文字列リテラル禁止」の唯一の例外であり
  （`docs/spec-core.md` §3、不変条件スキャナ INV3A の除外対象）、
  別名表はその例外枠を使う唯一の箇所である。**他の場所に増やさない。**
- **代表名が必ず存在することを実測で確認した。** `jpeg` と `tiff` は Qt の
  読み込み・書き出しの両方の一覧に含まれる。代表名側が存在しない環境では
  畳んだ結果が引けなくなるが、次の項目により実害は生じない。
- **`CapabilityTable` も同じ `formatIdFromString()` で正規化する。**
  表の鍵と問い合わせの鍵が常に同じ関数を通るため、片側だけ畳まれて
  引けなくなることはない。仮に Qt が `tif` しか報告しない環境でも、
  表側も `tiff` として格納されるため一致する。
- **T3 の `CapabilityTable::buildFromQt()` は正規化後の重複を畳む必要がある。**
  Qt は `jpeg` と `jpg` と `jfif` を別々に報告するため、正規化すると 3 件が
  同じ `FormatId` になる。**同一 `FormatId` の `FormatCapability` は 1 件に統合し、
  `extensions` は和集合を取る。** これを怠ると `find()` がどれを返すか不定になる。
- 別名表を増やすときは、**Qt の MIME タイプを実測してから**この ADR に追記する。
  推測で足さない。
- Phase 3 で追加コーデックを導入した際、新しい形式が既存の代表名と同じ MIME を
  報告する場合は別名表への追加を検討する。
