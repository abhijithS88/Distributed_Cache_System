#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <map>

struct Response{
    uint32_t status = 0;
    std::vector<uint8_t>data;
};

enum {
    RES_OK = 0,
    RES_ERR = 1,    // error
    RES_NX = 2,     // key not found
};

void do_request(std::vector<std::string>& cmd, Response &res);