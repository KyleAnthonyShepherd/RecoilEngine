/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef BUILD_ACTIONS_H
#define BUILD_ACTIONS_H

class CUnit;
class CFeature;
struct UnitDef;

/**
 * Shared build-action helpers used by both CBuilder and CBuildPowerWeapon.
 * These contain the synced game logic (formulas, resource costs, event checks)
 * but NOT the caller-specific bookkeeping (range checks, command queue, visuals,
 * StopBuild, nanoPieceCache, BuilderCaches, etc.).
 */
namespace BuildActions {

	enum class CaptureResult {
		InProgress,   // capture step succeeded but not yet complete
		Completed,    // capture finished, target team changed
		Failed,       // ChangeTeam failed (unit limit)
		Blocked       // energy/event check prevented this step
	};

	/**
	 * Apply one frame of capture progress to @p target.
	 *
	 * @param capturer     the unit performing the capture
	 * @param target       the enemy unit being captured
	 * @param captureSpeed the capturer's effective capture speed (pre-scaled by INV_GAME_SPEED)
	 * @return result indicating what happened
	 *
	 * Handles: magic-number formula, energy cost, AllowUnitBuildStep/AllowUnitCaptureStep
	 * events, progress increment, and ChangeTeam on completion.
	 * Does NOT handle: range checks, visuals, command queue, StopBuild.
	 */
	CaptureResult ApplyCaptureStep(CUnit* capturer, CUnit* target, float captureSpeed);


	enum class ResurrectResult {
		Restoring,    // feature was partially reclaimed, restoring resources first
		InProgress,   // resurrect step succeeded but not yet complete
		Completed,    // resurrect finished, new unit spawned (returned via outUnit)
		Blocked,      // energy/event check prevented this step
		InvalidTarget // feature has no udef (can't be resurrected)
	};

	/**
	 * Apply one frame of resurrect progress to @p target.
	 *
	 * @param resurrector   the unit performing the resurrection
	 * @param target        the feature corpse being resurrected
	 * @param resurrectSpeed the resurrector's effective resurrect speed (pre-scaled by INV_GAME_SPEED)
	 * @param[out] outUnit  if Completed, the newly spawned unit (otherwise nullptr)
	 * @return result indicating what happened
	 *
	 * Handles: restore-before-resurrect, energy cost, AllowFeatureBuildStep event,
	 * progress increment, UnBlock, LoadUnit, SetSoloBuilder, SetHeading, DeleteFeature.
	 * Does NOT handle: range checks, visuals, command queue, StopBuild, BuilderCaches.
	 */
	ResurrectResult ApplyResurrectStep(CUnit* resurrector, CFeature* target, float resurrectSpeed, CUnit** outUnit);

}

#endif // BUILD_ACTIONS_H