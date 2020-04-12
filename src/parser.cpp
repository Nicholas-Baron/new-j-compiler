//
// Created by nick on 4/10/20.
//

#include "parser.h"
#include "nodes.h"
#include <iostream>
std::unique_ptr<ast::program> parser::parse_program() {

    ast::program prog{};
    while (not done()) {
        auto next_item = parse_top_level();
        if (next_item == nullptr)
            std::cerr << "Could not parse top level item\n";
        else if (not prog.find(next_item->identifier()))
            prog.add_item(std::move(next_item));
        else
            std::cerr << "Top Level Item " << next_item->identifier() << " already exists\n";
    }
    return std::make_unique<ast::program>(std::move(prog));
}
std::unique_ptr<ast::top_level> parser::parse_top_level() {
    switch (auto next_token_type = lex.peek().type(); next_token_type) {
    case token_type ::Func:
        return this->parse_function();
    case token_type ::Const:
        // return this->parse_const_decl();
    case token_type ::Struct:
        // return this->parse_struct_decl();
    default:
        std::cerr << "Token " << lex.peek() << " cannot start a top level item\n";
        return nullptr;
    }
}

std::unique_ptr<ast::function> parser::parse_function() {
    // The func token is definitely here
    consume();

    auto name = consume();

    // Generate parameter list
    std::vector<ast::parameter> params;
    if (lex.peek().type() == token_type::LParen) {
        params = parse_params();
    }

    std::optional<token> return_type;
    if (lex.peek().type() == token_type::Colon) {
        consume();
        return_type = lex.next();
    }

    return std::make_unique<ast::function>(std::move(name), std::move(params),
                                           std::move(return_type), parse_statement());
}

std::unique_ptr<ast::statement> parser::parse_statement() { return nullptr; }

bool parser::done() { return this->lex.peek().type() == token_type::EndOfFile; }

token parser::consume() { return this->lex.next(); }

std::vector<ast::parameter> parser::parse_params() {
    if (consume().type() != token_type::LParen) {
        std::cerr << "Tried to parse parameters without starting parenthesis\n";
        return {};
    }

    std::vector<ast::parameter> params;

    while (true) {

        auto name = consume();
        consume(); // colon
        auto type = consume();
        params.emplace_back(std::move(name), std::move(type));

        if (lex.peek().type() == token_type::RParen)
            break;
        else if (lex.peek().type() == token_type::Comma)
            consume();
    }

    return params;
}
