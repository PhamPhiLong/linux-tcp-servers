/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2019, Pham Phi Long <phamphilong2010@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "multi_worker_reuseport_nonblocking_io_multiplexing_server.h"
#include "print_utility.h"

struct read_handler {
    void operator()(const std::string &worker_process_id, char* buff, size_t buff_len) const {
        concurrent_servers::log_info(worker_process_id, "  received: ", std::string{buff, buff_len});
    }
};
int main(int argc, char *argv[]) {
    const std::string port_num = (argc >= 2) ? argv[1] : concurrent_servers::DEFAULT_PORT;
    const int backlog = (argc >= 3) ? atoi(argv[2]) : DEFAULT_BACKLOG;
    const int worker_process_num = (argc >= 4) ? atoi(argv[3]) : concurrent_servers::DEFAULT_WORKER_PROCESS_NUMBER;
    const concurrent_servers::linux_concurrent_server<read_handler> server{worker_process_num, port_num, backlog};

    server.start();
}