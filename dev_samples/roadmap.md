# Roadmap

Individual lists are (as of yet) in no particular order.

## Design (can happen pre-jam-results)
- finalize syntax & semantics for tensors (I dislike `int#[2,3,4]`)
- finalize syntax for generics/templates (not necessarily semantics, not yet)
- finalize (most of) semantics for arrays & strings
- finalize syntax for local vs global imports

## Short-Term (immediately after jam)
- language server support (it'd normally be too early, but it's architecturally significant)
- proper module management (e.g. working `import` & friends)
- proper internal error handling (i.e. a "final" system for it)
- memory management rework & cleanup (no leaks, no valgrind errors)
- proper handling of chained comparison ops (e.g. `0 <= x < 10`)
- compound datatypes: classes, structs, arrays, tensors
- strings (could just be a special type of array?)

## Mid-Term (pre-1.0)
- various user hooks (errors, std{in,out,err}, imports, ...)
- better error messages
- debugging utilities: `trace(...)`, `assert(...)`, `unittest(...)`, `debug.line`
- rich math library (apart from what C offers, things such as `clamp()` or `lerp()` are useful)
- `implicit trait`
- operator overloading
- properties (getters/setters)
- attributes (`@foo`)
- `dynamic` (or `mixed` or `variant` --- whatever we call it)
- standard library (I/O, math, string manipulation, bit manipulation, containers, ...)

## Long-Term (post-1.0)
- `cent` and `ucent` types
- native compilation

## Uncategorized
- type inferrence e.g. `var x = 30;` (Short or Mid?)
- bigint support (Short or Mid?)
- option/error types (Short or Mid?)
- generics/templates (Short or Mid?)
- move towards refcounting (Short or Mid?)
- traits (Short or Mid?)
- JIT (Mid or Long?)
- JavaScript backend (Mid or Long?)
