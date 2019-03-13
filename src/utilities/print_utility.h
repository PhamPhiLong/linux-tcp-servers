/*
 * Copyright (C) Pham Phi Long
 * Created on 3/11/19.
 */

#ifndef LINUX_TCP_SERVERS_PRINT_UTILITY_H
#define LINUX_TCP_SERVERS_PRINT_UTILITY_H

#include <string>
#include <iostream>

namespace tcpserver {
    template<typename Content>
    void log(const Content &content) {
        std::cout << content << "\033[0m" << std::endl;
    }

    template<typename Content, typename... LogContents>
    void log(const Content &content, const LogContents &... log_contents) {
        std::cout << content;
        return log(log_contents...);
    }
}

#endif /* LINUX_TCP_SERVERS_PRINT_UTILITY_H */