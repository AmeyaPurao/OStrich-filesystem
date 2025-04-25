//
// Created by caleb on 4/20/2025.
//


// test_fs.cpp
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>

#include "../interface/FakeDiskDriver.h"
#include "../interface/BlockManager.h"
#include "../filesys/FileSystem.h"
#include "../filesys/fs_requests.h"

using namespace fs;

static bool dirContains(inode_index_t dirInode, const char *name) {
    auto resp = fs::fs_req_read_dir(dirInode);
    assert(resp.status == fs::FS_RESP_SUCCESS);
    for (int i = 0; i < resp.entry_count; i++) {
        if (std::strcmp(resp.entries[i].name, name) == 0)
            return true;
    }
    return false;
}

// static bool dirContains(inode_index_t dirInode, const char* want) {
//     fs::fs_req_t rd{};
//     rd.req_type                       = fs::FS_REQ_READ_DIR;
//     rd_dir.inode_index     = dirInode;
//     auto resp = fs::fs_req_read_dir(&rd);
//     assert(resp.status == fs::FS_RESP_SUCCESS);
//
//     printf("dirContains: %d entries in inode %u\n", resp.entry_count, dirInode);
//     for (int i = 0; i < resp.entry_count; i++) {
//         auto &e = resp.entries[i];
//         printf("  [%d] name=\"%s\" inode=%u\n", i, e.name, e.inodeNumber);
//         if (std::strcmp(e.name, want) == 0)
//             return true;
//     }
//     return false;
// }

static std::string readFile(inode_index_t inode, int nBytes) {
    // first open
    static char pathBuf[64];
    std::snprintf(pathBuf, sizeof(pathBuf), "/file%u", (unsigned) inode);
    auto opro = fs::fs_req_open(std::string(pathBuf));
    assert(opro.status == fs::FS_RESP_SUCCESS);
    inode_index_t ino = opro.inode_index;

    // then read
    char buffer[nBytes] = {};
    auto rdr = fs::fs_req_read(ino, buffer, 0, nBytes);
    assert(rdr.status == fs::FS_RESP_SUCCESS);
    return std::string(buffer, nBytes - 1);
}

int main() {
    using namespace fs;

    // Setup
    FakeDiskDriver disk("test_fs.img", 8192);
    assert(disk.createPartition(0, 8192, "ext4"));
    auto parts = disk.listPartitions();
    BlockManager bm(disk, parts[0], 1024);
    block_t emptyBlock{};
    bm.writeBlock(0, emptyBlock.data);
    init(&bm);
    FileSystem *liveFS = fileSystem;

    // 1) CREATE file1
    {
        auto r = fs_req_create_file(0, false, "file1", 0);
        assert(r.status == FS_RESP_SUCCESS);

        // assert it shows up in root
        assert(dirContains(0, "file1"));
    }

    // 2) WRITE "hello"
    inode_index_t file2inode; {
        // open to fetch inode number
        string p1 = "/file1";
        auto ro = fs_req_open(p1);
        assert(ro.status == FS_RESP_SUCCESS);
        file2inode = ro.inode_index;

        const char *msg = "hello";
        int len = std::strlen(msg) + 1;  // includes null terminator
        auto wr = fs_req_write(file2inode, msg, 0, len);
        assert(wr.status == FS_RESP_SUCCESS);

        // assert read returns the same data
        std::string got = readFile(file2inode, len);
        assert(got == "hello");
    }

    //2.5 WRITE large amounts of data (20 KB)
    {
        cout << "Writing large file "<< endl;
        const int bufferSize = 20 * 4096;
        char msg[bufferSize];
        std::memset(msg, 'a', bufferSize);  // populate with 'a's
        msg[bufferSize - 1] = '\0';  // null terminate

        auto wr = fs_req_write(file2inode, msg, 0, bufferSize);
        assert(wr.status == FS_RESP_SUCCESS);

        // assert read returns the same data
        char buffer[bufferSize] = {};
        auto rd = fs_req_read(file2inode, buffer, 0, bufferSize);
        assert(rd.status == FS_RESP_SUCCESS);
        assert(buffer[bufferSize - 2] == 'a');
    }

    // 3) DELETE file1
    {
        auto rr = fs_req_remove_file(0, "file1");
        assert(rr.status == FS_RESP_SUCCESS);

        // assert it's gone
        assert(!dirContains(0, "file1"));
    }

    cout << "completed 1-3" << endl;

     // 4) Snapshot consistency
     {
         // create
         auto r = fs_req_create_file(0, false, "file2", 0);
         assert(r.status == FS_RESP_SUCCESS);
         file2inode = r.inode_index;
         // check that file exists

        auto ro = fs_req_open("/file2");
        assert(ro.status == FS_RESP_SUCCESS);
        inode_index_t checkInode = ro.inode_index;
        assert(checkInode == file2inode);



         // write first
        const int bufferSize = 20 * 4096;
        char msg[bufferSize];
        {
             std::memset(msg, 'e', bufferSize);  // populate with 'e's
             msg[bufferSize - 1] = '\0';  // null terminate

             // const char *m2 = "second";
             // int len = std::strlen(m2) + 1;
             auto wr = fs_req_write(file2inode, msg, 0, bufferSize);
             assert(wr.status == FS_RESP_SUCCESS);

             char buffer[bufferSize] = {};
             auto rd = fs_req_read(file2inode, buffer, 0, bufferSize);
             assert(rd.status == FS_RESP_SUCCESS);
             assert(buffer[bufferSize - 2] == 'e');
             assert(buffer[0] == 'e');
             assert(buffer[bufferSize/2] == 'e');

        }

         // checkpoint (2)
         assert(liveFS->createCheckpoint());

         // overwrite
        {
             const char *m1 = "first";
             int len = std::strlen(m1) + 1;
             auto wr = fs_req_write(file2inode, m1, 0, len);
             assert(wr.status == FS_RESP_SUCCESS);
        }

         // checkpoint (3)
         assert(liveFS->createCheckpoint());

         // delete
         auto rr = fs_req_remove_file(0, "file2");
         assert(rr.status == FS_RESP_SUCCESS);

         // checkpoint (4)
         assert(liveFS->createCheckpoint());

         // helper to test
         auto validate = [&](int cp, bool exists, const char *expected) {
             auto resp = fs_req_mount_snapshot(cp);
             assert(resp.status == FS_RESP_SUCCESS);

             bool found = dirContains(0, "file2");
             assert(found == exists);
             if (exists) {
                 int expectedLen = std::strlen(expected) + 1;
                 std::string content = readFile(file2inode, expectedLen);
                 assert(content == expected);
             }
         };

         validate(3, true, "first");
         validate(4, false, nullptr);
         validate(0, false, nullptr);

        // manually validate checkpoint 2
         {
             cout << "validating large file checkpoint "<< endl;
             auto resp = fs_req_mount_snapshot(2);
             assert(resp.status == FS_RESP_SUCCESS);
             auto r = fs_req_open("/file2");
             assert(r.status == FS_RESP_SUCCESS);
             file2inode = r.inode_index;
             char buffer[bufferSize] = {};
             auto rd = fs_req_read(file2inode, buffer, 0, bufferSize);
             assert(rd.status == FS_RESP_SUCCESS);
             assert(buffer[bufferSize - 2] == 'e');
             assert(buffer[0] == 'e');
             assert(buffer[bufferSize/2] == 'e');
         }
     }



    printf("reset live fs test\n");
    auto resp = fs_req_mount_snapshot(0);
    assert(resp.status == FS_RESP_SUCCESS);

    // 5) Recreate file2
    {
        auto r = fs_req_create_file(0, false, "file2", 0);
        assert(r.status == FS_RESP_SUCCESS);
        file2inode = r.inode_index;
        // check that file exists
        assert(dirContains(0, "file2"));
    }

    // write hello world to file 2
    {
        const char *msg = "hello world";
        int len = std::strlen(msg) + 1;  // includes null terminator
        auto wr = fs_req_write(file2inode, msg, 0, len);
        assert(wr.status == FS_RESP_SUCCESS);

        file2inode = fs_req_open("/file2").inode_index;
        printf("file2 inode: %u\n", file2inode);

        // assert read returns the same data by making a fsrequest and a buffer
        // read from file2
        char buffer[256] = {};
        auto rd = fs_req_read(file2inode, buffer, 0, len);
        assert(rd.status == FS_RESP_SUCCESS);
        assert(std::strcmp(buffer, msg) == 0);
    }

    // now overwrite file2 and test again
    {
        const char *msg = "goodbye world";
        int len = std::strlen(msg) + 1;  // includes null terminator
        auto wr = fs_req_write(file2inode, msg, 0, len);
        assert(wr.status == FS_RESP_SUCCESS);

        file2inode = fs_req_open("/file2").inode_index;
        printf("file2 inode: %u\n", file2inode);

        // assert read returns the same data by making a fsrequest and a buffer
        // read from file2
        char buffer[256] = {};
        auto rd = fs_req_read(file2inode, buffer, 0, len);
        assert(rd.status == FS_RESP_SUCCESS);
        assert(std::strcmp(buffer, msg) == 0);
    }

    std::puts("All tests passed!");
    return 0;
}
