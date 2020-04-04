#include "romdb.h"
#include <algorithm>
#include <cctype>
#include "file.h"
#include <iostream>
#include "schema.h"
#include "utils.h"

using namespace sqlite3pp;
namespace fs = std::filesystem;

namespace
{
	using TagsMap = utils::stringMapNoCase<utils::stringMapNoCase<std::string>>;

	TagsMap getTags(const fs::path& tagsPath)
	{
		if (!fs::exists(tagsPath) || !fs::is_directory(tagsPath))
		{
			return {};
		}

		TagsMap tags;

		using directory_iterator = fs::directory_iterator;
		for (const auto& it : directory_iterator(tagsPath))
		{
			if (!it.is_regular_file())
				continue;
			if (it.path().extension() != ".txt")
				continue;

			auto tag = utils::splitStringIn2(it.path().stem().string(), '.');
			if (tag.first.empty())
				continue;

			auto tagLines = utils::splitStringInLines(file::readText(it.path().string()));
			if (tagLines.empty())
				continue;

			for (const auto& key : tagLines)
			{
				if (key.empty())
					continue;
				tags[key][tag.first] = tag.second;
			}
		}
		return tags;
	}

	fs::path getImportFile(const fs::path& importPath, const std::string& fileName, const std::string& configName)
	{
		if (!configName.empty())
		{
			auto filePath = importPath / (fileName + "." + configName + ".txt");
			if (fs::exists(filePath))
				return filePath;
		}
		return importPath / (fileName + ".txt");
	}

	void upsertChecksum(
		database& db, const std::vector<char>& data, long long fileId, const std::string& hashingAlgorithm)
	{
		auto hash = file::hash::compute(data, hashingAlgorithm);
		if (!hash.empty())
		{
			command cmd(db, "INSERT INTO checksum (file_id, name, data) VALUES(:file_id, :name, :data) ON "
							"CONFLICT(file_id, name) DO UPDATE SET data = excluded.data");
			cmd.bind(":file_id", fileId);
			cmd.bind(":name", hashingAlgorithm, nocopy);
			cmd.bind(":data", hash, nocopy);
			cmd.execute();
		}
	}
}

bool Romdb::open(const std::string& dbPath)
{
	if (db)
		return false;
	db = std::move(database());
	if (db->connect(dbPath.c_str(), SQLITE_OPEN_READWRITE) == SQLITE_OK && isValid())
		return true;
	db.reset();
	return false;
}

bool Romdb::openOrCreate(const std::string& dbPath, const std::string& schemaPath)
{
	if (db)
		return false;
	db = std::move(database(dbPath.c_str()));
	createSchema(schemaPath);
	if (isValid())
		return true;
	db.reset();
	return false;
}

bool Romdb::createSchema(const std::string& schemaPath)
{
	if (!db)
		return false;

	// check if db is empty
	long long val;
	if (getLong("SELECT count(*) FROM sqlite_master WHERE type = 'table'", val) && val != 0)
	{
		return false;
	}

	// create schema
	std::string schema;
	if (fs::exists(schemaPath))
	{
		schema = file::readText(schemaPath);
	}
	if (!schema.empty())
	{
		return db->execute(schema.c_str()) == SQLITE_OK;
	}
	else
	{
		return db->execute(defaultSchema.c_str()) == SQLITE_OK;
	}
}

bool Romdb::getLong(const std::string_view sql, long long& val)
{
	query qry(*db, sql.data());
	if (qry.column_count() == 1)
	{
		val = (*qry.begin()).get<long long>(0);
		return true;
	}
	return false;
}

bool Romdb::isValid()
{
	try
	{
		query qry1(*db, "SELECT id, name, code FROM system WHERE id = -1");
		query qry2(*db, "SELECT id, name, system_id FROM media WHERE id = -1");
		query qry3(*db, "SELECT id, name, data, size, compression, media_id, parent_id FROM file WHERE id = -1");
		query qry4(*db, "SELECT file_id, name, data FROM checksum WHERE file_id = -1");
		query qry5(*db, "SELECT id, name, value FROM tag WHERE id = -1");
		query qry6(*db, "SELECT tag_id, media_id FROM mediatag WHERE tag_id = -1");
		query qry7(*db, "SELECT tag_id, file_id FROM filetag WHERE tag_id = -1");
	}
	catch (database_error ex)
	{
		return false;
	}
	return true;
}

bool Romdb::import(const std::string& importPath_, const std::string& configName)
{
	auto romsPath = fs::path(importPath_) / "files";
	return import(romsPath.string(), importPath_, configName);
}

bool Romdb::import(const std::string& romsPath_, const std::string& importPath_, const std::string& configName)
{
	if (!db)
		return false;

	fs::path romsPath(romsPath_);
	fs::path importPath(importPath_);

	if (!fs::exists(romsPath) || !fs::is_directory(romsPath))
		return false;

	if (!fs::exists(importPath) || !fs::is_directory(importPath))
		return false;

	auto systemsFilePath = getImportFile(importPath, "systems", configName);
	if (fs::exists(systemsFilePath) && !fs::is_directory(systemsFilePath))
	{
		bool ret = false;
		auto systemsLines = utils::splitStringInLines(file::readText(systemsFilePath.string()));
		for (const auto& line : systemsLines)
		{
			if (line.empty())
				continue;

			auto systemImportPath = importPath / line;
			if (!fs::exists(systemImportPath) || !fs::is_directory(systemImportPath))
				continue;

			ret |= importSystem(romsPath, systemImportPath, configName);
		}
		return ret;
	}
	return importSystem(romsPath, importPath, configName);
}

bool Romdb::importSystem(const fs::path& romsPath, const fs::path& importPath, const std::string& configName)
{
	// import system
	long long systemId = 0;
	std::string systemName;
	std::string systemCode;
	std::string compressionAlgorithm;
	std::string hashingAlgorithm;
	{
		auto systemFilePath = getImportFile(importPath, "system", configName);
		if (!fs::exists(systemFilePath) || fs::is_directory(systemFilePath))
		{
			return false;
		}
		auto systemLines = utils::splitStringInLines(file::readText(systemFilePath.string()));
		if (systemLines.size() < 2)
		{
			return false;
		}
		if (systemLines.size() >= 3)
		{
			compressionAlgorithm = utils::toLower(systemLines[2]);
		}
		if (systemLines.size() >= 4)
		{
			hashingAlgorithm = utils::toLower(systemLines[3]);
		}

		command cmd(*db, "INSERT INTO system (name, code) VALUES(:name, :code) ON CONFLICT(code) DO NOTHING");
		cmd.bind(":name", systemLines[1], nocopy);
		cmd.bind(":code", systemLines[0], nocopy);

		if (cmd.execute() == SQLITE_OK)
		{
			query qry(*db, "SELECT id, name, code FROM system WHERE code = :code");
			qry.bind(":code", systemLines[0], nocopy);
			for (const auto& row : qry)
			{
				systemId = row.get<long long>(0);
				systemName = row.get<std::string>(1);
				systemCode = row.get<std::string>(2);
				break;
			}
		}
	}

	// import media
	{
		auto mediaFilePath = getImportFile(importPath, "media", configName);
		if (!fs::exists(mediaFilePath) || fs::is_directory(mediaFilePath))
		{
			return false;
		}
		auto mediaLines = utils::splitStringInLines(file::readText(mediaFilePath.string()));
		if (mediaLines.empty())
		{
			return false;
		}

		auto mediaTags = getTags(importPath / "mediatag");

		for (const auto& media : mediaLines)
		{
			if (media.empty())
				continue;

			command cmd(*db, "INSERT INTO media (name, system_id) VALUES(:name, :system_id) ON CONFLICT DO NOTHING");
			cmd.bind(":name", media, nocopy);
			cmd.bind(":system_id", systemId);

			long long mediaId = 0;
			if (cmd.execute() == SQLITE_OK)
			{
				query qry(*db, "SELECT id FROM media WHERE name = :name AND system_id = :system_id");
				qry.bind(":name", media, nocopy);
				qry.bind(":system_id", systemId);
				for (const auto& row : qry)
				{
					mediaId = row.get<long long>(0);
					break;
				}
			}

			auto it = mediaTags.find(media);
			if (it == mediaTags.end())
				continue;

			for (const auto& tag : it->second)
			{
				command cmd2(*db, "INSERT INTO tag (name, value) VALUES(:name, :value) ON CONFLICT DO NOTHING");
				cmd2.bind(":name", tag.first, nocopy);
				if (!it->second.empty())
					cmd2.bind(":value", tag.second, nocopy);
				else
					cmd2.bind(":value");

				long long tagId = 0;
				if (cmd2.execute() == SQLITE_OK)
				{
					query qry(*db, "SELECT id FROM tag WHERE name = :name AND value = :value");
					qry.bind(":name", tag.first, nocopy);
					if (!it->second.empty())
						qry.bind(":value", tag.second, nocopy);
					else
						qry.bind(":value");
					for (const auto& row : qry)
					{
						tagId = row.get<long long>(0);
						break;
					}
				}

				command cmd3(
					*db, "INSERT INTO mediatag (tag_id, media_id) VALUES(:tag_id, :media_id) ON CONFLICT DO NOTHING");
				cmd3.bind(":tag_id", tagId);
				cmd3.bind(":media_id", mediaId);
				cmd3.execute();
			}
		}
	}

	// load patch
	utils::stringMapNoCase<std::string> patchLinesMap;
	utils::stringMapNoCase<long long> patchIds;
	utils::stringMapNoCase<long long> patchParentIds;
	while (true)
	{
		auto patchFilePath = getImportFile(importPath, "patch", configName);
		if (!fs::exists(patchFilePath) || fs::is_directory(patchFilePath))
		{
			break;
		}
		auto patchLines = utils::splitStringInLines(file::readText(patchFilePath.string()));
		if (patchLines.empty())
		{
			break;
		}
		std::string val;
		for (const auto& line : patchLines)
		{
			if (line.empty())
			{
				val.clear();
				continue;
			}
			if (val.empty())
			{
				val = line;
				continue;
			}
			patchLinesMap[line] = val;
			patchParentIds[val] = 0;
		}
		break;
	}

	// import files
	{
		auto fileFilePath = getImportFile(importPath, "file", configName);
		if (!fs::exists(fileFilePath) || fs::is_directory(fileFilePath))
		{
			return false;
		}
		auto fileLines = utils::splitStringInLines(file::readText(fileFilePath.string()));
		if (fileLines.empty())
		{
			return false;
		}
		utils::stringSetNoCase fileLinesSet(fileLines.begin(), fileLines.end());

		using MediaFile = std::pair<long long, utils::stringSetNoCase>;
		std::set<MediaFile> filesToInsert;

		// group files to media
		query qry(*db, "SELECT id, name FROM media WHERE system_id = :system_id ORDER BY name COLLATE NOCASE DESC");
		qry.bind(":system_id", systemId);
		for (const auto& row : qry)
		{
			auto mediaId = row.get<long long>(0);
			auto mediaName = row.get<std::string>(1);
			auto filteredFiles = utils::filterStrings(fileLinesSet, mediaName);
			filesToInsert.insert({ mediaId, filteredFiles });
		}

		auto fileTags = getTags(importPath / "filetag");

		// insert files
		for (const auto& files : filesToInsert)
		{
			auto mediaId = files.first;
			for (const auto& file : files.second)
			{
				if (file.empty())
					continue;

				auto filePath = romsPath / file;
				if (!fs::exists(filePath) || fs::is_directory(filePath))
				{
					continue;
				}
				std::vector<char> fileBytes;
				long long uncompressedFileSize = 0;
				bool fileCompressed = false;
				if (patchLinesMap.find(file) == patchLinesMap.end())
				{
					fileBytes = file::readBytes(filePath.string());
					uncompressedFileSize = fileBytes.size();
					fileCompressed = file::compress(fileBytes, compressionAlgorithm);
				}
				else
					uncompressedFileSize = fs::file_size(filePath);

				command cmd(*db, "INSERT INTO file (name, data, size, compression, media_id) VALUES(:name, :data, "
								 ":size, :compression, :media_id) ON CONFLICT DO NOTHING");
				cmd.bind(":name", file, nocopy);
				if (!fileBytes.empty())
					cmd.bind(":data", fileBytes.data(), fileBytes.size(), nocopy);
				else
					cmd.bind(":data");
				cmd.bind(":size", uncompressedFileSize);
				if (fileCompressed)
					cmd.bind(":compression", compressionAlgorithm, nocopy);
				else
					cmd.bind(":compression");
				cmd.bind(":media_id", mediaId);
				auto fileInsertResult = cmd.execute();

				if (hashingAlgorithm.empty())
					continue;

				long long fileId = 0;
				if (fileInsertResult == SQLITE_OK)
				{
					query qry(*db, "SELECT id FROM file WHERE name = :name AND media_id = :media_id");
					qry.bind(":name", file, nocopy);
					qry.bind(":media_id", mediaId);
					for (const auto& row : qry)
					{
						fileId = row.get<long long>(0);

						if (fileBytes.empty())
							patchIds[file] = fileId;

						auto patchParentIt = patchParentIds.find(file);
						if (patchParentIt != patchParentIds.end())
							patchParentIt->second = fileId;

						break;
					}
				}

				// upsert file hash
				upsertChecksum(*db, fileBytes, fileId, hashingAlgorithm);

				// insert file tags
				auto it = fileTags.find(file);
				if (it == fileTags.end())
					continue;

				for (const auto& tag : it->second)
				{
					command cmd2(*db, "INSERT INTO tag (name, value) VALUES(:name, :value) ON CONFLICT DO NOTHING");
					cmd2.bind(":name", tag.first, nocopy);
					if (!it->second.empty())
						cmd2.bind(":value", tag.second, nocopy);
					else
						cmd2.bind(":value");

					long long tagId = 0;
					if (cmd2.execute() == SQLITE_OK)
					{
						query qry(*db, "SELECT id FROM tag WHERE name = :name AND value = :value");
						qry.bind(":name", tag.first, nocopy);
						if (!it->second.empty())
							qry.bind(":value", tag.second, nocopy);
						else
							qry.bind(":value");
						for (const auto& row : qry)
						{
							tagId = row.get<long long>(0);
							break;
						}
					}

					command cmd3(*db, "INSERT INTO filetag (tag_id, file_id) VALUES(:tag_id, "
									  ":file_id) ON CONFLICT DO NOTHING");
					cmd3.bind(":tag_id", tagId);
					cmd3.bind(":file_id", fileId);
					cmd3.execute();
				}
			}
		}
	}

	// import patches
	for (const auto& patchLine : patchLinesMap)
	{
		// update patch parent ids that are 0 (parent files from another system)
		for (auto& patchParentId : patchParentIds)
		{
			if (patchParentId.second)
				continue;

			query qry(*db, "SELECT id FROM file WHERE name = :name AND media_id NOT IN (SELECT id FROM media "
						   "WHERE system_id = :system_id)");
			qry.bind(":name", patchParentId.first, nocopy);
			qry.bind(":system_id", systemId);
			for (const auto& row : qry)
			{
				patchParentId.second = row.get<long long>(0);
				break;
			}
		}

		long long fileId = 0;
		long long parentId = 0;
		{
			auto it = patchIds.find(patchLine.first);
			if (it == patchIds.end())
				continue;
			fileId = it->second;

			auto it2 = patchParentIds.find(patchLine.second);
			if (it2 == patchParentIds.end())
				continue;
			parentId = it2->second;
		}

		auto file1Path = romsPath / patchLine.second;
		auto file2Path = romsPath / patchLine.first;
		std::vector<char> fileBytes;
		bool hasPatch = file::createPatch(file1Path.string(), file2Path.string(), fileBytes);
		bool bytesCompressed = file::compress(fileBytes, compressionAlgorithm);

		command cmd(*db, "UPDATE file SET data = :data, compression = :compression, parent_id = :parent_id "
						 "WHERE id = :file_id");
		cmd.bind(":data", fileBytes.data(), fileBytes.size(), nocopy);
		if (bytesCompressed)
			cmd.bind(":compression", compressionAlgorithm, nocopy);
		else
			cmd.bind(":compression");
		if (hasPatch)
			cmd.bind(":parent_id", parentId);
		else
			cmd.bind(":parent_id");
		cmd.bind(":file_id", fileId);
		cmd.execute();

		if (!hashingAlgorithm.empty())
			upsertChecksum(*db, fileBytes, fileId, hashingAlgorithm);
	}
	return true;
}

std::vector<char> Romdb::getFile(long long fileId)
{
	if (!db)
		return {};

	std::vector<char> fileBytes;
	query qry(*db, "SELECT data, LENGTH(data), size, IFNULL(compression, ''), parent_id FROM file WHERE id = :file_id");
	qry.bind(":file_id", fileId);
	for (const auto& file : qry)
	{
		auto data = file.get<void const*>(0);
		auto size = file.get<long long>(1);
		auto uncompressedSize = file.get<long long>(2);
		auto compression = file.get<std::string>(3);
		auto parentId = file.get<long long>(4);

		fileBytes = std::vector<char>((const char*)data, (const char*)data + size);
		file::uncompress(fileBytes, (size_t)uncompressedSize, compression);

		if (file.column_type(4) != SQLITE_NULL)
		{
			auto inputBytes = getFile(parentId);
			fileBytes = file::applyPatch(
				inputBytes.data(), inputBytes.size(), fileBytes.data(), fileBytes.size(), (size_t)uncompressedSize);
		}
		break;
	}
	return fileBytes;
}

bool Romdb::dump(const std::string& dumpPath_, bool fullDump)
{
	if (!db)
		return false;

	fs::path dumpPath(dumpPath_);

	if (!fs::exists(dumpPath) || !fs::is_directory(dumpPath))
	{
		return false;
	}
	for (const auto& system : query(*db, "SELECT id, name, code FROM system"))
	{
		auto systemId = system.get<long long>(0);
		auto systemName = system.get<std::string>(1);
		auto systemCode = system.get<std::string>(2);

		auto systemPath = dumpPath / systemCode;
		fs::create_directories(systemPath);
		if (!fs::is_empty(systemPath))
			continue;

		if (fullDump)
		{
			// system
			std::string systemText = systemCode + "\n" + systemName + "\n";

			// compression
			std::string compression = "none";
			query qry(*db,
				"SELECT LOWER(compression) FROM file WHERE compression IS NOT NULL AND media_id IN (SELECT id "
				"FROM media WHERE system_id = 1) LIMIT 1");
			qry.bind(":system_id", systemId);
			for (const auto& val : qry)
			{
				compression = val.get<std::string>(0);
				break;
			}
			systemText += compression + "\n";

			// checksum
			std::string checksum = "none";
			query qry2(*db, "SELECT LOWER(name) FROM checksum WHERE file_id IN (SELECT id FROM file WHERE media_id IN "
							"(SELECT id FROM media WHERE system_id = :system_id)) LIMIT 1");
			qry2.bind(":system_id", systemId);
			for (const auto& val : qry2)
			{
				checksum = val.get<std::string>(0);
				break;
			}
			systemText += checksum + "\n";

			auto systemTextPath = systemPath / "system.txt";
			file::writeText(systemTextPath.string(), systemText);
		}

		// files
		std::string fileText;
		std::string fileTagText;
		fs::path filesPath;
		if (fullDump)
		{
			filesPath = systemPath / "files";
			fs::create_directories(filesPath);
		}
		else
		{
			filesPath = systemPath;
		}
		{
			query qry(*db, "SELECT id, name FROM file WHERE media_id IN (SELECT id FROM media "
						   "WHERE system_id = :system_id)");
			qry.bind(":system_id", systemId);
			for (const auto& file : qry)
			{
				auto fileId = file.get<long long>(0);
				auto fileName = file.get<std::string>(1);
				auto fileData = getFile(fileId);

				if (fullDump)
					fileText += fileName + "\n";

				auto filePath = filesPath / fileName;
				file::writeBytes(filePath.string(), fileData.data(), fileData.size());
			}
		}
		if (!fullDump)
			continue;

		auto fileTextPath = systemPath / "file.txt";
		file::writeText(fileTextPath.string(), fileText);

		// patch
		{
			std::string patchText;
			std::string currentPatchKey;
			query qry(*db, "SELECT f2.name parent, f1.name name FROM file f1, file f2 WHERE f1.parent_id IS NOT "
						   "NULL AND f1.parent_id = f2.id AND f2.media_id IN (SELECT id FROM media WHERE system_id = "
						   ":system_id) ORDER BY parent, name COLLATE NOCASE");
			qry.bind(":system_id", systemId);
			for (const auto& patch : qry)
			{
				auto patchKey = patch.get<std::string>(0);
				if (currentPatchKey.empty())
				{
					currentPatchKey = patchKey;
					patchText += currentPatchKey + "\n";
				}
				else if (currentPatchKey != patchKey)
				{
					currentPatchKey = patchKey;
					patchText += "\n" + currentPatchKey + "\n";
				}
				patchText += patch.get<std::string>(1) + "\n";
			}
			auto patchTextPath = systemPath / "patch.txt";
			file::writeText(patchTextPath.string(), patchText);
		}

		// media
		{
			std::string mediaText;
			query qry(*db, "SELECT name FROM media WHERE system_id = :system_id");
			qry.bind(":system_id", systemId);
			for (const auto& media : qry)
			{
				mediaText += media.get<std::string>(0) + "\n";
			}
			auto mediaTextPath = systemPath / "media.txt";
			file::writeText(mediaTextPath.string(), mediaText);
		}

		// filetag
		{
			utils::stringMapNoCase<std::string> fileTagTexts;
			query qry(*db, "SELECT * FROM (SELECT t.name || '.txt' filetagname, f.name name FROM tag t, file f, "
						   "filetag ft, media m WHERE t.id = ft.tag_id AND f.id = ft.file_id AND f.media_id = m.id "
						   "AND m.system_id = :system_id AND LENGTH(t.value) = 0 UNION SELECT t.name || '.' || "
						   "t.value || '.txt' filetagname, f.name name FROM tag t, file f, filetag ft, media m "
						   "WHERE t.id = ft.tag_id AND f.id = ft.file_id AND f.media_id = m.id AND m.system_id = "
						   ":system_id AND LENGTH(t.value) > 0) ORDER BY filetagname, name COLLATE NOCASE");
			qry.bind(":system_id", systemId);
			for (const auto& fileTag : qry)
			{
				auto key = fileTag.get<std::string>(0);
				auto value = fileTag.get<std::string>(1);
				fileTagTexts[key] += value + "\n";
			}

			auto fileTagPath = systemPath / "filetag";
			fs::create_directories(fileTagPath);
			for (const auto& fileTag : fileTagTexts)
			{
				auto fileTagFilePath = fileTagPath / fileTag.first;
				file::writeText(fileTagFilePath.string(), fileTag.second);
			}
		}

		// mediatag
		{
			utils::stringMapNoCase<std::string> mediaTagTexts;
			query qry(*db,
				"SELECT * FROM (SELECT t.name || '.txt' filetagname, m.name name FROM tag t, media m, mediatag "
				"mt WHERE t.id = mt.tag_id AND m.id = mt.media_id AND LENGTH(t.value) = 0 AND m.system_id = "
				":system_id UNION SELECT t.name || '.' || t.value || '.txt' filetagname, m.name name FROM tag "
				"t, media m, mediatag mt WHERE t.id = mt.tag_id AND m.id = mt.media_id AND LENGTH(t.value) > 0 "
				"AND m.system_id = :system_id) ORDER BY filetagname, name COLLATE NOCASE");
			qry.bind(":system_id", systemId);
			for (const auto& mediaTag : qry)
			{
				auto key = mediaTag.get<std::string>(0);
				auto value = mediaTag.get<std::string>(1);
				mediaTagTexts[key] += value + "\n";
			}

			auto mediaTagPath = systemPath / "mediatag";
			fs::create_directories(mediaTagPath);
			for (const auto& mediaTag : mediaTagTexts)
			{
				auto fileTagFilePath = mediaTagPath / mediaTag.first;
				file::writeText(fileTagFilePath.string(), mediaTag.second);
			}
		}
	}
	return true;
}

void Romdb::verify()
{
	if (!db)
		return;

	for (const auto& system : query(*db, "SELECT id, name, code FROM system"))
	{
		auto systemId = system.get<long long>(0);
		auto systemName = system.get<std::string>(1);
		auto systemCode = system.get<std::string>(2);
		long long filesGood = 0;
		long long filesBad = 0;
		long long filesNoChecksum = 0;

		std::cout << systemCode << " - " << systemName << std::endl;

		query qry(*db, "SELECT id FROM media WHERE system_id = :system_id");
		qry.bind(":system_id", systemId);
		for (const auto& media : qry)
		{
			query qry2(*db, "SELECT id, name, data, LENGTH(data) FROM file WHERE media_id = :media_id");
			qry2.bind(":media_id", media.get<long long>(0));
			for (const auto& file : qry2)
			{
				bool hasChecksum = false;
				query qry3(
					*db, "SELECT LOWER(name), LOWER(data) FROM checksum WHERE file_id = :file_id ORDER BY name DESC");
				qry3.bind(":file_id", file.get<long long>(0));
				for (const auto& checksum : qry3)
				{
					hasChecksum = true;
					auto checksumName = checksum.get<std::string>(0);
					auto checksumHash = checksum.get<std::string>(1);
					auto fileData = file.get<void const*>(2);
					auto fileDataSize = file.get<long long>(3);
					auto filehash = file::hash::compute((const char*)fileData, (size_t)fileDataSize, checksumName);
					if (checksumHash == filehash)
						filesGood++;
					else
					{
						filesBad++;
						std::cout << "bad         : " << file.get<std::string>(1) << std::endl;
					}
					break;
				}
				if (!hasChecksum)
					filesNoChecksum++;
			}
		}
		std::cout << "total good  : " << filesGood << std::endl;
		std::cout << "total bad   : " << filesBad << std::endl;
		std::cout << "no checksum : " << filesNoChecksum << std::endl << std::endl;
	}
}


bool Romdb::createPatchFile(
	const std::string& importPath_, const std::string& patchFilePath_, const std::string& configName)
{
	fs::path importPath(importPath_);
	fs::path patchFilePath(patchFilePath_);

	if (!fs::exists(importPath) || !fs::is_directory(importPath))
		return false;

	if (fs::exists(patchFilePath) && !fs::is_directory(patchFilePath))
		return false;

	auto systemsFilePath = getImportFile(importPath, "systems", configName);
	if (fs::exists(systemsFilePath) && !fs::is_directory(systemsFilePath))
	{
		bool ret = false;
		auto systemsLines = utils::splitStringInLines(file::readText(systemsFilePath.string()));
		for (const auto& line : systemsLines)
		{
			if (line.empty())
				continue;

			auto systemImportPath = importPath / line;
			if (!fs::exists(systemImportPath) || !fs::is_directory(systemImportPath))
				continue;

			auto systemPatchFilePath = patchFilePath / line;
			fs::create_directories(systemPatchFilePath);
			systemPatchFilePath /= "patch.txt";

			ret |= createSystemPatchFile(systemImportPath, systemPatchFilePath.string(), configName);
		}
		return ret;
	}
	return createSystemPatchFile(importPath, patchFilePath.string(), configName);
}

bool Romdb::createSystemPatchFile(
	const fs::path& importPath, const std::string& patchFilePath, const std::string& configName)
{
	// load media
	std::vector<std::string> mediaLines;
	{
		auto mediaFilePath = getImportFile(importPath, "media", configName);
		if (!fs::exists(mediaFilePath) || fs::is_directory(mediaFilePath))
		{
			return false;
		}
		mediaLines = utils::splitStringInLines(file::readText(mediaFilePath.string()));
		if (mediaLines.empty())
		{
			return false;
		}
	}

	// load files
	utils::stringSetNoCase fileLinesSet;
	{
		auto filePath = getImportFile(importPath, "file", configName);
		if (!fs::exists(filePath) || fs::is_directory(filePath))
		{
			return false;
		}
		auto fileLines = utils::splitStringInLines(file::readText(filePath.string()));
		fileLinesSet = utils::stringSetNoCase(fileLines.begin(), fileLines.end());
	}

	// group files to media
	std::set<utils::stringSetNoCase> mediaFilesSet;
	for (const auto& media : utils::reverse(mediaLines))
	{
		if (media.empty())
			continue;

		auto filteredFiles = utils::filterStrings(fileLinesSet, media);
		mediaFilesSet.insert(filteredFiles);
	}

	// create patch.txt
	std::string patchText;
	for (const auto& mediaFiles : mediaFilesSet)
	{
		if (mediaFiles.size() <= 1)
			continue;

		if (!patchText.empty())
			patchText += "\n";

		for (const auto& file : mediaFiles)
		{
			patchText += file + "\n";
		}
	}
	file::writeText(patchFilePath, patchText);
	return true;
}
