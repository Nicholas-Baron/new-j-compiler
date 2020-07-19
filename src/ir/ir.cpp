//
// Created by nick on 4/17/20.
//

#include "ir.h"

#include <algorithm>
#include <iostream>
#include <ostream>

namespace ir {

program::program() {
    types.emplace("int32", std::make_shared<ir::i32_type>());
    types.emplace("int64", std::make_shared<ir::i64_type>());
    types.emplace("float32", std::make_shared<ir::f32_type>());
    types.emplace("float64", std::make_shared<ir::f64_type>());
    types.emplace("boolean", std::make_shared<ir::boolean_type>());
    types.emplace("string", std::make_shared<ir::string_type>());
    types.emplace("unit", std::make_shared<ir::unit_type>());
}

bool program::function_exists(const std::string & name) const noexcept {
    return std::find_if(prog.begin(), prog.end(),
                        [&name](const auto & func) -> bool { return func->name == name; })
           != prog.end();
}

bool program::function_exists(const std::string & name, size_t param_count) const noexcept {

    return std::find_if(this->prog.begin(), this->prog.end(),
                        [&](const auto & func) -> bool {
                            return func->name == name and func->parameters().size() == param_count;
                        })
           != this->prog.end();
}

function * program::lookup_function(const std::string & name) const noexcept {
    if (not function_exists(name)) return nullptr;

    return std::find_if(prog.begin(), prog.end(),
                        [&name](const auto & func) -> bool { return func->name == name; })
        ->get();
}

function * program::register_function(const std::string & name,
                                      std::shared_ptr<ir::function_type> func_type) {

    if (this->function_exists(name, *func_type)) return lookup_function(name, *func_type);

    prog.push_back(std::make_unique<ir::function>(name, std::move(func_type)));
    return prog.back().get();
}
function * program::lookup_function(const std::string & name, size_t param_count) const noexcept {
    if (not this->function_exists(name, param_count)) return nullptr;

    return std::find_if(this->prog.begin(), this->prog.end(),
                        [&](const auto & func) -> bool {
                            return func->name == name and func->parameters().size() == param_count;
                        })
        ->get();
}

bool program::function_exists(const std::string & name,
                              const ir::function_type & func_type) const noexcept {
    return std::find_if(this->prog.begin(), this->prog.end(),
                        [&](const auto & func) -> bool {
                            return func->name == name and *func->type == func_type;
                        })
           != this->prog.end();
}

function * program::lookup_function(const std::string & name,
                                    const ir::function_type & func_type) const noexcept {
    if (not this->function_exists(name, func_type)) return nullptr;

    return std::find_if(this->prog.begin(), this->prog.end(),
                        [&](const auto & func) -> bool {
                            return func->name == name and *func->type == func_type;
                        })
        ->get();
}
std::shared_ptr<ir::type> program::lookup_type(const std::string & name) {
    auto iter = this->types.find(name);
    if (iter != types.end()) { return iter->second; }

    if (function_exists(name)) return this->lookup_function(name)->type;

    std::cerr << "Failed to lookup type " << name << std::endl;
    return nullptr;
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
    if (rhs.type == nullptr) { return lhs << "(null type)" << std::flush; }

    lhs << '(';
    switch (static_cast<ir_type>(*rhs.type)) {
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
    return lhs << ')' << std::flush;
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
    case operation::eq:
        return lhs << rhs.operands.at(1) << " == " << rhs.operands.at(2);
    case operation::lt:
        return lhs << rhs.operands.at(1) << " < " << rhs.operands.at(2);
    case operation::le:
        return lhs << rhs.operands.at(1) << " <= " << rhs.operands.at(2);
    case operation::gt:
        return lhs << rhs.operands.at(1) << " > " << rhs.operands.at(2);
    case operation::ge:
        return lhs << rhs.operands.at(1) << " >= " << rhs.operands.at(2);
    case operation::assign:
        return lhs << rhs.operands.back();
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
            for (const auto & op : rhs.operands) lhs << op << ' ';

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
    case operation::phi:
        lhs << "phi ";
        for (auto iter = rhs.operands.begin() + 1; iter != rhs.operands.end(); ++iter)
            lhs << *iter << ' ';
        break;
    default:
        lhs << "Unimplemented operation printer";
    }

    return lhs;
}
std::optional<operand> three_address::result() const {
    if (operands.empty()) return {};

    switch (op) {

    case operation::call:
        if (static_cast<ir_type>(*operands.front().type) == ir::ir_type::func) return {};
        else
            [[fallthrough]];
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
    case operation::lt:
    case operation ::le:
    case operation ::gt:
    case operation ::ge:
    case operation::eq:
    case operation ::ne:
    case operation::assign:
    case operation::load:
    case operation::phi:
        return std::optional{operands.front()};

    default:
        return {};
    }
}
std::vector<operand> three_address::inputs() const {
    // The operand at the front is usually the output
    switch (this->op) {
    case operation::add:
    case operation::bit_and:
    case operation::bit_or:
    case operation::bool_and:
    case operation::bool_or:
    case operation::div:
    case operation::eq:
    case operation::ge:
    case operation::gt:
    case operation::le:
    case operation::lt:
    case operation::mul:
    case operation::ne:
    case operation::phi:
    case operation::shift_left:
    case operation::shift_right:
    case operation::sub:
        return {operands.at(1), operands.at(2)};
    case operation::assign:
        return {operands.back()};
    case operation::branch:
    case operation::halt:
        return {operands.front()};
    case operation::call:
        if (result().has_value()) {
            std::vector<operand> to_ret;
            bool first = true;
            for (auto & operand : operands)
                if (first) first = false;
                else
                    to_ret.push_back(operand);

            return to_ret;
        } else
            return operands;

    case operation::load: {
        std::vector<operand> to_ret;
        bool first = true;
        for (auto & operand : operands)
            if (first) first = false;
            else
                to_ret.push_back(operand);

        return to_ret;
    }
    case operation::ret:
    case operation::store:
        return operands;
    default:
        std::cerr << "Unimplemented ir inputs helper" << *this << std::endl;
        return {};
    }
}
three_address * function::instruction_number(size_t pos) const {
    size_t block_num = 0;
    while (block_num < body.size() and pos >= this->body.at(block_num)->contents.size()) {
        pos -= this->body.at(block_num)->contents.size();
        block_num++;
    }

    if (block_num >= body.size()) return nullptr;
    else
        return &body.at(block_num)->contents.at(pos);
}
std::vector<ir::operand> function::parameters() const {
    std::vector<ir::operand> to_ret;
    to_ret.reserve(type->parameters.size());

    for (size_t i = 0; i < type->parameters.size(); i++) {
        auto param_name = param_names.at(i);
        auto param_type = this->type->parameters.at(i);
        to_ret.push_back({param_name, param_type, false});
    }

    return to_ret;
}
} // namespace ir