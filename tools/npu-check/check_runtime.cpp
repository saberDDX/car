#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <string>

// This checks the loader and symbols only. It never initializes an NPU context.
int main(int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [runtime-library]\n";
        return 2;
    }
    const char* path = argc == 2 ? argv[1] : "/lib/librknnrt.so";
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "runtime_load=FAIL library=" << path << " error=" << dlerror() << '\n';
        return 1;
    }
    int status = 0;
    for (const char* name : {"rknn_init", "rknn_query", "rknn_inputs_set", "rknn_run",
                             "rknn_outputs_get", "rknn_outputs_release", "rknn_destroy"}) {
        dlerror();
        void* symbol = dlsym(handle, name);
        const char* error = dlerror();
        if (error || !symbol) {
            std::cerr << "symbol=" << name << " status=MISSING error="
                      << (error ? error : "null symbol") << '\n';
            status = 1;
        }
    }
    if (dlclose(handle) != 0) {
        std::cerr << "runtime_close=FAIL error=" << dlerror() << '\n';
        status = 1;
    }
    std::cout << "runtime_load=" << (status ? "FAIL" : "PASS") << " library=" << path
              << " model_inference=NOT_RUN\n";
    return status;
}
