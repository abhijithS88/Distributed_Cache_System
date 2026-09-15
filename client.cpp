#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>


static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static int32_t read_full(int fd, uint8_t *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const uint8_t *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// append to the back
static void
buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

const size_t k_max_msg = 65536;

static int32_t send_req(int fd, const std::vector<std::string> &args) {
    std::vector<uint8_t> payload;
    uint32_t arg_count = static_cast<uint32_t>(args.size());
    buf_append(payload, (const uint8_t *)&arg_count, 4);

    for (const std::string &arg : args) {
        uint32_t arg_len = static_cast<uint32_t>(arg.size());
        buf_append(payload, (const uint8_t *)&arg_len, 4);
        buf_append(payload, (const uint8_t *)arg.data(), arg.size());
    }

    if (payload.size() > k_max_msg) {
        return -1;
    }

    std::vector<uint8_t> wbuf;
    uint32_t msg_len = static_cast<uint32_t>(payload.size());
    buf_append(wbuf, (const uint8_t *)&msg_len, 4);
    buf_append(wbuf, payload.data(), payload.size());
    return write_all(fd, wbuf.data(), wbuf.size());
}

static int32_t read_res(int fd) {
    // 4 bytes header
    std::vector<uint8_t> rbuf;
    rbuf.resize(4);
    errno = 0;
    int32_t err = read_full(fd, &rbuf[0], 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf.data(), 4);
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // reply body
    rbuf.resize(4 + len);
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    if (len < 4) {
        msg("invalid response");
        return -1;
    }

    uint32_t status = 0;
    memcpy(&status, &rbuf[4], 4);
    uint32_t data_len = len - 4;
    printf("status:%u data:%.*s\n", status,
           data_len < 100 ? data_len : 100, &rbuf[8]);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4005);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }

    std::string input;
    while (std::getline(std::cin, input)) {
        std::istringstream input_stream(input);
        std::vector<std::string> args;
        std::string arg;
        while (input_stream >> arg) {
            args.push_back(arg);
        }

        if (send_req(fd, args) != 0) {
            close(fd);
            return 1;
        }

        if (read_res(fd) != 0) {
            close(fd);
            return 1;
        }
    }

    close(fd);
    return 0;
}