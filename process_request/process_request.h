#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include <cstdint>
#include "../hashtable/hashtable.h"

struct Response{
    uint32_t status = 0;
    std::vector<uint8_t>data;
};

struct Database {
    table older;
    table newer;
    Map db;

    Database() {
        db.older = &older;
        db.newer = &newer;
    }
};

static Database data;

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

enum class RequestResult {
    Processed,
    RegisterReplica,
    ResetDatabase,
    ReplicatedWrite,
    Invalid
};

RequestResult do_request(std::vector<std::string>& cmd, Response &res, uint64_t* replication_sequence = nullptr);
void reset_database();