#include "syntax.hpp"
#include "ast.hpp"

#include "tao/pegtl.hpp"
#include "tao/pegtl/contrib/trace.hpp"
#include "tao/pegtl/contrib/analyze.hpp"
#include "tao/pegtl/contrib/parse_tree.hpp"
#include "tao/pegtl/contrib/parse_tree_to_dot.hpp"

#include <cstdio>
#include <fstream>

struct Discard : pegtl::parse_tree::apply<Discard> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr< Node >& n, States&&...) {
		n.reset();
	}
};

struct Fold : pegtl::parse_tree::apply<Fold> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr< Node >& n, States&&...) {
		if(n->children.size() == 1) {
			n = std::move(n->children.front());
		} else if(n->children.empty() && (!n->has_content() || n->string_view().empty())) {
			n.reset();
		}
	}
};

struct FoldDiscard : pegtl::parse_tree::apply<Fold> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr< Node >& n, States&&...) {
		if(n->children.size() == 1) {
			n = std::move(n->children.front());
		} else if(n->children.empty()) {
			n.reset();
		}
	}
};

struct DiscardChildren : pegtl::parse_tree::apply<DiscardChildren> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr<Node>& n, States&&...) {
		n->children.clear();
	}
};

struct ExprSelector : pegtl::parse_tree::apply<DiscardChildren> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr<Node>& n, States&&...) {
		if(n->children.size() == 2) {
			// In that case there are multiple parts to the chained expr
			/*
			if(n->children[1]->children.size() > 1) {
				auto n2 = std::move(n->children[1]);
				n->children.erase(n->children.begin() + 1);

				auto b = std::make_move_iterator(n2->children.begin());
				auto e = std::make_move_iterator(n2->children.end());
				n->children.insert(n->children.end(), b, e);
			}
			*/
		} else if(n->children.size() == 1) {
			n = std::move(n->children.front());
		}
	}
};

struct Keep : pegtl::parse_tree::apply<Keep> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr<Node>& n, States&&...) {
	}
};


template<typename Rule> struct selector : FoldDiscard {};

template<> struct selector<Identifier> : DiscardChildren {};
template<> struct selector<NumberLiteral> : DiscardChildren {};

// template<> struct selector<AddExpr> : ExprSelector {};
// template<> struct selector<MultExpr> : ExprSelector {};

// template<> struct selector<SubPart> : Keep {};
// template<> struct selector<DivPart> : Keep {};

template<> struct selector<CodeBlock> : Keep {};
template<> struct selector<CodeBlockStatements> : Keep {};
template<> struct selector<CodeBlockReturn> : Keep {};
template<> struct selector<Branch> : Keep {};
template<> struct selector<ElseIfBranch> : Keep {};

template<> struct selector<IfExpr> : Keep {};
template<> struct selector<ElseIfs> : Keep {};

template<> struct selector<ExprStatement> : Keep {};

template<> struct selector<Seps> : Discard {};
template<> struct selector<Plus> : Discard {};
template<> struct selector<Minus> : Discard {};
template<> struct selector<Mult> : Discard {};
template<> struct selector<Divide> : Discard {};
template<char c> struct selector<pegtl::one<c>> : Discard {};

class TreeBuilder {
	using ParseTreeNode = tao::pegtl::parse_tree::node;

	struct {
		ast::CodeBlock* codeBlock {};
		ast::Function* function {};
	} current_;

	struct {
		std::vector<std::vector<ast::VariableDeclaration>> vars;
		std::vector<std::vector<ast::Type*>> types;
	} decls_;

	ast::Module module_;

	std::unique_ptr<ast::Statement> parseStatement(const ParseTreeNode& node) {
		if(node.is_type<ExprStatement>()) {
			assert(node.children.size() == 1);
			auto ret = std::make_unique<ast::ExpressionStatement>();
			ret->expr = parseExpr(*node.children[0]);
			return ret;
		} else if(node.is_type<Assign>()) {
			assert(node.children.size() == 2);
			auto ret = std::make_unique<ast::AssignStatement>();
			ret->left = parseExpr(*node.children[0]);
			ret->right = parseExpr(*node.children[1]);
			return ret;
		} else if(node.is_type<IfExpr>()) {
			auto ret = std::make_unique<ast::ExpressionStatement>();
			ret->expr = std::make_unique<ast::IfExpression>(parseIfExpr(node));
			return ret;
		}

		assert(!"Invalid statement type");
	}

	std::unique_ptr<ast::CodeBlock> parseCodeBlock(const ParseTreeNode& node) {
		assert(node.children.size() >= 1 & node.children.size() <= 2);
		assert(node.is_type<CodeBlock>());

		auto ret = std::make_unique<ast::CodeBlock>();
		auto& statements = *node.children[0];
		assert(statements.is_type<CodeBlockStatements>());
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
		assert(node.children.size() >= 2);
		auto ret = std::make_unique<ast::OpExpression>();
		ret->opType = op;
		for(auto& child : node.children) {
			ret->children.push_back(parseExpr(*child));
		}

		return ret;
	}

	std::unique_ptr<ast::Expression> parseExpr(const ParseTreeNode& node) {
		if(node.is_type<IfExpr>()) {
			return std::make_unique<ast::IfExpression>(parseIfExpr(node));
		} else if(node.is_type<AddExpr>()) {
			return parseOpExpr(node, ast::OpExpression::OpType::add);
		} else if(node.is_type<MultExpr>()) {
			return parseOpExpr(node, ast::OpExpression::OpType::mult);
		}

		// TODO

		// unkown expression type!
		assert(!"Invalid expression type");
	}

	ast::IfExpression::Branch parseBranch(const ParseTreeNode& node) {
		ast::IfExpression::Branch ret;
		assert(node.children.size() == 2);
		assert(node.is_type<Branch>());
		ret.condition = parseExpr(*node.children[0]);
		ret.code = parseCodeBlock(*node.children[1]);
		return ret;
	}

	ast::IfExpression parseIfExpr(const ParseTreeNode& node) {
		ast::IfExpression ret;
		assert(node.children.size() >= 2 && node.children.size() <= 3);
		assert(node.is_type<IfExpr>());

		ret.ifBranch = parseBranch(*node.children[0]);
		for(auto& elseif : node.children[1]->children) {
			ret.elsifBranches.push_back(parseBranch(*elseif));
		}

		if(node.children.size() == 3) {
			ret.elseBranch = parseCodeBlock(*node.children[2]);
		}

		return ret;
	}
};


int main(int argc, char** argv) {
	if(argc < 2) {
		std::printf("No argument given\n");
		return 1;
	}

	if(pegtl::analyze<Statement>() != 0) {
		std::printf("cycles without progress detected!\n");
		return 2;
   }

	pegtl::file_input in(argv[1]);
	auto root = tao::pegtl::parse_tree::parse<pegtl::must<Expr, pegtl::eof>, selector>(in);
	if(root) {
		auto of = std::ofstream("test.dot");
		pegtl::parse_tree::print_dot(of, *root);
	} else {
		std::printf("Parsing failed\n");
	}
}
