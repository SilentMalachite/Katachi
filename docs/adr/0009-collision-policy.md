# ADR-0009: 衝突ポリシーは FileSink が適用し、スキップは IoError で表す

- 状態: 承認
- 日付: 2026-08-09

---

## 背景

ADR-0005 は「名前の生成は core、衝突の解決は io」と決め、衝突ポリシーの実装を Phase 2 へ送った。
その宿題を果たすにあたり、置き場所と表し方の 2 つが未定だった。

**1. 適用する場所。** 衝突判定には出力先の実在確認が要る。
1000 件のバッチで main thread が事前に 1000 回 `stat` すると、それだけで UI が止まる
（`docs/phases.md` §4 の受け入れ基準に反する）。

**2. スキップの表し方。** `docs/cpp-conventions.md` §2.2 は `ByteSink` の形を固定している。

```cpp
template <typename T>
concept ByteSink = requires(T& t, const QByteArray& bytes) {
    { t.write(bytes) } -> std::same_as<Result<std::monostate, IoError>>;
};
```

`CollisionPolicy::Skip` で書き出さなかったことは**失敗ではない**が、
この戻り値型に「書かなかった」を載せる場所が無い。
ADR-0004 は同種の問題（警告の置き場所）を「成功値の側に載せる」で解いたが、
ここでの成功値は `std::monostate` であり、何も載せられない。

## 選択肢

### A. `IoError::DestinationExists` として返し、表示側で読み替える

`ByteSink` の形を変えない。app 層が「スキップ（既存）」と表示し、失敗件数に数えない。

### B. `ByteSink` の戻り値型を変える

`Result<WriteOutcome, IoError>` のようにして、成功値にスキップを載せる。
`docs/cpp-conventions.md` §2.2 の変更を伴う。

### C. `FileSink` の外で衝突を解決し、スキップなら `FileSink` を作らない

`JobRunner` が衝突を判定する。ただし `JobRunner` は `ByteSink` を型引数で受け取るテンプレートであり、
ファイルシステムに触れないことがテスト容易性の根拠（`docs/cpp-conventions.md` §2.3）。
そこへ実在確認を持ち込むと、その根拠が崩れる。

## 決定

**A を採る。`FileSink` が書き出しの直前に衝突を解決し、スキップは `IoError::DestinationExists` で返す。**

理由。

1. **`docs/cpp-conventions.md` §2.2 の concept を変えずに済む。**
   B は「1 つのスキップを表すために、Phase 1 で確定した契約を動かす」ことになる。
2. **「宛先が既に存在し、ポリシーが Skip のため書かなかった」は io 層の事実である。**
   `IoError` を「io 層で起きた、呼び出し側が知るべき結末」と読めば、嘘ではない。
   *エラー*と*結末*の区別は app 層の表示で行う。
3. **衝突判定がワーカースレッドに乗る。** 実在確認は `FileSink::write()` の中、
   すなわちワーカースレッドで、しかも**そのファイルを書く直前**に行われる。
   main thread は 1 度も `stat` しない。事前判定より正確でもある
   （バッチ実行中に外部でファイルが増えても正しく振る舞う）。

## 帰結

- **`FileSink` は「出力ディレクトリ + 希望する名前 + `CollisionPolicy`」を持って構築される。**
  実際の出力パスは `write()` の中で決まる。呼び出し側は `resolvedPath()` で後から取得する。
  このメンバは `ByteSink` concept には含めない（concept が要求する操作は 4 個以下 —
  `docs/cpp-conventions.md` §2.6）。`JobRunnerBridge` は具象型を知っているため取得できる。
- **既定は `Skip`。** ADR-0005 の「破壊的操作を既定にしない」を引き継ぐ。
  `SettingsPanel` の初期表示も `Skip` とし、テストで固定する。
  **既定値を確認せずに `Overwrite` にしないこと。**
- **`Rename` は `_1` から順に試し、上限 10000 で打ち切って `IoError::WriteFailed` を返す。**
  上限が無いと、名前が埋まった状況で無限ループする。
- **`Overwrite` も `QSaveFile` 経由で置換する。** 書き出しに失敗したときに、
  既存のファイルを壊したまま終わらない。
- **app 層は `DestinationExists` を「スキップ（既存）」と表示し、失敗件数に数えない。**
  結果一覧には行として残す（`docs/phases.md` §4 の「失敗したジョブが理由付きで残る」と同じ扱い）。
- **`Overwrite` を選んだ状態で実行を開始するときだけ、確認のモーダルを出す。**
  `docs/spec-core.md` §7 が認める「破壊的操作の確認」に該当する。詳細は ADR-0010。
- ADR-0005 が Phase 1 で定義しないと決めた `CollisionPolicy` の列挙は、
  ここで初めて `src/io/CollisionPolicy.hpp` に置かれる。**core には置かない。**
