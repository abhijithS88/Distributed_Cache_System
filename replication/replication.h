#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

namespace replication {

enum class Phase {
    None,
    Snapshot,
    Backlog,
    Online
};

struct State {
    Phase phase = Phase::None;
    int pipe_fd = -1;
    pid_t child_pid = -1;
    size_t backlog_offset = 0;
    std::vector<uint8_t> backlog;
};

bool start_snapshot(State& state, int listen_fd, const std::vector<int>& inherited_fds,
                    uint64_t sequence);
bool should_poll_pipe(const State& state, size_t outgoing_size);
int pipe_fd(const State& state);
bool read_snapshot(State& state, std::vector<uint8_t>& outgoing, bool& want_write);
void queue_command(State& state, const std::vector<std::string>& command, uint64_t sequence,
                   std::vector<uint8_t>& outgoing, bool& want_write, bool& want_close);
void pump_backlog(State& state, std::vector<uint8_t>& outgoing, bool& want_write);
void cleanup(State& state);

}  // namespace replication
