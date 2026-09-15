#include "process_request.h"

static std::map<std::string, std::string> g_data;

void do_request(std::vector<std::string> &cmd, Response &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        auto it = g_data.find(cmd[1]);
        if (it == g_data.end()) {
            out.status = RES_NX;    // not found
            return;
        }
        const std::string &val = it->second;
        out.data.assign(val.begin(), val.end());
    } 
    else if (cmd.size() == 3 && cmd[0] == "set") {
        g_data[cmd[1]].swap(cmd[2]);
    }
    else if (cmd.size() == 2 && cmd[0] == "del") {
        g_data.erase(cmd[1]);
    } 
    else {
        out.status = RES_ERR;
    }
}