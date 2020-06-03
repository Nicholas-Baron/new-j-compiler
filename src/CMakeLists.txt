
add_executable(new_jc
        main.cpp
        config.cpp
        lexer.cpp
        parser.cpp
        program.cpp
        visitor.cpp
        ir.cpp
        bytecode.cpp
        )

set_property(TARGET new_jc PROPERTY CXX_STANDARD 17)
