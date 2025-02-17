#ifndef LOG_RECORD_H
#define LOG_RECORD_H

#include <cstdint>
#include <vector>
#include <cstring>

enum class LogRecordType : uint32_t {
    INODE_UPDATE = 1,
    BLOCK_ALLOCATION = 2,
    CHECKPOINT = 3,
    // Extend with additional types as needed.
};

struct LogRecordHeader {
    uint32_t recordType;       // From LogRecordType.
    uint64_t sequenceNumber;   // Global sequence.
    uint32_t payloadLength;    // Bytes in payload.
    uint32_t headerChecksum;   // Checksum over header fields.
};

class LogRecord {
public:
    // Default constructor.
    LogRecord();

    // Public constructor: users supply record type and payload.
    LogRecord(LogRecordType type, const std::vector<uint8_t>& payload);

    // Compute header checksum.
    static uint32_t computeChecksum(const LogRecordHeader &header);

    // Serialize record (header + payload) into a vector.
    std::vector<uint8_t> serialize() const;

    // Deserialize from data starting at offset.
    static LogRecord deserialize(const std::vector<uint8_t>& data, size_t &offset, bool &valid);

    LogRecordType getType() const;
    uint64_t getSequenceNumber() const;
    const std::vector<uint8_t>& getPayload() const;

private:
    LogRecordHeader header;
    std::vector<uint8_t> payload;

    // Internal setter to assign the sequence number and update checksum.
    void setSequenceNumber(uint64_t seq);

    // Declare LogManager as a friend so it can call setSequenceNumber.
    friend class LogManager;
};

#endif // LOG_RECORD_H
