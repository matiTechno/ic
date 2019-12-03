all:
	g++ -O3 main.cpp compile_auxiliary.cpp compile_binary.cpp compile_unary.cpp compiler.cpp vm.cpp -o ic
