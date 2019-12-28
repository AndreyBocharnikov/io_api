
#include <vector>
#include <iostream>
#include "server.h"
#include <thread>

int main(int argc, char *argv[])
{
    worker w;
    server s(&w, 15396);
    w.execute(1000);
}
