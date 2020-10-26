#pragma once

#include "syntax.hpp"
#include "ast.hpp"
#include "tao/pegtl/contrib/parse_tree.hpp"

namespace builder {

class TreeBuilder {
	using ParseTreeNode = tao::pegtl::parse_tree::node;
	using VariableMap = std::unordered_map<std::string_view, ast::VariableDeclaration*>;

	struct {
		ast::CodeBlock* codeBlock {};
		ast::Function* function {};
	} current_;

	struct {
		std::vector<VariableMap> vars;
		std::vector<std::vector<ast::Type*>> types;
	} decls_;

	ast::Module module_;

	std::unique_ptr<ast::Statement> parseStatement(const ParseTreeNode& node) {
		if(node.is_type<syn::ExprStatement>()) {
			assert(node.children.size() == 1);
			auto ret = std::make_unique<ast::ExpressionStatement>();
			ret->expr = parseExpr(*node.children[0]);
			return ret;
		} else if(node.is_type<syn::Assign>()) {
			assert(node.children.size() == 2);
			auto ret = std::make_unique<ast::AssignStatement>();
			ret->left = parseExpr(*node.children[0]);
			ret->right = parseExpr(*node.children[1]);
			return ret;
		} else if(node.is_type<syn::IfExpr>()) {
			auto ret = std::make_unique<ast::ExpressionStatement>();
			ret->expr = std::make_unique<ast::IfExpression>(parseIfExpr(node));
			return ret;
		}

		assert(!"Invalid statement type");
	}

	std::unique_ptr<ast::CodeBlock> parseCodeBlock(const ParseTreeNode& node) {
		assert(node.children.size() >= 1 & node.children.size() <= 2);
		assert(node.is_type<syn::CodeBlock>());

		auto ret = std::make_unique<ast::CodeBlock>();
		auto& statements = *node.children[0];
		assert(statements.is_type<syn::CodeBlockStatements>());
		for(auto& statement : statements.children) {
			ret->statements.emplace_back(parseStatement(*statement));
		}

		if(node.children.size() > 1) {
			auto& retexpr = node.children[1];
			ret->ret = parseExpr(*retexpr);
		}

		return ret;
	}

	std::unique_ptr<ast::Expression> parseOpExpr(const ParseTreeNode& node,
			ast::OpExpression::OpType op) {
		assert(node.children.size() == 2);
		auto ret = std::make_unique<ast::OpExpression>();
		ret->opType = op;
		ret->left = parseExpr(*node.children[0]);
		ret->right = parseExpr(*node.children[0]);
		return ret;
	}

	std::unique_ptr<ast::Expression> parseExpr(const ParseTreeNode& node) {
		if(node.is_type<syn::IfExpr>()) {
			return std::make_unique<ast::IfExpression>(parseIfExpr(node));
		} else if(node.is_type<syn::AddExpr>()) {
			return parseOpExpr(node, ast::OpExpression::OpType::add);
		} else if(node.is_type<syn::MultExpr>()) {
			return parseOpExpr(node, ast::OpExpression::OpType::mult);
		} else if(node.is_type<syn::SubExpr>()) {
			return parseOpExpr(node, ast::OpExpression::OpType::sub);
		} else if(node.is_type<syn::DivExpr>()) {
			return parseOpExpr(node, ast::OpExpression::OpType::div);
		} else if(node.is_type<syn::TrueLiteral>()) {
			auto ret = std::make_unique<ast::LiteralImpl<bool>>();
			ret->value = true;
			return ret;
		} else if(node.is_type<syn::FalseLiteral>()) {
			auto ret = std::make_unique<ast::LiteralImpl<bool>>();
			ret->value = false;
			return ret;
		} else if(node.is_type<syn::SuffixedNumberLiteral>()) {
			assert(node.children.size() == 2);
			auto& value = node.children[0];
			auto& suffix = node.children[1];

			if(suffix->is_type<syn::SuffixF32OrEmpty>()) {
				auto ret = std::make_unique<ast::LiteralImpl<ast::f32>>();
				ret->value = std::stof(value->string());
				return ret;
			} else if(suffix->is_type<syn::SuffixF64>()) {
				auto ret = std::make_unique<ast::LiteralImpl<ast::f64>>();
				ret->value = std::stod(value->string());
				return ret;
			} else if(suffix->is_type<syn::SuffixI32>()) {
				assert(!value->is_type<syn::FNumber>());
				auto ret = std::make_unique<ast::LiteralImpl<ast::i32>>();
				ret->value = std::stol(value->string());
				return ret;
			} else if(suffix->is_type<syn::SuffixU32>()) {
				assert(!value->is_type<syn::FNumber>());
				auto ret = std::make_unique<ast::LiteralImpl<ast::u32>>();
				ret->value = std::stol(value->string());
				return ret;
			} else {
				assert(!"Invalid suffix");
			}
		} else if(node.is_type<syn::IdentifierExpr>()) {
			auto name = node.string_view();
			auto& vars = decls_.vars.back();
			auto it = vars.find(name);
			if(it == vars.end()) {
				assert(!"Invalid identifier");
			}

			auto ret = std::make_unique<ast::IdentifierExpression>();
			ret->decl = it->second;
			return ret;
		} else if(node.is_type<syn::CodeBlock>()) {
			return parseCodeBlock(node);
		} else if(node.is_type<syn::FunctionCall>()) {
			auto ret = std::make_unique<ast::FunctionCall>();
			assert(node.children.size() == 2);

			std::vector<const ast::Type*> argTypes;
			for(auto& param : node.children[1]->children) {
				auto& p = ret->arguments.emplace_back(parseExpr(*param));
				argTypes.push_back(&p->type());
			}

			auto cname = node.children[0]->string_view();
			ret->func = findCallable(cname, argTypes);
			assert(ret->func);

			return ret;
		}

		// unkown expression type!
		assert(!"Invalid expression type");
	}

	const ast::Callable* findCallable(std::string_view name,
			const std::vector<const ast::Type*>& params) {
		// TODO
	}

	ast::IfExpression::Branch parseBranch(const ParseTreeNode& node) {
		ast::IfExpression::Branch ret;
		assert(node.children.size() == 2);
		assert(node.is_type<syn::Branch>());
		ret.condition = parseExpr(*node.children[0]);
		ret.code = parseCodeBlock(*node.children[1]);
		return ret;
	}

	ast::IfExpression parseIfExpr(const ParseTreeNode& node) {
		ast::IfExpression ret;
		assert(node.children.size() >= 2 && node.children.size() <= 3);
		assert(node.is_type<syn::IfExpr>());

		ret.ifBranch = parseBranch(*node.children[0]);
		for(auto& elseif : node.children[1]->children) {
			ret.elsifBranches.push_back(parseBranch(*elseif));
		}

		if(node.children.size() == 3) {
			ret.elseBranch = parseCodeBlock(*node.children[2]);
		}

		return ret;
	}

	const ast::Type& parseType(const ParseTreeNode& node) {
		// TODO
	}

	void addFunction(const ParseTreeNode& node) {
		assert(node.children.size() == 4);
		assert(node.children[0]->is_type<syn::Identifier>());
		assert(node.children[1]->is_type<syn::Identifier>());
		assert(node.children[2]->is_type<syn::FunctionParameterList>());
		assert(node.children[3]->is_type<syn::CodeBlock>());

		auto func = std::make_unique<ast::Function>();

		func->retType = &parseType(*node.children[0]);
		func->ident.name = node.children[1]->string();
		func->code = parseCodeBlock(*node.children[3]);

		for(auto& child : node.children[2]->children) {
			assert(child->children.size() == 2);
			assert(child->children[0]->is_type<syn::Identifier>());
			assert(child->children[1]->is_type<syn::Identifier>());

			auto& param = func->params.emplace_back();
			param.type = &parseType(*child->children[0]);
			param.ident.name = child->children[1]->string();
		}

		module_.functions.emplace_back(std::move(func));
	}

	void addStruct(const ParseTreeNode& node) {
	}

	void addEnum(const ParseTreeNode& node) {
	}

	void parseModule(const ParseTreeNode& module) {
		for(auto& child : module.children) {
			if(child->is_type<syn::FunctionDecl>()) {
				addFunction(*child);
			} else if(child->is_type<syn::StructDecl>()) {
				addStruct(*child);
			} else if(child->is_type<syn::EnumDecl>()) {
				addEnum(*child);
			}
		}
	}
};

} // namespace ast
