# ADR-0016: 配布物に同梱する第三者ライセンスの扱い

- 状態: 承認
- 日付: 2026-08-11

---

## 背景

`docs/phases.md` §4 Phase 4 の受け入れ基準 3 は「成果物に `third_party_licenses.txt` が
同梱されている」と定めている。配布方式そのものは **ADR-0015** に分けた。ここでは
**何を、どうやって列挙し、どの法文を添えるか**を決める。

`docs/licenses.md` §4 は Phase 0 の時点で「Phase 4 で実際に同梱するプラグインを列挙し、
対応するライセンス文を収集する」と書いていた。その実行にあたる。

### 決定の前提とした実測（推測ではない）

| 事実 | 出所 |
|---|---|
| **Qt のバイナリ配布物にライセンス文は含まれない。** `~/Qt/<ver>/<plat>/` の直下に法文は 0 件、`~/Qt/Licenses/` は Qt 自身の 5 ファイルのみ | `find` / `ls` |
| **Qt 6.8 以降は SPDX 2.3 形式の SBOM を同梱する** | `doc.qt.io/qt-6/licenses-used-in-qt.html` の記載と、`~/Qt/6.11.1/macos/sbom/*.spdx.json` の実在 |
| `macdeployqt` の素の出力は 15 フレームワーク。**本体が直接必要とするのは 4 つだけ** | `otool -L` |
| `windeployqt` は既定で MSVC ランタイムを複製する | CI run 31412328017 |

Qt 公式は次のように述べており、後述の D8 と一致する。

> You only need to acknowledge and comply with the licenses of the third-party components
> that you are **actually shipping** with your application.

---

## 論点 1: 第三者コードをどうやって列挙するか（D8）

### 選択肢

**A. Qt 公式の第三者コード帰属ページを人が読み写す**
**B. Qt が同梱する SBOM から機械的に列挙する**

### 決定: B

Qt のバイナリに法文が無いことを実測したため、当初は A を計画していた。
**SBOM の存在が分かった時点で B に変えた。** 機械可読であり、人が読み写す工程が消える。

`packaging/licenses/from-qt-sbom.py` が、**同梱するターゲットから `DEPENDS_ON` を
推移的にたどって**第三者コードを集める。

**最初の実装は不完全だった。** `*_Attribution_*` という名前のパッケージだけを拾い、
`Bundled*`（`BundledLibjpeg` `BundledFreetype` `BundledHarfbuzz` `BundledLibpng`
`BundledPcre2`）を取りこぼしていた。**`QJpegPlugin` に何も出ていないことで気づいた**
（JPEG のプラグインが libjpeg を使わないはずがない）。推移閉包に直して 34 → 43 件になった。

---

## 論点 2: SBOM に現れないものをどう扱うか

`opengl32sw.dll`（Mesa）は Qt が Windows 向けに**ビルド済みバイナリとして同梱する**もので、
Qt のモジュールの依存ではないため **SBOM の `DEPENDS_ON` グラフに現れない。**

### 決定: 手で管理する受け皿を用意し、**1 件ごとに「なぜ SBOM に無いか」を書く**

`packaging/licenses/extra-components.json`。書けないものは
**同梱してよいか分かっていないということなので、止めて調べる。**

**`platforms` で配布物を限る。** Mesa は Windows にしか入らないので、macOS の
権利表示には載せない。**法的な文書なので、入っていないものを並べない。**

---

## 論点 3: `OR` を含むライセンス式のどちらを選ぶか

### 決定: **本体の GPLv3-or-later と両立する側を選び、選択を成果物に印字する**

| ライセンス式 | 選択 | 理由 |
|---|---|---|
| `FTL OR GPL-2.0-only` | **FTL** | **GPL-2.0-only は本体の GPLv3 と両立しない**（LGPLv3 の Qt から v2 へは下げられない。ADR-0012） |
| `AFL-2.1 OR GPL-2.0-or-later` | **GPL-2.0-or-later** | **AFL-2.1 は GPL と両立しない。** or-later 側は v3 を含む |
| `CC0-1.0 OR Apache-2.0` | **CC0-1.0** | どちらも両立する。より制約の少ない側 |

選ばなかった側の法文は同梱しない。**43 件のうち GPLv3 と両立しないものは無かった。**

---

## 論点 4: GPLv3 §6 の対応するソースの提供（D9）

### 決定: **リリースにソース tarball を添える**

`git archive` で配布物と同じタグのソースを作り、`third_party_licenses.txt` と
`README.md` に入手先を書く。**公開リポジトリへの URL だけに頼らない。**

---

## 論点 5: LGPLv3 §4 の再リンク可能性（D10）

### 決定: **動的リンクで満たし、再署名の手順まで書く**

Qt を独立した共有ライブラリとして同梱しているため、利用者は差し替えられる。
加えて `third_party_licenses.txt` に次を書く。

- Qt の版と入手先
- Katachi は Qt に改変を加えていないこと
- **署名済み `.app` はフレームワークを差し替えると署名が壊れる**ため、
  `codesign --force --deep --sign -` で再署名する必要があること

**「差し替えられます」とだけ書いて、実際にやると壊れる状態にしない。**

---

## 論点 6: MSVC ランタイムの同梱（D13）

`windeployqt` は既定で `msvcp140.dll` などを複製する。Qt 公式ドキュメントが明示している。

> These individual DLLs are **not intended or licensed for redistribution**, and should not
> be shipped directly. **Only the official Microsoft Redistributable installer** should be
> used for deployment on end-user systems.

### 決定: **`--no-compiler-runtime` で配置しない**

| 配布物 | 対応 |
|---|---|
| インストーラ | **公式の再頒布可能パッケージを実行する**（同梱されていれば） |
| ポータブル zip | **同梱しない。** 同梱の `README.txt` と `README.md` に**必要であると明記する** |

**「入っていないのに動くはず」と書かない。必要なら必要と書く。**

同じ範疇の懸念として、`d3dcompiler_47.dll` / `dxcompiler.dll` / `dxil.dll` は
**ビルド機の Windows SDK 由来**であるため `--no-system-d3d-compiler` /
`--no-system-dxc-compiler` で外す。

**`docs/licenses.md` §3 の訂正を伴う。** 同節は「ビルド時のみの依存」を Catch2 だけと
書いていたが、MSVC ランタイムは**成果物の実行に必要な第三者コード**である。§3.1 を起こした。

---

## 帰結

### 仕組み

| ファイル | 役割 |
|---|---|
| `packaging/licenses/from-qt-sbom.py` | SBOM から列挙し、`LicenseRef-*` の法文も書き出す |
| `packaging/licenses/fetch-spdx-texts.py` | 標準 SPDX 識別子の法文を正本から取得する |
| `packaging/licenses/extra-components.json` | SBOM に現れないものの受け皿（理由必須） |
| `packaging/licenses/spdx/` | 法文 22 種（標準 19 + SBOM 由来 3） |
| `packaging/licenses/SOURCES.md` | 出所・取得日・sha256・`OR` の選択理由 |
| `cmake/ThirdPartyLicenses.cmake` | 組み立てる。**法文が 1 つでも欠けたら `FATAL_ERROR`** |

生成物は 1,489 行 / 128 KB。macOS 43 件、Windows 44 件。

### 機械検査（Phase 4 T7）

**P5** が「配布物に入っているものと権利表示の整合」を、**P6** が
「LGPLv3 / GPLv3 / ソース入手先が揃っていること」を確かめる。
違反フィクスチャで、空振りしないことも確認している。

### **この列挙の限界（偽らないために書く）**

**機械で確かめられるのは「SBOM に記載がある範囲」までである。**

`QtCore` に静的に取り込まれた第三者コードは配布物にファイルとして現れない。
したがって**「配布物のファイル一覧」からは検出できず**、完全性は Qt の SBOM の
正確さに依存する。

P5 が確かめるのは**「配布物に入っている各ファイルに対応する記載があるか」**であって、
**「一覧が完全か」ではない。** この区別を、生成物・`docs/licenses.md` §4.3・
報告のすべてで崩さない。

### 本体のライセンスとの関係

- **`LICENSE`（本体の GPLv3 正本）は改変しない。** 成果物へはそのまま複製する
- **`packaging/licenses/` に置く第三者の法文は、本体のライセンスの対象外**
  （`docs/licenses.md` §1.3）。改変せず、GPLv3 を主張しない
