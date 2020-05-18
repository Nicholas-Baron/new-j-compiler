//
// Created by nick on 4/17/20.
//

#ifndef NEW_J_COMPILER_IR_H
#define NEW_J_COMPILER_IR_H

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ir {

enum struct operation {
    add,
    assign,
    bit_and,
    bit_or,
    bool_and,
    bool_or,
    branch, // format: condition true_case false_case
    call,
    div,
    eq,
    ge,
    gt,
    halt,
    le,
    load,
    lt,
    mul,
    ne,
    phi,
    ret,
    shift_left,
    shift_right,
    store,
    sub,
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
    [[nodiscard]] std::optional<operand> result() const;
    [[nodiscard]] std::vector<operand> inputs() const;

  private:
    friend std::ostream & operator<<(std::ostream & lhs, const three_address & rhs);
};

struct basic_block {
    explicit basic_block(std::string name, std::vector<three_address> inst = {})
        : name{std::move(name)}, contents{std::move(inst)} {}
    std::string name;
    std::vector<three_address> contents;
    bool terminated();
};

struct function {
    explicit function(std::string name) : name{std::move(name)} {}

    three_address * instruction_number(size_t) const;

    std::string name;
    std::vector<operand> parameters;
    std::vector<std::unique_ptr<basic_block>> body;
    ir_type return_type{ir_type::unit};
};

class program {
  public:
    explicit program() = default;

    program(const program &) = delete;
    program & operator=(const program &) = delete;

    program(program &&) noexcept = default;
    program & operator=(program &&) noexcept = default;

    ~program() noexcept = default;

    [[nodiscard]] bool function_exists(const std::string & name) const noexcept;
    [[nodiscard]] bool function_exists(const std::string & name, size_t param_count) const noexcept;
    [[nodiscard]] function * lookup_function(const std::string & name) const noexcept;
    [[nodiscard]] function * lookup_function(const std::string & name,
                                             size_t param_count) const noexcept;
    [[nodiscard]] function * register_function(const std::string & name, size_t param_count);

    void for_each_func(const std::function<void(ir::function *)> & visitor) const {
        for (const auto & func : prog) visitor(func.get());
    }

  private:
    std::vector<std::unique_ptr<ir::function>> prog;
};

} // namespace ir

#endif // NEW_J_COMPILER_IR_H
