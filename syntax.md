# Syntax Ideas

C++ like
```
struct A {
	int a {7};
	bool b {false};
};

// Can use class for C++-compat but does not matter?
enum /*class*/ Test {
	a, // 0
	b, // 1
	c, // 2
};

// Using declarations
using Vec4f = vec4;
```

---

How to parse `a.b().c.d()`?

should result in something like:

```
FunctionCall {
	MemberAccess {
		MemberAccess {
			FunctionCall {
				MemberAccess {
					a,
					b
				},
				()
			},
			c
		},
		d
	},
	()
}
```

```
MemberAccessChain ::= Expr0[.FunctionCall]*
FunctionCall ::= MemberAccessChain[(FunctionArg*)]*
```

Could even embed more in syntax, e.g. forbid literals as member-access:

```
Expr0 ::= Literal | Identifier | (Expr) | CodeBlock

MemberAccessor ::= Identifier
MemberAccessNested ::= MemberAccessor[.FunctionCallNested]?
FunctionCallNested ::= MemberAccessNested[(FunctionArg*)]?

MemberAccess ::= Expr0[.FunctionCallNested]
FunctionCall ::= MemberAccessChain[(FunctionArg*)]?

# ignoring binary expression etc here
Stuff ::= FunctionCall
Expr ::= Stuff
```

```
wrong: a.(b.c)
right: (a.b).c

// maybe fix via selector, transform?
```


second try, just creating a list:

```
Expr0 ::= Literal | Identifier | (Expr) | CodeBlock

FunctionCall ::= Expr0[(FunctionArg*)]*
MemberAccessor ::= Identifier[(FunctionArgs*)]*
MemberAccessChain ::= FunctionCall[.MemberAccessor]*

Stuff ::= MemberAccessChain
```

how could we parse this into an AST?

```
createFunctionChain(unique_ptr<ast::Expression> called, vector<unique_ptr<node>>& callArgs) {
	for(auto& ca : callArgs) {
		auto call = make_unique<ast::FunctionCall>();
		call.callable = std::move(called);
		call.args = parseFunctionArgs(*ca);
		called = std::move(call);
	}

	return called;
}

parse(node) {
	if(node is MemberAccessChain) {
		assert(node.children.size() > 1);
		auto accessed = parseExpr(*node.children[0])
		node.children.erase(node.children.begin());

		for(auto& accessor : node.children) {
			assert(accessor->children.size() >= 1);

			auto res = make_unique<ast::MemberAccess>();
			res.accessed = std::move(accessed);
			res.identifier = parseIdentifier(accessor->children[0]);

			acessed = std::move(res);

			// function call chain
			if(accessor->children.size() > 1) {
				accessor->children.erase(accessor->children.begin());
				accessed = createFunctionChain(std::move(accessed), accessor.children);
			}
		}

		return accessed;
	} else if(node is FunctionCall) {
		assert(node.children.size() > 1);
		auto called = parseExpr(*node.children[0]);
		node.children.erase(node.children.begin());
		called = createFunctionChain(std::move(called), node.children);
		return called;
	}
}
```

We could (technically) try to forbid something like `5.a` in the syntax
already (making sure a literal can't be the first expression in a member
access chain) but i feel like the error messages for that should be
the same as for e.g. `i.a`, where `i` is an integer. And it's easier
to keep such details/special cases out of the syntax.
