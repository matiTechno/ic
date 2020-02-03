all:
	g++ -O3 -fno-exceptions -fno-rtti -o ic \
            main.cpp ic_impl.cpp compile_auxiliary.cpp compile_binary.cpp \
            compile_unary.cpp compiler.cpp vm.cpp disassemble.cpp
