#include "sec_parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <curl/curl.h>
#include <tinyxml2.h>
#include <chrono>
#include <thread>

using namespace std;
using namespace tinyxml2;

// Callback for libcurl
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

string stripNamespace(const char* tagName) {
    string name(tagName);
    auto pos = name.find(':');
    if (pos != string::npos) {
        return name.substr(pos + 1);
    }
    return name;
}

string getQuarterFromDate(const string& dateStr) {
    int year = stoi(dateStr.substr(0, 4));
    int month = stoi(dateStr.substr(5, 2));
    int q = (month - 1) / 3 + 1;
    return to_string(year) + "Q" + to_string(q);
}

const char* schema = R"sql(
    CREATE TABLE IF NOT EXISTS firms (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT UNIQUE
    );
    CREATE TABLE IF NOT EXISTS filings (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        firm_id INTEGER,
        filing_date TEXT,
        quarter TEXT,
        FOREIGN KEY(firm_id) REFERENCES firms(id)
    );
    CREATE TABLE IF NOT EXISTS holdings (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        filing_id INTEGER,
        cusip TEXT,
        name_of_issuer TEXT,
        shares INTEGER,
        value INTEGER,
        FOREIGN KEY(filing_id) REFERENCES filings(id)
    );
)sql";

vector<string> extractXmlLinks(const string& html, const string& baseUrl) {
    auto start = std::chrono::high_resolution_clock::now();
    vector<string> links;
    unordered_set<string> seen;
    smatch match;

    regex filePattern(R"(([\w\-\./]+\.xml))", regex_constants::icase);

    auto begin = sregex_iterator(html.begin(), html.end(), filePattern);
    auto end = sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        string filename = (*it)[1].str();

        // Extract just the filename if any path-like prefix exists
        size_t lastSlash = filename.find_last_of("/\\");
        string cleanFilename = (lastSlash != string::npos) ? filename.substr(lastSlash + 1) : filename;

        // Avoid duplicates
        if (seen.count(cleanFilename)) continue;
        seen.insert(cleanFilename);

        // Build full URL
        links.push_back(baseUrl + cleanFilename);
    }

    auto endd = std::chrono::high_resolution_clock::now();
    //std::cout << "Time taken for extractXmlLinks: "
    //      << std::chrono::duration_cast<std::chrono::milliseconds>(endd - start).count()
    //      << " ms" << std::endl;

    return links;
}

// returns: (firmName, folderUrl, quarter)
vector<tuple<string, string,string, string>> extract13FHRUrls(const string& idxPath) {
    auto start = std::chrono::high_resolution_clock::now();
    ifstream file(idxPath);
    vector<tuple<string, string,string, string>> filings;
    if (!file.is_open()) {
        cerr << "Could not open master.idx file.\n";
        return filings;
    }

    string line;
    bool startReading = false;

    while (getline(file, line)) {
        // Skip header until the real data starts
        if (!startReading) {
            if (line.find("CIK|Company Name|Form Type|Date Filed|Filename") != string::npos) {
                startReading = true;
            }
            continue;
        }

        vector<string> fields;
        stringstream ss(line);
        string field;

        while (getline(ss, field, '|')) {
            fields.push_back(field);
        }

        if (fields.size() != 5) continue;

        string cik = fields[0];
        string name = fields[1];
        string formType = fields[2];
        string dateFiled = fields[3]; // Format: YYYY-MM-DD
        string filename = fields[4];

        if (formType != "13F-HR") continue;

        // Extract accession number and format it
        smatch match;
        if (regex_search(filename, match, regex(R"(data/(\d+)/([^/]+)\.txt)"))) {
            string folderCIK = match[1].str();
            string accessionDashes = match[2].str();
            string accessionNoDashes = accessionDashes;
            accessionNoDashes.erase(remove(accessionNoDashes.begin(), accessionNoDashes.end(), '-'), accessionNoDashes.end());

            // Build base folder URL
            string folderUrl = "https://www.sec.gov/Archives/edgar/data/" + folderCIK + "/" + accessionNoDashes + "/";

            string quarter;
            if (dateFiled.size() >= 7) {
                int month = stoi(dateFiled.substr(5, 2));
                int year = stoi(dateFiled.substr(0, 4));
                int q = (month - 1) / 3 + 1;
                quarter = to_string(year) + "Q" + to_string(q);
            } else {
                quarter = "Unknown";
            }
            filings.emplace_back(name, folderUrl, quarter, dateFiled); 
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    //std::cout << "Time taken for extract13FHRUrls: "
    //      << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
    //      << " ms" << std::endl;
    return filings;
}

// Download a URL into a string
string fetchURL(const string& url) {
    auto start = std::chrono::high_resolution_clock::now();
    CURL* curl = curl_easy_init();
    string response;

    if (!curl) {
        cerr << "curl_easy_init() failed" << endl;
        return "";
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/xml, text/xml, */*;q=0.9");
    headers = curl_slist_append(headers, "Connection: keep-alive");

    // 🛠 Replace with your company/project info
    const char* userAgent = "FinanceApp/1.0 (Contact: vilhelm.helsing@gmail.com)";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent);
    curl_easy_setopt(curl, CURLOPT_REFERER, "https://www.sec.gov/");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    //cout << "HTTP response code: " << http_code << endl;

    if (res != CURLE_OK) {
        cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
    } else if (http_code != 200) {
        //cerr << "Server returned non-200 response: " << http_code << endl;
        response = "";
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    auto end = std::chrono::high_resolution_clock::now();
    //std::cout << "Time taken for fetchURL: "
    //      << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
    //      << " ms" << std::endl;

    return response;
}

void parse13F(string const& xmlContent, string const& name, string const& quarter, string const& filing_date, sqlite3* db) {
    auto start = std::chrono::high_resolution_clock::now();
    if (xmlContent.find("<html") != string::npos || xmlContent.find("<!DOCTYPE html") != string::npos) {
        cerr << "Received HTML instead of XML. Probably an error page." << endl;
        return;
    }

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xmlContent.c_str()) != tinyxml2::XML_SUCCESS) {
        cerr << "Failed to parse XML" << endl;
        return;
    }

    tinyxml2::XMLElement* root = doc.RootElement(); 
    unordered_map<string, long long> totalValue;
    unordered_map<string, long long> totalShares;

    // Insert or ignore firm
    sqlite3_stmt* stmt;

    // Insert or ignore firm (prepared)
    sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO firms (name) VALUES (?);", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Select firm ID
    sqlite3_prepare_v2(db, "SELECT id FROM firms WHERE name = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int firm_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        firm_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    // Add a sample filing (e.g., "2025-03-31", "2025Q1")
    sqlite3_prepare_v2(db, 
        "INSERT OR IGNORE INTO filings (firm_id, filing_date, quarter) VALUES (?, ?, ?);",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, firm_id);
    sqlite3_bind_text(stmt, 2, filing_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, quarter.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Get filing ID
    int filing_id = -1;
    sqlite3_prepare_v2(db,
        "SELECT id FROM filings WHERE firm_id = ? AND quarter = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, firm_id);
    sqlite3_bind_text(stmt, 2, quarter.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        filing_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db,
        "INSERT OR IGNORE INTO holdings (filing_id, cusip, name_of_issuer, shares, value, put_call) VALUES (?, ?, ?, ?, ?, ?);",
        -1, &stmt, nullptr);

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    for (XMLElement* entry = root->FirstChildElement(); entry; entry = entry->NextSiblingElement()) {
        if (stripNamespace(entry->Name()) != "infoTable") continue;

        auto getText = [&](XMLElement* elem, const char* tagName) -> const char* {
            for (XMLElement* child = elem->FirstChildElement(); child; child = child->NextSiblingElement()) {
                if (stripNamespace(child->Name()) == tagName)
                    return child->GetText();
            }
            return nullptr;
        };

        const char* name = getText(entry, "nameOfIssuer");
        const char* cusip = getText(entry, "cusip");
        const char* value = getText(entry, "value");
        const char* put_call = getText(entry, "putCall");  

        // sshPrnamt is nested inside <shrsOrPrnAmt>
        const char* shares = nullptr;

        XMLElement* shrsOrPrnAmt = nullptr;
        for (XMLElement* child = entry->FirstChildElement(); child; child = child->NextSiblingElement()) {
            if (stripNamespace(child->Name()) == "shrsOrPrnAmt") {
                shrsOrPrnAmt = child;
                break;
            }
        }
        if (shrsOrPrnAmt) {
            shares = getText(shrsOrPrnAmt, "sshPrnamt");
        }

        if (filing_id == -1) {
            std::cerr << "Error: filing_id not found for firm_id " << firm_id << " and quarter " << quarter << std::endl;
            return;
        }

        if (!name || !cusip || !shares) {
            std::cerr << "Skipping entry — missing fields: "
                      << (name ? "" : "name ") 
                      << (cusip ? "" : "cusip ") 
                      << (shares ? "" : "shares ") 
                      << std::endl;
            continue;
        }        
        
        string cusipStr = trim(cusip);

        // Insert all entries
        
        sqlite3_bind_int(stmt, 1, filing_id);
        sqlite3_bind_text(stmt, 2, cusipStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, atoll(shares));
        // Safely handle NULL value
        sqlite3_bind_int64(stmt, 5, value ? atoll(value) : 0);
        // Safely handle NULL put_call
        sqlite3_bind_text(stmt, 6, put_call ? put_call : nullptr, -1, SQLITE_STATIC);
        sqlite3_step(stmt);   

        
    }
    sqlite3_finalize(stmt); 
    sqlite3_exec(db, "END TRANSACTION;", nullptr, nullptr, nullptr);

    auto end = std::chrono::high_resolution_clock::now();
    //std::cout << "Time taken for parse13F: "
    //      << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
    //      << " ms" << std::endl;
}