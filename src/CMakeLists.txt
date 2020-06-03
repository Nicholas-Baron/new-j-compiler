
add_executable(new_jc
        main.cpp
        config.cpp
        ast/lexer.cpp
        ast/parser.cpp
        ast/program.cpp
        visitor.cpp
        ir.cpp
        bytecode.cpp
        )

target_include_directories(new_jc PRIVATE ast)

set_property(TARGET new_jc PROPERTY CXX_STANDARD 17)
