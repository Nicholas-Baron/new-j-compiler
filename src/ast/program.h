//
// Created by nick on 6/3/20.
//

#ifndef NEW_J_COMPILER_PROGRAM_H
#define NEW_J_COMPILER_PROGRAM_H

#include "node_forward.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ast {

class program final {
  public:
    bool add_item(std::unique_ptr<top_level> item);
    [[nodiscard]] top_level * find(const std::string & id) const;

    void visit(const std::function<void(top_level &)> & visitor) {
        for (auto & item : items) visitor(*item);
    }

  private:
    std::vector<std::unique_ptr<top_level>> items{};
};

} // namespace ast
#endif // NEW_J_COMPILER_PROGRAM_H
