#define _CRT_SECURE_NO_WARNINGS
#include "ic.h"
#include <chrono>
#include <assert.h>
// which header includes stdio.h here? I have no idea, wtf

ic_data host_random01(ic_data*)
{
    ic_data data;
    data.f64 = (double)rand() / RAND_MAX; // todo; the distribution of numbers is probably not that good
    return data;
}

ic_data host_write_ppm6(ic_data* argv)
{
    FILE* file = fopen((char*)argv[0].pointer, "wb");
    char buf[1024];
    int width = argv[1].s32;
    int height = argv[2].s32;
    snprintf(buf, sizeof(buf), "P6 %d %d 255 ", width, height);
    int len = strlen(buf);
    fwrite(buf, 1, len, file);
    fwrite(argv[3].pointer, 1, width * height * 3, file);
    fclose(file);
    return {};
}

void write_to_file(unsigned char* data, int size, const char* filename)
{
    FILE* file = fopen(filename, "wb");
    fwrite(data, 1, size, file);
    fclose(file);
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
    ic_host_function host_functions[] =
    {
        {"void write_ppm6(const s8*, s32, s32, const u8*)", host_write_ppm6},
        {"f64 random01()", host_random01},
    };

    assert(argc == 3);

    if (strcmp(argv[1], "run_source") == 0)
    {
        auto t1 = std::chrono::high_resolution_clock::now();
        {
            auto file_data = load_file(argv[2]);
            ic_program program;
            {
                auto t1 = std::chrono::high_resolution_clock::now();
                bool success = compile_to_bytecode((char*)file_data.data(), &program, IC_LIB_CORE, host_functions, 2);
                assert(success);
                auto t2 = std::chrono::high_resolution_clock::now();
                printf("compilation time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
            }
            {
                ic_vm vm;
                vm_init(vm, program, host_functions, 2);
                auto t1 = std::chrono::high_resolution_clock::now();
                vm_run(vm, program);
                auto t2 = std::chrono::high_resolution_clock::now();
                printf("execution time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
            }
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        printf("total time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
        return 0;
    }
    else if (strcmp(argv[1], "run_bytecode") == 0)
    {
        auto file_data = load_file(argv[2]);
        ic_program program = load_program(file_data.data());
        ic_vm vm;
        vm_init(vm, program, host_functions, 2);
        vm_run(vm, program);
        return 0;
    }
    else if (strcmp(argv[1], "compile") == 0)
    {
        auto file_data = load_file(argv[2]);
        ic_program program;
        bool success = compile_to_bytecode((char*)file_data.data(), &program, IC_LIB_CORE, host_functions, 2);
        assert(success);
        unsigned char* buf;
        int size;
        ic_serialize(program, buf, size);
        write_to_file(buf, size, "bytecode.bc");
        return 0;
    }
    else if (strcmp(argv[1], "disassemble") == 0)
    {
        auto file_data = load_file(argv[2]);
        ic_program program = load_program(file_data.data());
        disassmble(program);
        return 0;
    }
    else
        assert(false);

    return -1;
}
