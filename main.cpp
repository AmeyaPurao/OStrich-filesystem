#include <cstring>

#include "interface/BlockManager.h"
#include "interface/FakeDiskDriver.h"
#include "filesys/Block.h"
#include "filesys/FileSystem.h"

void runFilesystemSetupTest(BlockManager& blockManager)
{
    FileSystem fileSystem(&blockManager);
    std::cout << "Loading root directory" << std::endl;
    auto* rootDir = fileSystem.getRootDirectory();

    std::cout << "Creating new directory /dir1" << std::endl;
    Directory* dir1 = rootDir->createDirectory("dir1");

    std::cout << "Creating new file /dir1/file1" << std::endl;
    File* file1 = dir1->createFile("file1");

    std::cout << "Writing data to file1" << std::endl;
    const char* testData = "testing!";
    file1->write_at(0, (uint8_t*)testData, strlen(testData) + 1);

    std::cout << "Creating new directory /dir2" << std::endl;
    Directory* dir2 = rootDir->createDirectory("dir2");

    std::cout << "Creating new directory /dir2/dir3" << std::endl;
    Directory* dir3 = dir2->createDirectory("dir3");

    std::cout << "Creating new file /dir2/dir3/file2" << std::endl;
    File* file2 = dir3->createFile("file2");

    std::cout << "Creating new file /dir2/largefile" << std::endl;
    File* largeFile = dir2->createFile("largefile");

    std::cout << "Writing >BLOCK_SIZE bytes to largefile" << std::endl;
    constexpr uint64_t BUFFER_SIZE = BlockManager::BLOCK_SIZE + 100;
    char* buffer = new char[BUFFER_SIZE];
    for (uint64_t i = 0; i < BUFFER_SIZE - 1; i++)
    {
        buffer[i] = 'a';
    }
    buffer[BUFFER_SIZE - 1] = '\0';
    largeFile->write_at(0, (uint8_t*)buffer, BUFFER_SIZE);
    delete[] buffer;

    std::cout << "Changing data in largefile" << std::endl;
    auto newTestData = " - new data! - ";
    largeFile->write_at(10, (uint8_t*)newTestData, strlen(newTestData)); // don't copy null terminator

    delete file1;
    delete file2;
    delete largeFile;
    delete dir1;
    delete dir2;
    delete dir3;
    delete rootDir;
}

void displayTree(const Directory* dir, const string& curPath)
{
    // std::cout << "Listing entries" << std::endl;
    const std::vector<char*> entries = dir->listDirectoryEntries();
    // std::cout << "Num entries: " << entries.size() << std::endl;
    for (const auto entry : entries)
    {
        auto* file = dir->getFile(entry);
        if (file->isDirectory())
        {
            std::cout << curPath << entry << std::endl;
            displayTree(dynamic_cast<Directory*>(file), "    " + curPath + entry + "/");
        }
        else
        {
            if (file->getSize() > 0)
            {
                char* data = new char[file->getSize()];
                file->read_at(0, (uint8_t*)data, file->getSize());
                data[file->getSize() - 1] = '\0';
                std::cout << curPath << entry << " (" << data << ")" << std::endl;
            }
            else
            {
                std::cout << curPath << entry << " (empty file)" << std::endl;
            }
        }
        delete file;
    }
    // delete all entries
    for (auto entry : entries)
    {
        delete[] entry;
    }
}

void displayFilesystem(BlockManager& blockManager)
{
    FileSystem fileSystem(&blockManager);
    auto* rootDir = fileSystem.getRootDirectory();
    std::cout << "/" << std::endl;
    displayTree(rootDir, "    /");
    delete rootDir;
}

int main()
{
    FakeDiskDriver disk("OStrich_Hard_Drive", 8192);
    if (!disk.createPartition(0, 8192, "ext4"))
    {
        std::cerr << "Error: Failed to create partition" << std::endl;
        return 1;
    }
    BlockManager block_manager(disk, disk.listPartitions()[0]);
    block_t emptyBlock{};
    block_manager.writeBlock(0, emptyBlock.data); // write empty superblock to force creation of new fs

    std::cout << "Running filesystem setup test" << std::endl;
    runFilesystemSetupTest(block_manager);

    std::cout << std::endl << std::endl << "Displaying filesystem" << std::endl;
    displayFilesystem(block_manager);

    return 0;
}
