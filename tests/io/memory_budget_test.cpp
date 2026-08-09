// MemoryBudget のテスト（Phase 2 T5 / ADR-0008）。
//
// 予算の役目は「1 枚分の消費をスレッド数倍しないこと」。
// **予算を超える単独ジョブを諦めさせることではない**（ADR-0008）。
//
// 待ちを含むテストがあるため、閾値は CI のばらつきで落ちない程度に緩く取る。
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "io/MemoryBudget.hpp"

#include <QSize>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>

namespace {

using katachi::io::budgetUnitBytes;
using katachi::io::estimateJobBytes;
using katachi::io::MemoryBudget;

constexpr qint64 fourUnits = 4 * budgetUnitBytes;
constexpr qint64 twoUnits = 2 * budgetUnitBytes;

// 待ち合わせの上限。落ちるときは「固まった」と判断できる長さにする。
constexpr auto waitLimit = std::chrono::seconds(5);
// 「まだ通っていない」ことを確かめるための短い猶予。
constexpr auto blockedProbe = std::chrono::milliseconds(50);

} // namespace

TEST_CASE("the budget admits jobs up to its total", "[io][budget]") {
    MemoryBudget budget(fourUnits);
    REQUIRE(budget.totalUnits() == 4);

    const auto first = budget.acquire(twoUnits);
    const auto second = budget.acquire(twoUnits);

    REQUIRE(budget.usedUnits() == 4);
    REQUIRE(budget.peakUnits() == 4);
}

TEST_CASE("the budget blocks the third job until one returns", "[io][budget]") {
    MemoryBudget budget(fourUnits);

    auto first = budget.acquire(twoUnits);
    const auto second = budget.acquire(twoUnits);
    REQUIRE(budget.usedUnits() == 4);

    std::atomic<bool> entered{false};
    std::atomic<bool> acquired{false};

    std::thread third([&budget, &entered, &acquired] {
        entered.store(true);
        const auto reservation = budget.acquire(twoUnits);
        acquired.store(true);
    });

    // スレッドが acquire に入るまで待つ。入る前に判定すると空振りする。
    while (!entered.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(blockedProbe);

    // 予算が空いていないので、まだ通っていないこと。
    REQUIRE_FALSE(acquired.load());

    // 1 件返すと通る。
    {
        const auto returning = std::move(first);
    }

    const auto deadline = std::chrono::steady_clock::now() + waitLimit;
    while (!acquired.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    third.join();

    REQUIRE(acquired.load());
}

TEST_CASE("an oversized job runs exclusively", "[io][budget]") {
    // 予算より大きい見積りは全量を確保する。デッドロックしない（ADR-0008）。
    MemoryBudget budget(fourUnits);

    {
        const auto huge = budget.acquire(1000 * budgetUnitBytes);
        REQUIRE(budget.usedUnits() == budget.totalUnits());
    }

    REQUIRE(budget.usedUnits() == 0);
}

TEST_CASE("the guard releases on every path", "[io][budget]") {
    MemoryBudget budget(fourUnits);

    {
        const auto reservation = budget.acquire(twoUnits);
        REQUIRE(budget.usedUnits() == 2);
    }
    REQUIRE(budget.usedUnits() == 0);

    // move したあと、元のガードは二重に返さない。
    {
        auto reservation = budget.acquire(twoUnits);
        {
            const auto moved = std::move(reservation);
            REQUIRE(budget.usedUnits() == 2);
        }
        REQUIRE(budget.usedUnits() == 0);
    }
    REQUIRE(budget.usedUnits() == 0);

    REQUIRE(budget.peakUnits() == 2);
}

TEST_CASE("the estimate follows the ADR-0008 formula", "[io][budget]") {
    // estimate = 2 * fileSize + 16 * max(sourcePixels, resizeBoundPixels)
    //
    // 係数 16 は src/core/Converter.cpp のアルファ合成で画像が 4 枚同時に
    // 生存することから導いた（4 枚 x 4 バイト/画素）。
    REQUIRE(estimateJobBytes(1000, QSize(64, 64), std::nullopt) == (2 * 1000) + (16 * 64 * 64));

    // リサイズで拡大する場合は出力側の寸法が支配的になる。
    REQUIRE(estimateJobBytes(1000, QSize(64, 64), QSize(128, 128)) ==
            (2 * 1000) + (16 * 128 * 128));

    // 縮小する場合は入力側のままで見積もる。
    REQUIRE(estimateJobBytes(1000, QSize(64, 64), QSize(16, 16)) == (2 * 1000) + (16 * 64 * 64));
}

TEST_CASE("an unknown size is treated as the whole budget", "[io][budget]") {
    // ADR-0008: 寸法が分からない入力を小さいと仮定しない。安全側に倒す。
    const qint64 estimate = estimateJobBytes(1000, QSize(), std::nullopt);
    REQUIRE(estimate > 1000 * budgetUnitBytes);

    MemoryBudget budget(fourUnits);
    {
        const auto reservation = budget.acquire(estimate);
        REQUIRE(budget.usedUnits() == budget.totalUnits());
    }
    REQUIRE(budget.usedUnits() == 0);
}
