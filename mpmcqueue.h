#pragma once
#include <mutex>
#include <chrono>
#include <thread>
#include <cstddef>
#include <atomic>
#include <functional>
#include <optional>
#include <queue>
#include <condition_variable>
#include <vector>
#include <shared_mutex>
#include <cstdint>


template <class T>
class MPMCQueue {
public:
    explicit MPMCQueue(uint64_t max_size) : data_(max_size) {
        for (uint64_t i = 0; i < data_.size(); ++i) {
            data_[i].generation = i;
        }
    }
    bool Push(const T& value) {
        while (1) {
            uint64_t curGeneration = data_[head_.load() & data_.size() - 1].generation;
            // Полная очередь <=> head != generation(head)
            uint64_t curHead = head_.load();
            if (curHead != data_[curHead & data_.size() - 1].generation) {
                return false;
            }
            if (head_.compare_exchange_weak(curGeneration, head_ + 1)) {
                data_[curGeneration].value = value;
                data_[curGeneration].generation.fetch_add(1);
                return true;
            }
        }
    }
    std::optional <T> Pop() {
        while (1) {
            uint64_t curGeneration = data_[tail_.load() & data_.size() - 1].generation - 1;
            uint64_t curTail = tail_.load();
            if (curTail == data_[curTail & data_.size() - 1].generation) {
                return std::nullopt;
            }
            if (tail_.compare_exchange_weak(curGeneration, tail_ + 1)) {
                data_[curGeneration].generation.fetch_add(data_.size() - 1);
                return data_[curGeneration].value;
            }
        }
    }

private:
    class Elem {
    public:
        std::atomic <uint64_t> generation = 0;
        T value;
    };
    std::vector <Elem> data_;
    std::atomic <uint64_t> head_ = 0;
    std::atomic <uint64_t> tail_ = 0;
};
