/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#include "TOP_CPlusPlusBase.h"
#include "FrameQueue.h"
#include <thread>
#include <atomic>

#include <iostream>
#include <thread>
#include <cstdlib>
#include <vector>
#include <memory>
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include "lz4.h"

#ifndef _MSC_VER
#define _FILE_OFFSET_BITS 64
#else
#endif


////
enum GPU_COMPRESS : uint32_t {
	GPU_COMPRESS_DXT1 = 1,
	GPU_COMPRESS_DXT3 = 3,
	GPU_COMPRESS_DXT5 = 5,
	GPU_COMPRESS_BC7 = 7
};

static const uint32_t kRawMemoryAt = 24;

struct Lz4Block {
	uint64_t address = 0;
	uint64_t size = 0;
};


class IGpuVideoReader {
public:
	virtual ~IGpuVideoReader() {}

	virtual uint32_t getWidth() const = 0;
	virtual uint32_t getHeight() const = 0;
	virtual uint32_t getFrameCount() const = 0;
	virtual float getFramePerSecond() const = 0;
	virtual GPU_COMPRESS getFormat() const = 0;
	virtual uint32_t getFrameBytes() const = 0;

	virtual bool isThreadSafe() const = 0;

	virtual void read(uint8_t* dst, int frame) const = 0;
};




class GpuVideoIO {
public:
	GpuVideoIO(const char* filename, const char* mode) {
		_fp = fopen(filename, mode);
		if (_fp == nullptr) {
			throw std::runtime_error("file not found");
		}
	}
	~GpuVideoIO() {
		fclose(_fp);
	}
	GpuVideoIO(const GpuVideoIO&) = delete;
	void operator=(const GpuVideoIO&) = delete;

	int seek(int64_t offset, int origin) {
#ifdef _MSC_VER
		return _fseeki64(_fp, offset, origin);
#else
		return fseeko(_fp, offset, origin);
#endif
	}
	int64_t tellg() {
#ifdef _MSC_VER
		return _ftelli64(_fp);
#else
		return ftello(_fp);
#endif
	}
	std::size_t read(void* dst, std::size_t size) {
		return fread(dst, 1, size, _fp);
	}
	std::size_t write(const void* src, size_t size) {
		return fwrite(src, 1, size, _fp);
	}
private:
	FILE* _fp = nullptr;
};

class GpuVideoReader : public IGpuVideoReader {
public:
	GpuVideoReader(const char* path, bool onMemory) {
		_onMemory = onMemory;

		_io = std::unique_ptr<GpuVideoIO>(new GpuVideoIO(path, "rb"));

		std::cout << _io << std::endl;

		_io->seek(0, SEEK_END);
		_rawSize = _io->tellg();
		_io->seek(0, SEEK_SET);

#define R(v) if(_io->read(&v, sizeof(v)) != sizeof(v)) { assert(0); }
		R(_width);
		R(_height);
		R(_frameCount);
		R(_framePerSecond);
		R(_format);
		R(_frameBytes);
#undef R

		_lz4Blocks.resize(_frameCount);
		size_t s = sizeof(Lz4Block);

		_io->seek(_rawSize - sizeof(Lz4Block) * _frameCount, SEEK_SET);
		if (_io->read(_lz4Blocks.data(), sizeof(Lz4Block) * _frameCount) != sizeof(Lz4Block) * _frameCount) {
			assert(0);
		}

		if (_onMemory) {
			_memory.resize(_rawSize);
			_io->seek(0, SEEK_SET);
			if (_io->read(_memory.data(), _rawSize) != _rawSize) {
				assert(0);
			}
			_io.reset();
		}
		else {
			_io->seek(kRawMemoryAt, SEEK_SET);

			uint64_t buffer_size = 0;
			for (auto b : _lz4Blocks) {
				buffer_size = std::max(buffer_size, b.size);
			}

			_lz4Buffer.resize(buffer_size);
		}
	}
	~GpuVideoReader() {}

	GpuVideoReader(const GpuVideoReader&) = delete;
	void operator=(const GpuVideoReader&) = delete;

	uint32_t getWidth() const { return _width; }
	uint32_t getHeight() const { return _height; }
	uint32_t getFrameCount() const { return _frameCount; }
	float getFramePerSecond() const { return _framePerSecond; }
	GPU_COMPRESS getFormat() const { return _format; }
	uint32_t getFrameBytes() const { return _frameBytes; }

	bool isThreadSafe() const { return _onMemory; }

	void read(uint8_t* dst, int frame) const {
		assert(0 <= frame && frame < _lz4Blocks.size());
		Lz4Block lz4block = _lz4Blocks[frame];
		if (_onMemory) {
			LZ4_decompress_safe((const char*)_memory.data() + lz4block.address, (char*)dst, static_cast<int>(lz4block.size), _frameBytes);
		}
		else {
			_io->seek(lz4block.address, SEEK_SET);
			if (_io->read(_lz4Buffer.data(), lz4block.size) != lz4block.size) {
				assert(0);
			}
			LZ4_decompress_safe((const char*)_lz4Buffer.data(), (char*)dst, static_cast<int>(lz4block.size), _frameBytes);
		}
	}
private:
	bool _onMemory = false;

	uint32_t _width = 0;
	uint32_t _height = 0;
	uint32_t _frameCount = 0;
	float _framePerSecond = 0;
	GPU_COMPRESS _format = GPU_COMPRESS_DXT1;
	uint32_t _frameBytes = 0;
	std::vector<Lz4Block> _lz4Blocks;

	std::unique_ptr<GpuVideoIO> _io;
	std::vector<uint8_t> _memory;
	mutable std::vector<uint8_t> _lz4Buffer;

	uint64_t _rawSize = 0;
};

class ExGpuVideoTOP : public TOP_CPlusPlusBase
{
public:
    ExGpuVideoTOP(const OP_NodeInfo *info);
    virtual ~ExGpuVideoTOP();

    virtual void		getGeneralInfo(TOP_GeneralInfo *, const OP_Inputs*, void*) override;
    virtual bool		getOutputFormat(TOP_OutputFormat*, const OP_Inputs*, void*) override;


    virtual void		execute(TOP_OutputFormatSpecs*,
							const OP_Inputs*,
							TOP_Context* context,
							void* reserved1) override;

	static void fillBuffer(float * mem, int width, int height, double step, double brightness);


    virtual int32_t		getNumInfoCHOPChans(void *reserved1) override;
    virtual void		getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan *chan, void* reserved1) override;

    virtual bool		getInfoDATSize(OP_InfoDATSize *infoSize, void *reserved1) override;
    virtual void		getInfoDATEntries(int32_t index,
									int32_t nEntries,
									OP_InfoDATEntries *entries,
									void *reserved1) override;

	virtual void		setupParameters(OP_ParameterManager *manager, void *reserved1) override;
	virtual void		pulsePressed(const char *name, void *reserved1) override;

	void				waitForMoreWork();

private:

	void				startMoreWork();
    // We don't need to store this pointer, but we do for the example.
    // The OP_NodeInfo class store information about the node that's using
    // this instance of the class (like its name).
    const OP_NodeInfo*	myNodeInfo;

    // In this example this value will be incremented each time the execute()
    // function is called, then passes back to the TOP 
    int					myExecuteCount;

	std::mutex			mySettingsLock;
	double				myStep;
	double				mySpeed;
	double				myBrightness;

	// Used for threading example
	// Search for #define THREADING_EXAMPLE to enable that example
	FrameQueue			myFrameQueue;
	std::thread*		myThread;
	std::atomic<bool>	myThreadShouldExit;

	std::condition_variable	myCondition;
	std::mutex			myConditionLock;
	std::atomic<bool>	myStartWork;



};
