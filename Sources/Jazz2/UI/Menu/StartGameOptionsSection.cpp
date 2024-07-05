﻿#include "StartGameOptionsSection.h"
#include "MainMenu.h"
#include "MenuResources.h"
#include "../../PreferencesCache.h"

#include <Utf8.h>

using namespace Jazz2::UI::Menu::Resources;

namespace Jazz2::UI::Menu
{
	StartGameOptionsSection::StartGameOptionsSection(const StringView episodeName, const StringView levelName, const StringView previousEpisodeName)
		: _episodeName(episodeName), _levelName(levelName), _previousEpisodeName(previousEpisodeName), _selectedIndex(2),
			_availableCharacters(3), _selectedPlayerType(0), _selectedDifficulty(1), _lastPlayerType(0), _lastDifficulty(0),
			_imageTransition(1.0f), _animation(0.0f), _transitionTime(0.0f), _shouldStart(false)
	{
		// TRANSLATORS: Menu item to select player character (Jazz, Spaz, Lori)
		_items[(std::int32_t)Item::Character].Name = _("Character");
		// TRANSLATORS: Menu item to select difficulty
		_items[(std::int32_t)Item::Difficulty].Name = _("Difficulty");
		// TRANSLATORS: Menu item to start selected episode/level
		_items[(std::int32_t)Item::Start].Name = _("Start");
	}

	void StartGameOptionsSection::OnShow(IMenuContainer* root)
	{
		MenuSection::OnShow(root);

		_animation = 0.0f;

		if (auto* mainMenu = dynamic_cast<MainMenu*>(_root)) {
			auto* res = mainMenu->_metadata->FindAnimation(LoriExistsCheck);
			_availableCharacters = (res != nullptr ? 3 : 2);
		}
	}

	void StartGameOptionsSection::OnUpdate(float timeMult)
	{
		if (_animation < 1.0f) {
			_animation = std::min(_animation + timeMult * 0.016f, 1.0f);
		}

		if (_imageTransition < 1.0f) {
			_imageTransition += timeMult * 0.1f;

			if (_imageTransition > 1.0f) {
				_imageTransition = 1.0f;
			}
		}

		if (!_shouldStart) {
			if (_root->ActionHit(PlayerActions::Fire)) {
				ExecuteSelected();
			} else if (_root->ActionHit(PlayerActions::Left)) {
				if (_selectedIndex == 0) {
					if (_selectedPlayerType > 0) {
						StartImageTransition();
						_selectedPlayerType--;
					} else {
						StartImageTransition();
						_selectedPlayerType = _availableCharacters - 1;
					}
					_root->PlaySfx("MenuSelect"_s, 0.5f);
				} else if (_selectedIndex == 1) {
					if (_selectedDifficulty > 0) {
						StartImageTransition();
						_selectedDifficulty--;
						_root->PlaySfx("MenuSelect"_s, 0.5f);
					}
				}
			} else if (_root->ActionHit(PlayerActions::Right)) {
				if (_selectedIndex == 0) {
					if (_selectedPlayerType < _availableCharacters - 1) {
						StartImageTransition();
						_selectedPlayerType++;
					} else {
						StartImageTransition();
						_selectedPlayerType = 0;
					}
					_root->PlaySfx("MenuSelect"_s, 0.5f);
				} else if (_selectedIndex == 1) {
					if (_selectedDifficulty < 3 - 1) {
						StartImageTransition();
						_selectedDifficulty++;
						_root->PlaySfx("MenuSelect"_s, 0.4f);
					}
				}
			} else if (_root->ActionHit(PlayerActions::Up)) {
				_root->PlaySfx("MenuSelect"_s, 0.5f);
				_animation = 0.0f;
				if (_selectedIndex > 0) {
					_selectedIndex--;
				} else {
					_selectedIndex = (int32_t)Item::Count - 1;
				}
			} else if (_root->ActionHit(PlayerActions::Down)) {
				_root->PlaySfx("MenuSelect"_s, 0.5f);
				_animation = 0.0f;
				if (_selectedIndex < (int32_t)Item::Count - 1) {
					_selectedIndex++;
				} else {
					_selectedIndex = 0;
				}
			} else if (_root->ActionHit(PlayerActions::Menu)) {
				_root->PlaySfx("MenuSelect"_s, 0.6f);
				_root->LeaveSection();
			}
		} else {
			_transitionTime -= 0.025f * timeMult;
			if (_transitionTime <= 0.0f) {
				OnAfterTransition();
			}
		}
	}

	void StartGameOptionsSection::OnDraw(Canvas* canvas)
	{
		Recti contentBounds = _root->GetContentBounds();
		Vector2f center = Vector2f(contentBounds.X + contentBounds.W * 0.5f, contentBounds.Y + contentBounds.H * 0.5f * 0.33f);

		AnimState selectedDifficultyImage;
		switch (_selectedPlayerType) {
			default:
			case 0: selectedDifficultyImage = MenuDifficultyJazz; break;
			case 1: selectedDifficultyImage = MenuDifficultySpaz; break;
			case 2: selectedDifficultyImage = MenuDifficultyLori; break;
		}

		float imageScale = (contentBounds.W >= 400 ? 1.0f : 0.5f);
		float imageOffsetX = center.X * (contentBounds.W >= 400 ? 0.36f : 0.32f);
		float imageOffsetY = center.Y * (contentBounds.W >= 400 ? 1.4f : 1.06f);
		_root->DrawElement(MenuDim, 0, imageOffsetX, imageOffsetY, IMenuContainer::ShadowLayer - 2, Alignment::Center, Colorf::White, 24.0f * imageScale, 36.0f * imageScale);
		_root->DrawElement(selectedDifficultyImage, _selectedDifficulty, imageOffsetX, imageOffsetY + 3.0f, IMenuContainer::ShadowLayer, Alignment::Center, Colorf(0.0f, 0.0f, 0.0f, 0.2f * _imageTransition), 0.88f * imageScale, 0.88f * imageScale);

		if (_imageTransition < 1.0f) {
			AnimState lastDifficultyImage;
			switch (_lastPlayerType) {
				default:
				case 0: lastDifficultyImage = MenuDifficultyJazz; break;
				case 1: lastDifficultyImage = MenuDifficultySpaz; break;
				case 2: lastDifficultyImage = MenuDifficultyLori; break;
			}
			_root->DrawElement(lastDifficultyImage, _lastDifficulty, imageOffsetX, imageOffsetY, IMenuContainer::MainLayer, Alignment::Center, Colorf(1.0f, 1.0f, 1.0f, 1.0f - _imageTransition), 0.88f * imageScale, 0.88f * imageScale);
		}

		_root->DrawElement(selectedDifficultyImage, _selectedDifficulty, imageOffsetX, imageOffsetY, IMenuContainer::MainLayer, Alignment::Center, Colorf(1.0f, 1.0f, 1.0f, _imageTransition), 0.88f * imageScale, 0.88f * imageScale);

		std::int32_t charOffset = 0;
		for (std::int32_t i = 0; i < (std::int32_t)Item::Count; i++) {
		    if (_selectedIndex == i) {
		        float size = 0.5f + IMenuContainer::EaseOutElastic(_animation) * 0.6f;

		        _root->DrawElement(MenuGlow, 0, center.X, center.Y, IMenuContainer::MainLayer, Alignment::Center, Colorf(1.0f, 1.0f, 1.0f, 0.4f * size), (Utf8::GetLength(_items[i].Name) + 3) * 0.5f * size, 4.0f * size, true, true);

		        _root->DrawStringShadow(_items[i].Name, charOffset, center.X, center.Y, IMenuContainer::FontLayer + 10,
		            Alignment::Center, Font::RandomColor, size, 0.7f, 1.1f, 1.1f, 0.4f, 0.9f);
		    } else {
		        _root->DrawStringShadow(_items[i].Name, charOffset, center.X, center.Y, IMenuContainer::FontLayer,
					Alignment::Center, Font::DefaultColor, 0.9f);
		    }

		    if (i == 0) {
		        static const StringView playerTypes[] = { "Jazz"_s, "Spaz"_s, "Lori"_s };
		        static const Colorf playerColors[] = {
		            Colorf(0.2f, 0.45f, 0.2f, 0.5f),
		            Colorf(0.45f, 0.27f, 0.22f, 0.5f),
		            Colorf(0.5f, 0.45f, 0.22f, 0.5f)
		        };

		        float offset, spacing;
		        if (_availableCharacters == 1) {
		            offset = 0.0f;
		            spacing = 0.0f;
		        } else if (_availableCharacters == 2) {
		            offset = 50.0f;
		            spacing = 100.0f;
		        } else {
		            offset = 100.0f;
		            spacing = 300.0f / _availableCharacters;
		        }

				if (contentBounds.W < 480) {
					offset *= 0.7f;
					spacing *= 0.7f;
				}

		        for (std::int32_t j = 0; j < _availableCharacters; j++) {
		            float x = center.X - offset + j * spacing;
		            if (_selectedPlayerType == j) {
		                _root->DrawElement(MenuGlow, 0, x, center.Y + 28.0f, IMenuContainer::MainLayer, Alignment::Center, Colorf(1.0f, 1.0f, 1.0f, 0.2f), (Utf8::GetLength(playerTypes[j]) + 3) * 0.4f, 2.2f, true, true);

		                _root->DrawStringShadow(playerTypes[j], charOffset, x, center.Y + 28.0f, IMenuContainer::FontLayer,
							Alignment::Center, playerColors[j], 1.0f, 0.4f, 0.9f, 0.9f, 0.8f, 0.9f);
		            } else {
		                _root->DrawStringShadow(playerTypes[j], charOffset, x, center.Y + 28.0f, IMenuContainer::FontLayer,
							Alignment::Center, Font::DefaultColor, 0.8f, 0.0f, 4.0f, 4.0f, 0.4f, 0.9f);
		            }
		        }

		        _root->DrawStringShadow("<"_s, charOffset, center.X - (100.0f + 40.0f), center.Y + 28.0f, IMenuContainer::FontLayer,
					Alignment::Center, Font::DefaultColor, 0.7f);
		        _root->DrawStringShadow(">"_s, charOffset, center.X + (100.0f + 40.0f), center.Y + 28.0f, IMenuContainer::FontLayer,
					Alignment::Center, Font::DefaultColor, 0.7f);

				_items[i].TouchY = center.Y + 28.0f;
		    } else if (i == 1) {
				static const StringView difficultyTypes[] = { _("Easy"), _("Medium"), _("Hard") };
				float spacing = (contentBounds.W >= 400 ? 100.0f : 70.0f);

		        for (std::int32_t j = 0; j < static_cast<std::int32_t>(arraySize(difficultyTypes)); j++) {
		            if (_selectedDifficulty == j) {
		                _root->DrawElement(MenuGlow, 0, center.X + (j - 1) * spacing, center.Y + 28.0f, IMenuContainer::MainLayer, Alignment::Center, Colorf(1.0f, 1.0f, 1.0f, 0.2f), (Utf8::GetLength(difficultyTypes[j]) + 3) * 0.4f, 2.2f, true, true);

		                _root->DrawStringShadow(difficultyTypes[j], charOffset, center.X + (j - 1) * spacing, center.Y + 28.0f, IMenuContainer::FontLayer,
							Alignment::Center, Colorf(0.45f, 0.45f, 0.45f, 0.5f), 1.0f, 0.4f, 0.9f, 0.9f, 0.8f, 0.9f);
		            } else {
		                _root->DrawStringShadow(difficultyTypes[j], charOffset, center.X + (j - 1) * spacing, center.Y + 28.0f, IMenuContainer::FontLayer,
							Alignment::Center, Font::DefaultColor, 0.8f, 0.0f, 4.0f, 4.0f, 0.9f);
		            }
		        }

		        _root->DrawStringShadow("<"_s, charOffset, center.X - (100.0f + 40.0f), center.Y + 28.0f, IMenuContainer::FontLayer,
					Alignment::Center, Font::DefaultColor, 0.7f);
		        _root->DrawStringShadow(">"_s, charOffset, center.X + (100.0f + 40.0f), center.Y + 28.0f, IMenuContainer::FontLayer,
					Alignment::Center, Font::DefaultColor, 0.7f);

				_items[i].TouchY = center.Y + 28.0f;
			} else {
				_items[i].TouchY = center.Y;
			}

		    center.Y += (contentBounds.H >= 250 ? 70.0f : 60.0f);
		}
	}

	void StartGameOptionsSection::OnDrawOverlay(Canvas* canvas)
	{
		if (_shouldStart) {
			auto command = canvas->RentRenderCommand();
			if (command->material().setShader(ContentResolver::Get().GetShader(PrecompiledShader::Transition))) {
				command->material().reserveUniformsDataMemory();
				command->geometry().setDrawParameters(GL_TRIANGLE_STRIP, 0, 4);
			}

			command->material().setBlendingFactors(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			auto instanceBlock = command->material().uniformBlock(Material::InstanceBlockName);
			instanceBlock->uniform(Material::TexRectUniformName)->setFloatVector(Vector4f(1.0f, 0.0f, 1.0f, 0.0f).Data());
			instanceBlock->uniform(Material::SpriteSizeUniformName)->setFloatVector(Vector2f(static_cast<float>(canvas->ViewSize.X), static_cast<float>(canvas->ViewSize.Y)).Data());
			instanceBlock->uniform(Material::ColorUniformName)->setFloatVector(Colorf(0.0f, 0.0f, 0.0f, _transitionTime).Data());

			command->setTransformation(Matrix4x4f::Identity);
			command->setLayer(999);

			canvas->DrawRenderCommand(command);
		}
	}

	void StartGameOptionsSection::OnTouchEvent(const nCine::TouchEvent& event, const Vector2i& viewSize)
	{
		if (!_shouldStart && event.type == TouchEventType::Down) {
			std::int32_t pointerIndex = event.findPointerIndex(event.actionIndex);
			if (pointerIndex != -1) {
				float x = event.pointers[pointerIndex].x * (float)viewSize.X;
				float y = event.pointers[pointerIndex].y * (float)viewSize.Y;
				float halfWidth = viewSize.X * 0.5f;

				if (y < 80.0f) {
					_root->PlaySfx("MenuSelect"_s, 0.5f);
					_root->LeaveSection();
					return;
				}

				for (std::int32_t i = 0; i < (std::int32_t)Item::Count; i++) {
					if (std::abs(x - halfWidth) < 150.0f && std::abs(y - _items[i].TouchY) < 30.0f) {
						switch (i) {
							case 0: {
								std::int32_t selectedSubitem = (x < halfWidth - 50.0f ? 0 : (x > halfWidth + 50.0f ? 2 : 1));
								if (_selectedPlayerType != selectedSubitem) {
									StartImageTransition();
									_selectedPlayerType = selectedSubitem;
									_root->PlaySfx("MenuSelect"_s, 0.5f);
								}
								break;
							}
							case 1: {
								std::int32_t selectedSubitem = (x < halfWidth - 50.0f ? 0 : (x > halfWidth + 50.0f ? 2 : 1));
								if (_selectedDifficulty != selectedSubitem) {
									StartImageTransition();
									_selectedDifficulty = selectedSubitem;
									_root->PlaySfx("MenuSelect"_s, 0.5f);
								}
								break;
							}
							default: {
								if (_selectedIndex == i) {
									ExecuteSelected();
								} else {
									_root->PlaySfx("MenuSelect"_s, 0.5f);
									_animation = 0.0f;
									_selectedIndex = i;
								}
								break;
							}
						}
						break;
					}
				}
			}
		}
	}

	void StartGameOptionsSection::ExecuteSelected()
	{
		if (_selectedIndex == 2) {
			_root->PlaySfx("MenuSelect"_s, 0.6f);

			_shouldStart = true;
			_transitionTime = 1.0f;
		}
	}

	void StartGameOptionsSection::OnAfterTransition()
	{
		bool playTutorial = (!PreferencesCache::TutorialCompleted && _episodeName == "prince"_s && _levelName == "01_castle1"_s);

		PlayerType players[] = { (PlayerType)((std::int32_t)PlayerType::Jazz + _selectedPlayerType) };
		LevelInitialization levelInit(_episodeName, (playTutorial ? "trainer"_s : StringView(_levelName)), (GameDifficulty)((std::int32_t)GameDifficulty::Easy + _selectedDifficulty),
			PreferencesCache::EnableReforgedGameplay, false, players, static_cast<std::int32_t>(arraySize(players)));

		if (!_previousEpisodeName.empty()) {
			auto previousEpisodeEnd = PreferencesCache::GetEpisodeEnd(_previousEpisodeName);
			if (previousEpisodeEnd != nullptr) {
				// Set CheatsUsed to true if cheats were used in the previous episode or the previous episode is not completed
				if ((previousEpisodeEnd->Flags & EpisodeContinuationFlags::CheatsUsed) == EpisodeContinuationFlags::CheatsUsed ||
					(previousEpisodeEnd->Flags & EpisodeContinuationFlags::IsCompleted) != EpisodeContinuationFlags::IsCompleted) {
					levelInit.CheatsUsed = true;
				}

				auto& firstPlayer = levelInit.PlayerCarryOvers[0];
				if (previousEpisodeEnd->Lives > 0) {
					firstPlayer.Lives = previousEpisodeEnd->Lives;
				}
				firstPlayer.Score = previousEpisodeEnd->Score;
				std::memcpy(firstPlayer.Ammo, previousEpisodeEnd->Ammo, sizeof(levelInit.PlayerCarryOvers[0].Ammo));
				std::memcpy(firstPlayer.WeaponUpgrades, previousEpisodeEnd->WeaponUpgrades, sizeof(levelInit.PlayerCarryOvers[0].WeaponUpgrades));
			} else {
				// Set CheatsUsed to true if the previous episode is not completed
				levelInit.CheatsUsed = true;
			}
		}

		if (PreferencesCache::EnableReforgedGameplay && _levelName == "01_xmas1"_s) {
			levelInit.LastExitType = ExitType::Warp | ExitType::Frozen;
		}

		if (PreferencesCache::AllowCheatsLives) {
			levelInit.PlayerCarryOvers[0].Lives = UINT8_MAX;
			levelInit.CheatsUsed = true;
		}

		_root->ChangeLevel(std::move(levelInit));
	}

	void StartGameOptionsSection::StartImageTransition()
	{
		_lastPlayerType = _selectedPlayerType;
		_lastDifficulty = _selectedDifficulty;
		_imageTransition = 0.0f;
	}
}