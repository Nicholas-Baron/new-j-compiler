//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_VISITOR_H
#define NEW_J_COMPILER_VISITOR_H

#include "node_forward.h"

class visitor {
  public:
    constexpr visitor() noexcept = default;

    visitor(const visitor &) = delete;
    visitor & operator=(const visitor &) = delete;

    visitor(visitor &&) = default;
    visitor & operator=(visitor &&) = default;

    virtual ~visitor() noexcept = default;

    virtual void visit(ast::node &) {}
};

#endif // NEW_J_COMPILER_VISITOR_H
