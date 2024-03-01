#pragma once
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    explicit IteratorRange(Iterator page_begin, Iterator page_end)
        : page_begin_(page_begin), page_end_(page_end)
    {
    }

    Iterator begin() {
        return page_begin_;
    }

    Iterator end() {
        return page_end_;
    }

    Iterator size() {
        return destination(page_begin_, page_end_);
    }

private:
    Iterator page_begin_;
    Iterator page_end_;
};



template <typename Iterator>
std::ostream& operator<<(std::ostream& out, IteratorRange<Iterator> page) {
    for (auto link = page.begin(); link < page.end(); ++link) {
        out << *link;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    explicit Paginator(Iterator range_begin, Iterator range_end, const int page_size)
        : range_begin_(range_begin), range_end_(range_end), 
        range_size_(static_cast<int>(distance(range_begin, range_end)))
    {
        if (range_size_ % page_size > 0) {
            pages_count_ = range_size_ / page_size + 1;
        }
        else {
            pages_count_ = range_size_ / page_size;
        }

        Iterator temp_begin_iterator = range_begin_;

        for (int page = 1; page <= pages_count_; ++page) {
            if (page < pages_count_) {
                pages_.push_back(IteratorRange(temp_begin_iterator, temp_begin_iterator + page_size));
                temp_begin_iterator = temp_begin_iterator + page_size;
            }
            else {
                pages_.push_back(IteratorRange(temp_begin_iterator, range_end_));
            }
        }

    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    int size() const {
        return pages_count_;
    }

private:
    Iterator range_begin_;
    Iterator range_end_;
    int range_size_ = 0;
    int pages_count_ = 0;
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

