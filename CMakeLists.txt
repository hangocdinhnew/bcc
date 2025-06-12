cmake_minimum_required(VERSION 3.15)
option(CMAKE_EXPORT_COMPILE_COMMANDS "Exports compile_commands.json" ON)

project(bcc)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM CONFIG REQUIRED)
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${BISON_INCLUDE_DIRS})
include_directories(${FLEX_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

BISON_TARGET(Parser
  ${CMAKE_SOURCE_DIR}/src/parser.y
  ${CMAKE_BINARY_DIR}/parser.cpp
  DEFINES_FILE ${CMAKE_BINARY_DIR}/parser.tab.h
)

FLEX_TARGET(Lexer
  ${CMAKE_SOURCE_DIR}/src/lexer.l
  ${CMAKE_BINARY_DIR}/lexer.cpp
)

set(SOURCES
  "./src/main.cpp"
  "./src/codegen.cpp"
  ${BISON_Parser_OUTPUTS}
  ${FLEX_Lexer_OUTPUTS}
)

add_executable(bcc
  ${SOURCES}
)

target_include_directories(bcc PUBLIC "include/")
target_include_directories(bcc PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(bcc LLVM)
