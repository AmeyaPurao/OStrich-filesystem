#include <iostream>
#include <string>
#include <thread>
#include "FakeDiskDriver.h"
#include "BlockManager.h"

using namespace std;

int main() {
    cout << "Mock File System test - creating fake disk with 512 KB" << endl;
    FakeDiskDriver disk("OStrich_Hard_Drive", 1024);

    cout << "Mock File System test - creating partition from sector 200 to 1024" << endl;
    if (!disk.createPartition(200, 824, "ext4")) {
        cerr << "Error: Failed to create partition\n";
        return 1;
    }

    vector<FakeDiskDriver::Partition> partitions = disk.listPartitions();
    if (partitions.empty()) {
        cerr << "Error: No partitions found\n";
        return 1;
    }

    BlockManager blockManager(disk, partitions[0]);

    cout << "Mock File System test - writing \"Hello, World!\" to block 0" << endl;
    BlockManager::Block block(BlockManager::BLOCK_SIZE);
    string hello = "Hello, World!";
    copy(hello.begin(), hello.end(), block.begin());
    if (!blockManager.writeBlock(0, block)) {
        cerr << "Error: Failed to write block\n";
        return 1;
    }

    cout << "Mock File System test - reading block 0" << endl;
    BlockManager::Block blockContents;
    if (!blockManager.readBlock(0, blockContents)) {
        cerr << "Error: Failed to read block\n";
        return 1;
    }
    cout << "Mock File System test - block 0 contains: " << string(blockContents.begin(), blockContents.end()) << endl;

    cout << "Mock File System test - testing concurrent access" << endl;

    thread writer1([&blockManager]() {
        BlockManager::Block block(BlockManager::BLOCK_SIZE, 'A');
        blockManager.writeBlock(0, block);
    });

    thread writer2([&blockManager]() {
        BlockManager::Block block(BlockManager::BLOCK_SIZE, 'B');
        blockManager.writeBlock(0, block);
    });

    thread reader1([&blockManager]() {
        BlockManager::Block block;
        blockManager.readBlock(0, block);
        cout << "Reader 1 read: " << string(block.begin(), block.end()) << endl;
    });

    thread reader2([&blockManager]() {
        BlockManager::Block block;
        blockManager.readBlock(0, block);
        cout << "Reader 2 read: " << string(block.begin(), block.end()) << endl;
    });

    writer1.join();
    writer2.join();
    reader1.join();
    reader2.join();

    return 0;
}