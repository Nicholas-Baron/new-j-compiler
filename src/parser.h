//
// Created by nick on 4/6/20.
//

#ifndef NEW_J_COMPILER_PARSER_H
#define NEW_J_COMPILER_PARSER_H

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "token.h"

class Parser final {
    using src_impl = std::variant<std::string, std::shared_ptr<std::string>>;

  public:
    explicit Parser(const std::string & filename) : src{filename} {}

    [[nodiscard]] Token next();

    [[nodiscard]] Token peek();

  private:
    // Stores either the name of a file or the stream of that file
    src_impl src;

    size_t current_pos = 0;

    std::optional<Token> peeked{};
};

#endif // NEW_J_COMPILER_PARSER_H
