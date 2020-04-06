
#ifndef CONFIG_H
#define CONFIG_H

#include <memory>
#include <string>

struct UserSettings {
    std::string input_filename{};
    bool print_help{false};
};

[[nodiscard]] std::shared_ptr<const UserSettings> parse_cmdline_args(int arg_count,
                                                                     const char ** args);

#endif
