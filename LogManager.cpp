#include "LogManager.h"
#include <iostream>
#include <cstring>

static const size_t BLOCK_SIZE = Superblock::BLOCK_SIZE;

LogManager::LogManager(BlockManager *blockManager, size_t segmentSize)
    : blockManager(blockManager), segmentSize(segmentSize),
      currentSegmentSeq(1), globalSequence(1)
{
    if (segmentSize % BLOCK_SIZE != 0) {
        std::cerr << "Error: segmentSize must be a multiple of BLOCK_SIZE (" << BLOCK_SIZE << ").\n";
        exit(1);
    }
    blocksPerSegment = segmentSize / BLOCK_SIZE;

    // Reserve block 0 for superblock; segments start at block 1.
    currentSegmentBlockStart = 1;
    currentOffset = 0;
    currentSegment = new Segment(currentSegmentSeq, segmentSize);
    std::vector<uint8_t> headerData = currentSegment->serializeHeader();
    if (!blockManager->writeBlock(currentSegmentBlockStart, headerData)) {
        std::cerr << "Failed to write segment header at block " << currentSegmentBlockStart << "\n";
    }
}

LogManager::~LogManager() {
    flush();
    delete currentSegment;
}

std::vector<uint8_t> LogManager::padToBlockSize(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> padded = data;
    if (padded.size() < BLOCK_SIZE) {
        padded.resize(BLOCK_SIZE, 0);
    }
    return padded;
}

bool LogManager::appendRecord(const LogRecord &record) {
    std::lock_guard<std::mutex> lock(logMutex);

    // Make a copy so we can assign a sequence number.
    LogRecord recordCopy = record;
    recordCopy.setSequenceNumber(globalSequence);

    std::vector<uint8_t> serialized = recordCopy.serialize();
    size_t recordSize = serialized.size();

    if (currentOffset + recordSize > currentSegment->data.size()) {
        if (!writeCurrentSegment()) {
            return false;
        }
        if (!startNewSegment()) {
            return false;
        }
    }

    std::memcpy(currentSegment->data.data() + currentOffset, serialized.data(), recordSize);
    currentOffset += recordSize;
    globalSequence++; // Advance global sequence internally.
    return true;
}

bool LogManager::appendCheckpoint(const LogRecord &checkpointRecord) {
    if (!appendRecord(checkpointRecord)) {
        return false;
    }

    Superblock sb;
    sb.latestCheckpointSeq = checkpointRecord.getSequenceNumber();
    sb.latestCheckpointSegment = currentSegmentSeq;
    sb.latestCheckpointOffset = static_cast<uint32_t>(currentOffset);

    std::vector<uint8_t> sbData = sb.serialize();
    if (!blockManager->writeBlock(0, sbData)) {
        std::cerr << "Failed to write updated superblock to block 0.\n";
        return false;
    }
    return true;
}

bool LogManager::flush() {
    std::lock_guard<std::mutex> lock(logMutex);
    return writeCurrentSegment();
}

bool LogManager::writeCurrentSegment() {
    size_t blocksNeeded = (currentOffset + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (size_t i = 0; i < blocksNeeded; ++i) {
        size_t blockIndex = currentSegmentBlockStart + 1 + i;
        size_t offsetInData = i * BLOCK_SIZE;
        size_t bytesToWrite = BLOCK_SIZE;
        if (offsetInData + bytesToWrite > currentOffset) {
            bytesToWrite = currentOffset - offsetInData;
        }
        std::vector<uint8_t> blockData(BLOCK_SIZE, 0);
        std::memcpy(blockData.data(), currentSegment->data.data() + offsetInData, bytesToWrite);
        if (!blockManager->writeBlock(blockIndex, blockData)) {
            std::cerr << "Failed to write segment data block at block " << blockIndex << "\n";
            return false;
        }
    }
    return true;
}

bool LogManager::startNewSegment() {
    currentSegmentSeq++;
    currentSegmentBlockStart += blocksPerSegment;
    delete currentSegment;
    currentSegment = new Segment(currentSegmentSeq, segmentSize);
    std::vector<uint8_t> headerData = currentSegment->serializeHeader();
    if (!blockManager->writeBlock(currentSegmentBlockStart, headerData)) {
        std::cerr << "Failed to write new segment header at block " << currentSegmentBlockStart << "\n";
        return false;
    }
    currentOffset = 0;
    return true;
}

bool LogManager::recover() {
    std::vector<uint8_t> sbData;
    if (!blockManager->readBlock(0, sbData)) {
        std::cerr << "Failed to read superblock during recovery.\n";
        return false;
    }
    Superblock sb = Superblock::deserialize(sbData);
    uint32_t checkpointSegment = sb.latestCheckpointSegment;
    uint32_t checkpointOffset = sb.latestCheckpointOffset;
    uint64_t checkpointSeq = sb.latestCheckpointSeq;
    std::cout << "Recovery: Latest checkpoint at segment " << checkpointSegment
              << ", offset " << checkpointOffset
              << ", sequence " << checkpointSeq << "\n";

    // TODO: Scan segments following the checkpoint and replay log records.
    return true;
}
