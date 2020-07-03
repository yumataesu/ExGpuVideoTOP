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
	: myNodeInfo(info)
	, isLoaded_(false)
	, width_(0.f)
	, height_(0.f)
	, myExecuteCount(0)
	, frame_(0)
	, _frameCount(0)
{

#ifdef _WIN32
	// GLEW is global static function pointers, only needs to be inited once,
	// and only on Windows.
	static bool needGLEWInit = true;
	if (needGLEWInit)
	{
		needGLEWInit = false;
		context->beginGLCommands();
		glewInit();
		context->endGLCommands();
	}
#endif

	//// If you wanted to do other GL initialization inside this constructor, you could
	//// uncomment these lines and do the work between the begin/end
	////
	context->beginGLCommands();
	setupGL();
	context->endGLCommands();
}

ExGpuVideoTOP::~ExGpuVideoTOP()
{}

void
ExGpuVideoTOP::getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	// Setting cookEveryFrame to true causes the TOP to cook every frame even
	// if none of its inputs/parameters are changing. Set it to false if it
	// only needs to cook when inputs/parameters change.
	ginfo->cookEveryFrame = true;
}

bool
ExGpuVideoTOP::getOutputFormat(TOP_OutputFormat* format, const OP_Inputs* inputs, void* reserved1)
{
	// In this function we could assign variable values to 'format' to specify
	// the pixel format/resolution etc that we want to output to.
	// If we did that, we'd want to return true to tell the TOP to use the settings we've
	// specified.
	// In this example we'll return false and use the TOP's settings
	return false;
}


void
ExGpuVideoTOP::execute(TOP_OutputFormatSpecs* outputFormat,
	const OP_Inputs* inputs,
	TOP_Context* context,
	void* reserved1)
{

	int width = outputFormat->width;
	int height = outputFormat->height;

	file = inputs->getParFilePath("File");

	context->beginGLCommands();
	glViewport(0, 0, width, height);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	if (isLoaded_) 
	{
		//setFrame
		frame_ = std::max(frame_, 0);
		frame_ = std::min(frame_, (int)_frameCount - 1);
		_videoTexture->updateCPU(frame_);
		_videoTexture->uploadGPU();

		glUseProgram(myProgram.getName());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, _videoTexture->getTexture());
		glUniform1i(glGetUniformLocation(myProgram.getName(), "u_src"), 1);

		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glUseProgram(0);
	}

	context->endGLCommands();

	frame_++;
	frame_ = frame_ % ((int)_frameCount - 1);
	myExecuteCount++;
}

int32_t
ExGpuVideoTOP::getNumInfoCHOPChans(void* reserved1)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the TOP. In this example we are just going to send one channel.
	return 2;
}

void
ExGpuVideoTOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name->setString("executeCount");
		chan->value = (float)myExecuteCount;
	}
}

bool
ExGpuVideoTOP::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
ExGpuVideoTOP::getInfoDATEntries(int32_t index,
	int32_t nEntries,
	OP_InfoDATEntries* entries,
	void* reserved1)
{
	char tempBuffer[4096];

	if (index == 0)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "executeCount");
#else // macOS
		strlcpy(tempBuffer, "executeCount", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%d", myExecuteCount);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%d", myExecuteCount);
#endif
		entries->values[1]->setString(tempBuffer);
	}
}

void
ExGpuVideoTOP::getErrorString(OP_String* error, void* reserved1)
{
	error->setString(myError);
}

void
ExGpuVideoTOP::setupParameters(OP_ParameterManager* manager, void* reserved1)
{

	// Unload pulse
	// shape
	{
		OP_StringParameter	sp;

		sp.name = "File";
		sp.label = "File";
		sp.defaultValue = "Sine";

		OP_ParAppendResult res = manager->appendFile(sp);
		assert(res == OP_ParAppendResult::Success);
	}

	// Load pulse
	{
		OP_NumericParameter	np;

		np.name = "Load";
		np.label = "Load";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}


	// Unload pulse
	{
		OP_NumericParameter	np;

		np.name = "Unload";
		np.label = "Unload";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}

}

void
ExGpuVideoTOP::pulsePressed(const char* name, void* reserved1)
{

	if (strcmp(name, "Load") == 0 && !isLoaded_)
	{	
		reader_ = std::make_shared<GpuVideoReader>(file, false);
		_videoTexture = std::make_shared<GpuVideoStreamingTexture>(reader_, GL_LINEAR, GL_CLAMP_TO_EDGE);

		width_ = reader_->getWidth();
		height_ = reader_->getHeight();
		_frameCount = reader_->getFrameCount();

		std::cout << "width : " << width_ << std::endl;
		std::cout << "header : " << height_ << std::endl;
		std::cout << "frame count : " << reader_->getFrameCount() << std::endl;
		std::cout << "fps : " << reader_->getFramePerSecond() << std::endl;
		std::cout << "==================" << std::endl;

		isLoaded_ = true;
	}


	if (strcmp(name, "Unload") == 0 && isLoaded_)
	{
		std::cout << "pulsePressed : " << name << std::endl;
		isLoaded_ = false;
	}
}

void ExGpuVideoTOP::setupGL()
{
	myError = myProgram.build(vertexShader, fragmentShader);

	// If an error occurred creating myProgram, we can't proceed
	if (myError == nullptr)
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
