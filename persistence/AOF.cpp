#include "AOF.h"
#include <unistd.h>
#include <climits>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include <sstream>
#include <string>

const char* filename = "./persistence/appendonly.aof";

static std::string read_all_from_fd(int fd) {
    std::string data;
    char buf[4096];

    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            close(fd);
            abort();
        }
        if (n == 0) {
            break;
        }
        data.append(buf, (size_t)(n));
    }

    return data;
}

static std::string serialize_aof_line(const std::vector<std::string> &cmd) {
    std::string line;
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (i != 0) {
            line += ' ';
        }
        line += cmd[i];
    }
    line += '\n';
    return line;
}

void append_only_file(std::vector<std::string> &cmd) {
    std::string line = serialize_aof_line(cmd);

    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open");
        abort();
    }

    const char *data = line.data();
    size_t remaining = line.size();
    while (remaining > 0) {
        ssize_t n = write(fd, data, remaining);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("write");
            close(fd);
            abort();
        }
        data += n;
        remaining -= (size_t)(n);
    }

    fsync(fd);
    close(fd);
}

void load_aof() {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            return;
        }
        perror("open");
        abort();
    }

    std::string data = read_all_from_fd(fd);
    close(fd);

    size_t start = 0;
    while (start < data.size()) {
        size_t end = start;
        while (end < data.size() && data[end] != '\n') {
            ++end;
        }

        if (start == end) {
            ++start;
            continue;
        }

        std::string line = data.substr(start, end - start);
        start = end + 1;

        std::istringstream iss(line);
        std::vector<std::string> cmd;
        std::string token;
        while (iss >> token) {
            cmd.push_back(token);
        }

        if (cmd.empty()) {
            continue;
        }

        Response out{};
        do_request(cmd, out);
    }
}