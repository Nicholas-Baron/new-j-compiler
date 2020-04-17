//
// Created by nick on 4/6/20.
//

#ifndef NEW_J_COMPILER_LEXER_H
#define NEW_J_COMPILER_LEXER_H

#include "token.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>

class lexer final {
    using src_impl = std::variant<std::string, std::shared_ptr<std::string>>;

  public:
    explicit lexer(const std::string & filename) : src{filename} {}

    // Consumes the next token, removing it from the input stream
    [[nodiscard]] token next();

    // Generates the next token, but does not remove it from the input stream
    [[nodiscard]] token peek();

  private:
    // Stores either the name of a file or the stream of that file
    src_impl src;

    size_t current_pos = 0;

    std::optional<token> peeked{};
};

#endif // NEW_J_COMPILER_LEXER_H
