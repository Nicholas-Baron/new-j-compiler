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
    node(std::shared_ptr<const std::string> text, size_t position, size_t end_pos)
        : src_text{std::move(text)}, start{position}, end{end_pos} {}

    virtual ~node() noexcept = default;
    virtual void accept(visitor &) {}

    // Functions to help with debug printing
    [[nodiscard]] auto start_pos() const noexcept { return start; }
    [[nodiscard]] auto end_pos() const noexcept { return end; }
    [[nodiscard]] auto text() const noexcept { return src_text->substr(start, end - start); }

  private:
    std::shared_ptr<const std::string> src_text;
    size_t start, end;
};

} // namespace ast

#endif // NEW_J_COMPILER_NODES_H
