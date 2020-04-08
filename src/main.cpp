
#include <iostream>

#include "config.h"
#include "lexer.h"

int main(const int arg_count, const char ** args) {
    const auto user_args = parse_cmdline_args(arg_count, args);

    if (user_args->print_help) {
        std::cout << args[0] << '\n'
                  << "A compiler for the New-J language\n"
                     "Options: [-h|--help] <input filename>\n"
                     "\t-h or --help -> print this help message\n"
                     "\tinput filename -> the input source code to compile"
                  << std::endl;
        return 0;
    }
    std::cout << "File to read: " << user_args->input_filename << std::endl;

    Lexer p{user_args->input_filename};

    while (p.peek().type() != TokenType::EndOfFile) {
        std::cout << ' ' << p.next() << std::endl;
    }
}
