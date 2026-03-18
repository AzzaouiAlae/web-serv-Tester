#include "TestsCLI/TestsCLI.hpp"
#include <signal.h>

int main()
{
    signal(SIGPIPE, SIG_IGN);
    system("clear");
    TestCLI test;
    test.run();

    return 0;
}
