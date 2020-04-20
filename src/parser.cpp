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
        if (next_item == nullptr) std::cerr << "Could not parse top level item\n";
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
    if (lex.peek().type() == token_type::LParen) { params = parse_params(); }

    std::cout << "Parameters read for function " << name << ": " << params.size() << '\n';

    std::optional<token> return_type;
    if (lex.peek().type() == token_type::Colon) {
        consume();
        return_type = lex.next();
    }

    return std::make_unique<ast::function>(std::move(name), std::move(params),
                                           std::move(return_type), parse_statement());
}

std::unique_ptr<ast::statement> parser::parse_statement() {

    consume_newlines_and_semis();
    switch (lex.peek().type()) {
    case token_type ::LBrace:
        return parse_stmt_block();
    case token_type ::Identifier:
        return parse_identifier_stmt();
    case token_type ::If:
        return parse_if_stmt();
    case token_type ::Return:
        return parse_return_stmt();
    default:
        std::cerr << "Unexpected start of statement " << lex.peek() << '\n';
        return nullptr;
    }
}

void parser::consume_newlines_and_semis() {
    while (lex.peek().type() == token_type::Newline or lex.peek().type() == token_type::Semi)
        consume();
}

bool parser::done() {
    consume_newlines_and_semis();
    return this->lex.peek().type() == token_type::EndOfFile;
}

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

        if (lex.peek().type() == token_type::RParen) {
            consume();
            break;
        } else if (lex.peek().type() == token_type::Comma)
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
    while (lex.peek().type() != token_type::RBrace) { block.append(parse_statement()); }

    // Will be a RBrace
    block.terminate(consume());

    return std::make_unique<ast::stmt_block>(std::move(block));
}

std::unique_ptr<ast::statement> parser::parse_identifier_stmt() {
    auto identifier = consume();

    if (lex.peek().type() == token_type::LParen) return parse_call_stmt(std::move(identifier));
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
    if (lex.peek().type() != token_type::RParen) {
        args.push_back(parse_expression(0));
        while (lex.peek().type() == token_type::Comma) {
            consume();
            args.push_back(parse_expression(0));
        }
        // Should be RParen
        if (auto tok = consume(); tok.type() != token_type::RParen)
            std::cerr << "Expected closing parenthesis for argument list. Got " << tok
                      << " instead\n";
    }

    return args;
}

std::unique_ptr<ast::if_stmt> parser::parse_if_stmt() {
    consume(); // should be the if token

    if (consume().type() != token_type::LParen) {
        std::cerr << "If requires an opening parenthesis.\n";
        return nullptr;
    }

    auto condition = parse_expression(0);

    if (consume().type() != token_type::RParen) {
        std::cerr << "If requires a closing parenthesis.\n";
        return nullptr;
    }

    auto then_block = parse_statement();

    if (then_block->type() == ast::node_type::statement_block
        and lex.peek().type() == token_type::Else) {

        consume();
        consume_newlines_and_semis();

        switch (lex.peek().type()) {
        case token_type ::If:
        case token_type ::LBrace:
            return std::make_unique<ast::if_stmt>(std::move(condition), std::move(then_block),
                                                  parse_statement());
        default:
            std::cerr << "Only '{' or 'if' allowed after 'else'.\n";
            return nullptr;
        }
    } else
        return std::make_unique<ast::if_stmt>(std::move(condition), std::move(then_block));
}

std::unique_ptr<ast::ret_stmt> parser::parse_return_stmt() {
    auto tok = consume();
    if (tok.type() != token_type::Return) {
        std::cerr << "Return statement must start with return.\n";
        return nullptr;
    }

    if (match_expr()) return std::make_unique<ast::ret_stmt>(std::move(tok), parse_expression(0));
    else
        return std::make_unique<ast::ret_stmt>(std::move(tok));
}

// Expression parsing

static int op_precedence(const token & tok) {
    switch (tok.type()) {
    case token_type ::LParen:
        return 25;
    case token_type ::Lt:
    case token_type ::Le:
        return 15;
    case token_type ::Eq:
        return 12;
    case token_type ::Boolean_Or:
    case token_type ::Boolean_And:
        return 1;
    default:
        std::cerr << "Asked for precedence of " << tok << '\n';
        return -1;
    }
}

static associativity op_associativity(const token & tok) {
    switch (tok.type()) {
    case token_type::LParen:
    case token_type ::Lt:
    case token_type ::Le:
    case token_type ::Boolean_Or:
    case token_type ::Boolean_And:
    case token_type ::Eq:
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
        if (precedence < min_preced) break;
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
    case token_type ::StringLiteral:
        return std::make_unique<ast::literal_or_variable>(consume());
    default:
        std::cerr << "Unexpected token as primary expression " << consume() << '\n';
        return nullptr;
    }
}
bool parser::match_expr() {
    switch (lex.peek().type()) {
    case token_type ::LParen:
    case token_type ::StringLiteral:
    case token_type ::Float:
    case token_type ::Int:
    case token_type ::Identifier:
        return true;
    default:
        return false;
    }
}
bool parser::match_secondary_expr() {
    switch (lex.peek().type()) {
    case token_type ::LParen:
    case token_type ::Lt:
    case token_type ::Le:
    case token_type ::Plus:
    case token_type ::Minus:
    case token_type ::Boolean_Or:
    case token_type ::Eq:
        return true;
    default:
        std::cerr << "Token " << lex.peek() << " is not a secondary expression.\n";
        [[fallthrough]];
    case token_type ::RParen:
    case token_type ::Newline:
    case token_type ::Semi:
        return false;
    }
}
std::unique_ptr<ast::expression> parser::parse_secondary_expr(std::unique_ptr<ast::expression> lhs,
                                                              int precedence,
                                                              associativity associativity) {
    switch (const auto op = consume(); op.type()) {
    case token_type ::Le:
        return std::make_unique<ast::bin_op>(std::move(lhs), ast::bin_op::operation::le,
                                             parse_expression(precedence, associativity));
    case token_type ::Eq:
        return std::make_unique<ast::bin_op>(std::move(lhs), ast::bin_op::operation::eq,
                                             parse_expression(precedence, associativity));
    case token_type ::Boolean_Or:
        return std::make_unique<ast::bin_op>(std::move(lhs), ast::bin_op::operation::boolean_or,
                                             parse_expression(precedence, associativity));
    default:
        std::cerr << "Unexpected token in secondary expression " << op << '\n';
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
