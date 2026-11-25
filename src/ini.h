#pragma once
// Tiny INI reader: [section] headers, key = value, ';' or '#' start a comment.
// Keys are looked up as "section.key". Colors/vectors are lists like "80, 200, 255".
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class Ini {
public:
    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string line, section;
        while (std::getline(f, line)) {
            auto c = line.find_first_of("#;");
            if (c != std::string::npos) line.erase(c);
            line = trim(line);
            if (line.empty()) continue;
            if (line.front() == '[' && line.back() == ']') {
                section = trim(line.substr(1, line.size() - 2));
                continue;
            }
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            data_[section + "." + trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
        }
        return true;
    }

    bool has(const std::string& key) const { return data_.count(key) != 0; }

    std::string str(const std::string& key, const std::string& def = "") const {
        auto it = data_.find(key);
        return it == data_.end() ? def : it->second;
    }
    float num(const std::string& key, float def) const {
        auto it = data_.find(key);
        if (it == data_.end() || it->second.empty()) return def;
        char* end = nullptr;
        float v = std::strtof(it->second.c_str(), &end);
        return end == it->second.c_str() ? def : v;
    }
    int integer(const std::string& key, int def) const { return static_cast<int>(num(key, static_cast<float>(def))); }
    bool boolean(const std::string& key, bool def) const {
        auto it = data_.find(key);
        if (it == data_.end()) return def;
        std::string s = it->second;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return s == "1" || s == "true" || s == "yes" || s == "on";
    }
    std::vector<float> list(const std::string& key) const {
        std::vector<float> out;
        auto it = data_.find(key);
        if (it == data_.end()) return out;
        std::string s = it->second;
        for (char& ch : s)
            if (ch == ',') ch = ' ';
        std::istringstream ss(s);
        float v;
        while (ss >> v) out.push_back(v);
        return out;
    }

private:
    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }
    std::unordered_map<std::string, std::string> data_;
};
