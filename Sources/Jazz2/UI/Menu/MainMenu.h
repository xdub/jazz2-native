﻿#pragma once

#include "IMenuContainer.h"
#include "MenuSection.h"
#include "../../IStateHandler.h"
#include "../../IRootController.h"
#include "../Canvas.h"
#include "../UpscaleRenderPass.h"
#include "../../ContentResolver.h"
#include "../../Tiles/TileMap.h"

#include "../../../nCine/Graphics/Camera.h"
#include "../../../nCine/Graphics/Shader.h"
#include "../../../nCine/Input/InputEvents.h"
#include "../../../nCine/Audio/AudioStreamPlayer.h"

using namespace Jazz2::Tiles;

namespace Jazz2::UI::Menu
{
	class BeginSection;
	class RefreshCacheSection;
	class StartGameOptionsSection;
#if defined(WITH_MULTIPLAYER)
	class CreateServerOptionsSection;
#endif

	class MainMenu : public IStateHandler, public IMenuContainer
	{
		friend class BeginSection;
		friend class RefreshCacheSection;
		friend class StartGameOptionsSection;
#if defined(WITH_MULTIPLAYER)
		friend class CreateServerOptionsSection;
#endif

	public:
		static constexpr int32_t DefaultWidth = 720;
		static constexpr int32_t DefaultHeight = 405;

		MainMenu(IRootController* root, bool afterIntro);
		~MainMenu() override;

		void Reset();

		void OnBeginFrame() override;
		void OnInitializeViewport(std::int32_t width, std::int32_t height) override;

		void OnKeyPressed(const KeyboardEvent& event) override;
		void OnKeyReleased(const KeyboardEvent& event) override;
		void OnTouchEvent(const nCine::TouchEvent& event) override;

		MenuSection* SwitchToSectionDirect(std::unique_ptr<MenuSection> section) override;
		void LeaveSection() override;
		MenuSection* GetUnderlyingSection() const override;
		void ChangeLevel(LevelInitialization&& levelInit) override;
		bool HasResumableState() const override;
		void ResumeSavedState() override;
#if defined(WITH_MULTIPLAYER)
		bool ConnectToServer(const StringView address, std::uint16_t port) override;
		bool CreateServer(LevelInitialization&& levelInit, std::uint16_t port) override;
#endif
		void ApplyPreferencesChanges(ChangedPreferencesType type) override;
		bool ActionPressed(PlayerActions action) override;
		bool ActionHit(PlayerActions action) override;

		Vector2i GetViewSize() const override {
			return _canvasBackground->ViewSize;
		}

		Recti GetContentBounds() const override {
			return _contentBounds;
		}

		void DrawElement(AnimState state, std::int32_t frame, float x, float y, std::uint16_t z, Alignment align, const Colorf& color,
			float scaleX = 1.0f, float scaleY = 1.0f, bool additiveBlending = false, bool unaligned = false) override;
		void DrawElement(AnimState state, float x, float y, std::uint16_t z, Alignment align, const Colorf& color,
			const Vector2f& size, const Vector4f& texCoords, bool unaligned = false) override;
		void DrawSolid(float x, float y, std::uint16_t z, Alignment align, const Vector2f& size, const Colorf& color, bool additiveBlending = false) override;
		void DrawTexture(const Texture& texture, float x, float y, std::uint16_t z, Alignment align, const Vector2f& size, const Colorf& color, bool unaligned = false) override;
		Vector2f MeasureString(const StringView text, float scale = 1.0f, float charSpacing = 1.0f, float lineSpacing = 1.0f) override;
		void DrawStringShadow(const StringView text, std::int32_t& charOffset, float x, float y, uint16_t z, Alignment align, const Colorf& color,
			float scale = 1.0f, float angleOffset = 0.0f, float varianceX = 4.0f, float varianceY = 4.0f,
			float speed = 0.4f, float charSpacing = 1.0f, float lineSpacing = 1.0f) override;
		void PlaySfx(const StringView identifier, float gain = 1.0f) override;

	private:
		IRootController* _root;

		enum class Preset {
			None,
			Default,
			Carrotus,
			SharewareDemo,
			Xmas
		};

		enum class ActiveCanvas {
			Background,
			Clipped,
			Overlay
		};

		class MenuBackgroundCanvas : public Canvas
		{
		public:
			MenuBackgroundCanvas(MainMenu* owner) : _owner(owner) { }

			bool OnDraw(RenderQueue& renderQueue) override;

		private:
			MainMenu* _owner;
		};

		class MenuClippedCanvas : public Canvas
		{
		public:
			MenuClippedCanvas(MainMenu* owner) : _owner(owner) { }

			bool OnDraw(RenderQueue& renderQueue) override;

		private:
			MainMenu* _owner;
		};

		class MenuOverlayCanvas : public Canvas
		{
		public:
			MenuOverlayCanvas(MainMenu* owner) : _owner(owner) { }

			bool OnDraw(RenderQueue& renderQueue) override;

		private:
			MainMenu* _owner;
		};

		class TexturedBackgroundPass : public SceneNode
		{
			friend class MainMenu;

		public:
			TexturedBackgroundPass(MainMenu* owner) : _owner(owner), _alreadyRendered(false)
			{
				setVisitOrderState(SceneNode::VisitOrderState::Disabled);
			}

			void Initialize();

			bool OnDraw(RenderQueue& renderQueue) override;

		private:
			MainMenu* _owner;
			std::unique_ptr<Texture> _target;
			std::unique_ptr<Viewport> _view;
			std::unique_ptr<Camera> _camera;
			SmallVector<std::unique_ptr<RenderCommand>, 0> _renderCommands;
			RenderCommand _outputRenderCommand;
			bool _alreadyRendered;
		};

		TexturedBackgroundPass _texturedBackgroundPass;
		UI::UpscaleRenderPassWithClipping _upscalePass;

		std::unique_ptr<TileSet> _tileSet;
		TileMapLayer _texturedBackgroundLayer;
		Vector2f _texturedBackgroundPos;
		float _texturedBackgroundPhase;
		Preset _preset;

		Recti _contentBounds;
		std::unique_ptr<MenuBackgroundCanvas> _canvasBackground;
		std::unique_ptr<MenuClippedCanvas> _canvasClipped;
		std::unique_ptr<MenuOverlayCanvas> _canvasOverlay;
		ActiveCanvas _activeCanvas;
		Metadata* _metadata;
		Font* _smallFont;
		Font* _mediumFont;

		float _transitionWhite;
		float _logoTransition;
#if defined(WITH_AUDIO)
		std::unique_ptr<AudioStreamPlayer> _music;
		SmallVector<std::shared_ptr<AudioBufferPlayer>> _playingSounds;
#endif
		SmallVector<Tiles::TileMap::DestructibleDebris, 0> _debrisList;

		SmallVector<std::unique_ptr<MenuSection>, 8> _sections;
		BitArray _pressedKeys;
		std::uint32_t _pressedActions;
		float _touchButtonsTimer;

		void PlayMenuMusic();
		void UpdateContentBounds(Vector2i viewSize);
		void UpdatePressedActions();
		void UpdateRichPresence();
		void UpdateDebris(float timeMult);
		void DrawDebris(RenderQueue& renderQueue);
		void PrepareTexturedBackground();
		bool TryLoadBackgroundPreset(Preset preset);
		void RenderTexturedBackground(RenderQueue& renderQueue);
		bool RenderLegacyBackground(RenderQueue& renderQueue);

		inline Canvas* GetActiveCanvas()
		{
			switch (_activeCanvas) {
				default:
				case ActiveCanvas::Background: return _canvasBackground.get();
				case ActiveCanvas::Clipped: return _canvasClipped.get();
				case ActiveCanvas::Overlay: return _canvasOverlay.get();
			}
		}
	};
}