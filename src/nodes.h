//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODES_H
#define NEW_J_COMPILER_NODES_H

#include "node_forward.h"
#include "token.h"
#include "visitor.h"

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ast {

class program final {
  public:
    bool add_item(std::unique_ptr<top_level> item);
    [[nodiscard]] top_level * find(const std::string & id) const;

    void visit(const std::function<void(top_level &)> & visitor) {
        for (auto & item : items) visitor(*item);
    }

  private:
    std::vector<std::unique_ptr<top_level>> items{};
};

class node {
  public:
    constexpr node() noexcept = default;

    node(const node &) = delete;
    node & operator=(const node &) = delete;

    node(node &&) noexcept = default;
    node & operator=(node &&) noexcept = default;

    virtual ~node() noexcept = default;

    virtual void accept(visitor &) {}
    [[nodiscard]] virtual ast::node_type type() const noexcept = 0;

    // Functions to help with debug printing
    [[nodiscard]] virtual size_t start_pos() const noexcept = 0;
    [[nodiscard]] virtual size_t end_pos() const noexcept = 0;
    [[nodiscard]] std::string text() const noexcept {
        return src()->substr(start_pos(), end_pos() - start_pos());
    }

    // TODO: Maybe better to use a std::string *
    // , as this function does not modify ownership
    [[nodiscard]] virtual token::source_t src() const noexcept = 0;
};

// Intermediate classes
class expression : public ast::node {
  public:
    [[nodiscard]] virtual bool has_children() const noexcept = 0;
};
class statement : public ast::node {};
class top_level : public ast::node {
  public:
    [[nodiscard]] virtual std::string identifier() const = 0;
};

class opt_typed final : public ast::node {
  public:
    explicit opt_typed(token && identifier, std::optional<token> && type = {})
        : ident{std::move(identifier)}, written_type{std::move(type)} {}

    [[nodiscard]] std::string name() const { return std::get<std::string>(ident.get_data()); }
    [[nodiscard]] token::source_t src() const noexcept final { return ident.src(); }

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::opt_typed; }

    [[nodiscard]] size_t start_pos() const noexcept final { return ident.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return written_type.has_value() ? written_type.value().end() : ident.end();
    }

    [[nodiscard]] auto user_typed() const noexcept { return written_type.has_value(); }

  private:
    token ident;
    std::optional<token> written_type;
};

// Top level items

class parameter final : public ast::node {
  public:
    parameter(token && name, token && type) : name{std::move(name)}, val_type{std::move(type)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::parameter; }

    [[nodiscard]] size_t start_pos() const noexcept final { return name.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return val_type.end(); }

    [[nodiscard]] token::source_t src() const noexcept final { return name.src(); }

  private:
    token name, val_type;
};

class function final : public ast::top_level {
  public:
    function(token && name, std::vector<parameter> params, std::optional<token> && return_type,
             std::unique_ptr<ast::statement> body)
        : name{std::move(name), std::move(return_type)}, params{std::move(params)}, body{std::move(
                                                                                        body)} {}

    void accept(visitor & v) final {
        v.visit(name);
        for (auto & param : params) v.visit(param);
        v.visit(*body);
    }

    [[nodiscard]] std::string identifier() const final { return name.name(); }

    [[nodiscard]] node_type type() const noexcept final { return node_type ::function; }
    [[nodiscard]] size_t start_pos() const noexcept final { return name.start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return body->end_pos(); }
    [[nodiscard]] token::source_t src() const noexcept final { return name.src(); }

  private:
    opt_typed name;
    std::vector<ast::parameter> params;
    std::unique_ptr<ast::statement> body;
};

// Statements

class stmt_block final : public ast::statement {
  public:
    explicit stmt_block(token && start) : start{std::move(start)} {}

    void append(std::unique_ptr<ast::statement> stmt) { stmts.push_back(std::move(stmt)); }
    void terminate(token && ender) { end = ender; }

    void accept(visitor & v) final {
        for (auto & stmt : stmts) v.visit(*stmt);
    }

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::statement_block; }
    [[nodiscard]] size_t start_pos() const noexcept final { return start.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return end.has_value() ? end->end()
                               : (stmts.empty() ? start.end() : stmts.back()->end_pos());
    }

    [[nodiscard]] token::source_t src() const noexcept final { return start.src(); }

  private:
    token start;
    std::vector<std::unique_ptr<ast::statement>> stmts;
    std::optional<token> end;
};

class if_stmt final : public ast::statement {
  public:
    if_stmt(std::unique_ptr<expression> condition, std::unique_ptr<statement> then_block,
            std::unique_ptr<statement> else_block = nullptr)
        : cond{std::move(condition)}, then_block{std::move(then_block)}, else_block{std::move(
                                                                             else_block)} {}

    void accept(visitor & v) override {
        v.visit(*cond);
        v.visit(*then_block);
        if (else_block != nullptr) v.visit(*else_block);
    }

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::if_statement; }
    [[nodiscard]] size_t start_pos() const noexcept final { return cond->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return (else_block == nullptr) ? then_block->end_pos() : else_block->end_pos();
    }
    [[nodiscard]] token::source_t src() const noexcept final { return cond->src(); }

  private:
    std::unique_ptr<ast::expression> cond;
    std::unique_ptr<ast::statement> then_block;
    std::unique_ptr<ast::statement> else_block;
};

class ret_stmt final : public ast::statement {
  public:
    explicit ret_stmt(token && ret, std::unique_ptr<expression> value = nullptr)
        : ret{std::move(ret)}, value{std::move(value)} {}

    void accept(visitor & v) final {
        if (value != nullptr) v.visit(*value);
    }

    [[nodiscard]] ast::node_type type() const noexcept final {
        return node_type ::return_statement;
    }
    [[nodiscard]] size_t start_pos() const noexcept final { return value->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return (value == nullptr) ? ret.end() : value->end_pos();
    }
    [[nodiscard]] token::source_t src() const noexcept final { return ret.src(); }

  private:
    token ret;
    std::unique_ptr<ast::expression> value;
};

class func_call final : public ast::statement, public ast::expression {
  public:
    func_call(std::unique_ptr<expression> callee,
              std::vector<std::unique_ptr<ast::expression>> && args)
        : func_name{std::move(callee)}, arguments{std::move(args)} {}

    void accept(visitor & v) final {
        v.visit(*func_name);
        for (auto & arg : arguments) v.visit(*arg);
    }

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::func_call; }
    [[nodiscard]] size_t start_pos() const noexcept final { return func_name->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return arguments.empty() ? func_name->end_pos() + 2 : arguments.back()->end_pos() + 1;
    }

    [[nodiscard]] token::source_t src() const noexcept final { return func_name->src(); }

    [[nodiscard]] bool has_children() const noexcept final { return not arguments.empty(); }

  private:
    std::unique_ptr<ast::expression> func_name;
    std::vector<std::unique_ptr<ast::expression>> arguments;
};

class const_decl final : public ast::top_level, public ast::statement {
  public:
    const_decl(opt_typed && ident, std::unique_ptr<expression> expr, bool is_global)
        : name{std::move(ident)}, val{std::move(expr)}, global{is_global} {}

    void accept(visitor & v) final { v.visit(*val); }

    [[nodiscard]] std::string identifier() const final { return name.name(); }

    [[nodiscard]] node_type type() const noexcept final { return node_type ::const_decl; }
    [[nodiscard]] size_t start_pos() const noexcept final { return name.start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return val->end_pos(); }
    [[nodiscard]] token::source_t src() const noexcept final { return name.src(); }

    [[nodiscard]] bool in_global_scope() const noexcept { return global; }

    [[nodiscard]] expression * value_expr() const noexcept { return val.get(); }

  private:
    opt_typed name;
    std::unique_ptr<expression> val;
    bool global;
};

// Expressions
class literal_or_variable final : public ast::expression {
  public:
    explicit literal_or_variable(token && val) : val{std::move(val)} {}

    literal_or_variable(const literal_or_variable & src) : val{src.val} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::value; }
    [[nodiscard]] size_t start_pos() const noexcept final { return val.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return val.end(); }

    [[nodiscard]] bool has_children() const noexcept final { return false; }

    [[nodiscard]] token::source_t src() const noexcept final { return val.src(); }

    [[nodiscard]] token::token_data data() const { return val.get_data(); }

  private:
    token val;
    friend std::ostream & operator<<(std::ostream & lhs, const literal_or_variable & rhs) {
        return lhs << rhs.val;
    }
};

class bin_op final : public ast::expression {
  public:
    enum class operation { add, sub, mult, div, boolean_and, boolean_or, le, lt, gt, ge, eq };

    bin_op(std::unique_ptr<expression> lhs, operation op, std::unique_ptr<expression> rhs)
        : lhs{std::move(lhs)}, op{op}, rhs{std::move(rhs)} {}

    void accept(visitor & v) final {
        v.visit(*lhs);
        v.visit(*rhs);
    }

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::binary_op; }
    [[nodiscard]] size_t start_pos() const noexcept final { return lhs->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return rhs->end_pos(); }

    [[nodiscard]] bool has_children() const noexcept final { return true; }

    [[nodiscard]] token::source_t src() const noexcept final { return lhs->src(); }

    [[nodiscard]] operation oper() const noexcept { return op; }

  private:
    std::unique_ptr<expression> lhs;
    operation op;
    std::unique_ptr<expression> rhs;
};

} // namespace ast

#endif // NEW_J_COMPILER_NODES_H
