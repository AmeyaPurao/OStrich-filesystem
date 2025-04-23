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
#ifdef NOT_KERNEL
    void init(BlockManager* block_manager) {
        fileSystem = FileSystem::getInstance(block_manager);
        blockManager = block_manager;
    }
#endif

    fs_resp_add_dir_t fs_req_add_dir(inode_index_t dir, inode_index_t file_to_add, const string& name) {
        FileSystem* fileSystem = FileSystem::getInstance();
        fs_resp_add_dir_t resp;

        if (fileSystem->isReadOnly()) {
            resp.status = FS_RESP_ERROR_PERMISSION;
            return resp;
        }

        Directory parent(dir, fileSystem->inodeTable, fileSystem->inodeBitmap,
                        fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);

        bool ok = parent.addDirectoryEntry(name.c_str(), file_to_add);
        resp.status = ok ? FS_RESP_SUCCESS : FS_RESP_ERROR_EXISTS;
        return resp;
    } 

fs_resp_create_file_t fs_req_create_file(inode_index_t cwd, bool is_dir, const string& name, uint16_t permissions) {
    fs_resp_create_file_t resp;
    FileSystem* fileSystem = FileSystem::getInstance();

    if (fileSystem->isReadOnly()) {
        resp.status = FS_RESP_ERROR_PERMISSION;
        resp.inode_index = INODE_NULL_VALUE;
        return resp;
    }

    resp.inode_index = INODE_NULL_VALUE;
    Directory parent(cwd, fileSystem->inodeTable, fileSystem->inodeBitmap,
                    fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);

    void* newEnt = nullptr;
    if (is_dir) {
        newEnt = parent.createDirectory(name.c_str());
    } else {
        newEnt = parent.createFile(name.c_str());
    }

    if (!newEnt) {
        resp.status = FS_RESP_ERROR_EXISTS;
    } else {
        resp.status = FS_RESP_SUCCESS;
        resp.inode_index = is_dir ? static_cast<Directory*>(newEnt)->getInodeNumber()
                                : static_cast<File*>(newEnt)->getInodeNumber();
    }
    return resp;
}

fs_resp_remove_file_t fs_req_remove_file(inode_index_t inode_index, const string& name) {
    fs_resp_remove_file_t resp;
    FileSystem* fileSystem = FileSystem::getInstance();
    if (fileSystem->isReadOnly()) {
        resp.status = FS_RESP_ERROR_PERMISSION;
        return resp;
    }

    Directory parent(inode_index, fileSystem->inodeTable, fileSystem->inodeBitmap,
                    fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);

    bool ok = parent.removeDirectoryEntry(name.c_str());
    resp.status = ok ? FS_RESP_SUCCESS : FS_RESP_ERROR_NOT_FOUND;
    return resp;
}
fs_resp_read_dir_t fs_req_read_dir(inode_index_t inode_index) {
    fs_resp_read_dir_t resp;
    FileSystem* fileSystem = FileSystem::getInstance();
    resp.entry_count = 0;

    Directory parent(inode_index, fileSystem->inodeTable, fileSystem->inodeBitmap,
                    fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);

    auto entries = parent.listDirectoryEntries();
    resp.status = FS_RESP_SUCCESS;

    for (auto namePtr : entries) {
        strncpy(resp.entries[resp.entry_count].name, namePtr, MAX_FILE_NAME_LENGTH);
        resp.entries[resp.entry_count].name[MAX_FILE_NAME_LENGTH] = '\0';
        resp.entries[resp.entry_count].inodeNumber = parent.getDirectoryEntry(namePtr);
        resp.entry_count++;
    }

    return resp;
}
    fs_resp_open_t fs_req_open(const string& path) {
        fs_resp_open_t resp;
        FileSystem* fileSystem = FileSystem::getInstance();
        resp.status = FS_RESP_ERROR_NOT_FOUND;
        resp.inode_index = INODE_NULL_VALUE;
        std::vector<string> path_parts;

        // Parse path (already minified)
        string cur_part;
        for (int i = 1; i < path.length(); i++) {
            if (path[i] == '/') {
                printf("Adding part %s\n", cur_part.c_str());
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
          resp.status = FS_RESP_SUCCESS;
        resp.inode_index = static_cast<File*>(newFile)->getInodeNumber();
            // todo need to implement get permissions
            // resp.data.open.permissions = (static_cast<File*>(newFile))->getPermissions();
        }
        return resp;
    }

fs_resp_write_t fs_req_write(inode_index_t inode_index, const char* buf, int offset, int n_bytes) {
    fs_resp_write_t resp;
    FileSystem* fileSystem = FileSystem::getInstance();

    if (fileSystem->isReadOnly()) {
        resp.status = FS_RESP_ERROR_PERMISSION;
        resp.bytes_written = 0;
        return resp;
    }

        File file(inode_index, fileSystem->inodeTable, fileSystem->inodeBitmap,
                fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
        bool success = file.write_at(offset, reinterpret_cast<const uint8_t*>(buf), n_bytes);
        if (success) {
            resp.status = FS_RESP_SUCCESS;
            resp.bytes_written = n_bytes;
        } else {
            resp.status = FS_RESP_ERROR_INVALID;
            resp.bytes_written = 0;
        }
        return resp;
    }

fs_resp_read_t fs_req_read(inode_index_t inode_index, char* buf, int offset, int n_bytes) {
    fs_resp_read_t resp;
    FileSystem* fileSystem = FileSystem::getInstance();
    File file(inode_index, fileSystem->inodeTable, fileSystem->inodeBitmap,
            fileSystem->blockBitmap, fileSystem->blockManager, fileSystem->logManager);
    bool success = file.read_at(offset, reinterpret_cast<uint8_t*>(buf), n_bytes);
    if (success) {
        resp.status = FS_RESP_SUCCESS;
        resp.bytes_read = n_bytes;
        } else {
            resp.status = FS_RESP_ERROR_INVALID;
            resp.bytes_read = 0;
        }
        return resp;
    }


    fs_resp_mount_snapshot_t fs_req_mount_snapshot(uint32_t checkpointID) {
        fs_resp_mount_snapshot_t resp;
        FileSystem* fileSystem = FileSystem::getInstance();
        if (checkpointID == 0) {
            resp.status = FS_RESP_SUCCESS;
        }
        else {
            FileSystem* snapshot = fileSystem->mountReadOnlySnapshot(checkpointID);
            if (snapshot) {
                fileSystem = snapshot;
                resp.status = FS_RESP_SUCCESS;
            } else {
                resp.status = FS_RESP_ERROR_NOT_FOUND;
            }
        }
        return resp;
    }
}

