//
// Created by nick on 5/15/20.
//

#include "bytecode.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>

namespace bytecode {

operation::reg_with_imm make_reg_with_imm(uint8_t dest, uint8_t src, uint32_t imm) {
    return {{dest, src}, imm};
}

constexpr auto mask_low_32_bit = 0xFFFFFFFFul;

std::pair<std::optional<operation>, operation> load_64_bits(uint8_t dest, uint64_t val) {

    std::optional<operation> first;
    if (val > mask_low_32_bit) first = {opcode::lui, make_reg_with_imm(dest, 0, val >> 32u)};

    auto second = operation{opcode::ori, make_reg_with_imm(dest, (val > mask_low_32_bit ? dest : 0),
                                                           val & mask_low_32_bit)};
    return std::make_pair(first, second);
}
operation program::print(const ir::three_address & inst,
                         const std::map<std::string, register_info> & reg_info) {
    auto print_val = inst.inputs().back();
    switch (static_cast<ir::ir_type>(*print_val.type)) {
    case ir::ir_type::i32:
        if (not print_val.is_immediate) {
            auto name = std::get<std::string>(print_val.data);
            append_instruction(opcode::ori, make_reg_with_imm(1, 0, 1));
            return {opcode::syscall, make_reg_with_imm(1, reg_info.at(name).reg_num, 1)};
        } else {
            auto value = std::get<long>(print_val.data);
            append_instruction(opcode::ori, make_reg_with_imm(1, 0, 1));
            append_instruction(opcode::ori, make_reg_with_imm(2, 0, value));
            return {opcode::syscall, make_reg_with_imm(1, 2, 1)};
        }
    case ir::ir_type::i64:
        if (not print_val.is_immediate) {
            auto name = std::get<std::string>(print_val.data);
            append_instruction(opcode::ori, make_reg_with_imm(1, 0, 5));
            return {opcode::syscall, make_reg_with_imm(1, reg_info.at(name).reg_num, 1)};
        } else {
            auto value = std::get<long>(print_val.data);
            append_instruction(opcode::ori, make_reg_with_imm(1, 0, 5));

            auto [first, second] = load_64_bits(2, value);
            if (first.has_value()) append_instruction(std::move(*first));
            append_instruction(std::move(second));

            return {opcode::syscall, make_reg_with_imm(1, 2, 1)};
        }
    case ir::ir_type::str:
        if (print_val.is_immediate) {
            auto str_ptr = append_data(std::get<std::string>(print_val.data));
            auto [first, second] = load_64_bits(2, str_ptr);

            append_instruction(opcode::ori, make_reg_with_imm(1, 0, 4));
            if (first.has_value()) append_instruction(std::move(*first));
            append_instruction(std::move(second));
        } else {
            std::cerr << "Tried to print string " << print_val << std::endl;
        }
        return {opcode::syscall, make_reg_with_imm(1, 2, 1)};
    default:
        std::cerr << "Cannot print " << print_val << std::endl;
        return {opcode::add, std::array<uint8_t, 3>{0, 0, 0}};
    }
}

std::optional<program> program::from_ir(const ir::program & input) {

    auto * main_func = input.lookup_function("main");
    if (main_func == nullptr) return {};

    bytecode::program output{};
    output.generate_bytecode(*main_func);

    input.for_each_func([&output, main_func](auto * func) {
        if (func != nullptr and func != main_func) output.generate_bytecode(*func);
    });

    return output;
}

constexpr uint8_t return_value_start = 10;
constexpr uint8_t param_start = 13;
constexpr uint8_t param_end = 19;
constexpr auto max_inputs = (param_end - param_start) + 1;
constexpr uint8_t temp_start = 20;
constexpr uint8_t stack_pointer = 61;
constexpr auto temp_end = stack_pointer;
constexpr uint8_t frame_pointer = 62;
constexpr uint8_t return_address = 63;

void program::generate_bytecode(const ir::function & function) {

    assign_label(function.name, text_end);

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
            else if (reg > last_reg)
                break;

        if (last_reg >= temp_end) { std::cerr << "Too many temporaries" << std::endl; }

        const auto & ir_name = std::get<std::string>(operand.data);
        if (auto iter = register_alloc.find(ir_name); iter != register_alloc.end()) {
            iter->second.writes.push_back(instruction);
        } else {
            register_alloc.insert(iter,
                                  std::make_pair(ir_name, register_info{last_reg, instruction}));
        }
    };

    // Parameters start at 13 and end at 19
    if (uint8_t param_num = 13; function.parameters().size() <= max_inputs)
        for (auto & param : function.parameters())
            register_alloc.insert_or_assign(std::get<std::string>(param.data),
                                            register_info{param_num++, 0});
    else {
        // Too many parameters were declared
        std::cerr << "Function " << function.name << " has more than " << max_inputs
                  << " parameters.\n"
                     "Currently, "
                  << function.parameters().size() << " parameter functions are not supported.\n";
        labels.erase(function.name);
        return;
    }

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
                allocate_register(res.value(), ir_inst_num);

            for (auto & input : inst.inputs()) {
                if (not input.is_immediate and *input.type != ir::ir_type::str) {
                    // Either a user defined variable or compiler temporary
                    auto name = std::get<std::string>(input.data);
                    register_alloc.at(name).reads.push_back(ir_inst_num);
                }
            }

            ir_inst_num++;
        }
    }

    ir_inst_num = 0;
    for (auto & block : function.body) {
        assign_label(block->name, text_end);
        for (auto & instruction : block->contents) {
            make_instruction(instruction, register_alloc, ir_inst_num++, function);
        }
    }
}
uint64_t program::append_data(const std::string & item) {
    auto to_ret = this->data.size() + data_start;
    for (size_t i = 1; i < item.size() - 1; i++) data.push_back(item.at(i));
    data.push_back('\0');
    return to_ret;
}
void program::make_instruction(const ir::three_address & instruction,
                               std::map<std::string, register_info> & register_alloc,
                               size_t inst_num, const ir::function & func) {

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
            auto [first, second] = load_64_bits(result_reg, lhs_value + rhs_value);
            if (first.has_value()) append_instruction(std::move(*first));
            append_instruction(std::move(second));
        } else if (lhs.is_immediate and not rhs.is_immediate) {
            // ori lhs + add rhs
            append_instruction(
                opcode::ori,
                make_reg_with_imm(result_reg, 0, static_cast<uint32_t>(std::get<long>(lhs.data))));

            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            append_instruction(opcode::add, std::array{result_reg, result_reg, rhs_reg});
        } else if (rhs.is_immediate and not lhs.is_immediate) {
            // ori rhs + add lhs
            append_instruction(
                opcode::ori,
                make_reg_with_imm(result_reg, 0, static_cast<uint32_t>(std::get<long>(rhs.data))));

            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            append_instruction(opcode::add, std::array{result_reg, result_reg, lhs_reg});
        } else {
            // both are not immediates
            // simple add

            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            append_instruction(opcode::add, std::array{result_reg, lhs_reg, rhs_reg});
        }

    } break;
    case ir::operation::sub: {
        auto lhs = instruction.operands.at(1);
        auto rhs = instruction.operands.at(2);
        auto result_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
        if (lhs.is_immediate and rhs.is_immediate) {
            auto & lhs_value = std::get<long>(lhs.data);
            auto & rhs_value = std::get<long>(rhs.data);
            if (rhs_value >= INT64_MIN - lhs_value) {
                std::cerr << "Detected negative integer overflow of " << lhs << " + " << rhs
                          << '\n';
            }
            auto [first, second] = load_64_bits(result_reg, lhs_value - rhs_value);
            if (first.has_value()) append_instruction(std::move(*first));
            append_instruction(std::move(second));

        } else if (lhs.is_immediate and not rhs.is_immediate) {
            // ori lhs + sub rhs
            append_instruction(
                opcode::ori,
                make_reg_with_imm(result_reg, 0, static_cast<uint32_t>(std::get<long>(lhs.data))));

            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            append_instruction(opcode::sub, std::array{result_reg, result_reg, rhs_reg});
        } else if (rhs.is_immediate and not lhs.is_immediate) {
            // addi of negative rhs
            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            append_instruction(opcode::addi,
                               make_reg_with_imm(result_reg, lhs_reg,
                                                 static_cast<uint32_t>(-std::get<long>(rhs.data))));

        } else {
            // both are not immediates
            // simple sub

            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            append_instruction(opcode::sub, std::array{result_reg, lhs_reg, rhs_reg});
        }
    } break;
    case ir::operation::mul: {
        auto lhs = instruction.operands.at(1);
        auto rhs = instruction.operands.at(2);
        auto result_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
        if (lhs.is_immediate and rhs.is_immediate) {
            auto result = std::get<long>(lhs.data) * std::get<long>(rhs.data);
            auto [first, second] = load_64_bits(result_reg, result);
            if (first.has_value()) append_instruction(std::move(*first));
            append_instruction(std::move(second));
        } else if (not lhs.is_immediate and not rhs.is_immediate) {
            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            append_instruction(opcode::mul, std::array{result_reg, lhs_reg, rhs_reg});
        } else {
            std::cerr << "Mul instruction in " << instruction << " cannot be translated.\n";
        }
    } break;
    case ir::operation::assign: {
        auto result_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
        auto src = instruction.operands.back();
        if (src.is_immediate) {
            switch (static_cast<ir::ir_type>(*src.type)) {
            case ir::ir_type::str: {
                auto [first, second]
                    = load_64_bits(result_reg, append_data(std::get<std::string>(src.data)));
                if (first.has_value()) append_instruction(std::move(*first));
                append_instruction(std::move(second));
            } break;
            case ir::ir_type::i32:
                append_instruction(opcode::ori,
                                   make_reg_with_imm(result_reg, 0, std::get<long>(src.data)));
                break;
            case ir::ir_type::i64: {
                auto [first, second] = load_64_bits(result_reg, std::get<long>(src.data));
                if (first.has_value()) append_instruction(std::move(*first));
                append_instruction(std::move(second));
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
                append_instruction(opcode::lui, make_reg_with_imm(result_reg, 0, val >> 32u));
                val &= mask_low_32_bit;
            }

            append_instruction(opcode::ori, make_reg_with_imm(result_reg, result_reg, val));

        } else if (rhs.is_immediate and not lhs.is_immediate) {
            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            append_instruction(opcode::ori,
                               make_reg_with_imm(result_reg, lhs_reg, std::get<long>(rhs.data)));
        } else if (lhs.is_immediate and not rhs.is_immediate) {
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            append_instruction(opcode::ori,
                               make_reg_with_imm(result_reg, rhs_reg, std::get<long>(lhs.data)));
        } else {
            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            append_instruction(opcode::or_, std::array{result_reg, lhs_reg, rhs_reg});
        }
    } break;

    case ir::operation::halt:
        // TODO: Use the actual value in the register if it exists
        append_instruction(opcode::syscall, make_reg_with_imm(0, 0, 5));
        break;

    case ir::operation::ret:
        if (not instruction.operands.empty()) {
            uint8_t ret_loc = return_value_start;
            for (auto & val : instruction.operands)
                append_instruction(
                    opcode::ori,
                    make_reg_with_imm(
                        ret_loc++, get_register_info(std::get<std::string>(val.data)).reg_num, 0));
        }
        append_instruction(opcode::jr, std::array<uint8_t, 3>{return_address, 0, 0});
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
            auto imm = std::get<long>(rhs.data);
            if (imm >= UINT32_MAX) {
                std::cerr << "Cannot put " << rhs << " in the imm field.\n";
                break;
            }
            append_instruction(opcode::sli, make_reg_with_imm(result_reg, lhs_reg, imm));
        } else {
            // sl
            append_instruction(
                opcode::sl, std::array{result_reg, lhs_reg,
                                       get_register_info(std::get<std::string>(rhs.data)).reg_num});
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
            auto imm = std::get<long>(rhs.data);
            if (imm >= UINT32_MAX) {
                std::cerr << "Cannot put " << rhs << " in the imm field.\n";
                break;
            }
            append_instruction(opcode::sri, make_reg_with_imm(result_reg, lhs_reg, imm));
        } else {
            // sl
            append_instruction(
                opcode::sr, std::array{result_reg, lhs_reg,
                                       get_register_info(std::get<std::string>(rhs.data)).reg_num});
        }
    } break;
    case ir::operation::call: {

        const auto & func_name
            = std::get<std::string>(instruction.operands.at(res.has_value()).data);

        // Determine which items to save
        std::set registers_to_save{stack_pointer, frame_pointer, return_address};
        for (const auto & entry : register_alloc) {
            const auto & reg_info = entry.second;
            const auto & writes = reg_info.writes;
            const auto & reads = reg_info.reads;

            // check that we are in between a write and a read
            bool written_to = false;
            size_t write_loc = 0;
            // find the latest write before us
            for (const auto & write : writes) {
                if (write <= inst_num) {
                    write_loc = std::max(write_loc, write);
                    written_to = true;
                }
            }

            if (not written_to) continue;

            size_t read_loc = write_loc;
            // find the earliest read after us
            for (const auto & read : reads) {
                if (read < read_loc) continue;
                else if (read > inst_num) {
                    read_loc = read;
                    break;
                }
            }

            if (inst_num > write_loc and inst_num < read_loc) {
                // save that register
                registers_to_save.insert(reg_info.reg_num);
            }
        }

        const auto stack_size = static_cast<uint32_t>(registers_to_save.size() * -8);

        // Setup arguments
        const auto setup_args = [&instruction, this, &get_register_info] {
            uint8_t param_reg = param_start;
            bool has_result = instruction.result().has_value();
            auto & operands = instruction.operands;
            for (auto iter = operands.begin() + (has_result ? 2 : 1); iter != operands.end();
                 ++iter) {
                if (not iter->is_immediate)
                    append_instruction(
                        opcode::or_,
                        std::array<uint8_t, 3>{
                            param_reg++, 0,
                            get_register_info(std::get<std::string>(iter->data)).reg_num});
                else {
                    switch (static_cast<ir::ir_type>(*iter->type)) {

                    case ir::ir_type::boolean:
                        append_instruction(
                            opcode::ori,
                            make_reg_with_imm(param_reg++, 0, std::get<bool>(iter->data) ? 1 : 0));
                        break;
                    case ir::ir_type::str: {
                        auto [first, second] = load_64_bits(
                            param_reg++, append_data(std::get<std::string>(iter->data)));
                        if (first.has_value()) append_instruction(std::move(*first));
                        append_instruction(std::move(second));
                    } break;
                    case ir::ir_type::i32:
                    case ir::ir_type::i64: {
                        auto [first, second]
                            = load_64_bits(param_reg++, std::get<long>(iter->data));
                        if (first.has_value()) append_instruction(std::move(*first));
                        append_instruction(std::move(second));
                    } break;
                    default:
                        std::cerr << "Cannot load " << *iter << " into register " << param_reg++
                                  << '\n';
                    }
                }
            }
        };

        if (func_name == "print") {
            // setup_args();
            append_instruction(print(instruction, register_alloc));
            break;
        }

        append_instruction(opcode::addi,
                           make_reg_with_imm(stack_pointer, stack_pointer, stack_size));

        uint16_t offset = 0;
        for (auto reg : registers_to_save) {
            append_instruction(opcode::sqw, make_reg_with_imm(reg, stack_pointer, offset));
            offset += 8;
        }

        setup_args();
        append_instruction(opcode::jal, read_label(func_name, true, text_end));

        // TODO: Implement multiple return value copies
        if (res.has_value()) {
            // Some return value (the register has already been allocated)
            auto dest_reg = get_register_info(std::get<std::string>(res.value().data)).reg_num;
            append_instruction(opcode::ori, make_reg_with_imm(dest_reg, return_value_start, 0));
        }

        for (auto iter = registers_to_save.rbegin(); iter != registers_to_save.rend(); ++iter) {
            offset -= 8;
            append_instruction(opcode::lqw, make_reg_with_imm(*iter, stack_pointer, offset));
        }

        append_instruction(opcode::addi,
                           make_reg_with_imm(stack_pointer, stack_pointer, -stack_size));
    } break;
    case ir::operation::branch:
        if (instruction.operands.size() == 1) {
            append_instruction(opcode::jmp,
                               read_label(std::get<std::string>(instruction.operands.front().data),
                                          true, text_end));
        } else {
            // conditional branch
            const auto & condition = std::get<std::string>(instruction.operands.front().data);

            const auto & writes = register_alloc.at(condition).writes;
            size_t write_loc = 0;
            for (auto & write : writes)
                if (write < inst_num) write_loc = std::max(write_loc, write);

            auto * cond_inst = func.instruction_number(write_loc);
            if (cond_inst == nullptr or cond_inst->operands.size() != 3) {
                std::cerr << "Could determine condition for " << instruction << std::endl;
                break;
            }

            const auto & lhs = cond_inst->operands.at(1);
            const auto & rhs = cond_inst->operands.at(2);

            const auto & true_dest = std::get<std::string>(instruction.operands.at(1).data);
            const auto & false_dest = std::get<std::string>(instruction.operands.back().data);

            switch (cond_inst->op) {
            case ir::operation::eq:
                if (not lhs.is_immediate and not rhs.is_immediate) {
                    auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
                    auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
                    append_instruction(opcode::jeq,
                                       make_reg_with_imm(lhs_reg, rhs_reg,
                                                         read_label(true_dest, false, text_end)));
                } else if (lhs.is_immediate and rhs.is_immediate) {
                    auto lhs_val = std::get<long>(lhs.data);
                    auto rhs_val = std::get<long>(rhs.data);
                    if (lhs_val == rhs_val) {
                        append_instruction(opcode::jmp, read_label(true_dest, true, text_end));
                    } else {
                        append_instruction(opcode::jmp, read_label(false_dest, true, text_end));
                    }
                } else if (lhs.is_immediate and not rhs.is_immediate) {
                    auto lhs_val = std::get<long>(lhs.data);
                    {
                        auto [first, second] = load_64_bits(1, lhs_val);
                        if (first.has_value()) append_instruction(std::move(*first));
                        append_instruction(std::move(second));
                    }
                    auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
                    append_instruction(
                        opcode::jeq,
                        make_reg_with_imm(1, rhs_reg, read_label(true_dest, false, text_end)));
                    append_instruction(opcode::jmp, read_label(false_dest, true, text_end));
                } else if (not lhs.is_immediate and rhs.is_immediate) {
                    auto rhs_val = std::get<long>(rhs.data);
                    {
                        auto [first, second] = load_64_bits(1, rhs_val);
                        if (first.has_value()) append_instruction(std::move(*first));
                        append_instruction(std::move(second));
                    }
                    auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
                    append_instruction(
                        opcode::jeq,
                        make_reg_with_imm(lhs_reg, 1, read_label(true_dest, false, text_end)));
                    append_instruction(opcode::jmp, read_label(false_dest, true, text_end));
                } else {
                    std::cerr << "Cannot generate jeq for " << *cond_inst << std::endl;
                }
                break;
            case ir::operation::le:
                // First, do the less than and then the equal
                if (not lhs.is_immediate and not rhs.is_immediate) {
                    auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
                    auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
                    append_instruction(opcode::slt, std::array<uint8_t, 3>{1, lhs_reg, rhs_reg});
                    append_instruction(
                        opcode::jne,
                        make_reg_with_imm(1, 0, read_label(true_dest, false, text_end)));
                    append_instruction(opcode::jeq,
                                       make_reg_with_imm(lhs_reg, rhs_reg,
                                                         read_label(true_dest, false, text_end)));
                    append_instruction(opcode::jmp, read_label(false_dest, true, text_end));
                } else if (rhs.is_immediate and not lhs.is_immediate) {
                    auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
                    // changes the less than or equal into just less than
                    auto rhs_val = std::get<long>(rhs.data) + 1;

                    append_instruction(opcode::slti, make_reg_with_imm(1, lhs_reg, rhs_val));

                    append_instruction(
                        opcode::jne,
                        make_reg_with_imm(1, 0, read_label(true_dest, false, text_end)));

                    append_instruction(opcode::jmp, read_label(false_dest, true, text_end));

                } else {
                    std::cerr << "Cannot generate jle for " << *cond_inst << std::endl;
                }
                break;
            case ir::operation::gt:
                // First, do the less than and then the equal
                if (not lhs.is_immediate and not rhs.is_immediate) {
                    auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
                    auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
                    append_instruction(opcode::slt, std::array<uint8_t, 3>{1, rhs_reg, lhs_reg});
                    append_instruction(
                        opcode::jne,
                        make_reg_with_imm(1, 0, read_label(true_dest, false, text_end)));
                    append_instruction(opcode::jmp, read_label(false_dest, true, text_end));
                } else if (not rhs.is_immediate and lhs.is_immediate) {
                    auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
                    // changes the less than or equal into just less than
                    auto lhs_val = std::get<long>(lhs.data) + 1;

                    append_instruction(opcode::slti, make_reg_with_imm(1, rhs_reg, lhs_val));

                    append_instruction(
                        opcode::jne,
                        make_reg_with_imm(1, 0, read_label(true_dest, false, text_end)));

                    append_instruction(opcode::jmp, read_label(false_dest, true, text_end));
                } else if (rhs.is_immediate and not lhs.is_immediate) {
                    uint8_t rhs_reg = 1;
                    {
                        auto [first, second] = load_64_bits(rhs_reg, std::get<long>(rhs.data));
                        if (first.has_value()) append_instruction(std::move(*first));
                        append_instruction(std::move(second));
                    }

                    auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
                    append_instruction(opcode::slt, std::array{rhs_reg, rhs_reg, lhs_reg});
                    append_instruction(
                        opcode::jne,
                        make_reg_with_imm(rhs_reg, 0, read_label(true_dest, false, text_end)));
                    append_instruction(opcode::jmp, read_label(false_dest, true, text_end));
                } else {
                    std::cerr << "Cannot generate jgt for " << *cond_inst << std::endl;
                }
                break;
                /*
            case ir::operation::ne:
                break;
            case ir::operation::ge:
                break;
            case ir::operation::lt:
                break;
                 */
            default:
                std::cerr << "Instruction " << *cond_inst << " is not a valid condition"
                          << std::endl;
            }
        }
        break;
    case ir::operation::le:
    case ir::operation::eq:
    case ir::operation::gt:
        break;
    default:
        std::cerr << "Instruction " << instruction << " cannot be translated to bytecode.\n";
        break;
    }
}
program::register_info::register_info(uint8_t register_number, size_t first_written)
    : reg_num{register_number}, writes{first_written}, reads{} {}

void operation::print_human_readable(std::ostream & lhs) const {

    using std::setw, std::get, std::dec, std::hex;

    // Print opcode number
    lhs << setw(3) << dec << static_cast<uint16_t>(this->code) << ' ';
    bool first_arg = true;
    static constexpr auto comma = ", ";

    static const auto print_reg
        = [&lhs](auto & reg_num) { lhs << setw(2) << std::to_string(reg_num); };

    static const auto print_imm
        = [&lhs](auto & imm) { lhs << hex << std::noshowbase << "#x" << imm; };

    switch (this->data.index()) {
    case 0:
        for (auto val : get<std::array<uint8_t, 3>>(data)) {
            if (first_arg) {
                first_arg = false;
            } else
                lhs << comma;

            print_reg(val);
        }
        break;
    case 1: {
        auto & with_imm = get<reg_with_imm>(this->data);
        auto left_reg = with_imm.registers.front();
        auto right_reg = with_imm.registers.back();

        print_reg(left_reg);
        lhs << comma;
        print_reg(right_reg);
        lhs << comma;
        print_imm(with_imm.immediate);
    } break;
    case 2:
        print_imm(get<uint64_t>(data));
        break;
    default:
        lhs << "UNKNOWN DATA";
        break;
    }
}

constexpr uint8_t reg_width = 6;
constexpr uint8_t imm_width = 32;
constexpr uint8_t opcode_offset = 54;
constexpr uint8_t r0_offset = opcode_offset - reg_width;
constexpr uint8_t r1_offset = opcode_offset - (2 * reg_width);
constexpr uint8_t r2_offset = opcode_offset - (3 * reg_width);
constexpr uint8_t imm_offset = opcode_offset - (2 * reg_width + imm_width);

uint64_t operation::raw_form() const {

    auto shift_field
        = [](auto value, uint8_t amt) -> uint64_t { return static_cast<uint64_t>(value) << amt; };

    auto op_area = static_cast<uint64_t>(this->code) << opcode_offset;

    uint64_t data = 0;
    switch (this->data.index()) {
    case 0: { // Array
        auto & reg = std::get<std::array<uint8_t, 3>>(this->data);
        data = shift_field(reg[0], r0_offset) | shift_field(reg[1], r1_offset)
               | shift_field(reg[2], r2_offset);
    } break;
    case 1: {
        auto & reg_and_imm = std::get<reg_with_imm>(this->data);
        data = shift_field(reg_and_imm.registers[0], r0_offset)
               | shift_field(reg_and_imm.registers[1], r1_offset)
               | shift_field(reg_and_imm.immediate, imm_offset);
    } break;
    case 2:
        data = std::get<uint64_t>(this->data) & ~(~0ul << opcode_offset);
        break;
    }

    return op_area | data;
}

void program::print_human_readable(std::ostream & lhs) const {

    const std::map label_map = [this] {
        std::map<uint64_t, std::string> label_map;
        for (const auto & entry : this->labels) { label_map.emplace(entry.second, entry.first); }
        return label_map;
    }();

    lhs << ".data:\n";
    uint8_t byte_in_word = 0;
    for (auto & byte : this->data) {
        ++byte_in_word %= 8;
        lhs << std::hex << (int)byte << (byte_in_word == 0 ? '\n' : ' ');
    }

    lhs << "\n.text:\n";
    auto pc = pc_start;
    for (auto & inst : this->bytecode) {
        if (auto label = label_map.find(pc); label != label_map.end()) {
            lhs << label->second << ":\n";
        }

        lhs << std::hex << std::showbase << pc << " : ";
        inst.print_human_readable(lhs);
        lhs << std::endl;
        pc += 8;
    }
}

void program::append_instruction(operation && op) {
    bytecode.push_back(op);
    text_end += 8;
}
void program::assign_label(const std::string & label, size_t bytecode_loc) {
    labels.emplace(label, text_end);

    std::vector<size_t> fulfilled;
    for (auto & entry : label_queue)
        if (entry.second.first == label) {
            auto & inst = bytecode.at((entry.first - pc_start) / 8);
            if (entry.second.second) {
                // use absolute addressing
                inst.data = bytecode_loc >> 3u;
            } else {
                // use relative addressing
                auto dist = bytecode_loc - (entry.first + 8);
                std::get<operation::reg_with_imm>(inst.data).immediate = dist >> 3u;
            }
            fulfilled.push_back(entry.first);
        }

    for (auto & satisfied : fulfilled) label_queue.erase(satisfied);
}
size_t program::read_label(const std::string & label, bool absolute, size_t bytecode_loc) {
    if (labels.count(label) == 0) {
        // Label does not exist. Fake it
        label_queue.insert(std::make_pair(bytecode_loc, std::make_pair(label, absolute)));
        return 0;
    }

    auto & addr = labels.at(label);
    if (absolute) return addr >> 3u;
    else {
        auto dist = bytecode_loc - (addr + 8);
        return dist >> 3u;
    }
}

constexpr uint8_t magic_byte = 0x7e;

void program::print_file(const std::string & filename) const {

    std::vector<uint8_t> file_contents{magic_byte, 'N', 'J'};
    file_contents.reserve(data.size() + bytecode.size() * 8);

    {
        const auto header_table = generate_header_table();

        for (auto i = 0u; i < 4; i++)
            file_contents.push_back((header_table.size() >> i * 8) & 0xFFu);
        for (auto byt : header_table) file_contents.push_back(byt);
    }

    const auto find_in_file = [&file_contents](std::vector<uint8_t> && content) -> size_t {
        auto iter = std::search(file_contents.begin(), file_contents.end(), content.begin(),
                                content.end());
        if (iter == file_contents.end()) return file_contents.size();
        else
            return (iter - file_contents.begin()) + content.size();
    };

    {
        auto data_offset = find_in_file({'.', 'd', 'a', 't', 'a', 0});
        if (data_offset != file_contents.size() and not data.empty()) {
            auto data_pos = file_contents.size();
            for (auto i = 0u; i < 4; i++)
                file_contents.at(data_offset + i) = (data_pos >> i * 8u) & 0xFFu;
        }

        for (auto byt : data) file_contents.push_back(byt);
    }

    {
        auto text_offset = find_in_file({'.', 't', 'e', 'x', 't', 0});
        if (text_offset != file_contents.size() and not bytecode.empty()) {
            auto text_pos = file_contents.size();
            for (auto i = 0u; i < 4; i++)
                file_contents.at(text_offset + i) = (text_pos >> i * 8u) & 0xFFu;
        }

        for (auto inst : bytecode) {
            auto raw_inst = inst.raw_form();
            for (auto i = 0u; i < sizeof(raw_inst); i++)
                file_contents.push_back((raw_inst >> i * 8) & 0xFFu);
        }
    }

    std::ofstream outfile{filename, std::ios::binary | std::ios::out};
    outfile.write((char *)file_contents.data(), file_contents.size());
}
std::vector<uint8_t> program::generate_header_table() const {

    std::vector<uint8_t> header_table;

    // header table
    if (not this->data.empty()) {
        // name of entry
        for (auto c : ".data") header_table.push_back(c);
        // byte offset
        for (auto i = 0u; i < sizeof(uint32_t); i++) header_table.push_back(0);
        // length of section
        auto data_length = data.size();
        if (data_length > UINT32_MAX) std::cerr << "Data section too long\n";
        for (auto i = 0u; i < sizeof(uint32_t); i++)
            header_table.push_back((data_length >> i * 8u) & 0xFFu);
        // null byte
        header_table.push_back('\0');
    }

    if (bytecode.empty()) std::cerr << "No runnable code generated\n";

    // text entry in header table
    for (auto c : ".text") header_table.push_back(c);
    // offset to text
    for (auto i = 0; i < 4; i++) header_table.push_back(0);
    // length of text
    {
        static_assert(sizeof(uint64_t) == 8);
        const auto text_length = bytecode.size() * sizeof(uint64_t);
        if (text_length > UINT32_MAX) std::cerr << "Text section too long\n";
        for (auto i = 0u; i < 4; i++) header_table.push_back((text_length >> i * 8u) & 0xFFu);
    }
    header_table.push_back('\0');

    if (header_table.size() > UINT32_MAX) {
        std::cerr << "Header table too long\n";
        return {};
    }

    return header_table;
}

} // namespace bytecode
