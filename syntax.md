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
