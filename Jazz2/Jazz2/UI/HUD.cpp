﻿#include "HUD.h"

#include "../LevelHandler.h"

#include "../../nCine/Graphics/RenderQueue.h"
#include "../../nCine/IO/IFileStream.h"
#include "../../nCine/Base/Random.h"
#include "../../nCine/Application.h"

namespace Jazz2::UI
{
	HUD::HUD(LevelHandler* levelHandler)
		:
		_levelHandler(levelHandler),
		_graphics(nullptr),
		_levelTextTime(-1.0f),
		_coins(0), _gems(0),
		_coinsTime(-1.0f), _gemsTime(-1.0f),
		_touchButtonsTimer(0.0f)
	{
		Metadata* metadata = ContentResolver::Current().RequestMetadata("UI/HUD"_s);
		if (metadata != nullptr) {
			_graphics = &metadata->Graphics;
		}

		_smallFont = std::make_unique<Font>(fs::joinPath({ "Content"_s, "Animations"_s, "_custom"_s, "font_small.png"_s }));

		_touchButtons[0] = CreateTouchButton(PlayerActions::None, "TouchDpad"_s, Alignment::BottomLeft, DpadLeft, DpadBottom, DpadSize, DpadSize);
		// D-pad subsections
		_touchButtons[1] = CreateTouchButton(PlayerActions::Left, { }, Alignment::BottomLeft | AllowRollover, DpadLeft - DpadThreshold, DpadBottom, (DpadSize / 3) + DpadThreshold, DpadSize);
		_touchButtons[2] = CreateTouchButton(PlayerActions::Right, { }, Alignment::BottomLeft | AllowRollover, DpadLeft + (DpadSize * 2 / 3), DpadBottom, (DpadSize / 3) + DpadThreshold, DpadSize);
		_touchButtons[3] = CreateTouchButton(PlayerActions::Up, { }, Alignment::BottomLeft, DpadLeft, DpadBottom + (DpadSize * 2 / 3), DpadSize, (DpadSize / 3) + DpadThreshold);
		_touchButtons[4] = CreateTouchButton(PlayerActions::Down, { }, Alignment::BottomLeft, DpadLeft, DpadBottom - DpadThreshold, DpadSize, (DpadSize / 3) + DpadThreshold);
		// Action buttons
		_touchButtons[5] = CreateTouchButton(PlayerActions::Fire, "TouchFire"_s, Alignment::BottomRight, (ButtonSize + 0.02f) * 2, 0.04f, ButtonSize, ButtonSize);
		_touchButtons[6] = CreateTouchButton(PlayerActions::Jump, "TouchJump"_s, Alignment::BottomRight, (ButtonSize + 0.02f), 0.04f + 0.08f, ButtonSize, ButtonSize);
		_touchButtons[7] = CreateTouchButton(PlayerActions::Run, "TouchRun"_s, Alignment::BottomRight, 0.001f, 0.01f + 0.15f, ButtonSize, ButtonSize);
		_touchButtons[8] = CreateTouchButton(PlayerActions::SwitchWeapon, "TouchSwitch"_s, Alignment::BottomRight, ButtonSize + 0.01f, 0.04f + 0.28f, SmallButtonSize, SmallButtonSize);

		static_assert(_countof(_touchButtons) == 9, "Touch button count mismatch");
	}

	void HUD::OnUpdate(float timeMult)
	{
		Canvas::OnUpdate(timeMult);

		if (_levelTextTime >= 0.0f) {
			_levelTextTime += timeMult;
		}
		if (_coinsTime >= 0.0f) {
			_coinsTime += timeMult;
		}
		if (_gemsTime >= 0.0f) {
			_gemsTime += timeMult;
		}
		if (_touchButtonsTimer > 0.0f) {
			_touchButtonsTimer -= timeMult;
		}
	}

	bool HUD::OnDraw(RenderQueue& renderQueue)
	{
		Canvas::OnDraw(renderQueue);

		if (_graphics == nullptr) {
			return false;
		}

		ViewSize = _levelHandler->GetViewSize();

		Rectf view = Rectf(0, 0, ViewSize.X, ViewSize.Y);
		Rectf adjustedView = view;
		if (_touchButtonsTimer > 0.0f) {
			float width = adjustedView.W;

			adjustedView.X = 90 + /*LeftPadding*/0.1f * width;
			adjustedView.W = adjustedView.W - adjustedView.X - (140 + /*RightPadding*/0.1f * width);
		}

		float right = adjustedView.X + adjustedView.W;
		float bottom = adjustedView.Y + adjustedView.H;

		int charOffset = 0;
		int charOffsetShadow = 0;
		char stringBuffer[32];

		auto& players = _levelHandler->GetPlayers();
		if (!players.empty()) {
			Actors::Player* player = players[0];
			PlayerType playerType = player->_playerType;

			// Bottom left
			StringView playerIcon;
			switch (playerType) {
				default:
				case PlayerType::Jazz: playerIcon = "CharacterJazz"_s; break;
				case PlayerType::Spaz: playerIcon = "CharacterSpaz"_s; break;
				case PlayerType::Lori: playerIcon = "CharacterLori"_s; break;
				case PlayerType::Frog: playerIcon = "CharacterFrog"_s; break;
			}

			DrawElement(playerIcon, -1, adjustedView.X + 36, bottom + 1.6f, ShadowLayer, Alignment::BottomRight, Colorf(0.0f, 0.0f, 0.0f, 0.4f));
			DrawElement(playerIcon, -1, adjustedView.X + 36, bottom, MainLayer, Alignment::BottomRight, Colorf::White);

			for (int i = 0; i < player->_health; i++) {
				stringBuffer[i] = '|';
			}
			stringBuffer[player->_health] = '\0';

			if (player->_lives > 0) {
				_smallFont->DrawString(this, stringBuffer, charOffsetShadow, adjustedView.X + 36 - 3 - 0.5f, bottom - 16 + 0.5f, FontShadowLayer,
					Alignment::BottomLeft, Colorf(0.0f, 0.0f, 0.0f, 0.42f), 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 1.1f);
				_smallFont->DrawString(this, stringBuffer, charOffsetShadow, adjustedView.X + 36 - 3 + 0.5f, bottom - 16 - 0.5f, FontShadowLayer,
					Alignment::BottomLeft, Colorf(0.0f, 0.0f, 0.0f, 0.42f), 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 1.1f);
				_smallFont->DrawString(this, stringBuffer, charOffset, adjustedView.X + 36 - 3, bottom - 16, FontLayer,
					Alignment::BottomLeft, Colorf::White, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 1.1f);

				snprintf(stringBuffer, _countof(stringBuffer), "x%i", player->_lives);
				_smallFont->DrawString(this, stringBuffer, charOffsetShadow, adjustedView.X + 36 - 4, bottom + 1.0f, FontShadowLayer,
					Alignment::BottomLeft, Colorf(0.0f, 0.0f, 0.0f, 0.32f));
				_smallFont->DrawString(this, stringBuffer, charOffset, adjustedView.X + 36 - 4, bottom, FontLayer,
					Alignment::BottomLeft, Colorf::White);
			} else {
				_smallFont->DrawString(this, stringBuffer, charOffsetShadow, adjustedView.X + 36 - 3 - 0.5f, bottom - 3 + 0.5f, FontShadowLayer,
					Alignment::BottomLeft, Colorf(0.0f, 0.0f, 0.0f, 0.42f), 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 1.1f);
				_smallFont->DrawString(this, stringBuffer, charOffsetShadow, adjustedView.X + 36 - 3 + 0.5f, bottom - 3 - 0.5f, FontShadowLayer,
					Alignment::BottomLeft, Colorf(0.0f, 0.0f, 0.0f, 0.42f), 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 1.1f);
				_smallFont->DrawString(this, stringBuffer, charOffset, adjustedView.X + 36 - 3, bottom - 3, FontLayer,
					Alignment::BottomLeft, Colorf::White, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 1.1f);
			}

			// Top left
			DrawElement("PickupFood"_s, -1, view.X + 3, view.Y + 3 + 1.6f, ShadowLayer, Alignment::TopLeft, Colorf(0.0f, 0.0f, 0.0f, 0.4f));
			DrawElement("PickupFood"_s, -1, view.X + 3, view.Y + 3, MainLayer, Alignment::TopLeft, Colorf::White);

			snprintf(stringBuffer, _countof(stringBuffer), "%08i", player->_score);
			_smallFont->DrawString(this, stringBuffer, charOffsetShadow, 14, 5 + 1, FontShadowLayer,
				Alignment::TopLeft, Colorf(0.0f, 0.0f, 0.0f, 0.32f), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.88f);
			_smallFont->DrawString(this, stringBuffer, charOffset, 14, 5, FontLayer,
				Alignment::TopLeft, Colorf::White, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.88f);

			// Bottom right
			if (playerType != PlayerType::Frog) {
				WeaponType weapon = player->_currentWeapon;
				Vector2f pos = Vector2f(right - 40, bottom);
				StringView currentWeaponString = GetCurrentWeapon(player, weapon, pos);

				StringView ammoCount;
				if (player->_weaponAmmo[(int)weapon] < 0) {
					ammoCount = "x\u221E"_s;
				} else {
					snprintf(stringBuffer, _countof(stringBuffer), "x%i", player->_weaponAmmo[(int)weapon] / 100);
					ammoCount = stringBuffer;
				}
				_smallFont->DrawString(this, ammoCount, charOffsetShadow, right - 40, bottom + 1.0f, FontShadowLayer,
					Alignment::BottomLeft, Colorf(0.0f, 0.0f, 0.0f, 0.32f), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.96f);
				_smallFont->DrawString(this, ammoCount, charOffset, right - 40, bottom, FontLayer,
					Alignment::BottomLeft, Colorf::White, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.96f);

				auto it = _graphics->find(String::nullTerminatedView(currentWeaponString));
				if (it != _graphics->end()) {
					if (it->second.Base->FrameDimensions.Y < 20) {
						pos.Y -= std::round((20 - it->second.Base->FrameDimensions.Y) * 0.5f);
					}

					DrawElement(currentWeaponString, -1, pos.X, pos.Y + 1.6f, ShadowLayer, Alignment::BottomRight, Colorf(0.0f, 0.0f, 0.0f, 0.4f));
					DrawElement(currentWeaponString, -1, pos.X, pos.Y, MainLayer, Alignment::BottomRight, Colorf::White);
				}
			}

			// Active Boss (health bar)
			// TODO

			// Misc
			DrawLevelText(charOffset);
			DrawCoins(charOffset);
			DrawGems(charOffset);

			// TODO
			//DrawWeaponWheel();

			// FPS
			snprintf(stringBuffer, _countof(stringBuffer), "%i", (int)std::round(theApplication().averageFps()));
			_smallFont->DrawString(this, stringBuffer, charOffset, view.W - 4, 0, FontLayer,
				Alignment::TopRight, Colorf::White, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.96f);

			// Touch Controls
			if (_touchButtonsTimer > 0.0f) {
				for (auto& button : _touchButtons) {
					if (button.Graphics == nullptr) {
						continue;
					}

					float x = button.Left;
					float y = button.Top;
					if ((button.Align & Alignment::Right) == Alignment::Right) {
						x = ViewSize.X - button.Width * 0.5f - x;
					} else {
						x = x + button.Width * 0.5f;
					}
					if ((button.Align & Alignment::Bottom) == Alignment::Bottom) {
						y = ViewSize.Y - button.Height * 0.5f - y;
					} else {
						y = y + button.Height * 0.5f;
					}
					x = x - ViewSize.X * 0.5f;
					y = ViewSize.Y * 0.5f - y;

					DrawTexture(*button.Graphics->Base->TextureDiffuse, Vector2f(x, y), TouchButtonsLayer, Vector2f(button.Width, button.Height), Vector4f(1.0f, 0.0f, -1.0f, 1.0f), Colorf::White);
				}
			}
		}

		return true;
	}

	void HUD::OnTouchEvent(const TouchEvent& event, uint32_t& overrideActions)
	{
		_touchButtonsTimer = 1200.0f;

		if (event.type == TouchEventType::Down || event.type == TouchEventType::PointerDown) {
			int pointerIndex = event.findPointerIndex(event.actionIndex);
			if (pointerIndex != -1) {
				float x = event.pointers[pointerIndex].x * (float)ViewSize.X;
				float y = event.pointers[pointerIndex].y * (float)ViewSize.Y;
				/*if (x < 0.5f) {
					x -= LeftPadding;
					y -= BottomPadding1;
				} else {
					x += RightPadding;
					y -= BottomPadding2;
				}*/

				for (int i = 0; i < TouchButtonsCount; i++) {
					auto& button = _touchButtons[i];
					if (button.Action != PlayerActions::None) {
						if (button.CurrentPointerId == -1 && IsOnButton(button, x, y)) {
							button.CurrentPointerId = event.actionIndex;
							overrideActions |= (1 << (int)button.Action);
						}
					}
				}
			}
		} else if (event.type == TouchEventType::Move) {
			for (int i = 0; i < TouchButtonsCount; i++) {
				auto& button = _touchButtons[i];
				if (button.Action != PlayerActions::None) {
					if (button.CurrentPointerId != -1) {
						bool isPressed = false;
						int pointerIndex = event.findPointerIndex(button.CurrentPointerId);
						if (pointerIndex != -1) {
							float x = event.pointers[pointerIndex].x * (float)ViewSize.X;
							float y = event.pointers[pointerIndex].y * (float)ViewSize.Y;
							/*if (x < 0.5f) {
								x -= LeftPadding;
							} else {
								x += RightPadding;
							}*/

							isPressed = IsOnButton(button, x, y);
						}

						if (!isPressed) {
							button.CurrentPointerId = -1;
							overrideActions &= ~(1 << (int)button.Action);
						}
					} else {
						// Only some buttons should allow roll-over (only when the player's on foot)
						auto& players = _levelHandler->GetPlayers();
						bool canPlayerMoveVertically = (!players.empty() && players[0]->CanMoveVertically());
						if ((button.Align & AllowRollover) != AllowRollover && !canPlayerMoveVertically) continue;

						for (int j = 0; j < event.count; j++) {
							float x = event.pointers[j].x * (float)ViewSize.X;
							float y = event.pointers[j].y * (float)ViewSize.Y;
							/*if (x < 0.5f) {
								x -= LeftPadding;
							} else {
								x += RightPadding;
							}*/

							if (IsOnButton(button, x, y)) {
								button.CurrentPointerId = event.pointers[j].id;
								overrideActions |= (1 << (int)button.Action);
								break;
							}
						}
					}
				}
			}
		} else if (event.type == TouchEventType::Up) {
			for (int i = 0; i < TouchButtonsCount; i++) {
				auto& button = _touchButtons[i];
				if (button.CurrentPointerId != -1) {
					button.CurrentPointerId = -1;
					overrideActions &= ~(1 << (int)button.Action);
				}
			}

		} else if (event.type == TouchEventType::PointerUp) {
			for (int i = 0; i < TouchButtonsCount; i++) {
				auto& button = _touchButtons[i];
				if (button.CurrentPointerId == event.actionIndex) {
					button.CurrentPointerId = -1;
					overrideActions &= ~(1 << (int)button.Action);
				}
			}
		}
	}

	void HUD::ShowLevelText(const StringView& text)
	{
		if (_levelText == text || text.empty()) {
			return;
		}

		_levelText = text;
		_levelTextTime = 0.0f;
	}

	void HUD::ShowCoins(int count)
	{
		constexpr float StillTime = 120.0f;
		constexpr float TransitionTime = 60.0f;

		_coins = count;

		if (_coinsTime < 0.0f) {
			_coinsTime = 0.0f;
		} else if (_coinsTime > TransitionTime) {
			_coinsTime = TransitionTime;
		}

		if (_gemsTime >= 0.0f) {
			if (_gemsTime <= TransitionTime + StillTime) {
				_gemsTime = TransitionTime + StillTime;
			} else {
				_gemsTime = -1.0f;
			}
		}
	}

	void HUD::ShowGems(int count)
	{
		constexpr float StillTime = 120.0f;
		constexpr float TransitionTime = 60.0f;

		_gems = count;

		if (_gemsTime < 0.0f) {
			_gemsTime = 0.0f;
		} else if (_gemsTime > TransitionTime) {
			_gemsTime = TransitionTime;
		}

		if (_coinsTime >= 0.0f) {
			if (_coinsTime <= TransitionTime + StillTime) {
				_coinsTime = TransitionTime + StillTime;
			} else {
				_coinsTime = -1.0f;
			}
		}
	}

	void HUD::DrawLevelText(int& charOffset)
	{
		constexpr float StillTime = 350.0f;
		constexpr float TransitionTime = 100.0f;
		constexpr float TotalTime = StillTime + TransitionTime * 2.0f;

		if (_levelTextTime < 0.0f) {
			return;
		}

		float offset;
		if (_levelTextTime < TransitionTime) {
			offset = std::powf((TransitionTime - _levelTextTime) / 12.0f, 3);
		} else if (_levelTextTime > TransitionTime + StillTime) {
			offset = -std::powf((_levelTextTime - TransitionTime - StillTime) / 12.0f, 3);
		} else {
			offset = 0;
		}

		int charOffsetShadow = charOffset;
		_smallFont->DrawString(this, _levelText, charOffsetShadow, ViewSize.X * 0.5f + offset, ViewSize.Y * 0.04f + 2.5f, FontShadowLayer,
			Alignment::Top, Colorf(0.0f, 0.0f, 0.0f, 0.3f), 1.0f, 0.72f, 0.8f, 0.8f);

		_smallFont->DrawString(this, _levelText, charOffset, ViewSize.X * 0.5f + offset, ViewSize.Y * 0.04f, FontLayer,
			Alignment::Top, Colorf::White, 1.0f, 0.72f, 0.8f, 0.8f);

		if (_levelTextTime > TotalTime) {
			_levelTextTime = -1.0f;
			_levelText = { };
		}
	}

	void HUD::DrawCoins(int& charOffset)
	{
		constexpr float StillTime = 120.0f;
		constexpr float TransitionTime = 60.0f;
		constexpr float TotalTime = StillTime + TransitionTime * 2.0f;

		if (_coinsTime < 0.0f) {
			return;
		}

		float offset, alpha;
		if (_coinsTime < TransitionTime) {
			offset = (TransitionTime - _coinsTime) / 10.0f;
			offset = -(offset * offset);
			alpha = std::max(_coinsTime / TransitionTime, 0.1f);
		} else if (_coinsTime > TransitionTime + StillTime) {
			offset = (_coinsTime - TransitionTime - StillTime) / 10.0f;
			offset = (offset * offset);
			alpha = (TotalTime - _coinsTime) / TransitionTime;
		} else {
			offset = 0.0f;
			alpha = 1.0f;
		}

		DrawElement("PickupCoin"_s, -1, ViewSize.X * 0.5f, ViewSize.Y * 0.92f + 2.5f + offset, ShadowLayer,
			Alignment::Right, Colorf(0.0f, 0.0f, 0.0f, 0.2f * alpha), 0.8f, 0.8f);
		DrawElement("PickupCoin"_s, -1, ViewSize.X * 0.5f, ViewSize.Y * 0.92f + offset, MainLayer,
			Alignment::Right, Colorf(1.0f, 1.0f, 1.0f, alpha * alpha), 0.8f, 0.8f);

		char stringBuffer[32];
		snprintf(stringBuffer, _countof(stringBuffer), "x%i", _coins);

		int charOffsetShadow = charOffset;
		_smallFont->DrawString(this, stringBuffer, charOffsetShadow, ViewSize.X * 0.5f, ViewSize.Y * 0.92f + 2.5f + offset, FontShadowLayer,
			Alignment::Left, Colorf(0.0f, 0.0f, 0.0f, 0.3f * alpha), 1.0f, 0.0f, 0.0f, 0.0f);

		_smallFont->DrawString(this, stringBuffer, charOffset, ViewSize.X * 0.5f, ViewSize.Y * 0.92f + offset, FontLayer,
			Alignment::Left, Colorf(1.0f, 1.0f, 1.0f, alpha), 1.0f, 0.0f, 0.0f, 0.0f);

		if (_coinsTime > TotalTime) {
			_coinsTime = -1.0f;
		}
	}

	void HUD::DrawGems(int& charOffset)
	{
		constexpr float StillTime = 120.0f;
		constexpr float TransitionTime = 60.0f;
		constexpr float TotalTime = StillTime + TransitionTime * 2.0f;

		if (_gemsTime < 0.0f) {
			return;
		}

		float offset, alpha;
		if (_gemsTime < TransitionTime) {
			offset = (TransitionTime - _gemsTime) / 10.0f;
			offset = -(offset * offset);
			alpha = std::max(_gemsTime / TransitionTime, 0.1f);
		} else if (_gemsTime > TransitionTime + StillTime) {
			offset = (_gemsTime - TransitionTime - StillTime) / 10.0f;
			offset = (offset * offset);
			alpha = (TotalTime - _gemsTime) / TransitionTime;
		} else {
			offset = 0.0f;
			alpha = 1.0f;
		}

		float animAlpha = alpha * alpha;
		DrawElement("PickupGem"_s, -1, ViewSize.X * 0.5f, ViewSize.Y * 0.92f + 2.5f + offset, ShadowLayer, Alignment::Right,
			Colorf(0.0f, 0.0f, 0.0f, 0.4f * animAlpha), 0.8f, 0.8f);
		DrawElement("PickupGem"_s, -1, ViewSize.X * 0.5f, ViewSize.Y * 0.92f + offset, MainLayer, Alignment::Right,
			Colorf(1.0f, 1.0f, 1.0f, animAlpha), 0.8f, 0.8f);

		char stringBuffer[32];
		snprintf(stringBuffer, _countof(stringBuffer), "x%i", _gems);

		int charOffsetShadow = charOffset;
		_smallFont->DrawString(this, stringBuffer, charOffsetShadow, ViewSize.X * 0.5f, ViewSize.Y * 0.92f + 2.5f + offset, FontShadowLayer,
			Alignment::Left, Colorf(0.0f, 0.0f, 0.0f, 0.3f * alpha), 1.0f, 0.0f, 0.0f, 0.0f);

		_smallFont->DrawString(this, stringBuffer, charOffset, ViewSize.X * 0.5f, ViewSize.Y * 0.92f + offset, FontLayer,
			Alignment::Left, Colorf(1.0f, 1.0f, 1.0f, alpha), 1.0f, 0.0f, 0.0f, 0.0f);

		if (_gemsTime > TotalTime) {
			_gemsTime = -1.0f;
		}
	}

	void HUD::DrawElement(const StringView& name, int frame, float x, float y, uint16_t z, Alignment align, const Colorf& color, float scaleX, float scaleY)
	{
		auto it = _graphics->find(String::nullTerminatedView(name));
		if (it == _graphics->end()) {
			return;
		}

		if (frame < 0) {
			frame = it->second.FrameOffset + ((int)(AnimTime * it->second.FrameCount / it->second.FrameDuration) % it->second.FrameCount);
		}

		GenericGraphicResource* base = it->second.Base;
		Vector2f size = Vector2f(base->FrameDimensions.X * scaleX, base->FrameDimensions.Y * scaleY);
		Vector2f adjustedPos = ApplyAlignment(align, Vector2f(x - ViewSize.X * 0.5f, ViewSize.Y * 0.5f - y), size);

		Vector2i texSize = base->TextureDiffuse->size();
		int col = frame % base->FrameConfiguration.X;
		int row = frame / base->FrameConfiguration.X;
		Vector4f texCoords = Vector4f(
			float(base->FrameDimensions.X) / float(texSize.X),
			float(base->FrameDimensions.X * col) / float(texSize.X),
			float(base->FrameDimensions.Y) / float(texSize.Y),
			float(base->FrameDimensions.Y * row) / float(texSize.Y)
		);

		texCoords.W += texCoords.Z;
		texCoords.Z *= -1;

		DrawTexture(*base->TextureDiffuse.get(), adjustedPos, z, size, texCoords, color);
	}

	StringView HUD::GetCurrentWeapon(Actors::Player* player, WeaponType weapon, Vector2f& offset)
	{
		if (weapon == WeaponType::Toaster && player->_inWater) {
			offset.X += 2;
			offset.Y += 2;
			return "WeaponToasterDisabled"_s;
		} else if (weapon == WeaponType::Seeker) {
			offset.X += 2;
		} else if (weapon == WeaponType::TNT) {
			offset.X += 2;
		} else if (weapon == WeaponType::Electro) {
			offset.X += 6;
		}

		if ((player->_weaponUpgrades[(int)weapon] & 0x01) != 0) {
			switch (weapon) {
				default:
				case WeaponType::Blaster:
					if (player->_playerType == PlayerType::Spaz) {
						return "WeaponPowerUpBlasterSpaz"_s;
					} else if (player->_playerType == PlayerType::Lori) {
						return "WeaponPowerUpBlasterLori"_s;
					} else {
						return "WeaponPowerUpBlasterJazz"_s;
					}

				case WeaponType::Bouncer: return "WeaponPowerUpBouncer"_s;
				case WeaponType::Freezer: return "WeaponPowerUpFreezer"_s;
				case WeaponType::Seeker: return "WeaponPowerUpSeeker"_s;
				case WeaponType::RF: return "WeaponPowerUpRF"_s;
				case WeaponType::Toaster: return "WeaponPowerUpToaster"_s;
				case WeaponType::TNT: return "WeaponPowerUpTNT"_s;
				case WeaponType::Pepper: return "WeaponPowerUpPepper"_s;
				case WeaponType::Electro: return "WeaponPowerUpElectro"_s;
				case WeaponType::Thunderbolt: return "WeaponPowerUpThunderbolt"_s;
			}
		} else {
			switch (weapon) {
				default:
				case WeaponType::Blaster:
					if (player->_playerType == PlayerType::Spaz) {
						return "WeaponBlasterSpaz"_s;
					} else if (player->_playerType == PlayerType::Lori) {
						return "WeaponBlasterLori"_s;
					} else {
						return "WeaponBlasterJazz"_s;
					}

				case WeaponType::Bouncer: return "WeaponBouncer"_s;
				case WeaponType::Freezer: return "WeaponFreezer"_s;
				case WeaponType::Seeker: return "WeaponSeeker"_s;
				case WeaponType::RF: return "WeaponRF"_s;
				case WeaponType::Toaster: return "WeaponToaster"_s;
				case WeaponType::TNT: return "WeaponTNT"_s;
				case WeaponType::Pepper: return "WeaponPepper"_s;
				case WeaponType::Electro: return "WeaponElectro"_s;
				case WeaponType::Thunderbolt: return "WeaponThunderbolt"_s;
			}
		}
	}

	HUD::TouchButtonInfo HUD::CreateTouchButton(PlayerActions action, const StringView& identifier, Alignment align, float x, float y, float w, float h)
	{
		TouchButtonInfo info;
		info.Action = action;
		info.Left = x * LevelHandler::DefaultWidth * 0.5f;
		info.Top = y * LevelHandler::DefaultWidth * 0.5f;
		info.Width = w * LevelHandler::DefaultWidth * 0.5f;
		info.Height = h * LevelHandler::DefaultWidth * 0.5f;

		if (!identifier.empty()) {
			auto it = _graphics->find(String::nullTerminatedView(identifier));
			info.Graphics = (it != _graphics->end() ? &it->second : nullptr);
		} else {
			info.Graphics = nullptr;
		}

		info.CurrentPointerId = -1;
		info.Align = align;
		return info;
	}

	bool HUD::IsOnButton(const HUD::TouchButtonInfo& button, float x, float y)
	{
		float left = button.Left;
		if ((button.Align & Alignment::Right) == Alignment::Right) { left = ViewSize.X - button.Width - left; }
		if (x < left) return false;

		float top = button.Top;
		if ((button.Align & Alignment::Bottom) == Alignment::Bottom) { top = ViewSize.Y - button.Height - top; }
		if (y < top) return false;

		float right = left + button.Width;
		if (x > right) return false;

		float bottom = top + button.Height;
		if (y > bottom) return false;

		return true;
	}
}