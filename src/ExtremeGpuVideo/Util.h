#pragma once

enum Mode 
{
	/* streaming from storage */
	GPU_VIDEO_STREAMING_FROM_STORAGE,

	/* on memory streaming */
	GPU_VIDEO_STREAMING_FROM_CPU_MEMORY,

	/* on memory streaming with pre decompressed */
	GPU_VIDEO_STREAMING_FROM_CPU_MEMORY_DECOMPRESSED,

	/* all gpu texture */
	GPU_VIDEO_ON_GPU_MEMORY
};

static const char* vertexShader = "#version 330\n\
layout(location = 0) in vec3 position; \
layout(location = 1) in vec2 texcoord; \
out vec2 v_texcoord; \
void main() { \
	v_texcoord = vec2(texcoord.x, 1.0 - texcoord.y); \
    gl_Position = vec4(position.xyz, 1); \
}";

static const char* fragmentShader = "#version 330\n\
uniform sampler2D u_src; \
in vec2 v_texcoord; \
out vec4 finalColor; \
void main() { \
    vec4 color = texture(u_src, v_texcoord); \
    finalColor = color; \
}";

static const char* uniformError = "A uniform location could not be found.";

