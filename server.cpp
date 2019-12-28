//
// Created by andreybocharnikov on 25.12.2019.
//

#include <stdexcept>
#include <cstring>
#include <memory>
#include <iostream>
#include "server.h"
#include "task.h"

server::server(worker* main_loop, int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Server() socket()" + std::string(std::strerror(errno)));
    }

    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY; //htonl
    local_addr.sin_port = htons(port);

    int bind_code = bind(server_fd, reinterpret_cast<sockaddr const*>(&local_addr), sizeof local_addr);
    if (bind_code < 0) {
        throw std::runtime_error("Server() bind()" + std::string(std::strerror(errno)));
    }

    int listen_code = listen(server_fd, SOMAXCONN);
    if (listen_code < 0) {
        throw std::runtime_error("Server() listen()" + std::string(std::strerror(errno)));
    }

    task::func add_new_client = [this, server_fd, main_loop] (task *const, uint32_t) {
        int new_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK);
        if (new_fd < 0) {
            // errno
            return;
        }

        try {
            std::unique_ptr<client> new_client = std::make_unique<client>();
            client* new_client_ptr = new_client.get();

            std::function<void()> disconnect = [this, new_client_ptr]() noexcept {
                // std::cout << "distconnect" << std::endl;
                save.erase(new_client_ptr);
            };

            std::unique_ptr<task> new_task = std::make_unique<task>(main_loop
                    ,[new_client_ptr](task* const cur_task, uint32_t event){new_client_ptr->run(cur_task, event);}
                    ,disconnect
                    ,new_fd);
            save.insert({new_client.get(), std::move(new_client)});
            main_loop->add_client(new_task);
            // save_taskes.insert({new_task.get(), std::move(new_task)});
        } catch (...) {}
    };
    save_ptr = std::make_unique<task>(main_loop, add_new_client, [](){}, server_fd);
}

server::client::thread_for_client::thread_for_client() : hints{0, AF_UNSPEC, SOCK_STREAM}, has_work(false), quit(false), th([this] {
    while (true) {
        std::unique_lock<std::mutex> lg(m);
        cv.wait(lg, [this] {
            return quit || has_work;
        });

        if (quit)
            break;

        std::string url = queries.front();
        queries.pop();
        lg.unlock();
        std::string result = handle_url(url);
        lg.lock();
        has_work = !queries.empty();
        results.push(result);
    }
})
{}

bool server::client::thread_for_client::has_result() noexcept {
    std::unique_lock<std::mutex> lg(m);
    return !results.empty();
}

bool server::client::thread_for_client::has_work_to_do() noexcept {
    return has_work;
}

std::string server::client::thread_for_client::get_required_result() {
    std::unique_lock<std::mutex> lg(m);
    if (results.empty()) {
        throw std::runtime_error("server::client::thread_for_client::get_required_result() - result is empty");
    }
    if (results.front().length() <= send_max_length) { // TODO change max_length for test
        auto tmp = results.front();
        results.pop();
        return tmp;
    } else {
        auto short_string = results.front().substr(0, send_max_length);
        results.front().erase(0, send_max_length);
        return short_string;
    }
}

void server::client::thread_for_client::add_new_task(const char *query_buffer, size_t size) {
    std::unique_lock<std::mutex> lg(m);
    size_t l = 0;
    for (size_t i = 0; i < size; i++) {
        if (query_buffer[i] == '\0' || query_buffer[i] == '\n' || query_buffer[i] == '\r') {
            new_query.append(query_buffer + l, query_buffer + i);
            l = i + 1;
            if (new_query.length() > 0) {
                queries.push(new_query);
            }
            new_query.clear();
        }
    }
    new_query.append(query_buffer + l, query_buffer + size);
    has_work.store(!queries.empty());
    cv.notify_all();
}

std::string server::client::thread_for_client::handle_url(std::string &url) {
    addrinfo *res = nullptr;
    int error_code = getaddrinfo(url.c_str(), nullptr, &hints, &res);
    std::string result = "URL: " + url + '\n';
    if (error_code != 0) {
        result += "Getaddrinfo() failed ";
        result += gai_strerror(error_code);
        result += '\n';
        return result;
    }

    void *ptr = nullptr;
    for (addrinfo *node = res; node; node = node->ai_next) {
        if (node->ai_family == AF_INET) {
            ptr = &(reinterpret_cast<sockaddr_in *>(node->ai_addr)->sin_addr);
        } else if (node->ai_family == AF_INET6) {
            ptr = &(reinterpret_cast<sockaddr_in6 *>(node->ai_addr)->sin6_addr);
        } else {
            continue;
        }
        inet_ntop(node->ai_family, ptr, url_address, node_max_size);
        result += url_address;
        // std::cout << url_address << std::endl;
        result += "\n";
    }
    freeaddrinfo(res);
    return result;
}

server::client::thread_for_client::~thread_for_client() {
    {
        std::unique_lock<std::mutex> lg(m);
        quit = true;
        cv.notify_all();
        kill(th.native_handle(), SIGINT);
    }
    th.join();
}

void server::client::run(task *const current_task, uint32_t events) {
    if ((events & EPOLLOUT) && client_worker.has_result()) {
        std::string tmp = client_worker.get_required_result();
        int code = current_task->write(tmp.c_str(), tmp.length());
        if (code < 0) {
            current_task->Close();
        }
    } else if (events & EPOLLIN) {
        int code = current_task->read(buffer, max_query_len);
        if (code > 0) {
            try {
                client_worker.add_new_task(buffer, code);
            } catch (...) {
                current_task->Close();
            }
        } else {
            current_task->Close();
        }
    }
    current_task->edit(change_mod());
}

uint32_t server::client::change_mod() {
    uint32_t new_mod = EPOLLIN;

    if (client_worker.has_result() || client_worker.has_work_to_do()) {
        new_mod |= EPOLLOUT;
    }
    return new_mod;
}
