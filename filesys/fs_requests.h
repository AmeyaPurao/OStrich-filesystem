#ifndef FS_REQUESTS_H
#define FS_REQUESTS_H

#include "Block.h"
#include "Directory.h"
#include "FileSystem.h"
#include "../interface/BlockManager.h"
#include "string"
#include "cstdint"

#ifndef NOT_KERNEL
#include "event.h"
#include "function.h"
#include "printf.h"
#endif

namespace fs {
    // Singleton instance of FileSystem
#ifdef NOT_KERNEL
    inline FileSystem* fileSystem;
    inline BlockManager* blockManager;
    // Function to initialize the filesystem - must be run before calling any other function and takes a BlockManager
    void init(BlockManager* block_manager);
#endif

    // Enum for different request types
    enum fs_req_type_t {
        FS_REQ_ADD_DIR,
        FS_REQ_CREATE_FILE,
        FS_REQ_REMOVE_FILE,
        FS_REQ_READ_DIR,
        FS_REQ_READ,
        FS_REQ_OPEN,
        FS_REQ_WRITE,
        FS_REQ_MOUNT_SNAPSHOT,
    };

    // Enum for different response status codes
    enum fs_resp_status_t {
        FS_RESP_SUCCESS,
        FS_RESP_ERROR_NOT_FOUND,
        FS_RESP_ERROR_EXISTS,
        FS_RESP_ERROR_PERMISSION,
        FS_RESP_ERROR_INVALID,
        FS_RESP_ERROR_FULL
    };

    // Structures for different response types
    struct fs_resp_add_dir_t {
        fs_resp_status_t status;
    };

    struct fs_resp_create_file_t {
        fs_resp_status_t status;
        inode_index_t inode_index;
    };

    struct fs_resp_remove_file_t {
        fs_resp_status_t status;
    };

    struct fs_resp_read_dir_t {
        fs_resp_status_t status;
        int entry_count;
        dirEntry_t entries[DIRECTORY_ENTRIES_PER_BLOCK];
    };

    struct fs_resp_open_t {
        fs_resp_status_t status;
        inode_index_t inode_index;
        uint16_t permissions;
    };

    struct fs_resp_write_t {
        fs_resp_status_t status;
        int bytes_written;
    };

    struct fs_resp_read_t {
        fs_resp_status_t status;
        int bytes_read;
    };

    struct fs_resp_mount_snapshot_t {
        fs_resp_status_t status;
    };

    // Union of all response types
    union fs_response_data_t {
        fs_resp_add_dir_t add_dir;
        fs_resp_create_file_t create_file;
        fs_resp_remove_file_t remove_file;
        fs_resp_read_dir_t read_dir;
        fs_resp_open_t open;
        fs_resp_write_t write;
        fs_resp_read_t read;
        fs_resp_mount_snapshot_t mount_snapshot;
    };

    // Main response structure
    struct fs_response_t {
        fs_req_type_t req_type;
        fs_response_data_t data;
    };

    // Add directory entry
    fs_resp_add_dir_t fs_req_add_dir(inode_index_t dir, inode_index_t file_to_add, const string name);
    
    // Create file or directory
    fs_resp_create_file_t fs_req_create_file(inode_index_t cwd, bool is_dir, const string name, uint16_t permissions);
    
    // Remove file or directory
    fs_resp_remove_file_t fs_req_remove_file(inode_index_t inode_index, const string name);
    
    // Read directory contents
    fs_resp_read_dir_t fs_req_read_dir(inode_index_t inode_index);
    
    // Open file by path
    fs_resp_open_t fs_req_open(const string path);
    
    // Write to file
    fs_resp_write_t fs_req_write(inode_index_t inode_index, const char* buf, int offset, int n_bytes);
    
    // Read from file
    fs_resp_read_t fs_req_read(inode_index_t inode_index, char* buf, int offset, int n_bytes);
    
    // Mount snapshot
    fs_resp_mount_snapshot_t fs_req_mount_snapshot(uint32_t checkpointID);

#ifndef NOT_KERNEL
    // Add directory entry
    void issue_fs_add_dir(inode_index_t dir, inode_index_t file_to_add, 
                         const string name, Function<void(fs_response_t)> callback);
    
    // Create file or directory
    void issue_fs_create_file(inode_index_t cwd, bool is_dir, const string name, 
                         uint16_t permissions, Function<void(fs_response_t)> callback);
    
    // Remove file or directory
    void issue_fs_remove_file(inode_index_t inode_index, const string name, 
                         Function<void(fs_response_t)> callback);
    
    // Read directory contents
    void issue_fs_read_dir(inode_index_t inode_index, 
                         Function<void(fs_response_t)> callback);
    
    // Open file by path
    void issue_fs_open(const string path, 
                         Function<void(fs_response_t)> callback);
    
    // Write to file
    void issue_fs_write(inode_index_t inode_index, const char* buf, 
                         int offset, int n_bytes, Function<void(fs_response_t)> callback);
    
    // Read from file
    void issue_fs_read(inode_index_t inode_index, char* buf, 
                         int offset, int n_bytes, Function<void(fs_response_t)> callback);
    
    // Mount snapshot
    void issue_fs_mount_snapshot(uint32_t checkpointID, 
                         Function<void(fs_response_t)> callback);
#endif

} // namespace fs

#endif // FS_REQUESTS_H
