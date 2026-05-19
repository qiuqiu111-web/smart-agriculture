# LubanCat (aarch64 / ARM64) 交叉编译工具链配置
# 用法:
#   cmake -B build \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
#       -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j4

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ---------- 工具链前缀（根据你的 SDK 路径调整） ----------
set(TOOLCHAIN_PREFIX aarch64-linux-gnu)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
