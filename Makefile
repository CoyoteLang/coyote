# AUTOGENERATED FILE; DO NOT MODIFY (use `build.py` instead)
CFLAGS+=-DCOY_DEBUG -Og -Wall -g -pedantic -std=c99
LDFLAGS+=-static -static-libgcc
INCLUDES+=-Ilib -Isrc

# for easy run in repl.it
default: test
	./build/test

all: stb_ds test

HEADERS=lib/stb_ds.h src/bytecode.h src/compiler/ast.h src/compiler/codegen.h src/compiler/lexer.h src/compiler/semalysis.h src/compiler/token.h src/function_builder.h src/typeinfo.h src/util/bitarray.h src/util/debug.h src/util/hints.h src/util/string.h src/vm/compat_shims.h src/vm/context.h src/vm/env.h src/vm/function.h src/vm/gc.h src/vm/register.h src/vm/slots.h src/vm/stack.h src/vm/vm.h test/test.h
build/obj/%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $< -c -o $@

### TARGET: stb_ds

stb_ds_OBJECTS = build/obj/lib/stb_ds.o

stb_ds_HEADERS = lib/stb_ds.h


### TARGET: test

test: build/test

test_OBJECTS = build/obj/src/compiler/ast.o build/obj/src/compiler/codegen.o build/obj/src/compiler/lexer.o build/obj/src/compiler/semalysis.o build/obj/src/compiler/token.o build/obj/src/function_builder.o build/obj/src/util/bitarray.o build/obj/src/util/debug.o build/obj/src/util/string.o build/obj/src/vm/compat_shims.o build/obj/src/vm/context.o build/obj/src/vm/env.o build/obj/src/vm/function.o build/obj/src/vm/gc.o build/obj/src/vm/slots.o build/obj/src/vm/stack.o build/obj/src/vm/vm.o build/obj/test/main.o

test_HEADERS = src/bytecode.h src/compiler/ast.h src/compiler/codegen.h src/compiler/lexer.h src/compiler/semalysis.h src/compiler/token.h src/function_builder.h src/typeinfo.h src/util/bitarray.h src/util/debug.h src/util/hints.h src/util/string.h src/vm/compat_shims.h src/vm/context.h src/vm/env.h src/vm/function.h src/vm/gc.h src/vm/register.h src/vm/slots.h src/vm/stack.h src/vm/vm.h test/test.h

build/test: $(test_OBJECTS)  $(stb_ds_OBJECTS) $(stb_ds_HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
