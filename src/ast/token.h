//
// Created by nick on 4/6/20.
//

#ifndef TOKEN_H
#define TOKEN_H

#include <memory>
#include <ostream>
#include <string>
#include <variant>

enum struct token_type {
    Assign,
    Bit_Or,
    Boolean_And,
    Boolean_Or,
    Colon,
    Comma,
    Const,
    Else,
    EndOfFile,
    Eq,
    Float,
    Func,
    Ge,
    Gt,
    Identifier,
    If,
    Int,
    Int32,
    Int64,
    LBrace,
    LParen,
    Le,
    Let,
    Lt,
    Minus,
    Minus_Assign,
    Mult,
    Mult_Assign,
    Newline,
    Plus,
    Plus_Assign,
    RBrace,
    RParen,
    Return,
    Semi,
    Shl,
    Shr,
    StringLiteral,
    Struct,
    While,
};

class token final {
  public:
    using token_data = std::variant<std::monostate, bool, long, double, std::string, token_type>;
    using source_t = std::shared_ptr<const std::string>;

    token(const std::shared_ptr<std::string> & text, size_t position, size_t length,
          token_type type)
        : src_text{text}, pos{position}, len{length}, tok_type{type} {}

    [[nodiscard]] token_data get_data() const {
        switch (const auto & text = src_text->substr(pos, len); tok_type) {
        case token_type::Identifier:
            return text;
        case token_type::Int:
            if (len < 3) return std::stol(text);
            else if (tolower(text[1]) == 'x')
                return std::stol(text, nullptr, 16);
            else if (tolower(text[1]) == 'b')
                return std::stol(text, nullptr, 2);
            else
                return std::stol(text, nullptr, 10);
        case token_type ::StringLiteral:
            return text;
        default:
            return tok_type;
        }
    }

    [[nodiscard]] token_type type() const noexcept { return tok_type; }
    [[nodiscard]] auto start() const noexcept { return pos; }
    [[nodiscard]] auto end() const noexcept { return pos + len; }
    [[nodiscard]] source_t src() const noexcept { return src_text; }

    [[nodiscard]] bool op_assign() const noexcept {
        switch (tok_type) {
        case token_type::Mult_Assign:
        case token_type::Plus_Assign:
        case token_type::Minus_Assign:
        case token_type::Assign:
            return true;
        default:
            return false;
        }
    }

  private:
    std::shared_ptr<const std::string> src_text;
    size_t pos, len;
    token_type tok_type;

    friend std::ostream & operator<<(std::ostream & lhs, const token & rhs) {

        size_t line_start = 1;
        size_t col_start = 0;

        size_t index = 0;
        while (index < rhs.pos) {
            if (rhs.src_text->at(index) == '\n') {
                line_start++;
                col_start = 0;
            }
            index++;
            col_start++;
        }

        // now at the start of the token
        auto line_end = line_start;
        auto col_end = col_start;
        while (index < rhs.pos + rhs.len) {
            if (rhs.src_text->at(index) == '\n') {
                line_end++;
                col_end = 0;
            }
            index++;
            col_end++;
        }

        lhs << '[';
        if (line_start == line_end) lhs << line_start << ':' << col_start << '-' << col_end;
        else
            lhs << line_start << ':' << col_start << '-' << line_end << ':' << col_end;

        return lhs << "] " << rhs.src_text->substr(rhs.pos, rhs.len);
    }
};

#endif // NEW_J_COMPILER_TOKEN_H
