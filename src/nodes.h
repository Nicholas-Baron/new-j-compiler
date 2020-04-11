//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODES_H
#define NEW_J_COMPILER_NODES_H

#include <memory>

#include "node_forward.h"
#include "visitor.h"

namespace ast {

class node {
  public:
    node(std::shared_ptr<const std::string> text, size_t position, size_t length)
        : src_text{std::move(text)}, start{position}, len{length} {}

    virtual ~node() noexcept = default;
    virtual void accept(visitor &) {}

    // Functions to help with debug printing
    [[nodiscard]] auto start_pos() const noexcept { return start; }
    [[nodiscard]] auto end_pos() const noexcept { return start + len; }
    [[nodiscard]] auto text() const noexcept { return src_text->substr(start, len); }

  private:
    std::shared_ptr<const std::string> src_text;
    size_t start, len;
};

} // namespace ast

#endif // NEW_J_COMPILER_NODES_H
