// MainWindow のテスト（Phase 2 T9）。
//
// docs/spec-core.md §7 の UI 非機能要件のうち、機械で確かめられるものをここで固定する。
//   - 自動スクロールしない
//   - エラーはステータス行に出す。モーダルを開かない
//   - タブ順が明示されている
//
// テスト名は ASCII に限る（Phase 0 の知見）。
#include "app/MainWindow.hpp"
#include "core/CapabilityTable.hpp"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QDropEvent>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QImageWriter>
#include <QList>
#include <QMimeData>
#include <QPointF>
#include <QPushButton>
#include <QScrollBar>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <Qt>

#include <catch2/catch_test_macros.hpp>

namespace {

using katachi::app::MainWindow;
using katachi::core::CapabilityTable;

constexpr int waitLimitMs = 60000;

const CapabilityTable& qtTable() {
    static const CapabilityTable table = CapabilityTable::buildFromQt();
    return table;
}

QString writePng(const QDir& dir, const QString& name) {
    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(QColor(1, 2, 3));

    const QString path = dir.filePath(name);
    QImageWriter writer(path, "png");
    REQUIRE(writer.write(image));
    return path;
}

QString writeText(const QDir& dir, const QString& name) {
    const QString path = dir.filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write("not an image");
    file.close();
    return path;
}

QList<QUrl> urlsOf(const QStringList& paths) {
    QList<QUrl> urls;
    for (const QString& path : paths) {
        urls.append(QUrl::fromLocalFile(path));
    }
    return urls;
}

// フォーカス連鎖上の位置。setTabOrder の結果を見るために使う。
int focusOrderOf(QWidget* from, const QWidget* target, int limit = 200) {
    QWidget* current = from;
    for (int step = 0; step < limit; ++step) {
        current = current->nextInFocusChain();
        if (current == nullptr) {
            break;
        }
        if (current == target) {
            return step;
        }
    }
    return -1;
}

bool waitForIdle(const MainWindow& window) {
    QEventLoop loop;
    bool idle = false;

    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&window, &idle, &loop] {
        if (!window.isConverting()) {
            idle = true;
            loop.quit();
        }
    });
    poll.start(20);
    QTimer::singleShot(waitLimitMs, &loop, &QEventLoop::quit);
    loop.exec();

    return idle;
}

} // namespace

TEST_CASE("dropping files adds one job per file", "[app][window]") {
    QTemporaryDir input;
    REQUIRE(input.isValid());
    const QDir dir(input.path());
    const QStringList paths{writePng(dir, QStringLiteral("a.png")),
                            writePng(dir, QStringLiteral("b.png")),
                            writePng(dir, QStringLiteral("c.png"))};

    MainWindow window(qtTable());
    window.addSources(urlsOf(paths));

    REQUIRE(window.jobCount() == 3);
}

TEST_CASE("dropping a folder adds its images recursively", "[app][window]") {
    QTemporaryDir input;
    REQUIRE(input.isValid());
    const QDir dir(input.path());
    REQUIRE(dir.mkdir(QStringLiteral("sub")));

    writePng(dir, QStringLiteral("a.png"));
    writePng(QDir(dir.filePath(QStringLiteral("sub"))), QStringLiteral("b.png"));
    writeText(dir, QStringLiteral("notes.txt"));

    MainWindow window(qtTable());
    window.addSources({QUrl::fromLocalFile(input.path())});

    // 画像 2 枚だけ。画像でないファイルは入らない（判定は能力表の extensions）。
    REQUIRE(window.jobCount() == 2);
}

TEST_CASE("dropping the same file twice adds it once", "[app][window]") {
    QTemporaryDir input;
    REQUIRE(input.isValid());
    const QString path = writePng(QDir(input.path()), QStringLiteral("a.png"));

    MainWindow window(qtTable());
    window.addSources(urlsOf({path}));
    window.addSources(urlsOf({path}));

    REQUIRE(window.jobCount() == 1);
}

TEST_CASE("the window accepts drops", "[app][window]") {
    // **合成した QDropEvent を QApplication::sendEvent で送っても dropEvent には届かない。**
    // Qt のドロップイベントは QWidgetWindow がプラットフォームのイベントから作る経路でしか
    // 配送されず、送っても握り潰される（実測で確認した）。
    //
    // したがって自動テストで確かめられるのはここまで。
    //   - ウィンドウがドロップを受け付ける設定になっていること（下記）
    //   - 落とされたあとの経路（addSources）が正しいこと（他のテスト）
    //
    // **dropEvent が addSources を呼んでいるかは実機で確認する。** T9 の完了条件に含める。
    MainWindow window(qtTable());

    REQUIRE(window.acceptDrops());
}

TEST_CASE("the table does not auto scroll", "[app][window]") {
    // docs/spec-core.md §7: 自動スクロール禁止。ジョブ完了時にリストが勝手に動かない。
    MainWindow window(qtTable());
    auto* view = window.findChild<QTableView*>();
    REQUIRE(view != nullptr);

    REQUIRE_FALSE(view->hasAutoScroll());

    QTemporaryDir input;
    REQUIRE(input.isValid());
    const QDir dir(input.path());
    QStringList paths;
    for (int index = 0; index < 100; ++index) {
        paths.append(writePng(dir, QStringLiteral("input%1.png").arg(index)));
    }

    const int before = view->verticalScrollBar()->value();
    window.addSources(urlsOf(paths));

    REQUIRE(view->verticalScrollBar()->value() == before);
}

TEST_CASE("the tab order is set explicitly", "[app][window]") {
    // docs/spec-core.md §7: 全機能がキーボードのみで操作可能。タブ順を明示する。
    MainWindow window(qtTable());

    auto* view = window.findChild<QTableView*>();
    auto* start = window.findChild<QPushButton*>(QStringLiteral("startButton"));
    auto* cancel = window.findChild<QPushButton*>(QStringLiteral("cancelButton"));
    REQUIRE(view != nullptr);
    REQUIRE(start != nullptr);
    REQUIRE(cancel != nullptr);

    // どれもキーボードで到達できること。
    REQUIRE(view->focusPolicy() != Qt::NoFocus);
    REQUIRE(start->focusPolicy() != Qt::NoFocus);
    REQUIRE(cancel->focusPolicy() != Qt::NoFocus);

    // 明示した順に並んでいること（表 -> ... -> 開始 -> キャンセル）。
    const int toStart = focusOrderOf(view, start);
    const int toCancel = focusOrderOf(view, cancel);
    REQUIRE(toStart >= 0);
    REQUIRE(toCancel >= 0);
    REQUIRE(toStart < toCancel);
}

TEST_CASE("errors are shown in the status line not a dialog", "[app][window]") {
    // docs/spec-core.md §7: モーダルは破壊的操作の確認のみ。エラーはステータス行へ。
    QTemporaryDir input;
    REQUIRE(input.isValid());
    const QString path = writePng(QDir(input.path()), QStringLiteral("a.png"));

    MainWindow window(qtTable());
    window.addSources(urlsOf({path}));

    // 出力先を決めずに開始する。
    window.startConversion();

    REQUIRE(QApplication::activeModalWidget() == nullptr);
    REQUIRE_FALSE(window.statusText().isEmpty());
    REQUIRE_FALSE(window.isConverting());
}

TEST_CASE("a finished batch reports its counts in the status line", "[app][window]") {
    QTemporaryDir input;
    QTemporaryDir output;
    REQUIRE(input.isValid());
    REQUIRE(output.isValid());
    const QDir dir(input.path());
    writePng(dir, QStringLiteral("a.png"));
    writePng(dir, QStringLiteral("b.png"));
    const QString broken = writeText(dir, QStringLiteral("broken.png"));

    MainWindow window(qtTable());
    window.addSources({QUrl::fromLocalFile(input.path())});
    // notes ではなく broken.png なので、拡張子の判定では画像として拾われる。
    REQUIRE(window.jobCount() == 3);
    REQUIRE_FALSE(broken.isEmpty());

    window.setOutputDirectory(output.path());
    window.startConversion();
    REQUIRE(waitForIdle(window));

    // 失敗が混じってもモーダルは開かない。ステータス行に結果が出る。
    REQUIRE(QApplication::activeModalWidget() == nullptr);
    REQUIRE_FALSE(window.statusText().isEmpty());
    REQUIRE(window.succeededCount() == 2);
    REQUIRE(window.failedCount() == 1);
    // 失敗した行も残る。
    REQUIRE(window.jobCount() == 3);
}
