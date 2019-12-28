//
// Created by andreybocharnikov on 26.12.2019.
//

#include <iostream>
#include <unistd.h>
#include "task.h"

task::task(worker* const main_loop, func run, std::function<void()> disconnect, int file_descriptor)
    : main_loop(main_loop), run(std::move(run)), disconnect(std::move(disconnect)), fd(file_descriptor) {
    epoll_event new_event{(CLOSE_EVENTS | EPOLLIN), {this}};
    main_loop->add(fd, &new_event);
}

void task::Close() {
    is_error = true;
}

bool task::error_happend() {
    return is_error;
}

bool task::is_closing(uint32_t event) {
    return (event & EPOLLERR) || (event & EPOLLHUP) || (event & EPOLLRDHUP);
}

void task::execute(uint32_t events) {
    run(this, events);
}

task::~task() {
    // std::cout << "distructor" << std::endl;
    if (main_loop) {
        epoll_event event{0, {this}};
        main_loop->del(fd, &event);
    }
    close(fd);
}

int task::read(char *buffer, size_t size) {
    int code = recv(fd, buffer, size, 0);
    update_last_action();
    return code;
}

int task::write(const char *buffer, size_t size) {
    int code = send(fd, buffer, size, 0);
    update_last_action();
    return code;
}

void task::edit(uint32_t new_event) {
    epoll_event event{(CLOSE_EVENTS | new_event), {this}};
    main_loop->edit(fd, &event);
}

void task::run_disconnect() {
    disconnect();
}

task::time_point task::get_last_action() const {
    return last_action;
}

void task::update_last_action() {
    last_action = std::chrono::steady_clock::now();
}


