#include "io/MemoryBudget.hpp"

#include <QSemaphore>
#include <QSize>
#include <QtTypes>

#include <algorithm>
#include <atomic>
#include <limits>
#include <optional>
#include <utility>

namespace katachi::io {
namespace {

// 画像 4 枚 x 4 バイト/画素（ADR-0008）。アルファ合成の最悪ケース。
constexpr qint64 bytesPerPixelWorstCase = 16;
// 入力バイト列と出力バイト列。
constexpr qint64 fileSizeFactor = 2;

[[nodiscard]] qint64 pixelsOf(const QSize& size) noexcept {
    return static_cast<qint64>(size.width()) * size.height();
}

} // namespace

qint64 estimateJobBytes(qint64 fileSizeBytes, const QSize& sourceSize,
                        const std::optional<QSize>& resizeBound) noexcept {
    if (!sourceSize.isValid() || sourceSize.isEmpty()) {
        // 分からない入力を小さいと仮定しない。呼び出し側で総量へ丸められる。
        return std::numeric_limits<qint64>::max();
    }

    qint64 pixels = pixelsOf(sourceSize);
    if (resizeBound.has_value() && resizeBound->isValid()) {
        // 拡大する場合は出力側の寸法が支配的になる。縮小なら入力側のまま。
        pixels = std::max(pixels, pixelsOf(*resizeBound));
    }

    return (fileSizeFactor * fileSizeBytes) + (bytesPerPixelWorstCase * pixels);
}

MemoryBudget::Reservation::Reservation(MemoryBudget& owner, int units) noexcept
    : owner_(&owner), units_(units) {}

MemoryBudget::Reservation::Reservation(Reservation&& other) noexcept
    : owner_(other.owner_), units_(other.units_) {
    other.owner_ = nullptr;
    other.units_ = 0;
}

MemoryBudget::Reservation& MemoryBudget::Reservation::operator=(Reservation&& other) noexcept {
    if (this != &other) {
        releaseNow();
        owner_ = std::exchange(other.owner_, nullptr);
        units_ = std::exchange(other.units_, 0);
    }
    return *this;
}

MemoryBudget::Reservation::~Reservation() { releaseNow(); }

void MemoryBudget::Reservation::releaseNow() noexcept {
    if (owner_ != nullptr) {
        owner_->release(units_);
        owner_ = nullptr;
        units_ = 0;
    }
}

MemoryBudget::MemoryBudget(qint64 totalBytes) noexcept
    : totalUnits_(static_cast<int>(std::max<qint64>(1, totalBytes / budgetUnitBytes))) {
    totalBytes_ = static_cast<qint64>(totalUnits_) * budgetUnitBytes;
    semaphore_.release(totalUnits_);
}

MemoryBudget::Reservation MemoryBudget::acquire(qint64 estimatedBytes) {
    const int units = unitsFor(estimatedBytes);

    semaphore_.acquire(units);

    const int now = used_.fetch_add(units, std::memory_order_relaxed) + units;
    int seen = peak_.load(std::memory_order_relaxed);
    while (seen < now && !peak_.compare_exchange_weak(seen, now, std::memory_order_relaxed)) {
        // compare_exchange_weak が seen を更新するため、条件の再評価だけでよい。
    }

    return {*this, units};
}

int MemoryBudget::totalUnits() const noexcept { return totalUnits_; }

int MemoryBudget::usedUnits() const noexcept { return used_.load(std::memory_order_relaxed); }

int MemoryBudget::peakUnits() const noexcept { return peak_.load(std::memory_order_relaxed); }

int MemoryBudget::unitsFor(qint64 bytes) const noexcept {
    // 総量以上は全量を確保して単独実行にする（ADR-0008）。
    // 先に判定するのは、この後の切り上げ計算が溢れないようにするため
    // （estimateJobBytes は「分からない」を qint64 の最大値で表す）。
    if (bytes >= totalBytes_) {
        return totalUnits_;
    }

    const qint64 units = (bytes + budgetUnitBytes - 1) / budgetUnitBytes;

    return static_cast<int>(std::clamp<qint64>(units, 1, totalUnits_));
}

void MemoryBudget::release(int units) noexcept {
    used_.fetch_sub(units, std::memory_order_relaxed);
    semaphore_.release(units);
}

} // namespace katachi::io
