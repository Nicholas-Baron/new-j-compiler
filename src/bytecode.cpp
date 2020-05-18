//
// Created by nick on 5/15/20.
//

#include "bytecode.h"

#include <iostream>
#include <set>

namespace bytecode {
std::optional<program> program::from_ir(const ir::program & input) {

    auto * main_func = input.lookup_function("main");
    if (main_func == nullptr) return {};

    bytecode::program output{};

    input.for_each_func([&output, main_func](auto * func) {
        if (func == nullptr) return;

        if (func == main_func)
            output.main_offset = static_cast<uint32_t>(output.text_end - pc_start);

        output.generate_bytecode(*func);
    });

    return output;
}

operation::reg_with_imm make_reg_with_imm(uint8_t dest, uint8_t src, uint32_t imm) {
    return {{dest, src}, imm};
}

constexpr auto mask_low_32_bit = 0xFFFFFFFFul;

std::pair<operation, operation> load_64_bits(uint8_t dest, uint64_t val) {
    auto first = operation{opcode::lui, make_reg_with_imm(dest, 0, val >> 32u)};
    auto second = operation{opcode::ori, make_reg_with_imm(dest, dest, val & mask_low_32_bit)};
    return std::make_pair(first, second);
}

void program::generate_bytecode(const ir::function & function) {

    labels.emplace(function.name, text_end);

    constexpr uint8_t temp_start = 20;
    constexpr uint8_t temp_end = 61;
    std::map<std::string, register_info> register_alloc;
    auto register_for_operand = [&register_alloc](const ir::operand & operand) -> register_info & {
        return register_alloc.at(std::get<std::string>(operand.data));
    };
    auto allocate_register
        = [&register_alloc](const ir::operand & operand, size_t instruction) -> void {
        std::set<uint8_t> used_registers;
        for (const auto & entry : register_alloc) used_registers.insert(entry.second.reg_num);

        uint8_t last_reg = temp_start;
        // Should loop in a sorted order
        for (const auto & reg : used_registers)
            if (reg == last_reg) last_reg++;
            else
                break;

        if (last_reg >= temp_end) { std::cerr << "Too many temporaries" << std::endl; }

        register_alloc.insert_or_assign(std::get<std::string>(operand.data),
                                        register_info{last_reg, instruction});
    };

    // Preallocate registers
    auto ir_inst_num = 0u;
    for (auto & block : function.body) {
        for (const auto & inst : block->contents) {
            if (auto res = inst.result(); inst.op == ir::operation::phi) {
                uint8_t phi_register = UINT8_MAX;
                for (const auto & op : inst.inputs())
                    phi_register = std::min(phi_register, register_for_operand(op).reg_num);

                for (auto & operand : inst.operands)
                    register_for_operand(operand).reg_num = phi_register;

            } else if (res.has_value())
                allocate_register(res.value(), ir_inst_num++);
        }
    }

    // Parameters start at 13 and end at 19
    constexpr auto max_inputs = (19 - 13) + 1;
    if (uint8_t param_num = 13; function.parameters.size() <= max_inputs)
        for (auto & param : function.parameters)
            register_alloc.insert_or_assign(std::get<std::string>(param.data),
                                            register_info{param_num++, 0});
    else {
        // Too many parameters were declared
        std::cerr << "Function " << function.name << " has more than " << max_inputs
                  << " parameters.\n"
                     "Currently, "
                  << function.parameters.size() << " parameter functions are not supported.\n";
        labels.erase(function.name);
        return;
    }

    ir_inst_num = 0;
    for (auto & block : function.body) {
        labels.emplace(block->name, text_end);
        for (auto & instruction : block->contents) {
            this->bytecode.push_back(make_instruction(instruction, register_alloc, ir_inst_num++));
            text_end += 8;
        }
    }
}
uint64_t program::append_data(const std::string & item) {
    auto to_ret = this->data.size();
    for (char c : item) data.push_back(c);
    data.push_back('\0');
    return to_ret;
}
operation program::make_instruction(const ir::three_address & instruction,
                                    std::map<std::string, register_info> & register_alloc,
                                    size_t inst_num) {
    operation next_op;

    // TODO: record the last written times
    const auto get_register_info = [&register_alloc](const std::string & name) -> register_info & {
        return register_alloc.at(name);
    };

    auto res = instruction.result();

    switch (instruction.op) {
    case ir::operation::add: {
        auto lhs = instruction.operands.at(1);
        auto rhs = instruction.operands.at(2);
        auto result_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
        if (lhs.is_immediate and rhs.is_immediate) {
            auto & lhs_value = std::get<long>(lhs.data);
            auto & rhs_value = std::get<long>(rhs.data);
            if (lhs_value >= INT64_MAX - rhs_value) {
                std::cerr << "Detected integer overflow of " << lhs << " + " << rhs << '\n';
            }
            if (lhs_value + rhs_value <= INT32_MAX) {
                // Just ori
                next_op.code = opcode::ori;
                next_op.data = make_reg_with_imm(result_reg, 0,
                                                 static_cast<uint32_t>(lhs_value + rhs_value));
            } else {
                // Lui + ori
                std::cerr << "Lui + ori combo for loading 64 bit is not implemented.\n";
            }
        } else if (lhs.is_immediate and not rhs.is_immediate) {
            // ori lhs + add rhs
            bytecode.push_back(
                {opcode::ori, make_reg_with_imm(result_reg, 0,
                                                static_cast<uint32_t>(std::get<long>(lhs.data)))});
            text_end += 8;

            next_op.code = opcode::add;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            next_op.data = std::array{result_reg, result_reg, rhs_reg};
        } else if (rhs.is_immediate and not lhs.is_immediate) {
            // ori rhs + add lhs
            bytecode.push_back(
                {opcode::ori, make_reg_with_imm(result_reg, 0,
                                                static_cast<uint32_t>(std::get<long>(rhs.data)))});
            text_end += 8;

            next_op.code = opcode::add;
            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            next_op.data = std::array{result_reg, result_reg, lhs_reg};
        } else {
            // both are not immediates
            // simple add

            next_op.code = opcode::add;

            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            next_op.data = std::array{result_reg, lhs_reg, rhs_reg};
        }

    } break;
    case ir::operation::assign: {
        auto result_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
        auto src = instruction.operands.back();
        if (src.is_immediate) {
            switch (src.type) {
            case ir::ir_type::str: {
                auto [first, second]
                    = load_64_bits(result_reg, append_data(std::get<std::string>(src.data)));
                bytecode.push_back(first);
                text_end += 8;
                next_op = second;
            } break;
            case ir::ir_type::i32:
                next_op.code = opcode::ori;
                next_op.data = make_reg_with_imm(result_reg, 0,
                                                 static_cast<uint32_t>(std::get<long>(src.data)));
                break;
            case ir::ir_type::i64: {
                auto [first, second] = load_64_bits(result_reg, std::get<long>(src.data));
                bytecode.push_back(first);
                text_end += 8;
                next_op = second;
            } break;
            default:
                std::cerr << "Cannot use " << src << " as the rhs of an assignment.\n";
                break;
            }
        }
    } break;
    case ir::operation::bit_or: {
        auto lhs = instruction.operands.at(1);
        auto rhs = instruction.operands.at(2);
        auto result_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
        if (lhs.is_immediate and rhs.is_immediate) {
            uint64_t val = std::get<long>(lhs.data) | std::get<long>(rhs.data);
            if (val >= UINT32_MAX) {

                bytecode.push_back({opcode::lui, make_reg_with_imm(result_reg, 0, val >> 32u)});
                text_end += 8;
                val &= mask_low_32_bit;
            }

            next_op.code = opcode::ori;
            next_op.data = make_reg_with_imm(result_reg, result_reg, val);

        } else if (rhs.is_immediate and not lhs.is_immediate) {
            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            next_op.code = opcode::ori;
            next_op.data = make_reg_with_imm(result_reg, lhs_reg, std::get<long>(rhs.data));
        } else if (lhs.is_immediate and not rhs.is_immediate) {
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            next_op.code = opcode::ori;
            next_op.data = make_reg_with_imm(result_reg, rhs_reg, std::get<long>(lhs.data));
        } else {
            next_op.code = opcode::or_;
            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            next_op.data = std::array{result_reg, lhs_reg, rhs_reg};
        }
    } break;

    case ir::operation::halt:
        next_op.code = opcode::syscall;
        next_op.data = make_reg_with_imm(
            get_register_info(std::get<std::string>(res.value().data)).reg_num, 0, 0);
        break;

    case ir::operation::ret:
        next_op.code = opcode::jr;
        next_op.data = std::array<uint8_t, 3>{63, 0, 0};
        break;
    case ir::operation::shift_left: {
        auto lhs = instruction.operands.at(1);
        if (lhs.is_immediate) {
            std::cerr << "Cannot use " << lhs << " as the lhs of <<.\n";
            break;
        }

        auto rhs = instruction.operands.at(2);
        auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
        auto result_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
        if (rhs.is_immediate) {
            // sli
            next_op.code = opcode::sli;
            auto imm = std::get<long>(rhs.data);
            if (imm >= UINT32_MAX) {
                std::cerr << "Cannot put " << rhs << " in the imm field.\n";
                break;
            }
            next_op.data = make_reg_with_imm(result_reg, lhs_reg, imm);
        } else {
            // sl
            next_op.code = opcode::sl;
            next_op.data = std::array{result_reg, lhs_reg,
                                      get_register_info(std::get<std::string>(rhs.data)).reg_num};
        }
    } break;
    case ir::operation::shift_right: {
        auto lhs = instruction.operands.at(1);
        if (lhs.is_immediate) {
            std::cerr << "Cannot use " << lhs << " as the lhs of <<.\n";
            break;
        }

        auto rhs = instruction.operands.at(2);
        auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
        auto result_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
        if (rhs.is_immediate) {
            // sli
            next_op.code = opcode::sri;
            auto imm = std::get<long>(rhs.data);
            if (imm >= UINT32_MAX) {
                std::cerr << "Cannot put " << rhs << " in the imm field.\n";
                break;
            }
            next_op.data = make_reg_with_imm(result_reg, lhs_reg, imm);
        } else {
            // sl
            next_op.code = opcode::sr;
            next_op.data = std::array{result_reg, lhs_reg,
                                      get_register_info(std::get<std::string>(rhs.data)).reg_num};
        }
    } break;
    case ir::operation::call:
        if (res.has_value()) {
            // Some return value
        } else {
            // Void function
        }
        break;
    default:
        std::cerr << "Instruction " << instruction << " cannot be translated to bytecode.\n";
        break;
    }

    return next_op;
}
program::register_info::register_info(uint8_t register_number, size_t first_written)
    : reg_num{register_number}, first_write{first_written}, last_read{first_written} {}
} // namespace bytecode