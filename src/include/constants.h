/*
 * Copyright (C) Pham Phi Long
 * Created on 3/11/19.
 */

#ifndef LINUX_TCP_SERVERS_CONSTANTS_H
#define LINUX_TCP_SERVERS_CONSTANTS_H

#define DEFAULT_BACKLOG 50
#define BUFF_SIZE 1000

namespace concurrent_servers {
    extern const char *DEFAULT_PORT;
    extern const int DEFAULT_WORKER_PROCESS_NUMBER;
}
#endif /* LINUX_TCP_SERVERS_CONSTANTS_H */