
#include "ast/lexer.h"
#include "ast/nodes.h"
#include "ast/parser.h"
#include "bytecode.h"
#include "config.h"
#include "visitor.h"

#include <iostream>

int main(const int arg_count, const char ** args) {
    const auto user_args = parse_cmdline_args(arg_count, args);

    if (user_args->print_help) {
        std::cout << args[0] << '\n'
                  << "A compiler for the New-J language\n"
                     "Options: [-h|--help|-v|--version] <input filename>\n"
                     "\t-h or --help -> print this help message and exit\n"
                     "\t-v or --version -> print version number and exit\n"
                     "\tinput filename -> the input source code to compile"
                  << std::endl;
        return 0;
    } else if (user_args->print_version) {
        std::cout << args[0] << '\n' << "Version 0.1" << std::endl;
        return 0;
    }

    std::cout << "File to read: " << user_args->input_filename << std::endl;

    parser p{lexer{user_args->input_filename}};

    auto program = p.parse_program();
    if (program == nullptr) std::cout << "Failed\n";
    else {
        std::cout << "Success\n";

        if (user_args->print_syntax) {
            std::cout << "Syntax tree:\n";
            printing_visitor pv{};
            program->visit([&](auto & node) { pv.visit(node); });
            std::cout << "Visited " << pv.visited_count() << " nodes" << std::endl;
        }

        ir_gen_visitor ir_gen{};
        program->visit([&](auto & node) { ir_gen.visit(node); });
        if (user_args->print_ir) {
            std::cout << "IR Dump" << std::endl;
            ir_gen.dump();
        }

        auto bytecode = bytecode::program::from_ir(ir_gen.program());
        if (not bytecode.has_value()) {
            std::cerr << "Bytecode generation failed." << std::endl;
        } else {
            std::cout << "Bytecode generated" << std::endl;
            if (user_args->print_bytecode) { bytecode->print_human_readable(std::cout); }

            auto byte_code_dest = user_args->input_filename;
            // Delete everything after the dot
            byte_code_dest.erase(byte_code_dest.find_last_of('.'));
            byte_code_dest += ".bin";
            std::cout << "Bytecode output to " << byte_code_dest << std::endl;
            bytecode->print_file(byte_code_dest);
        }
    }
}
