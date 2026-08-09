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
