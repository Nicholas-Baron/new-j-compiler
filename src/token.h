//
// Created by nick on 4/6/20.
//

#ifndef TOKEN_H
#define TOKEN_H

#include <memory>
#include <variant>

enum struct TokenType {
    Assign,
    Colon,
    Const,
    EndOfFile,
    Float,
    Identifier,
    Int,
    Int32,
    Int64,
    Newline,
    Semi,
};

class Token final {
  public:
    using token_data = std::variant<std::monostate, bool, long, float, std::string, TokenType>;

    Token(const std::shared_ptr<std::string> & text, size_t position, size_t length, TokenType type)
        : src_text{text}, pos{position}, len{length}, token_type{type} {}

    [[nodiscard]] token_data get_data() const {
        switch (token_type) {
        case TokenType::Identifier:
            return src_text->substr(pos, len);
        case TokenType::Int:
            return std::stol(src_text->substr(pos, len));
        default:
            return token_type;
        }
    }

    [[nodiscard]] TokenType type() const noexcept { return token_type; }

  private:
    std::shared_ptr<const std::string> src_text;
    size_t pos, len;
    TokenType token_type;

    friend std::ostream & operator<<(std::ostream & lhs, const Token & rhs) {
        return lhs << rhs.src_text->substr(rhs.pos, rhs.len);
    }
};

#endif // NEW_J_COMPILER_TOKEN_H
