#include "LogManager.h"
// Assume that klog is a kernel logging function: void klog(const char *fmt, ...);

// extern "C" void klog(const char *fmt, ...); // Prototype for kernel logging.

LogManager::LogManager(BlockManager *blockManager, BitmapManager *blockBitmap, InodeTable *inode_table,
                       uint32_t startBlock, uint32_t numBlocks, uint64_t latestSystemSeq)
        : blockManager(blockManager),
          logStartBlock(startBlock),
          logNumBlocks(numBlocks),
          globalSequence(latestSystemSeq),
          inodeTable(inode_table),
          blockBitmap(blockBitmap) {
    // Find the latest logrecord to see if it matches the system state
    // If it doesn't, replay the log from the last checkpoint
    // If it does, continue logging operations
    block_index_t latestLogBlock = startBlock + latestSystemSeq / NUM_LOGRECORDS_PER_LOGENTRY;
    int32_t latestLogOffset = latestSystemSeq % NUM_LOGRECORDS_PER_LOGENTRY;
    logEntry_t tempBlock;
    if (!blockManager->readBlock(latestLogBlock, reinterpret_cast<uint8_t *>(&tempBlock))) {
        std::cerr << "Could not read latest log block" << std::endl;
        return;
    }
    if (tempBlock.records[latestLogOffset + 1].magic == RECORD_MAGIC) {
        std::cout << "System is not caught up to the latest log record" << std::endl;
        //TODO check to make sure system state is consistent, then replay future log entries
        recover();
    } else if (tempBlock.records[latestLogOffset].sequenceNumber == latestSystemSeq) {
        std::cout << "System is caught up to the latest log record" << std::endl;
        currentLogEntry = tempBlock;
    } else {
        std::cerr << "Superblock latest system state not consistent with log state" << std::endl;
    }


}

bool LogManager::logOperation(LogOpType opType, LogRecordPayload *payload) {
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
    block_index_t index = logStartBlock + globalSequence / NUM_LOGRECORDS_PER_LOGENTRY;
    if (!blockManager->writeBlock(index, reinterpret_cast<uint8_t *>(&currentLogEntry))) {
        std::cerr << "Could not write log entry to disk" << std::endl;
        // logLock.unlock();
        return false;
    }

    // logLock.unlock();
    return true;
}


logRecord_t LogManager::createCheckpoint() {
    // logLock.lock();

    superBlock_t superBlock;
    if (!blockManager->readBlock(0, (uint8_t *) &superBlock)) {
        std::cout << "Failed to read superblock" << std::endl;
        // logLock.unlock();
        return {};
    }

    auto *checkpoint = new checkpointBlock_t{};
    checkpoint->checkpointID = superBlock.checkpointArr[superBlock.latestCheckpointIndex] + 1;
    checkpoint->magic = CHECKPOINT_MAGIC;
    checkpoint->isHeader = true;
    checkpoint->numBlocks = 0;
    block_index_t thisCheckpointIndex = blockBitmap->findNextFree();
    block_index_t firstCheckpointIndex = thisCheckpointIndex;
    if (!blockBitmap->setAllocated(thisCheckpointIndex)) {
        std::cerr << "Could not set block bitmap" << std::endl;
        // logLock.unlock();
        return {};
    }
    block_index_t newCheckpointIndex;

    checkpointBlock_t *currentCheckpoint = checkpoint;
    for (int i = 0; i < superBlock.inodeCount; i++) {
        // TODO need to make a much more efficient implementation of getInodeLocation
        block_index_t inodeLocation = inodeTable->getInodeLocation(i);
        if (inodeLocation != InodeTable::NULL_VALUE) {
            checkpoint_entry_t entry;
            entry.inodeIndex = i;
            entry.inodeLocation = inodeLocation;
            if (currentCheckpoint->numBlocks < NUM_CHECKPOINTENTRIES_PER_CHECKPOINT) {
                currentCheckpoint->entries[currentCheckpoint->numBlocks++] = entry;
            } else {
                // current checkpoint block is full, start a new one
                newCheckpointIndex = blockBitmap->findNextFree();
                if (!blockBitmap->setAllocated(newCheckpointIndex)) {
                    std::cerr << "Could not set block bitmap" << std::endl;
                    // logLock.unlock();
                    return {};
                }
                currentCheckpoint->nextCheckpointBlock = newCheckpointIndex;
                //write to the disk at thisCheckpointIndex
                if (!blockManager->writeBlock(thisCheckpointIndex, reinterpret_cast<uint8_t *>(currentCheckpoint))) {
                    std::cerr << "Could not write checkpoint block to disk" << std::endl;
                    // logLock.unlock();
                    return {};
                }
                thisCheckpointIndex = newCheckpointIndex;
                delete (currentCheckpoint);
                currentCheckpoint = new checkpointBlock_t{};
                currentCheckpoint->checkpointID = checkpoint->checkpointID;
                currentCheckpoint->magic = CHECKPOINT_MAGIC;
                currentCheckpoint->isHeader = false;
                currentCheckpoint->numBlocks = 0;
                currentCheckpoint->entries[currentCheckpoint->numBlocks++] = entry;
            }
        }
    }
    logRecord_t checkpointRecord;
    checkpointRecord.sequenceNumber = globalSequence++;
    checkpointRecord.magic = RECORD_MAGIC;
    checkpointRecord.opType = LogOpType::LOG_UPDATE_CHECKPOINT;
    checkpointRecord.payload.checkpoint.checkpointLocation = firstCheckpointIndex;
    // logLock.unlock();
    return checkpointRecord;
}


bool LogManager::recover() {
    std::cout
            << "Recovery: Reapplying log entries from the last checkpoint... (this currently is a stub and does nothing)\n ";
    // A full recovery would scan the log area starting at the checkpoint pointer.
    return true;
}


bool LogManager::mountReadOnlySnapshot(uint32_t checkpointID) {
    //TODO: Implement this function
    return false;
}

uint64_t LogManager::get_timestamp() {
    // This function should return a current 64-bit timestamp.
    // For bare-metal systems, you might use a hardware timer or RTC.
    // Here we simply return a dummy value (e.g., a tick count).
    static uint64_t dummyTime = 0;
    return ++dummyTime;
}
