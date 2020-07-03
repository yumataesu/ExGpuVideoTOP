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

#include "ExGpuVideoTOP.h"

#include <assert.h>
#include <cstdio>

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{
	DLLEXPORT
		void
		FillTOPPluginInfo(TOP_PluginInfo* info)
	{
		// This must always be set to this constant
		info->apiVersion = TOPCPlusPlusAPIVersion;

		// Change this to change the executeMode behavior of this plugin.
		info->executeMode = TOP_ExecuteMode::OpenGL_FBO;

		// The opType is the unique name for this TOP. It must start with a 
		// capital A-Z character, and all the following characters must lower case
		// or numbers (a-z, 0-9)
		info->customOPInfo.opType->setString("ExGpuVideoPlayer");

		// The opLabel is the text that will show up in the OP Create Dialog
		info->customOPInfo.opLabel->setString("ExGpu VideoPlayer");

		// Will be turned into a 3 letter icon on the nodes
		info->customOPInfo.opIcon->setString("EGV");

		// Information about the author of this OP
		info->customOPInfo.authorName->setString("Yuma Taesu");
		info->customOPInfo.authorEmail->setString("yuma.taesu@gmail.com");

		// This TOP works with 0 inputs
		info->customOPInfo.minInputs = 0;
		info->customOPInfo.maxInputs = 0;

	}

	DLLEXPORT
		TOP_CPlusPlusBase*
		CreateTOPInstance(const OP_NodeInfo* info, TOP_Context* context)
	{
		// Return a new instance of your class every time this is called.
		// It will be called once per TOP that is using the .dll

		// Note we can't do any OpenGL work during instantiation

		return new ExGpuVideoTOP(info, context);
	}

	DLLEXPORT
		void
		DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context* context)
	{
		// Delete the instance here, this will be called when
		// Touch is shutting down, when the TOP using that instance is deleted, or
		// if the TOP loads a different DLL

		// We do some OpenGL teardown on destruction, so ask the TOP_Context
		// to set up our OpenGL context
		context->beginGLCommands();

		delete (ExGpuVideoTOP*)instance;

		context->endGLCommands();
	}

};


ExGpuVideoTOP::ExGpuVideoTOP(const OP_NodeInfo* info, TOP_Context* context)
	: node_info(info)
	, isLoaded_(false)
	, width_(0.f)
	, height_(0.f)
	, exec_count_(0)
	, frame_(0)
	, frame_count_(0)
{

	static bool needGLEWInit = true;
	if (needGLEWInit)
	{
		needGLEWInit = false;
		context->beginGLCommands();
		glewInit();
		context->endGLCommands();
	}

	//// If you wanted to do other GL initialization inside this constructor, you could
	//// uncomment these lines and do the work between the begin/end
	////
	context->beginGLCommands();
	setupGL();
	context->endGLCommands();
}

ExGpuVideoTOP::~ExGpuVideoTOP()
{}

void ExGpuVideoTOP::getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	// Setting cookEveryFrame to true causes the TOP to cook every frame even
	// if none of its inputs/parameters are changing. Set it to false if it
	// only needs to cook when inputs/parameters change.
	ginfo->cookEveryFrame = true;
}

bool ExGpuVideoTOP::getOutputFormat(TOP_OutputFormat* format, const OP_Inputs* inputs, void* reserved1)
{
	// In this function we could assign variable values to 'format' to specify
	// the pixel format/resolution etc that we want to output to.
	// If we did that, we'd want to return true to tell the TOP to use the settings we've
	// specified.
	// In this example we'll return false and use the TOP's settings
	return false;
}


void ExGpuVideoTOP::execute(TOP_OutputFormatSpecs* outputFormat, 
							const OP_Inputs* inputs,
							TOP_Context* context, 
							void* reserved1)
{

	int w = outputFormat->width;
	int h = outputFormat->height;

	mode_ = (Mode)inputs->getParInt("Loadmode");
	filepath = inputs->getParFilePath("File");

	context->beginGLCommands();
	glViewport(0, 0, w, h);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	if (isLoaded_)
	{
		frame_ += fps_ / 60.f;
		frame_ = std::fmodf(frame_, (float)frame_count_ - 1.f);

		video_texture_->updateCPU(frame_);
		video_texture_->uploadGPU();

		glUseProgram(shader_prg.getName());

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, video_texture_->getTexture());
		glUniform1i(glGetUniformLocation(shader_prg.getName(), "u_src"), 0);

		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glUseProgram(0);
	}

	context->endGLCommands();

	exec_count_++;
}

int32_t ExGpuVideoTOP::getNumInfoCHOPChans(void* reserved1)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the TOP. In this example we are just going to send one channel.
	return 0;
}

void ExGpuVideoTOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.
}

bool ExGpuVideoTOP::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void ExGpuVideoTOP::getInfoDATEntries(int32_t index,
	int32_t nEntries,
	OP_InfoDATEntries* entries,
	void* reserved1)
{
	char tempBuffer[4096];

	if (index == 0)
	{
		strcpy_s(tempBuffer, "executeCount");
		entries->values[0]->setString(tempBuffer);

		sprintf_s(tempBuffer, "%d", exec_count_);
		entries->values[1]->setString(tempBuffer);
	}
}

void ExGpuVideoTOP::getErrorString(OP_String* error, void* reserved1)
{
	error->setString(shader_err);
}

void ExGpuVideoTOP::setupParameters(OP_ParameterManager* manager, void* reserved1)
{

	// file
	{
		OP_StringParameter	sp;

		sp.name = "File";
		sp.label = "File";
		sp.page = "Play";
		sp.defaultValue = "None";

		OP_ParAppendResult res = manager->appendFile(sp);
		assert(res == OP_ParAppendResult::Success);
	}

	// load mode
	{
		OP_StringParameter	sp;

		sp.name = "Loadmode";
		sp.label = "Load Mode";
		sp.page = "Play";
		sp.defaultValue = "Streamingfromstrage";

		const char* names[] = { "Streamingfromstrage", "Streamingfromvpumemory", "Streamingfromcpumemorydecompressed", "Ongpumemory" };
		const char* labels[] = { "Streaming From Strage", "Streaming From CPU Memory", "Streaming From CPU Memory Decompressed", "On GPU Memory" };

		OP_ParAppendResult res = manager->appendMenu(sp, 4, names, labels);
		assert(res == OP_ParAppendResult::Success);
	}

	// Load pulse
	{
		OP_NumericParameter	np;

		np.name = "Reload";
		np.label = "ReLoad";
		np.page = "Play";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}


	// Unload pulse
	{
		OP_NumericParameter	np;

		np.name = "Unload";
		np.label = "UnLoad";
		np.page = "Play";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}

}

void ExGpuVideoTOP::pulsePressed(const char* name, void* reserved1)
{
	if (strcmp(name, "Reload") == 0 && !isLoaded_)
	{
		reload();
	}


	if (strcmp(name, "Unload") == 0 && isLoaded_)
	{
		unload();
	}
}


void ExGpuVideoTOP::reload() 
{
	std::thread t;
	switch (mode_)
	{
		case GPU_VIDEO_STREAMING_FROM_STORAGE:
		{
			t = std::move(std::thread([this]() {
				reader_ = std::make_shared<GpuVideoReader>(filepath, false);
			}));
			t.join();
			video_texture_ = std::make_shared<GpuVideoStreamingTexture>(reader_, GL_LINEAR, GL_CLAMP_TO_EDGE);
			break;
		}

		case GPU_VIDEO_STREAMING_FROM_CPU_MEMORY:
		{
			t = std::move(std::thread([this]() {
				reader_ = std::make_shared<GpuVideoReader>(filepath, true);
			}));
			t.join();
			video_texture_ = std::make_shared<GpuVideoStreamingTexture>(reader_, GL_LINEAR, GL_CLAMP_TO_EDGE);
			break;
		}

		case GPU_VIDEO_STREAMING_FROM_CPU_MEMORY_DECOMPRESSED:
		{
			t = std::move(std::thread([this]() {
				reader_ = std::make_shared<GpuVideoReaderDecompressed>(std::make_shared<GpuVideoReader>(filepath, false));
			}));
			t.join();
			video_texture_ = std::make_shared<GpuVideoStreamingTexture>(reader_, GL_LINEAR, GL_CLAMP_TO_EDGE);
			break;
		}

		case GPU_VIDEO_ON_GPU_MEMORY:
		{
			t = std::move(std::thread([this]() {
				reader_ = std::make_shared<GpuVideoReader>(filepath, false);
			}));
			t.join();
			video_texture_ = std::make_shared<GpuVideoOnGpuMemoryTexture>(reader_, GL_LINEAR, GL_CLAMP_TO_EDGE);
			break;
		}
	}

	width_ = reader_->getWidth();
	height_ = reader_->getHeight();
	frame_count_ = reader_->getFrameCount();
	fps_ = reader_->getFramePerSecond();

	//std::cout << "width : " << width_ << std::endl;
	//std::cout << "header : " << height_ << std::endl;
	//std::cout << "frame count : " << reader_->getFrameCount() << std::endl;
	//std::cout << "fps : " << reader_->getFramePerSecond() << std::endl;
	//std::cout << "==================" << std::endl;

	isLoaded_ = true;
}

void ExGpuVideoTOP::unload() 
{
	video_texture_ = std::shared_ptr<IGpuVideoTexture>();
	width_ = 0;
	height_ = 0;
	frame_count_ = 0;
	fps_ = 0;
	frame_ = 0;
	isLoaded_ = false;
}


void ExGpuVideoTOP::setupGL()
{
	shader_err = shader_prg.build(vertexShader, fragmentShader);

	// If an error occurred creating myProgram, we can't proceed
	if (shader_err == nullptr)
	{

		GLfloat vertices[] = {
			-1.0f, -1.0f, 0.0f,
			1.0f, -1.0f, 0.0f,
			1.0f, 1.0f, 0.0f,
			-1.0f, 1.0f, 0.0f
		};

		GLfloat texcoords[] = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f
		};

		int indices[] = { 0, 1, 2, 0, 3, 2 };


		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vertex_vbo);
		glGenBuffers(1, &texcoord_vbo);
		glGenBuffers(1, &ebo);


		glBindVertexArray(vao);

		// Position attribute
		glBindBuffer(GL_ARRAY_BUFFER, vertex_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);

		// Texcoord attribute
		glBindBuffer(GL_ARRAY_BUFFER, texcoord_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);

		// Element Array Buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

	}
}
