#define _CRT_SECURE_NO_WARNINGS
#include <chrono>
#include <assert.h>
#include <string.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include "ic.h"

void host_random01(ic_data*, ic_data* retv, void*)
{
    retv->f64 = (double)rand() / RAND_MAX; // todo; the distribution of numbers is probably not that good
}

void host_write_ppm6(ic_data* argv, ic_data*, void*)
{
    FILE* file = fopen((char*)argv[0].pointer, "wb");
    assert(file);
    char buf[1024];
    int width = argv[1].s32;
    int height = argv[2].s32;
    snprintf(buf, sizeof(buf), "P6 %d %d 255 ", width, height);
    int len = strlen(buf);
    fwrite(buf, 1, len, file);
    fwrite(argv[3].pointer, 1, width * height * 3, file);
    fclose(file);
}

std::vector<unsigned char> load_file(const char* name);

void host_read_file(ic_data* argv, ic_data*, void*)
{
    std::vector<unsigned char> vec = load_file((char*)argv[0].pointer);
    void* ptr = malloc(vec.size());
    memcpy(ptr, vec.data(), vec.size());
    *(void**)argv[1].pointer = ptr;
    *(int*)argv[2].pointer = vec.size();
}

void write_to_file(unsigned char* data, int size, const char* filename)
{
    FILE* file = fopen(filename, "wb");
    assert(file);
    fwrite(data, 1, size, file);
    fclose(file);
}

struct test_struct
{
    char a;
    int b;
    char c;
    double d;
    float e;
};

static const char* test_struct_decl = R"(
struct test_struct
{
    s8 a;
    s32 b;
    s8 c;
    f64 d;
    f32 e;
};
)";

static const char* test_program = R"(
void print_test(test_struct* ptr)
{
    printf(ptr->a);
    printf(ptr->b);
    printf(ptr->c);
    printf(ptr->d);
    printf(ptr->e);
}

s32 main()
{
    test_struct t = host_test_value();
    print_test(&t);

    test_struct* ptr = host_test_ptr();
    for(s32 i = 0; i < 2; ++i)
    {
        print_test(ptr);
        ++ptr;
    }

    test_struct t2;
    t2.a = 1;
    t2.b = 2;
    t2.c = 3;
    t2.d = 4;
    t2.e = 5;

    host_print_test(666, t2, 999);
    host_print_test_ptr(&t2);
    return 55;
}
)";

void host_test_value(ic_data*, ic_data* retv, void*)
{
    test_struct t;
    t.a = 10;
    t.b = 11;
    t.c = 12;
    t.d = 13;
    t.e = 14;

    *(test_struct*)retv = t;
}

void host_test_ptr(ic_data*, ic_data* retv, void*)
{
    static test_struct t[2];
    for (int i = 0; i < 2; ++i)
    {
        t[i].a = 100 + i;
        t[i].b = 101 + i;
        t[i].c = 102 + i;
        t[i].d = 103 + i;
        t[i].e = 104 + i;
    }
    *(test_struct**)retv = t;
}

void print(test_struct t)
{
    printf("%f\n", (double)t.a);
    printf("%f\n", (double)t.b);
    printf("%f\n", (double)t.c);
    printf("%f\n", (double)t.d);
    printf("%f\n", (double)t.e);
}

void host_print_test(ic_data* argv, ic_data*, void*)
{
    int arg1 = ic_get_int(argv);
    test_struct arg2 = *(test_struct*)ic_get_arg(argv, sizeof(test_struct));
    double arg3 = ic_get_double(argv);
    printf("arg1 = %d\n", arg1);
    print(arg2);
    printf("arg3 = %f\n", arg3);
}

void host_print_test_ptr(ic_data* argv, ic_data*, void*)
{
    print(**(test_struct**)argv);
}

std::vector<unsigned char> load_file(const char* name)
{
    FILE* file = fopen(name, "rb"); // oh my dear Windows, you have to make life harder
    assert(file);
    int rc = fseek(file, 0, SEEK_END);
    assert(rc == 0);
    int size = ftell(file);
    assert(size != EOF);
    assert(size != 0);
    rewind(file);
    std::vector<unsigned char> data;
    data.resize(size + 1);
    fread(data.data(), 1, size, file);
    data.back() = '\0';
    fclose(file);
    return data;
}

int main(int argc, const char** argv)
{
    ic_host_function functions[] =
    {
        {"void write_ppm6(const s8*, s32, s32, const u8*)", host_write_ppm6},
        {"f64 random01()", host_random01},
        {"void read_file(const s8*, u8**, s32*)", host_read_file},
        nullptr
    };

    assert(argc == 3);

    if (strcmp(argv[1], "run_source") == 0)
    {
        auto t1 = std::chrono::high_resolution_clock::now();
        {
            std::vector<unsigned char> file_data = load_file(argv[2]);
            ic_vm vm;
            ic_vm_init(vm);
            ic_program program;
            {
                auto t1 = std::chrono::high_resolution_clock::now();
                bool success = ic_program_init_compile(program, (char*)file_data.data(), IC_LIB_CORE, { functions, nullptr });
                assert(success);
                auto t2 = std::chrono::high_resolution_clock::now();
                printf("compilation time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
            }
            {
                auto t1 = std::chrono::high_resolution_clock::now();
                ic_vm_run(vm, program);
                auto t2 = std::chrono::high_resolution_clock::now();
                printf("execution time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
            }
            ic_program_free(program);
            ic_vm_free(vm);
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        printf("total time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
        return 0;
    }
    else if (strcmp(argv[1], "run_bytecode") == 0)
    {
        std::vector<unsigned char> file_data = load_file(argv[2]);
        ic_vm vm;
        ic_vm_init(vm);
        ic_program program;
        ic_program_init_load(program, file_data.data(), IC_LIB_CORE, { functions, nullptr });
        ic_vm_run(vm, program);
        ic_program_free(program);
        ic_vm_free(vm);
        return 0;
    }
    else if (strcmp(argv[1], "compile") == 0)
    {
        std::vector<unsigned char> file_data = load_file(argv[2]);
        ic_program program;
        bool success = ic_program_init_compile(program, (char*)file_data.data(), IC_LIB_CORE, { functions, nullptr });
        assert(success);
        unsigned char* buf;
        int size;
        ic_program_serialize(program, buf, size);
        write_to_file(buf, size, "bytecode.bc");
        ic_buf_free(buf);
        ic_program_free(program);
        return 0;
    }
    else if (strcmp(argv[1], "disassemble") == 0)
    {
        std::vector<unsigned char> file_data = load_file(argv[2]);
        ic_program program;
        ic_program_init_load(program, file_data.data(), IC_LIB_CORE, { functions, nullptr });
        ic_program_print_disassembly(program);
        ic_program_free(program);
        return 0;
    }
    else if (strcmp(argv[1], "test") == 0)
    {
        ic_host_function test_functions[] =
        {
            {"test_struct host_test_value()", host_test_value},
            {"test_struct* host_test_ptr()", host_test_ptr},
            {"void host_print_test(s32, test_struct, f64)", host_print_test},
            {"void host_print_test_ptr(test_struct*)", host_print_test_ptr},
            nullptr
        };

        ic_program program;
        bool success = ic_program_init_compile(program, test_program, IC_LIB_CORE, { test_functions, test_struct_decl });
        assert(success);
        ic_vm vm;
        ic_vm_init(vm);
        int ret = ic_vm_run(vm, program);
        printf("main() returned %d\n", ret);
        ic_vm_free(vm);
        ic_program_free(program);
        return 0;
    }
    else
        assert(false);
    return -1;
}
