#include <cstdint>
#include <cstdio>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(int8_t X) {
    fputc((char)X, stderr);
    return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
    fprintf(stderr, "%f\n", X);
    return 0;
}

// i32out - Outputs the contents of X
extern "C" DLLEXPORT double i32out(int32_t X) {
    fprintf(stderr, "%i\n", X);
    return 0;
}
