#include <iostream>
#include <thread>

#include "TOP_CPlusPlusBase.h"
#include "GL/Program.h"

#include "ExtremeGpuVideo/Util.h"
#include "ExtremeGpuVideo/GpuVideo.h"
#include "ExtremeGpuVideo/GpuVideoIO.h"
#include "ExtremeGpuVideo/GpuVideoReader.h"
#include "ExtremeGpuVideo/GpuVideoReaderDecompressed.h"
#include "ExtremeGpuVideo/GpuVideoTexture.h"
#include "ExtremeGpuVideo/GpuVideoStreamingTexture.h"
#include "ExtremeGpuVideo/GpuVideoOnGpuMemoryTexture.h"


class ExGpuVideoTOP : public TOP_CPlusPlusBase
{
public:
	ExGpuVideoTOP(const OP_NodeInfo *info, TOP_Context *context);
	virtual ~ExGpuVideoTOP();

	virtual void		getGeneralInfo(TOP_GeneralInfo*, const OP_Inputs*, void* reserved1) override;
	virtual bool		getOutputFormat(TOP_OutputFormat*, const OP_Inputs*, void* reserved1) override;


	virtual void		execute(TOP_OutputFormatSpecs*,
								const OP_Inputs*,
								TOP_Context *context, void* reserved1) override;


	virtual int32_t		getNumInfoCHOPChans(void* reserved1) override;
	virtual void		getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan *chan,
										void* reserved1) override;

	virtual bool		getInfoDATSize(OP_InfoDATSize *infoSize, void* reserved1) override;
	virtual void		getInfoDATEntries(int32_t index,
											int32_t nEntries,
											OP_InfoDATEntries *entries,
											void* reserved1) override;

    virtual void		getErrorString(OP_String *error, void* reserved1) override;

	virtual void		setupParameters(OP_ParameterManager *manager, void* reserved1) override;
	virtual void		pulsePressed(const char *name, void* reserved1) override;

private:
    void                setupGL();
	void				load();
	void				unload();

	const OP_NodeInfo*	node_info;

	int32_t				exec_count_;

    Program				shader_prg;
    const char*			shader_err;

	GLuint				vao;
	GLuint				vertex_vbo, texcoord_vbo, ebo;

	bool				isLoaded_;
	int					width_, height_;
	int					frame_count_;
	float				fps_;
	float				frame_;
	Mode				mode_;
	const char*			filepath;

	std::shared_ptr<IGpuVideoReader> reader_;
	std::unique_ptr<IGpuVideoTexture> video_texture_;

	std::unique_ptr<std::thread> thread_;

	std::string current;
	std::string previous;

};
