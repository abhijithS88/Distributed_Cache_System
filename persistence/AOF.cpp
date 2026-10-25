#include "AOF.h"
#include <unistd.h>
#include <climits>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include <sstream>
#include <string>
#include <charconv>
#include <cstdio>
#include <cstdint>

static std::string filename = "persistence/appendonly.aof";
static std::uint64_t global_counter = 0;

void set_aof_path(const char* path) {
    filename = path;
}

uint64_t aof_sequence() {
    return global_counter;
}

static bool remove_counter_prefix(std::string &line, std::uint64_t* sequence = nullptr) {
    const size_t separator = line.find(' ');
    if (separator == std::string::npos || separator == 0) {
        return false;
    }

    std::uint64_t counter = 0;
    const char *begin = line.data();
    const char *end = begin + separator;
    const auto result = std::from_chars(begin, end, counter);
    if (result.ec != std::errc{} || result.ptr != end) {
        return false;
    }

    if (counter > global_counter) {
        global_counter = counter;
    }
    if (sequence != nullptr) {
        *sequence = counter;
    }
    line.erase(0, separator + 1);
    return true;
}

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

static std::string serialize_aof_line(const std::vector<std::string> &cmd, uint64_t sequence) {
    std::string line = std::to_string(sequence);
    line += ' ';
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (i != 0) {
            line += ' ';
        }
        line += cmd[i];
    }
    line += '\n';
    return line;
}

static void append_aof_line(const std::string& line) {
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
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

void append_only_file(std::vector<std::string> &cmd) {
    append_aof_line(serialize_aof_line(cmd, ++global_counter));
}

void append_only_file_at_sequence(std::vector<std::string> &cmd, uint64_t sequence) {
    append_aof_line(serialize_aof_line(cmd, sequence));
    if (sequence > global_counter) {
        global_counter = sequence;
    }
}

static bool write_all(int fd, const uint8_t* data, size_t size) {
    while (size > 0) {
        ssize_t n = write(fd, data, size);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data += n;
        size -= static_cast<size_t>(n);
    }
    return true;
}

static bool write_command_frame(int fd, std::vector<std::string>& cmd) {
    std::vector<uint8_t> payload;
    if (serialize(cmd, payload) < 0 || payload.size() > UINT32_MAX) {
        return false;
    }
    uint32_t len = static_cast<uint32_t>(payload.size());
    return write_all(fd, reinterpret_cast<const uint8_t*>(&len), sizeof(len)) &&
           write_all(fd, payload.data(), payload.size());
}

bool stream_aof_prefix(int output_fd, uint64_t sequence) {
    std::vector<std::string> reset_cmd{"sync_reset"};
    if (!write_command_frame(output_fd, reset_cmd)) {
        return false;
    }
    if (sequence == 0) {
        return true;
    }

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open AOF for replication");
        return false;
    }

    std::string line;
    char buffer[4096];
    uint64_t last_sequence = 0;
    bool ok = true;
    bool done = false;
    while (!done) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read AOF for replication");
            ok = false;
            break;
        }
        if (n == 0) {
            break;
        }

        for (ssize_t i = 0; i < n; ++i) {
            if (buffer[i] != '\n') {
                line += buffer[i];
                continue;
            }

            uint64_t record_sequence = 0;
            if (!remove_counter_prefix(line, &record_sequence)) {
                ok = false;
                done = true;
                break;
            }
            std::istringstream input(line);
            std::vector<std::string> command;
            std::string token;
            while (input >> token) {
                command.push_back(token);
            }
            line.clear();

            if (record_sequence <= sequence && !command.empty()) {
                std::vector<std::string> replicated{"replicate", std::to_string(record_sequence)};
                replicated.insert(replicated.end(), command.begin(), command.end());
                if (!write_command_frame(output_fd, replicated)) {
                    ok = false;
                    done = true;
                    break;
                }
                last_sequence = record_sequence;
            }
            if (record_sequence >= sequence) {
                done = true;
                break;
            }
        }
    }

    close(fd);
    return ok && last_sequence == sequence;
}

void reset_aof() {
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("truncate AOF");
        abort();
    }
    if (fsync(fd) < 0) {
        perror("fsync AOF");
        close(fd);
        abort();
    }
    close(fd);
    global_counter = 0;
}

void load_aof() {
    int fd = open(filename.c_str(), O_RDONLY);
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

        remove_counter_prefix(line);

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