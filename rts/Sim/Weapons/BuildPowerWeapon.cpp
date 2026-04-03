/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "BuildPowerWeapon.h"

#include "WeaponDef.h"
#include "WeaponMemPool.h"

#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Misc/BuildActions.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"

#include "System/Misc/TracyDefs.h"


CR_BIND_DERIVED(CBuildPowerWeapon, CWeapon, )
CR_REG_METADATA(CBuildPowerWeapon, (
	CR_MEMBER(curFeatureTarget),
	CR_MEMBER(currentAction),
	CR_MEMBER(commandedAction),
	CR_MEMBER(repairSpeed),
	CR_MEMBER(buildSpeed),
	CR_MEMBER(reclaimSpeed),
	CR_MEMBER(captureSpeed),
	CR_MEMBER(resurrectSpeed)
))


CBuildPowerWeapon::CBuildPowerWeapon(CUnit* owner, const WeaponDef* def)
	: CWeapon(owner, def)
{
}


void CBuildPowerWeapon::Init()
{
	CWeapon::Init();

	repairSpeed    = weaponDef->bpRepairSpeed;
	buildSpeed     = weaponDef->bpBuildSpeed;
	reclaimSpeed   = weaponDef->bpReclaimSpeed;
	captureSpeed   = weaponDef->bpCaptureSpeed;
	resurrectSpeed = weaponDef->bpResurrectSpeed;
}


// ============================================================================
// Feature target tracking (features don't fit SWeaponTarget)
// ============================================================================

void CBuildPowerWeapon::SetFeatureTarget(CFeature* f)
{
	if (curFeatureTarget == f)
		return;

	ClearFeatureTarget();

	curFeatureTarget = f;

	if (f != nullptr)
		AddDeathDependence(f, DEPENDENCE_WEAPONTARGET);
}

void CBuildPowerWeapon::ClearFeatureTarget()
{
	if (curFeatureTarget != nullptr) {
		DeleteDeathDependence(curFeatureTarget, DEPENDENCE_WEAPONTARGET);
		curFeatureTarget = nullptr;
	}

	currentAction = BP_None;
}


// ============================================================================
// CommandAI interface — set/clear user-commanded build power action
// ============================================================================

void CBuildPowerWeapon::SetBuildPowerCommand(CUnit* target, int action)
{
	ClearFeatureTarget();
	commandedAction = action;
	SetAttackTarget(SWeaponTarget(target, true));
}

void CBuildPowerWeapon::SetBuildPowerCommand(CFeature* target, int action)
{
	DropCurrentTarget();
	commandedAction = action;
	SetFeatureTarget(target);
}

void CBuildPowerWeapon::ClearBuildPowerCommand()
{
	commandedAction = BP_None;
	DropCurrentTarget();
	ClearFeatureTarget();
}

CBuildPowerWeapon* CBuildPowerWeapon::GetBuildPowerWeapon(CUnit* unit)
{
	for (CWeapon* w : unit->weapons) {
		if (w->weaponDef->IsBuildPowerWeapon())
			return static_cast<CBuildPowerWeapon*>(w);
	}
	return nullptr;
}


void CBuildPowerWeapon::DependentDied(CObject* o)
{
	if (o == curFeatureTarget) {
		curFeatureTarget = nullptr;
		currentAction = BP_None;
	}

	CWeapon::DependentDied(o);
}


// ============================================================================
// Action determination
// ============================================================================

int CBuildPowerWeapon::DetermineActionUnit(CUnit* target) const
{
	if (target == nullptr || target->isDead)
		return BP_None;

	// Only act on explicit commands from CommandAI or Lua.
	// Behavior decisions are made gameside, not by the engine.
	switch (commandedAction) {
		case BP_Repair:    return (weaponDef->bpCanRepair)  ? BP_Repair  : BP_None;
		case BP_Build:     return (weaponDef->bpCanBuild)   ? BP_Build   : BP_None;
		case BP_Reclaim:   return (weaponDef->bpCanReclaim) ? BP_Reclaim : BP_None;
		case BP_Capture:   return (weaponDef->bpCanCapture) ? BP_Capture : BP_None;
		default:           return BP_None;
	}
}

int CBuildPowerWeapon::DetermineActionFeature(CFeature* target) const
{
	if (target == nullptr || target->deleteMe)
		return BP_None;

	// Only act on explicit commands from CommandAI or Lua.
	// Behavior decisions are made gameside, not by the engine.
	switch (commandedAction) {
		case BP_Resurrect: return (weaponDef->bpCanResurrect) ? BP_Resurrect : BP_None;
		case BP_Reclaim:   return (weaponDef->bpCanReclaim)   ? BP_Reclaim   : BP_None;
		default:           return BP_None;
	}
}


// ============================================================================
// Build power application
// ============================================================================

void CBuildPowerWeapon::ApplyBuildPowerUnit(CUnit* target, int action)
{
	RECOIL_DETAILED_TRACY_ZONE;

	switch (action) {
		case BP_Repair: {
			const float speed = repairSpeed;

			// Respect maxRepairSpeed like CBuilder does
			const float adjSpeed = std::min(speed, owner->unitDef->maxRepairSpeed * 0.5f - target->repairAmount);
			if (adjSpeed <= 0.0f)
				break;

			if (target->AddBuildPower(owner, adjSpeed))
				CreateBuildPowerVisual(target->midPos, target->radius * 0.5f, false);
		} break;

		case BP_Build: {
			const float speed = buildSpeed;

			if (target->AddBuildPower(owner, speed))
				CreateBuildPowerVisual(target->midPos, target->radius * 0.5f, false);
		} break;

		case BP_Reclaim: {
			const float speed = reclaimSpeed;

			if (target->AddBuildPower(owner, -speed))
				CreateBuildPowerVisual(target->midPos, target->radius * 0.7f, true);
		} break;

		case BP_Capture: {
			const float captSpeed = captureSpeed;

			if (target->team == owner->team)
				break;

			const auto result = BuildActions::ApplyCaptureStep(owner, target, captSpeed);

			if (result == BuildActions::CaptureResult::InProgress || result == BuildActions::CaptureResult::Completed)
				CreateBuildPowerVisual(target->midPos, target->radius * 0.7f, false);
		} break;

		default: break;
	}
}

void CBuildPowerWeapon::ApplyBuildPowerFeature(CFeature* target, int action)
{
	RECOIL_DETAILED_TRACY_ZONE;

	switch (action) {
		case BP_Reclaim: {
			const float speed = reclaimSpeed;

			if (target->AddBuildPower(owner, -speed))
				CreateBuildPowerVisual(target->midPos, target->radius * 0.7f, true);

			// Feature may have been deleted by AddBuildPower
			if (target->deleteMe)
				ClearFeatureTarget();
		} break;

		case BP_Resurrect: {
			const float speed = resurrectSpeed;

			if (target->udef == nullptr) {
				ClearFeatureTarget();
				break;
			}

			CUnit* spawnedUnit = nullptr;
			const auto result = BuildActions::ApplyResurrectStep(owner, target, speed, &spawnedUnit);

			switch (result) {
				case BuildActions::ResurrectResult::Restoring:
					CreateBuildPowerVisual(target->midPos, target->radius * 0.7f, false);
					break;
				case BuildActions::ResurrectResult::InProgress:
					CreateBuildPowerVisual(target->midPos, target->radius * 0.7f, (gsRNG.NextInt(2) != 0));
					break;
				case BuildActions::ResurrectResult::Completed:
					ClearFeatureTarget();
					break;
				case BuildActions::ResurrectResult::InvalidTarget:
					ClearFeatureTarget();
					break;
				default:
					break;
			}
		} break;

		default: break;
	}
}


// ============================================================================
// Nano-particle visual from weapon muzzle
// ============================================================================

void CBuildPowerWeapon::CreateBuildPowerVisual(const float3& targetPos, float radius, bool inverse)
{
	projectileHandler.AddNanoParticle(weaponMuzzlePos, targetPos, owner->unitDef, owner->team, radius, inverse, false);
}


// ============================================================================
// Update — only needed to keep currentTargetPos in sync for feature targets.
// All firing logic goes through the base Update -> UpdateFire -> Fire -> FireImpl chain.
// ============================================================================

void CBuildPowerWeapon::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;

	// The base Update sets currentTargetPos = GetLeadTargetPos(currentTarget),
	// but that only works for unit targets. For features, keep it in sync manually.
	if (curFeatureTarget != nullptr) {
		if (curFeatureTarget->deleteMe) {
			ClearFeatureTarget();
		} else {
			currentTargetPos = curFeatureTarget->midPos;
		}
	}

	CWeapon::Update();
}


// ============================================================================
// CanFire — base version rejects firing when HaveTarget() is false.
// We also consider a feature target as valid.
// ============================================================================

bool CBuildPowerWeapon::CanFire(bool ignoreAngleGood, bool ignoreTargetType, bool ignoreRequestedDir) const
{
	// treat feature target same as having a unit target
	if (!ignoreTargetType && !HaveTarget() && curFeatureTarget == nullptr)
		return false;

	return CWeapon::CanFire(ignoreAngleGood, true, ignoreRequestedDir);
}


// ============================================================================
// FireImpl — called by base Fire() when all checks pass (angleGood, reload,
// resources, CobBlockShot, etc.). This is where build power is applied.
// ============================================================================

void CBuildPowerWeapon::FireImpl(const bool scriptCall)
{
	RECOIL_DETAILED_TRACY_ZONE;

	// Apply to unit target
	if (currentTarget.type == Target_Unit && currentTarget.unit != nullptr) {
		CUnit* target = currentTarget.unit;
		const int action = DetermineActionUnit(target);

		if (action != BP_None) {
			currentAction = action;
			ApplyBuildPowerUnit(target, action);
		}
	}
	// Apply to feature target
	else if (curFeatureTarget != nullptr) {
		const int action = DetermineActionFeature(curFeatureTarget);

		if (action != BP_None) {
			currentAction = action;
			ApplyBuildPowerFeature(curFeatureTarget, action);
		}
	}
}