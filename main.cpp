#include <iostream>
#include <string.h>
#include <string>
#include <thread>

#include "interface/BlockManager.h"
#include "interface/FakeDiskDriver.h"
#include "filesys/Block.h"
#include "filesys/FileSystem.h"

using namespace std;

int main()
{
    FakeDiskDriver disk("OStrich_Hard_Drive", 8192);
    if (!disk.createPartition(0, 8192, "ext4"))
    {
        cerr << "Error: Failed to create partition\n";
        return 1;
    }
    BlockManager block_manager(disk, disk.listPartitions()[0]);
    FileSystem file_system(&block_manager);
    inode_index_t rootIndex = file_system.createDirectory(InodeTable::NULL_VALUE, "");
    if (rootIndex != 0)
    {
        std::cerr << "Root inode not created properly" << std::endl;
        return 1;
    }
    auto dir1Name = "dir1";
    inode_index_t dir1Index = file_system.createDirectory(rootIndex, dir1Name);
    if (dir1Index == InodeTable::NULL_VALUE)
    {
        std::cerr << "Directory inode not created properly" << std::endl;
        return 1;
    }
    std::cout << "Created directory " << dir1Name << " with inode number " << dir1Index << std::endl;
    auto dir2Name = "dir2";
    inode_index_t dir2Index = file_system.createDirectory(dir1Index, dir2Name);
    if (dir2Index == InodeTable::NULL_VALUE)
    {
        std::cerr << "Directory inode not created properly" << std::endl;
        return 1;
    }
    std::cout << "Created directory " << dir2Name << " with inode number " << dir2Index << std::endl;


    return 0;
}
