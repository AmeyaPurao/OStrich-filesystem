#include <iostream>
#include <string.h>
#include <string>
#include <thread>

#include "interface/BlockManager.h"
#include "interface/FakeDiskDriver.h"
#include "filesys/Block.h"
#include "filesys/FileSystem.h"

using namespace std;

int main() {

    FakeDiskDriver disk("OStrich_Hard_Drive", 8192*1024);
    if (!disk.createPartition(0, 8192*1024, "ext4")) {
        cerr << "Error: Failed to create partition\n";
        return 1;
    }
    BlockManager block_manager(disk, disk.listPartitions()[0]);
    FileSystem file_system(&block_manager);

    return 0;
}