#include "LogManager.h"
// Assume that klog is a kernel logging function: void klog(const char *fmt, ...);

// extern "C" void klog(const char *fmt, ...); // Prototype for kernel logging.

LogManager::LogManager(BlockManager* blockManager, uint32_t startBlock, uint32_t numBlocks)
    : blockManager(blockManager),
      logStartBlock(startBlock),
      logNumBlocks(numBlocks),
      currentRecordCount(0),
      checkpointCount(0),
      nextSequenceNumber(1),
      nextLogBlock(0)
{
    memset(&currentLogEntry, 0, sizeof(currentLogEntry));
    currentLogEntry.header.magic = 0x4C4F4745; // 'LOGE'
    currentLogEntry.header.sequenceNumber = nextSequenceNumber;
    currentLogEntry.header.numRecords = 0;
    currentLogEntry.header.timestamp = get_timestamp();
}

bool LogManager::logOperation(const logRecord_t *record, bool critical)
{
    // logLock.lock();

    // Add record to current log entry.
    if (currentRecordCount < 63) {
        currentLogEntry.records[currentRecordCount] = *record;
        currentRecordCount++;
        currentLogEntry.header.numRecords = currentRecordCount;
    } else {
        // Buffer full; commit the current log entry.
        if (!commitCurrentLogEntry()) {
            // logLock.unlock();
            return false;
        }
        // Start a new log entry.
        memset(&currentLogEntry, 0, sizeof(currentLogEntry));
        nextSequenceNumber++;
        currentLogEntry.header.magic = 0x4C4F4745;
        currentLogEntry.header.sequenceNumber = nextSequenceNumber;
        currentLogEntry.header.timestamp = get_timestamp();
        currentRecordCount = 0;
        currentLogEntry.header.numRecords = 0;
        // Add the record.
        currentLogEntry.records[currentRecordCount] = *record;
        currentRecordCount++;
        currentLogEntry.header.numRecords = currentRecordCount;
    }

    // If marked as critical, commit immediately.
    if (critical) {
        if (!commitCurrentLogEntry()) {
            // logLock.unlock();
            return false;
        }
    }

    // logLock.unlock();
    return true;
}

bool LogManager::commitCurrentLogEntry()
{
    uint32_t blockIndex = logStartBlock + nextLogBlock;
    // Write the current log entry to disk.
    if (!blockManager->writeBlock(blockIndex, (uint8_t*)&currentLogEntry)) {
        std::cout << "Failed to write log entry to block " << blockIndex << std::endl;
        return false;
    }
    nextLogBlock = (nextLogBlock + 1) % logNumBlocks;

    // Reset the current log entry buffer.
    memset(&currentLogEntry, 0, sizeof(currentLogEntry));
    currentRecordCount = 0;
    nextSequenceNumber++;
    return true;
}

bool LogManager::createCheckpoint()
{
    // logLock.lock();
    // Flush any pending log entry.
    if (currentRecordCount > 0) {
        if (!commitCurrentLogEntry()) {
            // logLock.unlock();
            return false;
        }
    }

    // Prepare a checkpoint log entry.
    logEntry_t checkpointEntry;
    memset(&checkpointEntry, 0, sizeof(checkpointEntry));
    checkpointEntry.header.magic = 0x4C4F4745;
    checkpointEntry.header.sequenceNumber = nextSequenceNumber;
    checkpointEntry.header.timestamp = get_timestamp();

    // Create a log record with opType = 100 to denote a checkpoint.
    logRecord_t cpRecord;
    cpRecord.opType = 100; // LOG_OP_CHECKPOINT
    cpRecord.reserved = 0;
    memset(&cpRecord.payload, 0, sizeof(cpRecord.payload));
    // (A real diff would be computed and stored in cpRecord.payload here.)

    checkpointEntry.records[0] = cpRecord;
    checkpointEntry.header.numRecords = 1;

    uint32_t blockIndex = logStartBlock + nextLogBlock;
    if (!blockManager->writeBlock(blockIndex, (uint8_t*)&checkpointEntry)) {
        std::cout << "Failed to write checkpoint log entry to block " << blockIndex << std::endl;
        // logLock.unlock();
        return false;
    }
    nextLogBlock = (nextLogBlock + 1) % logNumBlocks;

    // Add checkpoint to the history table.
    if (checkpointCount < MAX_CHECKPOINTS) {
        checkpointHistory[checkpointCount].checkpointID = nextSequenceNumber;
        checkpointHistory[checkpointCount].blockIndex = blockIndex;
        checkpointHistory[checkpointCount].timestamp = get_timestamp();
        checkpointCount++;
    } else {
        std::cout << "Failed to write checkpoint log entry to block "<< std::endl;
    }

    // Update superblock pointers (using a double-buffering approach).
    updateSuperblockPointers(blockIndex);

    nextSequenceNumber++;
    // logLock.unlock();
    return true;
}

bool LogManager::updateSuperblockPointers(uint32_t checkpointBlock)
{
    // Create a temporary block to hold the superblock.
    block_t temp;

    // Read the superblock from disk (block 0).
    if (!blockManager->readBlock(0, temp.data)) {
        cout << "updateSuperblockPointers: Failed to read superblock from block 0" << endl;
        return false;
    }

    // Update the pointers:
    // Set both the latest log commit and the latest system state pointers to the checkpoint block.
    // Since we store entire log entries per block, the offset is set to 0. We will need to change this if we want to
    // commit individual log records separately to disk.
    temp.superBlock.latestLogCommitBlock = checkpointBlock;
    temp.superBlock.latestLogCommitOffset = 0;
    temp.superBlock.latestSystemStateBlock = checkpointBlock;
    temp.superBlock.latestSystemStateOffset = 0;

    // Write the updated superblock back to block 0.
    if (!blockManager->writeBlock(0, temp.data)) {
        cout << "updateSuperblockPointers: Failed to write updated superblock to block 0" << endl;
        return false;
    }

    cout << "Superblock pointers updated: checkpoint at block " << checkpointBlock << ", offset 0" << endl;
    return true;
}

bool LogManager::recover()
{
    std::cout << "Recovery: Reapplying log entries from the last checkpoint... (this currently is a stub and does nothing)\n ";
    // A full recovery would scan the log area starting at the checkpoint pointer.
    return true;
}

uint32_t LogManager::listCheckpoints(CheckpointEntry* outArray)
{
    // Copy checkpoint history into the caller's array.
    // Caller must provide an array of at least MAX_CHECKPOINTS elements.
    for (uint32_t i = 0; i < checkpointCount; i++) {
        outArray[i] = checkpointHistory[i];
    }
    return checkpointCount;
}

bool LogManager::mountReadOnlySnapshot(uint32_t checkpointID)
{
    for (uint32_t i = 0; i < checkpointCount; i++) {
        if (checkpointHistory[i].checkpointID == checkpointID) {
            std::cout << "Mounting read-only snapshot from checkpoint " << checkpointID << std::endl;
            // In a complete implementation, reconstruct the state from the checkpoint diff chain.
            return true;
        }
    }
    std::cout << "Checkpoint ID not found: " << checkpointID << std::endl;
    return false;
}

uint64_t LogManager::get_timestamp()
{
    // This function should return a current 64-bit timestamp.
    // For bare-metal systems, you might use a hardware timer or RTC.
    // Here we simply return a dummy value (e.g., a tick count).
    static uint64_t dummyTime = 0;
    return ++dummyTime;
}
