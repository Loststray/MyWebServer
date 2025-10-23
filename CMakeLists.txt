cmake_minimum_required(VERSION 3.28)

project(WebServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -pedantic -Werror -DLOCAL)

include_directories(src/server src/logger)

file(GLOB LIB_TARGETS "src/**/*.cpp")

add_executable(${PROJECT_NAME} src/main.cpp ${LIB_TARGETS})
target_link_libraries(${PROJECT_NAME} PRIVATE sqlite3)
