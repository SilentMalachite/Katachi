# ADR-0002: コア層の `noexcept` と確保失敗の扱い

- 状態: 承認
- 日付: 2026-08-09

---

## 背景

`docs/cpp-conventions.md` §1 は「コアの関数は `noexcept`。エラーは `Result` で返す」と定める。
一方 C++ の動的確保は `std::bad_alloc` を投げうる。`convert()` は `QImage` のデコードや
`QByteArray` のエンコード結果で確保を行うため、この 2 つはそのままでは両立しない。

`noexcept` 関数から例外が送出されると `std::terminate` が呼ばれる。
つまり「`noexcept` を付ける」という選択は、暗黙のうちに
「確保失敗時にプロセスを落とす」という選択を含んでいる。

この判断は `convert()` に限らない。`CapabilityTable::find()` は `std::optional<FormatCapability>`
を返し、`FormatCapability` は `QStringList` を含むため確保しうる。`encodable()` も `std::vector`
を返す。**確保を伴う `noexcept` なコア関数はすべて同じ問題を抱える。**

## 選択肢

### A. コア関数を `noexcept` に保ち、確保失敗時の `std::terminate` を受け入れる

規約どおり。確保失敗は回復不能として扱う。

### B. `noexcept` を外し、`std::bad_alloc` を呼び出し側へ伝播させる

正直だが、`docs/cpp-conventions.md` §1 の規約に反する。
また `src/core` での例外送出は `CLAUDE.md` の「絶対禁止」に該当する。

### C. `ConvertError` に `AllocationFailed` を足し、内部で `try/catch` して `Result` に変換する

一見すると丁寧だが、`catch (std::bad_alloc&)` の後に `Result` を構築する処理自体が
確保を必要としうる。メモリが枯渇した状態で確実に動く保証がない。
また `src/core` での例外の捕捉は、送出禁止の趣旨（コアを例外から切り離す）から外れる。

## 決定

**A を採る。コア層全体で `noexcept` を維持し、確保失敗時に `std::terminate` することを
意図的に受け入れる。**

理由。

1. **変換中の確保失敗は回復不能である。** 入力画像を保持できないメモリ状況で、
   部分的にデコードされた画像から「正しい出力」を作る道はない。
   壊れた出力を書くより即座に落ちる方が安全。
2. **`maxPixels` の事前チェックで、現実的な入力では到達しないようにする。**
   `ConversionSpec::maxPixels`（既定 268,435,456 = 16384 x 16384）を
   **デコード前**に判定し、超過する入力は `ConvertError::ImageTooLarge` で弾く。
   確保失敗は「上限内なのにメモリが足りない」という異常事態に限られる。
3. 選択肢 C は、メモリ枯渇下で追加の確保を行う点で、解決になっていない。

## 帰結

- **この方針は `convert()` だけでなく、確保を伴う全ての `noexcept` なコア関数に適用する。**
  `CapabilityTable::find()` / `encodable()`、`resolveOutputName()` を含む。
- **暗黙に変更しない。** 変えるならこの ADR を書き直す。
  個別の関数だけ `noexcept` を外すことを禁じる。
- `maxPixels` の事前判定は `convert()` の処理順序で**デコードより前**に置く。
  この順序はテストで固定する（デコード不能な巨大サイズ宣言で `ImageTooLarge` が返ること）。
- Phase 2 でバッチ実行時の同時メモリ量が問題になった場合、対処は
  `src/io` 側の並列度制御で行う。コアの `noexcept` 方針は動かさない
  （`docs/phases.md` §5.3 の検討事項）。
- ASan / UBSan のゲートは確保失敗を模擬しない。この方針の検証は上記の
  `maxPixels` 事前判定のテストで代替する。
