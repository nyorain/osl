#pragma once

#include <string>
#include <vector>
#include <memory>
#include <string>
#include <optional>

namespace ast {

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
	Identifier ident;
};

struct EnumValue {
	Identifier name;
	std::vector<Type*> types;
};

struct EnumType : Type {
	std::vector<EnumValue> values;
};

struct StructMember {
	Type* type;
	Identifier name;
	std::unique_ptr<Expression> init;
};

struct StructType : Type {
	std::vector<StructMember> members;
};

struct Parameter {
	Type* type;
	Identifier name;
};

struct Node {
	virtual void visit(Visitor&) = 0;
	virtual std::string print() const = 0;
};

struct Expression : Node {
	virtual Type* type() const = 0;
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
	VariableDeclaration* decl;

	Type* type() const override { return decl->type; }
	std::string print() const override { return decl->name.name; }
};

struct NumberLiteral : DeriveVisitor<Expression, NumberLiteral> {
	Type* ptype;
	long double number;

	Type* type() const override { return ptype; }
	std::string print() const override { return std::to_string(number); }
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
	CodeBlock* parent; // might be null
	std::vector<std::unique_ptr<Statement>> statements;
	std::unique_ptr<Expression> ret; // optional

	Type* type() const override {
		if(ret) {
			return ret->type();
		}

		// TODO: return void
		return nullptr;
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
};

struct Function {
	struct Parameter {
		Identifier ident;
		Type* type;
	};

	Identifier name;
	std::vector<Parameter> params;
	CodeBlock code;
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

	Type* type() const override { return ptype; }
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

struct FunctionCall : DeriveVisitor<Expression, FunctionCall> {
	Function* func;
	std::vector<std::unique_ptr<Expression>> arguments;

	Type* type() const override { return func->code.type(); }
	std::string print() const override {
		std::string ret;
		ret += func->name.name;
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

	Type* type() const override { return ptype; }
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
	}
	virtual void visit(CodeBlock& e) {
		visit(static_cast<Expression&>(e));
	}
	virtual void visit(NumberLiteral& e) {
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


/*
// TODO
// parser state
struct State {
	std::vector<std::unique_ptr<Function>> functions;
	std::vector<std::unique_ptr<VariableDeclaration>> globalVars;

	std::unique_ptr<Function> function;
	std::unique_ptr<CodeBlock> codeBlock;
	std::unique_ptr<Statement> statement;
	std::unique_ptr<Expression> expression;

	std::vector<VariableDeclaration*> localVars;
};
*/

} // namespace ast
