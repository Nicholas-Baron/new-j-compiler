
#include "config.h"
#include "lexer.h"
#include "nodes.h"
#include "parser.h"
#include "visitor.h"

#include <iostream>

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

    parser p{lexer{user_args->input_filename}};

    auto program = p.parse_program();
    if (program == nullptr) std::cout << "Failed\n";
    else {
        std::cout << "Success\n";

        if (user_args->print_syntax) {
            printing_visitor pv{};
            program->visit([&](auto & node) { pv.visit(node); });
            std::cout << "Visited " << pv.visited_count() << " nodes" << std::endl;
        }

        ir_gen_visitor ir_gen{};
        program->visit([&](auto & node) { ir_gen.visit(node); });
        ir_gen.dump();
    }
}
