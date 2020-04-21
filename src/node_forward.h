//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODE_FORWARD_H
#define NEW_J_COMPILER_NODE_FORWARD_H

namespace ast {

// All node types
enum struct node_type {
    binary_op,
    const_decl,
    func_call,
    function,
    if_statement,
    let_decl,
    opt_typed,
    parameter,
    parameter_list,
    return_statement,
    statement_block,
    value,
};

// Not part of the AST, just a container for it
class program;

// Base class
class node;

// AST helpers
class opt_typed;

// Intermediate classes
class statement;
class expression;
class top_level;

// function related
class function;
class parameter;

// declarations
class const_decl;
class let_decl;

// Statements
class if_stmt;
class ret_stmt;

// expressions
class bin_op;
class literal_or_variable;

} // namespace ast

#endif // NEW_J_COMPILER_NODE_FORWARD_H
