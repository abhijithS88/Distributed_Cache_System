#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include "../hashtable/hashtable.h"

struct Response{
    uint32_t status = 0;
    std::vector<uint8_t>data;
};

static struct {
    Map db;    // top-level hashtable
} data;

struct entry{
    node ptr;
    std::string key;
    std::string val;
};

enum {
    RES_OK = 0,
    RES_ERR = 1,    // error
    RES_NX = 2,     // key not found
};

bool do_request(std::vector<std::string>& cmd, Response &res);