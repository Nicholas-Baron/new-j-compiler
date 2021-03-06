//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODES_H
#define NEW_J_COMPILER_NODES_H

#include "node_forward.h"
#include "token.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ast {

struct node {
  public:
    constexpr node() noexcept = default;

    node(const node &) = delete;
    node & operator=(const node &) = delete;

    node(node &&) noexcept = default;
    node & operator=(node &&) noexcept = default;

    virtual ~node() noexcept = default;

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
struct expression : public virtual ast::node {
    [[nodiscard]] virtual bool has_children() const noexcept = 0;
};
struct statement : public virtual ast::node {};
struct top_level : public virtual ast::node {
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

    // The user has explicitly marked a type
    [[nodiscard]] bool user_explicit() const noexcept { return written_type.has_value(); }

    // The user has explicitly marked a type and it is a non-primitive type.
    [[nodiscard]] bool user_defined_type() const noexcept {
        return user_explicit() and written_type.value().type() == token_type::Identifier;
    }

    [[nodiscard]] token type_data() const { return written_type.value(); }

  private:
    token ident;
    std::optional<token> written_type;
};

// Top level items

struct struct_decl final : public ast::top_level {

    struct_decl(token && name, std::vector<std::pair<token, token>> && fields)
        : name{std::move(name)}, fields{std::move(fields)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type::struct_decl; }

    [[nodiscard]] size_t start_pos() const noexcept final { return name.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return fields.back().second.end(); }

    [[nodiscard]] token::source_t src() const noexcept final { return name.src(); }
    [[nodiscard]] std::string identifier() const final {
        return std::get<std::string>(name.get_data());
    }

    token name;
    std::vector<std::pair<token, token>> fields;
};

struct parameter final : public ast::node {
    parameter(token && name, token && type) : name{std::move(name)}, val_type{std::move(type)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::parameter; }

    [[nodiscard]] size_t start_pos() const noexcept final { return name.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return val_type.end(); }

    [[nodiscard]] token::source_t src() const noexcept final { return name.src(); }

    token name, val_type;
};

struct function final : public ast::top_level {
    function(token && name, std::vector<parameter> params, std::optional<token> && return_type,
             std::unique_ptr<ast::statement> body)
        : name{std::move(name), std::move(return_type)}, params{std::move(params)}, body{std::move(
                                                                                        body)} {}

    [[nodiscard]] std::string identifier() const final { return name.name(); }

    [[nodiscard]] node_type type() const noexcept final { return node_type ::function; }
    [[nodiscard]] size_t start_pos() const noexcept final { return name.start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return body->end_pos(); }
    [[nodiscard]] token::source_t src() const noexcept final { return name.src(); }

    opt_typed name;
    std::vector<ast::parameter> params;
    std::unique_ptr<ast::statement> body;
};

// Statements

struct stmt_block final : public ast::statement {
    explicit stmt_block(token && start) : start{std::move(start)} {}

    void append(std::unique_ptr<ast::statement> stmt) { stmts.push_back(std::move(stmt)); }
    void terminate(token && ender) { end = ender; }

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::statement_block; }
    [[nodiscard]] size_t start_pos() const noexcept final { return start.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return end.has_value() ? end->end()
                               : (stmts.empty() ? start.end() : stmts.back()->end_pos());
    }

    [[nodiscard]] token::source_t src() const noexcept final { return start.src(); }

    token start;
    std::vector<std::unique_ptr<ast::statement>> stmts;
    std::optional<token> end;
};

struct if_stmt final : public ast::statement {
    if_stmt(std::unique_ptr<expression> condition, std::unique_ptr<statement> then_block,
            std::unique_ptr<statement> else_block = nullptr)
        : cond{std::move(condition)}, then_block{std::move(then_block)}, else_block{std::move(
                                                                             else_block)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::if_statement; }
    [[nodiscard]] size_t start_pos() const noexcept final { return cond->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return (else_block == nullptr) ? then_block->end_pos() : else_block->end_pos();
    }
    [[nodiscard]] token::source_t src() const noexcept final { return cond->src(); }

    std::unique_ptr<ast::expression> cond;
    std::unique_ptr<ast::statement> then_block;
    std::unique_ptr<ast::statement> else_block;
};

struct while_loop final : public ast::statement {
    while_loop(std::unique_ptr<ast::expression> cond, std::unique_ptr<ast::statement> stmt)
        : condition{std::move(cond)}, body{std::move(stmt)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type::while_loop; }
    [[nodiscard]] size_t start_pos() const noexcept final { return condition->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return body->end_pos(); }
    [[nodiscard]] token::source_t src() const noexcept final { return condition->src(); }

    std::unique_ptr<ast::expression> condition;
    std::unique_ptr<ast::statement> body;
};

struct ret_stmt final : public ast::statement {
    explicit ret_stmt(token && ret, std::unique_ptr<expression> value = nullptr)
        : ret{std::move(ret)}, value{std::move(value)} {}

    [[nodiscard]] ast::node_type type() const noexcept final {
        return node_type ::return_statement;
    }
    [[nodiscard]] size_t start_pos() const noexcept final { return value->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return (value == nullptr) ? ret.end() : value->end_pos();
    }
    [[nodiscard]] token::source_t src() const noexcept final { return ret.src(); }

    token ret;
    std::unique_ptr<ast::expression> value;
};

struct func_call final : public ast::statement, public ast::expression {
    func_call(std::unique_ptr<expression> callee,
              std::vector<std::unique_ptr<ast::expression>> && args)
        : func_name{std::move(callee)}, arguments{std::move(args)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::func_call; }
    [[nodiscard]] size_t start_pos() const noexcept final { return func_name->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return arguments.empty() ? func_name->end_pos() + 2 : arguments.back()->end_pos() + 1;
    }

    [[nodiscard]] token::source_t src() const noexcept final { return func_name->src(); }

    [[nodiscard]] bool has_children() const noexcept final { return not arguments.empty(); }

    [[nodiscard]] ast::expression * name() const noexcept { return func_name.get(); }

    std::unique_ptr<ast::expression> func_name;
    std::vector<std::unique_ptr<ast::expression>> arguments;
};

struct var_decl final : public ast::top_level, public ast::statement {
  public:
    enum class details { Let, Const, GlobalConst };
    var_decl(opt_typed && ident, std::unique_ptr<expression> expr, details detail)
        : name{std::move(ident)}, val{std::move(expr)}, detail{detail} {}

    [[nodiscard]] std::string identifier() const final { return name.name(); }

    [[nodiscard]] node_type type() const noexcept final { return node_type ::var_decl; }
    [[nodiscard]] size_t start_pos() const noexcept final { return name.start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return val->end_pos(); }
    [[nodiscard]] token::source_t src() const noexcept final { return name.src(); }

    [[nodiscard]] bool in_global_scope() const noexcept { return detail == details::GlobalConst; }

    [[nodiscard]] expression & value_expr() const noexcept { return *val; }

    opt_typed name;
    std::unique_ptr<expression> val;
    details detail;
};

struct assign_stmt final : public ast::statement {

    assign_stmt(std::unique_ptr<ast::expression> dest, std::unique_ptr<ast::expression> value_src,
                ast::operation oper = ast::operation::assign)
        : dest{std::move(dest)}, value_src{std::move(value_src)}, assign_op{oper} {}

    [[nodiscard]] node_type type() const noexcept final { return node_type ::assign_statement; }
    [[nodiscard]] size_t start_pos() const noexcept final { return dest->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return value_src->end_pos(); }
    [[nodiscard]] token::source_t src() const noexcept final { return dest->src(); }

    std::unique_ptr<ast::expression> dest;
    std::unique_ptr<ast::expression> value_src;
    // If the source code use the 'op=' form, assign_op is set to just 'op'
    ast::operation assign_op;
};

// Expressions
struct literal_or_variable final : public ast::expression {
    explicit literal_or_variable(token && val) : val{std::move(val)} {}

    literal_or_variable(const literal_or_variable & src) : val{src.val} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::value; }
    [[nodiscard]] size_t start_pos() const noexcept final { return val.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return val.end(); }

    [[nodiscard]] bool has_children() const noexcept final { return false; }

    [[nodiscard]] token::source_t src() const noexcept final { return val.src(); }

    [[nodiscard]] token::token_data data() const { return val.get_data(); }

    token val;

  private:
    friend std::ostream & operator<<(std::ostream & lhs, const literal_or_variable & rhs) {
        return lhs << rhs.val;
    }
};

struct bin_op final : public ast::expression {

    bin_op(std::unique_ptr<expression> lhs, operation op, std::unique_ptr<expression> rhs)
        : lhs{std::move(lhs)}, op{op}, rhs{std::move(rhs)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::binary_op; }
    [[nodiscard]] size_t start_pos() const noexcept final { return lhs->start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return rhs->end_pos(); }

    [[nodiscard]] bool has_children() const noexcept final { return true; }

    [[nodiscard]] token::source_t src() const noexcept final { return lhs->src(); }

    [[nodiscard]] operation oper() const noexcept { return op; }

    [[nodiscard]] expression & lhs_ref() const noexcept { return *lhs; }
    [[nodiscard]] expression & rhs_ref() const noexcept { return *rhs; }

    std::unique_ptr<expression> lhs;
    operation op;
    std::unique_ptr<expression> rhs;
};

} // namespace ast

#endif // NEW_J_COMPILER_NODES_H
