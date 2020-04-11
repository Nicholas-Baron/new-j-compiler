//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_PARSER_H
#define NEW_J_COMPILER_PARSER_H

#include "lexer.h"
#include "node_forward.h"

class parser final {
  public:
    explicit parser(lexer && lex_in) : lex{std::move(lex_in)} {}
    
    std::unique_ptr<ast::program> parse_program();

  private:
    bool done();
    token consume();
    
    bool match_expr();
    bool match_stmt();
    
    lexer lex;
};

#endif // NEW_J_COMPILER_PARSER_H
