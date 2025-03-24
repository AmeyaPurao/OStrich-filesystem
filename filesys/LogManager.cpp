#include "LogManager.h"
// Assume that klog is a kernel logging function: void klog(const char *fmt, ...);

// extern "C" void klog(const char *fmt, ...); // Prototype for kernel logging.

LogManager::LogManager(BlockManager *blockManager, BitmapManager *blockBitmap, InodeTable *inode_table,
                       uint32_t startBlock, uint32_t numBlocks, uint64_t latestSystemSeq)
    : globalSequence(latestSystemSeq),
      blockManager(blockManager),
      inodeTable(inode_table),
      blockBitmap(blockBitmap),
      logStartBlock(startBlock),
      logNumBlocks(numBlocks) {
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
        cout << "latest system log sequence is: " << globalSequence << endl;
        std::cout << "System is caught up to the latest log record" << std::endl;
        currentLogEntry = tempBlock;
        if (globalSequence == 0) {
            // New filesytem, need to create first checkpoint
            if (!createCheckpoint()) {
                std::cout << "Failed to create initial checkpoint" << std::endl;
            }
        }
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


bool LogManager::createCheckpoint() {
    // logLock.lock();
    block_t temp;
    if (!blockManager->readBlock(0, (uint8_t *) &temp)) {
        std::cout << "Failed to read superblock" << std::endl;
        // logLock.unlock();
        return false;
    }

    auto *checkpoint = new checkpointBlock_t{};
    checkpoint->checkpointID = temp.superBlock.checkpointArr[temp.superBlock.latestCheckpointIndex] + 1;
    checkpoint->magic = CHECKPOINT_MAGIC;
    checkpoint->isHeader = true;
    checkpoint->sequenceNumber = globalSequence;
    checkpoint->timestamp = get_timestamp();
    checkpoint->numEntries = 0;
    checkpoint->nextCheckpointBlock = NULL_INDEX;
    block_index_t thisCheckpointIndex = blockBitmap->findNextFree();
    block_index_t firstCheckpointIndex = thisCheckpointIndex;
    if (!blockBitmap->setAllocated(thisCheckpointIndex)) {
        std::cerr << "Could not set block bitmap" << std::endl;
        // logLock.unlock();
        return false;
    }
    block_index_t newCheckpointIndex;

    checkpointBlock_t *currentCheckpoint = checkpoint;
    for (int i = 0; i < temp.superBlock.inodeCount; i++) {
        // TODO need to make a much more efficient implementation of getInodeLocation
        block_index_t inodeLocation = inodeTable->getInodeLocation(i);
        if (inodeLocation != NULL_INDEX) {
            checkpoint_entry_t entry;
            entry.inodeIndex = i;
            entry.inodeLocation = inodeLocation;
            if (currentCheckpoint->numEntries < NUM_CHECKPOINTENTRIES_PER_CHECKPOINT) {
                currentCheckpoint->entries[currentCheckpoint->numEntries++] = entry;
            } else {
                // current checkpoint block is full, start a new one
                newCheckpointIndex = blockBitmap->findNextFree();
                if (!blockBitmap->setAllocated(newCheckpointIndex)) {
                    std::cerr << "Could not set block bitmap" << std::endl;
                    // logLock.unlock();
                    return false;
                }
                currentCheckpoint->nextCheckpointBlock = newCheckpointIndex;
                //write to the disk at thisCheckpointIndex
                if (!blockManager->writeBlock(thisCheckpointIndex, reinterpret_cast<uint8_t *>(currentCheckpoint))) {
                    std::cerr << "Could not write checkpoint block to disk" << std::endl;
                    // logLock.unlock();
                    return false;
                }
                thisCheckpointIndex = newCheckpointIndex;
                delete (currentCheckpoint);
                currentCheckpoint = new checkpointBlock_t{};
                currentCheckpoint->checkpointID = checkpoint->checkpointID;
                currentCheckpoint->magic = CHECKPOINT_MAGIC;
                currentCheckpoint->isHeader = false;
                currentCheckpoint->numEntries = 0;
                currentCheckpoint->entries[currentCheckpoint->numEntries++] = entry;
                currentCheckpoint->nextCheckpointBlock = NULL_INDEX;
            }
        }
    }
    // write the last checkpoint block
    // if (currentCheckpoint->numEntries != 0) {
        if (!blockManager->writeBlock(thisCheckpointIndex, reinterpret_cast<uint8_t *>(currentCheckpoint))) {
            std::cerr << "Could not write checkpoint block to disk" << std::endl;
            // logLock.unlock();
            return false;
        }
    // }
    delete (currentCheckpoint);
    logRecord_t checkpointRecord;
    checkpointRecord.payload.checkpoint.checkpointLocation = firstCheckpointIndex;
    logOperation(LogOpType::LOG_UPDATE_CHECKPOINT, &checkpointRecord.payload);

    //Update superblock to show new checkpoint
    temp.superBlock.latestCheckpointIndex ++;
    temp.superBlock.checkpointArr[temp.superBlock.latestCheckpointIndex] = firstCheckpointIndex;
    if (!blockManager->writeBlock(0, (uint8_t *) &temp)) {
        std::cerr << "Could not write superblock" << std::endl;
        // logLock.unlock();
        return false;
    }
    cout << "Checkpoint created at block " << firstCheckpointIndex << endl;
    // logLock.unlock();
    return true;
}


bool LogManager::recover() {
    std::cout << "Recovery: Reapplying log entries from the last checkpoint...\n";
    // Get the latest checkpoint
    block_t temp;
    if (!blockManager->readBlock(0, (uint8_t *) &temp)) {
        std::cout << "Failed to read superblock" << std::endl;
        return false;
    }
    block_index_t latestCheckpointIndex = temp.superBlock.checkpointArr[temp.superBlock.latestCheckpointIndex];
    checkpointBlock_t checkpoint;
    int64_t checkpointLogRecordIndex;
    cout << "Latest checkpoint block index: " << latestCheckpointIndex << endl;
    while (blockManager->readBlock(latestCheckpointIndex, (uint8_t *) &checkpoint)) {
        if (checkpoint.magic != CHECKPOINT_MAGIC) {
            std::cerr << "Invalid checkpoint block" << std::endl;
            return false;
        }
        if (checkpoint.isHeader) {
            checkpointLogRecordIndex = checkpoint.sequenceNumber;
        }

        std::cout << "Recovering from Checkpoint Block " << latestCheckpointIndex << std::endl;
        for (int i = 0; i < checkpoint.numEntries; i++) {
            // TODO need to make a much more efficient implementation of setInodeLocation, and need to handle deletions
            inodeTable->setInodeLocation(checkpoint.entries[i].inodeIndex, checkpoint.entries[i].inodeLocation);
        }
        if (checkpoint.nextCheckpointBlock == NULL_INDEX) {
            break;
        }
        latestCheckpointIndex = checkpoint.nextCheckpointBlock;
    }

    // inode table recovered, now replay log entries from the checkpoint log record to the current global sequence number
    for (long long i = checkpointLogRecordIndex; i < globalSequence; i++) {
        block_index_t logBlockIndex = logStartBlock + i / NUM_LOGRECORDS_PER_LOGENTRY;
        if (!blockManager->readBlock(logBlockIndex, (uint8_t *) &currentLogEntry)) {
            std::cerr << "Could not read log block" << std::endl;
            return false;
        }
        logRecord_t logRecord = currentLogEntry.records[i % NUM_LOGRECORDS_PER_LOGENTRY];
        switch (logRecord.opType) {
            case LogOpType::LOG_OP_INODE_ADD: {
                inodeTable->setInodeLocation(logRecord.payload.inodeAdd.inodeIndex,
                                             logRecord.payload.inodeAdd.inodeLocation);
                break;
            }
            case LogOpType::LOG_OP_INODE_UPDATE: {
                inodeTable->setInodeLocation(logRecord.payload.inodeUpdate.inodeIndex,
                                             logRecord.payload.inodeUpdate.inodeLocation);
                break;
            }
            case LogOpType::LOG_OP_INODE_DELETE: {
                inodeTable->setInodeLocation(logRecord.payload.inodeDelete.inodeIndex, NULL_INDEX);
                break;
            }
            case LogOpType::LOG_UPDATE_CHECKPOINT: {
                // Do nothing
                break;
            }
            default:
                std::cerr << "Invalid log operation type" << std::endl;
                return false;
        }
    }
    cout << "Recovery complete" << endl;
    return true;
}


bool LogManager::mountReadOnlySnapshot(uint32_t checkpointID) {
    //TODO: Implement this function
    return false;
}

uint64_t LogManager::get_timestamp() {
    // This function should return a current 64-bit timestamp. Will need some hardware support for this.
    static uint64_t dummyTime = 0;
    return ++dummyTime;
}
