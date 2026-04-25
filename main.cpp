

#include "printf.hpp"
#include <vector>
#include <string>

int main() {
    sjtu::printf("Hello, %s!\n", "world");
    sjtu::printf("Integer: %d, Unsigned: %u, Default: %_\n", -123, 456u, 789);
    sjtu::printf("Vector: %_\n", std::vector<int>{1, 2, 3});
    sjtu::printf("Mixed: %s %d %u %_\n", std::string("test"), 10, 20u, std::vector<std::string>{"a", "b"});
    sjtu::printf("Escape: %%\n");

    

    return 0;
}

