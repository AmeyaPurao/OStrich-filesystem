//
// Created by caleb on 2/26/2025.
//

#ifndef LOGRECORD_H
#define LOGRECORD_H

#include <cstdint>

// Enum for the different log operation types.
enum LogOpType {
    LOG_OP_NONE = 0,
    LOG_OP_INODE_CREATE,
    LOG_OP_INODE_UPDATE,
    LOG_OP_BITMAP_SET,
    LOG_OP_BITMAP_CLEAR,
    LOG_OP_DIR_ADD_ENTRY,
    // Add more operation types as needed.
};

// Payload for an inode creation operation.
struct InodeCreatePayload {
    uint32_t inodeIndex;       // Newly created inode index.
    uint16_t uid;              // Owner user ID.
    uint16_t gid;              // Owner group ID.
    uint16_t permissions;      // Permission bits.
    uint16_t reserved;         // Padding for alignment.
    uint64_t timestamp;        // Timestamp when the inode was created.
    uint8_t reserved2[36];     // Reserved space to fill payload to 56 bytes.
};

// Payload for an inode update operation.
struct InodeUpdatePayload {
    uint32_t inodeIndex;       // Inode to update.
    uint64_t newSize;          // Updated file size.
    uint16_t newPermissions;   // Updated permission bits.
    uint8_t reserved2[42];     // Padding to reach 56 bytes.
};

// Payload for a bitmap operation (set or clear).
struct BitmapOpPayload {
    uint32_t blockIndex;       // Block index being modified.
    uint32_t bitIndex;         // Specific bit within the bitmap.
    uint8_t reserved2[48];     // Padding to reach 56 bytes.
};

// Payload for a directory entry addition operation.
struct DirAddEntryPayload {
    uint32_t parentInode;      // Parent directory inode.
    uint32_t childInode;       // New entry’s inode.
    char name[32];             // File or directory name (truncated if necessary).
    uint8_t reserved2[16];     // Padding to reach 56 bytes.
};

// Union that combines all possible payload types.
union LogRecordPayload {
    InodeCreatePayload inodeCreate;
    InodeUpdatePayload inodeUpdate;
    BitmapOpPayload bitmapOp;
    DirAddEntryPayload dirAddEntry;
    // Add additional payload types as needed.
};

// Log record structure (exactly 64 bytes) for a single operation.
typedef struct logRecord {
    uint32_t opType;              // Operation type (from LogOpType).
    uint32_t reserved;            // Reserved for future use.
    LogRecordPayload payload;     // Operation-specific payload.
} logRecord_t;

// Header for a log entry (first 64 bytes of a 4KB block).
typedef struct logEntryHeader {
    uint32_t magic;             // Magic constant for log validation, e.g., 0x4C4F4745 ("LOGE").
    uint32_t sequenceNumber;    // Incremental sequence number.
    uint16_t numRecords;        // Number of valid log records in this entry (max 63).
    uint16_t reserved;          // Reserved for alignment or future use.
    uint64_t timestamp;         // Creation timestamp of this log entry.
    uint8_t reserved2[44];      // Reserved to pad the header to 64 bytes.
} logEntryHeader_t;

// Log entry structure that fits exactly one 4KB block.
typedef struct logEntry {
    logEntryHeader_t header;
    logRecord_t records[63];      // 63 log records of 64 bytes each (63 * 64 = 4032 bytes).
} logEntry_t;



#endif //LOGRECORD_H
