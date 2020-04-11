//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODE_FORWARD_H
#define NEW_J_COMPILER_NODE_FORWARD_H

namespace ast {

// Not part of the AST, just a container for it
class program;

// Base class
class node;

// Intermediate classes
class statement;
class expression;
class topLevel;

} // namespace ast

#endif // NEW_J_COMPILER_NODE_FORWARD_H
