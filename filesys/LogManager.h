#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include "Block.h"
#include "LogRecord.h"
#include "../interface/BlockManager.h"
// #include "SpinLock.h"     // Will need to use spinlock from kernel team
#include <stdint.h>
#include <string.h>

// Maximum number of checkpoints we will store.
#define MAX_CHECKPOINTS 64

// Structure to store checkpoint history.
typedef struct {
    uint32_t checkpointID;
    uint32_t blockIndex; // Block in the log area where the checkpoint is stored.
    uint64_t timestamp;  // A 64-bit timestamp.
} CheckpointEntry;

class LogManager {
public:
    // Constructor: pass a pointer to BlockManager and specify the log area (starting block and number of blocks).
    LogManager(BlockManager* blockManager, uint32_t startBlock, uint32_t numBlocks);

    // Log an operation; if 'critical' is true, flush immediately.
    bool logOperation(const logRecord_t *record, bool critical = false);

    // Create a checkpoint (flush pending logs, compute diff, write checkpoint record).
    bool createCheckpoint();

    // Recovery: replay log entries from the last checkpoint (simplified).
    bool recover();

    // List available checkpoints. Fills the provided array (of size MAX_CHECKPOINTS) and returns the count.
    uint32_t listCheckpoints(CheckpointEntry* outArray);

    // Mount as a read-only snapshot based on a checkpoint ID.
    bool mountReadOnlySnapshot(uint32_t checkpointID);

private:
    BlockManager* blockManager;
    uint32_t logStartBlock; // starting block of the dedicated log area
    uint32_t logNumBlocks;  // number of blocks allocated for the log area

    // In-memory buffer for the current log entry being filled.
    logEntry_t currentLogEntry;
    uint16_t currentRecordCount;

    // // Spinlock to protect log operations.
    // SpinLock logLock;

    // In-memory checkpoint history table.
    CheckpointEntry checkpointHistory[MAX_CHECKPOINTS];
    uint32_t checkpointCount;

    // Helper: Commit the current log entry to disk.
    bool commitCurrentLogEntry();

    // Next log sequence number.
    uint32_t nextSequenceNumber;

    // Helper: Update the superblock pointers (double-buffered update).
    bool updateSuperblockPointers(uint32_t checkpointBlock);

    // Next available log block (relative to logStartBlock).
    uint32_t nextLogBlock;

    // Helper: Get a timestamp (assumed to be provided by the kernel).
    uint64_t get_timestamp();
};

#endif // LOG_MANAGER_H
