cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(hlcup2)

add_custom_command(
    COMMAND re2c -cd "${CMAKE_CURRENT_SOURCE_DIR}/AccountParser.cpp.re" -o "${CMAKE_BINARY_DIR}/AccountParser.cpp"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/AccountParser.cpp.re"
    OUTPUT "${CMAKE_BINARY_DIR}/AccountParser.cpp"
)

add_executable(hlcup2
    main.cpp
    "${CMAKE_BINARY_DIR}/AccountParser.cpp"
    )
#add_dependencies(hlcup2 re2c)
target_include_directories(hlcup2 PUBLIC
    platform/linux
    platform/x86_64
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}"
)


