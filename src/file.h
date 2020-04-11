#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace file
{
	namespace hash
	{
		std::string compute(const std::vector<char>& bytes, const std::string_view hashingAlgorithm);
		std::string compute(const char* data, size_t size, const std::string_view hashingAlgorithm);

		std::string crc32(const char* data, size_t size);
		std::string sha1(const char* data, size_t size);
		std::string sha256(const char* data, size_t size);
		std::string sha512(const char* data, size_t size);
	}

	void sort(const std::string& filePath);

	std::vector<char> readBytes(const std::string& filePath);

	std::string readText(const std::string& filePath);

	void writeBytes(const std::string& filePath, const char* data, size_t size);

	void writeText(const std::string& filePath, const std::string& str);

	// create a VCDIFF patch. outputFile -> inputFile + returned patch file
	// returns true + patch bytes or false + inputFile bytes
	bool createPatch(const std::string& inputFile, const std::string& outputFile, std::vector<char>& bytes);

	// apply a VCDIFF patch. inputFile + patchFile = outputFile
	std::vector<char> applyPatch(const std::string& inputFile, const std::string& patchFile);

	// apply a VCDIFF patch. input + patch = outputFile
	std::vector<char> applyPatch(
		const char* input, size_t inputSize, const char* patch, size_t patchSize, size_t originalSize);

	// compress a file
	bool compress(std::vector<char>& bytes, const std::string& algorithm);

	// compress a file using deflate
	bool compressDeflate(std::vector<char>& bytes);

	// compress a file using xz
	bool compressXz(std::vector<char>& bytes);

	// uncompress a file
	bool uncompress(std::vector<char>& bytes, size_t uncompressedSize, const std::string& algorithm);

	// uncompress a file using deflate
	bool uncompressDeflate(std::vector<char>& bytes, size_t uncompressedSize);

	// uncompress a file using xz
	bool uncompressXz(std::vector<char>& bytes, size_t uncompressedSize);
}
