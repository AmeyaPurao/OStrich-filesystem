// TestLogBasedFS.cpp
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// Include our filesystem headers.
#include "BlockManager.h"
#include "Superblock.h"
#include "Segment.h"
#include "LogRecord.h"
#include "LogManager.h"

// Include FakeDiskDriver (assumed to be implemented elsewhere).
#include "FakeDiskDriver.h"

// --------------------
// Test Superblock
// --------------------
void testSuperblock() {
    std::cout << "Running Superblock test...\n";
    Superblock sb;
    sb.latestCheckpointSeq = 12345;
    sb.latestCheckpointSegment = 2;
    sb.latestCheckpointOffset = 512;

    std::vector<uint8_t> serialized = sb.serialize();
    Superblock sb2 = Superblock::deserialize(serialized);
    assert(sb2.latestCheckpointSeq == 12345);
    assert(sb2.latestCheckpointSegment == 2);
    assert(sb2.latestCheckpointOffset == 512);

    std::cout << "Superblock test passed.\n\n";
}

// --------------------
// Test LogRecord
// --------------------
void testLogRecord() {
    std::cout << "Running LogRecord test...\n";
    // Create a log record with a known payload.
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    // In the new design, the sequence number is assigned internally, so we create without it.
    LogRecord record(LogRecordType::INODE_UPDATE, payload);
    // Initially, the sequence should be 0.
    assert(record.getSequenceNumber() == 0);

    // Serialize and then deserialize.
    std::vector<uint8_t> serialized = record.serialize();
    size_t offset = 0;
    bool valid = false;
    LogRecord record2 = LogRecord::deserialize(serialized, offset, valid);
    assert(valid);
    assert(record2.getType() == LogRecordType::INODE_UPDATE);
    assert(record2.getPayload() == payload);

    //Create a second log record
    std::vector<uint8_t> payload2 = {6, 7, 8, 9, 69};
    LogRecord record3(LogRecordType::BLOCK_ALLOCATION, payload2);
    serialized = record3.serialize();
    offset = 0;
    valid = false;
    LogRecord record4 = LogRecord::deserialize(serialized, offset, valid);
    assert(valid);
    assert(record4.getType() == LogRecordType::BLOCK_ALLOCATION);
    assert(record4.getPayload() == payload2);

    std::cout << "LogRecord test passed.\n\n";
}

// --------------------
// Test Segment
// --------------------
void testSegment() {
    std::cout << "Running Segment test...\n";
    // Create a segment with segmentSize = 3 blocks (3 * 4096 bytes).
    size_t segmentSize = Superblock::BLOCK_SIZE * 3;
    Segment seg(1, segmentSize);
    std::vector<uint8_t> header = seg.serializeHeader();
    uint32_t segSeq = 0;
    bool valid = false;
    bool deserialized = Segment::deserializeHeader(header, segSeq, valid);
    assert(deserialized);
    assert(valid);
    assert(segSeq == 1);

    std::cout << "Segment test passed.\n\n";
}

// --------------------
// Test LogManager (Basic)
// --------------------
void testLogManagerBasic() {
    std::cout << "Running LogManager basic test...\n";
    // Create a FakeDiskDriver with enough sectors.
    // (For testing, we simulate a disk file "test_disk.img" with, say, 10000 sectors.)
    FakeDiskDriver disk("test_disk.img", 10000, std::chrono::milliseconds(10));
    FakeDiskDriver::Partition partition{0, 10000};

    // Instantiate a BlockManager.
    BlockManager blockManager(disk, partition);

    // Create a LogManager with a segment size of 5 blocks.
    size_t segmentSize = Superblock::BLOCK_SIZE * 5;
    LogManager logManager(&blockManager, segmentSize);

    // Append several records.
    std::vector<uint8_t> payload1 = {10, 20, 30};
    LogRecord rec1(LogRecordType::INODE_UPDATE, payload1);
    assert(logManager.appendRecord(rec1));

    std::vector<uint8_t> payload2 = {40, 50};
    LogRecord rec2(LogRecordType::BLOCK_ALLOCATION, payload2);
    assert(logManager.appendRecord(rec2));

    // Append a checkpoint record.
    std::vector<uint8_t> payload3 = {60};
    LogRecord rec3(LogRecordType::CHECKPOINT, payload3);
    assert(logManager.appendCheckpoint(rec3));

    // Flush and then recover.
    assert(logManager.flush());
    assert(logManager.recover());

    // Write some large records to fill up the segment to test segment rollover.
    for (int i = 0; i < 30; i++) {
        std::vector<uint8_t> payload(1000, static_cast<uint8_t>(i));
        LogRecord rec(LogRecordType::BLOCK_ALLOCATION, payload);
        assert(logManager.appendRecord(rec));
    }

    // Write another checkpoint, flush, and recover.
    std::vector<uint8_t> payload4 = {70};
    LogRecord rec4(LogRecordType::CHECKPOINT, payload4);
    assert(logManager.appendCheckpoint(rec4));
    assert(logManager.flush());
    assert(logManager.recover());

    std::cout << "LogManager basic test passed.\n\n";
}

// --------------------
// Test LogManager Concurrency
// --------------------
void testLogManagerConcurrency() {
    std::cout << "Running LogManager concurrency test...\n";
    // Create a FakeDiskDriver with more sectors.
    FakeDiskDriver disk("test_disk_concurrent.img", 50000, std::chrono::milliseconds(10));
    FakeDiskDriver::Partition partition{0, 50000};
    BlockManager blockManager(disk, partition);
    size_t segmentSize = Superblock::BLOCK_SIZE * 5;
    LogManager logManager(&blockManager, segmentSize);

    const int threadCount = 8;
    const int recordsPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    auto worker = [&logManager, recordsPerThread, &successCount](int threadId) {
        for (int i = 0; i < recordsPerThread; i++) {
            std::vector<uint8_t> payload = {static_cast<uint8_t>(threadId), static_cast<uint8_t>(i)};
            LogRecord rec(LogRecordType::INODE_UPDATE, payload);
            if (logManager.appendRecord(rec)) {
                successCount++;
            }
        }
    };

    threads.reserve(threadCount);
    for (int i = 0; i < threadCount; i++) {
        threads.emplace_back(worker, i);
    }
    for (auto &t: threads) {
        t.join();
    }

    int totalRecords = threadCount * recordsPerThread;
    assert(successCount == totalRecords);

    // Flush and perform a recovery (which prints checkpoint info).
    assert(logManager.flush());
    assert(logManager.recover());

    std::cout << "LogManager concurrency test passed.\n\n";
}

void DebugPrintDisk(string disk_name, size_t numSectors, size_t segmentSize) {
    std::cout << "DebugPrintDisk: " << disk_name << "\n";
    //Open the disk, initiate a logmanager, and print each log record in the disk
    FakeDiskDriver disk(disk_name, numSectors, std::chrono::milliseconds(10));
    FakeDiskDriver::Partition partition{0, numSectors};
    BlockManager blockManager(disk, partition);
    LogManager logManager(&blockManager, segmentSize);
    logManager.recover();
    std::cout << "Finished recovering\n\n";

}

// --------------------
// Main
// --------------------
int main() {
    testSuperblock();
    testLogRecord();
    testSegment();
    testLogManagerBasic();
    DebugPrintDisk("test_disk.img", 10000, Superblock::BLOCK_SIZE * 5);
    testLogManagerConcurrency();
    std::cout << "All tests passed.\n";
    return 0;
}
