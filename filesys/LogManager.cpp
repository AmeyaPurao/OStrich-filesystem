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

    cout << "Initialize logmanager: latest system log sequence is: " << latestSystemSeq << endl;
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
//        cout << "latest system log sequence is: " << globalSequence << endl;
        std::cout << "System is caught up to the latest log record" << std::endl;
        currentLogEntry = tempBlock;
        if (globalSequence == 0) {
            // New filesytem, need to create first checkpoint
            if (!createCheckpoint()) {
                std::cout << "Failed to create initial checkpoint" << std::endl;
            }
        }
    } else {
        std::cerr << "Superblock latest system state not consistent with log state: " << tempBlock.records[latestLogOffset].sequenceNumber << std::endl;
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

    //cout<<"Logging operation with sequence number: "<<record.sequenceNumber<<endl;


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

    //update global sequence in superblock
    block_t temp;
    if (!blockManager->readBlock(0, (uint8_t *) &temp)) {
        std::cout << "Failed to read superblock" << std::endl;
        // logLock.unlock();
        return false;
    }
    // Subtract 1 since global sequence is post incremented
    temp.superBlock.systemStateSeqNum = globalSequence - 1;
    if (!blockManager->writeBlock(0, (uint8_t *) &temp)) {
        std::cerr << "Could not write superblock" << std::endl;
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

    // Calculate the total number of blocks covering the inode table.
    inode_index_t totalInodes = temp.superBlock.inodeCount;
    inode_index_t entriesPerBlock = TABLE_ENTRIES_PER_BLOCK;
    inode_index_t totalBlocks = (totalInodes + entriesPerBlock - 1) / entriesPerBlock;

    // Buffer to hold one block's worth of inode numbers.
    inode_index_t inodeBuffer[TABLE_ENTRIES_PER_BLOCK];

    for (inode_index_t blockIdx = 0; blockIdx < totalBlocks; blockIdx++) {
        if (!inodeTable->readInodeBlock(blockIdx, inodeBuffer)) {
            std::cerr << "Failed to read inode table block " << blockIdx << std::endl;
            return false;
        }
        for (inode_index_t j = 0; j < entriesPerBlock; j++) {
            inode_index_t globalInodeIndex = blockIdx * entriesPerBlock + j;
            if (globalInodeIndex >= totalInodes)
                break;
            // Only add if the inode entry is valid (not NULL_INDEX).
            if (inodeBuffer[j] != INODE_NULL_VALUE) {
                checkpoint_entry_t entry;
                entry.inodeIndex = globalInodeIndex;
                entry.inodeLocation = inodeBuffer[j];
                if (currentCheckpoint->numEntries < NUM_CHECKPOINTENTRIES_PER_CHECKPOINT) {
                    currentCheckpoint->entries[currentCheckpoint->numEntries++] = entry;
                } else {
                    // Current checkpoint block is full, allocate a new one.
                    newCheckpointIndex = blockBitmap->findNextFree();
                    if (!blockBitmap->setAllocated(newCheckpointIndex)) {
                        std::cerr << "Could not set block bitmap for new checkpoint block" << std::endl;
                        return false;
                    }
                    currentCheckpoint->nextCheckpointBlock = newCheckpointIndex;
                    if (!blockManager->writeBlock(thisCheckpointIndex, reinterpret_cast<uint8_t *>(currentCheckpoint))) {
                        std::cerr << "Could not write checkpoint block to disk" << std::endl;
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
    }
    // Write the last checkpoint block.
    if (!blockManager->writeBlock(thisCheckpointIndex, reinterpret_cast<uint8_t *>(currentCheckpoint))) {
        std::cerr << "Could not write final checkpoint block to disk" << std::endl;
        return false;
    }
    delete (currentCheckpoint);

    // Create a checkpoint log record.
    logRecord_t checkpointRecord;
    checkpointRecord.payload.checkpoint.checkpointLocation = firstCheckpointIndex;
    logOperation(LogOpType::LOG_UPDATE_CHECKPOINT, &checkpointRecord.payload);

    // Update superblock to show new checkpoint.
    temp.superBlock.latestCheckpointIndex++;
    cout << "checkpointed at latest checkpoint index: " << temp.superBlock.latestCheckpointIndex << endl;
    temp.superBlock.checkpointArr[temp.superBlock.latestCheckpointIndex] = firstCheckpointIndex;
    if (!blockManager->writeBlock(0, (uint8_t *)&temp)) {
        std::cerr << "Could not write superblock" << std::endl;
        return false;
    }
    std::cout << "Checkpoint created at block " << firstCheckpointIndex << std::endl;
    return true;
}


bool LogManager::recover() {
    std::cout << "Recovery: Reapplying log entries from the last checkpoint...\n";
    // Read the superblock to get the checkpoint information.
    block_t superblockBlock;
    if (!blockManager->readBlock(0, reinterpret_cast<uint8_t *>(&superblockBlock))) {
        std::cout << "Failed to read superblock" << std::endl;
        return false;
    }

    //set readonly to false
    superblockBlock.superBlock.readOnly = false;
    if (!blockManager->writeBlock(0, reinterpret_cast<uint8_t *>(&superblockBlock))) {
        std::cout << "Failed to write superblock" << std::endl;
        return false;
    }

    // Get the latest checkpoint block index from the superblock.
    block_index_t latestCheckpointIndex = superblockBlock.superBlock.checkpointArr[superblockBlock.superBlock.latestCheckpointIndex];
    checkpointBlock_t checkpoint;
    int64_t checkpointLogRecordIndex = -1; // initialize to an invalid value
    std::cout << "Latest global sequence number: " << globalSequence << std::endl;
    std::cout << "Latest checkpoint block index: " << latestCheckpointIndex << std::endl;

    // Traverse the checkpoint chain.
    while (true) {
        if (!blockManager->readBlock(latestCheckpointIndex, reinterpret_cast<uint8_t *>(&checkpoint))) {
            std::cerr << "Failed to read checkpoint block at index " << latestCheckpointIndex << std::endl;
            return false;
        }
        if (checkpoint.magic != CHECKPOINT_MAGIC) {
            std::cerr << "Invalid checkpoint block at index " << latestCheckpointIndex << std::endl;
            return false;
        }
        // If this block is the header, capture its sequence number.
        if (checkpoint.isHeader) {
            checkpointLogRecordIndex = checkpoint.sequenceNumber;
        }
        std::cout << "Processing checkpoint block " << latestCheckpointIndex << " with "
                  << checkpoint.numEntries << " entries." << std::endl;

        // For each entry in the checkpoint block, update the inode table.
        for (int i = 0; i < checkpoint.numEntries; i++) {
            if (!inodeTable->setInodeLocation(checkpoint.entries[i].inodeIndex, checkpoint.entries[i].inodeLocation)) {
                std::cerr << "Failed to set inode location for inode index "
                          << checkpoint.entries[i].inodeIndex << std::endl;
                return false;
            }
        }
        // If there is no next checkpoint block, break out of the loop.
        if (checkpoint.nextCheckpointBlock == NULL_INDEX) {
            break;
        }
        latestCheckpointIndex = checkpoint.nextCheckpointBlock;
    }

    if (checkpointLogRecordIndex < 0) {
        std::cerr << "No header found in checkpoint chain. Recovery cannot proceed." << std::endl;
        return false;
    }

    // Replay log records from the checkpoint's sequence number to the current global sequence.
    for (int64_t i = checkpointLogRecordIndex; i < globalSequence; i++) {
        block_index_t logBlockIndex = logStartBlock + (i / NUM_LOGRECORDS_PER_LOGENTRY);
        if (!blockManager->readBlock(logBlockIndex, reinterpret_cast<uint8_t *>(&currentLogEntry))) {
            std::cerr << "Could not read log block at index " << logBlockIndex << std::endl;
            return false;
        }
        logRecord_t logRecord = currentLogEntry.records[i % NUM_LOGRECORDS_PER_LOGENTRY];
        std::cout << "Reapplying log record: sequence " << logRecord.sequenceNumber
                  << ", type " << static_cast<uint16_t>(logRecord.opType) << std::endl;
        switch (logRecord.opType) {
            case LogOpType::LOG_OP_INODE_ADD:
                if (!inodeTable->setInodeLocation(logRecord.payload.inodeAdd.inodeIndex,
                                                  logRecord.payload.inodeAdd.inodeLocation)) {
                    std::cerr << "Failed to apply LOG_OP_INODE_ADD for inode index "
                              << logRecord.payload.inodeAdd.inodeIndex << std::endl;
                    return false;
                }
                break;
            case LogOpType::LOG_OP_INODE_UPDATE:
                if (!inodeTable->setInodeLocation(logRecord.payload.inodeUpdate.inodeIndex,
                                                  logRecord.payload.inodeUpdate.inodeLocation)) {
                    std::cerr << "Failed to apply LOG_OP_INODE_UPDATE for inode index "
                              << logRecord.payload.inodeUpdate.inodeIndex << std::endl;
                    return false;
                }
                break;
            case LogOpType::LOG_OP_INODE_DELETE:
                if (!inodeTable->setInodeLocation(logRecord.payload.inodeDelete.inodeIndex, NULL_INDEX)) {
                    std::cerr << "Failed to apply LOG_OP_INODE_DELETE for inode index "
                              << logRecord.payload.inodeDelete.inodeIndex << std::endl;
                    return false;
                }
                break;
            case LogOpType::LOG_UPDATE_CHECKPOINT:
                // No action required.
                break;
            default:
                std::cerr << "Invalid log operation type encountered during recovery." << std::endl;
                return false;
        }
    }
    std::cout << "Recovery complete." << std::endl;
    return true;
}


bool LogManager::mountReadOnlySnapshot(uint32_t checkpointID) {
    // read superblock to get checkpoint information and set readonly to true
    block_t superblockBlock;
    if (!blockManager->readBlock(0, reinterpret_cast<uint8_t *>(&superblockBlock))) {
        std::cout << "Failed to read superblock" << std::endl;
        return false;
    }
    superblockBlock.superBlock.readOnly = true;
    if (!blockManager->writeBlock(0, reinterpret_cast<uint8_t *>(&superblockBlock))) {
        std::cout << "Failed to write superblock" << std::endl;
        return false;
    }

    //clear the inode table

    block_index_t checkpointBlockIndex = superblockBlock.superBlock.checkpointArr[checkpointID];
    return applyCheckpoint(checkpointBlockIndex);
}

bool LogManager::applyCheckpoint(block_index_t checkpointBlockIndex) {
    checkpointBlock_t checkpoint;
    int64_t checkpointLogRecordIndex = -1; // initialize to an invalid value
    std::cout << "Latest global sequence number: " << globalSequence << std::endl;
    std::cout << "Latest checkpoint block index: " << checkpointBlockIndex << std::endl;

    // Traverse the checkpoint chain.
    while (true) {
        if (!blockManager->readBlock(checkpointBlockIndex, reinterpret_cast<uint8_t *>(&checkpoint))) {
            std::cerr << "Failed to read checkpoint block at index " << checkpointBlockIndex << std::endl;
            return false;
        }
        if (checkpoint.magic != CHECKPOINT_MAGIC) {
            std::cerr << "Invalid checkpoint block at index " << checkpointBlockIndex << std::endl;
            return false;
        }
        // If this block is the header, capture its sequence number.
        if (checkpoint.isHeader) {
            checkpointLogRecordIndex = checkpoint.sequenceNumber;
        }
        std::cout << "Processing checkpoint block " << checkpointBlockIndex << " with "
                  << checkpoint.numEntries << " entries." << std::endl;

        // For each entry in the checkpoint block, update the inode table.
        for (int i = 0; i < checkpoint.numEntries; i++) {
            if (!inodeTable->setInodeLocation(checkpoint.entries[i].inodeIndex, checkpoint.entries[i].inodeLocation)) {
                std::cerr << "Failed to set inode location for inode index "
                          << checkpoint.entries[i].inodeIndex << std::endl;
                return false;
            }
        }
        // If there is no next checkpoint block, break out of the loop.
        if (checkpoint.nextCheckpointBlock == NULL_INDEX) {
            break;
        }
        checkpointBlockIndex = checkpoint.nextCheckpointBlock;
    }

    if (checkpointLogRecordIndex < 0) {
        std::cerr << "No header found in checkpoint chain. Recovery cannot proceed." << std::endl;
        return false;
    }

    std::cout << "Mount complete." << std::endl;
    return true;
}

uint64_t LogManager::get_timestamp() {
    // This function should return a current 64-bit timestamp. Will need some hardware support for this.
    static uint64_t dummyTime = 0;
    return ++dummyTime;
}

