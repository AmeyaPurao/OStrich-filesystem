#include "Segment.h"

Segment::Segment(uint32_t seq, size_t segmentSize)
    : segmentMagic(SEGMENT_MAGIC), segmentSequence(seq),
      data(segmentSize - Superblock::BLOCK_SIZE, 0)
{
    headerChecksum = computeHeaderChecksum();
}

uint32_t Segment::computeHeaderChecksum() const {
    return segmentMagic + segmentSequence;
}

std::vector<uint8_t> Segment::serializeHeader() const {
    std::vector<uint8_t> buffer(Superblock::BLOCK_SIZE, 0);
    size_t offset = 0;
    std::memcpy(buffer.data() + offset, &segmentMagic, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    std::memcpy(buffer.data() + offset, &segmentSequence, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    std::memcpy(buffer.data() + offset, &headerChecksum, sizeof(uint32_t));
    return buffer;
}

bool Segment::deserializeHeader(const std::vector<uint8_t>& headerData, uint32_t &segSeq, bool &valid) {
    if (headerData.size() < HEADER_SIZE) {
        valid = false;
        return false;
    }
    uint32_t magic, seq, chksum;
    size_t offset = 0;
    std::memcpy(&magic, headerData.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    std::memcpy(&seq, headerData.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    std::memcpy(&chksum, headerData.data() + offset, sizeof(uint32_t));
    valid = (magic == SEGMENT_MAGIC) && (chksum == magic + seq);
    segSeq = seq;
    return valid;
}
