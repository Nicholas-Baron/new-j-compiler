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
        return this->parse_const_decl();
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

std::unique_ptr<ast::statement> parser::parse_statement() {
    switch (lex.peek().type()) {
    case token_type ::LBrace:
        return parse_stmt_block();
    case token_type ::Identifier:
        return parse_identifier_stmt();
    default:
        std::cerr << "Unexpected start of statement " << lex.peek();
        return nullptr;
    }
}

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
        else {
            std::cerr << "Unexpected token in parameter list " << lex.peek();
            return {};
        }
    }

    return params;
}

std::unique_ptr<ast::stmt_block> parser::parse_stmt_block() {
    if (lex.peek().type() != token_type::LBrace) {
        std::cerr << "Statement blocks always start with {\n";
        return nullptr;
    }

    ast::stmt_block block{consume()};
    while (lex.peek().type() != token_type::RBrace) {
        block.append(parse_statement());
    }

    // Will be a RBrace
    block.terminate(consume());

    return std::make_unique<ast::stmt_block>(std::move(block));
}

std::unique_ptr<ast::statement> parser::parse_identifier_stmt() {
    auto identifier = consume();

    if (lex.peek().type() == token_type::LParen)
        return parse_call_stmt(std::move(identifier));
    else {
        std::cerr << "Unexpected token after identifier " << lex.peek();
        return nullptr;
    }
}

std::unique_ptr<ast::statement> parser::parse_call_stmt(token && tok) {
    return std::make_unique<ast::func_call>(std::move(tok), parse_arguments());
}

std::vector<std::unique_ptr<ast::expression>> parser::parse_arguments() {
    if (lex.peek().type() != token_type::LParen) {
        std::cerr << "Unexpected token at start of arguments: " << lex.peek();
        return {};
    }

    consume();
    std::vector<std::unique_ptr<ast::expression>> args;
    while (lex.peek().type() != token_type::RParen) {
        args.push_back(parse_expression(0));
        if (consume().type() != token_type::Comma) {
            std::cerr << "Expected comma to separate expressions in argument list\n";
        }
    }

    return args;
}

// Expression parsing

int op_precedence(const token & tok) {
    switch (tok.type()) {
    case token_type ::LParen:
        return 25;
    default:
        std::cerr << "Asked for precedence of " << tok << '\n';
        return -1;
    }
}

associativity op_associativity(const token & tok) {
    switch (tok.type()) {
    case token_type::LParen:
        return associativity::left;
    default:
        std::cerr << "Asked for associativity of " << tok << '\n';
        return associativity::right;
    }
}

std::unique_ptr<ast::expression> parser::parse_expression(int min_preced, associativity assoc) {

    auto expr = parse_primary_expr();
    while (match_secondary_expr()) {
        auto precedence = op_precedence(lex.peek());
        if (precedence < min_preced)
            break;
        else if (precedence == min_preced and assoc == associativity::left)
            break;

        auto associate = op_associativity(lex.peek());
        expr = parse_secondary_expr(std::move(expr), precedence, associate);
    }

    return expr;
}
std::unique_ptr<ast::expression> parser::parse_primary_expr() {
    switch (lex.peek().type()) {
    case token_type ::Identifier:
    case token_type ::Int:
    case token_type ::Float:
        return std::make_unique<ast::value>(consume());
    default:
        std::cerr << "Unexpected token as primary expression " << consume() << '\n';
        return nullptr;
    }
}
bool parser::match_secondary_expr() {
    switch (lex.peek().type()) {
    case token_type ::LParen:
        return true;
    default:
        return false;
    }
}
std::unique_ptr<ast::expression> parser::parse_secondary_expr(std::unique_ptr<ast::expression> lhs,
                                                              int precedence,
                                                              associativity associativity) {
    switch (lex.peek().type()) {
    default:
        std::cerr << "Unexpected token in secondary expression " << lex.peek() << '\n';
        return lhs;
    }
}
std::unique_ptr<ast::const_decl> parser::parse_const_decl() {
    if (consume().type() != token_type::Const) {
        std::cerr << "A const declaration can only start with const\n";
        return nullptr;
    }

    auto ident = parse_opt_typed();

    if (consume().type() != token_type::Assign) {
        std::cerr << "Declarations must use '='\n";
        return nullptr;
    }

    return std::make_unique<ast::const_decl>(std::move(ident), parse_expression(0), true);
}
ast::opt_typed parser::parse_opt_typed() {
    auto ident = consume();
    if (lex.peek().type() == token_type::Colon) {
        consume();
        return ast::opt_typed{std::move(ident), consume()};
    }
    return ast::opt_typed{std::move(ident)};
}
