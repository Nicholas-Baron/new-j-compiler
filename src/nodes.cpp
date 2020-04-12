//
// Created by nick on 4/10/20.
//

#include "nodes.h"
#include <algorithm>

namespace ast {
top_level * program::find(const std::string & id) const {

    const auto iter = std::find_if(this->items.begin(), this->items.end(),
                                   [&id](const auto & item) { return id == item->identifier(); });

    return iter == items.end() ? nullptr : iter->get();
}
bool program::add_item(std::unique_ptr<top_level> item) {
    if (not this->find(item->identifier())) {
        this->items.push_back(std::move(item));
        return true;
    } else
        return false;
}
} // namespace ast