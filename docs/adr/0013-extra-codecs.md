# ADR-0013: 追加コーデックは kimageformats のプラグイン 4 つを固定タグでビルドして配置する

- 状態: **提案（承認待ち）** — ライセンスの判断を含むため `CLAUDE.md` 停止条件 7 に該当する
- 日付: 2026-08-09

---

## 背景

`docs/phases.md` §1 の Phase 3 は「拡張コーデック：HEIF / AVIF / JPEG XL / RAW / PSD の
可否調査と導入」であり、§4 の受け入れ基準は次の 3 つである。

1. 追加コーデックの導入**前**に `docs/licenses.md` が更新されている
2. 追加コーデックが無い環境でもビルド・起動する（オプショナル依存）
3. 能力表が追加コーデックを自動的に反映する（コード変更不要）

Phase 3 T1 で両 OS の対応形式を CI の実測で確定した（`docs/progress/phase3.md`）。

| | macOS | Windows |
|---|---|---|
| Qt 6.8.3 + `qtimageformats` で使える形式 | 21 | 18 |
| 差 | `heic` / `heif` / `jp2` が macOS のみ | — |

**AVIF / JPEG XL / PSD / RAW はどちらの OS にも無い。**

## 調査（一次情報。2026-08-09 に取得）

### ライセンス

| 対象 | 版 | ライセンス | 出典 |
|---|---|---|---|
| libavif | 1.4.2（Homebrew stable） | **BSD-2-Clause**。`LICENSE` には同梱コードの条項も併記されている（`src/obu.c` は dav1d 由来、`third_party/iccjpeg/*`、`contrib/gdk-pixbuf/*`） | `raw.githubusercontent.com/AOMediaCodec/libavif/main/LICENSE` |
| libjxl | 0.12.0（Homebrew `jpeg-xl`） | **BSD-3-Clause**（第 3 条「Neither the name of the copyright holder…」を確認） | `raw.githubusercontent.com/libjxl/libjxl/main/LICENSE` |
| LibRaw | 0.22.2（Homebrew） | **LGPL-2.1 または CDDL-1.0 の選択制**。本プロジェクトは LGPL-2.1 を選ぶ | `raw.githubusercontent.com/LibRaw/LibRaw/master/COPYRIGHT` |
| kimageformats `psd` / `raw` | v6.20.0 | **LGPL-2.0-or-later**（各 `.cpp` の `SPDX-License-Identifier`） | `raw.githubusercontent.com/KDE/kimageformats/master/src/imageformats/psd.cpp` / `raw.cpp` |
| kimageformats `avif` / `jxl` | v6.20.0 | **BSD-2-Clause**（同上） | 同 `avif.cpp` / `jxl.cpp` |
| extra-cmake-modules (ECM) | v6.20.0 | `COPYING-CMAKE-SCRIPTS` に BSD 系の条項。**ビルド時のみの依存で成果物に含まれない** | `raw.githubusercontent.com/KDE/extra-cmake-modules/master/COPYING-CMAKE-SCRIPTS` |

**すべて `docs/licenses.md` §5 の表で「リンクできる」に分類される。**
LGPL-2.1 / LGPL-2.0-or-later が GPLv3 と両立するのは、LGPL 第 3 条が
「この License の代わりに通常の GPL version 2 **またはそれ以降**を適用してよい」と
定めているためである（`LICENSE.LGPL` 211 行目で条文を確認した）。

### 技術的制約

| # | 確認した事実 | 出典 |
|---|---|---|
| 1 | AVIF / JPEG XL / PSD / RAW の `QImageIOPlugin` を提供する既製プロジェクトは、**いずれも ECM（extra-cmake-modules）を要求する** | kimageformats / novomesk 各 `CMakeLists.txt` の `find_package(ECM …)` |
| 2 | **PSD と RAW のプラグインは kimageformats にしか無い。** novomesk の単体プラグインは AVIF と JPEG XL のみ | 両リポジトリの構成 |
| 3 | kimageformats の**最新タグ v6.28.1 は Qt 6.9.0 と CMake 3.29 を要求する**。CI は Qt 6.8.3 に固定されている（`docs/phases.md` §1.5、Phase 0 の決定）ため**使えない** | `kimageformats/v6.28.1/CMakeLists.txt` |
| 4 | **v6.20.0 は Qt 6.8.0 / CMake 3.16 / ECM 6.20.0 を要求し、`psd` `raw` `avif` `jxl` の 4 プラグインを含む。CI の Qt 6.8.3 で使える** | `kimageformats/v6.20.0/CMakeLists.txt` および `src/imageformats/CMakeLists.txt` |
| 5 | kimageformats は 25 個のプラグインを持ち、その中の `kimg_tga` / `kimg_jp2` は **Qt 同梱の `qtga` / `qjp2` と同じ形式を扱う** | 同上のプラグイン一覧 |
| 6 | `kimg_heif` は既定で無効（`option(KIMAGEFORMATS_HEIF … OFF)`）で、`libheif` が見つかった場合のみ作られる | `kimageformats/v6.20.0/CMakeLists.txt:67` |

## 選択肢

### A. システムに導入済みのプラグインを使う（自前でビルドしない）

Homebrew や vcpkg の kimageformats を入れてもらい、こちらは何もビルドしない。

**採れない。** Qt のプラグインは**アプリと同じ Qt に対してビルドされている必要がある**。
Homebrew の kimageformats は Homebrew の Qt に対して作られるため、公式インストーラ /
aqtinstall の Qt を使う本プロジェクトでは 1 プロセスに 2 つの Qt が載ることになる。

### B. `FetchContent` で kimageformats を取り込む

我々の CMake ツリーの一部として構成される。`include(KDECMakeSettings)` が
同じディレクトリスコープで走り、KDE 側のビルド設定が我々の `-Werror` 等と干渉しうる。

### C. `ExternalProject_Add` で別ビルドし、生成物だけ受け取る

別プロセスの CMake 実行になるため、KDE 側の設定は我々のツリーに漏れない。
受け取るのは共有ライブラリのファイルだけ。

### D. 自前で `QImageIOPlugin` を書く

`src/` に新しい層が増え、`docs/spec-core.md` §1 の構成を変えることになる。
コーデック本体のラッパを自作する保守コストも負う。

## 決定

**C を採る。`ExternalProject_Add` で ECM と kimageformats を固定タグでビルドし、
必要な 4 プラグインだけを配置する。**

| 項目 | 値 |
|---|---|
| kimageformats | **v6.20.0**（Qt 6.8 系で動く最も新しい系列。制約 #4） |
| ECM | **v6.20.0**（kimageformats v6.20.0 の要求と揃える） |
| 配置するプラグイン | **`kimg_psd` / `kimg_raw` / `kimg_avif` / `kimg_jxl` の 4 つだけ** |
| 配置先 | `${CMAKE_BINARY_DIR}/plugins/imageformats/` |
| 有効化 | `KATACHI_EXTRA_CODECS`（**既定 OFF**） |
| コーデック本体 | `libavif` / `libjxl` / `LibRaw` を kimageformats 側の検出機構に任せる。`ON` なのに見つからなければ**構成を失敗させる** |

**4 つだけを配置するのは制約 #5 のためである。** kimageformats の `kimg_tga` / `kimg_jp2` を
一緒に置くと、Qt 同梱の `qtga` / `qjp2` と同じ形式を扱うプラグインが 2 つ載り、
**どちらが使われるかが不定になる。** 能力表は実行時にプラグインへ問い合わせるため、
この不定性はそのまま変換結果の不定性になる。**必要なものだけ置く。**

**HEIF は対象に含めない。**

### 除外理由の訂正（Phase 3 の計画時に述べた理由は誤りだった）

計画時、HEIF を外す理由として「libheif の HEVC エンコーダ backend である x265 が
**GPL-2.0-only** であれば本体 GPLv3 とリンクできない」と述べ、未検証と明記していた。

**検証した結果、この前提は誤りである。** x265 のソースヘッダは
「either version 2 of the License, or (at your option) **any later version**」と述べており、
**GPL-2.0-or-later** である（出典: `raw.githubusercontent.com/videolan/x265/master/source/encoder/encoder.cpp`。
リポジトリ直下の `COPYING` は GPLv2 の全文だが、条項の範囲はヘッダが定める）。
libheif 本体は LGPL-3（`COPYING`）。**どちらも GPLv3 と両立する。**

したがって**除外の理由はライセンスではない。** 実際の理由は次の 2 点である。

1. **macOS では `qmacheif` により既に読み書きできる**（T1 の実測）。追加で得られるのは Windows 側のみ
2. HEIF を入れると **libheif + HEVC エンコーダ**という依存が増える。AVIF / JPEG XL / PSD / RAW の 4 つは
   Homebrew に単体で存在するのに対し、HEIF はエンコーダの選択という追加の判断を伴う

**この訂正の結果、HEIF を入れる選択肢はライセンス上は開いている。**
Phase 3 では入れないが、`docs/licenses.md` §5 に「ライセンス上は可」と記録する。

## 帰結

- **受け入れ基準 2 は既定値で守られる。** `KATACHI_EXTRA_CODECS=OFF` が既定なので、
  日常の品質ゲートは常に「追加コーデックが無い環境」を検証している
- **アプリは追加コーデックに一切リンクしない。** プラグインは実行時に読み込まれる
  共有ライブラリであり、`katachi_app` の依存に現れない。
  したがって `src/` の変更は不要であり、受け入れ基準 3 が構造として満たされる
- **Qt 6.8 系という CI の固定（`docs/phases.md` §1.5）は崩さない。**
  kimageformats のタグを Qt 6.8 対応の系列に固定することで両立させた。
  **Qt の下限を上げる判断は Phase 3 では行わない**
- kimageformats を将来 v6.28 以降へ上げるには **Qt 6.9 以上が必要**になる。
  Qt の下限を上げる判断とセットであり、そのときは ADR を書く
- Phase 4 で配布する場合、この 4 プラグインと、その先の libavif / libjxl / LibRaw の
  ライセンス文を `third_party_licenses.txt` に含める必要がある（`docs/licenses.md` §4 と同じ扱い）
- **プラグインの探索方法（`QT_PLUGIN_PATH` か、アプリからのライブラリパス追加か）は
  この ADR では決めない。** 開発時とテストは環境変数で足り、配布時の配置は Phase 4 の論点である
