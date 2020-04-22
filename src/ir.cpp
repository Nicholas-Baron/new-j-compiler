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
function * program::lookup_function(const std::string & name, size_t param_count) const noexcept {
    if (not this->function_exists(name, param_count)) return nullptr;
    return std::find_if(this->program.begin(), this->program.end(),
                        [&](const auto & func) -> bool {
                            return func->name == name and func->parameters.size() == param_count;
                        })
        ->get();
}
bool basic_block::terminated() {
    if (contents.empty()) return false;
    else
        switch (this->contents.back().op) {
        case operation::halt:
        case operation::branch:
        case operation::ret:
            return true;
        default:
            return false;
        }
}
} // namespace ir