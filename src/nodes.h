//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODES_H
#define NEW_J_COMPILER_NODES_H

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "node_forward.h"
#include "token.h"
#include "visitor.h"

namespace ast {

class program {
  public:
    bool add_item(std::unique_ptr<top_level> item);
    [[nodiscard]] top_level * find(const std::string & id) const;

    void visit(const std::function<void(top_level &)> & visitor) {
        for (auto & item : items)
            visitor(*item);
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
    [[nodiscard]] virtual std::string text() const noexcept = 0;
};

// Intermediate classes
class expression : public ast::node {};
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
    [[nodiscard]] auto src() const { return ident.src(); }

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::opt_typed; }

    [[nodiscard]] size_t start_pos() const noexcept final { return ident.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return written_type.has_value() ? written_type.value().end() : ident.end();
    }
    [[nodiscard]] std::string text() const noexcept final {
        return ident.src()->substr(start_pos(), end_pos() - start_pos());
    }

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
    [[nodiscard]] std::string text() const noexcept final {
        return name.src()->substr(start_pos(), end_pos() - start_pos());
    }

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
        for (auto & param : params)
            v.visit(param);
        v.visit(*body);
    }

    [[nodiscard]] std::string identifier() const final { return name.name(); }

    [[nodiscard]] node_type type() const noexcept final { return node_type ::function; }
    [[nodiscard]] size_t start_pos() const noexcept final { return name.start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return body->end_pos(); }
    [[nodiscard]] std::string text() const noexcept final {
        return name.src()->substr(start_pos(), end_pos() - start_pos());
    }

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
        for (auto & stmt : stmts)
            v.visit(*stmt);
    }

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::statement_block; }
    [[nodiscard]] size_t start_pos() const noexcept final { return start.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return end.has_value() ? end->end()
                               : (stmts.empty() ? start.end() : stmts.back()->end_pos());
    }
    [[nodiscard]] std::string text() const noexcept final {
        return start.src()->substr(start_pos(), end_pos() - start_pos());
    }

  private:
    token start;
    std::vector<std::unique_ptr<ast::statement>> stmts;
    std::optional<token> end;
};

class func_call final : public ast::statement, public ast::expression {
  public:
    func_call(token && callee, std::vector<std::unique_ptr<ast::expression>> && args)
        : func_name{std::move(callee)}, arguments{std::move(args)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::func_call; }
    [[nodiscard]] size_t start_pos() const noexcept final { return func_name.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final {
        return arguments.empty() ? func_name.end() + 2 : arguments.back()->end_pos() + 1;
    }
    [[nodiscard]] std::string text() const noexcept final {
        return func_name.src()->substr(start_pos(), end_pos() - start_pos());
    }

  private:
    token func_name;
    std::vector<std::unique_ptr<ast::expression>> arguments;
};

class const_decl final : public ast::top_level, public ast::statement {
  public:
    const_decl(opt_typed && ident, std::unique_ptr<expression> expr, bool is_global)
        : name{std::move(ident)}, val{std::move(expr)}, global{is_global} {}

    void accept(visitor & v) final {
        if (global)
            v.visit(*static_cast<ast::top_level *>(this));
        else
            v.visit(*static_cast<ast::statement *>(this));

        v.visit(*val);
    }

    [[nodiscard]] std::string identifier() const final { return name.name(); }

    [[nodiscard]] node_type type() const noexcept final { return node_type ::const_decl; }
    [[nodiscard]] size_t start_pos() const noexcept final { return name.start_pos(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return val->end_pos(); }
    [[nodiscard]] std::string text() const noexcept final {
        return name.src()->substr(start_pos(), end_pos() - start_pos());
    }

  private:
    opt_typed name;
    std::unique_ptr<expression> val;
    bool global;
};

// Expressions
class value final : public ast::expression {
  public:
    explicit value(token && val) : val{std::move(val)} {}

    [[nodiscard]] ast::node_type type() const noexcept final { return node_type ::value; }
    [[nodiscard]] size_t start_pos() const noexcept final { return val.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return val.end(); }
    [[nodiscard]] std::string text() const noexcept final {
        return val.src()->substr(start_pos(), end_pos() - start_pos());
    }

  private:
    token val;
};

} // namespace ast

#endif // NEW_J_COMPILER_NODES_H
