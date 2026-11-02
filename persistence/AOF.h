#include "../command_parser/command_parser.h"
#include "../process_request/process_request.h"
#include <cstdint>

void set_aof_path(const char* path);
uint64_t aof_sequence();
void append_only_file(std::vector<std::string> &cmd);
void append_only_file_at_sequence(std::vector<std::string> &cmd, uint64_t sequence);
bool stream_aof_prefix(int output_fd, uint64_t sequence);
void load_aof();
void reset_aof();