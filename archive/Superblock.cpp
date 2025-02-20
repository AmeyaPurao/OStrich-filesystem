#include "Superblock.h"

Superblock::Superblock()
    : magic(MAGIC), version(VERSION),
      latestCheckpointSeq(0), latestCheckpointSegment(0), latestCheckpointOffset(0)
{
}

std::vector<uint8_t> Superblock::serialize() const {
    std::vector<uint8_t> buffer(BLOCK_SIZE, 0);
    size_t offset = 0;
    auto write_uint32 = [&](uint32_t value) {
        std::memcpy(&buffer[offset], &value, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    };
    auto write_uint64 = [&](uint64_t value) {
        std::memcpy(&buffer[offset], &value, sizeof(uint64_t));
        offset += sizeof(uint64_t);
    };

    write_uint32(magic);
    write_uint32(version);
    write_uint64(latestCheckpointSeq);
    write_uint32(latestCheckpointSegment);
    write_uint32(latestCheckpointOffset);
    return buffer;
}

Superblock Superblock::deserialize(const std::vector<uint8_t>& data) {
    Superblock sb;
    size_t offset = 0;
    auto read_uint32 = [&](uint32_t &value) {
        std::memcpy(&value, &data[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);
    };
    auto read_uint64 = [&](uint64_t &value) {
        std::memcpy(&value, &data[offset], sizeof(uint64_t));
        offset += sizeof(uint64_t);
    };

    read_uint32(sb.magic);
    read_uint32(sb.version);
    read_uint64(sb.latestCheckpointSeq);
    read_uint32(sb.latestCheckpointSegment);
    read_uint32(sb.latestCheckpointOffset);
    return sb;
}
