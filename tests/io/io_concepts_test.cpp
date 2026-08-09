// io 層の concept とエラー列挙の契約テスト（docs/phases.md §2.2）。
//
// 本番型（FileSource / FileSink / JobRunnerBridge）はまだ存在しないため、
// ここで concept を満たすのはテストダブルのみ。
// **本番型の static_assert は T2（FileSource / FileSink）と T6（JobRunnerBridge）で足す。**
//
// テスト名は ASCII に限る。catch_discover_tests はテスト名をそのまま
// フィルタ引数として実行ファイルへ渡すため（Phase 0 の知見）。
#include "core/Result.hpp"
#include "io/IoConcepts.hpp"
#include "io/IoError.hpp"
#include "test_doubles.hpp"

#include <QByteArray>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>

namespace {

using katachi::core::Result;
using katachi::io::ByteSink;
using katachi::io::ByteSource;
using katachi::io::IoError;
using katachi::io::ProgressSink;
using katachi::test::FakeProgress;
using katachi::test::MemorySink;
using katachi::test::MemorySource;
using katachi::test::NoexceptlessProgress;

// 肯定側。
static_assert(ByteSource<MemorySource>);
static_assert(ByteSink<MemorySink>);
static_assert(ProgressSink<FakeProgress>);

// 否定側。制約が緩すぎて何でも通る事故を検出する（docs/phases.md §2.2）。
static_assert(!ByteSource<int>);
static_assert(!ByteSink<MemorySource>);
static_assert(!ProgressSink<MemorySink>);
static_assert(!ProgressSink<NoexceptlessProgress>);

} // namespace

TEST_CASE("IoError values are distinct and comparable", "[io]") {
    const std::array<IoError, 5> values{IoError::NotFound, IoError::OpenFailed,
                                        IoError::WriteFailed, IoError::TooLarge,
                                        IoError::DestinationExists};

    for (std::size_t left = 0; left < values.size(); ++left) {
        REQUIRE(values.at(left) == values.at(left));

        for (std::size_t right = left + 1; right < values.size(); ++right) {
            REQUIRE(values.at(left) != values.at(right));
        }
    }
}

TEST_CASE("the test doubles carry bytes through the io concepts", "[io]") {
    MemorySource source(QByteArray("katachi"));
    MemorySink sink;
    FakeProgress progress;

    const Result<QByteArray, IoError> read = source.read();
    REQUIRE(read.isOk());
    REQUIRE(sink.write(read.value()).isOk());
    REQUIRE(sink.written() == QByteArray("katachi"));

    REQUIRE_FALSE(progress.isCancelled());
    progress.cancel();
    REQUIRE(progress.isCancelled());

    progress.onProgress(3, 10);
    REQUIRE(progress.calls().size() == 1);
    REQUIRE(progress.calls().front().first == 3);
    REQUIRE(progress.calls().front().second == 10);
}
