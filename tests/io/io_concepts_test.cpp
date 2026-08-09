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

#include <QByteArray>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <utility>
#include <variant>

namespace {

using katachi::core::Result;
using katachi::io::ByteSink;
using katachi::io::ByteSource;
using katachi::io::IoError;
using katachi::io::ProgressSink;

// ファイルシステムに触れない入力。JobRunner を T4 でテストするときにも使う。
class MemorySource {
public:
    explicit MemorySource(QByteArray bytes) : bytes_(std::move(bytes)) {}

    [[nodiscard]] Result<QByteArray, IoError> read() {
        return Result<QByteArray, IoError>::ok(bytes_);
    }

private:
    QByteArray bytes_;
};

// ファイルシステムに触れない出力。
class MemorySink {
public:
    [[nodiscard]] Result<std::monostate, IoError> write(const QByteArray& bytes) {
        written_ = bytes;
        return Result<std::monostate, IoError>::ok(std::monostate{});
    }

    [[nodiscard]] const QByteArray& written() const noexcept { return written_; }

private:
    QByteArray written_;
};

// Qt のイベントループに触れない進捗とキャンセル。
class FakeProgress {
public:
    void onProgress(int done, int total) {
        done_ = done;
        total_ = total;
    }

    [[nodiscard]] bool isCancelled() const noexcept { return cancelled_; }

    void cancel() noexcept { cancelled_ = true; }

    [[nodiscard]] int done() const noexcept { return done_; }

    [[nodiscard]] int total() const noexcept { return total_; }

private:
    int done_ = 0;
    int total_ = 0;
    bool cancelled_ = false;
};

// isCancelled() が noexcept でない型。concept の noexcept 要求が実際に効いていることを
// 否定側で確かめるために置く（docs/cpp-conventions.md §2.4）。
class NoexceptlessProgress {
public:
    void onProgress(int, int) {}

    [[nodiscard]] bool isCancelled() const { return false; }
};

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
    REQUIRE(progress.done() == 3);
    REQUIRE(progress.total() == 10);
}
