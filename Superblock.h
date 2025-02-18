#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <cstdint>
#include <vector>
#include <cstring>

class Superblock {
public:
    static const size_t BLOCK_SIZE = 4096;
    static const uint32_t MAGIC = 0xDEADBEEF;
    static const uint32_t VERSION = 1;

    Superblock();

    // Latest checkpoint information.
    uint64_t latestCheckpointSeq;     // Global sequence number of the checkpoint record.
    uint32_t latestCheckpointSegment;   // Segment number where the checkpoint was written.
    uint32_t latestCheckpointOffset;    // Offset (in bytes) within that segment.

    // Serialize the superblock into a BLOCK_SIZE–sized buffer.
    std::vector<uint8_t> serialize() const;

    // Deserialize a superblock from a BLOCK_SIZE–sized buffer.
    static Superblock deserialize(const std::vector<uint8_t>& data);

    uint32_t magic;
    uint32_t version;
};

#endif // SUPERBLOCK_H
