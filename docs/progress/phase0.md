# Phase 0 — 基盤構築

**追記のみ。既存の記述は書き換えない。** 誤りを見つけた場合も遡って修正せず、新しい日付で訂正を追記する。

---

## 2026-08-09 — 初回セッション / 着手予定（承認待ち）

### 実施内容

実装は未着手。`agent-protocol.md` §7 の 1〜2（通読・不明点の列挙・回答取得・本ファイルの作成）まで。

### 読み込んだファイル

- `CLAUDE.md`（リポジトリ直下）
- `phases.md` / `spec-core.md` / `cpp-conventions.md` / `agent-protocol.md`
  — **いずれもリポジトリ直下にあった。`docs/` 直下ではない**（Phase 0 受け入れ基準に違反した状態）
- `docs/progress/` — 存在しなかった（本ファイルが最初のエントリ）
- `docs/adr/` — 存在しなかった
- `docs/licenses.md` — 存在しなかった（Phase 0 受け入れ基準が要求）

### 環境調査の結果（実測値。推測ではない）

| 項目 | 実測値 | 備考 |
|---|---|---|
| CMake | 4.4.2 | 要件 3.24 以上を満たす |
| Ninja | 1.13.2 | — |
| コンパイラ | Apple clang 21.0.0 (clang-2100.1.1.101) | C++20 可 |
| Qt | `~/Qt/6.11.1/macos`（Widgets / Gui / Concurrent を確認） | ローカルは 6.11。CI は `phases.md` §1.5 に従い 6.8 系 LTS に固定する |
| Qt 画像プラグイン（ローカル実測） | gif, icns, ico, jpeg, macheif, macjp2, pdf, svg, tga, tiff, wbmp, webp | 組込の png / bmp / ppm / xbm / xpm を除く |
| clang-format / clang-tidy | `/opt/homebrew/opt/llvm/bin/`（Homebrew LLVM 22.1.8） | Xcode 同梱は clang-format のみで clang-tidy が無い。**LLVM 22 側に統一する** |
| Catch2 最新タグ | v3.15.3（GitHub API で取得） | 固定タグ候補 |
| git | 2.50.1 / **リポジトリ未初期化** | 品質ゲート 2 本が `git ls-files` に依存するため Phase 0 で初期化する |
| gh | 2.97.0 | — |

### 指示書に指定が無く、判断を仰いだ事項と回答

| # | 事項 | 回答 |
|---|---|---|
| 1 | git 初期化とリモート作成の分担 | **ローカルのコミットまでをエージェントが行う。** GitHub リモートの作成と push は利用者が行う |
| 2 | 不変条件スキャナ 6 種の実装手段（`phases.md` §3 に言語指定が無い） | **CMake script（`cmake -P`）**。新規依存が増えず、macOS / Windows で同一挙動 |
| 3 | スキャナ #3「フォーマット名の文字列リテラル」の判定方法 | **`src/core` は文字列リテラルを全面禁止**（`src/core/FormatId.hpp` のみ除外）。**`src/app` はスキャナが保持するフォーマット名一覧との照合**（`tr("...")` を許すため） |
| 4 | チェックリストに無い追加項目の可否 | **採用**: スキャナのネガティブテスト / Catch2 配線 + Qt 画像プラグイン smoke テスト / `src/app` をライブラリ + 薄い `main.cpp` に分割。**不採用**: `LICENSE` ファイルの配置（Phase 0 では行わない） |

### 未解決 / 保留（推測で埋めていない箇所）

1. **CI で固定する Qt 6.8 系のパッチ版が未確定。** `aqtinstall` で実在するバージョンを確認してから固定する。**現時点で番号を推測して書かない。**
2. **CI の clang-format / clang-tidy のメジャー版。** ローカル（LLVM 22）と CI ランナーの版が異なると整形結果が食い違うため、CI 側でも版を固定する。固定値はランナーで入手可能な版を確認してから決める。
3. **`phases.md` §5.2 のメタデータ保持方針**は Phase 1 着手時の判断事項。Phase 0 では触れない。
4. **Phase 0 受け入れ基準の「GitHub Actions が macOS / Windows の両方でビルド + テストを通す」は、利用者が push するまで検証不能。** ローカル作業完了時点では未達として扱い、push 後に本ファイルへ追記する。
5. **Windows は実機を持たないため、実機起動確認を行えない。** 受け入れ基準の要求どおり「CI ビルドのみ・実機未確認」と報告に明記する。

### 着手予定（承認後に実施する順序）

1. `git init` + `.gitignore` + 初回コミット、`phase0` ブランチ作成
2. 参照 4 文書を `docs/` 直下へ移動
3. `.clang-format` / `.clang-tidy` の配置
4. `CMakeLists.txt` / `CMakePresets.json` / `cmake/QualityGates.cmake`
5. `src/app`（空のメインウィンドウ。ライブラリ + 薄い `main.cpp`）
6. 不変条件スキャナ 6 種 + 違反フィクスチャ + ネガティブテスト 6 種
7. Catch2 配線（v3.15.3 固定 + `FIND_PACKAGE_ARGS`）と Qt 画像プラグイン smoke テスト
8. `docs/licenses.md` / `docs/adr/0001-ui-toolkit.md`
9. `.github/workflows/ci.yml`
10. 品質ゲートをローカルで全て実行し、本ファイルへ結果を追記

### 今回やらないこと

- `src/core` の型実装一切（`Result` / `FormatId` / `CapabilityTable` / `ConversionSpec` / `convert()` / `NamingRule`）→ Phase 1
- `src/io` 一切 → Phase 2
- `docs/adr/0002-noexcept-and-allocation.md` → Phase 1
- `docs/format-matrix.md` の自動生成 → Phase 1
- 追加コーデックの調査・導入 → Phase 3
- 配布・署名・公証 → Phase 4
