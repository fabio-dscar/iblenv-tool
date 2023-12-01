#include <iostream>
#include <string.h>
#include <format>

#define TINYEXR_USE_MINIZ 0
#include <zlib.h>
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

int main() {
    const char* input = "asakusa.exr";
    float* out; // width * height * RGBA
    int width;
    int height;
    const char* err = NULL; // or nullptr in C++11

    int ret = LoadEXR(&out, &width, &height, input, &err);

    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            fprintf(stderr, "ERR : %s\n", err);
            FreeEXRErrorMessage(err); // release memory of error message.
        }
    } else {
        std::cout << std::format("Loaded! {}x{}\n", width, height);
        std::cout << out[56] << "\n";
        free(out); // release memory of image data
    }

    std::puts("Hello, world");
}