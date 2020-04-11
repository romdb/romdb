#include "archive.h"
#include "7zip.h"

std::unique_ptr<Archive> Archive::openArchive(const char* fileBytes, size_t fileSize)
{
	if (!fileBytes || !fileSize)
		return nullptr;

	auto szArch = std::make_unique<SZIPArchive>();
	if (szArch->open(fileBytes, fileSize))
		return std::move(szArch);

	return nullptr;
}
