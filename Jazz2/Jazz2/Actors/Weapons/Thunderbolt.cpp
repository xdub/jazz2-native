﻿#include "Thunderbolt.h"
#include "../../ILevelHandler.h"
#include "../../LevelInitialization.h"
#include "../Player.h"
#include "../Enemies/EnemyBase.h"

#include "../../../nCine/Base/Random.h"

namespace Jazz2::Actors::Weapons
{
	Thunderbolt::Thunderbolt()
		:
		_hit(false),
		_lightProgress(0.0f)
	{
	}

	Task<bool> Thunderbolt::OnActivatedAsync(const ActorActivationDetails& details)
	{
		co_await ShotBase::OnActivatedAsync(details);

		_upgrades = details.Params[0];
		_initialLayer = _renderer.layer();
		_strength = 2;
		_health = INT32_MAX;
		CollisionFlags &= ~CollisionFlags::ApplyGravitation;

		co_await RequestMetadataAsync("Weapon/Thunderbolt"_s);

		SetAnimation((AnimState)(Random().NextBool() ? 1 : 0));

		co_return true;
	}

	void Thunderbolt::OnFire(const std::shared_ptr<ActorBase>& owner, Vector2f gunspotPos, Vector2f speed, float angle, bool isFacingLeft)
	{
		angle += Random().FastFloat(-0.1f, 0.1f);

		float distance = (isFacingLeft ? -140.0f : 140.0f);
		_farPoint = Vector2f(gunspotPos.X + cosf(angle) * distance, gunspotPos.Y + sinf(angle) * distance);

		_owner = owner;
		SetFacingLeft(isFacingLeft);

		MoveInstantly(gunspotPos, MoveType::Absolute | MoveType::Force);
		OnUpdateHitbox();

		if (Random().NextBool()) {
			_renderer.setFlippedY(true);
		}

		_renderer.setRotation(angle);
	}

	void Thunderbolt::OnUpdate(float timeMult)
	{
		if (_hit) {
			_strength = 0;
		} else if (_strength > 0) {
			TileCollisionParams params = { TileDestructType::Weapon | TileDestructType::IgnoreSolidTiles, false, WeaponType::Thunderbolt, _strength };
			_levelHandler->IsPositionEmpty(this, AABBInner, params);
			if (params.WeaponStrength <= 0) {
				_hit = true;
				_strength = 0;
			}
		}

		_lightProgress += timeMult * 0.123f;
		_renderer.setLayer((uint16_t)(_initialLayer - _lightProgress * 10.0f));
	}

	void Thunderbolt::OnUpdateHitbox()
	{
		constexpr float Size = 10.0f;

		if (_farPoint.X != 0.0f && _farPoint.Y != 0.0f) {
			AABBInner = AABBf(_pos, _farPoint);
			AABBInner.L -= Size;
			AABBInner.T -= Size;
			AABBInner.R += Size;
			AABBInner.B += Size;
		}
	}

	void Thunderbolt::OnEmitLights(SmallVectorImpl<LightEmitter>& lights)
	{
		constexpr int LightCount = 4;

		if (_lightProgress < fPi) {
			float lightIntensity = sinf(_lightProgress) * 0.2f;
			for (int i = -1; i <= LightCount; i++) {
				float dist = (float)i / LightCount;
				auto& light = lights.emplace_back();
				light.Pos = Vector2f(lerp(_pos.X, _farPoint.X, dist), lerp(_pos.Y, _farPoint.Y, dist));
				light.Intensity = lightIntensity;
				light.Brightness = lightIntensity * (0.1f + (1.0f - dist) * 0.4f);
				light.RadiusNear = 20.0f;
				light.RadiusFar = 100.0f;
			}
		}
	}

	void Thunderbolt::OnAnimationFinished()
	{
		ShotBase::OnAnimationFinished();

		DecreaseHealth(INT32_MAX);
	}

	bool Thunderbolt::OnHandleCollision(std::shared_ptr<ActorBase> other)
	{
		if (auto enemyBase = dynamic_cast<Enemies::EnemyBase*>(other.get())) {
			if (enemyBase->CanCollideWithAmmo) {
				_hit = true;
			}
		}

		return false;
	}

	void Thunderbolt::OnHitWall()
	{
	}

	void Thunderbolt::OnRicochet()
	{
	}
}