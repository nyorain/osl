#include "tao/pegtl.hpp"

namespace pegtl = tao::pegtl;

// https://stackoverflow.com/questions/53427551/pegtl-how-to-skip-spaces-for-the-entire-grammar
struct LineComment : pegtl::seq<
		pegtl::sor<
			pegtl::string<'/', '/'>,
			pegtl::one<'#'>>,
		pegtl::until<
			pegtl::eolf,
			pegtl::utf8::any>> {};
struct InlineComment : pegtl::seq<
		pegtl::string<'/', '*'>,
		pegtl::until<
			pegtl::string<'*', '/'>,
			pegtl::utf8::any>> {};

struct Comment : pegtl::disable<pegtl::sor<LineComment, InlineComment>> {};
struct Separator : pegtl::sor<tao::pegtl::ascii::space, Comment> {};
struct Seps : tao::pegtl::star<Separator> {}; // Any separators, whitespace or comments

template<typename S, typename... R> using Interleaved = pegtl::seq<S, pegtl::seq<R, S>...>;

struct Plus : pegtl::one<'+'> {};
struct Minus : pegtl::one<'-'> {};
struct Mult : pegtl::one<'*'> {};
struct Divide : pegtl::one<'/'> {};
struct Comma : pegtl::one<','> {};
struct Semicolon : pegtl::one<';'> {};

struct Identifier : pegtl::plus<pegtl::alpha> {};
struct NumberLiteral : pegtl::plus<pegtl::digit> {};

struct Expr;
struct IfExpr;

struct Assign : Interleaved<Seps,
	Expr,
	pegtl::one<'='>,
	Expr> {};

struct ExprStatement : pegtl::seq<Expr> {};
struct StatementSemi : Interleaved<Seps,
	pegtl::sor<Assign, ExprStatement>,
	Semicolon> {};
struct Statement : pegtl::sor<StatementSemi, IfExpr> {};

struct CodeBlockReturn : pegtl::seq<Expr> {};
struct CodeBlockStatements : pegtl::star<pegtl::pad<Statement, Separator>> {};
struct CodeBlock : Interleaved<Seps,
	pegtl::one<'{'>,
	CodeBlockStatements,
	pegtl::opt<CodeBlockReturn>,
	pegtl::one<'}'>> {};

struct Branch : Interleaved<Seps, Expr, CodeBlock> {};
struct ElseIfBranch : Interleaved<Seps,
	pegtl::keyword<'e', 'l', 's', 'e'>,
	pegtl::keyword<'i', 'f'>,
	Branch
> {};
struct ElseBranch : Interleaved<Seps,
	pegtl::keyword<'e', 'l', 's', 'e'>,
	CodeBlock
> {};
struct ElseIfs : pegtl::star<ElseIfBranch> {};
struct IfExpr : Interleaved<Seps,
	pegtl::keyword<'i', 'f'>,
	Branch,
	ElseIfs,
	pegtl::opt<ElseBranch>
> {};

struct Type : pegtl::seq<Identifier> {};

// function
struct FunctionParameter : Interleaved<Seps,
	Type,
	Identifier> {};
struct Function : Interleaved<Seps,
	// return type
	Type,
	// name
	Identifier,
	// optional paramter list
	pegtl::opt<Interleaved<Seps,
		pegtl::one<'('>,
		pegtl::opt<pegtl::list_tail<FunctionParameter, Comma, Seps>>,
		pegtl::one<')'>>>,
	// body
	CodeBlock> {};

// enum
struct PlainEnumValue : Interleaved<Seps, Identifier> {};
struct ContentEnumValue : Interleaved<Seps,
	Identifier,
	pegtl::one<'('>,
	pegtl::list_tail<Identifier, Comma, Seps>,
	pegtl::one<')'>
> {};
struct EnumValue : pegtl::sor<ContentEnumValue, PlainEnumValue> {};
struct EnumDecl : Interleaved<Seps,
	pegtl::keyword<'e', 'n', 'u', 'm'>,
	Identifier,
	pegtl::one<'{'>,
	pegtl::list_tail<EnumValue, Comma, Seps>,
	pegtl::one<'}'>
> {};

// struct
struct StructMemberInit : Interleaved<Seps,
	pegtl::one<'{'>,
	Expr,
	pegtl::one<'}'>
> {};
struct StructMember : Interleaved<Seps,
	Type,
	Identifier,
	pegtl::opt<StructMemberInit>
> {};
struct StructDecl : Interleaved<Seps,
	pegtl::keyword<'s', 't', 'r', 'u', 'c', 't'>,
	Identifier,
	pegtl::one<'{'>,
	pegtl::list_tail<StructMember, Semicolon, Seps>,
	pegtl::one<'}'>
> {};

// using declarations
struct UsingTypeDecl : Interleaved<Seps,
	pegtl::keyword<'u', 's', 'i', 'n', 'g'>,
	Identifier,
	pegtl::one<'='>,
	Identifier,
	Semicolon
> {};

// expression
struct IdentifierExpr : pegtl::seq<Identifier> {};
struct AtomExpr : pegtl::sor<NumberLiteral, IdentifierExpr> {};

struct ParanthExpr : Interleaved<Seps,
	pegtl::one<'('>,
	Expr,
	pegtl::one<')'>> {};

struct NonUnaryMinusExpr : pegtl::sor<
	ParanthExpr,
	AtomExpr,
	CodeBlock> {};

struct PrimaryExpr : pegtl::sor<
	Interleaved<Seps, pegtl::one<'-'>, NonUnaryMinusExpr>,
	NonUnaryMinusExpr> {};

/*
struct MultPart : Interleaved<Seps, Mult, PrimaryExpr> {};
struct DivPart : Interleaved<Seps, Divide, PrimaryExpr> {};
struct MultExpr : Interleaved<Seps,
	PrimaryExpr,
	pegtl::star<pegtl::sor<MultPart, DivPart>>> {};

struct AddPart : Interleaved<Seps, Plus, MultExpr> {};
struct SubPart : Interleaved<Seps, Minus, MultExpr> {};
struct AddRest : pegtl::star<pegtl::sor<AddPart, SubPart>> {};
struct AddExpr : Interleaved<Seps, MultExpr, AddRest> {};
*/

struct DivExpr : Interleaved<Seps, PrimaryExpr, pegtl::opt<Divide, PrimaryExpr>> {};
struct MultExpr : Interleaved<Seps, DivExpr, pegtl::opt<Mult, DivExpr>> {};
struct SubExpr : Interleaved<Seps, MultExpr, pegtl::opt<Minus, MultExpr>> {};
struct AddExpr : Interleaved<Seps, SubExpr, pegtl::opt<Plus, AddExpr>> {};

struct Expr : pegtl::sor<IfExpr, AddExpr> {};

// Inputs
/*
struct ConstantBufferKeyword : pegtl::keyword<'c', 'o', 'n', 's', 't'> {};
struct BufferInput : Interleaved<Seps,
	pegtl::opt<ConstantBufferKeyword>
*/

struct NamespaceDecl;
struct GlobalDecl : pegtl::sor<Function> {};
struct GlobalDecls : pegtl::star<pegtl::pad<GlobalDecl, Separator>> {};

// namespace
struct NamespaceDecl : Interleaved<Seps,
	pegtl::keyword<'n', 'a', 'm', 'e', 's', 'p', 'a', 'c', 'e'>,
	Identifier,
	pegtl::one<'{'>,
	GlobalDecls,
	pegtl::one<'}'>
> {};

// import
struct ImportDecl : Interleaved<Seps,
	pegtl::keyword<'i', 'm', 'p', 'o', 'r', 't'>,
	Identifier
> {};

// TODO: export

// Module
struct Module : GlobalDecls {};

struct Grammar : pegtl::must<Module, pegtl::eof> {};
