//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_PARSER_H
#define NEW_J_COMPILER_PARSER_H

#include "lexer.h"
#include "node_forward.h"
#include "nodes.h"

#include <vector>

enum struct associativity { left, right };

class parser final {
  public:
    explicit parser(lexer && lex_in) : lex{std::move(lex_in)} {}

    std::unique_ptr<ast::program> parse_program();

  private:
    bool done();
    token consume();
    void consume_newlines_and_semis();

    bool match_expr();
    bool match_unary_expr();
    bool match_secondary_expr();
    bool match_stmt();

    // Top level items
    std::unique_ptr<ast::top_level> parse_top_level();
    std::unique_ptr<ast::function> parse_function();
    std::vector<ast::parameter> parse_params();

    std::unique_ptr<ast::const_decl> parse_const_decl();
    ast::opt_typed parse_opt_typed();

    // Statements
    std::unique_ptr<ast::statement> parse_statement();
    std::unique_ptr<ast::stmt_block> parse_stmt_block();
    std::unique_ptr<ast::statement> parse_identifier_stmt();
    std::unique_ptr<ast::func_call> parse_call(std::unique_ptr<ast::expression> tok);
    std::unique_ptr<ast::if_stmt> parse_if_stmt();
    std::unique_ptr<ast::ret_stmt> parse_return_stmt();

    std::vector<std::unique_ptr<ast::expression>> parse_arguments();

    // Expressions
    // TODO: Make min_preced optional
    std::unique_ptr<ast::expression> parse_expression(int min_preced = 0,
                                                      associativity assoc = associativity::right);
    std::unique_ptr<ast::expression> parse_primary_expr();
    std::unique_ptr<ast::expression>
    parse_secondary_expr(std::unique_ptr<ast::expression> unique_ptr, int precedence,
                         associativity associativity);

    lexer lex;
};

#endif // NEW_J_COMPILER_PARSER_H
