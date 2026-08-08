# Katachi

Qt6 / C++20 のクロスプラットフォーム画像フォーマット変換アプリ。
**純粋関数コア + Qt Widgets UI の 2 層**。対応フォーマットはハードコードせず、実行時に `QImageReader` / `QImageWriter` から能力表を生成する。

| 項目 | 決定 |
|---|---|
| 言語 | C++20（`std::expected` は C++23 のため不可。自前 `Result<T,E>`） |
| フレームワーク | Qt 6.8 LTS 以上、**Qt Widgets**（Qt Quick 不採用） |
| ビルド / テスト | CMake 3.24 以上 + `CMakePresets.json` / Catch2 v3 + CTest |
| 対象 OS | macOS 13 以上（universal）、Windows 10 以上（x64） |
| ライセンス | 本体 Apache-2.0。Qt は LGPLv3 のため **動的リンクのみ** |

---

## 絶対禁止（他のファイルを読んでいなくても守る）

- **アプリ実行時のネットワーク通信全般。** `QNetworkAccessManager` を include した時点で違反
  （ビルド時の依存取得は対象外 → `docs/phases.md` §1.5）
- `src/core` `src/app` にフォーマット名の文字列リテラル（`"png"` 等）を書く（例外は `src/core/FormatId.hpp` の変換関数のみ）
- `src/core` でのファイルアクセス・時刻取得・例外送出・グローバル可変状態
- 依存方向の逆流（正: `core → io → app`）。**concept も層ごとに置く**（`core/Concepts.hpp` / `io/IoConcepts.hpp`）
- `docs/progress/` の既存記述の書き換え（**追記のみ**）
- 品質ゲートを実行せずに「通った」と報告する
- 警告抑制プラグマ・`NOLINT` の追加
- UI にアニメーション / フェード / 自動スクロールを入れる

## 停止条件（該当したら実装を止め、報告して指示を待つ）

1. 指示書にない機能・依存が必要だと判断した
2. 指示書内に矛盾を見つけた
3. Qt の API の存在・挙動に確信が持てない（公式ドキュメントで確認できない）
4. **テストの期待値を緩めたくなった**
5. 品質ゲートが同じ理由で 2 回連続して落ちた
6. 既存の `docs/progress/` または `docs/adr/` の決定と矛盾する実装をしようとした
7. ライセンスの判断が必要になった
8. 1 タスクの差分が 400 行を超えそう（**Phase 0 の基盤構築は例外として承認済み**。他は計画段階で申告して承認を得る）
9. **「この設計は指示書より良い」と判断した**

9 の改善案は歓迎する。ただし実装してから提案しない。`docs/progress/phaseN.md` に「提案」として書き、判断を仰ぐ。
止まることは失敗ではない。推測で進むことが失敗である。

## セッション手順

1. **読み込み** — この CLAUDE.md、`docs/progress/` の最新、関連 ADR。読んだファイルを列挙する
2. **計画** — 実装前に提示し、**承認を待つ**。触るファイル / 追加するテストと期待値 / 通すゲート / 完了条件 / 今回やらないこと
3. **実装** — TDD。1 コミット = 1 変更。「ついでに」禁止
4. **検証と記録** — ゲートを自分で実行 → `docs/progress/phaseN.md` に追記 → 報告

承認なしに 3 へ進まない。「実装しました」だけの報告は未完了とみなす。

## 参照先（作業に応じて読む）

**この `CLAUDE.md` のみリポジトリ直下。他の 4 文書は `docs/` 直下に置く。**

| 読むタイミング | ファイル |
|---|---|
| セッション開始時（毎回） | `docs/progress/` の最新 |
| コードを書く前 | `docs/cpp-conventions.md`（C++20 規約・concept） |
| コア / GUI を実装するとき | `docs/spec-core.md`（型・能力表・アルファ・命名・並行・UI 要件） |
| Phase の着手・完了時 | `docs/phases.md`（分割・受け入れ基準・品質ゲート） |
| 進め方に迷ったとき | `docs/agent-protocol.md`（報告書式・サブエージェント・曖昧さの解決順序） |
| ライセンス判断 | `docs/licenses.md` |

## 品質ゲート

```bash
cmake --preset dev && cmake --build --preset dev   # 警告ゼロ（-Werror / MSVC: /WX）
ctest --preset dev --output-on-failure
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')
clang-tidy -p build/dev $(git ls-files 'src/*.cpp')
cmake --preset asan && ctest --preset asan
```

詳細と追加スキャナは `docs/phases.md`。

## 報告書式

```markdown
## 実施内容
## 変更ファイル（追加 / 変更 / 削除）
## 追加・変更したテスト（期待値も書く）
## 品質ゲートの実行結果
## 推測で埋めた箇所      ← 空欄禁止。無ければ「なし」
## 残課題 / 次にやること
```
