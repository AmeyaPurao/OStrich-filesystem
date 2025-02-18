#include "LogManager.h"
#include <iostream>
#include <cstring>

static const size_t BLOCK_SIZE = Superblock::BLOCK_SIZE;

LogManager::LogManager(BlockManager *blockManager, size_t segmentSize)
        : blockManager(blockManager), segmentSize(segmentSize),
          currentSegmentSeq(0), globalSequence(1) {
    // Ensure that segmentSize is a multiple of BLOCK_SIZE.
    if (segmentSize % BLOCK_SIZE != 0) {
        std::cerr << "Error: segmentSize must be a multiple of BLOCK_SIZE ("
                  << BLOCK_SIZE << ").\n";
        exit(1);
    }
    blocksPerSegment = segmentSize / BLOCK_SIZE;

    // Attempt to read the superblock from block 0.
    BlockManager::Block sbData;
    bool sbRead = blockManager->readBlock(0, sbData);
    Superblock sb = Superblock::deserialize(sbData);
    if (sbRead && sb.magic == Superblock::MAGIC) {
        std::cout << "Existing superblock found. Running recovery...\n";
        // Use checkpoint info to set initial state.
        currentSegmentSeq = sb.latestCheckpointSegment;
        // For now, we set currentSegmentBlockStart based on currentSegmentSeq:
        currentSegmentBlockStart = 1 + (currentSegmentSeq - 1) * blocksPerSegment;
        // Call recovery to scan the log and reinitialize internal state.
        recover();
    } else {
        std::cout << "No valid superblock found. Creating new filesystem...\n";
        // Create a new superblock and write it.
        Superblock newSb;
        // (At this point, newSb.latestCheckpointSegment and latestCheckpointOffset are 0.)
        sbData = newSb.serialize();
        if (!blockManager->writeBlock(0, sbData)) {
            std::cerr << "Failed to write new superblock to block 0.\n";
            exit(1);
        }
        // Initialize starting state for the new filesystem.
        currentSegmentSeq = 1;
        currentSegmentBlockStart = 1; // Log segments start at block 1.
        currentOffset = 0;
        // Create the current segment.
        currentSegment = new Segment(currentSegmentSeq, segmentSize);
        // Write the segment header to disk.
        std::vector<uint8_t> headerData = currentSegment->serializeHeader();
        if (!blockManager->writeBlock(currentSegmentBlockStart, headerData)) {
            std::cerr << "Failed to write segment header at block "
                      << currentSegmentBlockStart << "\n";
        }
        // Immediately append an initial checkpoint record
        std::vector<uint8_t> cpPayload; // Empty payload.
        LogRecord cpRecord(LogRecordType::CHECKPOINT, cpPayload);
        if (!appendCheckpoint(cpRecord)) {
            std::cerr << "Failed to append initial checkpoint in new filesystem.\n";
            exit(1);
        }
    }


}

LogManager::~LogManager() {
    flush();
    delete currentSegment;
}

std::vector<uint8_t> LogManager::padToBlockSize(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> padded = data;
    if (padded.size() < BLOCK_SIZE) {
        padded.resize(BLOCK_SIZE, 0);
    }
    return padded;
}

bool LogManager::appendRecord(const LogRecord &record) {
    std::lock_guard<std::mutex> lock(logMutex);
    // Make a copy and let LogManager assign the sequence number internally.
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
    globalSequence++; // Advance global sequence.
    return true;
}

bool LogManager::appendCheckpoint(const LogRecord &checkpointRecord) {
    std::lock_guard<std::mutex> lock(logMutex);
    return doAppendCheckpoint(checkpointRecord);
}

bool LogManager::doAppendCheckpoint(const LogRecord &checkpointRecord) {
    // This function assumes the lock is already held.
    LogRecord cpRecord = checkpointRecord;
    cpRecord.setSequenceNumber(globalSequence);
    std::vector<uint8_t> serialized = cpRecord.serialize();
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
    globalSequence++;

    // Immediately update the superblock with checkpoint info.
    Superblock sb;
    sb.latestCheckpointSeq = cpRecord.getSequenceNumber();
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
        size_t blockIndex = currentSegmentBlockStart + 1 + i; // +1 to skip header block.
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
    // Advance to the next segment.
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
    // Immediately write a checkpoint at the start of the new segment.
    // (Here, we create a checkpoint record with an empty payload.)
    std::vector<uint8_t> cpPayload; // Empty payload.
    LogRecord cpRecord(LogRecordType::CHECKPOINT, cpPayload);
    if (!doAppendCheckpoint(cpRecord)) {
        std::cerr << "Failed to append checkpoint in new segment.\n";
        return false;
    }
    return true;
}

bool LogManager::recover() {
    // Recovery: read the superblock (block 0) to determine the checkpoint.
    BlockManager::Block sbData;
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

    // Here we scan from checkpointSegment up to the last segment on disk.
    // For simplicity, we assume that segments are sequentially stored.
    // This loop will update currentSegmentSeq and currentOffset based solely on disk data.
    uint32_t segNum = checkpointSegment;
    while (true) {
        size_t segStartBlock = 1 + (segNum - 1) * blocksPerSegment;
        BlockManager::Block headerBlock;
        if (!blockManager->readBlock(segStartBlock, headerBlock)) {
            std::cout << "No segment header found for segment " << segNum << ".\n";
            break;
        }
        uint32_t segSeq = 0;
        bool headerValid = false;
        if (!Segment::deserializeHeader(headerBlock, segSeq, headerValid) || !headerValid) {
            std::cout << "Invalid segment header for segment " << segNum << ".\n";
            break;
        }
        // Read the data blocks for this segment.
        std::vector<uint8_t> segData;
        for (size_t b = segStartBlock + 1; b < segStartBlock + blocksPerSegment; ++b) {
            BlockManager::Block blockData;
            if (!blockManager->readBlock(b, blockData)) {
                break;
            }
            segData.insert(segData.end(), blockData.begin(), blockData.end());
        }
        // Scan segData to check if there are any valid log records.
        size_t offset = (segNum == checkpointSegment) ? checkpointOffset : 0;
        bool foundRecord = false;
        while (offset < segData.size()) {
            size_t recordStart = offset;
            bool validRecord = false;
            LogRecord record = LogRecord::deserialize(segData, offset, validRecord);
            if (!validRecord) {
                break;
            }
            foundRecord = true;
            std::cout << "Recovered record: segment " << segNum
                      << ", data offset " << recordStart
                      << ", global sequence " << record.getSequenceNumber() << "\n";
            globalSequence = record.getSequenceNumber() + 1;
        }
        if (!foundRecord) {
            // No valid records in this segment; assume we've reached the end.
            break;
        }
        cout << "Segment " << segNum << " recovered.\n";
        segNum++;
    }
    // Update currentSegmentSeq and currentSegmentBlockStart to the next segment.
    currentSegmentSeq = segNum;
    currentSegmentBlockStart = 1 + (currentSegmentSeq - 1) * blocksPerSegment;
    // Create a new segment for subsequent writes.
    //if (currentSegment) delete currentSegment;
    currentSegment = new Segment(currentSegmentSeq, segmentSize);
    std::vector<uint8_t> newHeader = currentSegment->serializeHeader();
    if (!blockManager->writeBlock(currentSegmentBlockStart, newHeader)) {
        std::cerr << "Failed to write header for new segment during recovery.\n";
        return false;
    }

    // Design choice here: If we always write a checkpoint after recovery, we risk wasting segment space
    // If we don't, repeated restarts between small writes result in longer and longer recovery times
//    std::vector<uint8_t> cpPayload; // Empty payload.
//    LogRecord cpRecord(LogRecordType::CHECKPOINT, cpPayload);
//    if (!appendCheckpoint(cpRecord)) {
//        std::cerr << "Failed to append initial checkpoint in new filesystem.\n";
//        exit(1);
//    }

    currentOffset = 0;
    std::cout << "Recovery complete. New segment " << currentSegmentSeq << " ready for writing.\n";
    return true;
}
