//
//  GpuVideoStreamingTexture.hpp
//  emptyExample
//
//  Created by Nakazi_w0w on 3/30/16.
//
//

#include <memory>
#include <array>
#ifdef _MSC_VER
#include <gl/glew.h>
#else
#include <OpenGL/gl.h>
#endif

#include "GpuVideoTexture.h"
#include "GpuVideoReader.h"

/**
 * CPU�������A�܂��̓X�g���[�W����̃X�g���[�~���O���s��
 */
class GpuVideoStreamingTexture : public IGpuVideoTexture {
public:
    GpuVideoStreamingTexture(std::shared_ptr<IGpuVideoReader> reader, GLenum interpolation = GL_LINEAR, GLenum wrap = GL_CLAMP_TO_EDGE);
    ~GpuVideoStreamingTexture();

    GpuVideoStreamingTexture(const GpuVideoStreamingTexture&) = delete;
    void operator=(const GpuVideoStreamingTexture&) = delete;

    void updateCPU(int frame);
    void uploadGPU();
    GLuint getTexture() const {
        return _textures[0];
    }
private:
    std::shared_ptr<IGpuVideoReader> _reader;

    GLuint _textures[2] = { 0, 0 };

    GLuint _glFmt = 0;
    int _curFrame = -1;

    bool _textureNeedsUpload = true;
    std::vector<uint8_t> _textureMemory;
};