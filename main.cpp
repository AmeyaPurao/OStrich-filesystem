#include <iostream>
#include <string>
#include "FakeDiskDriver.h"
using namespace std;
int main() {
    cout << "Mock File System test - creating fake disk with 512 MB" << endl;
    FakeDiskDriver disk("OStrich_Hard_Drive", 512);
    cout << "Mock File System test - writing \"Hello, World!\" to block 100" << endl;
    FakeDiskDriver::Block block(512);
    string hello = "Hello, World!";
    copy(hello.begin(), hello.end(), block.begin());
    disk.writeBlock(100, block);
    cout << "Mock File System test - reading block 100" << endl;
    FakeDiskDriver::Block blockContents;
    disk.readBlock(100, blockContents);
    cout << "Mock File System test - block 100 contains: ";
    for (auto c : blockContents) {
        cout << c;
    }



    return 0;
}
