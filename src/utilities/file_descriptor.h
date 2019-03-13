/*
 * Copyright (C) Pham Phi Long
 * Created on 3/11/19.
 */

#ifndef LINUX_TCP_SERVERS_FILE_DESCRIPTOR_H
#define LINUX_TCP_SERVERS_FILE_DESCRIPTOR_H

#include <unistd.h>
#include <fcntl.h>

namespace tcpserver {
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
