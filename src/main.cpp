#include <iostream>
#include <string.h>
#include <format>

#include <util.h>
#include <iblcli.h>

int main() {
    ibl::InitOpenGL();
    ibl::ComputeBRDF(1024, 4096);

    // float* img = ibl::util::LoadEXRImage();
    // ibl::util::SaveEXRImage("asakura2.exr", 660, 440, 3, img);
    // free(img);

    std::puts("Hello, world");
}