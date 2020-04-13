//
// Created by nick on 4/6/20.
//

#include "lexer.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

std::optional<token_type> keyword(const std::string & identifier) {
    static std::map<std::string, token_type> keywords;
    if (keywords.empty()) {
        keywords.emplace("int32", token_type::Int32);
        keywords.emplace("int64", token_type::Int64);
        keywords.emplace("const", token_type::Const);
        keywords.emplace("func", token_type::Func);
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

token lexer::next() {

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

    const auto & input_text = std::get<std::shared_ptr<std::string>>(this->src);

    {
        // The following two lambdas help preprocess the input
        // by removing non-newline whitespace and comments.
        // They are scoped to decrease name pollution and possibly help the compiler
        // (mostly to help humans)
        const auto consume_whitespace = [&input_text, this] {
            const auto start = current_pos;
            while (current_pos < input_text->size() and
                   (input_text->at(current_pos) == ' ' or input_text->at(current_pos) == '\t'))
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

        while (consume_whitespace() or consume_comment())
            ;
    }

    if (current_pos >= input_text->size())
        return {input_text, input_text->size() - 1, 1, token_type::EndOfFile};

    auto current_char = input_text->at(current_pos);

    const auto start = current_pos;
    current_pos++;

    if (isalpha(current_char)) {
        while (isalnum(input_text->at(current_pos)))
            current_pos++;

        const auto length = current_pos - start;

        const auto tokentype =
            keyword(input_text->substr(start, length)).value_or(token_type::Identifier);

        return token{input_text, start, length, tokentype};

    } else if (isdigit(current_char)) {

        if (current_char == '0') {
            current_char = input_text->at(current_pos);
            if (tolower(current_char) == 'x') {
                // Eat hexadecimal literal
                current_pos++;
                while (isxdigit(input_text->at(current_pos)))
                    current_pos++;

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
                while (isdigit(input_text->at(current_pos)))
                    current_pos++;

                return {input_text, start, current_pos - start, token_type::Float};
            } else {
                // Just 0
                return {input_text, start, 1, token_type::Int};
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
                    is_float ? token_type::Float : token_type::Int};
        }

    } else {
        switch (current_char) {
        case ';':
            return {input_text, start, 1, token_type ::Semi};
        case ':':
            return {input_text, start, 1, token_type ::Colon};
        case '=':
            return {input_text, start, 1, token_type ::Assign};
        case '\n':
            return {input_text, start, 1, token_type ::Newline};
        case '(':
            return {input_text, start, 1, token_type ::LParen};
        case ')':
            return {input_text, start, 1, token_type ::RParen};
        case '{':
            return {input_text, start, 1, token_type ::LBrace};
        case '}':
            return {input_text, start, 1, token_type ::RBrace};
        case ',':
            return {input_text, start, 1, token_type ::Comma};
        default:
            return {input_text, start, 1, token_type ::EndOfFile};
        }
    }
}

token lexer::peek() {

    // If there is nothing peeked, we need to read the next token.
    if (not peeked.has_value())
        peeked = this->next();

    return peeked.value();
}
