# ADR-0005: 名前の生成は core、衝突の解決は io

- 状態: 承認
- 日付: 2026-08-09

---

## 背景

`docs/spec-core.md` §5 は命名規則について 2 つのことを定めていた。

1. 純粋関数 `resolveOutputName(sourceBaseName, index, pattern, extension)` が
   `Result<QString, NamingError>` を返す
2. 衝突ポリシー `Overwrite` / `Skip` / `Rename`（`_1`, `_2` を付与）。既定は `Skip`

しかし次の 2 点が定まっていなかった。

- **`NamingError` の列挙値が定義されていない。** 戻り値の型に現れるが、中身の記述が無い。
- **衝突ポリシーを適用する場所が無い。** `resolveOutputName()` の引数に衝突ポリシーが無く、
  そもそも「衝突しているか」の判定には出力先の実在確認、すなわちファイルシステム参照が要る。
  `CLAUDE.md` は `src/core` でのファイルアクセスを**絶対禁止**としている。

`CLAUDE.md` 停止条件 2（指示書内の矛盾）に該当するため、実装前に停止して判断を仰いだ。

## 選択肢

### A. core は純粋な名前生成のみ。衝突解決は io 層（Phase 2）

`resolveOutputName()` のシグネチャを変えない。衝突ポリシーは Phase 2 で `src/io` に置く。

### B. 既存名の集合を引数で渡し、core 内で純粋に解決する

```cpp
resolveOutputName(..., const QSet<QString>& taken, CollisionPolicy policy)
```

ファイルシステムには触れないため core の禁止事項には抵触しない。
ただし `docs/spec-core.md` §5 のシグネチャ変更を伴い、
呼び出し側が「既存名の集合」を事前に集める責務を負う。

### C. Phase 1 では `NamingRule` を実装せず Phase 2 に送る

`docs/phases.md` §1 が Phase 1 の内容に「命名規則」を含めているため、Phase 分割の変更になる。

## 決定

**A を採る。`resolveOutputName()` は名前の生成のみを担い、衝突の解決は Phase 2 の `src/io` 層に置く。**

`NamingError` を次の 4 値で定義する。

```cpp
enum class NamingError {
    EmptyPattern,        // pattern が空
    UnknownPlaceholder,  // {} 内が既知の名前でない
    InvalidIndexSpec,    // {index:...} の桁指定が不正
    EmptyResult,         // 展開結果が空になった
};
```

理由。

1. **責務が層の境界と一致する。** 「与えられた材料から名前を組み立てる」のは純粋な計算であり core。
   「その名前が使えるか確かめて避ける」のは外界の観測であり io。
   `docs/spec-core.md` §1 の依存方向（`core → io → app`）と矛盾しない。
2. **B は Phase 1 の時点で早すぎる。** 「既存名の集合」をどう集めるかは、
   バッチ実行の設計（`docs/phases.md` §5.3 のメモリ上限を含む）に依存する。
   Phase 2 で並行実行の形が決まってから設計する方が、手戻りが少ない。
   B が必要になった時点で、この ADR を書き直して core に純粋関数を足せばよい。
3. **C は Phase 分割を動かす。** `docs/phases.md` の Phase 1 に「命名規則」が明記されている以上、
   実装できる部分（名前の生成）は Phase 1 でやる。

## 帰結

- `docs/spec-core.md` §5 に `NamingError` の定義と、衝突ポリシーの適用層が io であることを追記した。
  **衝突ポリシー自体の記述（`Overwrite` / `Skip` / `Rename`、既定 `Skip`）は削っていない。**
  Phase 2 での実装対象として残る。
- Phase 1 では `CollisionPolicy` の列挙を**定義しない**。使う場所が無いうちに型だけ作ると
  死んだコードになるため。Phase 2 で `src/io` に置く。
- `resolveOutputName()` は `noexcept`。確保失敗の扱いは ADR-0002 に従う。
- テストは `NamingError` の**全 4 値**に発生ケースを 1 つ以上用意する
  （`docs/phases.md` §2.2 のエラー種別テストと同じ方針）。
- 既定を `Skip` とする理由（破壊的操作を既定にしない）は Phase 2 に持ち越す。
  **Phase 2 で実装するとき、既定値を確認せずに `Overwrite` にしないこと。**
