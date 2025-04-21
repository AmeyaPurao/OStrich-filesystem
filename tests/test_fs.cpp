//
// Created by caleb on 4/20/2025.
//


// test_fs.cpp
#include <cassert>
#include <cstring>
#include <cstdio>

#include "../interface/FakeDiskDriver.h"
#include "../interface/BlockManager.h"
#include "../filesys/FileSystem.h"
#include "../filesys/fs_requests.h"

using namespace fs;

static bool dirContains(inode_index_t dirInode, const char *name) {
    fs::fs_req_t rd{};
    rd.req_type = fs::FS_REQ_READ_DIR;
    rd.data.read_dir.inode_index = dirInode;
    auto resp = fs::fs_req_read_dir(&rd);
    assert(resp.data.read_dir.status == fs::FS_RESP_SUCCESS);
    for (int i = 0; i < resp.data.read_dir.entry_count; i++) {
        if (std::strcmp(resp.data.read_dir.entries[i].name, name) == 0)
            return true;
    }
    return false;
}

// static bool dirContains(inode_index_t dirInode, const char* want) {
//     fs::fs_req_t rd{};
//     rd.req_type                       = fs::FS_REQ_READ_DIR;
//     rd.data.read_dir.inode_index     = dirInode;
//     auto resp = fs::fs_req_read_dir(&rd);
//     assert(resp.data.read_dir.status == fs::FS_RESP_SUCCESS);
//
//     printf("dirContains: %d entries in inode %u\n", resp.data.read_dir.entry_count, dirInode);
//     for (int i = 0; i < resp.data.read_dir.entry_count; i++) {
//         auto &e = resp.data.read_dir.entries[i];
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
    fs::fs_req_t op{};
    op.req_type = fs::FS_REQ_OPEN;
    op.data.open.path = pathBuf;
    auto opro = fs::fs_req_open(&op);
    assert(opro.data.open.status == fs::FS_RESP_SUCCESS);
    inode_index_t ino = opro.data.open.inode_index;

    // then read
    char buffer[256] = {};
    fs::fs_req_t rd{};
    rd.req_type              = fs::FS_REQ_READ;
    rd.data.read.inode_index = inode;
    rd.data.read.offset      = 0;
    rd.data.read.n_bytes     = nBytes;
    rd.data.read.buf         = buffer;
    auto rdr = fs::fs_req_read(&rd);
    assert(rdr.data.read.status == fs::FS_RESP_SUCCESS);
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
        fs_req_t cr{};
        cr.req_type = FS_REQ_CREATE_FILE;
        cr.data.create.cwd = 0;
        cr.data.create.is_dir = false;
        cr.data.create.name = const_cast<char *>("file1");
        auto r = fs_req_create_file(&cr);
        assert(r.data.create_file.status == FS_RESP_SUCCESS);

        // assert it shows up in root
        assert(dirContains(0, "file1"));
    }

    // 2) WRITE "hello"
    inode_index_t file1Inode; {
        // open to fetch inode number
        fs_req_t op{};
        op.req_type = FS_REQ_OPEN;
        static char p1[] = "/file1";
        op.data.open.path = p1;
        auto ro = fs_req_open(&op);
        assert(ro.data.open.status == FS_RESP_SUCCESS);
        file1Inode = ro.data.open.inode_index;

        const char *msg = "hello";
        int len = std::strlen(msg) + 1;  // includes null terminator
        fs_req_t w{};
        w.req_type = FS_REQ_WRITE;
        w.data.write.inode_index = file1Inode;
        w.data.write.offset = 0;
        w.data.write.n_bytes = std::strlen(msg) + 1;
        w.data.write.buf = const_cast<char *>(msg);
        auto wr = fs_req_write(&w);
        assert(wr.data.write.status == FS_RESP_SUCCESS);

        // assert read returns the same data
        std::string got = readFile(file1Inode, len);
        assert(got == "hello");
    }

    // 3) DELETE file1
    {
        fs_req_t rm{};
        rm.req_type = FS_REQ_REMOVE_FILE;
        rm.data.remove.inode_index = 0;
        rm.data.remove.name = const_cast<char *>("file1");
        auto rr = fs_req_remove_file(&rm);
        assert(rr.data.remove_file.status == FS_RESP_SUCCESS);

        // assert it's gone
        assert(!dirContains(0, "file1"));
    }

    cout << "completed 1-3" << endl;

    // // 4) Snapshot consistency
    // // recreate and overwrite twice, checkpoint at 0 and 1, then delete and cp 2
    // {
    //     // recreate
    //     fs_req_t cr{};
    //     cr.req_type = FS_REQ_CREATE_FILE;
    //     cr.data.create.cwd = 0;
    //     cr.data.create.is_dir = false;
    //     cr.data.create.name = const_cast<char *>("file1");
    //     auto r = fs_req_create_file(&cr);
    //     assert(r.data.create_file.status == FS_RESP_SUCCESS);
    //     //file1Inode = r.data.create_file.inode_index;
    //     //assert(dirContains(0, "file1"));
    //
    //     // write first
    //     {
    //         const char *m1 = "first";
    //         fs_req_t w1{};
    //         w1.req_type = FS_REQ_WRITE;
    //         w1.data.write.inode_index = file1Inode;
    //         w1.data.write.offset = 0;
    //         w1.data.write.n_bytes = std::strlen(m1) + 1;
    //         w1.data.write.buf = const_cast<char *>(m1);
    //         assert(fs_req_write(&w1).data.write.status == FS_RESP_SUCCESS);
    //     }
    //     // checkpoint 0
    //     assert(liveFS->createCheckpoint());
    //
    //     // overwrite
    //     {
    //         const char *m2 = "second";
    //         fs_req_t w2{};
    //         w2.req_type = FS_REQ_WRITE;
    //         w2.data.write.inode_index = file1Inode;
    //         w2.data.write.offset = 0;
    //         w2.data.write.n_bytes = std::strlen(m2) + 1;
    //         w2.data.write.buf = const_cast<char *>(m2);
    //         assert(fs_req_write(&w2).data.write.status == FS_RESP_SUCCESS);
    //     }
    //     // checkpoint 1
    //     assert(liveFS->createCheckpoint());
    //
    //     // delete
    //     fs_req_t rm{};
    //     rm.req_type = FS_REQ_REMOVE_FILE;
    //     rm.data.remove.inode_index = 0;
    //     rm.data.remove.name = const_cast<char *>("file1");
    //     assert(fs_req_remove_file(&rm).data.remove_file.status == FS_RESP_SUCCESS);
    //
    //     // checkpoint 2
    //     assert(liveFS->createCheckpoint());
    //
    //     // helper to test
    //     auto validate = [&](int cp, bool exists, const char *expected) {
    //         FileSystem *snap = liveFS->mountReadOnlySnapshot(cp);
    //         assert(snap);
    //         auto *old = fileSystem;
    //         fileSystem = snap;
    //
    //         bool found = dirContains(0, "file1");
    //         assert(found == exists);
    //         if (exists) {
    //             int expectedLen = std::strlen(expected) + 1;
    //             std::string content = readFile(file1Inode, expectedLen);
    //             assert(content == expected);
    //         }
    //
    //         fileSystem = old;
    //         delete snap;
    //     };
    //
    //     validate(2, true, "first");
    //     validate(3, true, "second");
    //     validate(4, false, nullptr);
    // }

    std::puts("All tests passed!");
    return 0;
}
