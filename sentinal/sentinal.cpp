#include "sentinal.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

static const char* MASTER_FILE = "sentinal/IPs/master.ip";

bool is_master(const char* IP, uint16_t PORT) {

    int fd = open(MASTER_FILE, O_RDONLY);
    if (fd < 0) {
        if (errno != ENOENT) {
            perror("open master.ip");
        }
        return false;
    }

    char buffer[256] = {};
    size_t used = 0;
    while (used < sizeof(buffer) - 1) {
        ssize_t n = read(fd, buffer + used, sizeof(buffer) - 1 - used);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read master.ip");
            close(fd);
            return false;
        }
        if (n == 0) {
            break;
        }
        used += (size_t)(n);
    }
    close(fd);
    buffer[used] = '\0';

    char master_ip[64] = {};
    unsigned int master_port = 0;
    if (sscanf(buffer, "%63s %u", master_ip, &master_port) != 2 ||
        master_port > UINT16_MAX) {
        return false;
    }

    return strcmp(IP, master_ip) == 0 &&
           PORT == static_cast<uint16_t>(master_port);
}