//
// Created by andreybocharnikov on 25.12.2019.
//

#ifndef MY_SERVER_WORKER_H
#define MY_SERVER_WORKER_H

#include <functional>
#include <utility>
#include <csignal>
#include <sys/epoll.h>
#include <memory>
#include <unordered_map>
#include <map>
#include <chrono>

struct task;

struct worker{
    using time_point = std::chrono::steady_clock::time_point;

    struct timer{
        explicit timer(uint64_t time_out);

        void add_client(std::unique_ptr<task>&);
        void delete_client(task*);
        void remove_old_clients();

        int64_t time_diff(time_point const&, time_point const&) const;

    private:
        const uint64_t time_out;
        std::unordered_map<task*, time_point> last_action;
        std::map <std::pair<time_point, task*>, std::unique_ptr<task>> tasks;
    };
    explicit worker(uint64_t time_out = 600); // afk time out equals to 10 mins
    void execute(int time_out); // epoll_wait time out

    void add_client(std::unique_ptr<task>&);
    void delete_client(task*);

    void add(int fd, epoll_event*);
    void edit(int fd, epoll_event*);
    void del(int fd, epoll_event*);

private:
    const int epoll_fd;
    timer clock;
    static const size_t EPOLL_MAX = 2000;
};

#endif //MY_SERVER_WORKER_H
