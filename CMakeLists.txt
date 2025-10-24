cmake_minimum_required(VERSION 3.28)

project(WebServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -pedantic -Werror -DLOCAL)

include_directories(src/server src/logger src/database)

file(GLOB LIB_TARGETS "src/**/*.cpp")

add_executable(${PROJECT_NAME} src/main.cpp ${LIB_TARGETS})
target_link_libraries(${PROJECT_NAME} PRIVATE sqlite3)

# Tests
enable_testing()
find_package(Threads REQUIRED)
add_executable(test_logger test/test_logger.cpp src/logger/logger.cpp)
target_link_libraries(test_logger PRIVATE Threads::Threads)
add_test(NAME logger_basic COMMAND test_logger)
