#pragma once
#include <cstdint>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

class RWSpinLock {
public:
    RWSpinLock() {
    }
    RWSpinLock(const RWSpinLock&) {
    }
    void LockRead() {
        uint64_t oldstate, newstate;
        while (1) {
            oldstate = state_.load();
            if (!(oldstate & 1)) {
                newstate = oldstate + 2;
                if (state_.compare_exchange_weak(oldstate, newstate)) {
                    break;
                }
            }
        }
    }

    void UnlockRead() {
        uint64_t oldstate, newstate;
        while (1) {
            oldstate = state_.load();
            newstate = oldstate - 2;
            if (state_.compare_exchange_weak(oldstate, newstate)) {
                break;
            }
        }
    }

    void LockWrite() {
        uint64_t oldstate, newstate;
        while (1) {
            oldstate = state_.load();
            if (!(oldstate & 1)) {
                newstate = oldstate | 1;
                if (state_.compare_exchange_weak(oldstate, newstate)) {
                    break;
                }
            }
        }
        while (1) {
            oldstate = state_.load();
            if (oldstate == 1) {
                break;
            }
        }
    }

    void UnlockWrite() {
        state_.store(0);
    }

private:
    std::atomic<uint64_t> state_ = 0;
};

template <class K, class V, class Hash = std::hash<K>>
class ConcurrentHashMap {
public:
    ConcurrentHashMap(const Hash& hasher = Hash()) : ConcurrentHashMap(kUndefinedSize, hasher) {
    }

    explicit ConcurrentHashMap(int expected_size, const Hash& hasher = Hash())
        : ConcurrentHashMap(expected_size, kDefaultConcurrencyLevel, hasher) {
    }

    ConcurrentHashMap(int expected_size, int expected_threads_count, const Hash& hasher = Hash())
        : hasher_(hasher) {
        if (expected_size != kUndefinedSize) {
            table_.resize(expected_size);
            locks_.resize(expected_size);
        } else {
            table_.resize(800000);
            locks_.resize(800000);
        }
    }

    bool Insert(const K& key, const V& value) {
        lock_.LockRead();
        if (size_ >= 2 * table_.size()) {
            lock_.UnlockRead();
            lock_.LockWrite();
            if (size_ >= 2 * table_.size()) {
                Expand();
            }
            lock_.UnlockWrite();
            lock_.LockRead();
        }
        bool res = false;
        size_t hash = GetIndex(key);
        locks_[hash].LockWrite();
        size_t num = table_[hash].size();
        for (size_t i = 0; i < num; ++i) {
            if (table_[hash][i].first == key) {
                res = true;
                break;
            }
        }
        if (res) {
            locks_[hash].UnlockWrite();
            lock_.UnlockRead();
            return false;
        }
        table_[hash].push_back({key, value});
        ++size_;
        locks_[hash].UnlockWrite();
        lock_.UnlockRead();
        return true;
    }

    bool Erase(const K& key) {
        lock_.LockRead();
        bool res = false;
        size_t hash = GetIndex(key);
        locks_[hash].LockWrite();
        size_t num = table_[hash].size();
        size_t curind;
        for (size_t i = 0; i < num; ++i) {
            if (table_[hash][i].first == key) {
                curind = i;
                res = true;
                break;
            }
        }
        if (res) {
            std::swap(table_[hash][curind], table_[hash].back());
            table_[hash].pop_back();
            --size_;
        }
        locks_[hash].UnlockWrite();
        lock_.UnlockRead();
        return res;
    }

    void Clear() {
        lock_.LockWrite();
        table_.clear();
        table_.resize(1);
        size_.store(0);
        lock_.UnlockWrite();
    }

    std::pair<bool, V> Find(const K& key) const {
        lock_.LockRead();
        bool res = false;
        size_t hash = GetIndex(key);
        locks_[hash].LockRead();
        size_t num = table_[hash].size();
        size_t curind;
        for (size_t i = 0; i < num; ++i) {
            if (table_[hash][i].first == key) {
                curind = i;
                res = true;
                break;
            }
        }
        V result;
        if (res) {
            result = table_[hash][curind].second;
        }
        locks_[hash].UnlockRead();
        lock_.UnlockRead();
        return {res, result};
    }

    const V At(const K& key) const {
        lock_.LockRead();
        bool res = false;
        size_t hash = GetIndex(key);
        locks_[hash].LockRead();
        size_t num = table_[hash].size();
        size_t curind;
        for (size_t i = 0; i < num; ++i) {
            if (table_[hash][i].first == key) {
                curind = i;
                res = true;
                break;
            }
        }
        V result;
        if (res) {
            result = table_[hash][curind].second;
        } else {
            throw std::out_of_range("");
        }
        locks_[hash].UnlockRead();
        lock_.UnlockRead();
        return result;
    }

    size_t Size() const {
        return size_.load();
    }
    static const int kDefaultConcurrencyLevel;
    static const int kUndefinedSize;

private:
    void Expand() {
        std::vector<std::vector<std::pair<K, V>>> newtable(table_.size() * 2);
        for (size_t i = 0; i < table_.size(); ++i) {
            for (size_t j = 0; j < table_[i].size(); ++j) {
                K key = table_[i][j].first;
                V value = table_[i][j].second;
                size_t hash = (hasher_(key) % newtable.size() + newtable.size()) % newtable.size();
                newtable[hash].emplace_back(key, value);
            }
        }
        locks_.resize(newtable.size());
        table_ = newtable;
    }
    size_t GetIndex(const K& key) const {
        return (hasher_(key) % table_.size() + table_.size()) % table_.size();
    }
    Hash hasher_;
    std::atomic<size_t> size_ = 0;
    std::vector<std::vector<std::pair<K, V>>> table_;
    mutable std::vector<RWSpinLock> locks_;
    mutable RWSpinLock lock_;
};

template <class K, class V, class Hash>
const int ConcurrentHashMap<K, V, Hash>::kDefaultConcurrencyLevel = 8;

template <class K, class V, class Hash>
const int ConcurrentHashMap<K, V, Hash>::kUndefinedSize = -1;
