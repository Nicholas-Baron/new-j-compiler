//
// Created by nick on 4/6/20.
//

#ifndef TOKEN_H
#define TOKEN_H

#include <memory>
#include <variant>

enum struct token_type {
    Assign,
    Colon,
    Const,
    EndOfFile,
    Float,
    Identifier,
    Int,
    Int32,
    Int64,
    LParen,
    Newline,
    RParen,
    Semi,
    Func,
    Struct,
    Comma,
};

class token final {
  public:
    using token_data = std::variant<std::monostate, bool, long, float, std::string, token_type>;

    token(const std::shared_ptr<std::string> & text, size_t position, size_t length,
          token_type type)
        : src_text{text}, pos{position}, len{length}, tok_type{type} {}

    [[nodiscard]] token_data get_data() const {
        switch (const auto & text = src_text->substr(pos, len); tok_type) {
        case token_type::Identifier:
            return text;
        case token_type::Int:
            if (len < 3)
                return std::stol(text);
            else if (tolower(text[1]) == 'x')
                return std::stol(text, nullptr, 16);
            else if (tolower(text[1]) == 'b')
                return std::stol(text, nullptr, 2);
            else
                return std::stol(text, nullptr, 10);
        default:
            return tok_type;
        }
    }

    [[nodiscard]] token_type type() const noexcept { return tok_type; }
    [[nodiscard]] auto start() const noexcept { return pos; }
    [[nodiscard]] auto end() const noexcept { return pos + len; }
    [[nodiscard]] auto src() const noexcept { return src_text; }

  private:
    std::shared_ptr<const std::string> src_text;
    size_t pos, len;
    token_type tok_type;

    friend std::ostream & operator<<(std::ostream & lhs, const token & rhs) {
        return lhs << rhs.src_text->substr(rhs.pos, rhs.len);
    }
};

#endif // NEW_J_COMPILER_TOKEN_H
