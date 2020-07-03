//
//  GpuVideoReader.hpp
//  emptyExample
//
//  Created by Nakazi_w0w on 3/29/16.
//
//

#pragma once
#include <cstdlib>
#include <vector>
#include <memory>
#include "GpuVideo.h"
#include "GpuVideoIO.h"

class IGpuVideoReader 
{
public:
    virtual ~IGpuVideoReader() {}

    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual uint32_t getFrameCount() const = 0;
    virtual float getFramePerSecond() const = 0;
    virtual GPU_COMPRESS getFormat() const = 0;
    virtual uint32_t getFrameBytes() const = 0;

    virtual bool isThreadSafe() const = 0;

    // ì«Ç›çûÇ›
    virtual void read(uint8_t* dst, int frame) const = 0;
};


class GpuVideoReader : public IGpuVideoReader 
{
public:
    GpuVideoReader(const char* path, bool onMemory);
    ~GpuVideoReader();

    GpuVideoReader(const GpuVideoReader&) = delete;
    void operator=(const GpuVideoReader&) = delete;

    uint32_t getWidth() const { return _width; }
    uint32_t getHeight() const { return _height; }
    uint32_t getFrameCount() const { return frame_count_; }
    float getFramePerSecond() const { return _framePerSecond; }
    GPU_COMPRESS getFormat() const { return _format; }
    uint32_t getFrameBytes() const { return _frameBytes; }

    bool isThreadSafe() const { return _onMemory; }

    // ì«Ç›çûÇ›
    void read(uint8_t* dst, int frame) const;
private:
    bool _onMemory = false;

    uint32_t _width = 0;
    uint32_t _height = 0;
    uint32_t frame_count_ = 0;
    float _framePerSecond = 0;
    GPU_COMPRESS _format = GPU_COMPRESS_DXT1;
    uint32_t _frameBytes = 0;
    std::vector<Lz4Block> _lz4Blocks;

    std::unique_ptr<GpuVideoIO> _io;
    std::vector<uint8_t> _memory;
    mutable std::vector<uint8_t> _lz4Buffer;

    uint64_t _rawSize = 0;
};