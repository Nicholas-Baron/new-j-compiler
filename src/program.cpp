//
// Created by nick on 4/10/20.
//

#include "nodes.h"

#include <algorithm>
#include <iostream>

namespace ast {

void fold(const_decl * declaration) {
    if (declaration == nullptr) return;

    auto * value = declaration->value_expr();

    if (value == nullptr) {
        std::cerr << "Null target value on the declaration of " << declaration->identifier()
                  << std::endl;
        return;
    }

    if (value->type() == node_type::func_call and declaration->in_global_scope()) {
        std::cerr << "Cannot have function calls in " << declaration->identifier()
                  << " as it is in global scope" << std::endl;
        return;
    }

    if (value->has_children()) switch (value->type()) {
        case node_type::value:
            break;
        default:
            std::cerr << "Cannot fold " << value->text() << std::endl;
            break;
        }
}
top_level * program::find(const std::string & id) const {

    const auto iter = std::find_if(this->items.begin(), this->items.end(),
                                   [&id](const auto & item) { return id == item->identifier(); });

    return iter == items.end() ? nullptr : iter->get();
}
bool program::add_item(std::unique_ptr<top_level> item) {
    if (not this->find(item->identifier())) {
        if (auto * decl = dynamic_cast<const_decl *>(item.get()); decl != nullptr) {
            ast::fold(decl);
        }
        this->items.push_back(std::move(item));
        return true;
    } else
        return false;
}
} // namespace ast