// sec_parser.h

#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <sqlite3.h>

std::string fetchURL(const std::string& url);
std::string trim(const std::string& s);
std::string stripNamespace(const char* tagName);
std::string getQuarterFromDate(const std::string& dateStr);

std::vector<std::string> extractXmlLinks(const std::string& html, const std::string& baseUrl);
std::vector<std::tuple<std::string, std::string, std::string, std::string>> extract13FHRUrls(const std::string& idxPath);

void parse13F(const std::string& xmlContent,
              const std::string& name,
              const std::string& quarter,
              const std::string& filing_date,
              sqlite3* db);

extern const char* schema;
