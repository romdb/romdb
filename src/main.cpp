#include <clipp.h>
#include "file.h"
#include <iostream>
#include "romdb.h"
#include <string>

int main(int argc, char* argv[])
{
	std::string dbPath;
	std::string schemaPath;
	std::string romsPath;
	std::string importPath;
	std::string patchFilePath;
	std::string configName;
	std::string sortFile;
	bool dump = false;
	bool fullDump = false;
	bool verify = false;
	bool help = false;

	auto cli = (clipp::option("-o", "--output") & clipp::value("romdb file", dbPath),
		clipp::option("-s", "--schema") & clipp::value("romdb schema file", schemaPath),
		clipp::option("-r", "--roms") & clipp::value("roms path/dump path", romsPath),
		clipp::option("-i", "--import") & clipp::value("import system(s) files path", importPath),
		clipp::option("-p", "--patch") & clipp::value("create patch.txt from import path", patchFilePath),
		clipp::option("-c", "--configuration") & clipp::value("import configuration name", configName),
		clipp::option("-d", "--dump").set(dump).doc("dump roms"),
		clipp::option("-f", "--full-dump").set(fullDump).doc("dump roms and metadata"),
		clipp::option("-v", "--verify").set(verify).doc("verify romdb integrity"),
		clipp::option("--sort") & clipp::value("natural sort text file", sortFile),
		clipp::option("-h", "--help").set(help).doc("help"));

	if (!parse(argc, argv, cli) || help)
	{
		std::cout << make_man_page(cli, argv[0]);
		return 0;
	}

	try
	{
		if (!sortFile.empty())
		{
			file::sort(sortFile);
		}
		else if (patchFilePath.empty())
		{
			if (!importPath.empty())
			{
				Romdb db;
				if (!db.openOrCreate(dbPath, schemaPath))
				{
					std::cerr << "invalid romdb database";
					return 1;
				}
				if (!romsPath.empty())
					db.import(romsPath, importPath, configName);
				else
					db.import(importPath, configName);
			}
			else
			{
				Romdb db;
				if (!db.open(dbPath))
				{
					std::cerr << "invalid romdb database";
					return 1;
				}
				if (dump)
				{
					db.dump(romsPath, fullDump);
					return 0;
				}
				if (verify)
				{
					db.verify();
					return 0;
				}
			}
		}
		else
		{
			if (!importPath.empty())
			{
				Romdb::createPatchFile(importPath, patchFilePath, configName);
			}
		}
	}
	catch (std::exception ex)
	{
		std::cerr << ex.what();
		return 1;
	}
	return 0;
}
