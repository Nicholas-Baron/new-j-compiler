
#include "config.h"

#include <iostream>

[[nodiscard]] std::shared_ptr<const user_settings> parse_cmdline_args(int arg_count,
                                                                      const char ** args) {

    user_settings settings;

    for (auto i = 1; i < arg_count; i++) {
        if (std::string arg{args[i]}; arg == "--help" or arg == "-h") {
            settings.print_help = true;
        } else if (arg == "-fsyntax-tree") {
            settings.print_syntax = true;
        } else if (arg == "-fir-dump") {
            settings.print_ir = true;
        } else if (arg == "-fbytecode") {
            settings.print_bytecode = true;
        } else if (arg.front() != '-') {
            settings.input_filename = arg;
        } else {
            std::cout << "Unrecognized option: " << arg << std::endl;
        }
    }

    return std::make_shared<const user_settings>(std::move(settings));
}
