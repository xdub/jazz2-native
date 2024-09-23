#include "AndroidAssetStream.h"

#if defined(DEATH_TARGET_ANDROID)

#include <sys/stat.h>		// for open()
#include <fcntl.h>			// for open()
#include <unistd.h>			// for close()

namespace Death { namespace IO {
//###==##====#=====--==~--~=~- --- -- -  -  -   -

	AAssetManager* AndroidAssetStream::_assetManager = nullptr;
	const char* AndroidAssetStream::_internalDataPath = nullptr;

	AndroidAssetStream::AndroidAssetStream(const Containers::StringView path, FileAccess mode)
		: AndroidAssetStream(Containers::String{path}, mode)
	{
	}

	AndroidAssetStream::AndroidAssetStream(Containers::String&& path, FileAccess mode)
		: _path(std::move(path)), _size(Stream::Invalid),
#if defined(DEATH_USE_FILE_DESCRIPTORS)
			_fileDescriptor(-1), _startOffset(0L)
#else
			_asset(nullptr)
#endif
	{
		Open(mode);
	}

	AndroidAssetStream::~AndroidAssetStream()
	{
		AndroidAssetStream::Dispose();
	}

	/*! This method will close a file both normally opened or fopened */
	void AndroidAssetStream::Dispose()
	{
#if defined(DEATH_USE_FILE_DESCRIPTORS)
		if (_fileDescriptor >= 0) {
			const std::int32_t retValue = ::close(_fileDescriptor);
			if (retValue < 0) {
				LOGW("Cannot close the file \"%s\"", _path.data());
			} else {
				LOGI("File \"%s\" closed", _path.data());
				_fileDescriptor = -1;
			}
		}
#else
		if (_asset != nullptr) {
			AAsset_close(_asset);
			_asset = nullptr;
			LOGI("File \"%s\" closed", _path.data());
		}
#endif
	}

	std::int64_t AndroidAssetStream::Seek(std::int64_t offset, SeekOrigin origin)
	{
		std::int64_t newPos = Stream::Invalid;
#if defined(DEATH_USE_FILE_DESCRIPTORS)
		if (_fileDescriptor >= 0) {
			switch (origin) {
				case SeekOrigin::Begin: newPos = ::lseek(_fileDescriptor, _startOffset + offset, SEEK_SET); break;
				case SeekOrigin::Current: newPos = ::lseek(_fileDescriptor, offset, SEEK_CUR); break;
				case SeekOrigin::End: newPos = ::lseek(_fileDescriptor, _startOffset + _size + offset, SEEK_END); break;
				default: return Stream::OutOfRange;
			}
			if (newPos >= _startOffset) {
				newPos -= _startOffset;
			} else if (newPos < 0) {
				newPos = Stream::OutOfRange;
			}
		}
#else
		if (_asset != nullptr) {
			newPos = AAsset_seek64(_asset, offset, static_cast<std::int32_t>(origin));
			if (newPos < 0) {
				newPos = Stream::OutOfRange;
			}
		}
#endif
		return newPos;
	}

	std::int64_t AndroidAssetStream::GetPosition() const
	{
		std::int64_t pos = Stream::Invalid;
#if defined(DEATH_USE_FILE_DESCRIPTORS)
		if (_fileDescriptor >= 0) {
			pos = ::lseek(_fileDescriptor, 0L, SEEK_CUR) - _startOffset;
		}
#else
		if (_asset != nullptr) {
			pos = AAsset_seek64(_asset, 0L, SEEK_CUR);
		}
#endif
		return pos;
	}

	std::int32_t AndroidAssetStream::Read(void* buffer, std::int32_t bytes)
	{
		DEATH_ASSERT(buffer != nullptr, "buffer is null", 0);

		if (bytes <= 0) {
			return 0;
		}

		std::int32_t bytesRead = 0;
#if defined(DEATH_USE_FILE_DESCRIPTORS)
		if (_fileDescriptor >= 0) {
			std::int32_t bytesToRead = bytes;
			const std::int64_t pos = ::lseek(_fileDescriptor, 0L, SEEK_CUR);
			if (pos >= _startOffset + _size) {
				bytesToRead = 0; // Simulating EOF
			} else if (pos + bytes > _startOffset + _size) {
				bytesToRead = (_startOffset + _size) - pos;
			}
			bytesRead = ::read(_fileDescriptor, buffer, bytesToRead);
		}
#else
		if (_asset != nullptr) {
			bytesRead = AAsset_read(_asset, buffer, bytes);
		}
#endif
		return bytesRead;
	}

	std::int32_t AndroidAssetStream::Write(const void* buffer, std::int32_t bytes)
	{
		// Not supported
		return Stream::Invalid;
	}

	bool AndroidAssetStream::Flush()
	{
		// Not supported
		return true;
	}

	bool AndroidAssetStream::IsValid()
	{
#if defined(DEATH_USE_FILE_DESCRIPTORS)
		return (_fileDescriptor >= 0);
#else
		return (_asset != nullptr);
#endif
	}

	std::int64_t AndroidAssetStream::GetSize() const
	{
		return _size;
	}

	Containers::StringView AndroidAssetStream::GetPath() const
	{
		return _path;
	}

	void AndroidAssetStream::InitializeAssetManager(struct android_app* state)
	{
		_assetManager = state->activity->assetManager;
		_internalDataPath = state->activity->internalDataPath;
	}

	const char* AndroidAssetStream::TryGetAssetPath(const char* path)
	{
		DEATH_ASSERT(path != nullptr, "path is null", nullptr);
		if (strncmp(path, Prefix.data(), Prefix.size()) == 0) {
			// Skip leading path separator character
			return (path[7] == '/' || path[7] == '\\' ? path + 8 : path + 7);
		}
		return nullptr;
	}

	bool AndroidAssetStream::TryOpen(const char* path)
	{
		DEATH_ASSERT(path != nullptr, "path is null", false);
		return (TryOpenFile(path) || TryOpenDirectory(path));
	}

	bool AndroidAssetStream::TryOpenFile(const char* path)
	{
		DEATH_ASSERT(path != nullptr, "path is null", false);
		const char* strippedPath = TryGetAssetPath(path);
		if (strippedPath == nullptr) {
			return false;
		}

		AAsset* asset = AAssetManager_open(_assetManager, strippedPath, AASSET_MODE_UNKNOWN);
		if (asset != nullptr) {
			AAsset_close(asset);
			return true;
		}

		return false;
	}

	bool AndroidAssetStream::TryOpenDirectory(const char* path)
	{
		DEATH_ASSERT(path != nullptr, "path is null", false);
		const char* strippedPath = TryGetAssetPath(path);
		if (strippedPath == nullptr) {
			return false;
		}

		AAsset* asset = AAssetManager_open(_assetManager, strippedPath, AASSET_MODE_UNKNOWN);
		if (asset != nullptr) {
			AAsset_close(asset);
			return false;
		}

		AAssetDir* assetDir = AAssetManager_openDir(_assetManager, strippedPath);
		if (assetDir != nullptr) {
			AAssetDir_close(assetDir);
			return true;
		}

		return false;
	}

	std::int64_t AndroidAssetStream::GetFileSize(const char* path)
	{
		DEATH_ASSERT(path != nullptr, "path is null", 0);

		off64_t assetLength = 0;
		const char* strippedPath = TryGetAssetPath(path);
		if (strippedPath == nullptr) {
			return assetLength;
		}

		AAsset* asset = AAssetManager_open(_assetManager, strippedPath, AASSET_MODE_UNKNOWN);
		if (asset != nullptr) {
			assetLength = AAsset_getLength64(asset);
			AAsset_close(asset);
		}

		return assetLength;
	}

	AAssetDir* AndroidAssetStream::OpenDirectory(const char* dirName)
	{
		DEATH_ASSERT(dirName != nullptr, "dirName is null", nullptr);
		return AAssetManager_openDir(_assetManager, dirName);
	}

	void AndroidAssetStream::CloseDirectory(AAssetDir* assetDir)
	{
		AAssetDir_close(assetDir);
	}

	void AndroidAssetStream::RewindDirectory(AAssetDir* assetDir)
	{
		AAssetDir_rewind(assetDir);
	}

	const char* AndroidAssetStream::GetNextFileName(AAssetDir* assetDir)
	{
		return AAssetDir_getNextFileName(assetDir);
	}

	void AndroidAssetStream::Open(FileAccess mode)
	{
		FileAccess maskedMode = mode & ~FileAccess::Exclusive;
		if (maskedMode != FileAccess::Read) {
			LOGE("Cannot open file \"%s\" - wrong open mode", _path.data());
			return;
		}

#if defined(DEATH_USE_FILE_DESCRIPTORS)
		// An asset file can only be read
		AAsset* asset = AAssetManager_open(_assetManager, _path.data(), AASSET_MODE_RANDOM);
		if (asset == nullptr) {
			LOGE("Cannot open file \"%s\"", _path.data());
			return;
		}

		off64_t outStart = 0;
		off64_t outLength = 0;
		_fileDescriptor = AAsset_openFileDescriptor64(asset, &outStart, &outLength);
		_startOffset = outStart;
		_size = outLength;

		::lseek(_fileDescriptor, _startOffset, SEEK_SET);
		AAsset_close(asset);
		asset = nullptr;

		if (_fileDescriptor < 0) {
			LOGE("Cannot open file \"%s\"", _path.data());
			return;
		}

		LOGI("File \"%s\" opened", _path.data());
#else
		// An asset file can only be read
		_asset = AAssetManager_open(_assetManager, _path.data(), AASSET_MODE_RANDOM);
		if (_asset == nullptr) {
			LOGE("Cannot open file \"%s\"", _path.data());
			return;
		}

		LOGI("File \"%s\" opened", _path.data());

		// Calculating file size
		_size = AAsset_getLength64(_asset);
#endif
	}

}}

#endif