#include "file.h"
#include <crc_32.h>
#include <fstream>
#include <lzma.h>
#include <sha1.h>
#include <sha2_256.h>
#include <sha2_512.h>
#include "utils.h"
#include <xdelta3.h>
#include <zlib.h>

std::string file::hash::compute(const std::vector<char>& bytes, const std::string_view hashingAlgorithm)
{
	return compute(bytes.data(), bytes.size(), hashingAlgorithm);
}

std::string file::hash::compute(const char* data, size_t size, const std::string_view hashingAlgorithm)
{
	if (hashingAlgorithm == "crc32")
		return crc32(data, size);
	else if (hashingAlgorithm == "sha1")
		return sha1(data, size);
	else if (hashingAlgorithm == "sha256")
		return sha256(data, size);
	else if (hashingAlgorithm == "sha512")
		return sha512(data, size);
	return {};
}

std::string file::hash::crc32(const char* data, size_t size)
{
	Chocobo1::CRC_32 hashFunc;
	hashFunc.addData(data, size);
	hashFunc.finalize();
	return hashFunc.toString();
}

std::string file::hash::sha1(const char* data, size_t size)
{
	Chocobo1::SHA1 hashFunc;
	hashFunc.addData(data, size);
	hashFunc.finalize();
	return hashFunc.toString();
}

std::string file::hash::sha256(const char* data, size_t size)
{
	Chocobo1::SHA2_256 hashFunc;
	hashFunc.addData(data, size);
	hashFunc.finalize();
	return hashFunc.toString();
}

std::string file::hash::sha512(const char* data, size_t size)
{
	Chocobo1::SHA2_512 hashFunc;
	hashFunc.addData(data, size);
	hashFunc.finalize();
	return hashFunc.toString();
}

void file::sort(const std::string& filePath)
{
	auto fileLines = utils::splitStringInLines(file::readText(filePath));
	if (fileLines.empty())
	{
		return;
	}
	std::sort(fileLines.begin(), fileLines.end(), utils::compareCaseInsensitive());
	std::string sortedText;
	size_t numBlanks = 0;
	for (const auto& line : fileLines)
	{
		if (line.empty())
		{
			numBlanks++;
			continue;
		}
		sortedText += line + "\n";
	}
	for (size_t i = 1; i < numBlanks; i++)
		sortedText += "\n";
	writeBytes(filePath, sortedText.data(), sortedText.size());
}

std::vector<char> file::readBytes(const std::string& filePath)
{
	std::ifstream ifs(filePath.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

	auto fileSize = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::vector<char> bytes((size_t)fileSize);
	ifs.read(bytes.data(), fileSize);

	return bytes;
}

std::string file::readText(const std::string& filePath)
{
	auto bytes = readBytes(filePath);
	return std::string(bytes.data(), bytes.size());
}

void file::writeBytes(const std::string& filePath, const char* data, size_t size)
{
	std::ofstream fileStream(filePath.c_str(), std::ios::binary);
	fileStream.write(data, size);
}

void file::writeText(const std::string& filePath, const std::string& str)
{
	std::ofstream fileStream(filePath, std::ios_base::binary | std::ios_base::out);
	fileStream << str;
}

bool file::createPatch(const std::string& inputFile, const std::string& outputFile, std::vector<char>& bytes)
{
	std::vector<char> inputBytes = file::readBytes(inputFile);
	std::vector<char> outputBytes = file::readBytes(outputFile);

	std::vector<char> diffBytes(xd3_max(inputBytes.size(), outputBytes.size()) * 4);
	usize_t diffSize = 0;

	auto ret =
		xd3_encode_memory((const uint8_t*)outputBytes.data(), outputBytes.size(), (const uint8_t*)inputBytes.data(),
			inputBytes.size(), (uint8_t*)diffBytes.data(), &diffSize, diffBytes.size(), 0);
	if (ret == 0)
	{
		diffBytes.resize((size_t)diffSize);
		bytes = diffBytes;
		return true;
	}
	else
	{
		bytes = inputBytes;
		return false;
	}
}

std::vector<char> file::applyPatch(const std::string& inputFile, const std::string& patchFile)
{
	std::vector<char> inputBytes = file::readBytes(inputFile);
	std::vector<char> patchBytes = file::readBytes(patchFile);
	return applyPatch(inputBytes.data(), inputBytes.size(), patchBytes.data(), patchBytes.size(), 0);
}

std::vector<char> file::applyPatch(
	const char* input, size_t inputSize, const char* patch, size_t patchSize, size_t originalSize)
{
	std::vector<char> diffBytes(originalSize ? originalSize : inputSize + patchSize);
	usize_t diffSize = 0;
	int iteration = 0;
	while (iteration <= 8)
	{
		auto ret = xd3_decode_memory((const uint8_t*)patch, patchSize, (const uint8_t*)input, inputSize,
			(uint8_t*)diffBytes.data(), &diffSize, diffBytes.size(), 0);

		if (ret == 0)
		{
			diffBytes.resize((size_t)diffSize);
			return diffBytes;
		}
		else if (ret == ENOSPC)
		{
			// increase buffer size and try again
			diffBytes.resize(diffBytes.size() * 4);
			iteration++;
			continue;
		}
		break;
	}
	diffBytes.clear();
	return diffBytes;
}

static lzma_ret lzma_compress2(uint8_t* dest, size_t* destLen, const uint8_t* source, size_t sourceLen, uint32_t level)
{
	lzma_stream stream = LZMA_STREAM_INIT;
	lzma_ret err;
	const size_t max = (size_t)-1;
	size_t left;

	left = *destLen;
	*destLen = 0;

	err = lzma_easy_encoder(&stream, level, LZMA_CHECK_NONE);
	if (err != LZMA_OK)
		return err;

	stream.next_out = dest;
	stream.avail_out = 0;
	stream.next_in = (z_const Bytef*)source;
	stream.avail_in = 0;

	do
	{
		if (stream.avail_out == 0)
		{
			stream.avail_out = left > (size_t)max ? max : (uInt)left;
			left -= stream.avail_out;
		}
		if (stream.avail_in == 0)
		{
			stream.avail_in = sourceLen > (size_t)max ? max : (uInt)sourceLen;
			sourceLen -= stream.avail_in;
		}
		err = lzma_code(&stream, sourceLen ? LZMA_RUN : LZMA_FINISH);
	} while (err == LZMA_OK);

	*destLen = (size_t)stream.total_out;
	lzma_end(&stream);
	return err == LZMA_STREAM_END ? LZMA_OK : err;
}

static lzma_ret lzma_uncompress2(uint8_t* dest, size_t* destLen, const uint8_t* source, size_t* sourceLen)
{
	lzma_stream stream = LZMA_STREAM_INIT;
	lzma_ret err;
	const size_t max = (size_t)-1;
	size_t len, left;
	uint8_t buf[1]; /* for detection of incomplete stream when *destLen == 0 */

	len = *sourceLen;
	if (*destLen)
	{
		left = *destLen;
		*destLen = 0;
	}
	else
	{
		left = 1;
		dest = buf;
	}

	stream.next_in = source;
	stream.avail_in = 0;

	err = lzma_stream_decoder(&stream, UINT32_MAX, 0);
	if (err != LZMA_OK)
		return err;

	stream.next_out = dest;
	stream.avail_out = 0;

	do
	{
		if (stream.avail_out == 0)
		{
			stream.avail_out = left > max ? max : left;
			left -= stream.avail_out;
		}
		if (stream.avail_in == 0)
		{
			stream.avail_in = len > max ? max : len;
			len -= stream.avail_in;
		}
		err = lzma_code(&stream, LZMA_RUN);
	} while (err == LZMA_OK);

	*sourceLen -= len + stream.avail_in;
	if (dest != buf)
		*destLen = (size_t)stream.total_out;
	else if (stream.total_out && err == LZMA_BUF_ERROR)
		left = 1;

	lzma_end(&stream);
	return err == LZMA_STREAM_END ? LZMA_OK : err == LZMA_BUF_ERROR && left + stream.avail_out ? LZMA_DATA_ERROR : err;
}

static int zlib_compress2(Bytef* dest, uLongf* destLen, const Bytef* source, uLong sourceLen, int level)
{
	z_stream stream;
	int err;
	const uInt max = (uInt)-1;
	uLong left;

	left = *destLen;
	*destLen = 0;

	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;

	err = deflateInit(&stream, level);
	if (err != Z_OK)
		return err;

	stream.next_out = dest;
	stream.avail_out = 0;
	stream.next_in = (z_const Bytef*)source;
	stream.avail_in = 0;

	do
	{
		if (stream.avail_out == 0)
		{
			stream.avail_out = left > (uLong)max ? max : (uInt)left;
			left -= stream.avail_out;
		}
		if (stream.avail_in == 0)
		{
			stream.avail_in = sourceLen > (uLong)max ? max : (uInt)sourceLen;
			sourceLen -= stream.avail_in;
		}
		err = deflate(&stream, sourceLen ? Z_NO_FLUSH : Z_FINISH);
	} while (err == Z_OK);

	*destLen = stream.total_out;
	deflateEnd(&stream);
	return err == Z_STREAM_END ? Z_OK : err;
}

static int zlib_uncompress2(Bytef* dest, uLongf* destLen, const Bytef* source, uLong* sourceLen)
{
	z_stream stream;
	int err;
	const uInt max = (uInt)-1;
	uLong len, left;
	Byte buf[1]; /* for detection of incomplete stream when *destLen == 0 */

	len = *sourceLen;
	if (*destLen)
	{
		left = *destLen;
		*destLen = 0;
	}
	else
	{
		left = 1;
		dest = buf;
	}

	stream.next_in = (z_const Bytef*)source;
	stream.avail_in = 0;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;

	err = inflateInit(&stream);
	if (err != Z_OK)
		return err;

	stream.next_out = dest;
	stream.avail_out = 0;

	do
	{
		if (stream.avail_out == 0)
		{
			stream.avail_out = left > (uLong)max ? max : (uInt)left;
			left -= stream.avail_out;
		}
		if (stream.avail_in == 0)
		{
			stream.avail_in = len > (uLong)max ? max : (uInt)len;
			len -= stream.avail_in;
		}
		err = inflate(&stream, Z_NO_FLUSH);
	} while (err == Z_OK);

	*sourceLen -= len + stream.avail_in;
	if (dest != buf)
		*destLen = stream.total_out;
	else if (stream.total_out && err == Z_BUF_ERROR)
		left = 1;

	inflateEnd(&stream);
	return err == Z_STREAM_END
			   ? Z_OK
			   : err == Z_NEED_DICT ? Z_DATA_ERROR : err == Z_BUF_ERROR && left + stream.avail_out ? Z_DATA_ERROR : err;
}

bool file::compress(std::vector<char>& bytes, const std::string& algorithm)
{
	if (!bytes.empty() && !algorithm.empty())
	{
		if (algorithm == "deflate")
			return file::compressDeflate(bytes);
		else if (algorithm == "xz")
			return file::compressXz(bytes);
	}
	return false;
}

bool file::compressDeflate(std::vector<char>& bytes)
{
	std::vector<char> compressedBytes(bytes.size());
	uLongf compressedBytesSize = bytes.size();
	auto ret = compress2(
		(Bytef*)compressedBytes.data(), &compressedBytesSize, (const Bytef*)bytes.data(), (uLongf)bytes.size(), 9);
	if (ret == Z_OK)
	{
		compressedBytes.resize(compressedBytesSize);
		bytes = compressedBytes;
		return true;
	}
	return false;
}

bool file::compressXz(std::vector<char>& bytes)
{
	std::vector<char> compressedBytes(bytes.size());
	size_t compressedBytesSize = bytes.size();
	auto ret = lzma_compress2(
		(uint8_t*)compressedBytes.data(), &compressedBytesSize, (const uint8_t*)bytes.data(), bytes.size(), 9);
	if (ret == LZMA_OK)
	{
		compressedBytes.resize(compressedBytesSize);
		bytes = compressedBytes;
		return true;
	}
	return false;
}

bool file::uncompress(std::vector<char>& bytes, size_t uncompressedSize, const std::string& algorithm)
{
	if (!bytes.empty() && !algorithm.empty())
	{
		if (algorithm == "deflate")
			return file::uncompressDeflate(bytes, uncompressedSize);
		else if (algorithm == "xz")
			return file::uncompressXz(bytes, uncompressedSize);
	}
	return false;
}

bool file::uncompressDeflate(std::vector<char>& bytes, size_t uncompressedSize)
{
	if (uncompressedSize == 0)
		uncompressedSize = bytes.size() * 2;
	else if (uncompressedSize >= 0x40000000) // limit initial size to 1GB
		uncompressedSize = 0x40000000;

	std::vector<char> uncompressedBytes(uncompressedSize);
	uLongf bytesSize = bytes.size();
	uLongf uncompressedBytesSize = uncompressedBytes.size();
	while (true)
	{
		auto ret =
			uncompress2((Bytef*)uncompressedBytes.data(), &uncompressedBytesSize, (Bytef*)bytes.data(), &bytesSize);
		if (ret == Z_OK)
		{
			uncompressedBytes.resize(uncompressedBytesSize);
			bytes = uncompressedBytes;
			return true;
		}
		else if (ret == Z_BUF_ERROR)
		{
			bytesSize = bytes.size();
			uncompressedBytes.resize(uncompressedBytes.size() * 2);
			uncompressedBytesSize = uncompressedBytes.size();
			continue;
		}
		break;
	}
	return false;
}

bool file::uncompressXz(std::vector<char>& bytes, size_t uncompressedSize)
{
	if (uncompressedSize == 0)
		uncompressedSize = bytes.size() * 2;
	else if (uncompressedSize >= 0x40000000) // limit initial size to 1GB
		uncompressedSize = 0x40000000;

	std::vector<char> uncompressedBytes(uncompressedSize);
	size_t bytesSize = bytes.size();
	size_t uncompressedBytesSize = uncompressedBytes.size();
	while (true)
	{
		auto ret = lzma_uncompress2(
			(uint8_t*)uncompressedBytes.data(), &uncompressedBytesSize, (const uint8_t*)bytes.data(), &bytesSize);
		if (ret == LZMA_OK)
		{
			uncompressedBytes.resize(uncompressedBytesSize);
			bytes = uncompressedBytes;
			return true;
		}
		else if (ret == LZMA_BUF_ERROR)
		{
			bytesSize = bytes.size();
			uncompressedBytes.resize(uncompressedBytes.size() * 2);
			uncompressedBytesSize = uncompressedBytes.size();
			continue;
		}
		break;
	}
	return false;
}
