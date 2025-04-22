//
// Created by caleb on 4/16/2025.
//

#include "fs_requests.h"
#include "cassert"
#include "cstring"
#include "cstdlib"
#include "string"
#include "vector"

#ifndef NOT_KERNEL
#include "atomic.h"
#include "event.h"
#endif

namespace fs {
    void init(BlockManager* block_manager) {
        fileSystem = FileSystem::getInstance(block_manager);
        blockManager = block_manager;
    }

    fs_response_t fs_req_add_dir(fs_req_t* req) {
        fs_response_t resp;

        if (fileSystem->isReadOnly()) {
            resp.req_type = FS_REQ_ADD_DIR;
            resp.data.add_dir.status = FS_RESP_ERROR_PERMISSION;
            return resp;
        }

        inode_index_t dir_inode_num =  req->data.add_dir.dir;
        Directory parent_dir = Directory(dir_inode_num, fileSystem->inodeTable, fileSystem->inodeBitmap,
                fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
        bool success = parent_dir.addDirectoryEntry(req->data.add_dir.name, req->data.add_dir.file_to_add);

        resp.req_type = FS_REQ_ADD_DIR;
        if(success) {
            resp.data.add_dir.status = FS_RESP_SUCCESS;
        } else {
            resp.data.add_dir.status = FS_RESP_ERROR_EXISTS;
        }
        return resp;
    }

    fs_response_t fs_req_create_file(fs_req_t *req) {
        fs_response_t resp;

        if (fileSystem->isReadOnly()) {
            resp.req_type = FS_REQ_CREATE_FILE;
            resp.data.create_file.status = FS_RESP_ERROR_PERMISSION;
            resp.data.create_file.inode_index = INODE_NULL_VALUE;
            return resp;
        }

        resp.data.create_file.inode_index = INODE_NULL_VALUE;
        inode_index_t dir_inode_num = req->data.create.cwd;
        Directory parent_dir = Directory(dir_inode_num, fileSystem->inodeTable, fileSystem->inodeBitmap,
                fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
        //TODO need to modify createFile()/createDirectory to take permissions and stuff

        void* newFile = nullptr;
        if (req->data.create.is_dir) {
            newFile = parent_dir.createDirectory(req->data.create.name);
        }
        else {
            newFile = parent_dir.createFile(req->data.create.name);
        }

        if (newFile == nullptr) {
            resp.data.create_file.status = FS_RESP_ERROR_EXISTS;
        } else {
            resp.data.create_file.status = FS_RESP_SUCCESS;
            if (req->data.create.is_dir) {
                resp.data.create_file.inode_index = (static_cast<Directory*>(newFile))->getInodeNumber();
            }
            else {
                resp.data.create_file.inode_index = (static_cast<File*>(newFile))->getInodeNumber();
            }
        }
        resp.req_type = FS_REQ_CREATE_FILE;
        return resp;
    }

    fs_response_t fs_req_remove_file(fs_req_t *req) {
        fs_response_t resp;

        if (fileSystem->isReadOnly()) {
            resp.req_type = FS_REQ_REMOVE_FILE;
            resp.data.remove_file.status = FS_RESP_ERROR_PERMISSION;
            return resp;
        }

        inode_index_t dir_inode_num = req->data.remove.inode_index;
        Directory parent_dir = Directory(dir_inode_num, fileSystem->inodeTable, fileSystem->inodeBitmap,
                fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
        bool success = parent_dir.removeDirectoryEntry(req->data.remove.name);
        resp.req_type = FS_REQ_REMOVE_FILE;
        if (success) {
            resp.data.remove_file.status = FS_RESP_SUCCESS;
        } else {
            resp.data.remove_file.status = FS_RESP_ERROR_NOT_FOUND;
        }
        return resp;
    }

    fs_response_t fs_req_read_dir(fs_req_t *req){
        fs_response_t resp{};
        resp.data.read_dir.entry_count = 0;
        inode_index_t dir_inode_num = req->data.read_dir.inode_index;
        Directory parent_dir = Directory(dir_inode_num, fileSystem->inodeTable, fileSystem->inodeBitmap,
                fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
        std::vector<char*> entries = parent_dir.listDirectoryEntries();
        resp.req_type = FS_REQ_READ_DIR;
        resp.data.read_dir.status = FS_RESP_SUCCESS;
        for(auto entry : entries) {
            strcpy(resp.data.read_dir.entries[resp.data.read_dir.entry_count].name, entry);
            resp.data.read_dir.entries[resp.data.read_dir.entry_count].name[MAX_FILE_NAME_LENGTH] = '\0';
            resp.data.read_dir.entries[resp.data.read_dir.entry_count].inodeNumber = parent_dir.getDirectoryEntry(entry);
            resp.data.read_dir.entry_count++;
        }
        return resp;
    }

    fs_response_t fs_req_open(fs_req_t *req){
        fs_response_t resp;
        resp.req_type = FS_REQ_OPEN;
        resp.data.open.status = FS_RESP_ERROR_NOT_FOUND;
        std::vector<string> path_parts;

        // Parse path (already minified)
        string path(req->data.open.path);
        string cur_part;
        for (int i = 1; i < path.length(); i++) {
            if (path[i] == '/') {
                path_parts.push_back(cur_part);
                cur_part = "";
            } else {
                cur_part += path[i];
            }
        }
        path_parts.push_back(cur_part);

        Directory* rootDir = fileSystem->getRootDirectory();
        Directory* curDir = rootDir;
        inode_index_t dir_inode_num = 0;
        for(int i = 0; i < path_parts.size() - 1; i++) {
            printf("nope\n");
            dir_inode_num = curDir->getDirectoryEntry(path_parts[i].c_str());
            if (dir_inode_num == INODE_NULL_VALUE) {
                return resp;
            }
            delete curDir;
            curDir = new Directory(dir_inode_num, fileSystem->inodeTable, fileSystem->inodeBitmap,
                    fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
        }
        //open the file in the last part of the path
        //TODO need to modify createFile()/createDirectory to take permissions and stuff
        void* newFile = nullptr;
        if (path_parts.back() == ".") {
            newFile = curDir;
        } else {
            newFile = curDir->getFile(path_parts.back().c_str());
        }
        if (newFile != nullptr) {
            resp.data.open.status = FS_RESP_SUCCESS;
            resp.data.open.inode_index = (static_cast<File*>(newFile))->getInodeNumber();
            // todo need to implement get permissions
            // resp.data.open.permissions = (static_cast<File*>(newFile))->getPermissions();
        }
        return resp;
    }

    fs_response_t fs_req_write(fs_req_t *req){
        fs_response_t resp;

        if (fileSystem->isReadOnly()) {
            resp.req_type = FS_REQ_WRITE;
            resp.data.write.status = FS_RESP_ERROR_PERMISSION;
            resp.data.write.bytes_written = 0;
            return resp;
        }

        inode_index_t inode_num = req->data.write.inode_index;
        File file = File(inode_num, fileSystem->inodeTable, fileSystem->inodeBitmap,
                fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
        bool success = file.write_at(req->data.write.offset, reinterpret_cast<uint8_t*>(req->data.write.buf), req->data.write.n_bytes);
        if (success) {
            resp.data.write.status = FS_RESP_SUCCESS;
            resp.data.write.bytes_written = req->data.write.n_bytes;
        } else {
            resp.data.write.status = FS_RESP_ERROR_INVALID;
            resp.data.write.bytes_written = 0;
        }
        resp.req_type = FS_REQ_WRITE;
        return resp;
    }

    fs_response_t fs_req_read(fs_req_t *req){
        fs_response_t resp;
        inode_index_t inode_num = req->data.read.inode_index;
        File file = File(inode_num, fileSystem->inodeTable, fileSystem->inodeBitmap,
                fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
        bool success = file.read_at(req->data.read.offset, reinterpret_cast<uint8_t*>(req->data.read.buf), req->data.read.n_bytes);
        if (success) {
            resp.data.read.status = FS_RESP_SUCCESS;
            resp.data.read.bytes_read = req->data.read.n_bytes;
        } else {
            resp.data.read.status = FS_RESP_ERROR_INVALID;
            resp.data.read.bytes_read = 0;
        }
        resp.req_type = FS_REQ_READ;
        return resp;
    }

    fs_response_t fs_req_mount_snapshot(fs_req_t *req) {
        fs_response_t resp;
        resp.req_type = FS_REQ_MOUNT_SNAPSHOT;
        fileSystem = FileSystem::getInstance(blockManager);
        if (req->data.mount_snapshot.checkpointID == 0) {
            resp.data.mount_snapshot.status = FS_RESP_SUCCESS;
        }
        else {
            FileSystem* snapshot = fileSystem->mountReadOnlySnapshot(req->data.mount_snapshot.checkpointID);
            if (snapshot) {
                fileSystem = snapshot;
                resp.data.mount_snapshot.status = FS_RESP_SUCCESS;
            } else {
                resp.data.mount_snapshot.status = FS_RESP_ERROR_NOT_FOUND;
            }
        }
        return resp;
    }

    fs_response_t fs_req_dispatch(fs_req_type_t req_type, fs_req_t req) {
        switch (req_type) {
            case FS_REQ_ADD_DIR:
                return fs_req_add_dir(&req);
            case FS_REQ_CREATE_FILE:
                return fs_req_create_file(&req);
            case FS_REQ_REMOVE_FILE:
                return fs_req_remove_file(&req);
            case FS_REQ_READ_DIR:
                return fs_req_read_dir(&req);
            case FS_REQ_OPEN:
                return fs_req_open(&req);
            case FS_REQ_WRITE:
                return fs_req_write(&req);
            case FS_REQ_READ:
                return fs_req_read(&req);
            case FS_REQ_MOUNT_SNAPSHOT:
                return fs_req_mount_snapshot(&req);
            default:
                printf("Invalid request type: %d\n", req_type);
        }
        assert(0);
        return fs_response_t();
    }

#ifndef NOT_KERNEL
    constexpr int NUM_FS_THREADS = 1;
    Semaphore fs_semaphore(NUM_FS_THREADS); // thought it was a nice way to queue requests w/o tons of extra code.

    void issue_fs_request(fs_req_type_t req_type, fs_req_t& req, Function<void(fs_response_t)> callback) {
        fs_semaphore.down([=]() {
            fs_response_t resp = fs_req_dispatch(req_type, req);
            fs_semaphore.up();
            create_event<fs_response_t>(callback, resp);
        });
    }
#endif
}

