#include "command_parser.h"

const size_t k_max_args = 200 * 1000;

static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
    if (cur + 4 > end) {
        return false;
    }
    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out) {
    if (cur + n > end) {
        return false;
    }
    out.assign(cur, cur + n);
    cur += n;
    return true;
}

static void append_u32(std::vector<uint8_t> &v, uint32_t value) {
    uint8_t bytes[4];
    memcpy(bytes, &value, 4);
    v.insert(v.end(), bytes, bytes + 4);
}

// +------+-----+------+-----+------+-----+-----+------+
// | nstr | len | str1 | len | str2 | ... | len | strn |
// +------+-----+------+-----+------+-----+-----+------+

int32_t deserialize(const uint8_t *data, size_t size, std::vector<std::string> &out) {
    out.clear();

    const uint8_t *cur = data;
    const uint8_t *end = data + size;
    uint32_t nstr = 0;
    if (!read_u32(cur, end, nstr)) {
        return -1;
    }
    if (nstr > k_max_args) {
        return -1;  // safety limit
    }

    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(cur, end, len)) {
            return -1;
        }
        std::string arg;
        if (!read_str(cur, end, len, arg)) {
            return -1;
        }
        out.push_back(arg);
    }
    if (cur != end) {
        return -1;  // trailing garbage
    }
    return 0;
}

bool is_write_command(std::vector<std::string> &cmd){
    if (cmd.empty()) {
        return false;
    }
    return cmd[0] == "set" || cmd[0] == "del";
}

int32_t serialize(std::vector<std::string> &cmd, std::vector<uint8_t> &v){
    v.clear();

    if (cmd.size() > k_max_args) {
        return -1;
    }

    uint32_t nstr = (uint32_t)(cmd.size());
    append_u32(v, nstr);

    for (const auto &arg : cmd) {
        if (arg.size() > UINT32_MAX) {
            return -1;
        }
        uint32_t len = (uint32_t)(arg.size());
        append_u32(v, len);
        v.insert(v.end(), arg.begin(), arg.end());
    }

    return 0;
}