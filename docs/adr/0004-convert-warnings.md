# ADR-0004: `convert()` の成功値に警告を載せる

- 状態: 承認
- 日付: 2026-08-09

---

## 背景

`docs/spec-core.md` §4 のアルファ処理表は、次の 1 行を定めている。

| 入力 | 出力形式 | AlphaPolicy | 挙動 |
|---|---|---|---|
| アルファあり | 非対応 | `Preserve` | **`Flatten` にフォールバックし、警告を結果に含める** |

一方 §2 の `convert()` は当初 `Result<QByteArray, ConvertError>` を返す定義だった。
`Result<T,E>` は成功なら `T`、失敗なら `E` のいずれか一方しか持たない。
**この行は「成功したが警告がある」状態を要求しているのに、その置き場所が型に無い。**

指示書内の矛盾であり、`CLAUDE.md` 停止条件 2 に該当するため、実装前に停止して判断を仰いだ。

なおこの行は落とせない。「アルファを保持したい」と指定した利用者に対し、
黙ってアルファを捨てるのは情報の欠落であり、`docs/spec-core.md` §4 が
「取りこぼしやすい」と明示している箇所そのものである。

## 選択肢

### A. 成功値を構造体にし、警告を含める

```cpp
struct ConversionOutput {
    QByteArray                  bytes;
    std::vector<ConvertWarning> warnings;
};
[[nodiscard]] Result<ConversionOutput, ConvertError> convert(...) noexcept;
```

### B. 出力引数で返す

```cpp
convert(source, spec, caps, std::vector<ConvertWarning>* warnings) noexcept;
```

### C. 警告を持たず、§4 の表から当該記述を削る

## 決定

**A を採る。`convert()` の戻り値を `Result<ConversionOutput, ConvertError>` に変更する。**

理由。

1. **`docs/spec-core.md` §4 の要求を素直に満たす。** 成功と警告が同時に成立する状態を、
   型がそのまま表現する。
2. **`ResultValue` 制約を満たす。** `ConversionOutput` は `QByteArray` と `std::vector` の
   2 メンバで、いずれも nothrow move 構築可能。したがって暗黙の move ctor も `noexcept` になり、
   `docs/cpp-conventions.md` §2.2 の `ResultValue`
   （`std::is_nothrow_move_constructible_v<T>`）を満たす。**`Result` 側の変更は不要。**
3. **Phase 2 の UI が理由を表示できる。** `docs/phases.md` §4 Phase 2 の受け入れ基準
   「失敗したジョブが結果一覧に理由付きで残る」と同じ形で、成功したが警告のあるジョブも
   理由を示せる。
4. B を採らない理由: 出力引数は純粋関数のシグネチャを濁す。呼び出し側が
   `nullptr` を渡せてしまい、「警告を無視する」経路が型で許されてしまう。
   `docs/cpp-conventions.md` §1 の方針（値で返す、`[[nodiscard]]`）とも相性が悪い。
5. C を採らない理由: 指示書の要求そのものを削ることになる。矛盾の解消として、
   仕様の要求を落とす方向は最後の手段であり、ここでは型を足すだけで解決できる。

## 帰結

- `docs/spec-core.md` §2 に `ConvertWarning` と `ConversionOutput` を追加し、
  `convert()` のシグネチャを変更した。§4 の表 3 行目に、警告の具体的な手段が
  `ConvertWarning::AlphaFlattenedFallback` であることを明記した。
- `ConvertWarning` は `src/core/ConvertError.hpp` に置く。専用ファイルを作らないのは
  `docs/agent-protocol.md` §6「ファイルを新規作成する前に、既存ファイルへの追記で
  済まないか検討する」に従ったもの。
- **警告は `enum class` の列挙とし、文字列にしない。** 文言は表示層（`src/app`）の責務であり、
  core に文字列リテラルを置けないため（`CLAUDE.md` の絶対禁止、不変条件スキャナ INV3A）。
- テストは次の 2 方向で行う。
  - アルファ表 3 行目で `warnings` に `AlphaFlattenedFallback` が**ちょうど 1 件**含まれること
  - **それ以外の経路では `warnings` が空であること**（警告が濫発されないこと）
- 将来 `ConvertWarning` に列挙値を足す場合、`docs/spec-core.md` §4 の表と
  対応関係を保つこと。表に無い警告を足さない。
