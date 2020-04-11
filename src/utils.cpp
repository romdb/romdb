#include "utils.h"
#include <algorithm>
#include "utf8.h"

namespace utils
{
	bool startsWith(const std::string_view str, const std::string_view startsWith_)
	{
		if (startsWith_.size() > str.size())
			return false;
		return str.compare(0, startsWith_.size(), startsWith_) == 0;
	}

	bool endsWith(const std::string_view str, const std::string_view endsWith_)
	{
		if (endsWith_.size() > str.size())
			return false;
		return std::equal(endsWith_.rbegin(), endsWith_.rend(), str.rbegin());
	}

	std::string toLower(std::string str)
	{
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		return str;
	}

	std::string toUpper(std::string str)
	{
		std::transform(str.begin(), str.end(), str.begin(), ::toupper);
		return str;
	}

	std::string replaceString(const std::string& str, const std::string_view search, const std::string_view replace)
	{
		auto ret = str;
		size_t pos = 0;
		while ((pos = ret.find(search, pos)) != std::string::npos)
		{
			ret.replace(pos, search.length(), replace);
			pos += replace.length();
		}
		return ret;
	}

	std::pair<std::string_view, std::string_view> splitFileExtension(const std::string_view str)
	{
		auto pos = str.find_last_of('.');
		if (pos != std::string_view::npos)
			return std::make_pair(str.substr(0, pos), str.substr(pos, str.size() - pos));
		return std::make_pair(str, "");
	};

	std::pair<std::string, std::string> splitStringIn2(const std::string& str, char delimiter)
	{
		auto pos = str.find(delimiter, 0);
		if (pos != std::string::npos)
			return std::make_pair(str.substr(0, pos), str.substr(pos + 1, str.size() - pos));
		return std::make_pair(str, "");
	}

	std::vector<std::string> splitStringInLines(std::string str)
	{
		// remove \r
		str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());

		// split by \n
		std::vector<std::string> strings;
		std::string_view::size_type pos = 0;
		std::string_view::size_type prev = 0;
		while ((pos = str.find('\n', prev)) != std::string::npos)
		{
			strings.push_back(std::string(str.substr(prev, pos - prev)));
			prev = pos + 1;
		}
		strings.push_back(std::string(str.substr(prev)));
		return strings;
	}

	std::string mergeLines(std::vector<std::string> lines)
	{
		std::string str;
		for (const auto& line : lines)
			str += line + "\n";
		return str;
	}

	std::vector<std::string> filterStrings(stringSetNoCase& strings, std::string startsWith_)
	{
		std::vector<std::string> filtered;
		for (auto it = strings.begin(); it != strings.end();)
		{
			if (startsWith(*it, startsWith_))
			{
				filtered.push_back(*it);
				strings.erase(it++);
			}
			else
			{
				++it;
			}
		}
		return filtered;
	}

	std::wstring str2wstr(const std::string& str)
	{
		auto itEnd = utf8::find_invalid(str.begin(), str.end());
		std::wstring convStr;
		utf8::utf8to16(str.begin(), itEnd, back_inserter(convStr));
		return convStr;
	}

	std::string wstr2str(const std::wstring& str)
	{
		auto itEnd = utf8::find_invalid(str.begin(), str.end());
		std::string convStr;
		utf8::utf16to8(str.begin(), itEnd, back_inserter(convStr));
		return convStr;
	}
}
