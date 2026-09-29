#pragma once
#include <vector>
#include <cstdint>
#include <iostream>
#include <cstring>

int32_t deserialize(const uint8_t* data, size_t len, std::vector<std::string> &cmd);
int32_t serialize(std::vector<std::string> &cmd, std::vector<uint8_t> &v);
bool is_write_command(std::vector<std::string> &cmd);