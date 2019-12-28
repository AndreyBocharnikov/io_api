//
// Created by andreybocharnikov on 25.12.2019.
//

#include <cstring>
#include <iostream>
#include "worker.h"
#include "task.h"

namespace {
    volatile bool *quit = nullptr;

    void signal_handler(int) {
        if (!quit)
            return;
        *quit = true;
    }
}

worker::timer::timer(uint64_t time_out) : time_out(time_out) {}

void worker::timer::add_client(std::unique_ptr<task> &new_client) {
    time_point current_time = std::chrono::steady_clock::now();
    auto insert_in_la = last_action.insert({new_client.get(), current_time});
    if (!insert_in_la.second) {
        throw std::runtime_error("worker::timer::add_client insert in la failed");
    }
    auto insert_in_tasks = tasks.insert({{current_time, new_client.get()}, std::move(new_client)});
    if (!insert_in_tasks.second) {
        last_action.erase(new_client.get());
        throw std::runtime_error("worker::timer::add_client insert in \"tasks\" failed");
    }
}

void worker::timer::delete_client(task* client) {
    time_point last_action_time = last_action[client];
    last_action.erase(client);
    tasks.erase({last_action_time, client});
}

void worker::timer::remove_old_clients() {
    time_point current_time = std::chrono::steady_clock::now();

    std::map <std::pair<time_point, task*>, std::unique_ptr<task>> updated_time;
    auto it = tasks.begin();
    for (; it != tasks.end(); ++it) {
        if (time_diff(current_time, it->first.first) < time_out) {
            break;
        }

        time_point updated_last_time = it->second->get_last_action();
        if (time_diff(current_time, updated_last_time) < time_out) {
            updated_time.insert({{updated_last_time, it->first.second}, std::move(it->second)});
        }
        last_action.erase(it->first.second);
    }
    tasks.erase(tasks.begin(), it);
    for (auto &it2 : updated_time) {
        last_action.insert({it2.first.second, it2.second->get_last_action()});
        tasks.insert({it2.first, std::move(it2.second)});
    }
}

int64_t worker::timer::time_diff(time_point const& lhs, time_point const& rhs) const {
    return std::chrono::duration_cast<std::chrono::seconds>(lhs - rhs).count();
}

worker::worker(uint64_t time_out) : epoll_fd(epoll_create1(0)), clock(time_out) {}

void worker::add_client(std::unique_ptr<task> &new_client) {
    clock.add_client(new_client);
}

void worker::delete_client(task *c) {
    clock.delete_client(c);
}

void worker::execute(int time_out) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    bool need_to_quit = false;
    quit = &need_to_quit;

    std::array<epoll_event, EPOLL_MAX> events{};
    while (true) {
        int amount = epoll_wait(epoll_fd, events.data(), EPOLL_MAX, -1);
        if (need_to_quit) {
            return;
        }
        // std::cout << amount << std::endl;
        if (amount < 0) {
            throw std::runtime_error("worker::execute() epoll_wait()" + std::string(std::strerror(errno)));
        }
        for (auto it = events.begin(); it != events.begin() + amount; ++it) {
            auto current_task = static_cast<task*>(it->data.ptr);
            if (current_task->error_happend() || task::is_closing(it->events)) {
                current_task->run_disconnect();
                delete_client(current_task);
            } else {
                current_task->execute(it->events);
            }
        }
        clock.remove_old_clients();
    }
}

void worker::add(int fd, epoll_event *new_event) {
    int ctl_code = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, new_event);

    if (ctl_code < 0) {
        throw std::runtime_error("worker::add(), epoll_ctl" + std::string(std::strerror(errno)));
    }
}

void worker::edit(int fd, epoll_event *new_event) {
    int ctl_code = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, new_event);

    if (ctl_code < 0) {
        throw std::runtime_error("worker::edit(), epoll_ctl" + std::string(std::strerror(errno)));
    }
}

void worker::del(int fd, epoll_event *null) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, null);
}



