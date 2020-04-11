//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_VISITOR_H
#define NEW_J_COMPILER_VISITOR_H

#include "node_forward.h"

class visitor {
  public:
    virtual ~visitor() noexcept = default;

    virtual visit(ast::node &) {}
};

#endif // NEW_J_COMPILER_VISITOR_H
