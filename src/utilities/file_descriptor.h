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


#ifndef LINUX_TCP_SERVERS_FILE_DESCRIPTOR_H
#define LINUX_TCP_SERVERS_FILE_DESCRIPTOR_H

#include <unistd.h>
#include <fcntl.h>

namespace concurrent_servers {
class file_descriptor {
public:
    explicit file_descriptor() = default;
    explicit file_descriptor(const int fd) : _fd{fd} {}

    bool set_fd(const int new_fd) {
        _fd = new_fd;
        return new_fd != -1;
    }

    int get_fd() const {
        return _fd;
    }

    bool set_nonblocking() const {
        int flags = fcntl(_fd, F_GETFL, 0);
        if (flags == -1) return false;
        return fcntl(_fd, F_SETFL, flags | O_NONBLOCK) == 0;
    }

    bool is_nonblocking() const {
        int flags = fcntl(_fd, F_GETFL, 0);
        return (flags & O_NONBLOCK) != 0;
    }

    void close_fd() {
        if (_fd != -1) {
            close(_fd);
            _fd = -1;
        }
    }

protected:
    int _fd = -1;
};
}

#endif //LINUX_TCP_SERVERS_FILE_DESCRIPTOR_H
