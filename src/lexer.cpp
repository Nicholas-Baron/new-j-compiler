//
// Created by nick on 4/6/20.
//

#include "lexer.h"
#include <map>
#include <sstream>

std::optional<TokenType> keyword(const std::string & identifier) {
    static std::map<std::string, TokenType> keywords;
    if (keywords.empty()) {
        keywords.emplace("int32", TokenType::Int32);
        keywords.emplace("int64", TokenType::Int64);
        keywords.emplace("const", TokenType::Const);
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

Token Lexer::next() {

    // If we have a peeked value, we just return it
    if (peeked.has_value()) {
        const auto to_ret = peeked.value();
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
            while (std::getline(file, temp))
                stream << temp << '\n';
        }

        // Store the data
        this->src = std::make_shared<std::string>(stream.str());
    }

    auto & input_text = std::get<std::shared_ptr<std::string>>(this->src);

    const auto consume_whitespace = [&input_text, this] {
        while (current_pos < input_text->size() and
               (input_text->at(current_pos) == ' ' or input_text->at(current_pos) == '\t'))
            current_pos++;
    };

    consume_whitespace();
    if (current_pos >= input_text->size())
        return {input_text, input_text->size() - 1, 1, TokenType::EndOfFile};

    auto current_char = input_text->at(current_pos);

    const auto start = current_pos;
    current_pos++;

    if (isalpha(current_char)) {
        while (isalnum(input_text->at(current_pos)))
            current_pos++;

        const auto length = current_pos - start;

        const auto tokentype =
            keyword(input_text->substr(start, length)).value_or(TokenType::Identifier);

        return Token{input_text, start, length, tokentype};

    } else if (isdigit(current_char)) {

        if (current_char == '0') {
            current_char = input_text->at(current_pos);
            if (tolower(current_char) == 'x') {
                // Eat hexadecimal literal
                current_pos++;
                while (isxdigit(input_text->at(current_pos)))
                    current_pos++;

                return {input_text, start, current_pos - start, TokenType::Int};
            } else if (tolower(current_char) == 'b') {
                // Eat binary literal
                current_pos++;
                while (input_text->at(current_pos) == '0' or input_text->at(current_pos) == '1')
                    current_pos++;

                return {input_text, start, current_pos - start, TokenType::Int};
            } else if (current_char == '.') {
                // Eat float literal
                current_pos++;
                while (isdigit(input_text->at(current_pos)))
                    current_pos++;

                return {input_text, start, current_pos - start, TokenType::Float};
            } else {
                // Just 0
                return {input_text, start, 1, TokenType::Int};
            }
        } else {
            // Does not start on a zero
            while (isdigit(input_text->at(current_pos)))
                current_pos++;

            bool is_float = false;
            if (input_text->at(current_pos) == '.') {
                is_float = true;
                current_pos++;

                // TODO: Remove trailing zeros
                while (isdigit(input_text->at(current_pos)))
                    current_pos++;
            }

            return {input_text, start, current_pos - start,
                    is_float ? TokenType::Float : TokenType::Int};
        }

    } else {
        switch (current_char) {
        case ';':
            return {input_text, start, 1, TokenType ::Semi};
        case ':':
            return {input_text, start, 1, TokenType ::Colon};
        case '=':
            return {input_text, start, 1, TokenType ::Assign};
        case '\n':
            return {input_text, start, 1, TokenType ::Newline};
        case '(':
            return {input_text, start, 1, TokenType ::LParen};
        case ')':
            return {input_text, start, 1, TokenType ::RParen};
        default:
            return {input_text, start, 1, TokenType ::EndOfFile};
        }
    }
}

Token Lexer::peek() {

    // If there is nothing peeked, we need to read the next token.
    if (not peeked.has_value())
        peeked = this->next();

    return peeked.value();
}
