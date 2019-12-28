//
// Created by andreybocharnikov on 26.12.2019.
//

#ifndef MY_SERVER_TASK_H
#define MY_SERVER_TASK_H

#include <functional>
#include <utility>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "worker.h"

struct task{
    using func = std::function<void(task *const, uint32_t)>;
    using time_point = std::chrono::steady_clock::time_point;

    task(worker*, func run, std::function<void()> disconnect, int file_descriptor);
    ~task();

    int read(char*, size_t);
    int write(const char*, size_t);
    void edit(uint32_t);

    bool error_happend();
    static bool is_closing(uint32_t event);
    void Close();

    time_point get_last_action() const;

    void execute(uint32_t events);
    void run_disconnect();

private:
    void update_last_action();
private:
    worker* main_loop;
    func run;
    std::function<void()> disconnect;
    int fd;
    // uint32_t const NEEDED_EVENTS = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
    uint32_t const CLOSE_EVENTS = EPOLLERR | EPOLLRDHUP | EPOLLHUP;
    bool is_error = false;
    time_point last_action;
};


#endif //MY_SERVER_TASK_H
