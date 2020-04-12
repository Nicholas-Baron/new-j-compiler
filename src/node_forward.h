//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODE_FORWARD_H
#define NEW_J_COMPILER_NODE_FORWARD_H

namespace ast {

// All node types
enum struct node_type {
    argument,
    const_decl,
    func_call,
    function,
    let_decl,
    parameter,
    parameter_list,
    statement_block,
    node,
};

// Not part of the AST, just a container for it
class program;

// Base class
class node;

// Intermediate classes
class statement;
class expression;
class top_level;

// function related
class function;
class parameter;

} // namespace ast

#endif // NEW_J_COMPILER_NODE_FORWARD_H
