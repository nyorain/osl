#pragma once

#include <string>
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <cstdint>

namespace ast {

using i32 = std::int32_t;
using u32 = std::uint32_t;
using f32 = float;
using f64 = double;

struct Identifier;
struct Function;
struct Expression;
struct OpExpression;
struct FunctionCall;
struct CodeBlock;
struct NumberLiteral;
struct IdentifierExpression;
class Visitor;

struct Identifier {
	std::string name;
};

struct Type {
	enum class Category {
		primitive,
		eStruct,
		eEnum,
		// function,
	};

	Category category;
};

struct BuiltinType : Type {
	enum class Type {
		eVoid,
		f32,
		f64,
		i32,
		u32,
		eBool,

		count
	};

	Type type;
	// for vectors & matrices
	unsigned rows {1};
	unsigned cols {1};

	// Builtin types
	static const ast::BuiltinType& voidType();
	static const ast::BuiltinType& f32Type();
	static const ast::BuiltinType& f64Type();
	static const ast::BuiltinType& i32Type();
	static const ast::BuiltinType& u32Type();
	static const ast::BuiltinType& boolType();
	static const ast::BuiltinType& vecType(Type type, unsigned rows);
	static const ast::BuiltinType& matType(Type type, unsigned rows, unsigned cols);
};

/*
struct FunctionType : Type {
	const Type* returnType;
	std::vector<const Type*> argumentTypes;
};
*/

struct EnumValue {
	Identifier name;
	std::vector<Type*> types;
};

struct EnumType : Type {
	std::vector<EnumValue> values;
};

struct StructMember {
	const Type* type;
	Identifier name;
	std::unique_ptr<Expression> init;
};

struct StructType : Type {
	std::vector<StructMember> members;
	std::string name;
};

struct Parameter {
	Type* type;
	Identifier name;
};

struct Node {
	virtual void visit(Visitor&) = 0;
	virtual std::string print() const = 0;
	virtual ~Node() = default;
};

struct Expression : Node {
	virtual const Type& type() const = 0;
};

template<typename Base, typename Derived>
struct DeriveVisitor : public Base {
	void visit(Visitor& v) override;
};

struct VariableDeclaration {
	Identifier name;
	Type* type;
	std::unique_ptr<Expression> init;
};

struct IdentifierExpression : DeriveVisitor<Expression, IdentifierExpression> {
	const VariableDeclaration* decl;

	const Type& type() const override { return *decl->type; }
	std::string print() const override { return decl->name.name; }
};

template<typename T>
constexpr const BuiltinType& builtinType() {
	if constexpr(std::is_same_v<T, i32>) {
		return BuiltinType::i32Type();
	} else if constexpr(std::is_same_v<T, bool>) {
		return BuiltinType::boolType();
	} else if constexpr(std::is_same_v<T, u32>) {
		return BuiltinType::u32Type();
	} else if constexpr(std::is_same_v<T, f32>) {
		return BuiltinType::f32Type();
	} else if constexpr(std::is_same_v<T, f64>) {
		return BuiltinType::f64Type();
	}

	// constexpr error landing here
}

struct Literal : DeriveVisitor<Expression, NumberLiteral> {};

template<typename T>
struct LiteralImpl : DeriveVisitor<Literal, NumberLiteral> {
	T value;

	const Type& type() const override { return builtinType<T>(); }
	std::string print() const override { return std::to_string(value); }
};

struct Statement : Node {
	virtual std::vector<Expression*> expressions() const = 0;
};

struct AssignStatement : DeriveVisitor<Statement, AssignStatement> {
	std::unique_ptr<Expression> left;
	std::unique_ptr<Expression> right;

	std::vector<Expression*> expressions() const override {
		return {left.get(), right.get()};
	}
	std::string print() const override {
		auto ret = left->print();
		ret += " = ";
		ret += right->print();
		return ret;
	}
};

struct CodeBlock : DeriveVisitor<Expression, CodeBlock> {
	CodeBlock* parent {}; // might be null
	std::vector<std::unique_ptr<Statement>> statements;
	std::unique_ptr<Expression> ret; // optional

	const Type& type() const override {
		if(ret) {
			return ret->type();
		}

		return BuiltinType::voidType();
	}

	std::string print() const override {
		std::string sret = "{\n";
		for(const auto& s : statements) {
			sret += s->print();
			sret += ";\n";
		}

		if(ret) {
			sret += ret->print();
			sret += "\n";
		}

		sret += "}\n";
		return sret;
	}
};

struct Module {
	std::vector<std::unique_ptr<Function>> functions;
	std::vector<std::unique_ptr<Type>> types;
};

// Function or builtin function
struct Callable {
	// If this Callable is callable with the given parameters,
	// returns the type it will return. Otherwise returns nullptr.
	// TODO: probably a better interface
	// virtual const Type* typeCheck(nytl::span<const Type*> params) const = 0;

	virtual std::vector<const Type*> parameters() const = 0;
	virtual const Type& returnType() const = 0;
	virtual std::string_view name() const = 0;
	virtual ~Callable() = default;
};

/*
struct BuiltinFunction : Callable {
	const Type* retType;
	std::vector<const Type*> parameters;
};
*/

struct Function : Callable {
	struct Parameter {
		const Type* type {};
		Identifier ident;
	};

	Identifier ident;
	std::vector<Parameter> params;
	// TODO: use that instead to bind them
	// std::vector<VariableDeclaration> params;
	const Type* retType;
	std::unique_ptr<CodeBlock> code;

	std::vector<const Type*> parameters() const override {
		std::vector<const Type*> ret;
		for(auto& param : params) {
			ret.push_back(param.type);
		}
		return ret;
	}
	const Type& returnType() const override {
		return *retType;
	}
	std::string_view name() const override {
		return ident.name;
	}
};

struct ExpressionStatement : DeriveVisitor<Statement, ExpressionStatement> {
	std::unique_ptr<Expression> expr;

	std::vector<Expression*> expressions() const override {
		return {expr.get()};
	}
	std::string print() const override {
		return expr->print();
	}
};

struct IfExpression : DeriveVisitor<Expression, IfExpression> {
	struct Branch {
		std::unique_ptr<Expression> condition;
		std::unique_ptr<CodeBlock> code;
	};

	Branch ifBranch;
	std::vector<Branch> elsifBranches; // may be empty
	std::unique_ptr<CodeBlock> elseBranch; // optional
	Type* ptype;

	const Type& type() const override { return *ptype; }
	std::string print() const override {
		std::string ret = "if ";
		ret += ifBranch.condition->print();
		ret += " ";
		ret += ifBranch.code->print();

		for(const auto& b : elsifBranches) {
			ret += " else if ";
			ret += b.condition->print();
			ret += " ";
			ret += b.code->print();
		}

		if(elseBranch) {
			ret += " else ";
			ret += elseBranch->print();
		}

		ret += "\n";
		return ret;
	}
};

struct MemberAccess : DeriveVisitor<Expression, MemberAccess> {
	std::unique_ptr<Expression> accessed;
	const StructMember* accessor;

	const Type& type() const override {
		return *accessor->type;
	}
};

struct FunctionCall : DeriveVisitor<Expression, FunctionCall> {
	const Callable* called;
	std::vector<std::unique_ptr<Expression>> arguments;

	const Type& type() const override { return called->returnType(); }
	std::string print() const override {
		std::string ret;
		ret += called->name();
		ret += "(";
		for(const auto& arg : arguments) {
			ret += arg->print();
		}
		ret += ")";

		return ret;
	}
};

struct OpExpression : DeriveVisitor<Expression, OpExpression> {
	enum class OpType {
		add,
		mult,
		sub,
		div
	};

	static const char* name(OpType op) {
		switch(op) {
			case OpType::add: return "+";
			case OpType::mult: return "*";
			case OpType::sub: return "-";
			case OpType::div: return "/";
			default: return "";
		}
	}

	std::vector<std::unique_ptr<Expression>> children;
	OpType opType;
	Type* ptype;

	const Type& type() const override { return *ptype; }
	std::string print() const override {
		std::string ret;
		ret += "(";
		auto first = true;
		const auto* sop = name(opType);
		for(const auto& c : children) {
			if(!first) {
				ret += " ";
				ret += sop;
				ret += " ";
			}

			ret += c->print();
			first = false;
		}
		ret += ")";
		return ret;
	}
};

class Visitor {
public:
	virtual ~Visitor() = default;

	virtual void visit(Node&) {}
	virtual void visit(Statement& s) {
		visit(static_cast<Node&>(s));
	}
	virtual void visit(AssignStatement& s) {
		visit(static_cast<Statement&>(s));
	}
	virtual void visit(ExpressionStatement& s) {
		visit(static_cast<Statement&>(s));
	}
	virtual void visit(Expression& e) {
		visit(static_cast<Node&>(e));
	}
	virtual void visit(OpExpression& e) {
		visit(static_cast<Expression&>(e));
	}
	virtual void visit(FunctionCall& e) {
		visit(static_cast<Expression&>(e));
		for(auto& arg : e.arguments) {
			visit(*arg);
		}
	}
	virtual void visit(CodeBlock& e) {
		visit(static_cast<Expression&>(e));
		for(auto& stmt : e.statements) {
			visit(*stmt);
		}
		if(e.ret) {
			visit(*e.ret);
		}
	}
	virtual void visit(Literal& e) {
		visit(static_cast<Expression&>(e));
	}
	virtual void visit(IdentifierExpression& e) {
		visit(static_cast<Expression&>(e));
	}
};

template<typename Base, typename Derived>
void ast::DeriveVisitor<Base, Derived>::visit(Visitor& v) {
	v.visit(static_cast<Derived&>(*this));
}

} // namespace ast
