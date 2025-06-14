cmake_minimum_required(VERSION 3.15)
project(bcc)

set(CMAKE_CXX_STANDARD 17)

add_executable(bcc
    src/main.cpp
    src/codegen.cpp
    src/lexer.cpp
    src/parser.cpp
)

target_include_directories(bcc PRIVATE include)
