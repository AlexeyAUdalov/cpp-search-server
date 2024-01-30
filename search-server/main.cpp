#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    // Defines an invalid document id
    // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) {
        set<string> words = MakeUniqueNonEmptyStrings(stop_words);
        for (const auto& word : words) {
            if (IsValidWord(word)) {
                stop_words_.insert(word);
            }
            else {
                throw invalid_argument("incorrect stop-words: there are invalid characters (characters with codes from 0 to 31) in the stop-words"s);
            }
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("incorrect document_id: document_id is negative"s);
        }

        if (documents_.count(document_id)) {
            throw invalid_argument("incorrect document_id: document_id is exist in SearchServer"s);
        }

        const vector<string> words = SplitIntoWordsNoStop(document);

        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        document_id_sequence_.push_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);

        if (raw_query.empty()) {
            throw invalid_argument("incorrect search request: empty search request"s);
        }

        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    int GetDocumentId(const int index) const {
        if (index >= 0 && index < GetDocumentCount()) {
            return document_id_sequence_[index];
        }
        else {
            throw out_of_range("document_id is out of range"s);
        }
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
        if (document_id < 0) {
            throw invalid_argument("incorrect document_id: document_id is negative"s);
        }

        const Query query = ParseQuery(raw_query);

        if (raw_query.empty()) {
            throw invalid_argument("incorrect search request: empty search request"s);
        }

        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }


private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_id_sequence_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (IsValidWord(word)) {
                if (!IsStopWord(word)) {
                    words.push_back(word);
                }
            }
            else {
                throw invalid_argument("incorrect document: there are invalid characters (characters with codes from 0 to 31) in the document"s);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (IsValidWord(text)) {
            if (text[0] == '-') {
                is_minus = true;
                text = text.substr(1);
                if (text[0] == '-') {
                    throw invalid_argument("incorrect search request: there is double minus ('--') in the search request"s);
                }
                if (text.empty()) {
                    throw invalid_argument("incorrect search request: there is minus-word atribute ('-') without text in the search request"s);
                }
            }
            return { text, is_minus, IsStopWord(text) };
        }
        else {
            throw invalid_argument("incorrect search request: there are invalid characters (characters with codes from 0 to 31) in the search request"s);
        }
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}
int main() {
    try {
        SearchServer search_server("is are was a an in the with near at"s);
        //vector<string> stop_words = { "is"s, "are"s, "was"s, "a"s, "an"s, "in"s, "the"s, "with"s, "near"s, "at"s };
        //SearchServer search_server(stop_words);

        try {
            search_server.AddDocument(1, "a colorful parrot with green wings and red ta-il is lost"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        try {
            search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        try {
            search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        try {
            search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        try {
            search_server.AddDocument(0, "a grey hound with black ears is found at the railway station"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        try {
            search_server.AddDocument(3, "a white cat with long furry tail is found near the red square"s, DocumentStatus::ACTUAL, { 1, 2 });
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        try {
            search_server.AddDocument(2, "white red suare long"s, DocumentStatus::ACTUAL, { 3 });
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        try {
            //for (const Document& document : search_server.FindTopDocuments("white grey cat long red tail"s)) {
            for (const Document& document : search_server.FindTopDocuments("red"s)) {
                PrintDocument(document);
            }
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        try {
            tuple<vector<string>, DocumentStatus> result;
            string search_sequence = "a colorful parrot with green wings and red ta-il is lost"s;
            int document_id = 1;
            result = search_server.MatchDocument(search_sequence, document_id);
            cout << "The following words from search seaquence : '"s << search_sequence;
            cout << "' were found in document with id = " << document_id << ":"s << endl;
            for (const auto& word : get<0>(result)) {
                cout << word << endl;
            }
            cout << "DocumentStatus: " << static_cast<int>(get<1>(result)) << endl;
        }
        catch (const invalid_argument& error_message) {
            cout << "Error: "s << error_message.what() << endl;
        }

        for (int i = 0; i < 5; ++i) {
            try {
                cout << "index = " << i << " -> ";
                cout << "document_id = "s << search_server.GetDocumentId(i) << endl;;
            }
            catch (const out_of_range& error_message) {
                cout << "Error: "s << error_message.what() << endl;
            }
        }
    }
    catch (const invalid_argument& error_message) {
        cout << "Error: "s << error_message.what() << endl;
        return 0;
    }
}