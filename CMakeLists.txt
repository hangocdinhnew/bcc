cmake_minimum_required(VERSION 3.15)
option(CMAKE_EXPORT_COMPILE_COMMANDS "Exports compile_commands.json" ON)

project(bcc)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM CONFIG REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

find_program(RE2C_EXECUTABLE re2c REQUIRED)

set(PROJECT_ROOT "${CMAKE_SOURCE_DIR}")
set(LEXER_RE_FILE "${PROJECT_ROOT}/src/lexer.re.cpp")
set(LEXER_CPP_FILE "${CMAKE_BINARY_DIR}/lexer.cpp")

add_custom_command(
  OUTPUT ${LEXER_CPP_FILE}
  COMMAND ${RE2C_EXECUTABLE} -i -o ${LEXER_CPP_FILE} ${LEXER_RE_FILE}
  DEPENDS ${LEXER_RE_FILE}
  COMMENT "Generating lexer.cpp from lexer.re using re2c"
  VERBATIM
)

set(SOURCES
  "${PROJECT_ROOT}/src/main.cpp"
  "${PROJECT_ROOT}/src/codegen.cpp"
  "${PROJECT_ROOT}/src/lexer_utils.cpp"
  ${LEXER_CPP_FILE}
)

add_executable(bcc
  ${SOURCES}
)

target_include_directories(bcc PUBLIC "include/")
target_include_directories(bcc PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(bcc LLVM)
