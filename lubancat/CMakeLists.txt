cmake_minimum_required(VERSION 3.10)
project(smart_agri VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------
# 源文件
# ------------------------------------------------------------------
set(C_SOURCES
    uart/uart.c
)

set(CXX_SOURCES
    main.cpp
    src/protocol/frame_codec.cpp
    src/control/expert_rule_controller.cpp
    src/control/ml_controller.cpp
    src/core/feature_engine.cpp
    src/datalog/csv_writer.cpp
    src/core/pipeline.cpp
    src/utils/logger.cpp
    src/utils/json_config.cpp
)

# ------------------------------------------------------------------
# 可执行文件
# ------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    ${C_SOURCES}
    ${CXX_SOURCES}
)

# ------------------------------------------------------------------
# 头文件搜索路径
# ------------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/uart
    ${CMAKE_SOURCE_DIR}/include
)

# ------------------------------------------------------------------
# 依赖库
# ------------------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# ------------------------------------------------------------------
# 编译选项
# ------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# ------------------------------------------------------------------
# 安装规则
# ------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/smart_agri)
