#pragma once

#include <vector>
#include <string>
#include <regex>


struct ipBytes
{
    uint8_t by1;
    uint8_t by2;
    uint8_t by3;
    uint8_t by4;
};

inline uint32_t cidr_to_u32(unsigned int cidr)
{
    return (cidr == 0 ? 0 : ((uint32_t)0xffffffff << (32 - cidr)));
}

inline int u32_to_cidr(uint32_t u32_cidr)
{
    int mask = 0;

    while((u32_cidr << mask) != 0)
        mask++;
        
    return mask;
}

inline ipBytes ip_u32_to_bytes(uint32_t ip)
{
    uint8_t by1 = (uint8_t)0xff & ((uint32_t)ip >> 24);
    uint8_t by2 = (uint8_t)0xff & ((uint32_t)ip >> 16);
    uint8_t by3 = (uint8_t)0xff & ((uint32_t)ip >> 8);
    uint8_t by4 = (uint8_t)0xff & ((uint32_t)ip);

    return {by1, by2, by3, by4};
}

inline ipBytes ip_u32_little_endian_to_bytes(uint32_t ip)
{
    uint8_t by4 = (uint8_t)0xff & ((uint32_t)ip >> 24);
    uint8_t by3 = (uint8_t)0xff & ((uint32_t)ip >> 16);
    uint8_t by2 = (uint8_t)0xff & ((uint32_t)ip >> 8);
    uint8_t by1 = (uint8_t)0xff & ((uint32_t)ip);

    return {by1, by2, by3, by4};
}

inline uint32_t ip_bytes_to_u32(ipBytes bytes)
{
    return
        (uint32_t)bytes.by1 << 24 |
        (uint32_t)bytes.by2 << 16 |
        (uint32_t)bytes.by3 << 8 |
        (uint32_t)bytes.by4;

}

inline uint32_t ip_bytes_to_u32_little_endian(ipBytes bytes)
{
    return
        (uint32_t)bytes.by4 << 24 |
        (uint32_t)bytes.by3 << 16 |
        (uint32_t)bytes.by2 << 8 |
        (uint32_t)bytes.by1;

}

inline ipBytes ip_string_to_bytes(std::string address)
{
    ipBytes bytes;
    size_t posDelim;
    std::string substrRemain = address;

    bytes.by1 = std::stoi(substrRemain.substr(0, substrRemain.find('.')));
    substrRemain = substrRemain.substr(substrRemain.find('.') + 1, substrRemain.length());

    bytes.by2 = std::stoi(substrRemain.substr(0, substrRemain.find('.')));
    substrRemain = substrRemain.substr(substrRemain.find('.') + 1, substrRemain.length());

    bytes.by3 = std::stoi(substrRemain.substr(0, substrRemain.find('.')));
    substrRemain = substrRemain.substr(substrRemain.find('.') + 1, substrRemain.length());

    bytes.by4 = std::stoi(substrRemain);

    return bytes;
}

inline std::string ip_bytes_to_string(ipBytes bytes)
{
    return
        std::to_string(bytes.by1) + "." +
        std::to_string(bytes.by2) + "." +
        std::to_string(bytes.by3) + "." +
        std::to_string(bytes.by4);
}

inline bool ip_valid_cidr(std::string address)
{
    return std::regex_match(address, std::regex("(\\d{1,3}\\.){3}\\d{1,3}/\\d{1,2}"));
}

inline uint8_t get_cidr_netmask(std::string address)
{
    return std::stoi(address.substr(address.find('/') +1, address.length()));
}

inline std::string get_address_wo_netmask(std::string address)
{
    return address.substr(0, address.find('/'));
}
