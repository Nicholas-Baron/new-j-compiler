//
// Created by nick on 7/18/20.
//

#ifndef NEW_J_COMPILER_IR_TYPE_H
#define NEW_J_COMPILER_IR_TYPE_H

#include <memory>
#include <string>
#include <vector>

namespace ir {

enum struct ir_type { unit, boolean, str, i32, i64, f32, f64, func, struct_ };

struct type {
    constexpr type() = default;

    type(type &) = delete;
    type & operator=(type &) = delete;

    type(type &&) noexcept = default;
    type & operator=(type &&) noexcept = default;

    virtual ~type() noexcept = default;

    [[nodiscard]] virtual explicit operator ir_type() const noexcept = 0;
};

template <ir_type T> struct primitive_type final : public type {
    [[nodiscard]] explicit operator ir_type() const noexcept final { return T; }
};

using unit_type = primitive_type<ir_type::unit>;
using i32_type = primitive_type<ir_type::i32>;
using i64_type = primitive_type<ir_type::i64>;
using f32_type = primitive_type<ir_type::f32>;
using f64_type = primitive_type<ir_type::f64>;
using boolean_type = primitive_type<ir_type::boolean>;

struct string_type final : public type {
    [[nodiscard]] explicit operator ir_type() const noexcept final { return ir_type::str; }
};

struct function_type final : public type {

    function_type(std::vector<type> && params, std::unique_ptr<type> return_type)
        : parameters{std::move(params)}, return_type{std::move(return_type)} {}

    [[nodiscard]] explicit operator ir_type() const noexcept final { return ir_type::func; }

    std::vector<type> parameters;
    std::unique_ptr<type> return_type;
};

struct struct_type final : public type {

    [[nodiscard]] explicit operator ir_type() const noexcept final { return ir_type::struct_; }

    struct field {
        std::string name;
        std::unique_ptr<type> type;
        uint64_t offset;
    };

    std::vector<field> fields;
};

} // namespace ir

#endif // NEW_J_COMPILER_IR_TYPE_H
