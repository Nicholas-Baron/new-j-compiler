//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODE_FORWARD_H
#define NEW_J_COMPILER_NODE_FORWARD_H

namespace ast {

// All node types
enum struct node_type {
    binary_op,
    func_call,
    function,
    if_statement,
    opt_typed,
    parameter,
    return_statement,
    statement_block,
    value,
    var_decl,
    while_loop,
};

// Not part of the AST, just a container for it
class program;

// Base class
struct node;

// AST helpers
class opt_typed;

// Intermediate classes
struct statement;
struct expression;
struct top_level;

// function related
struct function;
struct parameter;
struct func_call;

// declarations
struct var_decl;

// Statements
struct if_stmt;
struct ret_stmt;
struct stmt_block;
struct while_loop;

// expressions
struct bin_op;
struct literal_or_variable;

} // namespace ast

#endif // NEW_J_COMPILER_NODE_FORWARD_H
