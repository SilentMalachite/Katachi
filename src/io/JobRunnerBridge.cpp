#include "io/JobRunnerBridge.hpp"

#include "core/CapabilityTable.hpp"
#include "core/NamingRule.hpp"
#include "core/Result.hpp"
#include "io/CollisionPolicy.hpp"
#include "io/FileSink.hpp"
#include "io/FileSource.hpp"
#include "io/IoConcepts.hpp"
#include "io/IoError.hpp"
#include "io/JobRunner.hpp"
#include "io/MemoryBudget.hpp"

#include <QFileInfo>
#include <QFuture>
#include <QImageReader>
#include <QList>
#include <QMutexLocker>
#include <QObject>
#include <QString>
#include <QThread>
#include <Qt>
#include <QtConcurrentMap>
// emit マクロの提供元。Qt 6 では qtmetamacros.h にある（qobjectdefs.h ではない）。
// フラットヘッダ <QObject> 経由では clang-tidy の misc-include-cleaner が
// 「提供ヘッダが未 include」と判定するため直接 include する
// （Qt 6 の QStringList を QList<QString> と綴ったのと同じ事情。T1.5 参照）。
#include <QtCore/qtmetamacros.h>

#include <algorithm>
#include <atomic>
#include <utility>

namespace katachi::io {
namespace {

// docs/spec-core.md §7: 進捗表示は 200ms 以下の間隔で更新しない。
constexpr int progressInterval = 200;

using PathResult = core::Result<QString, IoError>;

} // namespace

// 契約の明文化（docs/phases.md §2.2 が求める本番型の static_assert）。
// これで ByteSource / ByteSink / ProgressSink の 3 つとも本番型が揃った。
static_assert(ProgressSink<JobRunnerBridge>);

JobRunnerBridge::JobRunnerBridge(const core::CapabilityTable& caps, QObject* parent)
    : QObject(parent), caps_(&caps) {
    // docs/spec-core.md §6: 既定並列度は idealThreadCount() - 1（最低 1）。
    // グローバルのプールは書き換えない。専用のプールを持つ。
    pool_.setMaxThreadCount(std::max(1, QThread::idealThreadCount() - 1));

    timer_.setInterval(progressInterval);
    // 進捗表示は正確さより間隔の粗さが大事。CoarseTimer で十分。
    timer_.setTimerType(Qt::CoarseTimer);

    connect(&timer_, &QTimer::timeout, this, &JobRunnerBridge::flush);
    connect(&watcher_, &QFutureWatcher<JobOutcome>::finished, this,
            &JobRunnerBridge::handleFinished);
}

JobRunnerBridge::~JobRunnerBridge() {
    // ワーカーは this を触る。**必ず終わってから壊す。**
    cancelled_.store(true, std::memory_order_relaxed);
    watcher_.future().cancel();
    watcher_.waitForFinished();
}

void JobRunnerBridge::onProgress(int done, int total) {
    lastDone_.store(done, std::memory_order_relaxed);
    lastTotal_.store(total, std::memory_order_relaxed);
    lastWorkerThread_.store(QThread::currentThread(), std::memory_order_relaxed);
}

bool JobRunnerBridge::isCancelled() const noexcept {
    return cancelled_.load(std::memory_order_relaxed);
}

bool JobRunnerBridge::isRunning() const { return watcher_.isRunning(); }

int JobRunnerBridge::progressIntervalMs() const { return timer_.interval(); }

QThread* JobRunnerBridge::lastWorkerThread() const noexcept {
    return lastWorkerThread_.load(std::memory_order_relaxed);
}

void JobRunnerBridge::start(const BatchRequest& request) {
    if (isRunning()) {
        // 二重起動しない。走っている間の再投入は無視する。
        return;
    }

    const int total = static_cast<int>(request.sourcePaths.size());

    cancelled_.store(false, std::memory_order_relaxed);
    counter_.completed.store(0, std::memory_order_relaxed);
    counter_.total = total;
    lastDone_.store(0, std::memory_order_relaxed);
    lastTotal_.store(total, std::memory_order_relaxed);
    emittedDone_ = -1;
    emittedResults_ = 0;
    {
        const QMutexLocker locker(&mutex_);
        reserved_.clear();
    }

    // JobItem はバイト列を持たないため、全件を先に作っても軽い（ADR-0008）。
    QList<JobItem> items;
    items.reserve(total);
    for (int index = 0; index < total; ++index) {
        const QString& path = request.sourcePaths.at(index);

        JobItem item;
        item.sourcePath = path;
        item.sourceBaseName = QFileInfo(path).completeBaseName();
        item.index = index;
        item.pattern = request.pattern;
        item.extension = request.extension;
        item.spec = request.spec;
        items.append(std::move(item));
    }

    // request をコピーして持たせる。ワーカーは呼び出し側の寿命に依存しない。
    auto future =
        QtConcurrent::mapped(&pool_, std::move(items), [this, request](const JobItem& item) {
            return runJob(item, request);
        });

    // 公式ドキュメントの指示どおり、接続を済ませてから setFuture する
    // （接続はコンストラクタで済ませてある）。
    watcher_.setFuture(future);
    timer_.start();
}

void JobRunnerBridge::cancel() {
    // 二段構え（ADR-0010）。
    //   1. QFuture::cancel() が待機中の項目を止める
    //   2. isCancelled() を見た JobRunner が、着手直前の項目を早期終了する
    cancelled_.store(true, std::memory_order_relaxed);
    watcher_.future().cancel();
}

core::Result<QString, IoError> JobRunnerBridge::reservePath(const OutputFileName& fileName,
                                                            const BatchRequest& request) {
    // **ロックの中は実在確認と予約だけ**（ADR-0009 の追補）。
    // 書き出しはロックの外で並列に走る。
    const QMutexLocker locker(&mutex_);

    const core::Result<QString, IoError> resolved =
        resolveCollision(request.outputDirectory, fileName, request.collision, reserved_);
    if (!resolved.isOk()) {
        return resolved;
    }

    reserved_.insert(resolved.value());

    return resolved;
}

void JobRunnerBridge::countOne() {
    const int done = counter_.completed.fetch_add(1, std::memory_order_relaxed) + 1;
    onProgress(done, counter_.total);
}

JobOutcome JobRunnerBridge::runJob(const JobItem& item, const BatchRequest& request) {
    JobOutcome outcome;
    outcome.sourcePath = item.sourcePath;

    if (isCancelled()) {
        outcome.status = JobStatus::Cancelled;
        return outcome;
    }

    // 1. 出力名。純粋（ADR-0005）。
    const core::Result<OutputFileName, core::NamingError> fileName = outputFileNameFor(item);
    if (!fileName.isOk()) {
        outcome.status = JobStatus::Failed;
        outcome.failure = JobFailure{fileName.error()};
        countOne();
        return outcome;
    }

    // 2. 衝突の解決と予約。
    const PathResult reserved = reservePath(fileName.value(), request);
    if (!reserved.isOk()) {
        outcome.status =
            reserved.error() == IoError::DestinationExists ? JobStatus::Skipped : JobStatus::Failed;
        outcome.failure = JobFailure{reserved.error()};
        countOne();
        return outcome;
    }

    // 3. メモリ予算（ADR-0008）。寸法はヘッダだけ読む。復号はしない。
    const QFileInfo info(item.sourcePath);
    const QImageReader reader(item.sourcePath);
    const auto reservation =
        budget_.acquire(estimateJobBytes(info.size(), reader.size(), item.spec.resize));

    // 4. 実行。**ここはロックの外。**
    //    予約済みの名前をそのまま使うため、Sink のポリシーは Overwrite でよい。
    FileSource source(item.sourcePath);
    FileSink sink(request.outputDirectory, OutputFileName{QFileInfo(reserved.value()).fileName()},
                  CollisionPolicy::Overwrite);

    JobRunner<FileSink, JobRunnerBridge> runner(*caps_, *this, counter_);
    JobOutcome result = runner.runOne(source, sink, item);
    result.outputPath = reserved.value();

    return result;
}

void JobRunnerBridge::flush() {
    const int done = lastDone_.load(std::memory_order_relaxed);
    if (done != emittedDone_) {
        emittedDone_ = done;
        emit progressChanged(done, lastTotal_.load(std::memory_order_relaxed));
    }

    // 結果は QFuture から取り出す。ワーカーが別の入れ物へ積むより
    // 経路が 1 本で済み、ロックも増えない。
    const QFuture<JobOutcome> future = watcher_.future();
    const int available = future.resultCount();
    if (available > emittedResults_) {
        QList<JobOutcome> batch;
        batch.reserve(available - emittedResults_);
        for (int index = emittedResults_; index < available; ++index) {
            batch.append(future.resultAt(index));
        }
        emittedResults_ = available;

        emit resultsReady(batch);
    }
}

void JobRunnerBridge::handleFinished() {
    timer_.stop();
    // 取りこぼしを出し切ってから終わりを伝える。
    flush();

    emit finished();
}

} // namespace katachi::io
