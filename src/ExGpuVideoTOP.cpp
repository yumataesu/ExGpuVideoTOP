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

static const char* vertexShader = "#version 330\n\
uniform mat4 uModelView; \
in vec3 P; \
void main() { \
    gl_Position = vec4(P, 1) * uModelView; \
}";

static const char* fragmentShader = "#version 330\n\
uniform vec4 uColor; \
out vec4 finalColor; \
void main() { \
    finalColor = uColor; \
}";

static const char* uniformError = "A uniform location could not be found.";


#include "ExGpuVideoTOP.h"


// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
void
FillTOPPluginInfo(TOP_PluginInfo *info)
{
	// This must always be set to this constant
	info->apiVersion = TOPCPlusPlusAPIVersion;

	// Change this to change the executeMode behavior of this plugin.
	info->executeMode = TOP_ExecuteMode::OpenGL_FBO;

	// The opType is the unique name for this TOP. It must start with a 
	// capital A-Z character, and all the following characters must lower case
	// or numbers (a-z, 0-9)
	info->customOPInfo.opType->setString("Cpumemsample");

	// The opLabel is the text that will show up in the OP Create Dialog
	info->customOPInfo.opLabel->setString("CPU Mem Sample");

	// Will be turned into a 3 letter icon on the nodes
	info->customOPInfo.opIcon->setString("CPM");

	// Information about the author of this OP
	info->customOPInfo.authorName->setString("Author Name");
	info->customOPInfo.authorEmail->setString("email@email.com");

	// This TOP works with 0 or 1 inputs connected
	info->customOPInfo.minInputs = 0;
	info->customOPInfo.maxInputs = 1;
}

DLLEXPORT
TOP_CPlusPlusBase*
CreateTOPInstance(const OP_NodeInfo* info, TOP_Context* context)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per TOP that is using the .dll
	return new ExGpuVideoTOP(info, context);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context *context)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL
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
	, myBrightness(1.f)
{

	// GLEW is global static function pointers, only needs to be inited once,
	// and only on Windows.
	
	static bool needGLEWInit = true;
	if (needGLEWInit)
	{
		needGLEWInit = false;
		context->beginGLCommands();
		// Setup all our GL extensions using GLEW
		//glewInit();
		context->endGLCommands();
	}

}

ExGpuVideoTOP::~ExGpuVideoTOP()
{

}

void ExGpuVideoTOP::getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	ginfo->cookEveryFrame = true;
    ginfo->memPixelType = OP_CPUMemPixelType::RGBA32Float;
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


void ExGpuVideoTOP::execute(TOP_OutputFormatSpecs* outputFormat, const OP_Inputs* inputs, TOP_Context* context, void* reserved1)
{
	int width = outputFormat->width;
	int height = outputFormat->height;

	myBrightness = inputs->getParDouble("Brightness");

	context->beginGLCommands();
	glViewport(0, 0, width, height);
	glClearColor(0.0, myBrightness, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	context->endGLCommands();

	myExecuteCount++;
}

void ExGpuVideoTOP::startMoreWork()
{
	//{
	//	std::unique_lock<std::mutex> lck(myConditionLock);
	//	myStartWork = true;
	//}
	//myCondition.notify_one();
}

int32_t ExGpuVideoTOP::getNumInfoCHOPChans(void *reserved1)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the TOP. In this example we are just going to send one channel.
	return 2;
}

void ExGpuVideoTOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name->setString("executeCount");
		chan->value = (float)myExecuteCount;
	}
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
								void *reserved1)
{
	char tempBuffer[4096];

	if (index == 0)
	{
        strcpy_s(tempBuffer, "executeCount");
        entries->values[0]->setString(tempBuffer);

        sprintf_s(tempBuffer, "%d", myExecuteCount);
        entries->values[1]->setString(tempBuffer);
	}

	if (index == 1)
	{
		strcpy_s(tempBuffer, "step");
		entries->values[0]->setString(tempBuffer);
	}
}

void ExGpuVideoTOP::setupParameters(OP_ParameterManager* manager, void *reserved1)
{
	// brightness
	{
		OP_NumericParameter	np;

		np.name = "Brightness";
		np.label = "Brightness";
		np.defaultValues[0] = 1.0;

		np.minSliders[0] =  0.0;
		np.maxSliders[0] =  1.0;

		np.minValues[0] = 0.0;
		np.maxValues[0] = 1.0;

		np.clampMins[0] = true;
		np.clampMaxes[0] = true;
		
		OP_ParAppendResult res = manager->appendFloat(np);
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

void ExGpuVideoTOP::pulsePressed(const char* name, void *reserved1)
{

	if (strcmp(name, "Load") == 0 && !isLoaded_)
	{
		std::cout << "pulsePressed : " << name << std::endl;
		std::string path = "Transvolt_12.gvintermediate.gv";
		reader_ = std::make_shared<GpuVideoReader>(path.c_str(), false);

		width_ = reader_->getWidth();
		height_ = reader_->getHeight();

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

