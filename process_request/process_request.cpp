#include "process_request.h"
#include "../container_of/container_of.h"

#include <new>
#include <iostream>
#include <charconv>

static uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

static bool entry_eq(node* lhs, node* rhs){
    if(lhs == nullptr || rhs == nullptr)return false;
    const entry* left = container_of(lhs, entry, ptr);
    const entry* right = container_of(rhs, entry, ptr);
    return left->key == right->key;
}

static void delete_entries(table* tab) {
    if (tab == nullptr || tab->arr == nullptr) {
        return;
    }
    for (int64_t bucket_index = 0; bucket_index <= tab->mask; ++bucket_index) {
        node* current = tab->arr[bucket_index];
        current = current == nullptr ? nullptr : current->next;
        while (current != nullptr) {
            node* next = current->next;
            delete container_of(current, entry, ptr);
            current = next;
        }
    }
}

void reset_database() {
    delete_entries(data.db.older);
    if (data.db.newer != data.db.older) {
        delete_entries(data.db.newer);
    }
    clear_map(&data.db);
}

bool do_get(std::vector<std::string> &cmd, Response &out){
    if(cmd.size() != 2){
        out.status = RES_ERR;
        return false;
    }
    entry key{};
    key.key = cmd[1];
    key.ptr.hash_value = (int64_t)(str_hash((const uint8_t*)(key.key.data()), key.key.size()));
    node* found_node = lookup(&data.db, &key.ptr, entry_eq);
    out.data.clear();
    if(found_node == nullptr){
        out.status = RES_NX;
        return true;
    }
    entry* found = container_of(found_node, entry, ptr);
    out.status = RES_OK;
    out.data.assign(found->val.begin(), found->val.end());
    return true;
}

bool do_set(std::vector<std::string> &cmd, Response &out){
    if(cmd.size() != 3){
        out.status = RES_ERR;
        return false;
    }
    entry key{};
    key.key = cmd[1];
    key.ptr.hash_value = (int64_t)(str_hash((const uint8_t*)(key.key.data()), key.key.size()));
    node* found_node = lookup(&data.db, &key.ptr, entry_eq);
    entry* found = found_node == nullptr ? nullptr : container_of(found_node, entry, ptr);
    if(found != nullptr){
        found->val = cmd[2];
    }
    else{
        entry* inserted = new (std::nothrow) entry{};
        if(inserted == nullptr){
            out.status = RES_ERR;
            return false;
        }
        inserted->key = cmd[1];
        inserted->val = cmd[2];
        inserted->ptr.hash_value = key.ptr.hash_value;
        size_t old_size = size(&data.db);
        insert(&data.db, &inserted->ptr);
        if(size(&data.db) == old_size){
            delete inserted;
            out.status = RES_ERR;
            return false;
        }
    }
    out.status = RES_OK;
    out.data.clear();
    return true;
}

bool do_del(std::vector<std::string> &cmd, Response &out){
    if(cmd.size() != 2){
        out.status = RES_ERR;
        return false;
    }
    entry key{};
    key.key = cmd[1];
    key.ptr.hash_value = (int64_t)(str_hash((const uint8_t*)(key.key.data()), key.key.size()));
    out.data.clear();
    node* removed = delete_node(&data.db, &key.ptr, entry_eq);
    if(removed == nullptr){
        out.status = RES_NX;
        return true;
    }
    delete container_of(removed, entry, ptr);
    out.status = RES_OK;
    return true;
}

RequestResult do_request(std::vector<std::string> &cmd, Response &out, uint64_t* replication_sequence) {
    if (cmd.size() == 1 && cmd[0] == "sync") {
        out.status = RES_OK;
        return RequestResult::RegisterReplica;
    }
    if (cmd.size() == 1 && cmd[0] == "sync_reset") {
        reset_database();
        out.status = RES_OK;
        return RequestResult::ResetDatabase;
    }
    if (cmd.size() >= 4 && cmd[0] == "replicate") {
        uint64_t sequence = 0;
        const char* begin = cmd[1].data();
        const char* end = begin + cmd[1].size();
        const auto parsed = std::from_chars(begin, end, sequence);
        if (parsed.ec != std::errc{} || parsed.ptr != end) {
            out.status = RES_ERR;
            return RequestResult::Invalid;
        }

        cmd.erase(cmd.begin(), cmd.begin() + 2);
        if (cmd.empty() || (cmd[0] != "set" && cmd[0] != "del")) {
            out.status = RES_ERR;
            return RequestResult::Invalid;
        }
        RequestResult result = do_request(cmd, out);
        if (result != RequestResult::Processed || out.status != RES_OK) {
            return RequestResult::Invalid;
        }
        if (replication_sequence != nullptr) {
            *replication_sequence = sequence;
        }
        return RequestResult::ReplicatedWrite;
    }
    if (cmd.size() == 2 && cmd[0] == "get") {
        return do_get(cmd, out) ? RequestResult::Processed : RequestResult::Invalid;
    } 
    else if (cmd.size() == 3 && cmd[0] == "set") {
        return do_set(cmd, out) ? RequestResult::Processed : RequestResult::Invalid;
    }
    else if (cmd.size() == 2 && cmd[0] == "del") {
        return do_del(cmd, out) ? RequestResult::Processed : RequestResult::Invalid;
    } 
    else {
        out.status = RES_ERR;
        return RequestResult::Invalid;
    }
}