/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef WEAPON_PROJECTILE_H
#define WEAPON_PROJECTILE_H

#include "Sim/Projectiles/Projectile.h"
#include "Sim/Projectiles/ProjectileParams.h" // easier to include this here
#include "Sim/Weapons/WeaponDef.h"
#include "WeaponProjectileTypes.h"

struct ProjectileParams;
class CVertexArray;
class DynDamageArray;

/**
 * Base class for all projectiles originating from a weapon or having
 * weapon-properties. Uses data from a weapon definition.
 */
class CWeaponProjectile : public CProjectile
{
	CR_DECLARE_DERIVED(CWeaponProjectile)
public:
	CWeaponProjectile(const ProjectileParams& params);
	~CWeaponProjectile() override;

	virtual void Explode(CUnit* hitUnit, CFeature* hitFeature, CWeapon* hitWeapon, float3 impactPos, float3 impactDir);
	void Collision() override;
	void Collision(CFeature* feature) override;
	void Collision(CUnit* unit) override;
	void Collision(CWeapon* weapon) override;
	void Update() override;

	/// @return 0=unaffected, 1=instant repulse, 2=gradual repulse
	virtual int ShieldRepulse(const float3& shieldPos, float shieldForce, float shieldMaxSpeed) { return 0; }

	void DrawOnMinimap() const override;

	// Why is this here? Here is why:
	// ProjectileCreated(id) issues Spring.SpawnExplosion(), but the projectile(id) construction
	// is not complete yet (only CWeaponProjectile constructor has managed to complete its task)
	// "if (projectileHandler.GetParticleSaturation() < 1.0f)" is getting called as part of
	// SpawnExplosion() flow and it cannot reach GetProjectilesCount() of derived classes, because
	// their constructor is not done yet, thus this workaround
	int GetProjectilesCount() const override { 	return 1; }

	void DependentDied(CObject* o) override;
	void PostLoad();

	void SetTargetObject(CWorldObject* newTarget) {
		if (newTarget != nullptr)
			targetPos = newTarget->pos;

		target = newTarget;
	}

	const CWorldObject* GetTargetObject() const { return target; }
	      CWorldObject* GetTargetObject()       { return target; }

	const WeaponDef* GetWeaponDef() const { return weaponDef; }

	int GetTimeToLive() const { return ttl; }
	void SetTimeToLive(int newTTL) { ttl = newTTL; }

	void SetStartPos(const float3& newStartPos) { startPos = newStartPos; }
	void SetTargetPos(const float3& newTargetPos) { targetPos = newTargetPos; }

	const float3& GetStartPos() const { return startPos; }
	const float3& GetTargetPos() const { return targetPos; }

	void SetBeingIntercepted(bool b) { targeted = b; }
	bool IsBeingIntercepted() const { return targeted; }
	bool CanBeInterceptedBy(const WeaponDef*) const;
	bool HasScheduledBounce() const { return bounced; }
	bool TraveledRange() const { return ((pos - startPos).SqLength() > (myrange * myrange)); }

	const DynDamageArray* damages;

protected:
	CWeaponProjectile() { }
	void UpdateInterception();
	virtual void UpdateGroundBounce();

protected:
	const WeaponDef* weaponDef;

	CWorldObject* target;

	unsigned int weaponNum;

	int ttl;
	int bounces;

	/// true if we are an interceptable projectile
	// and an interceptor projectile is on the way
	bool targeted;
	bool bounced;

	float3 startPos;
	float3 targetPos;

	float3 bounceHitPos;
	float3 bounceParams;
};

#endif /* WEAPON_PROJECTILE_H */