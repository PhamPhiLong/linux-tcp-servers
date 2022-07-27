#include "servers/multi_worker_nonblocking_io_multiplexing_server.h"

int main(int argc, char *argv[]) {
    const std::string port_num = (argc >= 2) ? argv[1] : concurrent_servers::DEFAULT_PORT;
    const int backlog = (argc >= 3) ? atoi(argv[2]) : DEFAULT_BACKLOG;
    const int worker_num = (argc >= 4) ? atoi(argv[3]) : concurrent_servers::DEFAULT_WORKER_PROCESS_NUMBER;
    MultiWorkerIoMultiplexingTCPServer server{port_num, backlog, worker_num, true};
    server.start();
}