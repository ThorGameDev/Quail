#include <iostream>

extern "C" {
    int average(int, int);
}

int main(){
    std::cout << "Average of 10 and 20: " << average(10, 20) << std::endl;
}
