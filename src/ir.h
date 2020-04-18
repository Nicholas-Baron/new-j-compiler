//
// Created by nick on 4/17/20.
//

#ifndef NEW_J_COMPILER_IR_H
#define NEW_J_COMPILER_IR_H

#include <array>
#include <memory>
#include <set>
#include <string>
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

enum struct ir_type { boolean, str, i32, i64, f32, f64 };

struct operand {
    std::string name;
    ir_type type;
};

struct three_address {
    three_address(operation op, std::array<operand, 3> && operands) : op(op), operands(operands) {}
    operation op;
    std::array<operand, 3> operands;
};

struct basic_block {
    std::string name;
    std::vector<three_address> contents;
};

struct function {
    std::string name;
    std::vector<operand> parameters;
    std::vector<basic_block> body;
};

class program {
  public:
    [[nodiscard]] bool function_exists(const std::string & name, size_t param_count) const noexcept;
    [[nodiscard]] function * register_function(const std::string & name);

  private:
    std::set<std::unique_ptr<ir::function>> program;
};

} // namespace ir

#endif // NEW_J_COMPILER_IR_H
