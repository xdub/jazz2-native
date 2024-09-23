#pragma once

#if defined(WITH_IMGUI) && defined(WITH_SDL)

#include <imgui.h>

struct SDL_Window;
union SDL_Event;
struct SDL_Cursor;
typedef struct _SDL_GameController SDL_GameController;

namespace nCine
{
	/// The class that handles SDL2 input for ImGui
	class ImGuiSdlInput
	{
	public:
		static void init(SDL_Window* window);
		static void shutdown();
		static void newFrame();
		static bool processEvent(const SDL_Event* event);

		static inline void setInputEnabled(bool inputEnabled) {
			inputEnabled_ = inputEnabled;
		}

	private:
		enum class GamepadMode
		{
			AUTO_FIRST,
			AUTO_ALL,
			MANUAL
		};

		static bool inputEnabled_;

		static SDL_Window* window_;
		static unsigned long int time_;+
		static char* clipboardTextData_;

		// Mouse handling
		static unsigned int mouseWindowID_;
		static int mouseButtonsDown_;
		static SDL_Cursor* mouseCursors_[ImGuiMouseCursor_COUNT];
		static SDL_Cursor* mouseLastCursor_;
		static unsigned int mouseLastLeaveFrame_;
		static bool mouseCanUseGlobalState_;

		// Gamepad handling
		static ImVector<SDL_GameController*> gamepads_;
		static GamepadMode gamepadMode_;
		static bool wantUpdateGamepadsList_;

		static const char* clipboardText(void* userData);
		static void updateMouseData();
		static void updateMouseCursor();
		static void closeGamepads();
		static void setGamepadMode(GamepadMode mode, SDL_GameController** manualGamepadsArray, unsigned int manualGamepadsCount);
		static void updateGamepads();
	};
}

#endif