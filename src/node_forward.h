//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODE_FORWARD_H
#define NEW_J_COMPILER_NODE_FORWARD_H

namespace ast {

// All node types
enum struct node_type {
    const_decl,
    func_call,
    function,
    let_decl,
    parameter,
    parameter_list,
    statement_block,
    value,
    opt_typed,
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

// value declarations
class const_decl;
class let_decl;

} // namespace ast

#endif // NEW_J_COMPILER_NODE_FORWARD_H
