#include "ast.hpp"
#include <array>
#include <cassert>

namespace ast {

// Builtin types
using Builtins = std::array<BuiltinType, 5>;
using std::array;

struct BuiltinTypeTable {
	BuiltinType _void;
	array<array<array<BuiltinType, unsigned(BuiltinType::Type::count) - 1>, 3>, 3> types;
};

BuiltinTypeTable initBuiltinTypes() {
	BuiltinTypeTable ret;
	ret._void.type = BuiltinType::Type::eVoid;

	for(auto t = 1u; t < unsigned(BuiltinType::Type::count); ++t) {
		for(auto r = 1u; r < 4; ++r) {
			for(auto c = 1u; c < 4; ++c) {
				ret.types[r - 1][c - 1][t - 1].rows = r;
				ret.types[r - 1][c - 1][t - 1].cols = c;
				ret.types[r - 1][c - 1][t - 1].type = BuiltinType::Type(t);
			}
		}
	}

	return ret;
}

const BuiltinTypeTable& builtinTypeTable() {
	static BuiltinTypeTable table = initBuiltinTypes();
	return table;
}

const ast::BuiltinType& BuiltinType::voidType() {
	return builtinTypeTable()._void;
}

const ast::BuiltinType& BuiltinType::f32Type() {
	return matType(Type::f32, 1, 1);
}

const ast::BuiltinType& BuiltinType::f64Type() {
	return matType(Type::f64, 1, 1);
}

const ast::BuiltinType& BuiltinType::i32Type() {
	return matType(Type::i32, 1, 1);
}

const ast::BuiltinType& BuiltinType::u32Type() {
	return matType(Type::u32, 1, 1);
}
const ast::BuiltinType& BuiltinType::boolType() {
	return matType(Type::eBool, 1, 1);
}

const ast::BuiltinType& BuiltinType::vecType(Type type, unsigned rows) {
	return matType(type, rows, 1);
}

const ast::BuiltinType& BuiltinType::matType(Type type, unsigned rows, unsigned cols) {
	assert(rows >= 1 && rows <= 4);
	assert(cols >= 1 && cols <= 4);
	return builtinTypeTable().types[rows - 1][cols - 1][unsigned(type) - 1];
}

} // namespace ast
