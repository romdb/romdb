#pragma once

#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace utils
{
	bool startsWith(const std::string_view str, const std::string_view startsWith_);
	bool endsWith(const std::string_view str, const std::string_view endsWith_);
	std::string toLower(std::string str);
	std::string toUpper(std::string str);
	std::string replaceString(const std::string& str, const std::string_view search, const std::string_view replace);

	struct compareCaseInsensitive
	{
		bool operator()(std::string a, std::string b) const { return toLower(a) < toLower(b); }
	};

	template <class T> using stringMapNoCase = std::map<std::string, T, compareCaseInsensitive>;
	using stringSetNoCase = std::set<std::string, compareCaseInsensitive>;

	std::pair<std::string, std::string> splitStringIn2(const std::string& str, char delimiter);
	std::vector<std::string> splitStringInLines(std::string str);
	stringSetNoCase filterStrings(stringSetNoCase& strings, std::string startsWith_);

	template <typename T> struct reversion_wrapper
	{
		T& iterable;
	};
	template <typename T> auto begin(reversion_wrapper<T> w) noexcept { return std::rbegin(w.iterable); }
	template <typename T> auto end(reversion_wrapper<T> w) noexcept { return std::rend(w.iterable); }
	template <typename T> reversion_wrapper<T> reverse(T&& iterable) noexcept { return { iterable }; }
}
