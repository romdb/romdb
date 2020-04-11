#pragma once

#include <memory>
#include <string>
#include <vector>

class Archive
{
private:
	Archive(const Archive& rhs) = delete;
	Archive& operator=(const Archive& rhs) = delete;

public:
	static std::unique_ptr<Archive> openArchive(const char* fileBytes, size_t fileSize);

	Archive() = default;
	virtual ~Archive() = default;

	virtual bool open(const char* fileBytes, size_t fileSize) = 0;
	virtual std::vector<char> getFile(const std::string& fileName) = 0;
	virtual const std::vector<std::string>& getFileNames() const = 0;
};
