#include "cstring"
#include "cstdio"
#include "vector"

#include "interface/BlockManager.h"
#include "interface/FakeDiskDriver.h"
#include "filesys/Block.h"
#include "filesys/FileSystem.h"

void runFilesystemSetupTest(BlockManager& blockManager)
{
    FileSystem fileSystem(&blockManager);
    printf("Loading root directory\n");
    auto* rootDir = fileSystem.getRootDirectory();

    printf("Creating new directory /dir1\n");
    Directory* dir1 = rootDir->createDirectory("dir1");

    printf("Creating new file /dir1/file1\n");
    File* file1 = dir1->createFile("file1");

    printf("Writing data to file1\n");
    const char* testData = "testing!";
    file1->write_at(0, (uint8_t*)testData, strlen(testData) + 1);

    printf("Creating new directory /dir2\n");
    Directory* dir2 = rootDir->createDirectory("dir2");

    printf("Creating new directory /dir2/dir3\n");
    Directory* dir3 = dir2->createDirectory("dir3");

    printf("Creating new file /dir2/dir3/file2\n");
    File* file2 = dir3->createFile("file2");

    printf("Creating new file /dir2/tmpfile\n");
    File* tmpFile = dir2->createFile("tmpfile");

    printf("Creating new file /dir2/largefile\n");
    File* largeFile = dir2->createFile("largefile");

    printf("Writing >BLOCK_SIZE bytes to largefile\n");
    constexpr uint64_t BUFFER_SIZE = BlockManager::BLOCK_SIZE + 100;
    char* buffer = new char[BUFFER_SIZE];
    for (uint64_t i = 0; i < BUFFER_SIZE - 1; i++)
    {
        buffer[i] = 'a';
    }
    buffer[BUFFER_SIZE - 1] = '\0';
    largeFile->write_at(0, (uint8_t*)buffer, BUFFER_SIZE);
    delete[] buffer;

    printf("Changing data in largefile\n");
    auto newTestData = " - new data! - ";
    largeFile->write_at(10, (uint8_t*)newTestData, strlen(newTestData)); // don't copy null terminator

    printf("Deleting /dir2/tmpfile\n");
    dir2->removeDirectoryEntry("tmpfile");
    delete tmpFile;



    delete file1;
    delete file2;
    delete largeFile;
    delete dir1;
    delete dir2;
    delete dir3;
    delete rootDir;
}


std::vector<char*> pathStrings;

void displayTree(const Directory* dir, const char* curPath)
{
    // printf("Listing entries\n");
    std::vector<char*> entries = dir->listDirectoryEntries();
    // std::cout << "Num entries: " << entries.size() << std::endl;
    for (int i = 0; i < entries.size(); i++)
    {
        char* entry = entries[i];
        auto* file = dir->getFile(entry);
        if (file->isDirectory())
        {
            printf("%s%s\n", curPath, entry);
            // allocate c string on heap
            char* newPath = new char[strlen(curPath) + strlen(entry) + 6];
            pathStrings.push_back(newPath);
            strcpy(newPath, "    ");
            strcat(newPath, curPath);
            strcat(newPath, entry);
            strcat(newPath, "/");
            displayTree(dynamic_cast<Directory*>(file), newPath);
        }
        else
        {
            if (file->getSize() > 0)
            {
                char* data = new char[file->getSize()];
                file->read_at(0, (uint8_t*)data, file->getSize());
                data[file->getSize() - 1] = '\0';
                printf(")\n");
            }
            else
            {
                printf(" (empty file)\n");
            }
        }
        delete file;
    }
    // delete all entries
    for (int i = 0; i < entries.size(); i++)
    {
        delete[] entries[i];
    } 
}

void displayFilesystem(BlockManager& blockManager)
{
    FileSystem fileSystem(&blockManager);
    auto* rootDir = fileSystem.getRootDirectory();
    printf("/\n");
    displayTree(rootDir, "    /");
    for (int i = 0; i < pathStrings.size(); i++)
    {
        delete[] pathStrings[i];
    }
    delete rootDir;
}

int main()
{
    FakeDiskDriver disk("OStrich_Hard_Drive", 8192);
    if (!disk.createPartition(0, 8192, "ext4"))
    {
        printf("Error: Failed to create partition\n");
        return 1;
    }
    BlockManager block_manager(disk, disk.listPartitions()[0]);
    block_t emptyBlock{};
    block_manager.writeBlock(0, emptyBlock.data); // write empty superblock to force creation of new fs

    printf("Running filesystem setup test\n");
    runFilesystemSetupTest(block_manager);

    printf("Displaying filesystem\n");
    displayFilesystem(block_manager);

    return 0;
}
