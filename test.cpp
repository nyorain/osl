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

struct FoldDiscard : pegtl::parse_tree::apply<FoldDiscard> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr< Node >& n, States&&...) {
		if(n->children.size() == 1) {
			n = std::move(n->children.front());
		} else if(n->children.empty()) {
			n.reset();
		}
	}
};

struct ConsumeFirst : pegtl::parse_tree::apply<ConsumeFirst> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr<Node>& n, States&&...) {
		auto nc = std::move(n->children[0]->children);
		n->children = std::move(nc);
	}
};

struct DiscardChildren : pegtl::parse_tree::apply<DiscardChildren> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr<Node>& n, States&&...) {
		n->children.clear();
	}
};

template<bool Fold = true>
struct ChainSelector : pegtl::parse_tree::apply<DiscardChildren> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr<Node>& n, States&&...) {
		assert(n->children.size() == 2);

		if(n->children[1]->children.empty() && Fold) {
			// no chain present
			n = std::move(n->children.front());
		} else {
			auto n2 = std::move(n->children[1]);
			n->children.erase(n->children.begin() + 1);

			auto b = std::make_move_iterator(n2->children.begin());
			auto e = std::make_move_iterator(n2->children.end());
			n->children.insert(n->children.end(), b, e);
		}

		/*
		if(n->children.size() == 2) {
			// In that case there are multiple parts to the chained expr
			if(n->children[1]->children.size() > 1) {
				auto n2 = std::move(n->children[1]);
				n->children.erase(n->children.begin() + 1);

				auto b = std::make_move_iterator(n2->children.begin());
				auto e = std::make_move_iterator(n2->children.end());
				n->children.insert(n->children.end(), b, e);
			}
		} else if(n->children.size() == 1) {
			n = std::move(n->children.front());
		}
		*/
	}
};

struct Keep : pegtl::parse_tree::apply<Keep> {
	template<typename Node, typename... States>
	static void transform(std::unique_ptr<Node>&, States&&...) {
	}
};


template<typename Rule> struct selector : FoldDiscard {};

template<> struct selector<syn::Identifier> : DiscardChildren {};

template<> struct selector<syn::CodeBlock> : ConsumeFirst {};
template<> struct selector<syn::CodeBlockStatements> : Keep {};
template<> struct selector<syn::CodeBlockReturn> : Keep {};
template<> struct selector<syn::Branch> : Keep {};
template<> struct selector<syn::ElseIfBranch> : Keep {};

template<> struct selector<syn::IfExpr> : ConsumeFirst {};
template<> struct selector<syn::ElseIfs> : Keep {};

template<> struct selector<syn::ExprStatement> : Keep {};

template<> struct selector<syn::TrueLiteral> : Keep {};
template<> struct selector<syn::FalseLiteral> : Keep {};
template<> struct selector<syn::SuffixI32> : Keep {};
template<> struct selector<syn::SuffixU32> : Keep {};
template<> struct selector<syn::SuffixF64> : Keep {};
template<> struct selector<syn::SuffixF32OrEmpty> : Keep {};
template<> struct selector<syn::SuffixedNumberLiteral> : Keep {};
template<> struct selector<syn::FNumber> : Keep {};
template<> struct selector<syn::DNumber> : Keep {};

template<> struct selector<syn::FunctionParameterList> : Keep {};
template<> struct selector<syn::FunctionArgsList> : Keep {};

// template<> struct selector<syn::FunctionArgLists> : Keep {};
// template<> struct selector<syn::MemberAccessors> : Keep {};
template<> struct selector<syn::AddRest> : Keep {};
template<> struct selector<syn::MultRest> : Keep {};
template<> struct selector<syn::DivRest> : Keep {};
template<> struct selector<syn::SubRest> : Keep {};
template<> struct selector<syn::MemberFunctionChainLinks> : Keep {};

template<> struct selector<syn::AddExpr> : ChainSelector<> {};
template<> struct selector<syn::SubExpr> : ChainSelector<> {};
template<> struct selector<syn::DivExpr> : ChainSelector<> {};
template<> struct selector<syn::MultExpr> : ChainSelector<> {};
// template<> struct selector<syn::MemberAccessChain> : ChainSelector<> {};
// template<> struct selector<syn::FunctionCall> : ChainSelector<> {};
// template<> struct selector<syn::MemberAccessor> : ChainSelector<false> {};
template<> struct selector<syn::MemberFunctionChain> : ChainSelector<> {};

template<> struct selector<syn::Seps> : Discard {};
template<> struct selector<syn::Plus> : Discard {};
template<> struct selector<syn::Minus> : Discard {};
template<> struct selector<syn::Mult> : Discard {};
template<> struct selector<syn::Divide> : Discard {};
template<char c> struct selector<pegtl::one<c>> : Discard {};

// errors
template<typename> inline constexpr const char* error_message = nullptr;
template<> inline constexpr const char* error_message<syn::AddPart> = "Expected expression after '+'";
template<> inline constexpr const char* error_message<syn::MultPart> = "Expected expression after '*'";
template<> inline constexpr const char* error_message<syn::DivPart> = "Expected expression after '/'";
template<> inline constexpr const char* error_message<syn::SubPart> = "Expected expression after '-'";
template<> inline constexpr const char* error_message<syn::ParanthExprClose> = "Closing ')' after expression is missing";
template<> inline constexpr const char* error_message<syn::CodeBlockClose> = "Closing '}' after code block is missing";
template<> inline constexpr const char* error_message<syn::Eof> = "Expected end of file";
template<> inline constexpr const char* error_message<syn::Branch> = "Expected branch (condition and codeblock)";
template<> inline constexpr const char* error_message<syn::ElseCodeBlock> = "Expected codeblock after 'else'";
template<> inline constexpr const char* error_message<syn::Expr> = "Expected expression";
template<> inline constexpr const char* error_message<syn::MemberAccessor> = "Expected member-access expression after '.'";
template<> inline constexpr const char* error_message<syn::FunctionArgsListClose> = "Expected ')' to close function arguments list";

template<> inline constexpr const char* error_message<syn::FunctionArgsList> = "Expected expression as function parameter"; // can't fail i guess?
template<> inline constexpr const char* error_message<syn::CodeBlockStatements> = "Expected statement"; // can't fail I guess?
template<> inline constexpr const char* error_message<syn::OptCodeBlockReturn> = "Expected (optional) code block return"; // can't fail I guess?
template<> inline constexpr const char* error_message<syn::Seps> = "Unexpected parser error: Expected separator"; // can't fail I guess?

struct error {
	template<typename Rule> static constexpr auto message = error_message<Rule>;

	// template<typename Rule> static constexpr auto message =
	// 	error_message<Rule> ? error_message<Rule> : tao::demangle<Rule>().data();
};

template<typename Rule> using control = tao::pegtl::must_if<error>::control<Rule>;

std::string readFile(std::string_view filename) {
	auto openmode = std::ios::openmode(std::ios::ate);

	std::ifstream ifs(std::string{filename}, openmode);
	ifs.exceptions(std::ostream::failbit | std::ostream::badbit);

	auto size = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::string buffer;
	buffer.resize(size);
	ifs.read(buffer.data(), size);

	return buffer;
}

int main(int argc, char** argv) {
	if(argc < 2) {
		std::printf("No argument given\n");
		return 1;
	}

	if(pegtl::analyze<syn::Grammar>() != 0) {
		std::printf("cycles without progress detected!\n");
		return 2;
   }

	// pegtl::file_input in(argv[1]);

	auto file = readFile(argv[1]);
	pegtl::memory_input in(file, argv[1]);
	// pegtl::standard_trace<syn::Grammar>(in);

	using Grammar = pegtl::must<syn::Expr, syn::Eof>;
	try {
		auto root = tao::pegtl::parse_tree::parse<Grammar, selector, pegtl::nothing, control>(in);
		auto of = std::ofstream("test.dot");
		pegtl::parse_tree::print_dot(of, *root);
	} catch(const pegtl::parse_error& error) {
		auto& pos = error.positions()[0];
		auto msg = error.message();
		std::cout << pos.source << ":" << pos.line << ":" << pos.column << ": " << msg << "\n";

		/*
		auto src = std::string_view(file);
		auto start = std::max(int(pos.byte) - int(pos.column), 0);
		src = src.substr(start);

		auto nl = src.find('\n');
		if(nl != src.npos) {
			src = src.substr(0, nl);
		}
		*/

		// TODO: make it work with tabs
		auto line = in.line_at(pos);
		auto tabCount = std::count(line.begin(), line.end(), '\t');
		std::cout << line << "\n";

		// hard to say what tab size is... eh. Maybe just replace it?
		auto col = pos.column + tabCount * (4 - 1);
		for(auto i = 1u; i < col; ++i) {
			std::cout << " ";
		}

		std::cout << "^\n";
	}
}
