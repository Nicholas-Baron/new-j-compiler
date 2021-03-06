//
// Created by nick on 4/6/20.
//

#include "lexer.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace {
std::optional<token_type> keyword(const std::string & identifier) {
    static std::map<std::string, token_type> keywords;
    if (keywords.empty()) {
        keywords.emplace("int32", token_type::Int32);
        keywords.emplace("int64", token_type::Int64);
        keywords.emplace("const", token_type::Const);
        keywords.emplace("func", token_type::Func);
        keywords.emplace("if", token_type::If);
        keywords.emplace("else", token_type::Else);
        keywords.emplace("ret", token_type::Return);
        keywords.emplace("return", token_type::Return);
        keywords.emplace("or", token_type::Boolean_Or);
        keywords.emplace("let", token_type::Let);
        keywords.emplace("while", token_type::While);
    }

    for (const auto & entry : keywords) {
        if (std::equal(
                entry.first.begin(), entry.first.end(), identifier.begin(), identifier.end(),
                [](const auto & lhs, const auto & rhs) { return tolower(lhs) == tolower(rhs); })) {
            return entry.second;
        }
    }

    return {};
}

std::optional<token_type> punctuation(char symbol) {
    static std::map<char, token_type> symbols;
    if (symbols.empty()) {
        symbols.emplace(';', token_type::Semi);
        symbols.emplace(',', token_type::Comma);
        symbols.emplace(':', token_type::Colon);
        symbols.emplace('(', token_type::LParen);
        symbols.emplace(')', token_type::RParen);
        symbols.emplace('\n', token_type::Newline);
        symbols.emplace('{', token_type::LBrace);
        symbols.emplace('}', token_type::RBrace);
    }

    if (auto iter = symbols.find(symbol); iter != symbols.end()) {
        return iter->second;
    } else {
        return {};
    }
}

} // namespace

token lexer::next() {

    // If we have previously peeked, just return the peeked item.
    if (peeked.has_value()) {
        auto to_ret = peeked.value();
        peeked.reset();
        return to_ret;
    }

    // Now we actually need to parse something
    // Check if we have to open a file
    if (std::holds_alternative<std::string>(this->src)) {
        // Open the file
        const auto filename = std::get<std::string>(this->src);
        std::stringstream stream;
        {
            std::ifstream file{filename};
            std::string temp;
            while (std::getline(file, temp)) stream << temp << '\n';
        }

        // Store the data
        this->src = std::make_shared<std::string>(stream.str());
    }

    const auto & input_text = std::get<std::shared_ptr<std::string>>(this->src);

    {
        // The following two lambdas help preprocess the input
        // by removing non-newline whitespace and comments.
        // They are scoped to decrease name pollution and possibly help the compiler
        // (mostly to help humans)
        const auto consume_whitespace = [&input_text, this] {
            const auto start = current_pos;
            while (current_pos < input_text->size()
                   and (input_text->at(current_pos) == ' ' or input_text->at(current_pos) == '\t'))
                current_pos++;

            return start != current_pos;
        };

        const auto consume_comment = [&input_text, this] {
            const auto start = current_pos;
            if (current_pos < input_text->size() and input_text->at(current_pos) == '#') {
                while (current_pos < input_text->size() and input_text->at(current_pos) != '\n')
                    ++current_pos;

                ++current_pos;
            }
            return start != current_pos;
        };

        while (consume_whitespace() or consume_comment()) {}
    }

    if (current_pos >= input_text->size())
        return {input_text, input_text->size() - 1, 1, token_type::EndOfFile};

    auto current_char = input_text->at(current_pos);

    const auto start = current_pos;
    current_pos++;

    if (isalpha(current_char)) {
        while (isalnum(input_text->at(current_pos))) current_pos++;

        const auto length = current_pos - start;

        const auto tokentype
            = keyword(input_text->substr(start, length)).value_or(token_type::Identifier);

        return token{input_text, start, length, tokentype};

    } else if (isdigit(current_char)) {

        if (current_char == '0') {
            current_char = input_text->at(current_pos);
            if (tolower(current_char) == 'x') {
                // Eat hexadecimal literal
                current_pos++;
                while (isxdigit(input_text->at(current_pos))) current_pos++;

                return {input_text, start, current_pos - start, token_type::Int};
            } else if (tolower(current_char) == 'b') {
                // Eat binary literal
                current_pos++;
                while (input_text->at(current_pos) == '0' or input_text->at(current_pos) == '1')
                    current_pos++;

                return {input_text, start, current_pos - start, token_type::Int};
            } else if (current_char == '.') {
                // Eat float literal
                current_pos++;
                while (isdigit(input_text->at(current_pos))) current_pos++;

                return {input_text, start, current_pos - start, token_type::Float};
            } else {
                // Just 0
                return {input_text, start, 1, token_type::Int};
            }
        } else {
            // Does not start on a zero
            while (isdigit(input_text->at(current_pos))) current_pos++;

            bool is_float = false;
            if (input_text->at(current_pos) == '.') {
                is_float = true;
                current_pos++;

                // TODO: Remove trailing zeros
                while (isdigit(input_text->at(current_pos))) current_pos++;
            }

            return {input_text, start, current_pos - start,
                    is_float ? token_type::Float : token_type::Int};
        }

    } else if (current_char == '"') {
        // Read in string literal
        char last_char = '"';
        bool done = false;
        while (not done) {
            while (current_pos < input_text->size() and input_text->at(current_pos) != '"') {
                last_char = input_text->at(current_pos);
                current_pos++;
            }
            if (last_char != '\\') {
                current_pos++;
                done = true;
            }
        }
        return {input_text, start, current_pos - start, token_type ::StringLiteral};
    } else {

        auto possible_token_type = punctuation(current_char);
        if (possible_token_type.has_value())
            return {input_text, start, 1, possible_token_type.value()};

        switch (current_char) {
        case '=':
            if (current_pos >= input_text->size() or input_text->at(current_pos) != current_char)
                return {input_text, start, 1, token_type ::Assign};
            else {
                current_pos++;
                return {input_text, start, 2, token_type::Eq};
            }
        case '|':
            if (current_pos >= input_text->size() or input_text->at(current_pos) != current_char)
                return {input_text, start, 1, token_type ::Bit_Or};
            else {
                current_pos++;
                return {input_text, start, 2, token_type ::Boolean_Or};
            }
        case '<':
            if (current_pos >= input_text->size()
                or (input_text->at(current_pos) != current_char
                    and input_text->at(current_pos) != '='))
                return {input_text, start, 1, token_type ::Lt};
            else if (input_text->at(current_pos) == current_char) {
                current_pos++;
                return {input_text, start, 2, token_type ::Shl};
            } else {
                current_pos++;
                return {input_text, start, 2, token_type ::Le};
            }
        case '>':
            if (current_pos >= input_text->size()
                or (input_text->at(current_pos) != current_char
                    and input_text->at(current_pos) != '=')) {
                return {input_text, start, 1, token_type::Gt};
            } else if (input_text->at(current_pos) == current_char) {
                current_pos++;
                return {input_text, start, 2, token_type::Shr};
            } else {
                current_pos++;
                return {input_text, start, 2, token_type::Ge};
            }
        case '+':
            if (current_pos >= input_text->size() or input_text->at(current_pos) != '=')
                return {input_text, start, 1, token_type ::Plus};
            else {
                current_pos++;
                return {input_text, start, 2, token_type ::Plus_Assign};
            }
        case '-':
            if (current_pos >= input_text->size() or input_text->at(current_pos) != '=')
                return {input_text, start, 1, token_type ::Minus};
            else {
                current_pos++;
                return {input_text, start, 2, token_type ::Minus_Assign};
            }
        case '*':
            if (current_pos >= input_text->size() or input_text->at(current_pos) != '=')
                return {input_text, start, 1, token_type::Mult};
            else {
                current_pos++;
                return {input_text, start, 2, token_type::Mult_Assign};
            }
        default:
            std::cerr << "Unrecognized symbol: " << current_char << '\n';
            return {input_text, start, 1, token_type ::EndOfFile};
        }
    }
}

token lexer::peek() {

    // If there is nothing peeked, we need to read the next token.
    if (not peeked.has_value()) peeked = this->next();

    return peeked.value();
}
