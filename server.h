//
// Created by andreybocharnikov on 25.12.2019.
//

#ifndef MY_SERVER_SERVER_H
#define MY_SERVER_SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <csignal>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "task.h"
#include "worker.h"


struct server{
    struct client{

        struct thread_for_client{
            thread_for_client();

            bool has_result() noexcept;

            bool has_work_to_do() noexcept;

            std::string get_required_result();

            void add_new_task(const char*, size_t);

            std::string handle_url(std::string&);

            ~thread_for_client();

        private:
            static const size_t send_max_length = 2048;
            static const size_t node_max_size = 64;

            const addrinfo hints;
            char url_address[node_max_size] = {0};

            std::mutex m;
            std::atomic<bool> has_work, quit;
            std::queue<std::string> results;
            std::string new_query;
            std::queue<std::string> queries;
            std::condition_variable cv;
            std::thread th;
        };

        client() = default;
        ~client() = default;

        void run(task *const, uint32_t);

        uint32_t change_mod();

    private:
        static const size_t max_query_len = 1000;
        // task::func run;
        char buffer[max_query_len] = {0};
        thread_for_client client_worker;
    };
    server(worker* main_loop, int port);

private:
    std::unordered_map<client*, std::unique_ptr<client>> save;
    // std::unordered_map<task*, std::unique_ptr<task>> save_taskes;
    std::unique_ptr<task> save_ptr;
};

#endif //MY_SERVER_SERVER_H
