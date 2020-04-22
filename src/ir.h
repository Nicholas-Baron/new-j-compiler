//
// Created by nick on 4/17/20.
//

#ifndef NEW_J_COMPILER_IR_H
#define NEW_J_COMPILER_IR_H

#include <array>
#include <functional>
#include <iosfwd>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace ir {

enum struct operation {
    add,
    sub,
    mul,
    div,
    shift_left,
    shift_right,
    bit_or,
    bit_and,
    bool_or,
    bool_and,
    compare_equal,
    compare,
    assign,
    halt,
    branch,
    call,
    ret,
    load,
    store
};

enum struct ir_type { unit, boolean, str, i32, i64, f32, f64, func };

struct operand {
    std::variant<std::monostate, bool, long, double, std::string> data;
    ir_type type;
    bool is_immediate;

  private:
    friend std::ostream & operator<<(std::ostream & lhs, const operand & rhs);
};

struct three_address {
    operation op;
    std::vector<operand> operands;
    [[nodiscard]] std::optional<operand> result() const {
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
        case operation::store:
            return std::optional{operands.front()};
        default:
            return {};
        }
    }
};

struct basic_block {
    explicit basic_block(std::string name, std::vector<three_address> inst = {})
        : name{std::move(name)}, contents{std::move(inst)} {}
    std::string name;
    std::vector<three_address> contents;
    bool terminated();
};

struct function {
    std::string name;
    std::vector<operand> parameters;
    std::vector<std::unique_ptr<basic_block>> body;
    ir_type return_type{ir_type::unit};
};

class program {
  public:
    [[nodiscard]] bool function_exists(const std::string & name, size_t param_count) const noexcept;
    [[nodiscard]] function * lookup_function(const std::string & name, size_t param_count) const
        noexcept;
    [[nodiscard]] function * register_function(const std::string & name);

    void for_each_func(const std::function<void(ir::function *)> & visitor) const {
        for (const auto & func : program) visitor(func.get());
    }

  private:
    std::set<std::unique_ptr<ir::function>> program;
};

} // namespace ir

#endif // NEW_J_COMPILER_IR_H
