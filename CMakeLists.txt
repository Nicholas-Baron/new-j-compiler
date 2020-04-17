cmake_minimum_required(VERSION 3.10)

message("Compiler is ${CMAKE_CXX_COMPILER}")
# Check whether we are compiling on Windows
# Any suggestions welcome on how to improve this line
if (NOT CMAKE_CXX_COMPILER MATCHES MSVC|cl[^a])
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -Og -g -fverbose-asm")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O")
    message("Setting options to Unix style")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W2 /Za")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /O2")
    message("Setting options to Windows style")
endif ()

project(new_j_compiler CXX)

add_executable(new_jc
        src/main.cpp
        src/config.cpp
        src/lexer.cpp
        src/parser.cpp
        src/program.cpp
        src/visitor.cpp
        )

set_property(TARGET new_jc PROPERTY CXX_STANDARD 17)
