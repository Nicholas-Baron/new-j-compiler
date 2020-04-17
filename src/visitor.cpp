//
// Created by nick on 4/17/20.
//

#include "visitor.h"

#include "nodes.h"

#include <iostream>

void printing_visitor::visit(ast::node & node) {
    std::cout << std::string(indent_depth, ' ');
    indent_depth += indent_size;

    switch (node.type()) {
    case ast::node_type::const_decl:
        std::cout << "Const decl: " << node.text() << '\n';
        break;
    case ast::node_type::function:
        std::cout << "Function decl: " << node.text() << '\n';
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
        break;
    case ast::node_type::func_call:
        std::cout << "Function call " << node.text() << '\n';
        break;
    default:
        std::cout << "Unimplemented visit on node " << node.text() << std::endl;
    }

    indent_depth -= indent_size;
}
