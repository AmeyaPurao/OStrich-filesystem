#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include "Superblock.h"
#include "LogRecord.h"
#include "Segment.h"
#include "BlockManager.h"
#include <mutex>
#include <vector>

class LogManager {
public:
    // segmentSize must be a multiple of Superblock::BLOCK_SIZE (4096).
    LogManager(BlockManager *blockManager, size_t segmentSize);
    ~LogManager();

    // Append a log record.
    bool appendRecord(const LogRecord &record);

    // Append a checkpoint record and update the superblock.
    bool appendCheckpoint(const LogRecord &checkpointRecord);

    // Flush the current segment.
    bool flush();

    // Recovery: read the superblock and scan log segments.
    bool recover();

private:
    BlockManager *blockManager;
    size_t segmentSize;
    uint32_t currentSegmentSeq;
    size_t currentSegmentBlockStart; // Block index where the current segment begins.
    size_t currentOffset;       // Offset into current segment's data buffer.
    Segment *currentSegment;
    std::mutex logMutex;
    uint64_t globalSequence;    // Global log record sequence number.
    size_t blocksPerSegment;    // = segmentSize / Superblock::BLOCK_SIZE

    // Write the current segment's data to disk.
    bool writeCurrentSegment();

    // Start a new segment.
    bool startNewSegment();

    // Pad a buffer to BLOCK_SIZE bytes.
    static std::vector<uint8_t> padToBlockSize(const std::vector<uint8_t>& data);
};

#endif // LOG_MANAGER_H
