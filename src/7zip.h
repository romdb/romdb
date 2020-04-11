#pragma once

#include "archive.h"
#include <physfs_lzmasdk.h>

class SZIPArchive : public Archive
{
private:
	const char* fileBytes = nullptr;
	size_t fileSize = 0;
	size_t currentPos = 0;

	CSzArEx archive;
	std::vector<std::string> fileNames;

	void loadEntries();
	void loadEntry(UInt32 idx);

public:
	SZIPArchive();
	~SZIPArchive() override;

	bool open(const char* fileBytes_, size_t fileSize_) override;

	Int64 read(void* buffer, UInt64 len);
	Int64 tell();
	int seek(UInt64 pos);
	Int64 length();

	std::vector<char> getFile(const std::string& fileName) override;

	const std::vector<std::string>& getFileNames() const override { return fileNames; }
};
