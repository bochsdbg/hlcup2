//#include "syscall.hpp"
#include "socket.hpp"

int main() {
    ef::platform::connect(0, nullptr, 0);

    //    ef::platform::syscall(ef::platform::SC::write, 1, "hello\n", 6);
    return 0;
}
