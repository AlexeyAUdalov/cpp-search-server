#pragma once
#include <deque>
#include <string>
#include "document.h"
#include "search_server.h"


class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server),
        current_request_id_(0) {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        QueryResult() = default;

        QueryResult(uint64_t request_time, size_t number_of_results, int number_of_no_results)
            : request_time(request_time)
            , number_of_request_results(number_of_results)
            , number_of_no_results(number_of_no_results) {
        }

        uint64_t request_time = 0;
        size_t number_of_request_results = 0;
        int number_of_no_results = 0;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    uint64_t current_request_id_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> search_results = search_server_.FindTopDocuments(raw_query, document_predicate);

    current_request_id_ += 1;

    size_t quantity_of_search_results = search_results.size();

    if (requests_.empty()) {
        requests_.emplace_back(current_request_id_, quantity_of_search_results,
            static_cast<int>(quantity_of_search_results == 0));
    }
    else {
        int previous_no_resilts_number = requests_.back().number_of_no_results;

        if (requests_.size() == min_in_day_) {
            int front_request_result = requests_.front().number_of_request_results;
            requests_.emplace_back(current_request_id_, quantity_of_search_results,
                previous_no_resilts_number + static_cast<int>(quantity_of_search_results == 0)
                - static_cast<int>(front_request_result == 0));
            requests_.pop_front();
        }
        else {
            requests_.emplace_back(current_request_id_, quantity_of_search_results, previous_no_resilts_number
                + static_cast<int>(quantity_of_search_results == 0));
        }
    }

    return search_results;
}