//
// Created by nick on 4/17/20.
//

#include "visitor.h"

#include "nodes.h"

#include <iostream>

void printing_visitor::visit(ast::node & node) {

    print_indent();
    indent_depth += indent_size;

    if (node_count == 0) std::cout << std::boolalpha;

    this->node_count++;

    switch (node.type()) {
    case ast::node_type::const_decl:
        std::cout << "Const decl for " << dynamic_cast<ast::const_decl &>(node).identifier()
                  << '\n';
        print_indent();
        std::cout << "Is global: " << dynamic_cast<ast::const_decl &>(node).in_global_scope()
                  << '\n';
        node.accept(*this);
        break;
    case ast::node_type::function:
        std::cout << "Function decl for " << dynamic_cast<ast::function &>(node).identifier()
                  << '\n';
        node.accept(*this);
        break;
    case ast::node_type::parameter:
        std::cout << "Parameter " << node.text() << '\n';
        break;
    case ast::node_type::statement_block:
        std::cout << "Statement block " << node.text() << '\n';
        break;
    case ast::node_type::value:
        std::cout << "Value " << node.text() << '\n';
        break;
    case ast::node_type::opt_typed:
        std::cout << "Optionally typed " << node.text() << '\n';
        print_indent();
        std::cout << "Has user type: " << dynamic_cast<ast::opt_typed &>(node).user_typed() << '\n';
        break;
    case ast::node_type::func_call:
        std::cout << "Function call to " << node.text() << '\n';
        node.accept(*this);
        break;
    default:
        std::cout << "Unimplemented visit on node " << node.text() << std::endl;
    }

    indent_depth -= indent_size;
}

void printing_visitor::print_indent() const { std::cout << std::string(indent_depth, ' '); }
