#pragma once

#include <iterator>
#include <map>
#include <natural_sort.hpp>
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
	std::pair<std::string_view, std::string_view> splitFileExtension(const std::string_view str);
	std::pair<std::string, std::string> splitStringIn2(const std::string& str, char delimiter);
	std::vector<std::string> splitStringInLines(std::string str);

	// compares case insensitive and compares file name separate from extension
	struct compareCaseInsensitive
	{
		bool operator()(const std::string& a, const std::string& b) const
		{
			auto aa = splitFileExtension(a);
			auto bb = splitFileExtension(b);
			int cmp = SI::natural::comparei(aa.first, bb.first);
			if (cmp == 0)
				return SI::natural::comparei(aa.second, bb.second) < 0;
			return cmp < 0;
		}
	};

	template <class T> using stringMapNoCase = std::map<std::string, T, compareCaseInsensitive>;
	using stringSetNoCase = std::set<std::string, compareCaseInsensitive>;

	std::vector<std::string> filterStrings(stringSetNoCase& strings, std::string startsWith_);

	// converts UTF-8 string to UTF-16 wstring
	std::wstring str2wstr(const std::string& str);

	// converts UTF-16 wstring to UTF-8 string
	std::string wstr2str(const std::wstring& str);

	template <typename T> struct reversion_wrapper
	{
		T& iterable;
	};
	template <typename T> auto begin(reversion_wrapper<T> w) noexcept { return std::rbegin(w.iterable); }
	template <typename T> auto end(reversion_wrapper<T> w) noexcept { return std::rend(w.iterable); }
	template <typename T> reversion_wrapper<T> reverse(T&& iterable) noexcept { return { iterable }; }
}
