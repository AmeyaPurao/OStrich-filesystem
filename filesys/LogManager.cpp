#include "LogManager.h"
// Assume that klog is a kernel logging function: void klog(const char *fmt, ...);

// extern "C" void klog(const char *fmt, ...); // Prototype for kernel logging.

LogManager::LogManager(BlockManager* blockManager, InodeTable* inode_table,  uint32_t startBlock, uint32_t numBlocks, uint64_t latestSystemSeq)
    : blockManager(blockManager),
      logStartBlock(startBlock),
      logNumBlocks(numBlocks),
      globalSequence(latestSystemSeq)
{
    // Find the latest logrecord to see if it matches the system state
    // If it doesn't, replay the log from the last checkpoint
    // If it does, continue logging operations
    block_index_t latestLogBlock = startBlock + latestSystemSeq/NUM_LOGRECORDS_PER_LOGENTRY;
    int32_t latestLogOffset = latestSystemSeq % NUM_LOGRECORDS_PER_LOGENTRY;
    logEntry_t tempBlock;
    if (!blockManager->readBlock(latestLogBlock, reinterpret_cast<uint8_t *>(&tempBlock)))
    {
        std::cerr << "Could not read latest log block" << std::endl;
        return;
    }
    if (tempBlock.records[latestLogOffset + 1].magic == RECORD_MAGIC) {
        std::cout << "System is not caught up to the latest log record" << std::endl;
        //TODO check to make sure system state is consistent, then replay future log entries
        recover();
    }
    else if (tempBlock.records[latestLogOffset].sequenceNumber == latestSystemSeq) {
        std::cout << "System is caught up to the latest log record" << std::endl;
        currentLogEntry = tempBlock;
    }
    else {
        std::cerr << "Superblock latest system state not consistent with log state" << std::endl;
    }


}

bool LogManager::logOperation(LogOpType opType, LogRecordPayload* payload)
{
    // logLock.lock();
    // Create a new log record
    logRecord_t record;
    record.sequenceNumber = globalSequence++;
    record.magic = RECORD_MAGIC;
    record.opType = opType;
    record.payload = *payload;



    if (currentLogEntry.numRecords < NUM_LOGRECORDS_PER_LOGENTRY) {
        // Add record to current log entry and update numRecords.
        currentLogEntry.records[currentLogEntry.numRecords++] = record;
    } else {
        // Start a new log entry and add the record.
        memset(&currentLogEntry, 0, sizeof(currentLogEntry));
        currentLogEntry.magic = ENTRY_MAGIC;
        currentLogEntry.numRecords = 0;
        currentLogEntry.records[currentLogEntry.numRecords++] = record;
    }

    // write back to disk
    block_index_t index = logStartBlock + globalSequence/NUM_LOGRECORDS_PER_LOGENTRY;
    if (!blockManager->writeBlock(index, reinterpret_cast<uint8_t *>(&currentLogEntry))) {
        std::cerr << "Could not write log entry to disk" << std::endl;
        // logLock.unlock();
        return false;
    }

    // logLock.unlock();
    return true;
}


logRecord_t LogManager::createCheckpoint()
{
    // logLock.lock();

    // Prepare a checkpoint log entry.
    logEntry_t checkpointEntry = {};
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


bool LogManager::recover()
{
    std::cout << "Recovery: Reapplying log entries from the last checkpoint... (this currently is a stub and does nothing)\n ";
    // A full recovery would scan the log area starting at the checkpoint pointer.
    return true;
}


bool LogManager::mountReadOnlySnapshot(uint32_t checkpointID) {
    //TODO: Implement this function
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
