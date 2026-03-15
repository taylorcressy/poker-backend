#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace utils::string {
    inline std::vector<std::string> split_string(const std::string& str, const char delimiter) {
        std::istringstream stream(str);
        std::vector<std::string> tokens;
        std::string token;

        while (std::getline(stream, token, delimiter)) {
            tokens.push_back(token);
        }

        return tokens;
    }

    inline std::vector<std::string_view> split_string_view(const std::string_view& view, const char delimiter) {
        std::vector<std::string_view> views;
        const char *curr = view.data();
        size_t i = 0;
        size_t start = 0;
        size_t size = 0;

        while (i < view.size()) {
            if (*(curr + i) == delimiter) {
                views.push_back({curr + start, size});
                start = i + 1;
                size = 0;
            }

            i++;
            size++;
        }

        if (start <= view.size()) {
            views.push_back(view.substr(start));
        }

        return views;
    }

    inline std::string_view trim_string_view(const std::string_view& view) {
        const char* whitespace = " \t\n\r\f\v";
        const size_t first = view.find_first_not_of(whitespace);
        if (first == std::string_view::npos) {
            return {};
        }
        const size_t last = view.find_last_not_of(whitespace);
        return view.substr(first, (last - first + 1));
    }
}