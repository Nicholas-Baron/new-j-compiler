//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_VISITOR_H
#define NEW_J_COMPILER_VISITOR_H

#include "ast/node_forward.h"
#include "ast/token.h"
#include "ir/ir.h"

#include <map>

class visitor {
  public:
    constexpr visitor() noexcept = default;

    visitor(const visitor &) = delete;
    visitor & operator=(const visitor &) = delete;

    visitor(visitor &&) = default;
    visitor & operator=(visitor &&) = default;

    virtual ~visitor() noexcept = default;

    virtual void visit(const ast::node &) = 0;
};

class printing_visitor final : public visitor {
  public:
    explicit printing_visitor(int indent_size = 2) : indent_size(indent_size) {}

    void visit(const ast::node & node) override;

    [[nodiscard]] constexpr auto visited_count() const noexcept { return node_count; }

  private:
    void print_indent() const;

    int indent_size;
    int indent_depth = 0;
    long node_count = 0;
};

class ir_gen_visitor final : public visitor {
    using scope_t = std::map<std::string, ir::operand>;

  public:
    void visit(const ast::node &) override;

    std::optional<ir::operand> fold_to_constant(ast::expression &);

    void dump() const;

    [[nodiscard]] const ir::program & program() const { return prog; }

  private:
    void generate_function(const ast::function &);

    [[nodiscard]] std::shared_ptr<ir::type> type_from(const token &);

    [[nodiscard]] ir::operand eval_ast(const ast::expression &);
    void eval_if_condition(const ast::expression &, const std::string & true_branch,
                           const std::string & false_branch);
    [[nodiscard]] std::optional<ir::operand> read_variable(const std::string & name) const;

    [[nodiscard]] scope_t & global_scope() noexcept;
    [[nodiscard]] scope_t & current_scope() noexcept;
    [[nodiscard]] ir::basic_block * current_block();

    void append_instruction(ir::three_address && inst);
    void append_instruction(ir::operation op, std::vector<ir::operand> && operands = {}) {
        append_instruction({op, std::move(operands)});
    }
    ir::basic_block * append_block(std::string && name);

    [[nodiscard]] std::string temp_name();
    [[nodiscard]] ir::operand temp_operand(std::shared_ptr<ir::type> type, bool immediate) {
        return {temp_name(), std::move(type), immediate};
    }
    [[nodiscard]] std::string block_name();

    ir::program prog{};
    std::vector<scope_t> active_variables{};
    ir::function * current_func = nullptr;
    long block_num = 0;
    long temp_num = 0;
};

#endif // NEW_J_COMPILER_VISITOR_H
