#include "replication.h"

#include "../command_parser/command_parser.h"
#include "../persistence/AOF.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

namespace replication {
namespace {

constexpr size_t kMaxOutgoing = 256 * 1024;
constexpr size_t kMaxBacklog = 16 * 1024 * 1024;

void append_bytes(std::vector<uint8_t>& dest, const uint8_t* data, size_t size) {
    dest.insert(dest.end(), data, data + size);
}

bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }
    return true;
}

bool encode_frame(std::vector<std::string>& command, std::vector<uint8_t>& frame) {
    std::vector<uint8_t> payload;
    if (serialize(command, payload) < 0 || payload.size() > UINT32_MAX) {
        return false;
    }
    uint32_t length = static_cast<uint32_t>(payload.size());
    frame.clear();
    append_bytes(frame, reinterpret_cast<const uint8_t*>(&length), sizeof(length));
    append_bytes(frame, payload.data(), payload.size());
    return true;
}

}  // namespace

bool start_snapshot(State& state, int listen_fd, const std::vector<int>& inherited_fds,
                    uint64_t sequence) {
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        return false;
    }

    pid_t child = fork();
    if (child < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return false;
    }

    if (child == 0) {
        close(pipe_fds[0]);
        close(listen_fd);
        for (int fd : inherited_fds) {
            if (fd != pipe_fds[1]) {
                close(fd);
            }
        }
        bool ok = stream_aof_prefix(pipe_fds[1], sequence);
        close(pipe_fds[1]);
        _exit(ok ? 0 : 1);
    }

    close(pipe_fds[1]);
    if (!set_nonblocking(pipe_fds[0])) {
        close(pipe_fds[0]);
        kill(child, SIGTERM);
        waitpid(child, nullptr, 0);
        return false;
    }

    state.phase = Phase::Snapshot;
    state.pipe_fd = pipe_fds[0];
    state.child_pid = child;
    state.backlog_offset = 0;
    state.backlog.clear();
    return true;
}

bool should_poll_pipe(const State& state, size_t outgoing_size) {
    return state.phase == Phase::Snapshot && state.pipe_fd >= 0 && outgoing_size < kMaxOutgoing;
}

int pipe_fd(const State& state) {
    return state.pipe_fd;
}

void pump_backlog(State& state, std::vector<uint8_t>& outgoing, bool& want_write) {
    if (state.phase != Phase::Backlog) {
        return;
    }

    size_t room = kMaxOutgoing > outgoing.size() ? kMaxOutgoing - outgoing.size() : 0;
    size_t available = state.backlog.size() - state.backlog_offset;
    size_t count = std::min(room, available);
    if (count > 0) {
        append_bytes(outgoing, state.backlog.data() + state.backlog_offset, count);
        state.backlog_offset += count;
        want_write = true;
    }
    if (state.backlog_offset == state.backlog.size()) {
        state.backlog.clear();
        state.backlog_offset = 0;
        state.phase = Phase::Online;
    }
}

bool read_snapshot(State& state, std::vector<uint8_t>& outgoing, bool& want_write) {
    uint8_t buffer[65536];
    size_t room = kMaxOutgoing > outgoing.size() ? kMaxOutgoing - outgoing.size() : 0;
    size_t amount = std::min(room, sizeof(buffer));
    if (amount == 0) {
        return true;
    }

    ssize_t n = read(state.pipe_fd, buffer, amount);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return true;
        }
        return false;
    }
    if (n > 0) {
        append_bytes(outgoing, buffer, static_cast<size_t>(n));
        want_write = true;
        return true;
    }

    close(state.pipe_fd);
    state.pipe_fd = -1;
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(state.child_pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    state.child_pid = -1;
    if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return false;
    }

    state.phase = Phase::Backlog;
    pump_backlog(state, outgoing, want_write);
    return true;
}

void queue_command(State& state, const std::vector<std::string>& command, uint64_t sequence,
                   std::vector<uint8_t>& outgoing, bool& want_write, bool& want_close) {
    std::vector<std::string> framed_command{"replicate", std::to_string(sequence)};
    framed_command.insert(framed_command.end(), command.begin(), command.end());
    std::vector<uint8_t> frame;
    if (!encode_frame(framed_command, frame)) {
        want_close = true;
        return;
    }

    std::vector<uint8_t>& destination =
        state.phase == Phase::Snapshot || state.phase == Phase::Backlog ? state.backlog : outgoing;
    if (destination.size() + frame.size() > kMaxBacklog) {
        want_close = true;
        return;
    }
    append_bytes(destination, frame.data(), frame.size());
    if (&destination == &outgoing) {
        want_write = true;
    }
}

void cleanup(State& state) {
    if (state.pipe_fd >= 0) {
        close(state.pipe_fd);
        state.pipe_fd = -1;
    }
    if (state.child_pid > 0) {
        kill(state.child_pid, SIGTERM);
        while (waitpid(state.child_pid, nullptr, 0) < 0 && errno == EINTR) {
        }
        state.child_pid = -1;
    }
    state.backlog.clear();
    state.backlog_offset = 0;
    state.phase = Phase::None;
}

}  // namespace replication
