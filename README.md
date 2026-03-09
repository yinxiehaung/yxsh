# 1. yxsh (Yin-Xie Shell)

`yxsh` is a UNIX shell built entirely from scratch in C.
Unlike typical academic shell implementations, `yxsh` avoids global variables and fragile string manipulations. Instead, it features a robust, compiler-like architecture:

- **Lexer & AST Parser**: Safely tokenizes input and builds an Abstract Syntax Tree.
- **Arena Memory Allocator**: Guarantees zero memory leaks by resetting the execution frame after each prompt.
- **Context-Driven Engine**: Encapsulates execution states, making it fully ready for multi-threaded network environments.

# 2. soul-build (The Build System)

**"Why write Makefiles when you can write C?"**
`soul-build` embeds the `yxsh` engine directly into your build scripts. It allows you to define build logic using standard C code combined with intuitive UNIX pipeline syntax.

- **Self-hosting Capability**: You can use `./build` to compile `yxsh` itself.
