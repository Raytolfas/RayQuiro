#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>

struct ArenaBlock {
    std::vector<uint8_t> data;
    size_t offset = 0;
};

class MemoryArena {
public:
    explicit MemoryArena(size_t defaultBlockSize = 65536)
        : defaultBlockSize_(std::max(defaultBlockSize, static_cast<size_t>(1024))) {
        allocateBlock(defaultBlockSize_);
    }

    uint8_t* alloc(size_t size, size_t alignment = 8) {
        if (size == 0) return nullptr;
        size_t currentOffset = blocks_.back().offset;
        size_t alignedOffset = (currentOffset + (alignment - 1)) & ~(alignment - 1);
        if (alignedOffset + size > blocks_.back().data.size()) {
            size_t newBlockSize = std::max(defaultBlockSize_, size + alignment);
            allocateBlock(newBlockSize);
            alignedOffset = 0;
            currentOffset = 0;
        }
        blocks_.back().offset = alignedOffset + size;
        usedBytes_ += (alignedOffset - currentOffset) + size;
        return blocks_.back().data.data() + alignedOffset;
    }

    void reset() {
        if (blocks_.size() > 1) {
            ArenaBlock first = std::move(blocks_[0]);
            first.offset = 0;
            blocks_.clear();
            blocks_.push_back(std::move(first));
        } else if (!blocks_.empty()) {
            blocks_[0].offset = 0;
        }
        usedBytes_ = 0;
    }

    size_t used() const {
        return usedBytes_;
    }

    size_t capacity() const {
        size_t total = 0;
        for (const auto& b : blocks_) {
            total += b.data.size();
        }
        return total;
    }

private:
    size_t defaultBlockSize_;
    size_t usedBytes_ = 0;
    std::vector<ArenaBlock> blocks_;

    void allocateBlock(size_t size) {
        ArenaBlock blk;
        blk.data.resize(size, 0);
        blk.offset = 0;
        blocks_.push_back(std::move(blk));
    }
};

class ArenaManager {
public:
    static ArenaManager& instance() {
        static ArenaManager inst;
        return inst;
    }

    int64_t createArena(size_t capacity = 65536) {
        std::lock_guard<std::mutex> lock(mutex_);
        int64_t id = nextId_++;
        arenas_[id] = std::make_shared<MemoryArena>(capacity);
        return id;
    }

    std::shared_ptr<MemoryArena> getArena(int64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = arenas_.find(id);
        if (it != arenas_.end()) return it->second;
        return nullptr;
    }

    bool destroyArena(int64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return arenas_.erase(id) > 0;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        arenas_.clear();
    }

private:
    std::mutex mutex_;
    int64_t nextId_ = 1;
    std::unordered_map<int64_t, std::shared_ptr<MemoryArena>> arenas_;
};
