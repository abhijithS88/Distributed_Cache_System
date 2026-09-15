#pragma once
#include <vector>
#include <cstdint>
#include <iostream>
#include <cstring>

int32_t command_parser(const uint8_t* data, size_t len, std::vector<std::string> &cmd);