#include <iostream>
#include <string>
#include <thread>
#include "FakeDiskDriver.h"


using namespace std;
int main() {
    cout << "Mock File System test - creating fake disk with 512 MB" << endl;
    FakeDiskDriver disk("OStrich_Hard_Drive", 512);
    cout << "Mock File System test - writing \"Hello, World!\" to block 100" << endl;
    FakeDiskDriver::Block block(4096);
    string hello = "Hello, World!";
    copy(hello.begin(), hello.end(), block.begin());
    disk.writeBlock(100, block);
    cout << "Mock File System test - reading block 100" << endl;
    FakeDiskDriver::Block blockContents;
    disk.readBlock(100, blockContents);
    cout << "Mock File System test - block 100 contains: " << string(blockContents.begin(), blockContents.end()) << endl;

    cout << "Mock File System test - creating partition from block 200 to 300" << endl;
    disk.createPartition(200, 100, "ext4");

    cout << "Mock File System test - testing concurrent access" << endl;

    thread writer1([&disk]() {
        FakeDiskDriver::Block block(4096, 'A');
        disk.writeBlock(100, block);
    });

    thread writer2([&disk]() {
        FakeDiskDriver::Block block(4096, 'B');
        disk.writeBlock(100, block);
    });

    thread reader1([&disk]() {
        FakeDiskDriver::Block block;
        disk.readBlock(100, block);
        cout << "Reader 1 read: " << string(block.begin(), block.end()) << endl;
    });

    thread reader2([&disk]() {
        FakeDiskDriver::Block block;
        disk.readBlock(100, block);
        cout << "Reader 2 read: " << string(block.begin(), block.end()) << endl;
    });

    writer1.join();
    writer2.join();
    reader1.join();
    reader2.join();


    return 0;
}
