#ifndef SEGMENT_H
#define SEGMENT_H

#include <cstdint>
#include <vector>
#include <cstring>
#include "Superblock.h"

class Segment {
public:
    static const uint32_t SEGMENT_MAGIC = 0xBEEFBEEF;
    static const size_t HEADER_SIZE = 12;

    uint32_t segmentMagic;
    uint32_t segmentSequence;
    uint32_t headerChecksum;

    // Data buffer for log records; size = (segmentSize - Superblock::BLOCK_SIZE).
    std::vector<uint8_t> data;

    // segmentSize must be a multiple of Superblock::BLOCK_SIZE.
    Segment(uint32_t seq, size_t segmentSize);

    // Compute header checksum as (segmentMagic + segmentSequence).
    uint32_t computeHeaderChecksum() const;

    // Serialize header into a buffer padded to Superblock::BLOCK_SIZE.
    std::vector<uint8_t> serializeHeader() const;

    // (Optional) Deserialize a header.
    static bool deserializeHeader(const std::vector<uint8_t>& headerData, uint32_t &segSeq, bool &valid);
};

#endif // SEGMENT_H
