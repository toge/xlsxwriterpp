cmake_minimum_required(VERSION 3.25)

project(xlsxwriterpp_consumer LANGUAGES CXX)

# ──────────────────────────────────────────────────────────────
# インストール済みの xlsxwriterpp を find_package で利用する例。
#
# 事前に以下のコマンドで xlsxwriterpp をインストールしておくこと:
#
#   cmake -B build_install . \
#         -DCMAKE_INSTALL_PREFIX=<install_dir> \
#         -DXLSXWRITERPP_BUILD_EXAMPLES=OFF -DBUILD_TEST=OFF
#   cmake --build build_install
#   cmake --install build_install
#
# その後、このディレクトリで:
#
#   cmake -B build -DCMAKE_PREFIX_PATH=<install_dir>
#   cmake --build build
#   ./build/consumer_app
# ──────────────────────────────────────────────────────────────

find_package(xlsxwriterpp CONFIG REQUIRED)

add_executable(consumer_app consumer_main.cpp)

# xlsxwriterpp::xlsxwriterpp をリンクするだけで全依存が解決される
target_link_libraries(consumer_app PRIVATE xlsxwriterpp::xlsxwriterpp)
