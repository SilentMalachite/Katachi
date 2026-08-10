# ライセンス

本体および依存物のライセンス条件。**追加の依存を導入する前に、必ずこのファイルを更新する**
（`docs/phases.md` §4 Phase 3 受け入れ基準）。

判断に迷う事項が出た場合は停止して指示を仰ぐ（`CLAUDE.md` 停止条件 7）。

---

## 1. 本体

| 項目 | 内容 |
|---|---|
| ライセンス | **GNU General Public License v3.0 or later**（全文は `LICENSE`） |
| 対象 | `src/` `tests/` `cmake/` `tools/` **`packaging/`**（ただし §1.3 の除外あり）および本リポジトリのドキュメント |
| 著作権者 | Silent Malachite（`git log` の実測で単独） |

**`LICENSE` は gnu.org の GPLv3 正本であり、改変しない。**

**2026-08-11 に対象へ `packaging/` と `tools/` を追記した。** Phase 4 でアプリアイコン
（`packaging/icons/`）と配布用のテンプレート（`packaging/macos/` `packaging/windows/`）を
追加したのに、対象の一覧が `src/` `tests/` `cmake/` のままで、**新しく書いた著作物が
どの条件で配布されるのか宙に浮いていた。** `tools/` は Phase 1 からある取りこぼしである。

**2026-08-09 に Apache License 2.0 から変更した。** 判断の記録は
`docs/adr/0012-license-gplv3.md`。変更時点で未リリースであり、影響を受ける利用者はいない。

### 1.1 なぜ GPLv3 と Qt の LGPLv3 が両立するのか

**LGPLv3 は GPLv3 での再配布を許可している。** そのため、LGPLv3 の Qt に動的リンクする
本体を GPLv3-or-later で配布できる。

**GPLv2 only は選べない。** LGPLv3 から GPLv3 への移行は可能だが、GPLv2 へは下げられない。
この組み合わせでは配布できなくなる（ADR-0012）。

### 1.2 貢献されたコードの扱い

**貢献も GPLv3-or-later で配布される**（`CONTRIBUTING.md` §6）。
GPL と両立しないライセンスのコードは受け入れられない。

### 1.3 本体のライセンスの対象外（第三者の法文）

**`packaging/licenses/` に置くファイルは、本体のライセンスの対象ではない。**

ここに入るのは Qt および Qt が内部に含む第三者ライブラリの**ライセンス文そのもの**であり、
他者の著作物である。配布時に成果物へ同梱する義務を果たすために、原文のまま置いている。

- **改変しない。** 表記の統一や翻訳も行わない
- **本体の GPLv3 を主張しない。** 各ファイルの条件はそのファイル自身が定める
- 取得元の URL と取得日は `packaging/licenses/SOURCES.md` に記録する

同じ理由で、`docs/format-matrix.md`（ビルド時の生成物）も本体の著作物ではあるが
自動生成であり、手で編集しない。

---

## 2. Qt 6

| 項目 | 内容 |
|---|---|
| **リンクするモジュール** | `Qt6::Concurrent` / `Qt6::Core` / `Qt6::Gui` / `Qt6::Widgets`（`CMakeLists.txt` の `find_package` と一致） |
| **同梱するが直接は使わないモジュール** | `QtDBus`（`QtGui` が参照する）/ `QtSvg`（SVG のプラグインが参照する） |
| 採用ライセンス | **LGPL v3** |
| リンク形態 | **動的リンクのみ。静的リンクは行わない** |

**2026-08-11 に「同梱するが直接は使わないモジュール」の行を足した。** Phase 4 T4 で
`macdeployqt` の出力を実測したところ、本体がリンクしていない 2 つが配布物に入ることが
分かったためである。`QtDBus` は削ると `QtGui` の読み込みに失敗して起動しない
（実測。`docs/progress/phase4.md`）。

**同梱する以上、この 2 つも LGPLv3 の条件の対象である。** §4 の一覧に含める。

### 2.1 なぜ動的リンクに限定するのか

LGPLv3 は、ライブラリ部分を利用者が**差し替えて再リンクできる**ことを求める
（LGPLv3 §4 の「Combined Works」の条件）。動的リンクであれば、利用者が Qt の共有ライブラリを
自分でビルドしたものに置き換えられるため、この条件を成果物の配布だけで満たせる。

静的リンクを選ぶと、再リンクに必要なオブジェクトファイル一式の提供義務が生じ、
配布物の構成が大きく変わる。本プロジェクトはこれを採らない。

**この判断は `CLAUDE.md` の決定事項である。変更するには ADR を書く。**

### 2.2 遵守のために行うこと

- Qt を動的リンクする（`CMakeLists.txt` は Qt の共有ライブラリ / フレームワークにリンクする）
- 成果物に Qt の LGPLv3 ライセンス全文を同梱する
- Qt に変更を加えた場合は、その変更部分のソースを開示する
  — **現時点で Qt に変更は加えていない**
- 配布物で動的リンクであることを確認する
  — macOS: `otool -L` / Windows: `dumpbin /dependents`（Phase 4 受け入れ基準）

---

## 3. ビルド時のみの依存（成果物に含まれない）

| 依存 | ライセンス | 用途 | 成果物への同梱 |
|---|---|---|---|
| Catch2 v3 | Boost Software License 1.0 | 単体テスト | **含まれない**（テストバイナリのみ） |
| Inno Setup | Inno Setup License（修正 BSD 系） | Windows のインストーラ生成（ADR-0015） | **含まれない**（生成物の中身は本プロジェクトの内容） |
| SPDX license-list-data | CC0-1.0 | `packaging/licenses/spdx/` の法文の取得元 | 法文そのものは**同梱する**（§1.3 / §4） |

### 3.1 MSVC ランタイムは「ビルド時のみ」ではない（**2026-08-11 追記**）

`windeployqt` は既定で MSVC のランタイム DLL（`msvcp140.dll` `vcruntime140.dll` 等）を
実行ファイルの隣へ複製する。**これは成果物の実行に必要な第三者コードであり、
本節の「ビルド時のみ」には当たらない。**

**しかし、これらを直接同梱してはならない。** Qt 公式ドキュメント
（`doc.qt.io/qt-6/windows-deployment.html`）が明示している。

> These individual DLLs are **not intended or licensed for redistribution**, and should not be
> shipped directly. **Only the official Microsoft Redistributable installer** should be used for
> deployment on end-user systems.

**したがって `--no-compiler-runtime` を付けて配置しない**（ADR-0016 の D13）。

| 配布物 | 対応 |
|---|---|
| Windows のインストーラ | **公式の Microsoft Visual C++ 再頒布可能パッケージ (x64) を実行する** |
| Windows のポータブル zip | **同梱しない。** 「再頒布可能パッケージが必要」と `README` と同梱の説明に明記する |

**「入っていないのに動くはず」と書かない。必要なら必要と書く。**

---

## 4. Qt 同梱の画像フォーマットプラグイン

`QImageReader` / `QImageWriter` が使うプラグイン（`qjpeg` / `qtiff` / `qwebp` 等）は、
libjpeg-turbo・libtiff・libwebp などの第三者ライブラリを内部に含む。
これらは Qt の配布物の一部として提供され、それぞれ独自のライセンス条項を持つ。

**プラグインは 2 つのモジュールに分かれている。**

| モジュール | 含まれるプラグイン（macOS 実測） |
|---|---|
| `qtbase` | png（組込） / `qjpeg` / `qgif` / `qico` / `qpdf` / `qsvg` |
| **`qtimageformats`（アドオン）** | `qtiff` / `qwebp` / `qjp2` / `qicns` / `qtga` / `qwbmp` / `qmacheif` |

**`qtimageformats` は既定ではインストールされない。** CI では `install-qt-action` の
`modules: qtimageformats` で明示的に入れている。これを入れないと TIFF / WebP などが
能力表から丸ごと消える（Phase 1 の CI で実際に起きた）。

### 4.1 実際に同梱するもの（**2026-08-11 に確定。Phase 4 T4**）

**`qtimageformats` は同梱する。** 入れないと TIFF / WebP などが能力表から消えるためである。

同梱物は `macdeployqt` / `windeployqt` の出力から**削ったうえで**確定した（ADR-0015 D12）。
削る理由と経緯は `docs/progress/phase4.md`。

| 区分 | 同梱するもの |
|---|---|
| Qt のライブラリ 6 | `QtConcurrent` `QtCore` `QtDBus` `QtGui` `QtSvg` `QtWidgets` |
| 画像フォーマットのプラグイン | macOS 11 個 / Windows 9 個（`qjp2` `qmacheif` `qmacjp2` は macOS のみ） |
| その他のプラグイン 3 | `iconengines/qsvgicon` / `platforms`（`qcocoa` または `qwindows`）/ `styles` |
| **削るもの** | `platforminputcontexts`（仮想キーボード）と、それだけが引く `QtQuick` `QtQml*` `QtVirtualKeyboard*` `QtNetwork` `QtOpenGL` の 9 つ |

**削る理由。** 仮想キーボードのプラグイン 1 つが 8 フレームワークを引き込み、その中に
`QtQuick`（**ADR-0001 は Qt Quick 不採用と決めている**）と `QtNetwork`
（**`README.md` はネットワーク通信を行わないと明記している**）が含まれていた。
**配布物の中身を、明文化した決定と食い違わせない。** 82 MB → 52 MB になった。

### 4.2 第三者コードの列挙は Qt の SBOM から機械的に行う

Qt 6.8 以降は SPDX 2.3 形式の SBOM を同梱している。
**Qt のバイナリ配布物にライセンス文は含まれていない**ことを実測したため、
公式ドキュメントを人が読み写すのではなく、この SBOM を典拠にする。

| ファイル | 役割 |
|---|---|
| `packaging/licenses/from-qt-sbom.py` | SBOM から同梱物が含む第三者コードを推移的に列挙する |
| `packaging/licenses/qt-third-party.json` / `.md` | その生成物。**成果物に入るもの 43 件** |
| `packaging/licenses/fetch-spdx-texts.py` | 標準 SPDX 識別子の法文を正本から取得する |
| `packaging/licenses/spdx/` | 取得した法文 **22 種**（`LicenseRef-*` 3 種は SBOM 由来） |
| `packaging/licenses/SOURCES.md` | 出所・取得日・sha256・`OR` 式の選択理由 |
| `cmake/ThirdPartyLicenses.cmake` | `third_party_licenses.txt` を組み立てる。**法文が 1 つでも欠けたら `FATAL_ERROR`** |

Qt 公式も次のように述べており、この方針と一致する。

> You only need to acknowledge and comply with the licenses of the third-party components
> that you are **actually shipping** with your application.

### 4.3 この列挙の限界（**偽らないために書く**）

**機械で確かめられるのは「SBOM に記載がある範囲」までである。**

`QtCore` の中に静的に取り込まれた第三者コードは、配布物にファイルとして現れない。
したがって**「配布物のファイル一覧」からは検出できず**、Qt の SBOM の正確さに依存する。

同じ理由で、Phase 4 T7 の機械検査 P5 が確かめるのは
**「配布物に入っている各ファイルに対応する記載があるか」**であって、
**「一覧が完全か」ではない。** この区別を報告でも崩さない。

---

## 5. 追加コーデック（Phase 3）

HEIF / AVIF / JPEG XL / RAW / PSD の追加コーデックは、**導入前に**本ファイルを更新する
（`docs/phases.md` §4 Phase 3 受け入れ基準）。

**2026-08-09 の GPLv3 化により、この節の制約は緩んだ。**
以前は「GPL のみで提供されるライブラリをリンクすると本体の Apache-2.0 と両立しない」
と記録していたが、**本体が GPLv3-or-later になったためこの非両立は解消した。**
GPL のみで提供されるライブラリを選べる（ADR-0012）。

**ただし確認そのものは引き続き必要である。**

| ライセンス | 可否 |
|---|---|
| GPLv3 / GPLv2-or-later / LGPL / MIT / BSD / Apache-2.0 | **リンクできる**（GPLv3 と両立） |
| **GPLv2 only** | **リンクできない。** 本体の GPLv3 と両立しない |
| 独占的ライセンス、GPL 非互換の条項を持つもの | **リンクできない** |

**導入判断より先にライセンスを確認する**という手順は変えない。
判断に迷う場合は停止して指示を仰ぐ（`CLAUDE.md` 停止条件 7）。

### 5.1 調査結果（2026-08-09 に一次情報で確認。Phase 3 T2）

**まだ導入していない。** 下表は導入判断のための調査結果である（**ADR-0013**）。

| 対象 | 版 | ライセンス | 出典（2026-08-09 取得） | GPLv3 との両立 |
|---|---|---|---|---|
| libavif | 1.4.2 | BSD-2-Clause（`LICENSE` に同梱コードの条項も併記） | `raw.githubusercontent.com/AOMediaCodec/libavif/main/LICENSE` | **可** |
| libjxl | 0.12.0 | BSD-3-Clause | `raw.githubusercontent.com/libjxl/libjxl/main/LICENSE` | **可** |
| LibRaw | 0.22.2 | LGPL-2.1 または CDDL-1.0 の**選択制**。LGPL-2.1 を選ぶ | `raw.githubusercontent.com/LibRaw/LibRaw/master/COPYRIGHT` | **可** |
| kimageformats `psd` / `raw` プラグイン | v6.20.0 | LGPL-2.0-or-later | `.../KDE/kimageformats/master/src/imageformats/psd.cpp` / `raw.cpp` の SPDX | **可** |
| kimageformats `avif` / `jxl` プラグイン | v6.20.0 | BSD-2-Clause | 同 `avif.cpp` / `jxl.cpp` の SPDX | **可** |
| extra-cmake-modules (ECM) | v6.20.0 | `COPYING-CMAKE-SCRIPTS` に BSD 系の条項 | `.../KDE/extra-cmake-modules/master/COPYING-CMAKE-SCRIPTS` | ビルド時のみ（§3 と同じ扱い。成果物に含まれない） |

**LGPL-2.1 / LGPL-2.0-or-later が GPLv3 と両立する根拠**: LGPL 第 3 条が
「この License の代わりに通常の GPL version 2 **またはそれ以降**を適用してよい」と定めている
（LibRaw 同梱の `LICENSE.LGPL` 211 行目で条文を確認した）。

### 5.2 HEIF について（**以前の記述の訂正**）

Phase 3 の計画時、HEIF を対象外とする理由として
「libheif の HEVC エンコーダ backend が GPL-2.0-only ならリンクできない」と述べていた。
**この前提は誤りである。**

| 対象 | ライセンス | 出典 |
|---|---|---|
| libheif（ライブラリ本体） | LGPL v3 | `raw.githubusercontent.com/strukturag/libheif/master/COPYING` |
| x265 | **GPL-2.0-or-later**（ソースヘッダに "either version 2 of the License, or (at your option) any later version"） | `raw.githubusercontent.com/videolan/x265/master/source/encoder/encoder.cpp` |

**どちらも GPLv3 と両立する。** 上表の「GPLv2 only はリンクできない」に該当しない。
**HEIF を導入しない判断は、ライセンスではなく依存の重さと、macOS では
`qmacheif` により既に読み書きできること（Phase 3 T1 の実測）に基づく**（ADR-0013）。

---

## 6. 未確定事項

以下は推測で埋めず、必要になった時点で一次情報を確認する。

1. **Qt 6.8 LTS のパッチリリースが、オープンソース利用者にどの範囲で提供されるか。**
   CI は 6.8 系に固定する方針（`docs/phases.md` §1.5）だが、固定するパッチ版の入手可否は
   CI 実装時に確認する。ライセンス条件そのもの（LGPLv3 での提供）とは別の論点。
2. **Phase 4 で同梱する第三者ライセンス文の正確な一覧。** 実際に配布物へ入るプラグインが
   確定してから収集する。
