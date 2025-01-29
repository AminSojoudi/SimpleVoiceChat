//
//  ConcurrentBag.hpp
//  VoiceChatClient
//
//  Created by Amin on 8/17/24.
//

#ifndef CONCURRENT_BAG_H
#define CONCURRENT_BAG_H

#include <vector>
#include <mutex>
#include <optional>

template<typename T>
class ConcurrentBag {
public:
    ConcurrentBag(size_t maxSize);
    ~ConcurrentBag();

    void Add(const T& item);
    std::optional<T> TryTake();
    std::optional<T> GetAt(size_t index) const;
    bool IsEmpty() const;
    size_t Size() const;
    void Reset();
    void Erase(int numberOfItems);

private:
    std::vector<T> items_;
    mutable std::mutex mutex_;
    size_t maxSize_;
};


template<typename T>
ConcurrentBag<T>::ConcurrentBag(size_t maxSize) : maxSize_(maxSize) {}

template<typename T>
ConcurrentBag<T>::~ConcurrentBag() {}

template<typename T>
void ConcurrentBag<T>::Add(const T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (items_.size() >= maxSize_) {
        items_.erase(items_.begin());
    }
    items_.emplace_back(item);
}

template<typename T>
std::optional<T> ConcurrentBag<T>::TryTake() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (items_.empty()) {
        return std::nullopt;
    }
    T item = std::move(items_.back());
    items_.pop_back();
    return item;
}

template<typename T>
std::optional<T> ConcurrentBag<T>::GetAt(size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= items_.size()) {
        return std::nullopt;
    }
    return items_[index];
}

template<typename T>
bool ConcurrentBag<T>::IsEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.empty();
}

template<typename T>
size_t ConcurrentBag<T>::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.size();
}

template<typename T>
void ConcurrentBag<T>::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.clear();
}

template<typename T>
inline void ConcurrentBag<T>::Erase(int numberOfItems)
{
    std::lock_guard<std::mutex> lock(mutex_);
    items_.erase(items_.begin(), items_.begin() + numberOfItems);
}

#endif // CONCURRENT_BAG_H
