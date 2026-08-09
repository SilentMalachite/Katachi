#pragma once

// バッチ実行時のメモリ上限（ADR-0008 / docs/phases.md §5.1）。
//
// `convert()` が `QByteArray` を受ける以上、同時にメモリへ載る量は並列度で決まる。
// `maxPixels` の既定を `ARGB32` で 1 枚持つと 1 GiB あり、並列度を掛けると破綻する。
//
// **予算の役目は「1 枚分の消費をスレッド数倍しないこと」であり、
// 予算を超える単独ジョブを諦めさせることではない。**
// 見積りが総量を超えるジョブは総量の全部を確保して単独で走る。
//
// 単位を MiB にするのは、バイトのまま `QSemaphore`（`int` 単位）へ渡すと
// 総量を大きくしたときに溢れるため。

#include <QSemaphore>
#include <QSize>
#include <QtTypes>

#include <atomic>
#include <optional>

namespace katachi::io {

// 予算を数える単位。掛け算を qint64 で行うのは clang-tidy の
// bugprone-implicit-widening-of-multiplication-result による（int で計算してから広げない）。
inline constexpr qint64 budgetUnitBytes = 1024LL * 1024;

// 予算の総量。ADR-0008: `ConversionSpec::maxPixels` の既定 268,435,456 px を
// `ARGB32` で 1 枚保持した量と一致させる。「デコード済みの最大画像 1 枚分」が単位。
inline constexpr qint64 defaultBudgetBytes = 1LL << 30;

// 1 件あたりの見積り（ADR-0008）。
//
//   estimate = 2 * fileSize + 16 * max(sourcePixels, resizeBoundPixels)
//
// 2 * fileSize は入力バイト列と出力バイト列。出力は入力と同程度と仮定する。
// 16 * pixels は画像 4 枚 x 4 バイト/画素。**アルファ合成の最悪ケースに合わせた**
// （`src/core/Converter.cpp` の `flattenOnto()` で 4 枚が同時に生存する）。
// **合成やリサイズの実装を変えるときは、この係数を見直すこと。**
//
// sourceSize が無効な場合は「分からない」を意味し、予算全量に相当する値を返す。
// **寸法が分からない入力を小さいと仮定しない**（ADR-0008）。
[[nodiscard]] qint64 estimateJobBytes(qint64 fileSizeBytes, const QSize& sourceSize,
                                      const std::optional<QSize>& resizeBound) noexcept;

class MemoryBudget {
public:
    // 確保を表す RAII のガード。**破棄で必ず返る。**
    // 早期 return でもキャンセルでも取りこぼさない。
    class Reservation {
    public:
        Reservation(const Reservation&) = delete;
        Reservation& operator=(const Reservation&) = delete;
        Reservation(Reservation&& other) noexcept;
        Reservation& operator=(Reservation&& other) noexcept;
        ~Reservation();

    private:
        friend class MemoryBudget;
        Reservation(MemoryBudget& owner, int units) noexcept;

        void releaseNow() noexcept;

        MemoryBudget* owner_;
        int units_;
    };

    explicit MemoryBudget(qint64 totalBytes = defaultBudgetBytes) noexcept;

    // 予算が空くまで待つ。見積りが総量を超える場合は総量の全部を確保する
    // （単独実行になる。デッドロックしない）。
    [[nodiscard]] Reservation acquire(qint64 estimatedBytes);

    [[nodiscard]] int totalUnits() const noexcept;
    [[nodiscard]] int usedUnits() const noexcept;

    // 同時確保量の最大値。受け入れ基準の検証（T10）で使う。
    [[nodiscard]] int peakUnits() const noexcept;

private:
    [[nodiscard]] int unitsFor(qint64 bytes) const noexcept;

    void release(int units) noexcept;

    QSemaphore semaphore_;
    qint64 totalBytes_;
    int totalUnits_;
    std::atomic<int> used_ = 0;
    std::atomic<int> peak_ = 0;
};

} // namespace katachi::io
