
#include <vector>
#include <iostream>
#include "server.h"
#include <thread>

int main(int argc, char *argv[])
{
    worker w;
    server s(&w, 15396);
    w.execute(1000);
    /*std::vector<pollfd> fds;
    {
        pollfd fd = {s, POLLIN, 0};
        fds.push_back(fd); // fds[0].fd = s, .events (что именно будет у socket'a отслеживать) POLLIN - (конкретно pollin - то что можно из него читать)
    }

    for (;;)
    {
        poll(fds.data(), fds.size(), -1); //-1 означает что таймаут вечен

        if (fds[0].revents != 0)
        {
            int c = accept(s, nullptr, nullptr);
            pollfd fd = {c, POLLIN, 0};
            fds.push_back(fd);
        }

        for (size_t i = 1; i != fds.size(); )
        {
            if (fds[i].revents != 0)
            {
                char buf[1024];
                ssize_t r = read(fds[i].fd, buf, sizeof buf);
                if (r == 0)
                {
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    continue;
                }
                write(fds[i].fd, buf, r);
            }
            ++i;
        }
    }
    close(s);
    return 0;*/
    /*for (;;)
    {
        int c = accept(s, nullptr, nullptr);
        for (;;)
        {
            char buf[1024];
            int r = read(c, buf, sizeof buf);
            if (r == 0)
                break;
            write(c, buf, r);
        }
        close(c);
    }
    close(s);
    return 0;*/
}
// poll (epoll)
// read -> write потом нельзя делать очередной read, потому что вдруг он повиснет, нужно обезательно делать poll
// поэтому можно включить у сокета non block режим
// fcntl
// закрывать нужно то что приходить из функции socket и из функции accept
