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

std::ostream & operator<<(std::ostream & lhs, const three_address & rhs) {
    if (auto result = rhs.result(); result) lhs << result.value() << " = ";

    switch (rhs.op) {
    case operation::add:
        return lhs << rhs.operands.at(1) << " + " << rhs.operands.at(2);
    case operation::sub:
        return lhs << rhs.operands.at(1) << " - " << rhs.operands.at(2);
    case operation::mul:
        return lhs << rhs.operands.at(1) << " * " << rhs.operands.at(2);
    case operation::div:
        return lhs << rhs.operands.at(1) << " / " << rhs.operands.at(2);
    case operation::shift_left:
        return lhs << rhs.operands.at(1) << " << " << rhs.operands.at(2);
    case operation::shift_right:
        return lhs << rhs.operands.at(1) << " >> " << rhs.operands.at(2);
    case operation::bit_or:
        return lhs << rhs.operands.at(1) << " | " << rhs.operands.at(2);
    case operation::bit_and:
        return lhs << rhs.operands.at(1) << " & " << rhs.operands.at(2);
    case operation::bool_or:
        return lhs << rhs.operands.at(1) << " || " << rhs.operands.at(2);
    case operation::bool_and:
        return lhs << rhs.operands.at(1) << " && " << rhs.operands.at(2);
    case operation::compare_equal:
        return lhs << rhs.operands.at(1) << " == " << rhs.operands.at(2);
    case operation::compare:
        return lhs << rhs.operands.at(1) << " < " << rhs.operands.at(2);
    case operation::assign:
        return lhs << rhs.operands.at(1) << " = " << rhs.operands.at(2);
    case operation::halt:
        lhs << "halt ";
        if (not rhs.operands.empty()) lhs << rhs.operands.front();
        break;
    case operation::branch:
        lhs << "branch ";
        if (rhs.operands.size() == 1) return lhs << rhs.operands.front();
        else
            for (const auto & op : rhs.operands) lhs << op << ' ';

        break;
    case operation::call:
        lhs << "call ";
        if (rhs.result())
            for (auto iter = rhs.operands.begin() + 1; iter != rhs.operands.end(); ++iter)
                lhs << *iter << ' ';
        else
            for (const auto & op : rhs.operands) lhs << op;

        break;
    case operation::ret:
        lhs << "ret ";
        if (not rhs.operands.empty()) lhs << rhs.operands.front();
        break;
    case operation::load:
        lhs << "load ";
        for (auto iter = rhs.operands.begin() + 1; iter != rhs.operands.end(); ++iter)
            lhs << *iter << ' ';
        break;
    case operation::store:
        lhs << "store ";
        for (const auto & op : rhs.operands) lhs << op << ' ';
        break;
    }

    return lhs;
}
std::optional<operand> three_address::result() const {
    if (operands.empty()) return {};

    switch (op) {
    case operation::add:
    case operation::sub:
    case operation::mul:
    case operation::div:
    case operation::shift_left:
    case operation::shift_right:
    case operation::bit_or:
    case operation::bit_and:
    case operation::bool_or:
    case operation::bool_and:
    case operation::compare_equal:
    case operation::compare:
    case operation::assign:
    case operation::call:
    case operation::load:
        return std::optional{operands.front()};
    default:
        return {};
    }
}
} // namespace ir