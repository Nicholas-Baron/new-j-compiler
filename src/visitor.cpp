//
// Created by nick on 4/17/20.
//

#include "visitor.h"

#include "nodes.h"

#include <iostream>

void printing_visitor::visit(const ast::node & node) {

    print_indent();
    indent_depth += indent_size;

    if (node_count == 0) std::cout << std::boolalpha;

    this->node_count++;

    switch (node.type()) {
    case ast::node_type::var_decl: {
        auto & decl = dynamic_cast<const ast::var_decl &>(node);
        std::cout << "Const decl for " << decl.identifier() << '\n';
        print_indent();
        std::cout << "Is global: " << decl.in_global_scope() << '\n';
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
        std::cout << "Has user type: " << dynamic_cast<const ast::opt_typed &>(node).user_typed()
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
    default:
        std::cout << "Unimplemented visit on node " << node.text() << std::endl;
    }

    indent_depth -= indent_size;
}

void printing_visitor::print_indent() const { std::cout << std::string(indent_depth, ' '); }

void ir_gen_visitor::visit(const ast::node & node) {
    switch (node.type()) {
    case ast::node_type ::var_decl:
        if (auto * decl = dynamic_cast<const ast::var_decl *>(&node); decl != nullptr) {
            auto id = decl->identifier();

            auto value = this->fold_to_constant(*decl->value_expr());
            if (not value) {
                std::cerr << "Could not evaluate the constant " << decl->identifier() << '\n';
                return;
            }

            if (decl->in_global_scope()) {
                // Is a global

                // Check that we are not redeclaring the value
                auto & globals = this->global_scope();
                if (globals.count(id) != 0) {
                    std::cerr << "Redeclaring the global constant " << decl->identifier() << '\n';
                    return;
                }

                globals.try_emplace(id, std::move(*value));
            } else {
                // Some local
                auto & locals = this->current_scope();
                if (locals.count(id) != 0) {
                    std::cerr << "Redeclaring the local constant " << decl->identifier() << '\n';
                    return;
                }

                locals.try_emplace(id, std::move(*value));
            }
        } else {
            std::cerr << "There is a node where the node.type() is var_decl, but is not an "
                         "ast::var_decl\n";
        }
        break;
    case ast::node_type ::function: {
        auto & func = dynamic_cast<const ast::function &>(node);
        auto * func_ir = this->prog.register_function(func.identifier(), func.params.size());
        this->current_func = func_ir;
        this->append_block(current_func->name + "_entry");
        this->active_variables.emplace_back();
        visit(func.name);
        for (const auto & param : func.params) visit(param);
        visit(*func.body);
        if (not func_ir->body.back()->terminated()) append_instruction({ir::operation ::ret, {}});
        this->active_variables.pop_back();
        this->current_func = nullptr;
    } break;
    case ast::node_type::opt_typed: {
        auto & name = dynamic_cast<const ast::opt_typed &>(node);
        if (current_func != nullptr and name.name() == current_func->name
            and current_func->return_type == ir::ir_type::unit and name.user_typed()) {
            auto type = this->type_from(name.type_data());
            if (type) current_func->return_type = type.value();
            else
                std::cerr << "Could not get explicit type for function " << name.name() << '\n';
        } else if (name.user_typed())
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

        append_instruction({ir::operation ::call, args});
    } break;
    case ast::node_type ::parameter: {
        auto & param = dynamic_cast<const ast::parameter &>(node);
        auto type = this->type_from(param.val_type);
        if (type) {
            auto name = std::get<std::string>(param.name.get_data());
            auto arg = ir::operand{name, type.value(), false};
            this->current_func->parameters.push_back(arg);
            if (not current_scope().try_emplace(name, arg).second) {
                std::cerr << "Parameter " << name << " not unique?\n";
            }
        } else
            std::cerr << "Could not get type for parameter " << param.name << '\n';
    } break;
    case ast::node_type::if_statement: {
        auto & if_stmt = dynamic_cast<const ast::if_stmt &>(node);
        auto to_not_jmp = eval_ast(*if_stmt.cond);

        auto then_block_name = this->block_name();
        auto exit_name = this->block_name();

        append_instruction({ir::operation::branch,
                            {to_not_jmp,
                             {then_block_name, ir::ir_type ::str, false},
                             {exit_name, ir::ir_type ::str, false}}});

        append_block(std::move(then_block_name));
        visit(*if_stmt.then_block);

        if (if_stmt.else_block == nullptr and not current_block()->terminated()) {
            append_instruction({ir::operation ::branch, {{exit_name, ir::ir_type ::str, false}}});
            append_block(std::move(exit_name));
        } else if (if_stmt.else_block != nullptr) {
            auto real_exit_name = block_name();
            if (not current_block()->terminated())
                append_instruction(
                    {ir::operation ::branch, {{real_exit_name, ir::ir_type ::str, false}}});
            append_block(std::move(exit_name));
            visit(*if_stmt.else_block);
            if (not current_block()->terminated()) {
                append_instruction(
                    {ir::operation ::branch, {{real_exit_name, ir::ir_type ::str, false}}});
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
        append_instruction({ir::operation ::ret, return_value});
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
            return std::optional{ir::operand{std::get<2>(value), ir::ir_type::i64, true}};

        case 0: // monostate
        default:
            std::cerr << "Unknown immediate data: "
                      << dynamic_cast<ast::literal_or_variable &>(expr)
                      << " (Index number: " << value.index() << ")\n";
        }
        break;
    case ast::node_type::binary_op:
        switch (auto & bin_op = dynamic_cast<ast::bin_op &>(expr); bin_op.oper()) {
        case ast::bin_op::operation ::add: {
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
                                ir::ir_type ::i64, false}};
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
                for (const auto & param : func->parameters) {
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
std::optional<ir::ir_type> ir_gen_visitor::type_from(const token & tok) {
    switch (tok.type()) {
    case token_type ::Int32:
        return {ir::ir_type ::i32};
    case token_type ::Int64:
        return {ir::ir_type ::i64};
    case token_type ::Struct:
        std::cerr << "Currently compiler does not support user-defined types\n";
        return {};
    default:
        std::cerr << "Could not get type from " << tok << '\n';
        return {};
    }
}
std::string ir_gen_visitor::block_name() {
    if (current_func == nullptr) return std::to_string(this->block_num++);
    else
        return current_func->name + std::to_string(this->block_num++);
}

ir::operand ir_gen_visitor::eval_ast(const ast::expression & expr) {
    static std::map<std::string, ir::operand> builtins{
        {"print", {std::string{"print"}, ir::ir_type ::func, true}}};

    switch (expr.type()) {
    case ast::node_type::binary_op: {
        auto & bin = dynamic_cast<const ast::bin_op &>(expr);
        auto lhs = eval_ast(bin.lhs_ref());

        // TODO: Make the ast do type checking
        // TODO: Modularize

        switch (bin.oper()) {
        case ast::bin_op::operation::add:
            append_instruction({ir::operation ::add,
                                {{temp_name(), lhs.type, false}, lhs, eval_ast(bin.rhs_ref())}});
            break;
        default:
            std::cerr << "Unimplemented operation: " << expr.text() << '\n';
            break;
        case ast::bin_op::operation::le:
            append_instruction(
                {ir::operation ::le,
                 {{temp_name(), ir::ir_type ::boolean, false}, lhs, eval_ast(bin.rhs_ref())}});
            break;
        case ast::bin_op::operation::eq:
            append_instruction(
                {ir::operation ::eq,
                 {{temp_name(), ir::ir_type ::boolean, false}, lhs, eval_ast(bin.rhs_ref())}});
            break;
        case ast::bin_op::operation::boolean_or:
            if (lhs.type != ir::ir_type::boolean) return {false, ir::ir_type ::boolean, false};
            else {
                auto false_block_name = block_name();
                auto true_block_name = block_name();
                append_instruction({ir::operation::branch,
                                    {lhs,
                                     {true_block_name, ir::ir_type::str, false},
                                     {false_block_name, ir::ir_type::str, false}}});
                append_block(std::move(false_block_name));
                auto rhs_val = eval_ast(bin.rhs_ref());
                append_block(std::move(true_block_name));
                append_instruction({ir::operation::phi,
                                    {{temp_name(), ir::ir_type::boolean, false}, lhs, rhs_val}});
            }
            break;
        case ast::bin_op::operation::sub:
            append_instruction({ir::operation::sub,
                                {{temp_name(), lhs.type, false}, lhs, eval_ast(bin.rhs_ref())}});
            break;
            /*
        case ast::bin_op::operation::mult:
            break;
        case ast::bin_op::operation::div:
            break;
        case ast::bin_op::operation::boolean_and:
            break;
        case ast::bin_op::operation::lt:
            break;
        case ast::bin_op::operation::gt:
            break;
        case ast::bin_op::operation::ge:
            break;
             */
        }
    } break;
    case ast::node_type::func_call: {
        if (current_block() == nullptr) {
            std::cerr << "Found func_call not in a block: " << expr.text() << '\n';
            return {0l, ir::ir_type ::i32, true};
        }
        auto & call = dynamic_cast<const ast::func_call &>(expr);

        if (call.name() == nullptr) {
            std::cerr << "Name expression of a function call was null\n";
            return {0l, ir::ir_type ::i32, true};
        }

        std::optional<ir::operand> func_name;
        if (call.name()->type() == ast::node_type::value) {
            func_name = ir::operand{call.name()->text(), ir::ir_type ::func, true};
        } else {
            visit(*call.func_name);
            func_name = current_block()->contents.back().result();
        }

        if (not func_name) {
            std::cerr << "Could not get name of function call for " << call.text() << '\n';
            return {0l, ir::ir_type ::i32, true};
        }

        auto lookup_name = std::get<std::string>(func_name.value().data);

        // Search previous declarations
        if (not prog.function_exists(lookup_name, call.arguments.size())) {
            std::cerr << "Function " << std::get<std::string>(func_name.value().data)
                      << " is not defined\n";
            return {0l, ir::ir_type ::i32, true};
        }

        auto * callee = prog.lookup_function(lookup_name, call.arguments.size());

        std::vector operands{ir::operand{temp_name(), callee->return_type, false},
                             func_name.value()};
        for (auto & arg : call.arguments) { operands.push_back(eval_ast(*arg)); }

        this->append_instruction({ir::operation ::call, operands});
    } break;
    case ast::node_type::value: {
        auto & value = dynamic_cast<const ast::literal_or_variable &>(expr);
        switch (value.val.type()) {
        case token_type ::Int:
            return {std::get<long>(value.data()), ir::ir_type ::i32, true};
        case token_type ::Identifier: {
            auto name = std::get<std::string>(value.data());
            if (auto oper = read_variable(name); oper) return oper.value();
            else if (auto iter = builtins.find(name); iter != builtins.end())
                return iter->second;
            else
                std::cerr << "Variable " << value.val << " does not exist\n";
        } break;
        case token_type ::StringLiteral:
            return {std::get<std::string>(value.data()), ir::ir_type ::str, true};
        default:
            std::cerr << "Cannot get value from " << value.text() << '\n';
        }
    } break;
    default:
        std::cerr << expr.text() << " cannot be evaluated." << std::endl;
        return {0l, ir::ir_type ::i32, true};
    }
    return current_block()->contents.back().result().value();
}
std::optional<ir::operand> ir_gen_visitor::read_variable(const std::string & name) const {
    for (auto iter = active_variables.rbegin(); iter != active_variables.rend(); ++iter) {
        if (iter->count(name) != 0) return iter->at(name);
    }
    return {};
}
