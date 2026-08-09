# Phase 3 進捗（拡張コーデック）

**追記のみ。既存の記述は書き換えない**（`CLAUDE.md`）。
誤りを見つけた場合も遡って修正せず、新しい日付で訂正を追記する。

---

## 2026-08-09 — Phase 3 に着手。計画の承認を得た

### 実施内容

`main`（`d982c0d`）から `phase3` ブランチを切り、Phase 3 の計画を提示して承認を得た。
本エントリはその計画と、計画の前提として**実測した事実**の記録である。

### 読んだ文書

`CLAUDE.md` / `docs/progress/phase2.md`（末尾のクローズ記録）/ `docs/phases.md` /
`docs/spec-core.md` / `docs/licenses.md` / `docs/agent-protocol.md` /
`docs/format-matrix.md`（ローカル生成物）/ `docs/adr/0003-metadata-policy.md` /
`docs/adr/0007-capability-probing.md`

コードは `src/core/CapabilityTable.hpp/.cpp` / `src/core/Concepts.hpp` /
`src/app/SettingsPanel.cpp` / `tools/format_matrix.cpp` / `cmake/FormatMatrix.cmake` /
`tests/CMakeLists.txt` / `tests/core/capability_table_test.cpp` /
`.github/workflows/ci.yml` を読んだ。

### 着手前に実測した事実（推測ではない）

| 事実 | 根拠 |
|---|---|
| ローカル Qt 6.11.1 / macOS 14 arm64 の imageformats プラグインは **11 個**: `qgif` `qicns` `qico` `qjpeg` `qmacheif` `qmacjp2` `qsvg` `qtga` `qtiff` `qwbmp` `qwebp` | `ls $(qmake6 -query QT_INSTALL_PLUGINS)/imageformats` の実行結果 |
| **AVIF / JPEG XL / PSD / RAW のプラグインは Qt に同梱されていない** | 同上（一覧に無い） |
| HEIF が読み書きできているのは `libqmacheif`（**macOS 固有**）による | 同上 + `docs/format-matrix.md` で `heic` / `heif` が読み書きとも `o` |
| 受け入れ基準 3「能力表が追加コーデックを自動的に反映する（コード変更不要）」は **ADR-0007 の実測方式により設計上すでに満たされている** | `CapabilityTable::buildFromQt()` がメモリ上の往復で性質を判定。`src/app/SettingsPanel.cpp:67` が `encodable()` から出力形式の候補を作る |
| 受け入れ基準 2「追加コーデックが無い環境でもビルド・起動する」は、追加コーデックを **`QImageIOPlugin`（実行時ロード）に限れば**自動的に満たせる | アプリ本体が第三者コーデックへリンクしない構成が取れる |

**Windows 側の実測はまだ無い。** `qmacheif` は macOS 固有なので Windows では
HEIF が落ちているはずだが、**これは推測であり、T1 で CI の実測に置き換える。**

### 承認された決定

| # | 論点 | 決定 |
|---|---|---|
| D1 | 導入方式 | 第三者 `QImageIOPlugin` を `find_package` で探してビルドする（既定 OFF）。**自前で `QImageIOPlugin` を書く案は採らない**（`src/` に新層が増え `docs/spec-core.md` §1 の構成を変えるため） |
| D2 | 対象コーデック | **AVIF / JPEG XL / PSD / RAW** の 4 つ。**HEIF は自前導入しない**（macOS は `qmacheif` で既に読み書き可。Windows は D3 の懸念があるため） |
| D3 | HEIF エンコーダ | libheif の HEVC エンコーダ backend に GPL-2.0-only のものがあると `docs/licenses.md` §5 の表により本体 GPLv3 とリンクできない。**現時点で未検証。T2 で一次情報を確認するまで判断しない** |
| D4 | 既定値 | `KATACHI_EXTRA_CODECS=OFF` を既定とする。**既定パスが常に「追加コーデックが無い環境」になり、受け入れ基準 2 が毎回の品質ゲートで検証される** |
| D5 | テスト専用ダミープラグイン | **採用する。** 実コーデックの可用性と独立に受け入れ基準 3 を検証できる（T4） |
| D6 | EXIF 全体保持（ADR-0003 の宿題） | **Phase 3 では見送り、ADR に「見送る」と決着を書く。** 受け入れ基準 3 項目に含まれず、外部 EXIF ライブラリは本体が**リンク**するため「オプショナル依存」の枠に収まらない |

`docs/spec-core.md` §1 のディレクトリ図へ `cmake/ExtraCodecs.cmake` と `tests/plugins/` を
追記することも併せて承認された（**指示書の変更**にあたるため計画時に申告した）。

### タスク分割

| # | 内容 | 主な変更先 |
|---|---|---|
| T0 | 着手記録（本エントリ） | `docs/progress/phase3.md`（新規）/ `docs/phases.md` §5.4 |
| T1 | OS 別の現状を CI で実測。`format-matrix.md` を artifact 化 | `.github/workflows/ci.yml` |
| T2 | ライセンス確定（**導入より先**。受け入れ基準 1） | `docs/licenses.md` §5 / `docs/adr/0013-extra-codecs.md`（新規） |
| T3 | オプショナル依存の枠組み（実コーデックはまだ入れない） | `cmake/ExtraCodecs.cmake`（新規）/ `CMakeLists.txt` / `CMakePresets.json` |
| T4 | 自動反映の機械検査（ダミープラグイン + 横断テスト） | `tests/plugins/TestFormatPlugin.cpp`（新規）/ `tests/core/extra_codec_test.cpp`（新規）/ `tests/CMakeLists.txt` |
| T5 | 実コーデック導入（T2 で可と決まったものだけ。1 コーデック 1 コミット） | `cmake/ExtraCodecs.cmake` / `.github/workflows/ci.yml` |
| T6 | CI に `KATACHI_EXTRA_CODECS=ON` ジョブを追加 | `.github/workflows/ci.yml` |
| T7 | ADR-0003 の宿題に決着 | `docs/adr/0014-exif-preserve-all.md`（新規） |
| T8 | 受け入れ基準の検証・記録・PR | `docs/progress/phase3.md` / `docs/phases.md` §4 |

### T4 で追加するテストと期待値（実装後に決めない）

| # | テスト | 期待値 |
|---|---|---|
| 1 | ダミープラグインが Qt に見えている | `QImageWriter::supportedImageFormats()` にダミー形式名が**含まれる**（前提の確認。ここが偽なら以下は空振り） |
| 2 | 能力表に自動で載る | `buildFromQt().find(dummy)` が値を返し `canDecode == true` かつ `canEncode == true` |
| 3 | ADR-0007 の実測が新形式にも効く | `supportsAlpha == true` かつ `isLossless == true`（プラグインをそう作るため） |
| 4 | UI の候補に自動で載る | `encodable()` にダミー形式が**含まれる** |
| 5 | 取りこぼしが無い | `encodable()` の id 集合 == `QImageWriter::supportedImageFormats()` を正規化した集合（**完全一致**） |
| 6 | 全 encodable 形式を横断して変換できる | フィクスチャ `gradient_rgb.png` を `encodable()` の**全形式**へ変換し、全件 `isOk()`、かつ出力を `QImageReader` で読み戻せる |

テスト 5 は既存テストに無い検査である（既存は「1 形式 1 件」「読み込み専用は false 固定」まで）。
テスト 6 は**データ駆動**であり、コーデックが増えれば検査対象が自動的に増える。
**T5 で形式名を書いたテストを足さない。** test 側から「ハードコードしない」原則を侵食するため。

### 通す品質ゲート

`CLAUDE.md` の 6 ゲートに加え、T3 以降は `KATACHI_EXTRA_CODECS=ON`（`dev-codecs` プリセット）
でのビルドとテストも通す。

### 今回やらないこと

- **多フレーム GIF / アニメーション**（`docs/spec-core.md` §8 の「Phase 3 以降の検討事項」。受け入れ基準に無い）
- **EXIF 全体の保持（`PreserveAll`）の実装**（T7 で判断のみ）
- **Phase 4 の配布時プラグイン同梱**（`macdeployqt` / `windeployqt` と第三者ライセンス文の収集は Phase 4）
- **`QStringLiteral` 禁止スキャナ**（`phase2.md` 末尾の未決の提案。Phase 3 に混ぜない。別 PR とすべき）
- **Phase 2 の未達 2 件**（キーボードのみでの実操作確認 / Windows 実機起動）。利用者にお願いする項目で、Phase 3 とは独立に残る

### 停止条件の事前申告

| 停止条件 | 該当箇所 |
|---|---|
| 1（指示書にない依存） | Phase 3 の本質。D1・D2・D5 について承認を得た |
| 7（ライセンス判断） | T2 の結論は提示して承認を得てから T5 に進む |
| 8（1 タスク 400 行超） | **T4 が 250〜350 行の見込み。**超えそうになったら分割して報告する |

### 変更ファイル

- 追加: `docs/progress/phase3.md`（本ファイル）
- 変更: `docs/phases.md`（§5.4 を追加）

### 追加・変更したテスト

**なし。** T0 は文書のみ。

### 品質ゲートの実行結果

文書のみの変更のため、コンパイル系のゲートは対象外。
`ctest --preset dev` を回帰確認として実行した結果は次のコミットで記録する。

### 推測で埋めた箇所

**Windows で HEIF が使えないという見込み。** `qmacheif` が macOS 固有であることから
そう考えているが、**Windows 実機・CI での実測はまだ無い。T1 で実測に置き換える。**
それ以外は上の「着手前に実測した事実」のとおり、すべて手元の実行結果に基づく。

### 残課題 / 次にやること

1. T1: CI で両 OS の `format-matrix.md` を artifact 化し、OS 別対応表をここに貼る
2. T2: ライセンス調査（T1 の結果を見てから対象を最終確定する）
3. `.serena/` は未追跡のまま（Phase 2 から継続）

---

## 2026-08-09 — T1 完了。OS 別の対応形式を CI の実測で確定した

### 実施内容

`.github/workflows/ci.yml` の `build-and-test` ジョブに、ビルド直後の
`docs/format-matrix.md` を artifact として上げる手順を足した。
run 31314295650（commit `a3e1e5e`）で 4 ジョブとも success、両 OS の生成物を入手した。

### 実測結果（**Qt 6.8.3 / `modules: qtimageformats` 込み。両 OS とも同一 Qt 版**）

| 形式 | macOS | Windows |
|---|---|---|
| bmp / cur / gif / icns / ico / jpeg / pbm / pgm / png / ppm / svg / svgz / tga / tiff / wbmp / webp / xbm / xpm（**18 形式**） | 記載どおり | **完全に同一**（拡張子・読み・書き・アルファ・品質・可逆の 6 列すべて一致） |
| **heic** | 読 o / 書 o / 品質 o | **無し** |
| **heif** | 読 o / 書 o / 品質 o | **無し** |
| **jp2** | 読 o / 書 o / 品質 o / 可逆 o | **無し** |

**macOS のみ 21 形式、Windows は 18 形式。差は `heic` / `heif` / `jp2` の 3 つだけ。**

### 着手記録の推測に対する答え合わせ

T0 で「Windows では HEIF が落ちているはずだが**推測である**」と書いた。
**実測の結果、推測は当たっていた。** さらに `jp2` も Windows に無いことが分かった。
これは T0 で予想していなかった。macOS の `libqmacjp2` は Apple の Image I/O を使う
macOS 固有プラグインであり、Windows 側には対応するものが無いためと考えられる
（**この因果の説明自体は未検証。事実は「Windows に jp2 が無い」ことのみ**）。

### この結果が Phase 3 に与える意味

- **AVIF / JPEG XL / PSD / RAW はどちらの OS にも無い。** D2 の対象選定は妥当だった
- **HEIF を自前導入しない（D2）と、Windows では HEIF が使えないままになる。**
  macOS だけが読み書きできる状態が残る。これは「対応フォーマットが OS で異なる」
  という利用者から見える差であり、Phase 4 の配布時に説明が要る
- `jp2` も同じ形の OS 差である。**どちらも今回の対象外だが、記録しておく**

### 変更ファイル

- 変更: `.github/workflows/ci.yml`（artifact 取得の手順を 8 行追加）
- 変更: `docs/progress/phase3.md`（本エントリ）

### 追加・変更したテスト

**なし。** 生成物の検証は既存の `tests/core/format_matrix_test.cpp` が担っている。

### 品質ゲートの実行結果

| # | 実行 | 結果 |
|---|---|---|
| 1 | ローカル `cmake --build --preset dev` | exit 0 / 警告 0（`ninja: no work to do.`） |
| 2 | ローカル `ctest --preset dev` | **166 / 166 pass** |
| 3 | CI run 31314295650（4 ジョブ） | **すべて success** |

CI の 4 ジョブが 6 ゲート全数を担っている。

### 推測で埋めた箇所

**Windows に `jp2` が無い理由**を「`libqmacjp2` が macOS 固有だから」と書いたが、
これは一般的な Qt の慣習からの推論であり（`docs/agent-protocol.md` §1 の 5）、
**Windows 側のプラグイン一覧を実測してはいない。** 事実として確定しているのは
「Windows の能力表に `jp2` が無い」ことだけである。

### 残課題 / 次にやること

1. T2: ライセンス調査の結果を記録する
2. Phase 4 で「対応形式が OS によって異なる」ことを利用者向けに説明する必要がある

---

## 2026-08-09 — T2 の調査を完了。**判断を仰ぐため停止する**（停止条件 7）

### 実施内容

追加コーデックのライセンスと導入方式を一次情報で調べ、`docs/licenses.md` §5.1 / §5.2 と
`docs/adr/0013-extra-codecs.md`（**状態: 提案（承認待ち）**）に記録した。
**まだ何も導入していない。**

### 一次情報で確認したこと（すべて 2026-08-09 に取得）

| 対象 | ライセンス | GPLv3 との両立 |
|---|---|---|
| libavif 1.4.2 | BSD-2-Clause | 可 |
| libjxl 0.12.0 | BSD-3-Clause | 可 |
| LibRaw 0.22.2 | LGPL-2.1 / CDDL-1.0 の選択制（LGPL-2.1 を選ぶ） | 可 |
| kimageformats の `psd` / `raw` プラグイン | LGPL-2.0-or-later | 可 |
| kimageformats の `avif` / `jxl` プラグイン | BSD-2-Clause | 可 |
| ECM v6.20.0 | BSD 系（ビルド時のみ・成果物に含まれない） | — |

出典 URL は `docs/licenses.md` §5.1 に列挙した。
LGPL 系が GPLv3 と両立する根拠は LGPL 第 3 条であり、**条文を実際に読んで確認した**
（LibRaw 同梱 `LICENSE.LGPL` の 211 行目）。

### 訂正 1: x265 は GPL-2.0-only ではなく **GPL-2.0-or-later** だった

計画時に「HEIF の HEVC エンコーダが GPL-2.0-only ならリンク不可」と述べ、
**未検証と明記していた前提が誤りだった。** x265 のソースヘッダは
"either version 2 of the License, or (at your option) any later version" と述べている。
libheif 本体は LGPL-3。**どちらも GPLv3 と両立する。**

**この結果、HEIF を入れる選択肢はライセンス上は開いている。**
D2（HEIF を自前導入しない）は維持できるが、**その理由は変わる**。
維持する場合の理由は「macOS では `qmacheif` で既に読み書きできる（T1 の実測）ため
得られるのは Windows 側のみ」と「libheif + HEVC エンコーダという依存の重さ」である。

### 訂正 2: 「`find_package` で探してビルド」という計画時の言い方は不正確だった

計画では D1 を「第三者 `QImageIOPlugin` を `find_package` で探してビルド」と書いたが、
調べた結果、**`find_package` で探すのはコーデック本体（libavif / libjxl / LibRaw）であり、
プラグインそのものはソースから取得してビルドする**必要がある。理由は次のとおり。

**Qt のプラグインはアプリと同じ Qt に対してビルドされている必要がある。**
Homebrew の kimageformats は Homebrew の Qt に対して作られるため、公式インストーラ /
aqtinstall の Qt を使う本プロジェクトでは 1 プロセスに 2 つの Qt が載る。
**この点は理屈による判断であり、実際に混ぜて試してはいない**（下記「推測で埋めた箇所」）。

### 判明した技術的制約

| # | 事実 | 意味 |
|---|---|---|
| 1 | 候補プラグインは**いずれも ECM を要求する** | ECM も固定タグで取得する必要がある |
| 2 | **PSD と RAW のプラグインは kimageformats にしか無い** | novomesk の単体プラグイン（AVIF / JXL のみ）では 4 形式を賄えない |
| 3 | kimageformats **最新 v6.28.1 は Qt 6.9.0 / CMake 3.29 要求** | CI の Qt 6.8.3 固定（`docs/phases.md` §1.5、Phase 0 の決定）と**衝突する** |
| 4 | **v6.20.0 は Qt 6.8.0 / CMake 3.16 / ECM 6.20.0 要求で、4 プラグインすべてを含む** | **タグを v6.20.0 に固定すれば Phase 0 の決定を崩さずに済む** |
| 5 | kimageformats は 25 プラグインを持ち、`kimg_tga` / `kimg_jp2` は Qt 同梱の `qtga` / `qjp2` と**同じ形式を扱う** | 全部置くと同一形式のプラグインが 2 つ載り、どちらが使われるか不定になる。**4 つだけ配置する** |
| 6 | `kimg_heif` は既定 OFF で、libheif が見つかったときのみ作られる | HEIF を足す判断をしても構成の変更は小さい |

制約 3 は `CLAUDE.md` 停止条件 6（既存の決定と矛盾する実装）に触れる可能性があったが、
**制約 4 により回避できる見込みが立った。** Qt の下限を上げる判断は行わない。

### 決定案（ADR-0013。**承認待ち**）

`ExternalProject_Add` で ECM v6.20.0 と kimageformats v6.20.0 を固定タグでビルドし、
**`kimg_psd` / `kimg_raw` / `kimg_avif` / `kimg_jxl` の 4 つだけ**を
`${CMAKE_BINARY_DIR}/plugins/imageformats/` へ配置する。`KATACHI_EXTRA_CODECS` は既定 OFF。

`FetchContent` ではなく `ExternalProject_Add` を選ぶのは、別プロセスの CMake 実行になり
KDE 側のビルド設定（`KDECMakeSettings` 等）が我々の `-Werror` に干渉しないため。

### 変更ファイル

- 追加: `docs/adr/0013-extra-codecs.md`（状態: 提案（承認待ち））
- 変更: `docs/licenses.md`（§5.1 調査結果 / §5.2 HEIF についての訂正 を追加）
- 変更: `docs/progress/phase3.md`（本エントリ）

### 追加・変更したテスト

**なし。** T2 は調査と文書のみ。

### 品質ゲートの実行結果

**文書のみの変更のため、コンパイル系のゲートは実行していない。**
直前の T1 時点で dev ビルド警告 0 / 166 テスト green / CI 4 ジョブ success を確認済みで、
その後にコードは触っていない。

### 推測で埋めた箇所

1. **「別々の Qt に対してビルドされたプラグインを混ぜられない」という判断。**
   Qt の一般的な慣習（`docs/agent-protocol.md` §1 の 5）に基づく理屈であり、
   **実際に Homebrew 版プラグインを公式 Qt のアプリへ読み込ませて試してはいない。**
   この判断が選択肢 A の却下根拠になっているため、明記する
2. **kimageformats v6.20.0 が実際に Qt 6.8.3 でビルドできるかは未検証。**
   `CMakeLists.txt` の宣言（`REQUIRED_QT_VERSION 6.8.0`）を読んだだけである。
   T3 で実際にビルドして確かめる

### 残課題 / 次にやること（**ここで停止し、判断を待つ**）

1. **ADR-0013 の承認**（ライセンスの判断を含むため停止条件 7）
2. **HEIF を対象に加えるかの再判断。** 除外理由がライセンスから依存の重さへ変わったため
3. 承認後に T3（枠組み）へ進む
