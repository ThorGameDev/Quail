#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT void putchard(int8_t X) {
    fputc((char)X, stderr);
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT void printd(double X) {
    fprintf(stderr, "%f\n", X);
}

// i32out - Outputs the contents of X
extern "C" DLLEXPORT void i32out(int32_t X) {
    fprintf(stderr, "%i\n", X);
}

// i8out - Outputs the contents of X
extern "C" DLLEXPORT void i16out(int16_t X) {
    fprintf(stderr, "%i\n", X);
}

// i8out - Outputs the contents of X
extern "C" DLLEXPORT void i8out(int8_t X) {
    fprintf(stderr, "%i\n", X);
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT void floatout(float X) {
    fprintf(stderr, "%f\n", X);
}

// Void - get a void
extern "C" DLLEXPORT void Void() { }

extern "C" DLLEXPORT int8_t input() {
    int8_t c = getchar();
    while (c == '\n')
        c = getchar();
    return c;
}
