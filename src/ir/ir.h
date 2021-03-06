//
// Created by nick on 4/17/20.
//

#ifndef NEW_J_COMPILER_IR_H
#define NEW_J_COMPILER_IR_H

#include "ir_type.h"

#include <functional>
#include <iosfwd>
#include <map>
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

struct operand {
    std::variant<std::monostate, bool, long, double, std::string> data;
    std::shared_ptr<ir::type> type;
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
    explicit function(std::string name, std::shared_ptr<ir::function_type> type)
        : name{std::move(name)}, type{std::move(type)} {}

    [[nodiscard]] three_address * instruction_number(size_t) const;

    [[nodiscard]] std::vector<ir::operand> parameters() const;

    std::string name;
    std::vector<std::unique_ptr<basic_block>> body{};
    std::vector<std::string> param_names;
    std::shared_ptr<ir::function_type> type;
};

class program {
  public:
    explicit program();

    program(const program &) = delete;
    program & operator=(const program &) = delete;

    program(program &&) noexcept = default;
    program & operator=(program &&) noexcept = default;

    ~program() noexcept = default;

    [[nodiscard]] bool function_exists(const std::string & name) const noexcept;
    [[nodiscard]] bool function_exists(const std::string & name, size_t param_count) const noexcept;
    [[nodiscard]] bool function_exists(const std::string & name,
                                       const ir::function_type & func_type) const noexcept;

    [[nodiscard]] function * lookup_function(const std::string & name) const noexcept;
    [[nodiscard]] function * lookup_function(const std::string & name,
                                             size_t param_count) const noexcept;
    [[nodiscard]] function * lookup_function(const std::string & name,
                                             const ir::function_type & func_type) const noexcept;

    [[nodiscard]] function * register_function(const std::string & name,
                                               std::shared_ptr<ir::function_type> func_type);

    // This function first looks up the type where the name is equal to the given name.
    // Next, if the first lookup failed, it finds the type of the function with the given name.
    [[nodiscard]] std::shared_ptr<ir::type> lookup_type(const std::string & name);

    void for_each_func(const std::function<void(ir::function *)> & visitor) const {
        for (const auto & func : prog) visitor(func.get());
    }

    [[nodiscard]] std::shared_ptr<ir::type>
    generate_func_type(const std::vector<std::string> param_types, std::string return_type);

  private:
    std::vector<std::unique_ptr<ir::function>> prog;
    std::map<std::string, std::shared_ptr<ir::type>> types;
};

} // namespace ir

#endif // NEW_J_COMPILER_IR_H
