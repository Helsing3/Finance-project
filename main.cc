#include "sec_parser.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sqlite3.h>
#include <curl/curl.h>

using namespace std;

int main() {
    
    sqlite3* db;
    int rc = sqlite3_open("holdings.db", &db);
    if (rc) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return 1;
    }

    // Create database schema
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, schema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    //vector<tuple<string, string, string, string>> filings = extract13FHRUrls("master2025Q1.idx");
    vector<tuple<string, string,string,string>> filings = extract13FHRUrls("master_idx/master2025Q2.idx");
    //vector<tuple<string, string,string,string>> filings = extract13FHRUrls("master2025Q3.idx");

    for (const auto& [name, folderUrl, quarter, filing_date] : filings) {
        cout << endl << name << endl;

        string html = fetchURL(folderUrl);
        if (html.empty()) {
            cerr << "Failed to fetch folder HTML: " << folderUrl << endl;
            continue;
        }

        auto xmlLinks = extractXmlLinks(html, folderUrl);

        bool foundValidXml = false;
        for (const auto& url : xmlLinks) {
            string content = fetchURL(url);

            if (content.find("infoTable") != string::npos || content.find("<nameOfIssuer") != string::npos) {
                cout << "Valid XML found: " << url << endl;

                parse13F(content, name, quarter, filing_date, db);
                foundValidXml = true;
                break;
            }
        }

        if (!foundValidXml) {
            cerr << "No valid XML found for: " << folderUrl << endl;
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    sqlite3_close(db);
    curl_global_cleanup();
    return 0;
}
