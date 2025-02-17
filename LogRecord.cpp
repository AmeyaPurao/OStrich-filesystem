#include "LogRecord.h"

LogRecord::LogRecord() {
    header.recordType = 0;
    header.sequenceNumber = 0;
    header.payloadLength = 0;
    header.headerChecksum = 0;
}

LogRecord::LogRecord(LogRecordType type, const std::vector<uint8_t>& payload)
    : payload(payload)
{
    header.recordType = static_cast<uint32_t>(type);
    header.sequenceNumber = 0; // Will be assigned by LogManager.
    header.payloadLength = static_cast<uint32_t>(payload.size());
    header.headerChecksum = computeChecksum(header);
}

uint32_t LogRecord::computeChecksum(const LogRecordHeader &header) {
    auto seqLow = static_cast<uint32_t>(header.sequenceNumber & 0xFFFFFFFF);
    auto seqHigh = static_cast<uint32_t>((header.sequenceNumber >> 32) & 0xFFFFFFFF);
    return header.recordType + seqLow + seqHigh + header.payloadLength;
}

void LogRecord::setSequenceNumber(uint64_t seq) {
    header.sequenceNumber = seq;
    // Recompute checksum now that the sequence number has changed.
    header.headerChecksum = computeChecksum(header);
}

std::vector<uint8_t> LogRecord::serialize() const {
    std::vector<uint8_t> buffer(sizeof(LogRecordHeader) + payload.size());
    std::memcpy(buffer.data(), &header, sizeof(LogRecordHeader));
    if (!payload.empty()) {
        std::memcpy(buffer.data() + sizeof(LogRecordHeader), payload.data(), payload.size());
    }
    return buffer;
}

LogRecord LogRecord::deserialize(const std::vector<uint8_t>& data, size_t &offset, bool &valid) {
    LogRecord record;
    valid = false;
    if (offset + sizeof(LogRecordHeader) > data.size())
        return record;
    std::memcpy(&record.header, data.data() + offset, sizeof(LogRecordHeader));
    uint32_t computed = computeChecksum(record.header);
    if (computed != record.header.headerChecksum)
        return record; // Checksum mismatch.
    offset += sizeof(LogRecordHeader);
    if (offset + record.header.payloadLength > data.size())
        return record;
    record.payload.resize(record.header.payloadLength);
    std::memcpy(record.payload.data(), data.data() + offset, record.header.payloadLength);
    offset += record.header.payloadLength;
    valid = true;
    return record;
}

LogRecordType LogRecord::getType() const {
    return static_cast<LogRecordType>(header.recordType);
}

uint64_t LogRecord::getSequenceNumber() const {
    return header.sequenceNumber;
}

const std::vector<uint8_t>& LogRecord::getPayload() const {
    return payload;
}
