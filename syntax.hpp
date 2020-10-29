#include "tao/pegtl.hpp"

namespace pegtl = tao::pegtl;

namespace syn {

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
template<typename S, typename... R> using IfMustSep =
	pegtl::if_must<pegtl::pad<S, Separator>, Seps, R...>;

struct Plus : pegtl::one<'+'> {};
struct Minus : pegtl::one<'-'> {};
struct Mult : pegtl::one<'*'> {};
struct Divide : pegtl::one<'/'> {};
struct Comma : pegtl::one<','> {};
struct Dot : pegtl::one<'.'> {};
struct Semicolon : pegtl::one<';'> {};


struct Identifier : pegtl::plus<pegtl::alpha> {};

// Literals
struct TrueLiteral : pegtl::keyword<'t', 'r', 'u', 'e'> {};
struct FalseLiteral : pegtl::keyword<'f', 'a', 'l', 's', 'e'> {};
struct BooleanLiteral : pegtl::sor<TrueLiteral, FalseLiteral> {};

// Number literals
// Default decimal suffix: 32-bit integer
struct SuffixI32 : pegtl::sor<
	pegtl::string<'i'>,
	pegtl::string<'i', '3', '2'>
> {};
struct SuffixU32 : pegtl::sor<
	pegtl::string<'u'>,
	pegtl::string<'u', '3', '2'>
> {};
// Default float suffix
struct SuffixF32 : pegtl::sor<
	pegtl::string<'f'>,
	pegtl::string<'f', '3', '2'>
> {};
struct SuffixF64 : pegtl::string<'f', '6', '4'> {};

// no-suffix number if f32
struct SuffixF32OrEmpty : pegtl::opt<SuffixF32> {};

struct OptNumberLiteralSuffix : pegtl::sor<
	SuffixI32,
	SuffixU32,
	SuffixF64,
	SuffixF32OrEmpty
> {};

// TODO: support 1.0e5 notation
struct Number : pegtl::plus<pegtl::digit> {};
struct FNumber : pegtl::seq<Number, Dot, Number> {};
struct DNumber : pegtl::seq<Number> {};

struct SuffixedNumberLiteral : pegtl::seq<
	pegtl::sor<FNumber, DNumber>,
	OptNumberLiteralSuffix
> {};

struct Literal : pegtl::sor<
	BooleanLiteral,
	SuffixedNumberLiteral
> {};

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
struct OptCodeBlockReturn : pegtl::opt<CodeBlockReturn> {};
struct CodeBlockStatements : pegtl::star<pegtl::pad<Statement, Separator>> {};
struct CodeBlockClose : pegtl::one<'}'> {};
struct CodeBlock : pegtl::if_must<pegtl::one<'{'>,
	Seps,
	CodeBlockStatements,
	Seps,
	OptCodeBlockReturn,
	Seps,
	CodeBlockClose
> {};

struct ElseKeyword : pegtl::keyword<'e', 'l', 's', 'e'> {};
struct ElseIfKeyword : Interleaved<Seps,
	pegtl::keyword<'e', 'l', 's', 'e'>,
	pegtl::keyword<'i', 'f'>
> {};
struct Branch : Interleaved<Seps, Expr, CodeBlock> {};
struct ElseIfBranch : pegtl::if_must<ElseIfKeyword, Seps, Branch> {};
struct ElseCodeBlock : CodeBlock {};
struct ElseBranch : pegtl::if_must<ElseKeyword, Seps, ElseCodeBlock> {};
struct ElseIfs : pegtl::star<ElseIfBranch> {};
struct IfExpr : Interleaved<Seps,
	pegtl::if_must<pegtl::keyword<'i', 'f'>, Seps, Branch>,
	ElseIfs,
	pegtl::opt<ElseBranch>
> {};

struct Type : pegtl::seq<Identifier> {};

// function
struct FunctionParameter : Interleaved<Seps,
	Type,
	Identifier> {};
struct FunctionParameterList : pegtl::opt<Interleaved<Seps,
	pegtl::one<'('>,
	pegtl::opt<pegtl::list_tail<FunctionParameter, Comma, Separator>>,
	pegtl::one<')'>>
> {};
struct FunctionDecl : Interleaved<Seps,
	// return type
	Type,
	// name
	Identifier,
	// optional paramter list
	FunctionParameterList,
	// body
	CodeBlock> {};

// enum
struct PlainEnumValue : Interleaved<Seps, Identifier> {};
struct ContentEnumValue : Interleaved<Seps,
	Identifier,
	pegtl::one<'('>,
	pegtl::list_tail<Identifier, Comma, Separator>,
	pegtl::one<')'>
> {};
struct EnumValue : pegtl::sor<ContentEnumValue, PlainEnumValue> {};
struct EnumDecl : Interleaved<Seps,
	pegtl::keyword<'e', 'n', 'u', 'm'>,
	Identifier,
	pegtl::one<'{'>,
	pegtl::list_tail<EnumValue, Comma, Separator>,
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
	pegtl::list_tail<StructMember, Semicolon, Separator>,
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
struct AtomExpr : pegtl::sor<Literal, IdentifierExpr> {};

// struct CallableExpr;
// struct FunctionCall : Interleaved<Seps,
// 	CallableExpr,
// 	pegtl::one<'('>,
// 	FunctionArgsList,
// 	pegtl::one<')'>
// > {};
struct ParanthExprClose : pegtl::one<')'> {};
struct ParanthExpr : pegtl::if_must<pegtl::one<'('>,
	Seps,
	Expr,
	Seps,
	ParanthExprClose
> {};

// Order here is extremely important.
// The ones that can be discarded quickly should be first.
// Recursive calls to Expr should be last.
struct Expr0 : pegtl::sor<
	ParanthExpr,
	CodeBlock,
	AtomExpr
> {};

struct FunctionArgsList : pegtl::opt<pegtl::list_tail<Identifier, Comma, Separator>> {};
struct FunctionArgsListClose : pegtl::one<')'> {};
struct FunctionArgsListP : pegtl::if_must<
	pegtl::one<'('>,
	Seps,
	FunctionArgsList,
	Seps,
	FunctionArgsListClose
> {};
struct FunctionArgLists : pegtl::star<FunctionArgsListP> {};
struct FunctionCall : Interleaved<Seps,
	Expr0,
	FunctionArgLists
> {};

struct MemberAccessor : Interleaved<Seps,
	Identifier,
	FunctionArgLists
> {};
struct MemberAccessors : pegtl::star<pegtl::if_must<Dot, Seps, MemberAccessor>> {};
struct MemberAccessChain : Interleaved<Seps,
	FunctionCall,
	MemberAccessors
> {};

// struct FunctionChain : Interleaved<Seps,
// 	Expr0,
// 	pegtl::star<FunctionArgsListP>
// > {};

// struct MemberAccessor : FunctionChain {};
// struct MemberAccess : pegtl::if_must<Dot,
// 	Seps,
// 	MemberAccessor
// > {};
// struct MemberAccessChain : Interleaved<Seps,
// 	FunctionChain,
// 	pegtl::star<MemberAccess>
// > {};

struct PrimaryExpr : pegtl::sor<
	// TODO: make if_must?
	Interleaved<Seps, pegtl::one<'-'>, MemberAccessChain>,
	MemberAccessChain> {};

template<typename R, typename... S>
struct OptIfMust : pegtl::if_then_else<R, pegtl::must<S...>, pegtl::success> {};

struct AddExpr;
struct MultExpr;
struct SubExpr;
struct DivExpr;

// struct AddRest : pegtl::opt<Interleaved<Seps, Plus, AddExpr>> {};
// struct DivRest : pegtl::opt<Interleaved<Seps, Divide, PrimaryExpr>> {};
// struct SubRest : pegtl::opt<Interleaved<Seps, Minus, MultExpr>> {};
// struct MultRest : pegtl::opt<Interleaved<Seps, Mult, DivExpr>> {};

struct AddPart : pegtl::seq<SubExpr> {};
struct DivPart : pegtl::seq<PrimaryExpr> {};
struct SubPart : pegtl::seq<MultExpr> {};
struct MultPart : pegtl::seq<DivExpr> {};

struct AddRest : pegtl::star<pegtl::if_must<Plus, Seps, SubPart, Seps>> {};
struct DivRest : pegtl::star<pegtl::if_must<Divide, Seps, DivPart, Seps>> {};
struct SubRest : pegtl::star<pegtl::if_must<Minus, Seps, MultPart, Seps>> {};
struct MultRest : pegtl::star<pegtl::if_must<Mult, Seps, DivPart, Seps>> {};

struct DivExpr : Interleaved<Seps, PrimaryExpr, DivRest> {};
struct MultExpr : Interleaved<Seps, DivExpr, MultRest> {};
struct SubExpr : Interleaved<Seps, MultExpr, SubRest> {};
struct AddExpr : Interleaved<Seps, SubExpr, AddRest> {};

struct Expr : pegtl::sor<IfExpr, AddExpr> {};

// Inputs
/*
struct ConstantBufferKeyword : pegtl::keyword<'c', 'o', 'n', 's', 't'> {};
struct BufferInput : Interleaved<Seps,
	pegtl::opt<ConstantBufferKeyword>
*/

struct NamespaceDecl;
struct GlobalDecl : pegtl::sor<FunctionDecl, StructDecl, EnumDecl> {};
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

struct Eof : pegtl::eof {};
struct Grammar : pegtl::must<Module, Eof> {};

} // namespace syntax
