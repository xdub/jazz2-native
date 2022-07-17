#pragma once

#define NCINE_INCLUDE_OPENGL
#include "../CommonHeaders.h"

#include "Material.h"
#include "../Primitives/Matrix4x4.h"
#include "GL/GLShaderUniforms.h"
#include "GL/GLShaderProgram.h"
#include "../Base/HashMap.h"

#include <memory>

namespace nCine
{
	class RenderBuffersManager;
	class RenderVaoPool;
	class RenderCommandPool;
	class RenderBatcher;
	class Camera;
	class Viewport;

	/// The class that creates and handles application common OpenGL rendering resources
	class RenderResources
	{
	public:
		/// A vertex format structure for vertices with positions only
		struct VertexFormatPos2
		{
			GLfloat position[2];
		};

		/// A vertex format structure for vertices with positions and texture coordinates
		struct VertexFormatPos2Tex2
		{
			GLfloat position[2];
			GLfloat texcoords[2];
		};

		/// A vertex format structure for vertices with positions and draw indices
		struct VertexFormatPos2Index
		{
			GLfloat position[2];
			int drawindex;
		};

		/// A vertex format structure for vertices with positions, texture coordinates and draw indices
		struct VertexFormatPos2Tex2Index
		{
			GLfloat position[2];
			GLfloat texcoords[2];
			int drawindex;
		};

		struct CameraUniformData
		{
			CameraUniformData()
				: camera(nullptr), updateFrameProjectionMatrix(0), updateFrameViewMatrix(0) {}

			GLShaderUniforms shaderUniforms;
			Camera* camera;
			unsigned long int updateFrameProjectionMatrix;
			unsigned long int updateFrameViewMatrix;
		};

		static inline RenderBuffersManager& buffersManager() {
			return *buffersManager_;
		}
		static inline RenderVaoPool& vaoPool() {
			return *vaoPool_;
		}
		static inline RenderCommandPool& renderCommandPool() {
			return *renderCommandPool_;
		}
		static inline RenderBatcher& renderBatcher() {
			return *renderBatcher_;
		}

		static GLShaderProgram* shaderProgram(Material::ShaderProgramType shaderProgramType);

		static GLShaderProgram* batchedShader(const GLShaderProgram* shader);
		static bool registerBatchedShader(const GLShaderProgram* shader, GLShaderProgram* batchedShader);
		static bool unregisterBatchedShader(const GLShaderProgram* shader);

		static inline unsigned char* cameraUniformsBuffer() {
			return cameraUniformsBuffer_;
		}
		static CameraUniformData* findCameraUniformData(GLShaderProgram* shaderProgram);
		static void insertCameraUniformData(GLShaderProgram* shaderProgram, CameraUniformData&& cameraUniformData);
		static bool removeCameraUniformData(GLShaderProgram* shaderProgram);

		static inline const Camera* currentCamera() {
			return currentCamera_;
		}
		static inline const Viewport* currentViewport() {
			return currentViewport_;
		}

		static void setDefaultAttributesParameters(GLShaderProgram& shaderProgram);

	private:
		static std::unique_ptr<RenderBuffersManager> buffersManager_;
		static std::unique_ptr<RenderVaoPool> vaoPool_;
		static std::unique_ptr<RenderCommandPool> renderCommandPool_;
		static std::unique_ptr<RenderBatcher> renderBatcher_;

		static std::unique_ptr<GLShaderProgram> defaultShaderPrograms_[16];
		static HashMap<const GLShaderProgram*, GLShaderProgram*> batchedShaders_;

		static constexpr int UniformsBufferSize = 128; // two 4x4 float matrices
		static unsigned char cameraUniformsBuffer_[UniformsBufferSize];
		static HashMap<GLShaderProgram*, CameraUniformData> cameraUniformDataMap_;

		static Camera* currentCamera_;
		static std::unique_ptr<Camera> defaultCamera_;
		static Viewport* currentViewport_;

		static void setCurrentCamera(Camera* camera);
		static void updateCameraUniforms();
		static void setCurrentViewport(Viewport* viewport);

		static void create();
		static void createMinimal();
		static void dispose();

		static void registerDefaultBatchedShaders();

		/// Static class, deleted constructor
		RenderResources() = delete;
		/// Static class, deleted copy constructor
		RenderResources(const RenderResources& other) = delete;
		/// Static class, deleted assignement operator
		RenderResources& operator=(const RenderResources& other) = delete;

		/// The `Application` class needs to create and dispose the resources
		friend class Application;
		/// The `Viewport` class needs to set the current camera
		friend class Viewport;
		/// The `ScreenViewport` class needs to change the projection of the default camera
		friend class ScreenViewport;
	};

}
