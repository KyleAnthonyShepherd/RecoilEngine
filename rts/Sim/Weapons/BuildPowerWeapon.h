/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef BUILDPOWER_WEAPON_H
#define BUILDPOWER_WEAPON_H

#include "Sim/Weapons/Weapon.h"

class CFeature;
class CSolidObject;

class CBuildPowerWeapon : public CWeapon
{
	CR_DECLARE_DERIVED(CBuildPowerWeapon)

public:
	CBuildPowerWeapon(CUnit* owner = nullptr, const WeaponDef* def = nullptr);

	void Init() override;
	void Update() override;
	bool CanFire(bool ignoreAngleGood, bool ignoreTargetType, bool ignoreRequestedDir) const override;

	void DependentDied(CObject* o) override;

	/// What this weapon can do to a given target
	enum {
		BP_None,
		BP_Repair,       // heal damaged friendly
		BP_Build,        // assist construction of friendly
		BP_Reclaim,      // reclaim unit or feature
		BP_Capture,      // capture enemy unit
		BP_Resurrect     // resurrect feature corpse
	};

	/// Set the weapon's target and action. Called by CommandAI or Lua (via synced API).
	void SetBuildPowerCommand(CUnit* target, int action);
	void SetBuildPowerCommand(CFeature* target, int action);
	void ClearBuildPowerCommand();

	/// Find the first BuildPower weapon on a unit, or nullptr
	static CBuildPowerWeapon* GetBuildPowerWeapon(CUnit* unit);

	/// Query what action this weapon would perform on a given target
	int DetermineActionUnit(CUnit* target) const;
	int DetermineActionFeature(CFeature* target) const;

	/// Feature we are currently operating on (resurrect/reclaim)
	/// Tracked separately because SWeaponTarget doesn't support features
	CFeature* curFeatureTarget = nullptr;

	/// Current action being performed
	int currentAction = BP_None;

	/// Action set by CommandAI or Lua
	int commandedAction = BP_None;

	/// Per-instance build power speeds (initialized from weapondef, mutable by Lua)
	float repairSpeed = 0.0f;
	float buildSpeed = 0.0f;
	float reclaimSpeed = 0.0f;
	float captureSpeed = 0.0f;
	float resurrectSpeed = 0.0f;

private:
	void FireImpl(const bool scriptCall) override;

	/// Apply the build-power effect for this frame
	void ApplyBuildPowerUnit(CUnit* target, int action);
	void ApplyBuildPowerFeature(CFeature* target, int action);

	/// Create nano-particle visual from weapon muzzle to target
	void CreateBuildPowerVisual(const float3& targetPos, float radius, bool inverse, CSolidObject* trackTarget = nullptr);

	/// Set the feature target (tracked separately from CWeapon::currentTarget)
	void SetFeatureTarget(CFeature* f);
	void ClearFeatureTarget();

	float GetPredictedImpactTime(const float3& p) const override { return 0.0f; };
};

#endif // BUILDPOWER_WEAPON_H