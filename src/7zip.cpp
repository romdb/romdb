#include "7zip.h"
#include <cstring>
#include <mutex>
#include "utils.h"

struct SZIPLookToRead
{
	ISeekInStream seekStream;
	SZIPArchive* archive = nullptr;
	CLookToRead lookStream;
};

static void* SZIP_ISzAlloc_Alloc(void* p, size_t size) { return malloc(size ? size : 1); }

static void SZIP_ISzAlloc_Free(void* p, void* address)
{
	if (address)
		free(address);
}

static ISzAlloc SZIP_SzAlloc = { SZIP_ISzAlloc_Alloc, SZIP_ISzAlloc_Free };

static SRes SZIP_ISeekInStream_Read(void* p, void* buf, size_t* size)
{
	SZIPLookToRead* stream = (SZIPLookToRead*)p;
	auto archive = stream->archive;
	auto len = (UInt64)*size;
	auto ret = (len == 0) ? 0 : archive->read(buf, len);

	if (ret < 0)
	{
		*size = 0;
		return SZ_ERROR_READ;
	}

	*size = (size_t)ret;
	return SZ_OK;
}

static SRes SZIP_ISeekInStream_Seek(void* p, Int64* pos, ESzSeek origin)
{
	SZIPLookToRead* stream = (SZIPLookToRead*)p;
	auto archive = stream->archive;
	Int64 base;
	UInt64 newpos;

	switch (origin)
	{
	case SZ_SEEK_SET:
		base = 0;
		break;
	case SZ_SEEK_CUR:
		base = archive->tell();
		break;
	case SZ_SEEK_END:
		base = archive->length();
		break;
	default:
		return SZ_ERROR_FAIL;
	}

	if (base < 0)
		return SZ_ERROR_FAIL;
	else if ((*pos < 0) && (((Int64)base) < -*pos))
		return SZ_ERROR_FAIL;

	newpos = (UInt64)(((Int64)base) + *pos);
	if (!archive->seek(newpos))
		return SZ_ERROR_FAIL;

	*pos = (Int64)newpos;
	return SZ_OK;
}

static void szipInitStream(SZIPLookToRead* stream, SZIPArchive* archive)
{
	stream->seekStream.Read = SZIP_ISeekInStream_Read;
	stream->seekStream.Seek = SZIP_ISeekInStream_Seek;

	stream->archive = archive;

	LookToRead_Init(&stream->lookStream);
	LookToRead_CreateVTable(&stream->lookStream, False);
	stream->lookStream.realStream = &stream->seekStream;
}

SZIPArchive::SZIPArchive()
{
	std::once_flag onceFlag;
	{
		std::call_once(onceFlag, [] { CrcGenerateTable(); });
	}
	SzArEx_Init(&archive);
}

SZIPArchive::~SZIPArchive() { SzArEx_Free(&archive, &SZIP_SzAlloc); }

bool SZIPArchive::open(const char* fileBytes_, size_t fileSize_)
{
	fileBytes = fileBytes_;
	fileSize = fileSize_;

	auto stream = std::make_unique<SZIPLookToRead>();
	szipInitStream(stream.get(), this);
	auto ret = SzArEx_Open(&archive, &stream->lookStream.s, &SZIP_SzAlloc, &SZIP_SzAlloc);
	if (ret != SZ_OK)
	{
		SzArEx_Free(&archive, &SZIP_SzAlloc);
		return false;
	}
	loadEntries();
	return true;
}

void SZIPArchive::loadEntries()
{
	for (UInt32 i = 0; i < archive.NumFiles; i++)
	{
		loadEntry(i);
	}
}

void SZIPArchive::loadEntry(UInt32 idx)
{
	auto strLength = SzArEx_GetFileNameUtf16(&archive, idx, nullptr);
	if (strLength > 0)
		strLength--;
	std::wstring fileName(strLength, L'\0');
	SzArEx_GetFileNameUtf16(&archive, idx, (UInt16*)fileName.data());
	fileNames.push_back(utils::wstr2str(fileName));
}

Int64 SZIPArchive::read(void* buffer, UInt64 len)
{
	if (!fileBytes)
		return -1;

	auto bytesLeft = (UInt64)(fileSize - currentPos);
	if (bytesLeft < len)
		len = bytesLeft;

	std::memcpy(buffer, fileBytes + currentPos, (size_t)len);
	currentPos += (size_t)len;
	return len;
}

Int64 SZIPArchive::tell()
{
	if (!fileBytes)
		return -1;
	return (Int64)currentPos;
}

int SZIPArchive::seek(UInt64 pos)
{
	if (!fileBytes)
		return 0;
	if (pos < fileSize)
		currentPos = (size_t)pos;
	return 1;
}

Int64 SZIPArchive::length()
{
	if (!fileBytes)
		return -1;
	return (Int64)fileSize;
}

std::vector<char> SZIPArchive::getFile(const std::string& fileName)
{
	auto it = std::find(fileNames.begin(), fileNames.end(), fileName);
	if (it == fileNames.end())
		return {};

	size_t idx = it - fileNames.begin();
	if (SzArEx_IsDir(&archive, idx) != 0)
		return {};

	auto stream = std::make_unique<SZIPLookToRead>();
	UInt32 blockIndex = 0xFFFFFFFF;
	Byte* outBuffer = nullptr;
	size_t outBufferSize = 0;
	size_t offset = 0;
	size_t outSizeProcessed = 0;

	szipInitStream(stream.get(), this);

	auto ret = SzArEx_Extract(&archive, &stream->lookStream.s, idx, &blockIndex, &outBuffer, &outBufferSize, &offset,
		&outSizeProcessed, &SZIP_SzAlloc, &SZIP_SzAlloc);
	if (ret != SZ_OK)
	{
		if (outBuffer)
			free(outBuffer);
		return {};
	}
	std::vector<char> fileBytes((char*)outBuffer + offset, (char*)outBuffer + offset + outSizeProcessed);
	if (outBuffer)
		free(outBuffer);
	return fileBytes;
}
