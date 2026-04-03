/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "BuildActions.h"

#include "Game/GlobalUnsynced.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/ModInfo.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitLoader.h"
#include "System/EventHandler.h"
#include "System/Log/ILog.h"

#include "System/Misc/TracyDefs.h"


namespace BuildActions {

CaptureResult ApplyCaptureStep(CUnit* capturer, CUnit* target, float captureSpeed)
{
	RECOIL_DETAILED_TRACY_ZONE;

	const float captureMagicNumber = (150.0f + (target->buildTime / captureSpeed) * (target->health + target->maxHealth) / target->maxHealth * 0.4f);
	const float captureProgressStep = 1.0f / captureMagicNumber;
	const float captureProgressTemp = std::min(target->captureProgress + captureProgressStep, 1.0f);

	const float captureFraction = captureProgressTemp - target->captureProgress;
	const float energyUseScaled = target->cost.energy * captureFraction * modInfo.captureCostFactor.energy;

	const bool buildStepAllowed = eventHandler.AllowUnitBuildStep(capturer, target, captureProgressStep);
	const bool captureStepAllowed = eventHandler.AllowUnitCaptureStep(capturer, target, captureProgressStep);
	const bool canExecCapture = (buildStepAllowed && captureStepAllowed && capturer->UseEnergy(energyUseScaled));

	if (!canExecCapture)
		return CaptureResult::Blocked;

	target->captureProgress += captureProgressStep;
	target->captureProgress = std::min(target->captureProgress, 1.0f);

	if (target->captureProgress < 1.0f)
		return CaptureResult::InProgress;

	// Capture complete
	if (!target->ChangeTeam(capturer->team, CUnit::ChangeCaptured)) {
		if (capturer->team == gu->myTeam) {
			LOG_L(L_WARNING, "%s: Capture failed, unit type limit reached", capturer->unitDef->humanName.c_str());
			eventHandler.LastMessagePosition(capturer->pos);
		}

		target->captureProgress = 0.0f;
		return CaptureResult::Failed;
	}

	target->captureProgress = 0.0f;
	return CaptureResult::Completed;
}


ResurrectResult ApplyResurrectStep(CUnit* resurrector, CFeature* target, float resurrectSpeed, CUnit** outUnit)
{
	RECOIL_DETAILED_TRACY_ZONE;

	*outUnit = nullptr;

	if (target->udef == nullptr)
		return ResurrectResult::InvalidTarget;

	// If partially reclaimed, need to restore resources before resurrection
	if ((modInfo.reclaimMethod != 1) && (target->reclaimLeft < 1.0f)) {
		target->AddBuildPower(resurrector, resurrectSpeed);
		return ResurrectResult::Restoring;
	}

	const UnitDef* resurrecteeDef = target->udef;
	const float step = resurrectSpeed / resurrecteeDef->buildTime;

	const bool resurrectAllowed = eventHandler.AllowFeatureBuildStep(resurrector, target, step);
	const bool canExecResurrect = (resurrectAllowed && resurrector->UseEnergy(resurrecteeDef->cost.energy * step * modInfo.resurrectCostFactor.energy));

	if (!canExecResurrect)
		return ResurrectResult::Blocked;

	target->resurrectProgress += step;
	target->resurrectProgress = std::min(target->resurrectProgress, 1.0f);

	if (target->resurrectProgress < 1.0f)
		return ResurrectResult::InProgress;

	// Resurrection complete
	if (target->deleteMe)
		return ResurrectResult::Completed;

	target->UnBlock();

	UnitLoadParams resurrecteeParams = {resurrecteeDef, resurrector, target->pos, ZeroVector, -1, resurrector->team, target->buildFacing, false, false};
	CUnit* resurrectee = unitLoader->LoadUnit(resurrecteeParams);

	resurrectee->SetSoloBuilder(resurrector, resurrecteeDef);
	resurrectee->SetHeading(target->heading, !resurrectee->upright && resurrectee->IsOnGround(), false, 0.0f);

	featureHandler.DeleteFeature(target);

	*outUnit = resurrectee;
	return ResurrectResult::Completed;
}

} // namespace BuildActions