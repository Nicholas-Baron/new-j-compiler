//
// Created by nick on 5/15/20.
//

#include "bytecode.h"

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
    switch (print_val.type) {
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

    input.for_each_func([&output, main_func](auto * func) {
        if (func == nullptr) return;

        if (func == main_func)
            output.main_offset = static_cast<uint32_t>(output.text_end - pc_start);

        output.generate_bytecode(*func);
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

    labels.emplace(function.name, text_end);

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

        const auto & ir_name = std::get<std::string>(operand.data);
        if (auto iter = register_alloc.find(ir_name); iter != register_alloc.end()) {
            iter->second.writes.push_back(instruction);
        } else {
            register_alloc.insert(iter,
                                  std::make_pair(ir_name, register_info{last_reg, instruction}));
        }
    };

    // Parameters start at 13 and end at 19
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
                if (not input.is_immediate and input.type != ir::ir_type::str) {
                    // Either a user defined variable or compiler temporary
                    auto name = std::get<std::string>(input.data);
                    register_alloc.at(name).last_read = ir_inst_num;
                }
            }

            ir_inst_num++;
        }
    }

    ir_inst_num = 0;
    for (auto & block : function.body) {
        labels.emplace(block->name, text_end);
        for (auto & instruction : block->contents) {
            append_instruction(make_instruction(instruction, register_alloc, ir_inst_num++));
        }
    }
}
uint64_t program::append_data(const std::string & item) {
    auto to_ret = this->data.size() + data_start;
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
            auto [first, second] = load_64_bits(result_reg, lhs_value + rhs_value);
            if (first.has_value()) append_instruction(std::move(*first));
            next_op = second;
        } else if (lhs.is_immediate and not rhs.is_immediate) {
            // ori lhs + add rhs
            append_instruction(
                opcode::ori,
                make_reg_with_imm(result_reg, 0, static_cast<uint32_t>(std::get<long>(lhs.data))));

            next_op.code = opcode::add;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            next_op.data = std::array{result_reg, result_reg, rhs_reg};
        } else if (rhs.is_immediate and not lhs.is_immediate) {
            // ori rhs + add lhs
            append_instruction(
                opcode::ori,
                make_reg_with_imm(result_reg, 0, static_cast<uint32_t>(std::get<long>(rhs.data))));

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
            next_op = second;

        } else if (lhs.is_immediate and not rhs.is_immediate) {
            // ori lhs + sub rhs
            append_instruction(
                opcode::ori,
                make_reg_with_imm(result_reg, 0, static_cast<uint32_t>(std::get<long>(lhs.data))));

            next_op.code = opcode::sub;
            auto rhs_reg = get_register_info(std::get<std::string>(rhs.data)).reg_num;
            next_op.data = std::array{result_reg, result_reg, rhs_reg};
        } else if (rhs.is_immediate and not lhs.is_immediate) {
            // ori rhs + sub lhs
            append_instruction(
                opcode::ori,
                make_reg_with_imm(result_reg, 0, static_cast<uint32_t>(std::get<long>(rhs.data))));

            next_op.code = opcode::sub;
            auto lhs_reg = get_register_info(std::get<std::string>(lhs.data)).reg_num;
            next_op.data = std::array{result_reg, lhs_reg, result_reg};
        } else {
            // both are not immediates
            // simple sub

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
                if (first.has_value()) append_instruction(std::move(*first));
                next_op = second;
            } break;
            case ir::ir_type::i32:
                next_op.code = opcode::ori;
                next_op.data = make_reg_with_imm(result_reg, 0,
                                                 static_cast<uint32_t>(std::get<long>(src.data)));
                break;
            case ir::ir_type::i64: {
                auto [first, second] = load_64_bits(result_reg, std::get<long>(src.data));
                if (first.has_value()) append_instruction(std::move(*first));
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
                append_instruction(opcode::lui, make_reg_with_imm(result_reg, 0, val >> 32u));
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
        if (not instruction.operands.empty()) {
            uint8_t ret_loc = return_value_start;
            for (auto & val : instruction.operands)
                append_instruction(
                    opcode::ori,
                    make_reg_with_imm(
                        ret_loc++, get_register_info(std::get<std::string>(val.data)).reg_num, 0));
        }
        next_op.code = opcode::jr;
        next_op.data = std::array<uint8_t, 3>{return_address, 0, 0};
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
    case ir::operation::call: {
        uint8_t param_reg = param_start;
        for (auto iter = instruction.operands.begin() + 1; iter != instruction.operands.end();
             ++iter) {
            if (not iter->is_immediate)
                append_instruction(
                    opcode::or_, std::array<uint8_t, 3>{
                                     param_reg++, 0,
                                     get_register_info(std::get<std::string>(iter->data)).reg_num});
        }

        const auto & func_name
            = std::get<std::string>(instruction.operands.at(res.has_value()).data);

        if (func_name == "print") {
            next_op = print(instruction, register_alloc);
            break;
        }
        // Determine which items to save
        std::set registers_to_save{stack_pointer, frame_pointer, return_address};
        for (const auto & entry : register_alloc) {
            if (entry.second.last_read >= inst_num
                and registers_to_save.count(entry.second.reg_num) == 0)
                registers_to_save.insert(entry.second.reg_num);
        }

        auto stack_size = static_cast<uint32_t>(registers_to_save.size() * -8);

        append_instruction(opcode::addi,
                           make_reg_with_imm(stack_pointer, stack_pointer, stack_size));

        uint16_t offset = 0;
        for (auto reg : registers_to_save) {
            append_instruction(opcode::sqw, make_reg_with_imm(reg, stack_pointer, offset));
            offset += 8;
        }

        append_instruction(opcode::jal, labels.at(func_name));

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

        next_op.code = opcode::addi;
        next_op.data = make_reg_with_imm(stack_pointer, stack_pointer, -stack_size);
    } break;
    default:
        std::cerr << "Instruction " << instruction << " cannot be translated to bytecode.\n";
        break;
    }

    return next_op;
}
program::register_info::register_info(uint8_t register_number, size_t first_written)
    : reg_num{register_number}, writes{first_written}, last_read{first_written} {}

void operation::print_human_readable(std::ostream & lhs) const {

    // print op code
    lhs << std::setw(3) << std::dec << static_cast<uint16_t>(this->code) << ' ';
    bool first_arg = true;
    static constexpr auto comma = ", ";
    switch (this->data.index()) {
    case 0:
        for (auto val : std::get<std::array<uint8_t, 3>>(data)) {
            if (first_arg) {
                first_arg = false;
            } else
                lhs << comma;

            lhs << std::setw(2) << std::dec << std::to_string(val);
        }
        break;
    case 1: {
        auto & with_imm = std::get<reg_with_imm>(this->data);
        auto left_reg = std::to_string(with_imm.registers.front());
        auto right_reg = std::to_string(with_imm.registers.back());

        lhs << std::setw(2) << left_reg << comma << std::setw(2) << right_reg << comma << std::hex
            << std::showbase << with_imm.immediate;
    } break;
    case 2:
        lhs << std::hex << std::showbase << std::get<uint64_t>(this->data);
        break;
    default:
        lhs << "UNKNOWN DATA";
        break;
    }
}

void program::print_human_readable(std::ostream & lhs) const {

    const std::map label_map = [this] {
        std::map<uint64_t, std::string> label_map;
        for (const auto & entry : this->labels) { label_map.emplace(entry.second, entry.first); }
        return label_map;
    }();

    lhs << ".text:\n";
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

} // namespace bytecode
