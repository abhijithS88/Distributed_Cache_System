#include <cstdint>
#include <cstddef>

bool is_master(const char* IP, uint16_t PORT);
bool get_master_address(char* IP, size_t IP_SIZE, uint16_t* PORT);