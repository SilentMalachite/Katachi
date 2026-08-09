#pragma once

// io 層のテストダブル（Phase 2 T1 で作り、T4 の JobRunner テストと共有する）。
//
// **Qt のイベントループにもファイルシステムにも触れない。**
// これが docs/cpp-conventions.md §2.3 が JobRunner をテンプレートにした理由そのもの。

#include "core/Result.hpp"
#include "io/IoError.hpp"

#include <QByteArray>

#include <utility>
#include <variant>
#include <vector>

namespace katachi::test {

// 与えたバイト列をそのまま返す入力。
class MemorySource {
public:
    explicit MemorySource(QByteArray bytes) : bytes_(std::move(bytes)) {}

    [[nodiscard]] core::Result<QByteArray, io::IoError> read() {
        return core::Result<QByteArray, io::IoError>::ok(bytes_);
    }

private:
    QByteArray bytes_;
};

// 書かれたバイト列を覚えておく出力。
class MemorySink {
public:
    [[nodiscard]] core::Result<std::monostate, io::IoError> write(const QByteArray& bytes) {
        written_ = bytes;
        return core::Result<std::monostate, io::IoError>::ok(std::monostate{});
    }

    [[nodiscard]] const QByteArray& written() const noexcept { return written_; }

private:
    QByteArray written_;
};

// 常に失敗する出力。書き出し失敗の経路を通すために使う。
class FailingSink {
public:
    [[nodiscard]] core::Result<std::monostate, io::IoError> write(const QByteArray& /*bytes*/) {
        return core::Result<std::monostate, io::IoError>::err(io::IoError::WriteFailed);
    }
};

// 進捗の記録とキャンセル。Qt のシグナルを使わない。
class FakeProgress {
public:
    void onProgress(int done, int total) { calls_.emplace_back(done, total); }

    [[nodiscard]] bool isCancelled() const noexcept { return cancelled_; }

    void cancel() noexcept { cancelled_ = true; }

    [[nodiscard]] const std::vector<std::pair<int, int>>& calls() const noexcept { return calls_; }

private:
    std::vector<std::pair<int, int>> calls_;
    bool cancelled_ = false;
};

// isCancelled() が noexcept でない型。concept の noexcept 要求が実際に効いていることを
// 否定側で確かめるために置く（docs/cpp-conventions.md §2.4）。
class NoexceptlessProgress {
public:
    void onProgress(int, int) {}

    [[nodiscard]] bool isCancelled() const { return false; }
};

} // namespace katachi::test
