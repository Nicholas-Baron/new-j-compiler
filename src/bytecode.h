//
// Created by nick on 5/15/20.
//

#ifndef NEW_J_COMPILER_BYTECODE_H
#define NEW_J_COMPILER_BYTECODE_H

#include "ir.h"

#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace bytecode {

enum class opcode : uint16_t {
    syscall = 0,
    add,
    sub,
    or_,
    ori,
    sl,
    sr,
    lui,
    sli,
    sri,
    slt,
    slti,
    addi,
    mul,
    jmp = 100,
    jal,
    jeq,
    jne,
    jr,
    lw = 200,
    sw,
    ldw,
    sdw,
    lqw,
    sqw,
    lb,
    sb,
};

struct operation {
    opcode code;

    struct reg_with_imm {
        std::array<uint8_t, 2> registers;
        uint32_t immediate;
    };

    std::variant<std::array<uint8_t, 3>, reg_with_imm, uint64_t> data;

    void print_human_readable(std::ostream &) const;
    uint64_t raw_form() const;
};

static constexpr uint64_t pc_start = 0x80000000;
static constexpr uint64_t data_start = 0x8C000000;

class program {
  public:
    static std::optional<program> from_ir(const ir::program &);

    void print_human_readable(std::ostream &) const;
    void print_file(const std::string & file_name) const;

  private:
    struct register_info {
        uint8_t reg_num;
        std::vector<size_t> writes;
        std::vector<size_t> reads;

        register_info(uint8_t register_number, size_t first_written);
    };

    void generate_bytecode(const ir::function & function);
    uint64_t append_data(const std::string &);
    void make_instruction(const ir::three_address &, std::map<std::string, register_info> &, size_t,
                          const ir::function &);

    void append_instruction(operation &&);
    void append_instruction(opcode op, decltype(operation::data) && data) {
        append_instruction({op, data});
    }

    std::map<size_t, std::pair<std::string, bool /* is absolute*/>> label_queue;
    void assign_label(const std::string &, size_t bytecode_loc);
    size_t read_label(const std::string &, bool absolute, size_t bytecode_loc);

    operation print(const ir::three_address &, const std::map<std::string, register_info> &);

    std::vector<uint8_t> generate_header_table() const;

    std::vector<char> data{};
    std::vector<operation> bytecode{};

    std::map<std::string, uint64_t> labels{};

    uint64_t text_end = pc_start;
};
} // namespace bytecode

#endif // NEW_J_COMPILER_BYTECODE_H
