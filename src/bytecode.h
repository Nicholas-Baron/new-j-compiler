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
};

static constexpr uint64_t pc_start = 0x80000000;

class program {
  public:
    static std::optional<program> from_ir(const ir::program &);

  private:
    void generate_bytecode(const ir::function & function);
    uint64_t append_data(const std::string &);

    std::vector<char> data{};
    std::vector<operation> bytecode{};

    std::map<std::string, uint64_t> labels{};

    uint64_t text_end = pc_start;
};
} // namespace bytecode

#endif // NEW_J_COMPILER_BYTECODE_H
