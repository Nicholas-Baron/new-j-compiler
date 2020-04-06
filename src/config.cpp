
#include "config.hpp"
#include <iostream>

[[nodiscard]] std::shared_ptr<const UserSettings> parse_cmdline_args(int arg_count,
                                                                     const char ** args) {

    UserSettings settings;

    for (auto i = 1; i < arg_count; i++) {
        if (std::string arg{args[i]}; arg == "--help" or arg == "-h") {
            settings.print_help = true;
        } else if (arg.front() != '-') {
            settings.input_filename = arg;
        } else {
            std::cout << "Unrecognized option: " << arg << std::endl;
        }
    }

    return std::make_shared<const UserSettings>(std::move(settings));
}
