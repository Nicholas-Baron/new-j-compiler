//
// Created by nick on 4/17/20.
//

#include "ir.h"

#include <algorithm>

namespace ir {

bool program::function_exists(const std::string & name, size_t param_count) const noexcept {
    return std::find_if(this->program.begin(), this->program.end(),
                        [&](const auto & func) -> bool {
                            return func->name == name and func->parameters.size() == param_count;
                        })
           != this->program.end();
}

function * program::register_function(const std::string & name) {

    auto ptr = std::make_unique<ir::function>();
    auto * to_ret = ptr.get();
    program.insert(std::move(ptr));

    to_ret->name = name;
    return to_ret;
}
} // namespace ir