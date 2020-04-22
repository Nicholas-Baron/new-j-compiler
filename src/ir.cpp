//
// Created by nick on 4/17/20.
//

#include "ir.h"

#include <algorithm>
#include <ostream>

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

std::ostream & operator<<(std::ostream & lhs, const operand & rhs) {
    lhs << '(';
    switch (rhs.type) {
    case ir_type::unit:
        lhs << "unit";
        break;
    case ir_type::boolean:
        if (const auto * val = std::get_if<bool>(&rhs.data); val != nullptr)
            lhs << "boolean imm. " << *val;
        else
            lhs << "boolean " << std::get<std::string>(rhs.data);
        break;
    case ir_type::str:
        if (rhs.is_immediate) lhs << "string imm. " << std::get<std::string>(rhs.data);
        else
            lhs << "string " << std::get<std::string>(rhs.data);
        break;
    case ir_type::i32:
        if (rhs.is_immediate) lhs << "i32 imm. " << std::get<long>(rhs.data);
        else
            lhs << "i32 " << std::get<std::string>(rhs.data);
        break;
    case ir_type::i64:
        if (rhs.is_immediate) lhs << "i64 imm. " << std::get<long>(rhs.data);
        else
            lhs << "i64 " << std::get<std::string>(rhs.data);
        break;
    case ir_type::f32:
        if (rhs.is_immediate) lhs << "f32 imm. " << std::get<double>(rhs.data);
        else
            lhs << "f32 " << std::get<std::string>(rhs.data);
        break;
    case ir_type::f64:
        if (rhs.is_immediate) lhs << "f64 imm. " << std::get<double>(rhs.data);
        else
            lhs << "f64 " << std::get<std::string>(rhs.data);
        break;
    case ir_type::func:
        lhs << "func " << std::get<std::string>(rhs.data);
        break;
    default:
        lhs << "unknown type";
        break;
    }
    return lhs << ')';
}

} // namespace ir