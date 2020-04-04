#pragma once

#include <filesystem>
#include <optional>
#include <sqlite3pp.h>

class Romdb
{
private:
	std::optional<sqlite3pp::database> db;

	// get long long from query that returns a single line/value
	bool getLong(const std::string_view sql, long long& val);

	// check if the database is a valid romdb database
	bool isValid();

	// import a system
	bool importSystem(
		const std::filesystem::path& romsPath, const std::filesystem::path& importPath, const std::string& configName);

	// creates a patch.txt list from the import folder
	static bool createSystemPatchFile(
		const std::filesystem::path& importPath, const std::string& patchFilePath, const std::string& configName);

public:
	// open a database
	bool open(const std::string& dbPath);

	// open a database or create one if it doesn't exist
	bool openOrCreate(const std::string& dbPath, const std::string& schemaPath);

	// check if the database is empty and create the schema if it is
	bool createSchema(const std::string& schemaPath);

	// import systems
	bool import(const std::string& importPath, const std::string& configName);

	// import systems
	bool import(const std::string& romsPath, const std::string& importPath, const std::string& configName);

	// get or reconstruct file
	std::vector<char> getFile(long long fileId);

	// dump a database
	bool dump(const std::string& dumpPath, bool fullDump);

	// verify a database
	void verify();

	// creates a patch.txt list from the import folder
	static bool createPatchFile(
		const std::string& importPath, const std::string& patchFilePath, const std::string& configName);
};
