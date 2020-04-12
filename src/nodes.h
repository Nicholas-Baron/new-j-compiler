//
// Created by nick on 4/10/20.
//

#ifndef NEW_J_COMPILER_NODES_H
#define NEW_J_COMPILER_NODES_H

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

  private:
    std::vector<std::unique_ptr<top_level>> items{};
};

class node {
  public:
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
    [[nodiscard]] virtual const std::string & identifier() const = 0;
};

// Top level items

class parameter final : public ast::node {
  public:
    parameter(token && name, token && type) : name{std::move(name)}, val_type{std::move(type)} {}

    [[nodiscard]] virtual ast::node_type type() const noexcept { return node_type ::parameter; }

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
        : name{std::move(name)}, ret_type{std::move(return_type)}, params{std::move(params)},
          body{std::move(body)} {}

    void accept(visitor & v) final {
        v.visit(*this);
        for (auto & param : params)
            v.visit(param);
        v.visit(*body);
    }

    [[nodiscard]] const std::string & identifier() const final {
        return std::get<std::string>(name.get_data());
    }

    [[nodiscard]] node_type type() const noexcept final { return node_type ::function; }
    [[nodiscard]] size_t start_pos() const noexcept final { return name.start(); }
    [[nodiscard]] size_t end_pos() const noexcept final { return body->end_pos(); }
    [[nodiscard]] std::string text() const noexcept final {
        return name.src()->substr(start_pos(), end_pos() - start_pos());
    }

  private:
    token name;
    std::optional<token> ret_type;
    std::vector<ast::parameter> params;
    std::unique_ptr<ast::statement> body;
};

} // namespace ast

#endif // NEW_J_COMPILER_NODES_H
