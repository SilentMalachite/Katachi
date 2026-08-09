// コンパイルが**失敗すること**を確認するためのファイル。通常ビルドの対象外。
//
// Result<T,E> は requires(!std::same_as<T,E>) で T == E を弾く（docs/spec-core.md §2）。
// この排除は型システム上の制約であり、実行時テストでは観測できない。
// そのため「このファイルのビルドが失敗すること」を ctest で確認する
// （不変条件スキャナのネガティブテストと同じ考え方）。
//
// QString は ResultValue も ResultError も満たすため、
// ここでの失敗理由は concept 不適合ではなく requires(!same_as<T,E>) である。
#include "core/Result.hpp"

#include <QString>

using Forbidden = katachi::core::Result<QString, QString>;

int main() {
    return 0;
}
