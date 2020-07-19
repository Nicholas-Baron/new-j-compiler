//
// Created by nick on 4/17/20.
//

#include "visitor.h"

#include "ast/nodes.h"

#include <iostream>

void printing_visitor::visit(const ast::node & node) {

    print_indent();
    indent_depth += indent_size;

    if (node_count == 0) std::cout << std::boolalpha;

    this->node_count++;

    switch (node.type()) {
    case ast::node_type::var_decl: {
        auto & decl = dynamic_cast<const ast::var_decl &>(node);
        std::cout << (decl.detail == ast::var_decl::details::Let ? "Variable" : "Const")
                  << " decl for " << decl.identifier() << '\n';
        if (decl.detail != ast::var_decl::details::Let) {
            print_indent();
            std::cout << "Is global: " << decl.in_global_scope() << '\n';
        }
        visit(decl.name);
        visit(*decl.val);
    } break;
    case ast::node_type::function: {
        auto & func_decl = dynamic_cast<const ast::function &>(node);
        std::cout << "Function decl for " << func_decl.identifier() << '\n';
        visit(func_decl.name);
        for (const auto & param : func_decl.params) visit(param);
        visit(*func_decl.body);
    } break;
    case ast::node_type::parameter:
        std::cout << "Parameter " << node.text() << '\n';
        break;
    case ast::node_type::statement_block: {
        auto & block = dynamic_cast<const ast::stmt_block &>(node);
        std::cout << "Statement block " << block.text() << '\n';
        for (const auto & stmt : block.stmts) visit(*stmt);
    } break;
    case ast::node_type::value:
        std::cout << "Value " << node.text() << '\n';
        break;
    case ast::node_type::opt_typed:
        std::cout << "Optionally typed " << node.text() << '\n';
        print_indent();
        std::cout << "Has user type: " << dynamic_cast<const ast::opt_typed &>(node).user_explicit()
                  << '\n';
        break;
    case ast::node_type::func_call: {
        auto & call = dynamic_cast<const ast::func_call &>(node);
        std::cout << "Function call to " << call.text() << '\n';
        visit(*call.func_name);
        for (const auto & arg : call.arguments) visit(*arg);
    } break;
    case ast::node_type::if_statement: {
        std::cout << "If statement\n";
        auto & condition = dynamic_cast<const ast::if_stmt &>(node);
        visit(*condition.cond);
        visit(*condition.then_block);
        if (condition.else_block != nullptr) visit(*condition.else_block);
    } break;
    case ast::node_type ::binary_op:
        std::cout << "Binary operation\n";
        visit(dynamic_cast<const ast::bin_op &>(node).lhs_ref());
        visit(dynamic_cast<const ast::bin_op &>(node).rhs_ref());
        break;
    case ast::node_type ::return_statement:
        std::cout << "Return\n";
        if (auto & ret = dynamic_cast<const ast::ret_stmt &>(node); ret.value != nullptr)
            visit(*ret.value);
        break;
    case ast::node_type::while_loop: {
        auto & loop = dynamic_cast<const ast::while_loop &>(node);
        std::cout << "While loop\n";
        visit(*loop.condition);
        visit(*loop.body);
    } break;
    case ast::node_type::assign_statement: {
        auto & assign_stmt = dynamic_cast<const ast::assign_stmt &>(node);
        std::cout << "Assignment statement\n";
        print_indent();
        std::cout << "Is op-assign: " << std::boolalpha
                  << (assign_stmt.assign_op != ast::operation::assign) << '\n';
        visit(*assign_stmt.dest);
        visit(*assign_stmt.value_src);
    } break;
    default:
        std::cout << "Unimplemented visit on node " << node.text() << std::endl;
    }

    indent_depth -= indent_size;
}

void printing_visitor::print_indent() const { std::cout << std::string(indent_depth, ' '); }

void ir_gen_visitor::visit(const ast::node & node) {
    switch (node.type()) {
    case ast::node_type ::var_decl: {
        auto & decl = dynamic_cast<const ast::var_decl &>(node);
        auto id = decl.identifier();

        auto value = this->fold_to_constant(decl.value_expr());
        if (not value) {
            std::cerr << "Could not evaluate the constant " << decl.identifier() << '\n';
            return;
        }

        if (decl.in_global_scope()) {
            // Is a global

            // Check that we are not redeclaring the value
            auto & globals = this->global_scope();
            if (globals.count(id) != 0) {
                std::cerr << "Redeclaring the global constant " << decl.identifier() << '\n';
                return;
            }

            globals.try_emplace(id, std::move(*value));
        } else {
            // Some local
            auto & locals = this->current_scope();
            if (locals.count(id) != 0) {
                std::cerr << "Redeclaring the local constant " << decl.identifier() << '\n';
                return;
            }

            if (decl.detail == ast::var_decl::details::Const)
                locals.try_emplace(id, std::move(*value));
            else {
                auto operand = ir::operand{id, value.value().type, false};
                append_instruction(ir::operation::assign, {operand, value.value()});
                locals.try_emplace(id, operand);
            }
        }

    } break;
    case ast::node_type ::function:
        generate_function(dynamic_cast<const ast::function &>(node));
        break;
    case ast::node_type::opt_typed: {
        auto & name = dynamic_cast<const ast::opt_typed &>(node);
        if (current_func != nullptr and name.name() == current_func->name
            and *current_func->type.return_type == ir::ir_type::unit and name.user_explicit()) {
            std::cerr << "Deprecated path for assigning return type of function\n";
        } else if (name.user_explicit())
            std::cerr << "Unknown action to preform on " << name.text() << '\n';
    } break;
    case ast::node_type ::statement_block:
        this->active_variables.emplace_back();
        if (not current_block()->contents.empty() and current_block()->terminated())
            this->append_block(this->block_name());

        for (const auto & stmt : dynamic_cast<const ast::stmt_block &>(node).stmts) visit(*stmt);
        this->active_variables.pop_back();
        break;
    case ast::node_type ::func_call: {
        auto & func_call = dynamic_cast<const ast::func_call &>(node);

        std::vector<ir::operand> args;
        args.push_back(eval_ast(*func_call.func_name));
        for (const auto & arg : func_call.arguments) args.push_back(eval_ast(*arg));

        append_instruction(ir::operation ::call, std::move(args));
    } break;
    case ast::node_type ::parameter: {
        std::cerr << "Deprecated path for generating parameter ir\n";
    } break;
    case ast::node_type::if_statement: {
        auto & if_stmt = dynamic_cast<const ast::if_stmt &>(node);

        auto then_block_name = this->block_name();
        auto exit_name = this->block_name();

        eval_if_condition(*if_stmt.cond, then_block_name, exit_name);

        append_block(std::move(then_block_name));
        visit(*if_stmt.then_block);

        auto label_type = prog.lookup_type("string");

        if (if_stmt.else_block == nullptr and not current_block()->terminated()) {
            append_instruction(ir::operation ::branch, {{exit_name, label_type, false}});
            append_block(std::move(exit_name));
        } else if (if_stmt.else_block != nullptr) {
            auto real_exit_name = block_name();
            if (not current_block()->terminated())
                append_instruction(ir::operation ::branch, {{real_exit_name, label_type, false}});
            append_block(std::move(exit_name));
            visit(*if_stmt.else_block);
            if (not current_block()->terminated()) {
                append_instruction(ir::operation ::branch, {{real_exit_name, label_type, false}});
                append_block(std::move(real_exit_name));
            }
        }
    } break;
    case ast::node_type ::value:
        std::cerr << "Cannot do anything with " << node.text() << " in visit\n";
        break;
    case ast::node_type ::return_statement: {
        auto & ret = dynamic_cast<const ast::ret_stmt &>(node);

        std::vector<ir::operand> return_value;

        if (ret.value != nullptr) return_value.push_back(eval_ast(*ret.value));
        append_instruction(ir::operation ::ret, std::move(return_value));
    } break;
    case ast::node_type::while_loop: {
        auto & loop = dynamic_cast<const ast::while_loop &>(node);

        auto * cond_block = append_block(block_name());

        auto loop_block = block_name();
        auto loop_end = block_name();

        eval_if_condition(*loop.condition, loop_block, loop_end);

        append_block(std::move(loop_block));
        visit(*loop.body);
        append_instruction(ir::operation::branch,
                           {{cond_block->name, prog.lookup_type("string"), false}});
        append_block(std::move(loop_end));
    } break;
    case ast::node_type::assign_statement: {
        auto & assign = dynamic_cast<const ast::assign_stmt &>(node);
        auto lhs_op = eval_ast(*assign.dest);
        if (lhs_op.is_immediate) {
            std::cerr << "Cannot assign to " << lhs_op << '\n';
            return;
        }

        auto rhs = eval_ast(*assign.value_src);

        switch (std::vector operands{lhs_op, lhs_op, rhs}; assign.assign_op) {
        case ast::operation::add:
            append_instruction(ir::operation::add, std::move(operands));
            break;
        case ast::operation::assign:
            if (not rhs.is_immediate)
                // Rename operand to us
                this->current_block()->contents.back().operands.front() = lhs_op;
            else
                append_instruction(ir::operation::assign, {lhs_op, rhs});
            break;
        case ast::operation::div:
            append_instruction(ir::operation::div, std::move(operands));
            break;
        case ast::operation::mult:
            append_instruction(ir::operation::mul, std::move(operands));
            break;
        case ast::operation::sub:
            append_instruction(ir::operation::sub, std::move(operands));
            break;
        default:
            std::cerr << "Unsupported op-assign " << assign.text() << '\n';
        }

    } break;
    default:
        std::cerr << "Unimplemented ir gen for node " << node.text() << std::endl;
        break;
    }
}

ir_gen_visitor::scope_t & ir_gen_visitor::global_scope() noexcept {
    if (active_variables.empty()) active_variables.emplace_back();
    return active_variables.front();
}

ir_gen_visitor::scope_t & ir_gen_visitor::current_scope() noexcept {
    if (active_variables.empty()) active_variables.emplace_back();
    return active_variables.back();
}
std::optional<ir::operand> ir_gen_visitor::fold_to_constant(ast::expression & expr) {
    switch (expr.type()) {
    case ast::node_type ::value:
        switch (auto value = dynamic_cast<ast::literal_or_variable &>(expr).data(); value.index()) {
        case 2: // long
            return std::optional{ir::operand{std::get<2>(value), prog.lookup_type("int64"), true}};

        case 0: // monostate
        default:
            std::cerr << "Unknown immediate data: "
                      << dynamic_cast<ast::literal_or_variable &>(expr)
                      << " (Index number: " << value.index() << ")\n";
        }
        break;
    case ast::node_type::binary_op:
        switch (auto & bin_op = dynamic_cast<ast::bin_op &>(expr); bin_op.oper()) {
        case ast::operation ::add: {
            auto lhs = fold_to_constant(bin_op.lhs_ref());
            if (not lhs) {
                std::cerr << "Could not evaluate left hand side of " << bin_op.text() << '\n';
                break;
            }
            auto rhs = fold_to_constant(bin_op.rhs_ref());
            if (not rhs) {
                std::cerr << "Could not evaluate right hand side of " << bin_op.text() << '\n';
                break;
            }

            if (lhs.value().data.index() != rhs.value().data.index()) {
                std::cerr << "Left and right hand sides of expression are of different types: "
                          << bin_op.text() << '\n';
                break;
            }

            switch (lhs.value().data.index()) {
            case 2: // long
                return std::optional{
                    ir::operand{std::get<2>(lhs.value().data) + std::get<2>(rhs.value().data),
                                prog.lookup_type("int64"), true}};
            case 0:
            default:
                std::cerr << "Unknown type of expression: " << bin_op.text()
                          << " (Index number: " << lhs.value().data.index() << ")\n";
            }
        } break;
        default:
            std::cerr << "Could not evaluate binary expression " << bin_op.text() << '\n';
        }
        break;
    default:
        std::cerr << "Could not fold expression " << expr.text() << '\n';
    }

    return {};
}

void ir_gen_visitor::append_instruction(ir::three_address && inst) {
    if (current_block() != nullptr) current_block()->contents.push_back(inst);
    else
        std::cerr << "[ " << (current_func != nullptr ? current_func->name : "global")
                  << " ] Cannot add instruction as a block does not exist\n";
}

ir::basic_block * ir_gen_visitor::current_block() {
    if (current_func == nullptr) return nullptr;
    if (current_func->body.empty()) return append_block(current_func->name + "_entry");
    return current_func->body.back().get();
}

ir::basic_block * ir_gen_visitor::append_block(std::string && name) {
    if (current_func != nullptr) {
        current_func->body.push_back(std::make_unique<ir::basic_block>(name));
        return current_func->body.back().get();
    } else {
        std::cerr << "Could not add block " << name << ", as there was no current function.\n";
        return nullptr;
    }
}
void ir_gen_visitor::dump() const {
    prog.for_each_func([](const ir::function * func) {
        if (func != nullptr) {
            std::cout << func->name << ' ';
            {
                bool first = true;
                for (const auto & param : func->parameters()) {
                    if (not first) std::cout << ", ";
                    else
                        first = false;

                    std::cout << param;
                }
            }
            std::cout << " {\n";
            for (const auto & block : func->body) {
                std::cout << block->name << ":\n";
                for (const auto & inst : block->contents) std::cout << '\t' << inst << '\n';
            }
            std::cout << "}\n" << std::endl;
        }
    });
}
std::string ir_gen_visitor::temp_name() { return "temp_" + std::to_string(this->temp_num++); }
std::shared_ptr<ir::type> ir_gen_visitor::type_from(const token & tok) {
    switch (tok.type()) {
    case token_type::Identifier: {
        auto user_specified = prog.lookup_type(std::get<std::string>(tok.get_data()));
        if (user_specified != nullptr) return user_specified;
        else {
            std::cerr << "Could find type named " << tok << '\n';
            return nullptr;
        }
    } break;
    case token_type::Int32:
        return prog.lookup_type("int32");
    case token_type::Int64:
        return prog.lookup_type("int64");
    default:
        std::cerr << "Cannot use " << tok << " as a type\n";
        return nullptr;
    }
}
std::string ir_gen_visitor::block_name() {
    if (current_func == nullptr) return std::to_string(this->block_num++);
    else
        return current_func->name + std::to_string(this->block_num++);
}

namespace {
std::map<std::string, ir::operand> builtins;
}

ir::operand ir_gen_visitor::eval_ast(const ast::expression & expr) {

    if (builtins.empty()) {

        ir::function_type print_type{{prog.lookup_type("i32")}, prog.lookup_type("unit")};

        builtins.emplace(
            "print",
            ir::operand{"print", std::make_shared<ir::function_type>(std::move(print_type)), true});
    }

    switch (expr.type()) {
    case ast::node_type::binary_op: {
        auto & bin = dynamic_cast<const ast::bin_op &>(expr);
        auto lhs = eval_ast(bin.lhs_ref());

        // TODO: Make the ast do type checking
        // TODO: Modularize

        switch (auto bool_type = prog.lookup_type("boolean"); bin.oper()) {
        case ast::operation::add:
            append_instruction(ir::operation ::add,
                               {temp_operand(lhs.type, false), lhs, eval_ast(bin.rhs_ref())});
            break;
        default:
            std::cerr << "Unimplemented operation: " << expr.text() << '\n';
            break;
        case ast::operation::gt:
            append_instruction(ir::operation::gt,
                               {temp_operand(bool_type, false), lhs, eval_ast(bin.rhs_ref())});
            break;
        case ast::operation::le:
            append_instruction(ir::operation ::le,
                               {temp_operand(bool_type, false), lhs, eval_ast(bin.rhs_ref())});
            break;
        case ast::operation::eq:
            append_instruction(ir::operation ::eq,
                               {temp_operand(bool_type, false), lhs, eval_ast(bin.rhs_ref())});
            break;
        case ast::operation::boolean_or:
            if (*lhs.type != ir::ir_type::boolean) return {false, bool_type, false};
            else {
                auto false_block_name = block_name();
                auto true_block_name = block_name();
                auto label_type = prog.lookup_type("string");
                append_instruction(ir::operation::branch, {lhs,
                                                           {true_block_name, label_type, false},
                                                           {false_block_name, label_type, false}});
                append_block(std::move(false_block_name));
                auto rhs_val = eval_ast(bin.rhs_ref());
                append_block(std::move(true_block_name));
                append_instruction(ir::operation::phi,
                                   {temp_operand(bool_type, false), lhs, rhs_val});
            }
            break;
        case ast::operation::sub:
            append_instruction(ir::operation::sub,
                               {temp_operand(lhs.type, false), lhs, eval_ast(bin.rhs_ref())});
            break;
            /*
        case ast::operation::mult:
            break;
        case ast::operation::div:
            break;
        case ast::operation::boolean_and:
            break;
        case ast::operation::lt:
            break;
        case ast::operation::gt:
            break;
        case ast::operation::ge:
            break;
             */
        }
    } break;
    case ast::node_type::func_call: {
        if (current_block() == nullptr) {
            std::cerr << "Found func_call not in a block: " << expr.text() << '\n';
            return {0l, prog.lookup_type("i32"), true};
        }
        auto & call = dynamic_cast<const ast::func_call &>(expr);

        if (call.name() == nullptr) {
            std::cerr << "Name expression of a function call was null\n";
            return {0l, prog.lookup_type("i32"), true};
        }

        std::optional<ir::operand> func_name;
        if (call.name()->type() == ast::node_type::value) {
            auto call_name = call.name()->text();
            // TODO: Should lookup_type(some_function_name) return the type of that function?
            func_name = ir::operand{call_name, prog.lookup_type(call_name), true};
        } else {
            visit(*call.func_name);
            func_name = current_block()->contents.back().result();
        }

        if (not func_name) {
            std::cerr << "Could not get name of function call for " << call.text() << '\n';
            return {0l, prog.lookup_type("i32"), true};
        }

        auto lookup_name = std::get<std::string>(func_name.value().data);

        // Search previous declarations
        if (not prog.function_exists(lookup_name, call.arguments.size())) {
            std::cerr << "Function " << std::get<std::string>(func_name.value().data)
                      << " is not defined\n";
            return {0l, prog.lookup_type("i32"), true};
        }

        auto * callee = prog.lookup_function(lookup_name, call.arguments.size());

        std::vector operands{temp_operand(callee->type.return_type, false), func_name.value()};
        for (auto & arg : call.arguments) { operands.push_back(eval_ast(*arg)); }

        this->append_instruction(ir::operation ::call, std::move(operands));
    } break;
    case ast::node_type::value: {
        auto & value = dynamic_cast<const ast::literal_or_variable &>(expr);
        switch (value.val.type()) {
        case token_type ::Int:
            return {std::get<long>(value.data()), prog.lookup_type("i32"), true};
        case token_type ::Identifier: {
            auto name = std::get<std::string>(value.data());
            if (auto oper = read_variable(name); oper) return oper.value();
            else if (auto iter = builtins.find(name); iter != builtins.end())
                return iter->second;
            else
                std::cerr << "Variable " << value.val << " does not exist\n";
        } break;
        case token_type ::StringLiteral:
            return {std::get<std::string>(value.data()), prog.lookup_type("string"), true};
        default:
            std::cerr << "Cannot get value from " << value.text() << '\n';
        }
    } break;
    default:
        std::cerr << expr.text() << " cannot be evaluated." << std::endl;
        return {0l, prog.lookup_type("i32"), true};
    }
    return current_block()->contents.back().result().value();
}
std::optional<ir::operand> ir_gen_visitor::read_variable(const std::string & name) const {
    for (auto iter = active_variables.rbegin(); iter != active_variables.rend(); ++iter) {
        if (iter->count(name) != 0) return iter->at(name);
    }
    return {};
}
void ir_gen_visitor::eval_if_condition(const ast::expression & expr,
                                       const std::string & true_branch,
                                       const std::string & false_branch) {

    auto label_type = prog.lookup_type("string");
    auto true_operand = ir::operand{true_branch, label_type, false};
    auto false_operand = ir::operand{false_branch, label_type, false};
    switch (expr.type()) {

    case ast::node_type::binary_op:
        switch (auto & bin = dynamic_cast<const ast::bin_op &>(expr); bin.op) {
        case ast::operation::boolean_and: {
            auto lhs = eval_ast(bin.lhs_ref());
            auto short_circuit = block_name();

            auto short_operand = ir::operand{short_circuit, label_type, false};
            append_instruction(ir::operation::branch, {lhs, short_operand, false_operand});

            append_block(std::move(short_circuit));
            auto rhs = eval_ast(bin.rhs_ref());
            append_instruction(ir::operation::branch, {rhs, true_operand, false_operand});
        } break;
        case ast::operation::boolean_or: {
            auto lhs = eval_ast(bin.lhs_ref());
            auto short_circuit = block_name();

            auto short_operand = ir::operand{short_circuit, label_type, false};
            append_instruction(ir::operation::branch, {lhs, true_operand, short_operand});

            append_block(std::move(short_circuit));
            auto rhs = eval_ast(bin.rhs_ref());
            append_instruction(ir::operation::branch, {rhs, true_operand, false_operand});
        } break;
        case ast::operation::le:
        case ast::operation::lt:
        case ast::operation::gt:
        case ast::operation::ge:
        case ast::operation::eq:
            append_instruction(ir::operation::branch,
                               {eval_ast(expr), true_operand, false_operand});
            break;
        default:
            std::cerr << "Unexpected non-boolean binary op in if branch: " << bin.text()
                      << std::endl;
        }
        break;
    case ast::node_type::func_call: {
        auto result = eval_ast(expr);

        if (*result.type != ir::ir_type::boolean) {
            std::cerr << "Function call " << expr.text() << " does not return boolean" << std::endl;
            return;
        }

        append_instruction(ir::operation::branch, {result, true_operand, false_operand});
    } break;
    case ast::node_type::value:
        append_instruction(ir::operation::branch, {eval_ast(expr), true_operand, false_operand});
        break;
    default:
        std::cerr << "Unexpected node as condition of if: " << expr.text() << std::endl;
        return;
    }
}
void ir_gen_visitor::generate_function(const ast::function & func) {

    auto return_type = prog.lookup_type("unit");

    if (auto func_type = func.name.type_data(); func.name.user_explicit()) {
        return_type = type_from(func_type);
        if (return_type == nullptr) {
            std::cerr << "Cannot use " << func_type << " as a return type.\n";
        }
    }

    std::vector<std::shared_ptr<ir::type>> param_types;
    for (auto & param : func.params) {
        auto param_type = type_from(param.val_type);
        if (param_type != nullptr) param_types.push_back(param_type);
        else
            std::cerr << "Could not find type " << param.val_type << '\n';
    }

    ir::function_type func_type{std::move(param_types), return_type};

    ir::function * func_ir = this->prog.register_function(func.identifier(), std::move(func_type));
    this->current_func = func_ir;
    this->append_block(current_func->name + "_entry");
    this->active_variables.emplace_back();

    for (const auto & param : func.params) {
        func_ir->param_names.push_back(std::get<std::string>(param.name.get_data()));
    }

    for (size_t i = 0; i < func.params.size(); i++) {
        auto name = std::get<std::string>(func.params.at(i).name.get_data());
        if (not current_scope().try_emplace(name, func_ir->parameters().at(i)).second) {
            std::cerr << "Duplicate parameter: " << name << '\n';
        }
    }

    visit(*func.body);
    if (not func_ir->body.back()->terminated()) {
        if (func_ir->name == "main")
            // TODO: Return the actual value from main
            append_instruction(ir::operation::halt, {{0, prog.lookup_type("i32"), true}});
        else
            append_instruction(ir::operation ::ret);
    }
    this->active_variables.pop_back();
    this->current_func = nullptr;
}
