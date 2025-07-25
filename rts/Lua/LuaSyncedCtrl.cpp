/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include <vector>
#include <cctype>

#include "LuaSyncedCtrl.h"

#include "LuaInclude.h"
#include "LuaConfig.h"
#include "LuaRules.h" // for MAX_LUA_COB_ARGS
#include "LuaHandleSynced.h"
#include "LuaHashString.h"
#include "LuaMetalMap.h"
#include "LuaSyncedMoveCtrl.h"
#include "LuaUtils.h"
#include "Game/Game.h"
#include "Game/GameSetup.h"
#include "Game/Camera.h"
#include "Game/GameHelper.h"
#include "Game/SelectedUnitsHandler.h"
#include "Game/Players/PlayerHandler.h"
#include "Game/Players/Player.h"
#include "Net/GameServer.h"
#include "Map/Ground.h"
#include "Map/MapDamage.h"
#include "Map/MapInfo.h"
#include "Map/ReadMap.h"
#include "Rendering/Env/GrassDrawer.h"
#include "Rendering/Env/IGroundDecalDrawer.h"
#include "Rendering/Models/IModelParser.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Features/FeatureDefHandler.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Misc/BuildingMaskMap.h"
#include "Sim/Misc/CollisionVolume.h"
#include "Sim/Misc/DamageArray.h"
#include "Sim/Misc/DamageArrayHandler.h"
#include "Sim/Misc/LosHandler.h"
#include "Sim/Misc/ExtractorHandler.h"
#include "Sim/Misc/ModInfo.h"
#include "Sim/Misc/SmoothHeightMesh.h"
#include "Sim/Misc/Team.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Misc/Wind.h"
#include "Sim/MoveTypes/AAirMoveType.h"
#include "Sim/Path/IPathManager.h"
#include "Sim/Projectiles/ExplosionGenerator.h"
#include "Sim/Projectiles/Projectile.h"
#include "Sim/Projectiles/PieceProjectile.h"
#include "Sim/Projectiles/WeaponProjectiles/MissileProjectile.h"
#include "Sim/Projectiles/WeaponProjectiles/StarburstProjectile.h"
#include "Sim/Projectiles/WeaponProjectiles/TorpedoProjectile.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectile.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectileFactory.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Units/Components/Extractor.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitLoader.h"
#include "Sim/Units/UnitToolTipMap.hpp"
#include "Sim/Units/Scripts/CobInstance.h"
#include "Sim/Units/Scripts/LuaUnitScript.h"
#include "Sim/Units/UnitTypes/Builder.h"
#include "Sim/Units/UnitTypes/Factory.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/CommandAI/FactoryCAI.h"
#include "Sim/Weapons/PlasmaRepulser.h"
#include "Sim/Weapons/Weapon.h"
#include "Sim/Weapons/WeaponDefHandler.h"
#include "Sim/Weapons/WeaponTarget.h"
#include "System/EventHandler.h"
#include "System/ObjectDependenceTypes.h"
#include "System/Log/ILog.h"

using std::max;


/******************************************************************************/
/******************************************************************************/

static int heightMapx1 = 0;
static int heightMapx2 = 0;
static int heightMapz1 = 0;
static int heightMapz2 = 0;

static float heightMapAmountChanged = 0.0f;
static float originalHeightMapAmountChanged = 0.0f;
static float smoothMeshAmountChanged = 0.0f;

/***
Synced Lua API
@see rts/Lua/LuaSyncedCtrl.cpp
*/


/******************************************************************************/

inline void LuaSyncedCtrl::CheckAllowGameChanges(lua_State* L)
{
	if (!CLuaHandle::GetHandleAllowChanges(L)) {
		luaL_error(L, "Unsafe attempt to change game state");
	}
}


/******************************************************************************/
/******************************************************************************/

bool LuaSyncedCtrl::PushEntries(lua_State* L)
{
	{
		// these need to be re-initialized here since we might have reloaded
		inCreateUnit = 0;
		inDestroyUnit = 0;
		inCreateFeature = 0;
		inDestroyFeature = 0;
		inGiveOrder = 0;
		inTransferUnit = 0;
		inHeightMap = false;
		inSmoothMesh = false;

		heightMapx1 = 0;
		heightMapx2 = 0;
		heightMapz1 = 0;
		heightMapz2 = 0;

		heightMapAmountChanged = 0.0f;
		originalHeightMapAmountChanged = 0.0f;
		smoothMeshAmountChanged = 0.0f;
	}


	REGISTER_LUA_CFUNC(SetAlly);
	REGISTER_LUA_CFUNC(SetAllyTeamStartBox);
	REGISTER_LUA_CFUNC(KillTeam);
	REGISTER_LUA_CFUNC(AssignPlayerToTeam);
	REGISTER_LUA_CFUNC(GameOver);
	REGISTER_LUA_CFUNC(SetGlobalLos);

	REGISTER_LUA_CFUNC(AddTeamResource);
	REGISTER_LUA_CFUNC(UseTeamResource);
	REGISTER_LUA_CFUNC(SetTeamResource);
	REGISTER_LUA_CFUNC(SetTeamShareLevel);
	REGISTER_LUA_CFUNC(ShareTeamResource);

	REGISTER_LUA_CFUNC(SetGameRulesParam);
	REGISTER_LUA_CFUNC(SetTeamRulesParam);
	REGISTER_LUA_CFUNC(SetPlayerRulesParam);
	REGISTER_LUA_CFUNC(SetUnitRulesParam);
	REGISTER_LUA_CFUNC(SetFeatureRulesParam);

	REGISTER_LUA_CFUNC(CreateUnit);
	REGISTER_LUA_CFUNC(DestroyUnit);
	REGISTER_LUA_CFUNC(TransferUnit);

	REGISTER_LUA_CFUNC(CreateFeature);
	REGISTER_LUA_CFUNC(DestroyFeature);
	REGISTER_LUA_CFUNC(TransferFeature);

	REGISTER_LUA_CFUNC(CreateUnitWreck);
	REGISTER_LUA_CFUNC(CreateFeatureWreck);

	REGISTER_LUA_CFUNC(SetUnitCosts);
	REGISTER_LUA_CFUNC(SetUnitResourcing);
	REGISTER_LUA_CFUNC(SetUnitStorage);
	REGISTER_LUA_CFUNC(SetUnitTooltip);
	REGISTER_LUA_CFUNC(SetUnitHealth);
	REGISTER_LUA_CFUNC(SetUnitMaxHealth);
	REGISTER_LUA_CFUNC(SetUnitStockpile);
	REGISTER_LUA_CFUNC(SetUnitUseWeapons);
	REGISTER_LUA_CFUNC(SetUnitWeaponState);
	REGISTER_LUA_CFUNC(SetUnitWeaponDamages);
	REGISTER_LUA_CFUNC(SetUnitMaxRange);
	REGISTER_LUA_CFUNC(SetUnitExperience);
	REGISTER_LUA_CFUNC(AddUnitExperience);
	REGISTER_LUA_CFUNC(SetUnitArmored);
	REGISTER_LUA_CFUNC(SetUnitLosMask);
	REGISTER_LUA_CFUNC(SetUnitLosState);
	REGISTER_LUA_CFUNC(SetUnitCloak);
	REGISTER_LUA_CFUNC(SetUnitStealth);
	REGISTER_LUA_CFUNC(SetUnitSonarStealth);
	REGISTER_LUA_CFUNC(SetUnitSeismicSignature);
	REGISTER_LUA_CFUNC(SetUnitLeavesGhost);
	REGISTER_LUA_CFUNC(SetUnitAlwaysVisible);
	REGISTER_LUA_CFUNC(SetUnitUseAirLos);
	REGISTER_LUA_CFUNC(SetUnitMetalExtraction);
	REGISTER_LUA_CFUNC(SetUnitHarvestStorage);
	REGISTER_LUA_CFUNC(SetUnitBuildSpeed);
	REGISTER_LUA_CFUNC(SetUnitBuildParams);
	REGISTER_LUA_CFUNC(SetUnitNanoPieces);

	REGISTER_LUA_CFUNC(SetUnitBlocking);
	REGISTER_LUA_CFUNC(SetUnitCrashing);
	REGISTER_LUA_CFUNC(SetUnitShieldState);
	REGISTER_LUA_CFUNC(SetUnitShieldRechargeDelay);
	REGISTER_LUA_CFUNC(SetUnitFlanking);
	REGISTER_LUA_CFUNC(GetUnitPhysicalState);
	REGISTER_LUA_CFUNC(SetUnitPhysicalStateBit);
	REGISTER_LUA_CFUNC(SetUnitTravel);
	REGISTER_LUA_CFUNC(SetUnitFuel);
	REGISTER_LUA_CFUNC(SetUnitMoveGoal);
	REGISTER_LUA_CFUNC(SetUnitLandGoal);
	REGISTER_LUA_CFUNC(ClearUnitGoal);
	REGISTER_LUA_CFUNC(SetUnitNeutral);
	REGISTER_LUA_CFUNC(SetUnitTarget);
	REGISTER_LUA_CFUNC(SetUnitMidAndAimPos);
	REGISTER_LUA_CFUNC(SetUnitRadiusAndHeight);
	REGISTER_LUA_CFUNC(SetUnitBuildeeRadius);

	REGISTER_LUA_CFUNC(SetUnitCollisionVolumeData);
	REGISTER_LUA_CFUNC(SetUnitPieceCollisionVolumeData);
	REGISTER_LUA_CFUNC(SetUnitPieceVisible);
	REGISTER_LUA_CFUNC(SetUnitPieceParent);
	REGISTER_LUA_CFUNC(SetUnitPieceMatrix);
	REGISTER_LUA_CFUNC(SetUnitSensorRadius);
	REGISTER_LUA_CFUNC(SetUnitPosErrorParams);
	REGISTER_LUA_CFUNC(SetUnitPhysics);
	REGISTER_LUA_CFUNC(SetUnitMass);
	REGISTER_LUA_CFUNC(SetUnitPosition);
	REGISTER_LUA_CFUNC(SetUnitVelocity);
	REGISTER_LUA_CFUNC(SetUnitRotation);
	REGISTER_LUA_CFUNC(SetUnitDirection);
	REGISTER_LUA_CFUNC(SetUnitHeadingAndUpDir);

	REGISTER_LUA_CFUNC(SetFactoryBuggerOff);
	REGISTER_LUA_CFUNC(BuggerOff);

	REGISTER_LUA_CFUNC(AddFeatureDamage);
	REGISTER_LUA_CFUNC(AddUnitDamage);
	REGISTER_LUA_CFUNC(AddUnitImpulse);
	REGISTER_LUA_CFUNC(AddUnitSeismicPing);

	REGISTER_LUA_CFUNC(AddUnitResource);
	REGISTER_LUA_CFUNC(UseUnitResource);

	REGISTER_LUA_CFUNC(AddObjectDecal);
	REGISTER_LUA_CFUNC(RemoveObjectDecal);
	REGISTER_LUA_CFUNC(AddGrass);
	REGISTER_LUA_CFUNC(RemoveGrass);

	REGISTER_LUA_CFUNC(SetFeatureAlwaysVisible);
	REGISTER_LUA_CFUNC(SetFeatureUseAirLos);
	REGISTER_LUA_CFUNC(SetFeatureHealth);
	REGISTER_LUA_CFUNC(SetFeatureMaxHealth);
	REGISTER_LUA_CFUNC(SetFeatureReclaim);
	REGISTER_LUA_CFUNC(SetFeatureResources);
	REGISTER_LUA_CFUNC(SetFeatureResurrect);

	REGISTER_LUA_CFUNC(SetFeatureMoveCtrl);
	REGISTER_LUA_CFUNC(SetFeaturePhysics);
	REGISTER_LUA_CFUNC(SetFeatureMass);
	REGISTER_LUA_CFUNC(SetFeaturePosition);
	REGISTER_LUA_CFUNC(SetFeatureVelocity);
	REGISTER_LUA_CFUNC(SetFeatureRotation);
	REGISTER_LUA_CFUNC(SetFeatureDirection);
	REGISTER_LUA_CFUNC(SetFeatureHeadingAndUpDir);

	REGISTER_LUA_CFUNC(SetFeatureBlocking);
	REGISTER_LUA_CFUNC(SetFeatureNoSelect);
	REGISTER_LUA_CFUNC(SetFeatureMidAndAimPos);
	REGISTER_LUA_CFUNC(SetFeatureRadiusAndHeight);
	REGISTER_LUA_CFUNC(SetFeatureCollisionVolumeData);
	REGISTER_LUA_CFUNC(SetFeaturePieceCollisionVolumeData);
	REGISTER_LUA_CFUNC(SetFeaturePieceVisible);

	REGISTER_LUA_CFUNC(SetFeatureFireTime);
	REGISTER_LUA_CFUNC(SetFeatureSmokeTime);

	REGISTER_LUA_CFUNC(SetProjectileAlwaysVisible);
	REGISTER_LUA_CFUNC(SetProjectileUseAirLos);
	REGISTER_LUA_CFUNC(SetProjectileMoveControl);
	REGISTER_LUA_CFUNC(SetProjectilePosition);
	REGISTER_LUA_CFUNC(SetProjectileVelocity);
	REGISTER_LUA_CFUNC(SetProjectileCollision);
	REGISTER_LUA_CFUNC(SetProjectileTarget);
	REGISTER_LUA_CFUNC(SetProjectileTimeToLive);
	REGISTER_LUA_CFUNC(SetProjectileIsIntercepted);
	REGISTER_LUA_CFUNC(SetProjectileDamages);
	REGISTER_LUA_CFUNC(SetProjectileIgnoreTrackingError);

	REGISTER_LUA_CFUNC(SetProjectileGravity);
	REGISTER_LUA_CFUNC(SetProjectileSpinAngle);
	REGISTER_LUA_CFUNC(SetProjectileSpinSpeed);
	REGISTER_LUA_CFUNC(SetProjectileSpinVec);
	REGISTER_LUA_CFUNC(SetPieceProjectileParams);

	REGISTER_LUA_CFUNC(SetProjectileCEG);


	REGISTER_LUA_CFUNC(CallCOBScript);
	REGISTER_LUA_CFUNC(GetCOBScriptID);
	//FIXME: REGISTER_LUA_CFUNC(GetUnitCOBValue);
	//FIXME: REGISTER_LUA_CFUNC(SetUnitCOBValue);

	REGISTER_LUA_CFUNC(UnitFinishCommand);
	REGISTER_LUA_CFUNC(GiveOrderToUnit);
	REGISTER_LUA_CFUNC(GiveOrderToUnitMap);
	REGISTER_LUA_CFUNC(GiveOrderToUnitArray);
	REGISTER_LUA_CFUNC(GiveOrderArrayToUnit);
	REGISTER_LUA_CFUNC(GiveOrderArrayToUnitMap);
	REGISTER_LUA_CFUNC(GiveOrderArrayToUnitArray);

	REGISTER_LUA_CFUNC(LevelHeightMap);
	REGISTER_LUA_CFUNC(AdjustHeightMap);
	REGISTER_LUA_CFUNC(RevertHeightMap);

	REGISTER_LUA_CFUNC(AddHeightMap);
	REGISTER_LUA_CFUNC(SetHeightMap);
	REGISTER_LUA_CFUNC(SetHeightMapFunc);

	REGISTER_LUA_CFUNC(LevelOriginalHeightMap);
	REGISTER_LUA_CFUNC(AdjustOriginalHeightMap);
	REGISTER_LUA_CFUNC(RevertOriginalHeightMap);

	REGISTER_LUA_CFUNC(AddOriginalHeightMap);
	REGISTER_LUA_CFUNC(SetOriginalHeightMap);
	REGISTER_LUA_CFUNC(SetOriginalHeightMapFunc);

	REGISTER_LUA_CFUNC(RebuildSmoothMesh);

	REGISTER_LUA_CFUNC(LevelSmoothMesh);
	REGISTER_LUA_CFUNC(AdjustSmoothMesh);
	REGISTER_LUA_CFUNC(RevertSmoothMesh);

	REGISTER_LUA_CFUNC(AddSmoothMesh);
	REGISTER_LUA_CFUNC(SetSmoothMesh);
	REGISTER_LUA_CFUNC(SetSmoothMeshFunc);

	REGISTER_LUA_CFUNC(SetMapSquareTerrainType);
	REGISTER_LUA_CFUNC(SetTerrainTypeData);

	REGISTER_LUA_CFUNC(SetTidal);
	REGISTER_LUA_CFUNC(SetWind);

	REGISTER_LUA_CFUNC(SetSquareBuildingMask);

	REGISTER_LUA_CFUNC(UnitWeaponFire);
	REGISTER_LUA_CFUNC(UnitWeaponHoldFire);

	REGISTER_LUA_CFUNC(ForceUnitCollisionUpdate);

	REGISTER_LUA_CFUNC(UnitAttach);
	REGISTER_LUA_CFUNC(UnitDetach);
	REGISTER_LUA_CFUNC(UnitDetachFromAir);
	REGISTER_LUA_CFUNC(SetUnitLoadingTransport);

	REGISTER_LUA_CFUNC(SpawnProjectile);
	REGISTER_LUA_CFUNC(DeleteProjectile);
	REGISTER_LUA_CFUNC(SpawnExplosion);
	REGISTER_LUA_CFUNC(SpawnCEG);
	REGISTER_LUA_CFUNC(SpawnSFX);

	REGISTER_LUA_CFUNC(EditUnitCmdDesc);
	REGISTER_LUA_CFUNC(InsertUnitCmdDesc);
	REGISTER_LUA_CFUNC(RemoveUnitCmdDesc);

	REGISTER_LUA_CFUNC(SetNoPause);
	REGISTER_LUA_CFUNC(SetExperienceGrade);

	REGISTER_LUA_CFUNC(SetRadarErrorParams);

	// Unfortunatly classes within classes does not work well in emmylua
	/*** @field Spring.MoveCtrl MoveCtrl */
	if (!LuaSyncedMoveCtrl::PushMoveCtrl(L))
		return false;

	if (!CLuaUnitScript::PushEntries(L))
		return false;

	if (!LuaMetalMap::PushCtrlEntries(L))
		return false;

	return true;
}




/******************************************************************************/
/******************************************************************************/
//
//  Parsing helpers
//

static inline CUnit* ParseRawUnit(lua_State* L, const char* caller, int index)
{
	return (unitHandler.GetUnit(luaL_checkint(L, index)));
}

static inline CUnit* ParseUnit(lua_State* L, const char* caller, int index)
{
	CUnit* unit = ParseRawUnit(L, caller, index);

	if (unit == nullptr)
		return nullptr;
	if (!CanControlUnit(L, unit))
		return nullptr;

	return unit;
}

static inline CFeature* ParseFeature(lua_State* L, const char* caller, int index)
{
	CFeature* f = featureHandler.GetFeature(luaL_checkint(L, index));

	if (f == nullptr)
		return nullptr;
	if (!CanControlFeature(L, f))
		return nullptr;

	return f;
}

static inline CProjectile* ParseProjectile(lua_State* L, const char* caller, int index)
{
	CProjectile* p = projectileHandler.GetProjectileBySyncedID(luaL_checkint(L, index));

	if (p == nullptr)
		return nullptr;

	if (!CanControlProjectileAllyTeam(L, p->GetAllyteamID()))
		return nullptr;

	return p;
}

static bool ParseProjectileParams(lua_State* L, ProjectileParams& params, const int tblIdx, const char* caller)
{
	if (!lua_istable(L, tblIdx)) {
		luaL_error(L, "[%s] argument %d must be a table!", caller, tblIdx);
		return false;
	}

	params.teamID = teamHandler.GaiaTeamID();

	for (lua_pushnil(L); lua_next(L, tblIdx) != 0; lua_pop(L, 1)) {
		if (!lua_israwstring(L, LUA_TABLE_KEY_INDEX))
			continue;

		const char* key = lua_tostring(L, LUA_TABLE_KEY_INDEX);

		if (lua_istable(L, -1)) {
			float array[3] = {0.0f, 0.0f, 0.0f};

			if (LuaUtils::ParseFloatArray(L, -1, array, 3) == 3) {
				switch (hashString(key)) {
					case hashString("pos"   ): { params.pos    = array; } break;
					case hashString("end"   ): { params.end    = array; } break;
					case hashString("speed" ): { params.speed  = array; } break;
					case hashString("spread"): { params.spread = array; } break;
					case hashString("error" ): { params.error  = array; } break;
				}
			}

			continue;
		}

		if (lua_isnumber(L, LUA_TABLE_VALUE_INDEX)) {
			switch (hashString(key)) {
				case hashString("owner" ): { params.ownerID   = lua_toint(L, LUA_TABLE_VALUE_INDEX)                        ; } break;
				case hashString("weapon"): { params.weaponNum = lua_toint(L, LUA_TABLE_VALUE_INDEX) - LUA_WEAPON_BASE_INDEX; } break;
				case hashString("team"  ): { params.teamID    = lua_toint(L, LUA_TABLE_VALUE_INDEX)                        ; } break;

				case hashString("ttl"): { params.ttl = lua_tofloat(L, LUA_TABLE_VALUE_INDEX); } break;

				case hashString("gravity" ): { params.gravity  = lua_tofloat(L, LUA_TABLE_VALUE_INDEX); } break;
				case hashString("tracking"): { params.tracking = lua_tofloat(L, LUA_TABLE_VALUE_INDEX); } break;

				case hashString("maxRange"): { params.maxRange = lua_tofloat(L, LUA_TABLE_VALUE_INDEX); } break;
				case hashString("upTime"  ): { params.upTime   = lua_tofloat(L, LUA_TABLE_VALUE_INDEX); } break;

				case hashString("startAlpha"): { params.startAlpha = lua_tofloat(L, LUA_TABLE_VALUE_INDEX); } break;
				case hashString("endAlpha"  ): { params.endAlpha   = lua_tofloat(L, LUA_TABLE_VALUE_INDEX); } break;
			}

			continue;
		}

		if (lua_isstring(L, LUA_TABLE_VALUE_INDEX)) {
			switch (hashString(key)) {
				case hashString("model"): {
					params.model = modelLoader.LoadModel(lua_tostring(L, LUA_TABLE_VALUE_INDEX));
				} break;
				case hashString("cegtag"): {
					params.cegID = explGenHandler.LoadGeneratorID(lua_tostring(L, LUA_TABLE_VALUE_INDEX));
				} break;
			}

			continue;
		}
	}

	return true;
}

static CTeam* ParseTeam(lua_State* L, const char* caller, int index)
{
	if (!lua_isnumber(L, index)) {
		luaL_error(L, "%s(): Bad teamID", caller);
		return nullptr;
	}

	const int teamID = lua_toint(L, index);

	if (!teamHandler.IsValidTeam(teamID))
		luaL_error(L, "%s(): Bad teamID: %d", caller, teamID);


	return (teamHandler.Team(teamID));
}

static void ParseUnitMap(lua_State* L, const char* caller,
	int table, vector<CUnit*>& unitIDs)
{
	if (!lua_istable(L, table))
		luaL_error(L, "%s(): error parsing unit map", caller);

	for (lua_pushnil(L); lua_next(L, table) != 0; lua_pop(L, 1)) {
		if (!lua_israwnumber(L, LUA_TABLE_KEY_INDEX))
			continue;

		CUnit* unit = ParseUnit(L, __func__, LUA_TABLE_KEY_INDEX);

		if (unit == nullptr)
			continue; // bad pointer

		unitIDs.push_back(unit);
	}
}


static void ParseUnitArray(lua_State* L, const char* caller,
	int table, vector<CUnit*>& unitIDs)
{
	if (!lua_istable(L, table))
		luaL_error(L, "%s(): error parsing unit array", caller);

	for (lua_pushnil(L); lua_next(L, table) != 0; lua_pop(L, 1)) {
		if (!lua_israwnumber(L, LUA_TABLE_KEY_INDEX) || !lua_isnumber(L, LUA_TABLE_VALUE_INDEX)) // avoid 'n'
			continue;

		CUnit* unit = ParseUnit(L, __func__, LUA_TABLE_VALUE_INDEX);

		if (unit == nullptr)
			continue; // bad pointer

		unitIDs.push_back(unit);
	}
}

static void ParseUnitDefArray(lua_State* L, const char* caller,
	int table, vector<const UnitDef*>& unitDefs)
{
	if (!lua_istable(L, table))
		luaL_error(L, "%s(): error parsing unitdef array", caller);

	for (lua_pushnil(L); lua_next(L, table) != 0; lua_pop(L, 1)) {
		if (!lua_israwnumber(L, LUA_TABLE_KEY_INDEX) || !lua_isnumber(L, LUA_TABLE_VALUE_INDEX)) // avoid 'n'
			continue;

		const UnitDef* ud = unitDefHandler->GetUnitDefByID(luaL_checkint(L, LUA_TABLE_VALUE_INDEX));

		if (ud == nullptr)
			continue; // bad pointer

		unitDefs.push_back(ud);
	}
}

static int SetSolidObjectCollisionVolumeData(lua_State* L, CSolidObject* o)
{
	if (o == nullptr)
		return 0;

	return LuaUtils::ParseColVolData(L, 2, &o->collisionVolume);
}

/** - Not exported
 * 
 * Parses the following params, starting from the 2nd index:
 * 
 * @param isBlocking boolean? If `true` add this object to the `GroundBlockingMap`, but only if it collides with solid objects (or is being set to collide with the `isSolidObjectCollidable` argument). If `false`, remove this object from the `GroundBlockingMap`. No change if `nil`.
 * @param isSolidObjectCollidable boolean? Enable or disable collision with solid objects, or no change if `nil`.
 * @param isProjectileCollidable boolean? Enable or disable collision with projectiles, or no change if `nil`.
 * @param isRaySegmentCollidable boolean? Enable or disable collision with ray segments, or no change if `nil`.
 * @param crushable boolean? Enable or disable crushable, or no change if `nil`.
 * @param blockEnemyPushing boolean? Enable or disable blocking enemy pushing, or no change if `nil`.
 * @param blockHeightChanges boolean? Enable or disable blocking height changes, or no change if `nil`.
 * @return boolean isBlocking
 */
static int SetSolidObjectBlocking(lua_State* L, CSolidObject* o)
{
	if (o == nullptr)
		return 0;

	// update SO-bit of collidable state
	if (lua_isboolean(L, 3)) {
		if (lua_toboolean(L, 3)) {
			o->SetCollidableStateBit(CSolidObject::CSTATE_BIT_SOLIDOBJECTS);
		} else {
			o->ClearCollidableStateBit(CSolidObject::CSTATE_BIT_SOLIDOBJECTS);
		}
	}

	// update blocking-bit of physical state (do this
	// after changing the SO-bit so it is reversible)
	if (lua_isboolean(L, 2)) {
		if (lua_toboolean(L, 2)) {
			o->Block();
		} else {
			o->UnBlock();
		}
	}

	o->UpdateCollidableStateBit(CSolidObject::CSTATE_BIT_PROJECTILES, luaL_optboolean(L, 4, o->HasCollidableStateBit(CSolidObject::CSTATE_BIT_PROJECTILES)));
	o->UpdateCollidableStateBit(CSolidObject::CSTATE_BIT_QUADMAPRAYS, luaL_optboolean(L, 5, o->HasCollidableStateBit(CSolidObject::CSTATE_BIT_QUADMAPRAYS)));

	o->crushable = luaL_optboolean(L, 6, o->crushable);
	o->blockEnemyPushing = luaL_optboolean(L, 7, o->blockEnemyPushing);
	o->blockHeightChanges = luaL_optboolean(L, 8, o->blockHeightChanges);

	lua_pushboolean(L, o->IsBlocking());
	return 1;
}

template<typename T>
static int SetSolidObjectRotation(lua_State* L, T* o)
{
	if (o == nullptr)
		return 0;

	float3 angles;
	angles[CMatrix44f::ANGLE_P] = luaL_checkfloat(L, 2);
	angles[CMatrix44f::ANGLE_Y] = luaL_checkfloat(L, 3);
	angles[CMatrix44f::ANGLE_R] = luaL_checkfloat(L, 4);

	o->SetDirVectorsEuler(angles);

	if constexpr(std::is_same_v<T, CFeature>)
		o->UpdateTransform(o->pos, true);

	return 0;
}

template<typename T>
static int SetSolidObjectHeadingAndUpDir(lua_State* L, T* o)
{
	if (o == nullptr)
		return 0;

	const auto heading = spring::SafeCast<short>(luaL_optint(L, 2, o->heading));
	const float3 newUpDir = float3(luaL_checkfloat(L, 3), luaL_checkfloat(L, 4), luaL_checkfloat(L, 5)).SafeNormalize();
	if (math::fabsf(newUpDir.SqLength() - 1.0f) > float3::cmp_eps())
		luaL_error(L, "[%s] Invalid upward-direction (%f, %f, %f), id = %d, model = %s, teamID = %d", __func__, newUpDir.x, newUpDir.y, newUpDir.z, o->id, o->model ? o->model->name.c_str() : "nullptr", o->team);

	o->heading = heading;
	o->UpdateDirVectors(newUpDir);
	o->SetFacingFromHeading();
	o->UpdateMidAndAimPos();

	if constexpr (std::is_same_v<T, CFeature>)
		o->UpdateTransform(o->pos, true);

	return 0;
}

static int SetSolidObjectDirection(lua_State* L, CSolidObject* o, const char* func)
{
	if (o == nullptr)
		return 0;

	const char* modelName = o->model ? o->model->name.c_str() : "nullptr";

	const float3 newFrontDir = float3(luaL_checkfloat(L, 2), luaL_checkfloat(L, 3), luaL_checkfloat(L, 4)).SafeNormalize();

	if (math::fabsf(newFrontDir.SqLength() - 1.0f) > float3::cmp_eps()) {
		luaL_error(L, "[%s] Invalid front-direction (%f, %f, %f), id = %d, model = %s, teamID = %d",
			func,
			newFrontDir.x,
			newFrontDir.y,
			newFrontDir.z,
			o->id,
			modelName,
			o->team
		);
	}

	// Note there's no need to call o->UpdateTransform(o->pos, true); because both variants of o->ForcedSpin
	// defined in CFeature do it anyway

	if (lua_isnumber(L, 5) && lua_isnumber(L, 6) && lua_isnumber(L, 7)) {
		const float3 newRightDir = float3(luaL_checkfloat(L, 5), luaL_checkfloat(L, 6), luaL_checkfloat(L, 7)).SafeNormalize();
		if (math::fabsf(newRightDir.SqLength() - 1.0f) > float3::cmp_eps()) {
			luaL_error(L, "[%s] Invalid optional right-direction (%f, %f, %f), id = %d, model = %s, teamID = %d",
				func,
				newRightDir.x,
				newRightDir.y,
				newRightDir.z,
				o->id,
				modelName,
				o->team
			);
		}

		const float dp = newFrontDir.dot(newRightDir);
		if (math::fabsf(dp) > float3::cmp_eps()) {
			luaL_error(L, "[%s] front(%f, %f, %f) and right(%f, %f, %f) vectors are not orthogonal(dp=%f), id = %d, model = %s, teamID = %d",
				func,
				newFrontDir.x,
				newFrontDir.y,
				newFrontDir.z,
				newRightDir.x,
				newRightDir.y,
				newRightDir.z,
				dp,
				o->id,
				modelName,
				o->team
			);
		}

		o->ForcedSpin(newFrontDir, newRightDir);
	}
	else {
		o->ForcedSpin(newFrontDir);
	}

	return 0;
}

static int SetWorldObjectVelocity(lua_State* L, CWorldObject* o)
{
	if (o == nullptr)
		return 0;

	float3 speed;
	speed.x = std::clamp(luaL_checkfloat(L, 2), -MAX_UNIT_SPEED, MAX_UNIT_SPEED);
	speed.y = std::clamp(luaL_checkfloat(L, 3), -MAX_UNIT_SPEED, MAX_UNIT_SPEED);
	speed.z = std::clamp(luaL_checkfloat(L, 4), -MAX_UNIT_SPEED, MAX_UNIT_SPEED);

	o->SetVelocityAndSpeed(speed);
	return 0;
}

static int SetSolidObjectMass(lua_State* L, CSolidObject* o)
{
	if (o == nullptr)
		return 0;

	o->SetMass(luaL_checknumber(L, 2));

	return 0;
}

static int SetSolidObjectPhysicalState(lua_State* L, CSolidObject* o)
{
	if (o == nullptr)
		return 0;

	float3 pos;
	float3 rot;
	float3 speed;
	float3& drag = o->dragScales;

	pos.x = luaL_checknumber(L, 2);
	pos.y = luaL_checknumber(L, 3);
	pos.z = luaL_checknumber(L, 4);

	speed.x = luaL_checknumber(L, 5);
	speed.y = luaL_checknumber(L, 6);
	speed.z = luaL_checknumber(L, 7);

	rot.x = luaL_checknumber(L, 8);
	rot.y = luaL_checknumber(L, 9);
	rot.z = luaL_checknumber(L, 10);

	drag.x = std::clamp(luaL_optnumber(L, 11, drag.x), 0.0f, 1.0f);
	drag.y = std::clamp(luaL_optnumber(L, 12, drag.y), 0.0f, 1.0f);
	drag.z = std::clamp(luaL_optnumber(L, 13, drag.z), 0.0f, 1.0f);

	o->Move(pos, false);
	o->SetDirVectorsEuler(rot);
	// do not need ForcedSpin, above three calls cover it
	o->ForcedMove(pos);
	o->SetVelocityAndSpeed(speed);
	return 0;
}

static int SetSolidObjectPieceCollisionVolumeData(lua_State* L, CSolidObject* obj)
{
	if (obj == nullptr)
		return 0;

	LocalModelPiece* lmp = ParseObjectLocalModelPiece(L, obj, 2);

	if (lmp == nullptr)
		luaL_argerror(L, 2, "invalid piece");

	CollisionVolume* vol = lmp->GetCollisionVolume();

	const float3 scales(luaL_checkfloat(L, 4), luaL_checkfloat(L, 5), luaL_checkfloat(L, 6));
	const float3 offset(luaL_checkfloat(L, 7), luaL_checkfloat(L, 8), luaL_checkfloat(L, 9));

	const unsigned int vType = luaL_optint(L, 10, vol->GetVolumeType());
	const unsigned int pAxis = luaL_optint(L, 11, vol->GetPrimaryAxis());

	// piece volumes are not allowed to use discrete hit-testing
	vol->InitShape(scales, offset, vType, CollisionVolume::COLVOL_HITTEST_CONT, pAxis);
	vol->SetIgnoreHits(!luaL_checkboolean(L, 3));
	return 0;
}

static int SetSolidObjectPieceVisible(lua_State* L, CSolidObject* obj)
{
	if (obj == nullptr)
		return 0;

	LocalModelPiece* lmp = ParseObjectLocalModelPiece(L, obj, 2);
	if (lmp == nullptr)
		luaL_argerror(L, 2, "invalid piece");

	lmp->SetScriptVisible(luaL_checkboolean(L, 3));
	return 0;
}


static int SetWorldObjectAlwaysVisible(lua_State* L, CWorldObject* o, const char* caller)
{
	if (o == nullptr)
		return 0;

	o->alwaysVisible = luaL_checkboolean(L, 2);
	return 0;
}

static int SetWorldObjectUseAirLos(lua_State* L, CWorldObject* o, const char* caller)
{
	if (o == nullptr)
		return 0;

	o->useAirLos = luaL_checkboolean(L, 2);
	return 0;
}


/******************************************************************************/
/******************************************************************************/

static inline bool IsPlayerSynced(const CPlayer* player)
{
	return (!gameSetup->hostDemo || player->isFromDemo);
}


/******************************************************************************
 * Teams
 * @section teams
******************************************************************************/

/*** Changes the value of the (one-sided) alliance between: firstAllyTeamID -> secondAllyTeamID.
 *
 * @function Spring.SetAlly
 * @param firstAllyTeamID integer
 * @param secondAllyTeamID integer
 * @param ally boolean
 * @return nil
 */
int LuaSyncedCtrl::SetAlly(lua_State* L)
{
	const int firstAllyTeamID = luaL_checkint(L, 1);
	const int secondAllyTeamID = luaL_checkint(L, 2);

	if (!teamHandler.IsValidAllyTeam(firstAllyTeamID))
		return 0;
	if (!teamHandler.IsValidAllyTeam(secondAllyTeamID))
		return 0;

	teamHandler.SetAlly(firstAllyTeamID, secondAllyTeamID, luaL_checkboolean(L, 3));
	return 0;
}


/*** Changes the start box position of an allyTeam.
 *
 * @function Spring.SetAllyTeamStartBox
 * @param allyTeamID integer
 * @param xMin number left start box boundary (elmos)
 * @param zMin number top start box boundary (elmos)
 * @param xMax number right start box boundary (elmos)
 * @param zMax number bottom start box boundary (elmos)
 * @return nil
 */
int LuaSyncedCtrl::SetAllyTeamStartBox(lua_State* L)
{
	const unsigned int allyTeamID = luaL_checkint(L, 1);
	const float xMin = luaL_checkfloat(L, 2);
	const float zMin = luaL_checkfloat(L, 3);
	const float xMax = luaL_checkfloat(L, 4);
	const float zMax = luaL_checkfloat(L, 5);

	if (!teamHandler.IsValidAllyTeam(allyTeamID)) {
		return 0;
	}

	const float startRectLeft   = xMin / (mapDims.mapx * SQUARE_SIZE);
	const float startRectTop    = zMin / (mapDims.mapy * SQUARE_SIZE);
	const float startRectRight  = xMax / (mapDims.mapx * SQUARE_SIZE);
	const float startRectBottom = zMax / (mapDims.mapy * SQUARE_SIZE);

	teamHandler.SetAllyTeamStartBox(allyTeamID, startRectLeft, startRectTop, startRectRight, startRectBottom);
	return 0;
}


/*** Assigns a player to a team.
 *
 * @function Spring.AssignPlayerToTeam
 * @param playerID integer
 * @param teamID integer
 * @return nil
 */
int LuaSyncedCtrl::AssignPlayerToTeam(lua_State* L)
{
	const int playerID = luaL_checkint(L, 1);
	const int teamID = luaL_checkint(L, 2);

	const CPlayer* player = playerHandler.IsValidPlayer(playerID)? playerHandler.Player(playerID): nullptr;

	if (player == nullptr)
		return 0;

	if (!IsPlayerSynced(player))
		return 0;

	if (!teamHandler.IsValidTeam(teamID))
		return 0;

	teamHandler.Team(teamID)->AddPlayer(playerID);
	return 0;
}


/*** Changes access to global line of sight for a team and its allies.
 *
 * @function Spring.SetGlobalLos
 * @param allyTeamID integer
 * @param globallos boolean
 * @return nil
 */
int LuaSyncedCtrl::SetGlobalLos(lua_State* L)
{
	const int allyTeam = luaL_checkint(L, 1);

	if (!teamHandler.IsValidAllyTeam(allyTeam))
		luaL_error(L, "bad allyTeam");

	losHandler->SetGlobalLOS(allyTeam, luaL_checkboolean(L, 2));
	return 0;
}


/***
 * Game End
 * @section gameend
 */

/*** Will declare a team to be dead (no further orders can be assigned to such teams units).
 *
 * @function Spring.KillTeam
 *
 * Gaia team cannot be killed.
 *
 * @param teamID integer
 * @return nil
 */
int LuaSyncedCtrl::KillTeam(lua_State* L)
{
	const int teamID = luaL_checkint(L, 1);

	if (!teamHandler.IsValidTeam(teamID))
		return 0;

	//FIXME either we disallow it here or it needs modifications in GameServer.cpp (it creates a `teams` vector w/o gaia)
	//  possible fix would be to always create the Gaia team (currently it's conditional on gs->useLuaGaia)
	if (teamID == teamHandler.GaiaTeamID())
		return 0;

	CTeam* team = teamHandler.Team(teamID);

	if (team == nullptr)
		return 0;

	team->Died();
	return 0;
}


/*** Declare game over.
 * 
 * @function Spring.GameOver
 * @param winningAllyTeamIDs integer[] A list of winning ally team IDs.
 *
 * Pass multiple winners to declare a draw.
 * Pass no arguments if undecided (e.g. when dropped from the host).
 *
 * @return integer Number of accepted (valid) ally teams.
 */
int LuaSyncedCtrl::GameOver(lua_State* L)
{
	if (!lua_istable(L, 1)) {
		luaL_error(L, "Incorrect arguments to GameOver()");
		return 0;
	}

	std::vector<unsigned char> winningAllyTeams;

	static const int tableIdx = 1;

	for (lua_pushnil(L); lua_next(L, tableIdx) != 0; lua_pop(L, 1)) {
		if (!lua_israwnumber(L, LUA_TABLE_VALUE_INDEX))
			continue;

		const unsigned char allyTeamID = lua_toint(L, LUA_TABLE_VALUE_INDEX);

		if (!teamHandler.ValidAllyTeam(allyTeamID))
			continue;

		winningAllyTeams.push_back(allyTeamID);
	}

	game->GameEnd(winningAllyTeams);
	// push the number of accepted allyteams
	lua_pushnumber(L, winningAllyTeams.size());
	return 1;
}


/***
 * Resources
 * @section resources
 */


/*** Set tidal strength
 *
 * @function Spring.SetTidal
 * @param strength number
 * @return nil
 */
int LuaSyncedCtrl::SetTidal(lua_State* L)
{
	envResHandler.LoadTidal(luaL_optnumber(L, 1, envResHandler.GetCurrentTidalStrength()));
	return 0;
}


/*** Set wind strength
 *
 * @function Spring.SetWind
 * @param minStrength number
 * @param maxStrength number
 * @return nil
 */
int LuaSyncedCtrl::SetWind(lua_State* L)
{
	envResHandler.LoadWind(luaL_optnumber(L, 1, envResHandler.GetMinWindStrength()), luaL_optnumber(L, 2, envResHandler.GetMaxWindStrength()));
	return 0;
}

/*** Adds metal or energy resources to the specified team.
 *
 * @function Spring.AddTeamResource
 * @param teamID integer
 * @param type ResourceName
 * @param amount number
 * @return nil
 */
int LuaSyncedCtrl::AddTeamResource(lua_State* L)
{
	const int teamID = luaL_checkint(L, 1);

	if (!teamHandler.IsValidTeam(teamID))
		return 0;

	if (!CanControlTeam(L, teamID))
		return 0;

	CTeam* team = teamHandler.Team(teamID);

	if (team == nullptr)
		return 0;

	const char* type = luaL_checkstring(L, 2);

	const float value = max(0.0f, luaL_checkfloat(L, 3));

	switch (type[0]) {
		case 'm': { team->AddMetal (value); } break;
		case 'e': { team->AddEnergy(value); } break;
		default : {                         } break;
	}

	return 0;
}

/***
 * Consumes metal or energy resources of the specified team.
 *
 * @function Spring.UseTeamResource
 * @param teamID integer
 * @param type ResourceName Resource type.
 * @param amount number Amount of resource to use.
 * @return boolean hadEnough
 * True if enough of the resource type was available and was consumed, otherwise false.
 */
/***
 * Consumes metal and/or energy resources of the specified team.
 *
 * @function Spring.UseTeamResource
 * @param teamID integer
 * @param amount ResourceUsage
 * @return boolean hadEnough
 * True if enough of the resource type(s) were available and was consumed, otherwise false.
 */
int LuaSyncedCtrl::UseTeamResource(lua_State* L)
{
	const int teamID = luaL_checkint(L, 1);

	if (!teamHandler.IsValidTeam(teamID))
		return 0;

	if (!CanControlTeam(L, teamID))
		return 0;

	CTeam* team = teamHandler.Team(teamID);

	if (team == nullptr)
		return 0;

	if (lua_isstring(L, 2)) {
		const char* type = lua_tostring(L, 2);

		const float value = std::max(0.0f, luaL_checkfloat(L, 3));

		switch (type[0]) {
			case 'm': {
				team->resPull.metal += value;
				lua_pushboolean(L, team->UseMetal(value));
				return 1;
			} break;
			case 'e': {
				team->resPull.energy += value;
				lua_pushboolean(L, team->UseEnergy(value));
				return 1;
			} break;
			default: {
			} break;
		}

		return 0;
	}

	if (lua_istable(L, 2)) {
		float metal  = 0.0f;
		float energy = 0.0f;

		constexpr int tableIdx = 2;

		for (lua_pushnil(L); lua_next(L, tableIdx) != 0; lua_pop(L, 1)) {
			if (!lua_israwstring(L, LUA_TABLE_KEY_INDEX) || !lua_isnumber(L, LUA_TABLE_VALUE_INDEX))
				continue;

			const char* key = lua_tostring(L, LUA_TABLE_KEY_INDEX);
			const float value = lua_tofloat(L, LUA_TABLE_VALUE_INDEX);

			switch (key[0]) {
				case 'm': { metal  = std::max(0.0f, value); } break;
				case 'e': { energy = std::max(0.0f, value); } break;
				default : {                                 } break;
			}
		}

		team->resPull.metal  += metal;
		team->resPull.energy += energy;

		if ((team->res.metal >= metal) && (team->res.energy >= energy)) {
			team->UseMetal(metal);
			team->UseEnergy(energy);
			lua_pushboolean(L, true);
		} else {
			lua_pushboolean(L, false);
		}

		return 1;
	}

	luaL_error(L, "bad arguments");
	return 0;
}


/***
 * @function Spring.SetTeamResource
 * @param teamID integer
 * @param resource ResourceName|StorageName
 * @param amount number
 * @return nil
 */
int LuaSyncedCtrl::SetTeamResource(lua_State* L)
{
	const int teamID = luaL_checkint(L, 1);

	if (!teamHandler.IsValidTeam(teamID))
		return 0;

	if (!CanControlTeam(L, teamID))
		return 0;

	CTeam* team = teamHandler.Team(teamID);

	if (team == nullptr)
		return 0;

	const float value = std::max(0.0f, luaL_checkfloat(L, 3));

	switch (hashString(luaL_checkstring(L, 2))) {
		case hashString("m"):
		case hashString("metal"): {
			team->res.metal = std::min<float>(team->resStorage.metal, value);
		} break;

		case hashString("e"):
		case hashString("energy"): {
			team->res.energy = std::min<float>(team->resStorage.energy, value);
		} break;

		case hashString("ms"):
		case hashString("metalStorage"): {
			team->res.metal = std::min<float>(team->res.metal, team->resStorage.metal = value);
		} break;

		case hashString("es"):
		case hashString("energyStorage"): {
			team->res.energy = std::min<float>(team->res.energy, team->resStorage.energy = value);
		} break;
	}

	return 0;
}


/*** Changes the resource amount for a team beyond which resources aren't stored but transferred to other allied teams if possible.
 *
 * @function Spring.SetTeamShareLevel
 * @param teamID integer
 * @param type ResourceName
 * @param amount number
 * @return nil
 */
int LuaSyncedCtrl::SetTeamShareLevel(lua_State* L)
{
	const int teamID = luaL_checkint(L, 1);

	if (!teamHandler.IsValidTeam(teamID))
		return 0;

	if (!CanControlTeam(L, teamID))
		return 0;

	CTeam* team = teamHandler.Team(teamID);

	if (team == nullptr)
		return 0;

	const char* type = luaL_checkstring(L, 2);

	const float value = luaL_checkfloat(L, 3);

	switch (type[0]) {
		case 'm': { team->resShare.metal  = std::clamp(value, 0.0f, 1.0f); } break;
		case 'e': { team->resShare.energy = std::clamp(value, 0.0f, 1.0f); } break;
		default : {                                                   } break;
	}

	return 0;
}


/*** Transfers resources between two teams.
 *
 * @function Spring.ShareTeamResource
 * @param teamID_src integer
 * @param teamID_recv integer
 * @param type ResourceName
 * @param amount number
 * @return nil
 */
int LuaSyncedCtrl::ShareTeamResource(lua_State* L)
{
	const int teamID1 = luaL_checkint(L, 1);

	if (!teamHandler.IsValidTeam(teamID1))
		luaL_error(L, "Incorrect arguments to ShareTeamResource(teamID1, teamID2, type, amount)");

	if (!CanControlTeam(L, teamID1))
		return 0;

	CTeam* team1 = teamHandler.Team(teamID1);

	if (team1 == nullptr)
		return 0;

	const int teamID2 = luaL_checkint(L, 2);

	if (!teamHandler.IsValidTeam(teamID2))
		luaL_error(L, "Incorrect arguments to ShareTeamResource(teamID1, teamID2, type, amount)");

	CTeam* team2 = teamHandler.Team(teamID2);

	if (team2 == nullptr)
		return 0;

	const char* type = luaL_checkstring(L, 3);
	float amount = luaL_checkfloat(L, 4);

	switch (type[0]) {
		case 'm': {
			amount = std::min(amount, (float)team1->res.metal);

			if (eventHandler.AllowResourceTransfer(teamID1, teamID2, "m", amount)) { //FIXME can cause an endless loop
				team1->res.metal                       -= amount;
				team1->resSent.metal                   += amount;
				team1->GetCurrentStats().metalSent     += amount;
				team2->res.metal                       += amount;
				team2->resReceived.metal               += amount;
				team2->GetCurrentStats().metalReceived += amount;
			}
		} break;
		case 'e': {
			amount = std::min(amount, (float)team1->res.energy);

			if (eventHandler.AllowResourceTransfer(teamID1, teamID2, "e", amount)) { //FIXME can cause an endless loop
				team1->res.energy                       -= amount;
				team1->resSent.energy                   += amount;
				team1->GetCurrentStats().energySent     += amount;
				team2->res.energy                       += amount;
				team2->resReceived.energy               += amount;
				team2->GetCurrentStats().energyReceived += amount;
			}
		} break;
		default: {
		} break;
	}

	return 0;
}


/******************************************************************************
 * Rules Params
 * @section rulesparams
******************************************************************************/

/***
 * Parameters for los access
 *
 *
 * If one condition is fulfilled all beneath it are too (e.g. if an unit is in
 * LOS it can read params with `inradar=true` even if the param has
 * `inlos=false`) All GameRulesParam are public, TeamRulesParams can just be
 * `private`,`allied` and/or `public` You can read RulesParams from any Lua
 * environments! With those losAccess policies you can limit their access.
 *
 * All GameRulesParam are public, TeamRulesParams can just be `private`,`allied` and/or `public`
 * You can read RulesParams from any Lua environments! With those losAccess policies you can limit their access.
 *
 * @class losAccess
 * @x_helper
 *
 * @field public private boolean? only readable by the ally (default)
 * @field public allied boolean? readable by ally + ingame allied
 * @field public inlos boolean? readable if the unit is in LOS
 * @field public inradar boolean? readable if the unit is in AirLOS
 * @field public public boolean? readable by all
 */
/* NOTE: Need to explicitly declare `public` scope here to prevent LLS from
 * detecting field names `public`/`private` as an access modifier.
 *
 * See https://github.com/LuaLS/lua-language-server/issues/1814
 */
void SetRulesParam(lua_State* L, const char* caller, int offset,
				LuaRulesParams::Params& params)
{
	const int index = offset + 1;
	const int valIndex = offset + 2;
	const int losIndex = offset + 3; // table

	const std::string& key = luaL_checkstring(L, index);

	LuaRulesParams::Param& param = params[key];

	// set the value of the parameter
	if (lua_israwnumber(L, valIndex)) {
		param.value.emplace <float> (lua_tofloat(L, valIndex));
	} else if (lua_israwboolean(L, valIndex)) {
		param.value.emplace <bool> (lua_toboolean(L, valIndex));
	} else if (lua_isstring(L, valIndex)) {
		param.value.emplace <std::string> (lua_tostring(L, valIndex));
	} else if (lua_isnoneornil(L, valIndex)) {
		params.erase(key);
		return; //no need to set los if param was erased
	} else {
		params.erase(key);
		luaL_error(L, "Incorrect arguments to %s()", caller);
	}

	// set the los checking of the parameter
	if (lua_istable(L, losIndex)) {
		int losMask = LuaRulesParams::RULESPARAMLOS_PRIVATE;

		for (lua_pushnil(L); lua_next(L, losIndex) != 0; lua_pop(L, 1)) {
			// ignore if the value is false
			if (!luaL_optboolean(L, LUA_TABLE_VALUE_INDEX, true))
				continue;

			// read the losType from the key
			if (!lua_isstring(L, LUA_TABLE_KEY_INDEX))
				continue;

			switch (hashString(lua_tostring(L, LUA_TABLE_KEY_INDEX))) {
				case hashString("public" ): { losMask |= LuaRulesParams::RULESPARAMLOS_PUBLIC;  } break;
				case hashString("inlos"  ): { losMask |= LuaRulesParams::RULESPARAMLOS_INLOS;   } break;
				case hashString("typed"  ): { losMask |= LuaRulesParams::RULESPARAMLOS_TYPED;   } break;
				case hashString("inradar"): { losMask |= LuaRulesParams::RULESPARAMLOS_INRADAR; } break;
				case hashString("allied" ): { losMask |= LuaRulesParams::RULESPARAMLOS_ALLIED;  } break;
				// case hashString("private"): { losMask |= LuaRulesParams::RULESPARAMLOS_PRIVATE; } break;
				default                   : {                                                   } break;
			}
		}

		param.los = losMask;
	} else {
		param.los = luaL_optint(L, losIndex, param.los);
	}
}


/***
 * @function Spring.SetGameRulesParam
 * @param paramName string
 * @param paramValue ?number|string numeric paramValues in quotes will be converted to number.
 * @param losAccess losAccess?
 * @return nil
 */
int LuaSyncedCtrl::SetGameRulesParam(lua_State* L)
{
	SetRulesParam(L, __func__, 0, CSplitLuaHandle::gameParams);
	return 0;
}


/***
 * @function Spring.SetTeamRulesParam
 * @param teamID integer
 * @param paramName string
 * @param paramValue ?number|string numeric paramValues in quotes will be converted to number.
 * @param losAccess losAccess?
 * @return nil
 */
int LuaSyncedCtrl::SetTeamRulesParam(lua_State* L)
{
	CTeam* team = ParseTeam(L, __func__, 1);
	if (team == nullptr)
		return 0;

	SetRulesParam(L, __func__, 1, team->modParams);
	return 0;
}

/***
 * @function Spring.SetPlayerRulesParam
 * @param playerID integer
 * @param paramName string
 * @param paramValue ?number|string numeric paramValues in quotes will be converted to number.
 * @param losAccess losAccess?
 * @return nil
 */
int LuaSyncedCtrl::SetPlayerRulesParam(lua_State* L)
{
	const int playerID = luaL_checkint(L, 1);
	if (!playerHandler.IsValidPlayer(playerID))
		return 0;

	const auto player = playerHandler.Player(playerID);
	if (player == nullptr || !IsPlayerSynced(player))
		return 0;

	SetRulesParam(L, __func__, 1, player->modParams);
	return 0;
}


/***
 *
 * @function Spring.SetUnitRulesParam
 * @param unitID integer
 * @param paramName string
 * @param paramValue ?number|string numeric paramValues in quotes will be converted to number.
 * @param losAccess losAccess?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitRulesParam(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	SetRulesParam(L, __func__, 1, unit->modParams);
	return 0;
}


/***
 * @function Spring.SetFeatureRulesParam
 * @param featureID integer
 * @param paramName string
 * @param paramValue ?number|string numeric paramValues in quotes will be converted to number.
 * @param losAccess losAccess?
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureRulesParam(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);
	if (feature == nullptr)
		return 0;

	SetRulesParam(L, __func__, 1, feature->modParams);
	return 0;
}


/******************************************************************************
 * Lua to COB
 * @section luatocob
******************************************************************************/


static inline void ParseCobArgs(
	lua_State* L,
	int first,
	int last,
	std::array<int, 1 + MAX_COB_ARGS>& args
) {
	args[0] = 0;

	for (int a = first; a <= last; a++) {
		if (lua_isnumber(L, a)) {
			args[1 + (args[0]++)] = lua_toint(L, a);
			continue;
		}
		if (lua_istable(L, a)) {
			lua_rawgeti(L, a, 1);
			lua_rawgeti(L, a, 2);

			if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
				const int x = lua_toint(L, -2);
				const int z = lua_toint(L, -1);

				args[1 + (args[0]++)] = PACKXZ(x, z);
			} else {
				args[1 + (args[0]++)] = 0;
			}

			lua_pop(L, 2);
			continue;
		}
		if (lua_isboolean(L, a)) {
			args[1 + (args[0]++)] = int(lua_toboolean(L, a));
			continue;
		}

		args[1 + (args[0]++)] = 0;
	}
}


/***
 * @function Spring.CallCOBScript
 * @param unitID integer
 * @param funcName integer|string? Function ID or name.
 * @param retArgs integer Number of values to return.
 * @param ... any Arguments
 * @return number ...
 */
int LuaSyncedCtrl::CallCOBScript(lua_State* L)
{
//FIXME?	CheckAllowGameChanges(L);
	const int numArgs = lua_gettop(L);

	if (numArgs < 3)
		luaL_error(L, "[%s] too few arguments", __func__);
	if (numArgs > MAX_COB_ARGS)
		luaL_error(L, "[%s] too many arguments", __func__);

	// unitID
	if (!lua_isnumber(L, 1))
		luaL_error(L, "[%s] unitID not a number", __func__);
	// number of returned parameters
	if (!lua_isnumber(L, 3))
		luaL_error(L, "[%s] retval-count not a number", __func__);

	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	CCobInstance* cob = dynamic_cast<CCobInstance*>(unit->script);

	if (cob == nullptr)
		luaL_error(L, "[%s] unit is not running a COB script", __func__);

	static_assert(MAX_LUA_COB_ARGS <= MAX_COB_ARGS, "");

	// collect the arguments; cobArgs[0] holds the final count
	std::array<int, 1 + MAX_COB_ARGS> cobArgs;
	ParseCobArgs(L, 4, numArgs, cobArgs);

	const int numRetVals = std::min(lua_toint(L, 3), std::min(MAX_LUA_COB_ARGS, cobArgs[0]));
	int retCode = 0;

	if (lua_israwnumber(L, 2)) {
		retCode = cob->RawCall(lua_toint(L, 2), cobArgs);
	} else if (lua_israwstring(L, 2)) {
		retCode = cob->Call(lua_tostring(L, 2), cobArgs);
	} else {
		luaL_error(L, "[%s] bad function id or name", __func__);
	}

	lua_settop(L, 0); // FIXME - ok?
	lua_pushnumber(L, retCode);
	for (int i = 0; i < numRetVals; i++) {
		lua_pushnumber(L, cobArgs[i]);
	}
	return (1 + numRetVals);
}


/***
 * @function Spring.GetCOBScriptID
 * @param unitID integer
 * @param funcName string
 * @return integer? funcID
 */
int LuaSyncedCtrl::GetCOBScriptID(lua_State* L)
{
	const int args = lua_gettop(L); // number of arguments

	if ((args < 2) || !lua_isnumber(L, 1) || !lua_isstring(L, 2))
		luaL_error(L, "[%s] incorrect arguments", __func__);

	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	CCobInstance* cob = dynamic_cast<CCobInstance*>(unit->script);

	// no error - allows using this to determine whether unit runs COB or LUS
	if (cob == nullptr)
		return 0;

	const int funcID = cob->GetFunctionId(lua_tostring(L, 2));

	if (funcID >= 0) {
		lua_pushnumber(L, funcID);
		return 1;
	}

	return 0;
}

/******************************************************************************
 * Unit Handling
 * @section unithandling
******************************************************************************/

/***
 * @function Spring.CreateUnit
 * @see Spring.DestroyUnit
 *
 * Offmap positions are clamped! Use MoveCtrl to move to such positions.
 *
 * @param unitDef string|integer UnitDef name or ID.
 * @param posX number
 * @param posY number
 * @param posZ number
 * @param facing Facing
 * @param teamID integer
 * @param build boolean? (Default: `false`) The unit is created in "being built" state with zero `buildProgress`.
 * @param flattenGround boolean? (Default: `true`) The unit flattens ground, if it normally does so.
 * @param unitID integer? Request a specific unitID.
 * @param builderID integer?
 * @return integer? unitID The ID of the created unit, or `nil` if the unit could not be created.
 */
int LuaSyncedCtrl::CreateUnit(lua_State* L)
{
	CheckAllowGameChanges(L);

	if (inCreateUnit >= MAX_CMD_RECURSION_DEPTH) {
		luaL_error(L, "[%s()]: recursion is not permitted, max depth: %d", __func__, MAX_CMD_RECURSION_DEPTH);
		return 0;
	}

	const UnitDef* unitDef = nullptr;

	if (lua_israwstring(L, 1)) {
		unitDef = unitDefHandler->GetUnitDefByName(lua_tostring(L, 1));
	} else if (lua_israwnumber(L, 1)) {
		unitDef = unitDefHandler->GetUnitDefByID(lua_toint(L, 1));
	} else {
		luaL_error(L, "[%s()] incorrect type for first argument", __func__);
		return 0;
	}

	if (unitDef == nullptr) {
		if (lua_israwstring(L, 1)) {
			luaL_error(L, "[%s()]: bad unitDef name: %s", __func__, lua_tostring(L, 1));
		} else {
			luaL_error(L, "[%s()]: bad unitDef ID: %d", __func__, lua_toint(L, 1));
		}
		return 0;
	}

	// CUnit::PreInit will clamp the position
	// TODO: also allow off-map unit creation?
	const float3 pos(
		luaL_checkfloat(L, 2),
		luaL_checkfloat(L, 3),
		luaL_checkfloat(L, 4)
	);
	const int facing = LuaUtils::ParseFacing(L, __func__, 5);
	const int teamID = luaL_optint(L, 6, CtrlTeam(L));

	const bool beingBuilt = luaL_optboolean(L, 7, false);
	const bool flattenGround = luaL_optboolean(L, 8, true);

	if (!teamHandler.IsValidTeam(teamID)) {
		luaL_error(L, "[%s()]: invalid team number (%d)", __func__, teamID);
		return 0;
	}
	if (!FullCtrl(L) && (CtrlTeam(L) != teamID)) {
		luaL_error(L, "[%s()]: not a controllable team (%d)", __func__, teamID);
		return 0;
	}
	if (!unitHandler.CanBuildUnit(unitDef, teamID))
		return 0; // unit limit reached

	ASSERT_SYNCED(pos);
	ASSERT_SYNCED(facing);

	inCreateUnit++;

	CUnit* builder = unitHandler.GetUnit(luaL_optint(L, 10, -1));

	UnitLoadParams params;
	params.unitDef = unitDef; /// must be non-NULL
	params.builder = builder; /// may be NULL
	params.pos     = pos;
	params.speed   = ZeroVector;
	params.unitID  = luaL_optint(L, 9, -1);
	params.teamID  = teamID;
	params.facing  = facing;
	params.beingBuilt = beingBuilt;
	params.flattenGround = flattenGround;

	CUnit* unit = unitLoader->LoadUnit(params);
	inCreateUnit--;

	if (unit == nullptr)
		return 0;

	unit->SetSoloBuilder(builder, unitDef);

	lua_pushnumber(L, unit->id);
	return 1;
}


/***
 * @function Spring.DestroyUnit
 * @see Spring.CreateUnit
 * @param unitID integer
 * @param selfd boolean? (Default: `false`) makes the unit act like it self-destructed.
 * @param reclaimed boolean? (Default: `false`) don't show any DeathSequences, don't leave a wreckage. This does not give back the resources to the team!
 * @param attackerID integer?
 * @param cleanupImmediately boolean? (Default: `false`) stronger version of reclaimed, removes the unit unconditionally and makes its ID available for immediate reuse (otherwise it takes a few frames)
 * @return nil
 */
int LuaSyncedCtrl::DestroyUnit(lua_State* L)
{
	CheckAllowGameChanges(L); // FIXME -- recursion protection
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const int args = lua_gettop(L); // number of arguments

	const bool selfDestr = luaL_optboolean(L, 2, false);
	const bool reclaimed = luaL_optboolean(L, 3, false);
	const bool recycleID = luaL_optboolean(L, 5, false);

	CUnit* attacker = nullptr;
	if (!lua_isnoneornil(L, 4))
		attacker = ParseUnit(L, __func__, 4);

	if (inDestroyUnit >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "DestroyUnit() recursion is not permitted, max depth: %d", MAX_CMD_RECURSION_DEPTH);

	inDestroyUnit++;

	ASSERT_SYNCED(unit->id);
	unit->ForcedKillUnit(attacker, selfDestr, reclaimed, -CSolidObject::DAMAGE_KILLED_LUA);

	if (recycleID)
		unitHandler.GarbageCollectUnit(unit->id);

	inDestroyUnit--;

	return 0;
}


/***
 * @function Spring.TransferUnit
 * @param unitID integer
 * @param newTeamID integer
 * @param given boolean? (Default: `true`) if false, the unit is captured.
 * @return nil
 */
int LuaSyncedCtrl::TransferUnit(lua_State* L)
{
	CheckAllowGameChanges(L);
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const int newTeam = luaL_checkint(L, 2);
	if (!teamHandler.IsValidTeam(newTeam))
		return 0;

	const CTeam* team = teamHandler.Team(newTeam);
	if (team == nullptr)
		return 0;

	bool given = true;
	if (FullCtrl(L) && lua_isboolean(L, 3))
		given = lua_toboolean(L, 3);

	if (inTransferUnit >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "TransferUnit() recursion is not permitted, max depth: %d", MAX_CMD_RECURSION_DEPTH);

	++ inTransferUnit;
	ASSERT_SYNCED(unit->id);
	ASSERT_SYNCED((int)newTeam);
	ASSERT_SYNCED(given);
	unit->ChangeTeam(newTeam, given ? CUnit::ChangeGiven
	                                : CUnit::ChangeCaptured);
	-- inTransferUnit;
	return 0;
}

/******************************************************************************
 * Unit Control
 * @section unitcontrol
******************************************************************************/

/***
 * @function Spring.SetUnitCosts
 * @param unitID integer
 * @param where table<number,number> keys and values are, respectively and in this order: buildTime=amount, metalCost=amount, energyCost=amount
 * @return nil
 */
int LuaSyncedCtrl::SetUnitCosts(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	if (!lua_istable(L, 2))
		luaL_error(L, "Incorrect arguments to SetUnitCosts");

	constexpr int tableIdx = 2;

	for (lua_pushnil(L); lua_next(L, tableIdx) != 0; lua_pop(L, 1)) {
		if (!lua_israwstring(L, LUA_TABLE_KEY_INDEX) || !lua_isnumber(L, LUA_TABLE_VALUE_INDEX))
			continue;

		switch (hashString(lua_tolstring(L, LUA_TABLE_KEY_INDEX, nullptr))) {
			case hashString("buildTime"): {
				unit->buildTime = std::max(1.0f, lua_tofloat(L, LUA_TABLE_VALUE_INDEX));
			} break;
			case hashString("metalCost"): {
				unit->cost.metal = std::max(1.0f, lua_tofloat(L, LUA_TABLE_VALUE_INDEX));
			} break;
			case hashString("energyCost"): {
				unit->cost.energy = std::max(1.0f, lua_tofloat(L, LUA_TABLE_VALUE_INDEX));
			} break;
			default: {
			} break;
 		}

		ASSERT_SYNCED(unit->buildTime);
		ASSERT_SYNCED(unit->cost.metal);
		ASSERT_SYNCED(unit->cost.energy);
	}

	return 0;
}


static bool SetUnitResourceParam(CUnit* unit, const char* name, float value)
{
	// [u|c][u|m][m|e]
	//
	// unconditional | conditional
	//           use | make
	//         metal | energy

	value *= 0.5f;

	switch (name[0]) {
		case 'u': {
			switch (name[1]) {
				case 'u': {
					if (name[2] == 'm') { unit->resourcesUncondUse.metal  = value; return true; }
					if (name[2] == 'e') { unit->resourcesUncondUse.energy = value; return true; }
				} break;

				case 'm': {
					if (name[2] == 'm') { unit->resourcesUncondMake.metal  = value; return true; }
					if (name[2] == 'e') { unit->resourcesUncondMake.energy = value; return true; }
				} break;

				default: {
				} break;
			}
		} break;

		case 'c': {
			switch (name[1]) {
				case 'u': {
					if (name[2] == 'm') { unit->resourcesCondUse.metal  = value; return true; }
					if (name[2] == 'e') { unit->resourcesCondUse.energy = value; return true; }
				} break;

				case 'm': {
					if (name[2] == 'm') { unit->resourcesCondMake.metal  = value; return true; }
					if (name[2] == 'e') { unit->resourcesCondMake.energy = value; return true; }
				} break;

				default: {
				} break;
			}
		} break;

		default: {
		} break;
	}

	return false;
}


static bool SetUnitStorageParam(CUnit* unit, const char* name, float value)
{
	// [m|e]
	//
	// metal | energy

	SResourcePack newStorage = unit->storage;

	switch (name[0]) {
		case 'm': {
			newStorage.metal  = value;
		} break;

		case 'e': {
			newStorage.energy = value;
		} break;

		default: {
			return false;
		}
	}
	unit->SetStorage(newStorage);
	return true;
}


/***
 * Unit Resourcing
 * @section unitresourcing
 */

/***
 * @function Spring.SetUnitResourcing
 * @param unitID integer
 * @param res string
 * @param amount number
 * @return nil
 */

/***
 * @function Spring.SetUnitResourcing
 * @param unitID integer
 * @param res table<string,number> keys are: "[u|c][u|m][m|e]" unconditional | conditional, use | make, metal | energy. Values are amounts
 * @return nil
 */
int LuaSyncedCtrl::SetUnitResourcing(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	if (lua_israwstring(L, 2)) {
		SetUnitResourceParam(unit, lua_tostring(L, 2), luaL_checkfloat(L, 3));
	} else if (lua_istable(L, 2)) {
		constexpr int tableIdx = 2;

		for (lua_pushnil(L); lua_next(L, tableIdx) != 0; lua_pop(L, 1)) {
			if (!lua_israwstring(L, LUA_TABLE_KEY_INDEX) || !lua_isnumber(L, LUA_TABLE_VALUE_INDEX))
				continue;

			SetUnitResourceParam(unit, lua_tostring(L, LUA_TABLE_KEY_INDEX), lua_tofloat(L, LUA_TABLE_VALUE_INDEX));
		}
	}
	else {
		luaL_error(L, "Incorrect arguments to SetUnitResourcing");
	}

	return 0;
}


/***
 * Unit Storage
 * @section unitstorage
 */

/***
 * @function Spring.SetUnitStorage
 * @param unitID integer
 * @param res string
 * @param amount number
 */

/***
 * @function Spring.SetUnitStorage
 * @param unitID integer
 * @param res ResourceUsage keys are: "[m|e]" metal | energy. Values are amounts
 */
int LuaSyncedCtrl::SetUnitStorage(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	if (lua_israwstring(L, 2)) {
		SetUnitStorageParam(unit, lua_tostring(L, 2), luaL_checkfloat(L, 3));
	} else if (lua_istable(L, 2)) {
		constexpr int tableIdx = 2;

		for (lua_pushnil(L); lua_next(L, tableIdx) != 0; lua_pop(L, 1)) {
			if (!lua_israwstring(L, LUA_TABLE_KEY_INDEX) || !lua_isnumber(L, LUA_TABLE_VALUE_INDEX))
				continue;

			SetUnitStorageParam(unit, lua_tostring(L, LUA_TABLE_KEY_INDEX), lua_tofloat(L, LUA_TABLE_VALUE_INDEX));
		}
	}
	else {
		luaL_error(L, "Incorrect arguments to SetUnitStorage");
	}

	return 0;
}


/***
 * @function Spring.SetUnitTooltip
 * @param unitID integer
 * @param tooltip string
 * @return nil
 */
int LuaSyncedCtrl::SetUnitTooltip(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const char* tmp = luaL_checkstring(L, 2);

	if (tmp != nullptr)
		unitToolTipMap.Set(unit->id, std::string(tmp, lua_strlen(L, 2)));
	else
		unitToolTipMap.Set(unit->id, "");

	return 0;
}

/***
 * @class SetUnitHealthAmounts
 * @x_helper
 * @field health number? Set the unit's health.
 * @field capture number? Set the unit's capture progress.
 * @field paralyze number? Set the unit's paralyze damage.
 * @field build number? Set the unit's build progress.
 */

/***
 * @function Spring.SetUnitHealth
 *
 * Note, if your game's custom shading framework doesn't support reverting into nanoframes
 * then reverting into nanoframes via the "build" tag will fail to render properly.
 *
 * @param unitID integer
 * @param health number|SetUnitHealthAmounts If a number, sets the units health
 * to that value. Pass a table to update health, capture progress, paralyze
 * damage, and build progress.
 * @return nil
 * 
 * @see SetUnitHealthAmounts
 */
int LuaSyncedCtrl::SetUnitHealth(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	if (lua_isnumber(L, 2)) {
		unit->health = std::min(unit->maxHealth, lua_tofloat(L, 2));
	} else if (lua_istable(L, 2)) {
		constexpr int tableIdx = 2;

		for (lua_pushnil(L); lua_next(L, tableIdx) != 0; lua_pop(L, 1)) {
			if (!lua_israwstring(L, LUA_TABLE_KEY_INDEX) || !lua_isnumber(L, LUA_TABLE_VALUE_INDEX))
				continue;

			switch (hashString(lua_tolstring(L, LUA_TABLE_KEY_INDEX, nullptr))) {
				case hashString("health"): {
					unit->health = std::min(unit->maxHealth, lua_tofloat(L, LUA_TABLE_VALUE_INDEX));
				} break;
				case hashString("capture"): {
					unit->captureProgress = lua_tofloat(L, LUA_TABLE_VALUE_INDEX);
				} break;
				case hashString("paralyze"): {
					const float argValue = lua_tofloat(L, LUA_TABLE_VALUE_INDEX);
					const float refValue = modInfo.paralyzeOnMaxHealth? unit->maxHealth: unit->health;

					if ((unit->paralyzeDamage = std::max(0.0f, argValue)) > refValue) {
						unit->SetStunned(true);
					} else if (argValue < 0.0f) {
						unit->SetStunned(false);
					}
				} break;
				case hashString("build"): {
					if ((unit->buildProgress = lua_tofloat(L, LUA_TABLE_VALUE_INDEX)) >= 1.0f)
						unit->FinishedBuilding(false);
					else
						unit->TurnIntoNanoframe();
				} break;
				default: {
				} break;
			}
		}
	} else {
		luaL_error(L, "Incorrect arguments to SetUnitHealth()");
	}

	return 0;
}


/***
 * @function Spring.SetUnitMaxHealth
 * @param unitID integer
 * @param maxHealth number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitMaxHealth(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->maxHealth = std::max(0.1f, luaL_checkfloat(L, 2));
	unit->health = std::min(unit->maxHealth, unit->health);
	return 0;
}


/***
 * @function Spring.SetUnitStockpile
 * @param unitID integer
 * @param stockpile number?
 * @param buildPercent number?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitStockpile(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	CWeapon* w = unit->stockpileWeapon;

	if (w == nullptr)
		return 0;

	if (lua_isnumber(L, 2)) {
		w->numStockpiled = std::max(0, luaL_checkint(L, 2));
		unit->commandAI->UpdateStockpileIcon();
	}

	if (lua_isnumber(L, 3))
		unit->stockpileWeapon->buildPercent = std::clamp(lua_tofloat(L, 3), 0.0f, 1.0f);

	return 0;
}


/*** Parameter for weapon states
 *
 * @class WeaponState
 * @x_helper
 * @field reloadState integer?
 * @field reloadFrame integer? Alias for `reloadState`.
 * @field reloadTime number?
 * @field reaimTime integer?
 * @field accuracy number?
 * @field sprayAngle number?
 * @field range number?
 * If you change the range of a weapon with dynamic damage make sure you use `SetUnitWeaponDamages` to change dynDamageRange as well.
 * @field projectileSpeed number?
 * @field burst integer?
 * @field burstRate number?
 * @field projectiles integer?
 * @field salvoLeft integer?
 * @field nextSalvo integer?
 * @field aimReady number?
 * Set to `true` if a non-zero value is passed, `false` is zero is passed.
 * @field forceAim integer?
 * @field avoidFlags integer?
 * @field collisionFlags integer?
 * @field ttl number? How many seconds the projectile should live 
 */

static bool SetSingleUnitWeaponState(lua_State* L, CWeapon* weapon, int index)
{
	// FIXME: missing checks and updates?
	switch (hashString(lua_tolstring(L, index, nullptr))) {
		case hashString("reloadState"):
		case hashString("reloadFrame"): {
			weapon->reloadStatus = lua_toint(L, index + 1);
		} break;

		case hashString("reloadTime"): {
			weapon->reloadTime = std::max(1, (int) (lua_tofloat(L, index + 1) * GAME_SPEED));
		} break;
		case hashString("reaimTime"): {
			weapon->reaimTime = std::max(1, lua_toint(L, index + 1));
		} break;

		case hashString("accuracy"): {
			weapon->accuracyError = lua_tofloat(L, index + 1);
		} break;
		case hashString("sprayAngle"): {
			weapon->sprayAngle = lua_tofloat(L, index + 1);
		} break;

		case hashString("range"): {
			weapon->UpdateRange(lua_tofloat(L, index + 1));
		} break;
		case hashString("projectileSpeed"): {
			weapon->UpdateProjectileSpeed(lua_tofloat(L, index + 1));
		} break;

		case hashString("autoTargetRangeBoost"): {
			weapon->autoTargetRangeBoost = std::max(0.0f, lua_tofloat(L, index + 1));
		} break;

		case hashString("burst"): {
			weapon->salvoSize = lua_toint(L, index + 1);
		} break;
		case hashString("burstRate"): {
			weapon->salvoDelay = (int) (lua_tofloat(L, index + 1) * GAME_SPEED);
		} break;
		case hashString("windup"): {
			weapon->salvoWindup = (int) (lua_tofloat(L, index + 1) * GAME_SPEED);
		} break;

		case hashString("projectiles"): {
			weapon->projectilesPerShot = lua_toint(L, index + 1);
		} break;

		case hashString("salvoLeft"): {
			weapon->salvoLeft = lua_toint(L, index + 1);
		} break;
		case hashString("nextSalvo"): {
			weapon->nextSalvo = lua_toint(L, index + 1);
		} break;

		case hashString("aimReady"): {
			weapon->angleGood = (lua_tofloat(L, index + 1) != 0.0f);
		} break;

		case hashString("forceAim"): {
			// move into the past by default s.t. Weapon::CallAimingScript runs the callin next Update
			weapon->lastAimedFrame -= luaL_optint(L, index + 1, weapon->reaimTime);
		} break;

		case hashString("avoidFlags"): {
			weapon->avoidFlags = lua_toint(L, index + 1);
		} break;
		case hashString("collisionFlags"): {
			weapon->collisionFlags = lua_toint(L, index + 1);
		} break;

		case hashString("ttl"): {
			weapon->ttl = (int) (lua_tonumber(L, index + 1) * GAME_SPEED);
		} break;

		default: {
			return false;
		} break;
	}

	return true;
}


/***
 *
 * @function Spring.SetUnitUseWeapons
 * @param unitID integer
 * @param forceUseWeapons number?
 * @param allowUseWeapons number?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitUseWeapons(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->forceUseWeapons = luaL_optboolean(L, 2, unit->forceUseWeapons);
	unit->allowUseWeapons = luaL_optboolean(L, 3, unit->allowUseWeapons);
	return 0;
}

/***
 * @function Spring.SetUnitWeaponState
 * @param unitID integer
 * @param weaponNum number
 * @param states WeaponState
 * @return nil
 */

/***
 * @function Spring.SetUnitWeaponState
 * @param unitID integer
 * @param weaponNum number
 * @param key string
 * @param value number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitWeaponState(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const size_t weaponNum = luaL_checkint(L, 2) - LUA_WEAPON_BASE_INDEX;

	if (weaponNum >= unit->weapons.size())
		return 0;

	CWeapon* weapon = unit->weapons[weaponNum];

	if (lua_istable(L, 3)) {
		// {key1 = value1, ...}
		for (lua_pushnil(L); lua_next(L, 3) != 0; lua_pop(L, 1)) {
			if (lua_israwstring(L, LUA_TABLE_KEY_INDEX) && lua_isnumber(L, LUA_TABLE_VALUE_INDEX)) {
				SetSingleUnitWeaponState(L, weapon, LUA_TABLE_KEY_INDEX);
			}
		}
	} else {
		// key, value
		if (lua_israwstring(L, 3) && lua_isnumber(L, 4)) {
			SetSingleUnitWeaponState(L, weapon, 3);
		}
	}

	return 0;
}

/*** Parameters for damage
 *
 * @class WeaponDamages
 * @x_helper
 * @field paralyzeDamageTime integer
 * @field impulseFactor number
 * @field impulseBoost number
 * @field craterMult number
 * @field craterBoost number
 * @field dynDamageExp number
 * @field dynDamageMin number
 * @field dynDamageRange number
 * @field dynDamageInverted number
 * Set to `true` if a non-zero value is passed, `false` is zero is passed.
 * @field craterAreaOfEffect number
 * @field damageAreaOfEffect number
 * @field edgeEffectiveness number
 * @field explosionSpeed number
 */

static int SetSingleDynDamagesKey(lua_State* L, DynDamageArray* damages, int index)
{
	const float value = lua_tofloat(L, index + 1);

	if (lua_isnumber(L, index)) {
		const unsigned armType = lua_toint(L, index);

		if (armType < damages->GetNumTypes())
			damages->Set(armType, value);

		return 0;
	}

	// FIXME: missing checks and updates?
	switch (hashString(lua_tostring(L, index))) {
		case hashString("paralyzeDamageTime"): {
			damages->paralyzeDamageTime = std::max((int)value, 0);
		} break;

		case hashString("impulseFactor"): {
			damages->impulseFactor = value;
		} break;
		case hashString("impulseBoost"): {
			damages->impulseBoost = value;
		} break;

		case hashString("craterMult"): {
			damages->craterMult = value;
		} break;
		case hashString("craterBoost"): {
			damages->craterBoost = value;
		} break;

		case hashString("dynDamageExp"): {
			damages->dynDamageExp = value;
		} break;
		case hashString("dynDamageMin"): {
			damages->dynDamageMin = value;
		} break;
		case hashString("dynDamageRange"): {
			damages->dynDamageRange = value;
		} break;
		case hashString("dynDamageInverted"): {
			// HACK, this should be set to result of lua_toboolean
			damages->dynDamageInverted = (value != 0.0f);
		} break;

		case hashString("craterAreaOfEffect"): {
			damages->craterAreaOfEffect = value;
		} break;
		case hashString("damageAreaOfEffect"): {
			damages->damageAreaOfEffect = value;
		} break;

		case hashString("edgeEffectiveness"): {
			damages->edgeEffectiveness = std::min(value, 1.0f);
		} break;
		case hashString("explosionSpeed"): {
			damages->explosionSpeed = value;
		} break;

		default: {
		} break;
	}

	return 0;
}


/***
 * @function Spring.SetUnitWeaponDamages
 * @param unitID integer
 * @param weaponNum number|"selfDestruct"|"explode"
 * @param damages WeaponDamages
 * @return nil
 */
/***
 * @function Spring.SetUnitWeaponDamages
 * @param unitID integer
 * @param weaponNum number|"selfDestruct"|"explode"
 * @param key string
 * @param value number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitWeaponDamages(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	DynDamageArray* damages;

	if (lua_israwstring(L, 2)) {
		switch (hashString(lua_tostring(L, 2))) {
			case hashString("explode"     ): { damages = DynDamageArray::GetMutable(unit->deathExpDamages); } break;
			case hashString("selfDestruct"): { damages = DynDamageArray::GetMutable(unit->selfdExpDamages); } break;
			default                        : {                                                    return 0; } break;
		}
	} else {
		const size_t weaponNum = luaL_checkint(L, 2) - LUA_WEAPON_BASE_INDEX;

		if (weaponNum >= unit->weapons.size())
			return 0;

		CWeapon* weapon = unit->weapons[weaponNum];

		damages = DynDamageArray::GetMutable(weapon->damages);
	}


	if (lua_istable(L, 3)) {
		// {key1 = value1, ...}
		for (lua_pushnil(L); lua_next(L, 3) != 0; lua_pop(L, 1)) {
			if ((lua_isnumber(L, LUA_TABLE_KEY_INDEX) || lua_israwstring(L, LUA_TABLE_KEY_INDEX)) && lua_isnumber(L, LUA_TABLE_VALUE_INDEX)) {
				SetSingleDynDamagesKey(L, damages, LUA_TABLE_KEY_INDEX);
			}
		}
	} else {
		// key, value
		if ((lua_isnumber(L, 3) || lua_israwstring(L, 3)) && lua_isnumber(L, 4)) {
			SetSingleDynDamagesKey(L, damages, 3);
		}
	}

	return 0;
}


/*** @function Spring.SetUnitMaxRange
 *
 * @param unitID integer
 * @param maxRange number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitMaxRange(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->maxRange = std::max(0.0f, luaL_checkfloat(L, 2));
	return 0;
}


/***
 * @function Spring.SetUnitExperience
 * @see Spring.AddUnitExperience
 * @see Spring.GetUnitExperience
 * @param unitID integer
 * @param experience number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitExperience(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->AddExperience(std::max(0.0f, luaL_checkfloat(L, 2)) - unit->experience);
	return 0;
}

/***
 * @function Spring.AddUnitExperience
 * @see Spring.SetUnitExperience
 * @see Spring.GetUnitExperience
 * @param unitID integer
 * @param deltaExperience number Can be negative to subtract, but the unit will never have negative total afterwards
 * @return nil
 */
int LuaSyncedCtrl::AddUnitExperience(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	// can subtract, but the result can't be negative
	unit->AddExperience(std::max(-unit->experience, luaL_checkfloat(L, 2)));
	return 0;
}


/***
 * @function Spring.SetUnitArmored
 * @param unitID integer
 * @param armored boolean?
 * @param armorMultiple number?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitArmored(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	if (lua_isboolean(L, 2))
		unit->armoredState = lua_toboolean(L, 2);

	unit->armoredMultiple = luaL_optfloat(L, 3, unit->armoredMultiple);

	if (lua_toboolean(L, 2)) {
		unit->curArmorMultiple = unit->armoredMultiple;
	} else {
		unit->curArmorMultiple = 1.0f;
	}
	return 0;
}

/******************************************************************************
 * Unit LOS
 * @section unitlos
******************************************************************************/


static unsigned char ParseLosBits(lua_State* L, int index, unsigned char bits)
{
	if (lua_isnumber(L, index))
		return (unsigned char)lua_tonumber(L, index);

	if (lua_istable(L, index)) {
		for (lua_pushnil(L); lua_next(L, index) != 0; lua_pop(L, 1)) {
			if (!lua_israwstring(L, LUA_TABLE_KEY_INDEX)) { luaL_error(L, "bad key type");   }
			if (!lua_isboolean(L, LUA_TABLE_VALUE_INDEX))   { luaL_error(L, "bad value type"); }

			const bool set = lua_toboolean(L, LUA_TABLE_VALUE_INDEX);

			switch (hashString(lua_tostring(L, LUA_TABLE_KEY_INDEX))) {
				case hashString("los"): {
					if (set) { bits |=  LOS_INLOS; }
					else     { bits &= ~LOS_INLOS; }
				} break;
				case hashString("radar"): {
					if (set) { bits |=  LOS_INRADAR; }
					else     { bits &= ~LOS_INRADAR; }
				} break;
				case hashString("prevLos"): {
					if (set) { bits |=  LOS_PREVLOS; }
					else     { bits &= ~LOS_PREVLOS; }
				} break;
				case hashString("contRadar"): {
					if (set) { bits |=  LOS_CONTRADAR; }
					else     { bits &= ~LOS_CONTRADAR; }
				} break;
				default: {
				} break;
			}
		}
		return bits;
	}

 	luaL_error(L, "ERROR: expected number or table");
	return 0;
}

/***
 * @alias LosTable table<"los"|"radar"|"prevLos"|"contRadar",boolean>
 */

/***
 * @enum LosMask
 * @x_helper
 * @field INLOS 1 the unit is currently in the los of the allyteam
 * @field INRADAR 2 the unit is currently in radar from the allyteam
 * @field PREVLOS 4 the unit has previously been in los from the allyteam
 * @field CONTRADAR 8 the unit has continuously been in radar since it was last inlos by the allyteam
 */

/*** Set visibility status mask for a unit and team
 *
 * Use this to allow or disallow a unit from having its visibility status
 * against a certain team updated by the engine.
 *
 * @see Spring.SetUnitLosState
 * @function Spring.SetUnitLosMask
 *
 * @param unitID integer
 * @param allyTeam number
 * @param losTypes LosTable|LosMask|integer A bitmask of `LosMask` bits or a
 * table. True bits disable engine updates to visibility.
 */
int LuaSyncedCtrl::SetUnitLosMask(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const int allyTeam = luaL_checkint(L, 2);

	if (!teamHandler.IsValidAllyTeam(allyTeam))
		luaL_error(L, "bad allyTeam");

	const unsigned char losStatus = unit->losStatus[allyTeam];
	const unsigned char  oldMask = losStatus >> LOS_MASK_SHIFT;
	const unsigned char  newMask = ParseLosBits(L, 3, oldMask);
	const unsigned char state = (newMask << LOS_MASK_SHIFT) | (losStatus & 0x0F);

	unit->losStatus[allyTeam] = state;
	unit->SetLosStatus(allyTeam, unit->CalcLosStatus(allyTeam));

	return 0;
}


/*** Set current visibility status for a unit and team
 *
 * Note this state will not be persisted if the visibility state is not masked.
 *
 * Use it to change visibility state, once you set the unit to not receive
 * engine visibility updates.
 *
 * A few notes on certain bits:
 *
 * - `contradar`: True when the unit entered los at some point, remains set
 *   until radar is lost. Useful for tracking the unit type in radar, if you
 *   lost los you still know what unit type that radar dot refers to.
 * - `prevlos`: True when the unit has entered los at least once, useful for
 *   tracking building locations (controls whether a ghost appears or not in
 *   the location)
 *
 * @see Spring.SetUnitLosMask
 * @function Spring.SetUnitLosState
 * @param unitID integer
 * @param allyTeam number
 * @param losTypes LosTable|LosMask|integer A bitmask of `LosMask` bits or a
 * table
 */
int LuaSyncedCtrl::SetUnitLosState(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const int allyTeam = luaL_checkint(L, 2);

	if (!teamHandler.IsValidAllyTeam(allyTeam))
		luaL_error(L, "bad allyTeam");

	const unsigned char losStatus = unit->losStatus[allyTeam];
	const unsigned char  oldState = losStatus & 0x0F;
	const unsigned char  newState = ParseLosBits(L, 3, oldState);

	unit->SetLosStatus(allyTeam, (losStatus & 0xF0) | newState);
	return 0;
}


/***
 * @function Spring.SetUnitCloak
 *
 * If the 2nd argument is a number, the value works like this:
 * 1:=normal cloak
 * 2:=for free cloak (cost no E)
 * 3:=for free + no decloaking (except the unit is stunned)
 * 4:=ultimate cloak (no ecost, no decloaking, no stunned decloak)
 *
 * The decloak distance is only changed:
 * - if the 3th argument is a number or a boolean.
 * - if the boolean is false it takes the default decloak distance for that unitdef,
 * - if the boolean is true it takes the absolute value of it.
 *
 * @param unitID integer
 * @param cloak boolean|number
 * @param cloakArg boolean|number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitCloak(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	// make unit {de}cloak at next SlowUpdate
	if (lua_isboolean(L, 2))
		unit->wantCloak = lua_toboolean(L, 2);
	if (lua_isnumber(L, 2))
		unit->wantCloak = (lua_tonumber(L, 2) != 0);

	if (lua_israwnumber(L, 3)) {
		unit->decloakDistance = lua_tofloat(L, 3);
		return 0;
	}

	if (lua_isboolean(L, 3)) {
		if (lua_toboolean(L, 3)) {
			unit->decloakDistance = math::fabsf(unit->unitDef->decloakDistance);
		} else {
			unit->decloakDistance = unit->unitDef->decloakDistance;
		}
	}

	return 0;
}


/***
 * @function Spring.SetUnitStealth
 * @param unitID integer
 * @param stealth boolean
 * @return nil
 */
int LuaSyncedCtrl::SetUnitStealth(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->stealth = luaL_checkboolean(L, 2);
	return 0;
}


/***
 * @function Spring.SetUnitSonarStealth
 * @param unitID integer
 * @param sonarStealth boolean
 * @return nil
 */
int LuaSyncedCtrl::SetUnitSonarStealth(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->sonarStealth = luaL_checkboolean(L, 2);
	return 0;
}

/***
 * @function Spring.SetUnitSeismicSignature
 * @param unitID integer
 * @param seismicSignature number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitSeismicSignature(lua_State* L)
{
	CUnit* const unit = ParseUnit(L, __func__, 1);
	if (unit == nullptr)
		return 0;

	unit->seismicSignature = luaL_checkfloat(L, 2);
	return 0;
}

/*** Set whether unit leaves static radar ghosts.
 *
 * @function Spring.SetUnitLeavesGhost
 *
 * @number unitID
 * @param leavesGhost boolean
 * @param leaveDeadGhost boolean? (Default: `false`) leave a dead ghost behind if disabling and the unit had a live static ghost.
 */
int LuaSyncedCtrl::SetUnitLeavesGhost(lua_State* L)
{
	if (!gameSetup->ghostedBuildings)
		return 0;

	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->SetLeavesGhost(luaL_checkboolean(L, 2), luaL_optboolean(L, 3, false));
	return 0;
}

/***
 * @function Spring.SetUnitAlwaysVisible
 * @param unitID integer
 * @param alwaysVisible boolean
 * @return nil
 */
int LuaSyncedCtrl::SetUnitAlwaysVisible(lua_State* L)
{
	return (SetWorldObjectAlwaysVisible(L, ParseUnit(L, __func__, 1), __func__));
}


/***
 *
 * @function Spring.SetUnitUseAirLos
 * @param unitID integer
 * @param useAirLos boolean
 * @return nil
 */
int LuaSyncedCtrl::SetUnitUseAirLos(lua_State* L)
{
	return (SetWorldObjectUseAirLos(L, ParseUnit(L, __func__, 1), __func__));
}


/***
 * @function Spring.SetUnitMetalExtraction
 * @param unitID integer
 * @param depth number corresponds to metal extraction rate
 * @param range number? similar to "extractsMetal" in unitDefs.
 * @return nil
 */
int LuaSyncedCtrl::SetUnitMetalExtraction(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	auto mex = extractorHandler.TryGetExtractor(unit);

	if (mex == nullptr)
		return 0;

	const float depth = luaL_checkfloat(L, 2);
	const float range = luaL_optfloat(L, 3, mex->GetExtractionRange());
	mex->ResetExtraction();
	mex->SetExtractionRangeAndDepth(range, depth);
	return 0;
}


/*** See also harvestStorage UnitDef tag.
 *
 * @function Spring.SetUnitHarvestStorage
 * @param unitID integer
 * @param metal number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitHarvestStorage(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	for (int i = 0; i < SResourcePack::MAX_RESOURCES; ++i) {
		unit->harvested[i]       = luaL_optfloat(L, 2 + i * 2,     unit->harvested[i]);
		unit->harvestStorage[i]  = luaL_optfloat(L, 2 + i * 2 + 1, unit->harvestStorage[i]);
	}
	return 0;
}

/***
 *
 * @function Spring.SetUnitBuildParams
 * @param unitID integer
 * @param paramName string one of `buildRange`|`buildDistance`|`buildRange3D`
 * @param value number|boolean boolean when `paramName` is `buildRange3D`, otherwise number.
 * @return nil
 */
int LuaSyncedCtrl::SetUnitBuildParams(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	CBuilder* builder = dynamic_cast<CBuilder*>(unit);

	if (builder == nullptr)
		return 0;

	switch (hashString(luaL_checkstring(L, 2))) {
		case hashString("buildRange"):
		case hashString("buildDistance"): {
			builder->buildDistance = luaL_optfloat(L, 3, builder->buildDistance);
		} break;
		case hashString("buildRange3D"): {
			builder->range3D = luaL_optboolean(L, 3, builder->range3D);
		} break;
		default: {} break;
	};

	return 0;
}

/***
 * @function Spring.SetUnitBuildSpeed
 * @param builderID integer
 * @param buildSpeed number
 * @param repairSpeed number?
 * @param reclaimSpeed number?
 * @param captureSpeed number?
 * @param terraformSpeed number?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitBuildSpeed(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const float buildSpeed = INV_GAME_SPEED * max(0.0f, luaL_checkfloat(L, 2));

	CFactory* factory = dynamic_cast<CFactory*>(unit);

	if (factory != nullptr) {
		factory->buildSpeed = buildSpeed;
		return 0;
	}

	CBuilder* builder = dynamic_cast<CBuilder*>(unit);

	if (builder == nullptr)
		return 0;

	builder->buildSpeed = buildSpeed;
	if (lua_isnumber(L, 3)) {
		builder->repairSpeed    = INV_GAME_SPEED * max(0.0f, lua_tofloat(L, 3));
	}
	if (lua_isnumber(L, 4)) {
		builder->reclaimSpeed   = INV_GAME_SPEED * max(0.0f, lua_tofloat(L, 4));
	}
	if (lua_isnumber(L, 5)) {
		builder->resurrectSpeed = INV_GAME_SPEED * max(0.0f, lua_tofloat(L, 5));
	}
	if (lua_isnumber(L, 6)) {
		builder->captureSpeed   = INV_GAME_SPEED * max(0.0f, lua_tofloat(L, 6));
	}
	if (lua_isnumber(L, 7)) {
		builder->terraformSpeed = INV_GAME_SPEED * max(0.0f, lua_tofloat(L, 7));
	}
	return 0;
}


/***
 * @function Spring.SetUnitNanoPieces
 *
 * This saves a lot of engine calls, by replacing: function script.QueryNanoPiece() return currentpiece end
 * Use it!
 *
 * @param builderID integer
 * @param pieces table
 * @return nil
 *
 */
int LuaSyncedCtrl::SetUnitNanoPieces(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	NanoPieceCache* pieceCache = nullptr;
	std::vector<int>* nanoPieces = nullptr;

	{
		CBuilder* builder = dynamic_cast<CBuilder*>(unit);

		if (builder != nullptr) {
			pieceCache = &builder->GetNanoPieceCache();
			nanoPieces = &pieceCache->GetNanoPieces();
		}

		CFactory* factory = dynamic_cast<CFactory*>(unit);

		if (factory != nullptr) {
			pieceCache = &factory->GetNanoPieceCache();
			nanoPieces = &pieceCache->GetNanoPieces();
		}
	}

	if (nanoPieces == nullptr)
		return 0;

	nanoPieces->clear();
	pieceCache->StopPolling();
	luaL_checktype(L, 2, LUA_TTABLE);

	for (lua_pushnil(L); lua_next(L, 2) != 0; lua_pop(L, 1)) {
		if (lua_israwnumber(L, LUA_TABLE_VALUE_INDEX)) {
			const int modelPieceNum = lua_toint(L, LUA_TABLE_VALUE_INDEX) - 1; //lua 1-indexed, c++ 0-indexed

			if (unit->localModel.HasPiece(modelPieceNum)) {
				nanoPieces->push_back(modelPieceNum);
			} else {
				luaL_error(L, "[SetUnitNanoPieces] incorrect model-piece number %d", modelPieceNum);
			}
		}
	}

	return 0;
}


/***
 * @function Spring.SetUnitBlocking
 * @param unitID integer
 * @param isBlocking boolean? If `true` add this unit to the `GroundBlockingMap`, but only if it collides with solid objects (or is being set to collide with the `isSolidObjectCollidable` argument). If `false`, remove this unit from the `GroundBlockingMap`. No change if `nil`.
 * @param isSolidObjectCollidable boolean? Enable or disable collision with solid objects, or no change if `nil`.
 * @param isProjectileCollidable boolean? Enable or disable collision with projectiles, or no change if `nil`.
 * @param isRaySegmentCollidable boolean? Enable or disable collision with ray segments, or no change if `nil`.
 * @param crushable boolean? Enable or disable crushable, or no change if `nil`.
 * @param blockEnemyPushing boolean? Enable or disable blocking enemy pushing, or no change if `nil`.
 * @param blockHeightChanges boolean? Enable or disable blocking height changes, or no change if `nil`.
 * @return boolean isBlocking
 */
int LuaSyncedCtrl::SetUnitBlocking(lua_State* L)
{
	return (SetSolidObjectBlocking(L, ParseUnit(L, __func__, 1)));
}


/***
 * @function Spring.SetUnitCrashing
 * @param unitID integer
 * @param crashing boolean
 * @return boolean success
 */
int LuaSyncedCtrl::SetUnitCrashing(lua_State* L) {
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	AAirMoveType* amt = dynamic_cast<AAirMoveType*>(unit->moveType);
	bool ret = false;

	if (amt != nullptr) {
		const bool wantCrash = luaL_optboolean(L, 2, false);
		const AAirMoveType::AircraftState aircraftState = amt->aircraftState;

		// for simplicity, this can only set a non-landed aircraft to
		// start crashing, or a crashing aircraft to start flying
		if ( wantCrash && (aircraftState != AAirMoveType::AIRCRAFT_LANDED))
			amt->SetState(AAirMoveType::AIRCRAFT_CRASHING);

		if (!wantCrash && (aircraftState == AAirMoveType::AIRCRAFT_CRASHING))
			amt->SetState(AAirMoveType::AIRCRAFT_FLYING);

		ret = (amt->aircraftState != aircraftState);
	}

	lua_pushboolean(L, ret);
	return 1;
}


/***
 * @function Spring.SetUnitShieldState
 * @param unitID integer
 * @param weaponID integer? (Default: `-1`)
 * @param enabled boolean?
 * @param power number?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitShieldState(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	int args = lua_gettop(L);
	int arg = 2;

	CPlasmaRepulser* shield = static_cast<CPlasmaRepulser*>(unit->shieldWeapon);

	if (lua_isnumber(L, 2) && args > 2) {
		const size_t idx = luaL_optint(L, 2, -1) - LUA_WEAPON_BASE_INDEX;

		if (idx < unit->weapons.size()) {
			shield = dynamic_cast<CPlasmaRepulser*>(unit->weapons[idx]);
		}

		arg++;
	}

	if (shield == nullptr)
		return 0;

	if (lua_isboolean(L, arg)) { shield->SetEnabled(lua_toboolean(L, arg)); arg++; }
	if (lua_isnumber(L, arg)) { shield->SetCurPower(lua_tofloat(L, arg)); }
	return 0;
}

/***
 * @function Spring.SetUnitShieldRechargeDelay
 * @param unitID integer
 * @param weaponID integer? (optional if the unit only has one shield)
 * @param rechargeTime number? (in seconds; emulates a regular hit if nil)
 * @return nil
 */
int LuaSyncedCtrl::SetUnitShieldRechargeDelay(lua_State* L)
{
	const auto unit = ParseUnit(L, __func__, 1);
	if (unit == nullptr)
		return 0;

	auto shield = static_cast <CPlasmaRepulser*> (unit->shieldWeapon);
	if (lua_isnumber(L, 2)) {
		const size_t index = lua_tointeger(L, 2) - LUA_WEAPON_BASE_INDEX;
		if (index < unit->weapons.size())
			shield = dynamic_cast <CPlasmaRepulser*> (unit->weapons[index]);
	}
	if (shield == nullptr)
		return 0;

	if (lua_isnumber(L, 3)) {
		const auto seconds = lua_tofloat(L, 3);
		const auto frames = static_cast <int> (seconds * GAME_SPEED);
		shield->SetRechargeDelay(frames, true);
	} else {
		/* Note, overwrite set to false on purpose. This is to emulate a regular
		 * weapon hit. This lets a sophisticated shield handler gadget coexist
		 * with a basic "emulate hits" gadget without the latter having to care.
		 * You can put the weaponDef value explicitly if you want to overwrite. */
		shield->SetRechargeDelay(shield->weaponDef->shieldRechargeDelay, false);
	}

	return 0;
}

/***
 * @function Spring.SetUnitFlanking
 * @param unitID integer
 * @param type string "dir"|"minDamage"|"maxDamage"|"moveFactor"|"mode"
 * @param arg1 number x|minDamage|maxDamage|moveFactor|mode
 * @param y number? only when type is "dir"
 * @param z number? only when type is "dir"
 * @return nil
 */
int LuaSyncedCtrl::SetUnitFlanking(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const string key = luaL_checkstring(L, 2);

	if (key == "mode") {
		unit->flankingBonusMode = luaL_checkint(L, 3);
	}
	else if (key == "dir") {
		float3 dir(luaL_checkfloat(L, 3),
		           luaL_checkfloat(L, 4),
		           luaL_checkfloat(L, 5));
		unit->flankingBonusDir = dir.Normalize();
	}
	else if (key == "moveFactor") {
		unit->flankingBonusMobilityAdd = luaL_checkfloat(L, 3);
	}
	else if (key == "minDamage") {
		const float minDamage = luaL_checkfloat(L, 3);
		const float maxDamage = unit->flankingBonusAvgDamage +
		                        unit->flankingBonusDifDamage;
		unit->flankingBonusAvgDamage = (maxDamage + minDamage) * 0.5f;
		unit->flankingBonusDifDamage = (maxDamage - minDamage) * 0.5f;
	}
	else if (key == "maxDamage") {
		const float maxDamage = luaL_checkfloat(L, 3);
		const float minDamage = unit->flankingBonusAvgDamage -
		                        unit->flankingBonusDifDamage;
		unit->flankingBonusAvgDamage = (maxDamage + minDamage) * 0.5f;
		unit->flankingBonusDifDamage = (maxDamage - minDamage) * 0.5f;
	}
	return 0;
}

/***
 * @function Spring.SetUnitPhysicalStateBit
 * @param unitID integer
 * @param Physical number[bit] state bit
 * @return nil
 */
int LuaSyncedCtrl::SetUnitPhysicalStateBit(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	int statebit = luaL_checkint(L, 2);

	unit->SetPhysicalStateBit(statebit);
	return 0;
}

/***
 * @function Spring.GetUnitPhysicalState
 * @param unitID integer
 * @return number Unit's PhysicalState bitmask
 */
int LuaSyncedCtrl::GetUnitPhysicalState(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	lua_pushnumber(L, unit->physicalState);
	return 1;
}



int LuaSyncedCtrl::SetUnitTravel(lua_State* L) { return 0; } // FIXME: DELETE ME
int LuaSyncedCtrl::SetUnitFuel(lua_State* L) { return 0; } // FIXME: DELETE ME



/***
 *
 * @function Spring.SetUnitNeutral
 *
 * @param unitID integer
 * @param neutral boolean
 * @return nil|boolean setNeutral
 */
int LuaSyncedCtrl::SetUnitNeutral(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->SetNeutral(luaL_checkboolean(L, 2));
	return 0;
}


/*** Defines a unit's target.
 *
 * @function Spring.SetUnitTarget
 * @param unitID integer
 * @param enemyUnitID integer? when nil drops the units current target.
 * @param dgun boolean? (Default: `false`)
 * @param userTarget boolean? (Default: `false`)
 * @param weaponNum number? (Default: `-1`)
 * @return boolean success
 */

/***
 * @function Spring.SetUnitTarget
 * @param unitID integer
 * @param x number? when nil or not passed it will drop target and ignore other parameters
 * @param y number?
 * @param z number?
 * @param dgun boolean? (Default: `false`)
 * @param userTarget boolean? (Default: `false`)
 * @param weaponNum number? (Default: `-1`)
 * @return boolean success
 */
int LuaSyncedCtrl::SetUnitTarget(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const int args = lua_gettop(L);
	if (lua_isnil(L, 2)) {
		unit->DropCurrentAttackTarget();
		lua_pushboolean(L, true);
		return 1;
	}
	if (args >= 4 && !lua_isboolean(L, 3)) {
		const float3 pos(luaL_checkfloat(L, 2),
		                 luaL_checkfloat(L, 3),
		                 luaL_checkfloat(L, 4));
		const bool manualFire = luaL_optboolean(L, 5, false);
		const bool userTarget = luaL_optboolean(L, 6, false);
		const int weaponNum = luaL_optint(L, 7, 0) - LUA_WEAPON_BASE_INDEX;
		bool ret = false;
		if (weaponNum < 0) {
			ret = unit->AttackGround(pos, userTarget, manualFire);
		} else if (weaponNum < unit->weapons.size()) {
			SWeaponTarget trg(pos, userTarget);
			trg.isManualFire = manualFire;
			ret = unit->weapons[weaponNum]->Attack(trg);
		}
		lua_pushboolean(L, ret);
		return 1;
	}
	else if (args >= 2) {
		CUnit* target = ParseRawUnit(L, __func__, 2);

		if (target == unit) {
			luaL_error(L, "[%s()]: unit tried to attack itself", __func__);
			return 0;
		}

		const bool manualFire = luaL_optboolean(L, 3, false);
		const bool userTarget = luaL_optboolean(L, 4, false);
		const int weaponNum = luaL_optint(L, 5, -1) - LUA_WEAPON_BASE_INDEX;
		bool ret = false;
		if (weaponNum < 0) {
			ret = unit->AttackUnit(target, userTarget, manualFire);
		} else if (weaponNum < unit->weapons.size()) {
			SWeaponTarget trg(target, userTarget);
			trg.isManualFire = manualFire;
			ret = unit->weapons[weaponNum]->Attack(trg);
		}
		lua_pushboolean(L, ret);
		return 1;
	}
	return 0;
}



/***
 * @function Spring.SetUnitMidAndAimPos
 * @param unitID integer
 * @param mpX number new middle positionX of unit
 * @param mpY number new middle positionY of unit
 * @param mpZ number new middle positionZ of unit
 * @param apX number new positionX that enemies aim at on this unit
 * @param apY number new positionY that enemies aim at on this unit
 * @param apZ number new positionZ that enemies aim at on this unit
 * @param relative boolean? (Default: `false`) are the new coordinates relative to world (false) or unit (true) coordinates? Also, note that apy is inverted!
 * @return boolean success
 */
int LuaSyncedCtrl::SetUnitMidAndAimPos(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr) {
		lua_pushboolean(L, false);
		return 1;
	}

	#define FLOAT(i) luaL_checkfloat(L, i)
	#define FLOAT3(i, j, k) float3(FLOAT(i), FLOAT(j), FLOAT(k))

	const int argc = lua_gettop(L);
	const float3 newMidPos = (argc >= 4)? FLOAT3(2, 3, 4): float3(unit->midPos);
	const float3 newAimPos = (argc >= 7)? FLOAT3(5, 6, 7): float3(unit->aimPos);
	const bool setRelative = luaL_optboolean(L, 8, false);
	const bool updateQuads = (newMidPos != unit->midPos);

	#undef FLOAT3
	#undef FLOAT

	if (updateQuads) {
		// safety, possibly just need MovedUnit
		quadField.RemoveUnit(unit);
	}

	unit->SetMidAndAimPos(newMidPos, newAimPos, setRelative);

	if (updateQuads) {
		quadField.MovedUnit(unit);
	}

	lua_pushboolean(L, true);
	return 1;
}

/***
 * @function Spring.SetUnitRadiusAndHeight
 * @param unitID integer
 * @param radius number
 * @param height number
 * @return boolean success
 */
int LuaSyncedCtrl::SetUnitRadiusAndHeight(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr) {
		lua_pushboolean(L, false);
		return 1;
	}

	const float newRadius = std::max(1.0f, luaL_optfloat(L, 2, unit->radius));
	const float newHeight = std::max(1.0f, luaL_optfloat(L, 3, unit->height));
	const bool updateQuads = (newRadius != unit->radius);

	if (updateQuads) {
		// safety, possibly just need MovedUnit
		quadField.RemoveUnit(unit);
	}

	unit->SetRadiusAndHeight(newRadius, newHeight);

	if (updateQuads) {
		quadField.MovedUnit(unit);
	}

	lua_pushboolean(L, true);
	return 1;
}


/***
 * @function Spring.SetUnitBuildeeRadius
 * Sets the unit's radius for when targeted by build, repair, reclaim-type commands.
 * @param unitID integer
 * @param build number radius for when targeted by build, repair, reclaim-type commands.
 * @return nil
 */
int LuaSyncedCtrl::SetUnitBuildeeRadius(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->buildeeRadius = std::max(0.0f, luaL_checkfloat(L, 2));

	return 0;
}


/*** Changes the pieces hierarchy of a unit by attaching a piece to a new parent.
 *
 * @function Spring.SetUnitPieceParent
 * @param unitID integer
 * @param AlteredPiece number
 * @param ParentPiece number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitPieceParent(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	LocalModelPiece* childPiece = ParseObjectLocalModelPiece(L, unit, 2);

	if (childPiece == nullptr) {
		luaL_error(L, "invalid piece");
		return 0;
	}

	LocalModelPiece* parentPiece = ParseObjectLocalModelPiece(L, unit, 3);

	if (parentPiece == nullptr) {
		luaL_error(L, "invalid parent piece");
		return 0;
	}

	if (childPiece == unit->localModel.GetRoot()) {
		luaL_error(L, "Can't change a root piece's parent");
		return 0;
	}

	childPiece->parent->RemoveChild(childPiece);
	childPiece->SetParent(parentPiece);
	parentPiece->AddChild(childPiece);
	return 0;
}


/*** Sets the local (i.e. parent-relative) matrix of the given piece.
 *
 * @function Spring.SetUnitPieceMatrix
 *
 * If any of the first three elements are non-zero, and also blocks all script animations from modifying it until {0, 0, 0} is passed.
 *
 * @param unitID integer
 * @param pieceNum number
 * @param matrix number[] an array of 16 floats
 * @return nil
 */
int LuaSyncedCtrl::SetUnitPieceMatrix(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	LocalModelPiece* lmp = ParseObjectLocalModelPiece(L, unit, 2);

	if (lmp == nullptr)
		return 0;

	CMatrix44f mat;

	if (LuaUtils::ParseFloatArray(L, 3, &mat.m[0], 16) == -1)
		return 0;

	if (lmp->SetPieceSpaceMatrix(mat))
		lmp->SetDirty();

	lua_pushboolean(L, lmp->blockScriptAnims);
	return 1;
}


/***
 * @function Spring.SetUnitCollisionVolumeData
 * @param unitID integer
 * @param scaleX number
 * @param scaleY number
 * @param scaleZ number
 * @param offsetX number
 * @param offsetY number
 * @param offsetZ number
 * @param vType number
 * @param tType number
 * @param Axis number
 * @return nil
 *
 *  enum COLVOL_TYPES {
 *      COLVOL_TYPE_DISABLED = -1,
 *      COLVOL_TYPE_ELLIPSOID = 0,
 *      COLVOL_TYPE_CYLINDER,
 *      COLVOL_TYPE_BOX,
 *      COLVOL_TYPE_SPHERE,
 *      COLVOL_NUM_TYPES       // number of non-disabled collision volume types
 *    };
 *    enum COLVOL_TESTS {
 *      COLVOL_TEST_DISC = 0,
 *      COLVOL_TEST_CONT = 1,
 *      COLVOL_NUM_TESTS = 2   // number of tests
 *    };
 *    enum COLVOL_AXES {
 *      COLVOL_AXIS_X   = 0,
 *      COLVOL_AXIS_Y   = 1,
 *      COLVOL_AXIS_Z   = 2,
 *      COLVOL_NUM_AXES = 3    // number of collision volume axes
 *    };
 */
int LuaSyncedCtrl::SetUnitCollisionVolumeData(lua_State* L)
{
	return (SetSolidObjectCollisionVolumeData(L, ParseUnit(L, __func__, 1)));
}


/***
 * @function Spring.SetUnitPieceCollisionVolumeData
 * @param unitID integer
 * @param pieceIndex number
 * @param enable boolean
 * @param scaleX number
 * @param scaleY number
 * @param scaleZ number
 * @param offsetX number
 * @param offsetY number
 * @param offsetZ number
 * @param volumeType number?
 * @param primaryAxis number?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitPieceCollisionVolumeData(lua_State* L)
{
	return (SetSolidObjectPieceCollisionVolumeData(L, ParseUnit(L, __func__, 1)));
}


/***
 *
 * @function Spring.SetUnitPieceVisible
 * @param unitID integer
 * @param pieceIndex number
 * @param visible boolean
 * @return nil
 */
int LuaSyncedCtrl::SetUnitPieceVisible(lua_State* L)
{
	return (SetSolidObjectPieceVisible(L, ParseUnit(L, __func__, 1)));
}


/***
 * @function Spring.SetUnitSensorRadius
 * @param unitID integer
 * @param type "los"|"airLos"|"radar"|"sonar"|"seismic"|"radarJammer"|"sonarJammer"
 * @param radius number
 * @return number? New radius, or `nil` if unit is invalid.
 */
int LuaSyncedCtrl::SetUnitSensorRadius(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const int radius = std::clamp(luaL_checkint(L, 3), 0, MAX_UNIT_SENSOR_RADIUS);

	switch (hashString(luaL_checkstring(L, 2))) {
		case hashString("los"): {
			unit->ChangeLos(unit->realLosRadius = radius, unit->realAirLosRadius);
			lua_pushnumber(L, unit->losRadius);
		} break;
		case hashString("airLos"): {
			unit->ChangeLos(unit->realLosRadius, unit->realAirLosRadius = radius);
			lua_pushnumber(L, unit->airLosRadius);
		} break;
		case hashString("radar"): {
			lua_pushnumber(L, unit->radarRadius = radius);
		} break;
		case hashString("sonar"): {
			lua_pushnumber(L, unit->sonarRadius = radius);
		} break;
		case hashString("seismic"): {
			lua_pushnumber(L, unit->seismicRadius = radius);
		} break;
		case hashString("radarJammer"): {
			lua_pushnumber(L, unit->jammerRadius = radius);
		} break;
		case hashString("sonarJammer"): {
			lua_pushnumber(L, unit->sonarJamRadius = radius);
		} break;
		default: {
			luaL_error(L, "Unknown sensor type to SetUnitSensorRadius()");
		} break;
	}

	return 1;
}


/*** Sets a unit's radar wobble
 *
 * Controls how much a unit's radar dot will wobble. Note that setting
 * this above the allyTeam's default wobble may result in the edgemost
 * dot positions failing to register in ray traces, i.e. things like
 * native "is under cursor" checks and some Lua interfaces.
 *
 * @function Spring.SetUnitPosErrorParams
 * @param unitID integer
 * @param posErrorVectorX number
 * @param posErrorVectorY number
 * @param posErrorVectorZ number
 * @param posErrorDeltaX number
 * @param posErrorDeltaY number
 * @param posErrorDeltaZ number
 * @param nextPosErrorUpdate number?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitPosErrorParams(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->posErrorVector.x = luaL_optfloat(L, 2, unit->posErrorVector.x);
	unit->posErrorVector.y = luaL_optfloat(L, 3, unit->posErrorVector.y);
	unit->posErrorVector.z = luaL_optfloat(L, 4, unit->posErrorVector.z);
	unit->posErrorDelta.x = luaL_optfloat(L, 5, unit->posErrorDelta.x);
	unit->posErrorDelta.y = luaL_optfloat(L, 6, unit->posErrorDelta.y);
	unit->posErrorDelta.z = luaL_optfloat(L, 7, unit->posErrorDelta.z);

	unit->nextPosErrorUpdate = luaL_optint(L, 8, unit->nextPosErrorUpdate);

	if (lua_isnumber(L, 9) && lua_isboolean(L, 10))
		unit->SetPosErrorBit(std::clamp(lua_tointeger(L, 9), 0, teamHandler.ActiveAllyTeams()), lua_toboolean(L, 10));

	return 0;
}


/*** Used by default commands to get in build-, attackrange etc.
 *
 * @function Spring.SetUnitMoveGoal
 * @param unitID integer
 * @param goalX number
 * @param goalY number
 * @param goalZ number
 * @param goalRadius number?
 * @param moveSpeed number?
 * @param moveRaw boolean?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitMoveGoal(lua_State* L)
{
	CheckAllowGameChanges(L);
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	assert(unit->moveType != nullptr);

	const float3 pos(luaL_checkfloat(L, 2), luaL_checkfloat(L, 3), luaL_checkfloat(L, 4));

	const float radius = luaL_optfloat(L, 5, 0.0f);
	const float speed  = luaL_optfloat(L, 6, unit->moveType->GetMaxSpeed());

	if (luaL_optboolean(L, 7, false)) {
		unit->moveType->StartMovingRaw(pos, radius);
	} else {
		unit->moveType->StartMoving(pos, radius, speed);
	}

	return 0;
}


/*** Used in conjunction with Spring.UnitAttach et al. to re-implement old airbase & fuel system in Lua.
 *
 * @function Spring.SetUnitLandGoal
 * @param unitID integer
 * @param goalX number
 * @param goalY number
 * @param goalZ number
 * @param goalRadius number?
 * @return nil
 */
int LuaSyncedCtrl::SetUnitLandGoal(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	AAirMoveType* amt = dynamic_cast<AAirMoveType*>(unit->moveType);

	if (amt == nullptr)
		luaL_error(L, "Not a flying unit (id = %d, dead = %d, name = %s)", unit->id, static_cast<int>(unit->isDead), unit->unitDef ? unit->unitDef->name.c_str() : "<null>");

	const float3 landPos(luaL_checkfloat(L, 2), luaL_checkfloat(L, 3), luaL_checkfloat(L, 4));
	const float radiusSq = lua_isnumber(L, 5)? Square(lua_tonumber(L, 5)): -1.0f;

	amt->LandAt(landPos, radiusSq);
	return 0;
}


/***
 * @function Spring.ClearUnitGoal
 * @param unitID integer
 * @return nil
 */
int LuaSyncedCtrl::ClearUnitGoal(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->moveType->StopMoving(false, false, luaL_optboolean(L, 2, true));
	return 0;
}


/***
 * @function Spring.SetUnitPhysics
 * @param unitID integer
 * @param posX number
 * @param posY number
 * @param posZ number
 * @param velX number
 * @param velY number
 * @param velZ number
 * @param rotX number
 * @param rotY number
 * @param rotZ number
 * @param dragX number
 * @param dragY number
 * @param dragZ number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitPhysics(lua_State* L)
{
	return (SetSolidObjectPhysicalState(L, ParseUnit(L, __func__, 1)));
}

/***
 * @function Spring.SetUnitMass
 * @param unitID integer
 * @param mass number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitMass(lua_State* L)
{
	return (SetSolidObjectMass(L, ParseUnit(L, __func__, 1)));
}


/*** Set unit position (2D)
 * @function Spring.SetUnitPosition
 *
 * Sets a unit's position in 2D, at terrain height.
 *
 * @param unitID integer
 * @param x number
 * @param z number
 * @param floating boolean? (Default: `false`) If true, over water the position is on surface. If false, on seafloor.
 * @return nil
 */


/*** Set unit position (3D)
 * @function Spring.SetUnitPosition
 *
 * Sets a unit's position in 3D, at an arbitrary height.
 *
 * @param unitID integer
 * @param x number
 * @param y number
 * @param z number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitPosition(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	float3 pos;

	if (lua_isnumber(L, 4)) {
		// 2=x, 3=y, 4=z
		pos.x = luaL_checkfloat(L, 2);
		pos.y = luaL_checkfloat(L, 3);
		pos.z = luaL_checkfloat(L, 4);
	} else {
		// 2=x, 3=z, 4=bool
		pos.x = luaL_checkfloat(L, 2);
		pos.z = luaL_checkfloat(L, 3);

		if (luaL_optboolean(L, 4, false)) {
			pos.y = CGround::GetHeightAboveWater(pos.x, pos.z);
		} else {
			pos.y = CGround::GetHeightReal(pos.x, pos.z);
		}
	}

	unit->ForcedMove(pos);
	return 0;
}


/***
 * @function Spring.SetUnitRotation
 * Note: PYR order
 * @param unitID integer
 * @param pitch number Rotation in X axis
 * @param yaw number Rotation in Y axis
 * @param roll number Rotation in Z axis
 * @return nil
 */
int LuaSyncedCtrl::SetUnitRotation(lua_State* L)
{
	return (SetSolidObjectRotation(L, ParseUnit(L, __func__, 1)));
}


/***
 * @function Spring.SetUnitDirection
 * Set unit front direction vector. The vector is normalized in
 * the engine.
 *
 * @deprecated It's strongly that you use the overload that accepts
 * a right direction as `frontDir` alone doesn't define object orientation.
 *
 * @param unitID integer
 * @param frontx number
 * @param fronty number
 * @param frontz number
 * @return nil
 */

 /***
  * @function Spring.SetUnitDirection
  * Set unit front and right direction vectors.
  *
  * Both vectors will be normalized in the engine.
  *
  * @param unitID integer
  * @param frontx number
  * @param fronty number
  * @param frontz number
  * @param rightx number
  * @param righty number
  * @param rightz number
  * @return nil
  */
int LuaSyncedCtrl::SetUnitDirection(lua_State* L)
{
	return SetSolidObjectDirection(L, ParseUnit(L, __func__, 1), __func__);
}

/***
 * Integer in range `[-32768, 32767]` that represents a 2D (xz plane) unit
 * orientation. 
 * 
 * ```
 *                   F(N=2) = H(-32768 / 32767)
 * 
 *                          ^
 *                          |
 *                          |
 *  F(W=3) = H(-16384)  <---o--->  F(E=1) = H(16384)
 *                          |
 *                          |
 *                          v
 * 
 *                   F(S=0) = H(0)
 * ```
 * @alias Heading integer
 */

/***
 * @function Spring.SetUnitHeadingAndUpDir
 * Use this call to set up unit direction in a robust way. If unit was
 * completely upright, new `{upx, upy, upz}` direction will be used as new "up"
 * vector, the rotation set by "heading" will remain preserved.
 * 
 * @param unitID integer
 * @param heading Heading
 * @param upx number
 * @param upy number
 * @param upz number
 * @return nil
 */
int LuaSyncedCtrl::SetUnitHeadingAndUpDir(lua_State* L)
{
	return SetSolidObjectHeadingAndUpDir(L, ParseUnit(L, __func__, 1));
}

/*** Set the velocity of a Unit
 *
 * @see Spring.SetUnitMoveCtrl for disabling/enabling this control
 * @function Spring.SetUnitVelocity
 * @param unitID integer
 * @param velX number in elmos/frame
 * @param velY number in elmos/frame
 * @param velZ number in elmos/frame
 */
int LuaSyncedCtrl::SetUnitVelocity(lua_State* L)
{
	return (SetWorldObjectVelocity(L, ParseUnit(L, __func__, 1)));
}


/***
 *
 * @function Spring.SetFactoryBuggerOff
 * @param unitID integer
 * @param buggerOff boolean?
 * @param offset number?
 * @param radius number?
 * @param relHeading Heading?
 * @param spherical boolean?
 * @param forced boolean?
 * @return nil|number buggerOff
 */
int LuaSyncedCtrl::SetFactoryBuggerOff(lua_State* L)
{
	CUnit* u = ParseUnit(L, __func__, 1);
	if (u == nullptr)
		return 0;

	CFactory* f = dynamic_cast<CFactory*>(u);
	if (f == nullptr)
		return 0;

	f->boPerform     = luaL_optboolean(L, 2, f->boPerform    );
	f->boOffset      = luaL_optfloat(  L, 3, f->boOffset     );
	f->boRadius      = luaL_optfloat(  L, 4, f->boRadius     );
	f->boRelHeading  = luaL_optint(    L, 5, f->boRelHeading );
	f->boSherical    = luaL_optboolean(L, 6, f->boSherical   );
	f->boForced      = luaL_optboolean(L, 7, f->boForced     );

	lua_pushboolean(L, f->boPerform);
	return 1;
}


/***
 *
 * @function Spring.BuggerOff
 * @param x number
 * @param y number
 * @param z number? uses ground height when unspecified
 * @param radius number
 * @param teamID integer
 * @param spherical boolean? (Default: `true`)
 * @param forced boolean? (Default: `true`)
 * @param excludeUnitID integer?
 * @param excludeUnitDefIDs number[]?
 * @return nil
 */
int LuaSyncedCtrl::BuggerOff(lua_State* L)
{
	float3 pos;
	pos.x = luaL_checkfloat(L, 1);
	pos.z = luaL_checkfloat(L, 3);
	pos.y = !lua_isnil(L, 2) ? luaL_checkfloat(L, 2) : CGround::GetHeightReal(pos.x, pos.z);

	const float radius = luaL_checkfloat(L, 4);
	const int teamID   = lua_toint(L      , 5);
	if (!teamHandler.IsValidTeam(teamID))
		luaL_error(L, "%s(): Bad teamID: %d", __func__, teamID);

	const bool spherical  = luaL_optboolean(L, 6, true);
	const bool forced     = luaL_optboolean(L, 7, true);
	const CUnit* excudie  = ParseRawUnit(L, __func__, 8); //can be nullptr

	if (lua_istable(L, 9)) {
		std::vector<const UnitDef*> exclUDefs;
		ParseUnitDefArray(L, __func__, 9, exclUDefs);
		CGameHelper::BuggerOff(pos, radius, spherical, forced, teamID, excudie, exclUDefs);
	}
	else {
		CGameHelper::BuggerOff(pos, radius, spherical, forced, teamID, excudie);
	}

	return 0;
}


static std::optional<std::tuple<float, int, CUnit*, int, float3> > ParseDamageParams(lua_State* L)
{
	const float damage    = luaL_checkfloat(L, 2);
	const int paralyze    = luaL_optint(L, 3, 0);
	const int attackerID  = luaL_optint(L, 4, -1);
	const int weaponDefID = luaL_optint(L, 5, -1);
	const float3 impulse  = float3(std::clamp(luaL_optfloat(L, 6, 0.0f), -MAX_EXPLOSION_IMPULSE, MAX_EXPLOSION_IMPULSE),
	                               std::clamp(luaL_optfloat(L, 7, 0.0f), -MAX_EXPLOSION_IMPULSE, MAX_EXPLOSION_IMPULSE),
	                               std::clamp(luaL_optfloat(L, 8, 0.0f), -MAX_EXPLOSION_IMPULSE, MAX_EXPLOSION_IMPULSE));

	CUnit* attacker = nullptr;

	if (attackerID >= 0) {
		if (static_cast<size_t>(attackerID) >= unitHandler.MaxUnits())
			return std::nullopt;

		attacker = unitHandler.GetUnit(attackerID);
	}

	// negated values from 'CSolidObject::DamageType' also allowed
	if (weaponDefID >= int(weaponDefHandler->NumWeaponDefs()))
		return std::nullopt;
	return std::make_tuple(damage, paralyze, attacker, weaponDefID, impulse);
}


/*** Apply damage to feature
 *
 * @function Spring.AddFeatureDamage
 *
 * Will trigger FeaturePreDamaged and FeatureDamaged callins.
 *
 * Won't do anything if paralyze is not zero, the feature is already marked for deletion, or in void.
 *
 * If health goes below 0 and featureDef is `destructable` the feature will be deleted and
 * a wreck created.
 *
 * @param featureID integer
 * @param damage number
 * @param paralyze number? (Default: `0`) equals to the paralyzetime in the WeaponDef.
 * @param attackerID integer? (Default: `-1`)
 * @param weaponID integer? (Default: `-1`)
 * @param impulseX number?
 * @param impulseY number?
 * @param impulseZ number?
 *
 * @see SyncedCallins:FeaturePreDamaged
 * @see SyncedCallins:FeatureDamaged
 */
int LuaSyncedCtrl::AddFeatureDamage(lua_State* L)
{
	CheckAllowGameChanges(L);

	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	const auto damageParams = ParseDamageParams(L);
	if (!damageParams)
		return 0;

	const auto [damage, paralyze, attacker, weaponDefID, impulse] = *damageParams;
	DamageArray damages(damage);

	if (paralyze)
		damages.paralyzeDamageTime = paralyze;

	feature->DoDamage(damages, impulse, attacker, weaponDefID, -1);
	return 0;
}


/***
 * @function Spring.AddUnitDamage
 *
 * @param unitID integer
 * @param damage number
 * @param paralyze number? (Default: `0`) equals to the paralyzetime in the WeaponDef.
 * @param attackerID integer? (Default: `-1`)
 * @param weaponID integer? (Default: `-1`)
 * @param impulseX number?
 * @param impulseY number?
 * @param impulseZ number?
 * @return nil
 */
int LuaSyncedCtrl::AddUnitDamage(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const auto damageParams = ParseDamageParams(L);
	if (!damageParams)
		return 0;

	const auto [damage, paralyze, attacker, weaponDefID, impulse] = *damageParams;

	DamageArray damages;
	damages.Set(unit->armorType, damage);

	if (paralyze)
		damages.paralyzeDamageTime = paralyze;

	unit->DoDamage(damages, impulse, attacker, weaponDefID, -1);
	return 0;
}


/***
 * @function Spring.AddUnitImpulse
 * @param unitID integer
 * @param x number
 * @param y number
 * @param z number
 * @param decayRate number?
 * @return nil
 */
int LuaSyncedCtrl::AddUnitImpulse(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const float3 impulse(std::clamp(luaL_checkfloat(L, 2), -MAX_EXPLOSION_IMPULSE, MAX_EXPLOSION_IMPULSE),
	                     std::clamp(luaL_checkfloat(L, 3), -MAX_EXPLOSION_IMPULSE, MAX_EXPLOSION_IMPULSE),
	                     std::clamp(luaL_checkfloat(L, 4), -MAX_EXPLOSION_IMPULSE, MAX_EXPLOSION_IMPULSE));

	unit->ApplyImpulse(impulse);
	return 0;
}


/***
 * @function Spring.AddUnitSeismicPing
 * @param unitID integer
 * @param pindSize number
 * @return nil
 */
int LuaSyncedCtrl::AddUnitSeismicPing(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	unit->DoSeismicPing(luaL_checkfloat(L, 2));
	return 0;
}


/******************************************************************************/

/***
 * @function Spring.AddUnitResource
 * @param unitID integer
 * @param resource string "m" | "e"
 * @param amount number
 * @return nil
 */
int LuaSyncedCtrl::AddUnitResource(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const string& type = luaL_checkstring(L, 2);

	if (type.empty())
		return 0;

	switch (type[0]) {
		case 'm': { unit->AddMetal (std::max(0.0f, luaL_checkfloat(L, 3))); } break;
		case 'e': { unit->AddEnergy(std::max(0.0f, luaL_checkfloat(L, 3))); } break;
		default: {} break;
	}

	return 0;
}

/*

/***
 * @function Spring.UseUnitResource
 * @param unitID integer
 * @param resource ResourceName
 * @param amount number
 * @return boolean? okay
 */

/***
 * @function Spring.UseUnitResource
 * @param unitID integer
 * @param resources ResourceUsage
 * @return boolean? okay
 */
int LuaSyncedCtrl::UseUnitResource(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	if (lua_isstring(L, 2)) {
		const char* type = lua_tostring(L, 2);

		switch (type[0]) {
			case 'm': { lua_pushboolean(L, unit->UseMetal (std::max(0.0f, lua_tofloat(L, 3)))); return 1; } break;
			case 'e': { lua_pushboolean(L, unit->UseEnergy(std::max(0.0f, lua_tofloat(L, 3)))); return 1; } break;
			default : {                                                                                   } break;
		}

		return 0;
	}

	if (lua_istable(L, 2)) {
		float metal  = 0.0f;
		float energy = 0.0f;

		constexpr int tableIdx = 2;

		for (lua_pushnil(L); lua_next(L, tableIdx) != 0; lua_pop(L, 1)) {
			if (lua_israwstring(L, LUA_TABLE_KEY_INDEX) && lua_isnumber(L, LUA_TABLE_VALUE_INDEX)) {
				const char* key = lua_tostring(L, LUA_TABLE_KEY_INDEX);
				const float val = std::max(0.0f, lua_tofloat(L, -1));

				switch (key[0]) {
					case 'm': {  metal = val; } break;
					case 'e': { energy = val; } break;
					default : {               } break;
				}
			}
		}

		CTeam* team = teamHandler.Team(unit->team);

		if ((team->res.metal >= metal) && (team->res.energy >= energy)) {
			unit->UseMetal(metal);
			unit->UseEnergy(energy);
			lua_pushboolean(L, true);
		} else {
			team->resPull.metal  += metal;
			team->resPull.energy += energy;
			lua_pushboolean(L, false);
		}

		return 1;
	}

	luaL_error(L, "Incorrect arguments to UseUnitResource()");
	return 0;
}


/******************************************************************************
 * Decals
 * @section decals
******************************************************************************/


/***
 *
 * @function Spring.AddObjectDecal
 * @param unitID integer
 * @return nil
 */
int LuaSyncedCtrl::AddObjectDecal(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	groundDecals->AddSolidObject(unit);
	return 0;
}


/***
 * @function Spring.RemoveObjectDecal
 * @param unitID integer
 * @return nil
 */
int LuaSyncedCtrl::RemoveObjectDecal(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	groundDecals->ForceRemoveSolidObject(unit);
	return 0;
}


/******************************************************************************
 * Grass
 * @section grass
******************************************************************************/


/***
 * @function Spring.AddGrass
 * @param x number
 * @param z number
 * @return nil
 */
int LuaSyncedCtrl::AddGrass(lua_State* L)
{
	const float3 pos(luaL_checkfloat(L, 1), 0.0f, luaL_checkfloat(L, 2));
	const uint8_t grassValue = static_cast<uint8_t>(luaL_optint(L, 3, 1));

	grassDrawer->AddGrass(pos.cClampInBounds(), grassValue);
	return 0;
}

/***
 * @function Spring.RemoveGrass
 * @param x number
 * @param z number
 * @return nil
 */
int LuaSyncedCtrl::RemoveGrass(lua_State* L)
{
	const float3 pos(luaL_checkfloat(L, 1), 0.0f, luaL_checkfloat(L, 2));

	grassDrawer->RemoveGrass(pos.cClampInBounds());
	return 0;
}


/******************************************************************************
 * Feature Handling
 * @section featurehandling
******************************************************************************/


/***
 * @function Spring.CreateFeature
 * @param featureDef string|integer name or id
 * @param x number
 * @param y number
 * @param z number
 * @param heading Heading?
 * @param AllyTeamID integer?
 * @param featureID integer?
 * @return integer featureID
 */
int LuaSyncedCtrl::CreateFeature(lua_State* L)
{
	CheckAllowGameChanges(L);

	const FeatureDef* featureDef = nullptr;

	if (lua_israwstring(L, 1)) {
		featureDef = featureDefHandler->GetFeatureDef(lua_tostring(L, 1));
	} else if (lua_israwnumber(L, 1)) {
		featureDef = featureDefHandler->GetFeatureDefByID(lua_toint(L, 1));
	}

	if (featureDef == nullptr)
		return 0; // do not error (featureDefs are dynamic)

	const float3 pos(luaL_checkfloat(L, 2),
	                 luaL_checkfloat(L, 3),
	                 luaL_checkfloat(L, 4));

	short int heading = 0;

	if (lua_isnumber(L, 5))
		heading = lua_toint(L, 5);

	int facing = GetFacingFromHeading(heading);
	int team = CtrlTeam(L);

	if (team < 0)
		team = -1; // default to global for AllAccessTeam

	// FIXME -- separate teamcolor/allyteam arguments

	if (lua_isnumber(L, 6)) {
		if ((team = lua_toint(L, 6)) < -1) {
			team = -1;
		} else if (team >= teamHandler.ActiveTeams()) {
			return 0;
		}
	}

	const int allyTeam = (team < 0) ? -1 : teamHandler.AllyTeam(team);

	if (!CanControlFeatureAllyTeam(L, allyTeam))
		luaL_error(L, "CreateFeature() bad team permission %d", team);

	if (inCreateFeature >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "CreateFeature() recursion is not permitted, max depth: %d", MAX_CMD_RECURSION_DEPTH);

	// use SetFeatureResurrect() to fill in the missing bits
	inCreateFeature++;

	FeatureLoadParams  params;
	params.parentObj   = nullptr;
	params.featureDef  = featureDef;
	params.unitDef     = nullptr;
	params.pos         = pos;
	params.speed       = ZeroVector;
	params.featureID   = luaL_optint(L, 7, -1);
	params.teamID      = team;
	params.allyTeamID  = allyTeam;
	params.heading     = heading,
	params.facing      = facing,
	params.wreckLevels = 0;
	params.smokeTime   = 0;

	CFeature* feature = featureHandler.LoadFeature(params);
	inCreateFeature--;

	if (feature != nullptr) {
		lua_pushnumber(L, feature->id);
		return 1;
	}

	return 0;
}


// Internal helper method
void LuaSyncedCtrl::DestroyFeatureCommon(lua_State* L, CFeature* feature)
{
	if (inDestroyFeature >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "DestroyFeature() recursion is not permitted, max depth: %d", MAX_CMD_RECURSION_DEPTH);

	inDestroyFeature++;
	featureHandler.DeleteFeature(feature);
	inDestroyFeature--;
}


/***
 * @function Spring.DestroyFeature
 * @param featureID integer
 * @return nil
 */
int LuaSyncedCtrl::DestroyFeature(lua_State* L)
{
	CheckAllowGameChanges(L);
	CFeature* feature = ParseFeature(L, __func__, 1);
	if (feature == nullptr)
		return 0;

	DestroyFeatureCommon(L, feature);

	return 0;
}


/*** Feature Control
 *
 * @function Spring.TransferFeature
 * @param featureID integer
 * @param teamID integer
 * @return nil
 */
int LuaSyncedCtrl::TransferFeature(lua_State* L)
{
	CheckAllowGameChanges(L);
	CFeature* feature = ParseFeature(L, __func__, 1);
	if (feature == nullptr)
		return 0;

	const int teamId = luaL_checkint(L, 2);
	if (!teamHandler.IsValidTeam(teamId))
		return 0;

	feature->ChangeTeam(teamId);
	return 0;
}


/***
 * @function Spring.SetFeatureAlwaysVisible
 * @param featureID integer
 * @param enable boolean
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureAlwaysVisible(lua_State* L)
{
	return (SetWorldObjectAlwaysVisible(L, ParseFeature(L, __func__, 1), __func__));
}

/***
 *
 * @function Spring.SetFeatureUseAirLos
 * @param featureID integer
 * @param useAirLos boolean
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureUseAirLos(lua_State* L)
{
	return (SetWorldObjectUseAirLos(L, ParseFeature(L, __func__, 1), __func__));
}


/***
 * @function Spring.SetFeatureHealth
 * @param featureID integer
 * @param health number
 * @param checkDestruction boolean? (Default: `false`) Whether to destroy feature if feature goes below 0 health.
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureHealth(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	feature->health = std::min(feature->maxHealth, luaL_checkfloat(L, 2));

	if (feature->health <= 0.0f && luaL_optboolean(L, 3, false)) {
		DestroyFeatureCommon(L, feature);
	}
	return 0;
}


/***
 *
 * @function Spring.SetFeatureMaxHealth
 * @param featureID integer
 * @param maxHealth number minimum 0.1
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureMaxHealth(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	feature->maxHealth = std::max(0.1f, luaL_checkfloat(L, 2));
	feature->health = std::min(feature->health, feature->maxHealth);
	return 0;
}


/***
 * @function Spring.SetFeatureReclaim
 * @param featureID integer
 * @param reclaimLeft number
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureReclaim(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	feature->reclaimLeft = luaL_checkfloat(L, 2);
	return 0;
}

/***
 * @function Spring.SetFeatureResources
 * @param featureID integer
 * @param metal number
 * @param energy number
 * @param reclaimTime number?
 * @param reclaimLeft number?
 * @param featureDefMetal number?
 * @param featureDefEnergy number?
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureResources(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	feature->defResources.metal  = std::max(0.0f, luaL_optfloat(L, 6, feature->defResources.metal));
	feature->defResources.energy = std::max(0.0f, luaL_optfloat(L, 7, feature->defResources.energy));

	feature->resources.metal  = std::clamp(luaL_checknumber(L, 2), 0.0f, feature->defResources.metal );
	feature->resources.energy = std::clamp(luaL_checknumber(L, 3), 0.0f, feature->defResources.energy);

	feature->reclaimTime = std::clamp(luaL_optnumber(L, 4, feature->reclaimTime), 1.0f, 1000000.0f);
	feature->reclaimLeft = std::clamp(luaL_optnumber(L, 5, feature->reclaimLeft), 0.0f,       1.0f);
	return 0;
}

/***
 * @function Spring.SetFeatureResurrect
 *
 * @param featureID integer
 * @param unitDef string|integer Can be a number id or a string name, this allows cancelling resurrection by passing `-1`.
 * @param facing Facing? (Default: `"south"`)
 * @param progress number? Set the level of progress.
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureResurrect(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	if (!lua_isnoneornil(L, 2)) {
		const UnitDef* ud = nullptr;

		// which type of unit this feature should turn into if resurrected, by id or name
		if (lua_israwnumber(L, 2))
			ud = unitDefHandler->GetUnitDefByID(lua_toint(L, 2));
		else if (lua_israwstring(L, 2))
			ud = unitDefHandler->GetUnitDefByName(lua_tostring(L, 2));

		// nullptr is also accepted, allows unsetting the target via id=-1
		feature->udef = ud;
	}

	if (!lua_isnoneornil(L, 3))
		feature->buildFacing = LuaUtils::ParseFacing(L, __func__, 3);

	feature->resurrectProgress = std::clamp(luaL_optnumber(L, 4, feature->resurrectProgress), 0.0f, 1.0f);
	return 0;
}

/***
 * Enable feature movement control.
 * 
 * @function Spring.SetFeatureMoveCtrl
 * @param featureID integer
 * @param enabled true Enable feature movement.
 * @param initialVelocityX number? Initial velocity on X axis, or `nil` for no change.
 * @param initialVelocityY number? Initial velocity on Y axis, or `nil` for no change.
 * @param initialVelocityZ number? Initial velocity on Z axis, or `nil` for no change.
 * @param accelerationX number? Acceleration per frame on X axis, or `nil` for no change.
 * @param accelerationY number? Acceleration per frame on Y axis, or `nil` for no change.
 * @param accelerationZ number? Acceleration per frame on Z axis, or `nil` for no change.
 */

/***
 * Disable feature movement control.
 * 
 * Optional parameter allow physics vectors to build when not using `MoveCtrl`.
 * 
 * It is necessary to unlock feature movement on x, z axis before changing
 * feature physics.
 *
 * For example:
 * 
 * ```lua
 * -- Unlock all movement before setting velocity.
 * Spring.SetFeatureMoveCtrl(featureID,false,1,1,1,1,1,1,1,1,1)
 * 
 * -- Set velocity.
 * Spring.SetFeatureVelocity(featureID,10,0,10)
 * ```
 * 
 * @function Spring.SetFeatureMoveCtrl
 * @param featureID integer
 * @param enabled false Disable feature movement.
 * @param velocityMaskX number? Lock velocity change in X dimension when not using `MoveCtrl`. `0` to lock, non-zero to allow, or `nil` to for no change.
 * @param velocityMaskY number? Lock velocity change in Y dimension when not using `MoveCtrl`. `0` to lock, non-zero to allow, or `nil` to for no change.
 * @param velocityMaskZ number? Lock velocity change in Z dimension when not using `MoveCtrl`. `0` to lock, non-zero to allow, or `nil` to for no change.
 * @param impulseMaskX number? Lock impulse in X dimension when not using `MoveCtrl`. `0` to lock, non-zero to allow, or `nil` to for no change.
 * @param impulseMaskY number? Lock impulse in Y dimension when not using `MoveCtrl`. `0` to lock, non-zero to allow, or `nil` to for no change.
 * @param impulseMaskZ number? Lock impulse in Z dimension when not using `MoveCtrl`. `0` to lock, non-zero to allow, or `nil` to for no change.
 * @param movementMaskX number? Lock move in X dimension when not using `MoveCtrl`. `0` to lock the axis, non-zero to allow, or `nil` for no change.
 * @param movementMaskY number? Lock move in Y dimension when not using `MoveCtrl`. `0` to lock the axis, non-zero to allow, or `nil` for no change.
 * @param movementMaskZ number? Lock move in Z dimension when not using `MoveCtrl`. `0` to lock the axis, non-zero to allow, or `nil` for no change.
 */
int LuaSyncedCtrl::SetFeatureMoveCtrl(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	CFeature::MoveCtrl& moveCtrl = feature->moveCtrl;

	if ((moveCtrl.enabled = luaL_optboolean(L, 2, moveCtrl.enabled))) {
		featureHandler.SetFeatureUpdateable(feature);

		// set vectors
		for (int i = 0; i < 3; i++) {
			moveCtrl.velVector[i] = luaL_optfloat(L, 3 + i, moveCtrl.velVector[i]);
			moveCtrl.accVector[i] = luaL_optfloat(L, 6 + i, moveCtrl.accVector[i]);
		}
	} else {
		// set masks
		for (int i = 0; i < 3; i++) {
			moveCtrl.velocityMask[i] = (luaL_optfloat(L, 3 + i, moveCtrl.velocityMask[i]) != 0.0f);
			moveCtrl. impulseMask[i] = (luaL_optfloat(L, 6 + i, moveCtrl. impulseMask[i]) != 0.0f);
			moveCtrl.movementMask[i] = (luaL_optfloat(L, 9 + i, moveCtrl.movementMask[i]) != 0.0f);
		}
	}

	return 0;
}


/***
 * @function Spring.SetFeaturePhysics
 * @param featureID integer
 * @param posX number
 * @param posY number
 * @param posZ number
 * @param velX number
 * @param velY number
 * @param velZ number
 * @param rotX number
 * @param rotY number
 * @param rotZ number
 * @param dragX number
 * @param dragY number
 * @param dragZ number
 * @return nil
 */
int LuaSyncedCtrl::SetFeaturePhysics(lua_State* L)
{
	return (SetSolidObjectPhysicalState(L, ParseFeature(L, __func__, 1)));
}


/***
 * @function Spring.SetFeatureMass
 * @param featureID integer
 * @param mass number
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureMass(lua_State* L)
{
	return (SetSolidObjectMass(L, ParseFeature(L, __func__, 1)));
}


/***
 * @function Spring.SetFeaturePosition
 * @param featureID integer
 * @param x number
 * @param y number
 * @param z number
 * @param snapToGround boolean?
 * @return nil
 */
int LuaSyncedCtrl::SetFeaturePosition(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	float3 pos;
	pos.x = luaL_checkfloat(L, 2);
	pos.y = luaL_checkfloat(L, 3);
	pos.z = luaL_checkfloat(L, 4);

	feature->ForcedMove(pos);
	return 0;
}


/***
 * @function Spring.SetFeatureRotation
 * Note: PYR order
 * @param featureID integer
 * @param pitch number Rotation in X axis
 * @param yaw number Rotation in Y axis
 * @param roll number Rotation in Z axis
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureRotation(lua_State* L)
{
	return (SetSolidObjectRotation(L, ParseFeature(L, __func__, 1)));
}


/***
 * @function Spring.SetFeatureDirection
 * Set feature front direction vector. The vector is normalized in
 * the engine.
 *
 * @deprecated It's strongly that you use the overload that accepts
 * a right direction as `frontDir` alone doesn't define object orientation.
 *
 * @param featureID integer
 * @param frontx number
 * @param fronty number
 * @param frontz number
 * @return nil
 */

 /***
  * @function Spring.SetFeatureDirection
  * Set feature front and right direction vectors.
  *
  * Both vectors will be normalized in the engine.
  *
  * @param featureID integer
  * @param frontx number
  * @param fronty number
  * @param frontz number
  * @param rightx number
  * @param righty number
  * @param rightz number
  * @return nil
  */
int LuaSyncedCtrl::SetFeatureDirection(lua_State* L)
{
	return SetSolidObjectDirection(L, ParseFeature(L, __func__, 1), __func__);
}

/***
 * @function Spring.SetFeatureHeadingAndUpDir
 * Use this call to set up feature direction in a robust way. If feature was
 * completely upright, new `{upx, upy, upz}` direction will be used as new "up"
 * vector, the rotation set by "heading" will remain preserved.
 * 
 * @param featureID integer
 * @param heading Heading
 * @param upx number
 * @param upy number
 * @param upz number
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureHeadingAndUpDir(lua_State* L)
{
	return SetSolidObjectHeadingAndUpDir(L, ParseFeature(L, __func__, 1));
}

/*** Set the velocity of a Feature
 *
 * @see Spring.SetFeatureMoveCtrl for disabling/enabling this control
 * @function Spring.SetFeatureVelocity
 * @param featureID integer
 * @param velX number in elmos/frame
 * @param velY number in elmos/frame
 * @param velZ number in elmos/frame
 */
int LuaSyncedCtrl::SetFeatureVelocity(lua_State* L)
{
	return (SetWorldObjectVelocity(L, ParseFeature(L, __func__, 1)));
}


/***
 * @function Spring.SetFeatureBlocking
 * @param featureID integer
 * @param isBlocking boolean? If `true` add this feature to the `GroundBlockingMap`, but only if it collides with solid objects (or is being set to collide with the `isSolidObjectCollidable` argument). If `false`, remove this feature from the `GroundBlockingMap`. No change if `nil`.
 * @param isSolidObjectCollidable boolean? Enable or disable collision with solid objects, or no change if `nil`.
 * @param isProjectileCollidable boolean? Enable or disable collision with projectiles, or no change if `nil`.
 * @param isRaySegmentCollidable boolean? Enable or disable collision with ray segments, or no change if `nil`.
 * @param crushable boolean? Enable or disable crushable, or no change if `nil`.
 * @param blockEnemyPushing boolean? Enable or disable blocking enemy pushing, or no change if `nil`.
 * @param blockHeightChanges boolean? Enable or disable blocking height changes, or no change if `nil`.
 * @return boolean isBlocking
 */
int LuaSyncedCtrl::SetFeatureBlocking(lua_State* L)
{
	return (SetSolidObjectBlocking(L, ParseFeature(L, __func__, 1)));
}


/***
 * @function Spring.SetFeatureNoSelect
 * @param featureID integer
 * @param noSelect boolean
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureNoSelect(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	feature->noSelect = !!luaL_checkboolean(L, 2);
	return 0;
}


/***
 * @function Spring.SetFeatureMidAndAimPos
 *
 * Check `Spring.SetUnitMidAndAimPos` for further explanation of the arguments.
 *
 * @param featureID integer
 * @param mpX number
 * @param mpY number
 * @param mpZ number
 * @param apX number
 * @param apY number
 * @param apZ number
 * @param relative boolean?
 * @return boolean success
 */
int LuaSyncedCtrl::SetFeatureMidAndAimPos(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr) {
		lua_pushboolean(L, false);
		return 1;
	}

	#define FLOAT(i) luaL_checkfloat(L, i)
	#define FLOAT3(i, j, k) float3(FLOAT(i), FLOAT(j), FLOAT(k))

	const int argc = lua_gettop(L);
	const float3 newMidPos = (argc >= 4)? FLOAT3(2, 3, 4): float3(feature->midPos);
	const float3 newAimPos = (argc >= 7)? FLOAT3(5, 6, 7): float3(feature->aimPos);
	const bool setRelative = luaL_optboolean(L, 8, false);
	const bool updateQuads = (newMidPos != feature->midPos);

	#undef FLOAT3
	#undef FLOAT

	if (updateQuads) {
		quadField.RemoveFeature(feature);
	}

	feature->SetMidAndAimPos(newMidPos, newAimPos, setRelative);

	if (updateQuads) {
		quadField.AddFeature(feature);
	}

	lua_pushboolean(L, true);
	return 1;
}


/***
 * @function Spring.SetFeatureRadiusAndHeight
 * @param featureID integer
 * @param radius number
 * @param height number
 * @return boolean success
 */
int LuaSyncedCtrl::SetFeatureRadiusAndHeight(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr) {
		lua_pushboolean(L, false);
		return 1;
	}

	const float newRadius = std::max(1.0f, luaL_optfloat(L, 2, feature->radius));
	const float newHeight = std::max(1.0f, luaL_optfloat(L, 3, feature->height));
	const bool updateQuads = (newRadius != feature->radius);

	if (updateQuads) {
		quadField.RemoveFeature(feature);
	}

	feature->SetRadiusAndHeight(newRadius, newHeight);

	if (updateQuads) {
		quadField.AddFeature(feature);
	}

	lua_pushboolean(L, true);
	return 1;
}


/***
 * @function Spring.SetFeatureCollisionVolumeData
 *
 * Check `Spring.SetUnitCollisionVolumeData` for further explanation of the arguments.
 *
 * @param featureID integer
 * @param scaleX number
 * @param scaleY number
 * @param scaleZ number
 * @param offsetX number
 * @param offsetY number
 * @param offsetZ number
 * @param vType number
 * @param tType number
 * @param Axis number
 * @return nil
 */
int LuaSyncedCtrl::SetFeatureCollisionVolumeData(lua_State* L)
{
	return (SetSolidObjectCollisionVolumeData(L, ParseFeature(L, __func__, 1)));
}


/***
 * @function Spring.SetFeaturePieceCollisionVolumeData
 * @param featureID integer
 * @param pieceIndex number
 * @param enable boolean
 * @param scaleX number
 * @param scaleY number
 * @param scaleZ number
 * @param offsetX number
 * @param offsetY number
 * @param offsetZ number
 * @param Axis number
 * @param volumeType number
 * @param primaryAxis number?
 * @return nil
 */
int LuaSyncedCtrl::SetFeaturePieceCollisionVolumeData(lua_State* L)
{
	return (SetSolidObjectPieceCollisionVolumeData(L, ParseFeature(L, __func__, 1)));
}

/***
 *
 * @function Spring.SetFeaturePieceVisible
 * @param featureID integer
 * @param pieceIndex number
 * @param visible boolean
 * @return nil
 */
int LuaSyncedCtrl::SetFeaturePieceVisible(lua_State* L)
{
	return (SetSolidObjectPieceVisible(L, ParseFeature(L, __func__, 1)));
}


/*** Set the fire timer for a feature.
 *
 * @function Spring.SetFeatureFireTime
 *
 * Starts or resets an internal feature fire timer, when reaching zero the
 * feature will be destroyed.
 *
 * @param featureID integer
 * @param fireTime number in seconds
 */
int LuaSyncedCtrl::SetFeatureFireTime(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	const float fireTime = luaL_checknumber(L, 2);

	if (fireTime < 0)
		return luaL_error(L, "[%s] 'fireTime' must be >= 0", __func__);

	const int prevFireTime = feature->fireTime;
	feature->fireTime = fireTime * GAME_SPEED;

	if (prevFireTime <= 0 && feature->fireTime > 0)
		featureHandler.SetFeatureUpdateable(feature);

	return 0;
}


/*** Set the smoke timer for a feature.
 *
 * @function Spring.SetFeatureSmokeTime
 *
 * If different than zero, starts emitting smoke until the timer counts down to zero.
 *
 * Setting to zero will stop smoke emission by the feature.
 *
 * The smoke timer affects both the duration and size of the smoke particles.
 *
 * @param featureID integer
 * @param smokeTime number in seconds
 */
int LuaSyncedCtrl::SetFeatureSmokeTime(lua_State* L)
{
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	const float smokeTime = luaL_checknumber(L, 2);

	if (smokeTime < 0)
		return luaL_error(L, "[%s] 'smokeTime' must be >= 0", __func__);

	const int prevSmokeTime = feature->smokeTime;
	feature->smokeTime = smokeTime * GAME_SPEED;

	if (prevSmokeTime <= 0 && feature->smokeTime > 0)
		featureHandler.SetFeatureUpdateable(feature);

	return 0;
}


/******************************************************************************
 * Wrecks
 * @section wrecks
******************************************************************************/


/*** Create a wreck from a unit
 *
 * @function Spring.CreateUnitWreck
 *
 * @param unitID integer
 * @param wreckLevel integer? (Default: `1`) Wreck index to use.
 * @param doSmoke boolean? (Default: `true`) Wreck emits smoke when `true`. 
 * @return integer? featureID The wreck featureID, or nil if it couldn't be created or unit doesn't exist.
 */
int LuaSyncedCtrl::CreateUnitWreck(lua_State* L)
{
	CheckAllowGameChanges(L);
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const int wreckLevel = luaL_optint(L, 2, 1) - 1;
	const int doSmoke = luaL_optboolean(L, 3, true);

	CFeature* wreck = unit->CreateWreck(wreckLevel, doSmoke ? 1 : 0);

	if (wreck) {
		lua_pushinteger(L, wreck->id);
		return 1;
	}

	return 0;
}


/*** Create a wreck from a feature
 *
 * @function Spring.CreateFeatureWreck
 *
 * @param featureID integer
 * @param wreckLevel integer? (Default: `1`) Wreck index to use.
 * @param doSmoke boolean? (Default: `false`) Wreck emits smoke when `true`.
 * @return integer? featureID The wreck featureID, or nil if it couldn't be created or unit doesn't exist.
 */

int LuaSyncedCtrl::CreateFeatureWreck(lua_State* L)
{
	CheckAllowGameChanges(L);
	CFeature* feature = ParseFeature(L, __func__, 1);

	if (feature == nullptr)
		return 0;

	const int wreckLevel = luaL_optint(L, 2, 1) - 1;
	const int doSmoke = luaL_optboolean(L, 3, false);

	CFeature* wreck = feature->CreateWreck(wreckLevel, doSmoke ? 1 : 0);
	if (wreck) {
		lua_pushinteger(L, wreck->id);
		return 1;
	}

	return 0;
}


/******************************************************************************
 * Projectiles
 * @section projectiles
******************************************************************************/

/***
 * @function Spring.SetProjectileAlwaysVisible
 * @param projectileID integer
 * @param alwaysVisible boolean
 * @return nil
 */
int LuaSyncedCtrl::SetProjectileAlwaysVisible(lua_State* L)
{
	return (SetWorldObjectAlwaysVisible(L, ParseProjectile(L, __func__, 1), __func__));
}


/***
 *
 * @function Spring.SetProjectileUseAirLos
 * @param projectileID integer
 * @param useAirLos boolean
 * @return nil
 */
int LuaSyncedCtrl::SetProjectileUseAirLos(lua_State* L)
{
	return (SetWorldObjectUseAirLos(L, ParseProjectile(L, __func__, 1), __func__));
}


/*** Set whether engine should process position and speed for a projectile
 *
 * Contrary to `Spring.SetFeatureMoveCtrl`, speed and position for projectiles
 * can be set regardless of whether this method has been called or not.
 *
 * Passing true merely skips engine updating velocity and position.
 *
 * @function Spring.SetProjectileMoveControl
 *
 * @param projectileID integer
 * @param enable boolean
 */
int LuaSyncedCtrl::SetProjectileMoveControl(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr)
		return 0;
	if (!proj->weapon && !proj->piece)
		return 0;

	proj->luaMoveCtrl = luaL_optboolean(L, 2, false);
	return 0;
}

/*** Set the position of a projectile
 *
 * @function Spring.SetProjectilePosition
 * @param projectileID integer
 * @param posX number? (Default: `0`)
 * @param posY number? (Default: `0`)
 * @param posZ number? (Default: `0`)
 */
int LuaSyncedCtrl::SetProjectilePosition(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr)
		return 0;

	proj->pos.x = luaL_optfloat(L, 2, 0.0f);
	proj->pos.y = luaL_optfloat(L, 3, 0.0f);
	proj->pos.z = luaL_optfloat(L, 4, 0.0f);

	return 0;
}

/*** Set the velocity of a projectile
 *
 * > [!NOTE]
 * > Differently from features, `Spring.SetProjectileMoveControl` is not
 * > required to have been called to make use of this method, but often used in
 * > conjunction.
 *
 * @see Spring.SetProjectileMoveControl
 * @function Spring.SetProjectileVelocity
 * @param projectileID integer
 * @param velX number in elmos/frame
 * @param velY number in elmos/frame
 * @param velZ number in elmos/frame
 */
int LuaSyncedCtrl::SetProjectileVelocity(lua_State* L)
{
	return (SetWorldObjectVelocity(L, ParseProjectile(L, __func__, 1)));
}

/***
 * @function Spring.SetProjectileCollision
 * @param projectileID integer
 */
int LuaSyncedCtrl::SetProjectileCollision(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr)
		return 0;

	proj->Collision();
	return 0;
}

/***
 * @enum ProjectileTargetType
 * @x_helper
 * @field feature 102 ASCII number for the character `f`
 * @field projectile 112 ASCII number for the character `p`
 * @field unit 117 ASCII number for the character `u`
 */

/*** Set projectile target (object)
 *
 * @function Spring.SetProjectileTarget
 * @param projectileID integer
 * @param targetID number
 * @param targetType ProjectileTargetType
 * @return boolean? validTarget
 */

/*** Set projectile target (position)
 *
 * @function Spring.SetProjectileTarget
 *
 * @param projectileID integer
 * @param posX number
 * @param posY number
 * @param posZ number
 * @return boolean? validTarget
 */
int LuaSyncedCtrl::SetProjectileTarget(lua_State* L)
{
	CProjectile* pro = ParseProjectile(L, __func__, 1);
	CWeaponProjectile* wpro = nullptr;

	if (pro == nullptr)
		return 0;
	if (!pro->weapon)
		return 0;

	struct ProjectileTarget {
		static DependenceType GetObjectDepType(const CWorldObject* o) {
			if (dynamic_cast<const      CSolidObject*>(o) != nullptr) return DEPENDENCE_WEAPONTARGET;
			if (dynamic_cast<const CWeaponProjectile*>(o) != nullptr) return DEPENDENCE_INTERCEPTTARGET;
			return DEPENDENCE_NONE;
		}
	};

	wpro = static_cast<CWeaponProjectile*>(pro);

	switch (lua_gettop(L)) {
		case 3: {
			const int id = luaL_checkint(L, 2);
			const int type = luaL_checkint(L, 3);

			CWorldObject* oldTargetObject = wpro->GetTargetObject();
			CWorldObject* newTargetObject = nullptr;

			switch (type) {
				case 'u': { newTargetObject = ParseUnit(L, __func__, 2); } break;
				case 'f': { newTargetObject = ParseFeature(L, __func__, 2); } break;
				case 'p': { newTargetObject = ParseProjectile(L, __func__, 2); } break;
				case 'g': { /* fall-through, needs four arguments (todo: or a table?) */ }
				default: { /* if invalid type-argument, current target will be cleared */ } break;
			}

			const DependenceType oldDepType = ProjectileTarget::GetObjectDepType(oldTargetObject);
			const DependenceType newDepType = ProjectileTarget::GetObjectDepType(newTargetObject);

			if (oldTargetObject != nullptr) {
				wpro->DeleteDeathDependence(oldTargetObject, oldDepType);
				wpro->SetTargetObject(nullptr);
			}
			if (newTargetObject != nullptr) {
				wpro->AddDeathDependence(newTargetObject, newDepType);
				wpro->SetTargetObject(newTargetObject);
			}

			assert(newTargetObject == nullptr || newTargetObject->id == id);
			lua_pushboolean(L, oldTargetObject != nullptr || newTargetObject != nullptr);
			return 1;
		} break;

		case 4: {
			if (wpro->GetTargetObject() != nullptr) {
				wpro->DeleteDeathDependence(wpro->GetTargetObject(), ProjectileTarget::GetObjectDepType(wpro->GetTargetObject()));
			}

			wpro->SetTargetObject(nullptr);
			wpro->SetTargetPos(float3(luaL_checkfloat(L, 2), luaL_checkfloat(L, 3), luaL_checkfloat(L, 4)));

			lua_pushboolean(L, wpro->GetTargetObject() == nullptr);
			return 1;
		} break;
	}

	return 0;
}


/*** Set Time To Live for a projectile
 *
 * @function Spring.SetProjectileTimeToLive
 * @param projectileID integer
 * @param ttl number Remaining time to live in frames
 */
int LuaSyncedCtrl::SetProjectileTimeToLive(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	const int newTimeToLive = luaL_checkint(L, 2);

	if (proj == nullptr || !proj->weapon)
		return 0;

	CWeaponProjectile* wproj = static_cast<CWeaponProjectile*>(proj);

	wproj->SetTimeToLive(newTimeToLive);
	return 0;
}


/***
 * @function Spring.SetProjectileIsIntercepted
 * @param projectileID integer
 */
int LuaSyncedCtrl::SetProjectileIsIntercepted(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr || !proj->weapon)
		return 0;

	CWeaponProjectile* wpro = static_cast<CWeaponProjectile*>(proj);

	wpro->SetBeingIntercepted(luaL_checkboolean(L, 2));
	return 0;
}



/***
 * @function Spring.SetProjectileDamages
 * @param unitID integer
 * @param weaponNum integer
 * @param key string
 * @param value number
 */
int LuaSyncedCtrl::SetProjectileDamages(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr || !proj->weapon)
		return 0;

	CWeaponProjectile* wpro = static_cast<CWeaponProjectile*>(proj);
	DynDamageArray* damages = DynDamageArray::GetMutable(wpro->damages);


	if (lua_istable(L, 3)) {
		// {key1 = value1, ...}
		for (lua_pushnil(L); lua_next(L, 3) != 0; lua_pop(L, 1)) {
			if (lua_israwstring(L, LUA_TABLE_KEY_INDEX) && lua_isnumber(L, LUA_TABLE_VALUE_INDEX)) {
				SetSingleDynDamagesKey(L, damages, LUA_TABLE_KEY_INDEX);
			}
		}
	} else {
		// key, value
		if (lua_israwstring(L, 3) && lua_isnumber(L, 4)) {
			SetSingleDynDamagesKey(L, damages, 3);
		}
	}

	return 0;
}


/***
 * @function Spring.SetProjectileIgnoreTrackingError
 * @param projectileID integer
 * @param ignore boolean
 */
int LuaSyncedCtrl::SetProjectileIgnoreTrackingError(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr)
		return 0;

	switch(proj->GetProjectileType()) {
		case WEAPON_MISSILE_PROJECTILE: {
			static_cast<CMissileProjectile*>(proj)->SetIgnoreError(luaL_checkboolean(L, 2));
		} break;
		case WEAPON_STARBURST_PROJECTILE: {
			static_cast<CStarburstProjectile*>(proj)->SetIgnoreError(luaL_checkboolean(L, 2));
		} break;
		case WEAPON_TORPEDO_PROJECTILE: {
			static_cast<CTorpedoProjectile*>(proj)->SetIgnoreError(luaL_checkboolean(L, 2));
		} break;
		default: break;
	}

	return 0;
}


/***
 * @function Spring.SetProjectileGravity
 * @param projectileID integer
 * @param grav number? (Default: `0`)
 * @return nil
 */
int LuaSyncedCtrl::SetProjectileGravity(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr)
		return 0;

	proj->mygravity = luaL_optfloat(L, 2, 0.0f);
	return 0;
}


int LuaSyncedCtrl::SetProjectileSpinAngle(lua_State* L) { return 0; } // FIXME: DELETE ME
int LuaSyncedCtrl::SetProjectileSpinSpeed(lua_State* L) { return 0; } // FIXME: DELETE ME
int LuaSyncedCtrl::SetProjectileSpinVec(lua_State* L) { return 0; } // FIXME: DELETE ME


/*** Set piece projectile params
 *
 * Non passed or nil args don't set params.
 *
 * @function Spring.SetPieceProjectileParams
 * @param projectileID integer
 * @param explosionFlags number?
 * @param spinAngle number?
 * @param spinSpeed number?
 * @param spinVectorX number?
 * @param spinVectorY number?
 * @param spinVectorZ number?
 */
int LuaSyncedCtrl::SetPieceProjectileParams(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr || !proj->piece)
		return 0;

	CPieceProjectile* pproj = static_cast<CPieceProjectile*>(proj);

	pproj->explFlags = luaL_optint(L, 2, pproj->explFlags);
	pproj->spinAngle = luaL_optfloat(L, 3, pproj->spinAngle);
	pproj->spinSpeed = luaL_optfloat(L, 4, pproj->spinSpeed);
	pproj->spinVec.x = luaL_optfloat(L, 5, pproj->spinVec.x);
	pproj->spinVec.y = luaL_optfloat(L, 6, pproj->spinVec.y);
	pproj->spinVec.z = luaL_optfloat(L, 7, pproj->spinVec.z);
	return 0;
}

//
// TODO: move this and SpawnCEG to LuaUnsyncedCtrl
//
/***
 * @function Spring.SetProjectileCEG
 * @param projectileID integer
 * @param ceg_name string
 * @return nil
 */
int LuaSyncedCtrl::SetProjectileCEG(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr)
		return 0;
	if (!proj->weapon && !proj->piece)
		return 0;

	unsigned int cegID = CExplosionGeneratorHandler::EXPGEN_ID_INVALID;

	if (lua_israwstring(L, 2)) {
		cegID = explGenHandler.LoadCustomGeneratorID(lua_tostring(L, 2));
	} else {
		cegID = luaL_checknumber(L, 2);
	}

	// if cegID is EXPGEN_ID_INVALID, this also returns NULL
	if (explGenHandler.GetGenerator(cegID) != nullptr)
		proj->SetCustomExpGenID(cegID);

	lua_pushnumber(L, cegID);
	return 1;
}

/***
 * Give Order
 * @section giveorder
 * Options can also be a bitmask; e.g. 0 instead of an empty table (can avoid performance hit on table creation)
 * See `Constants.CMD` for relevant constants.
 */


/***
 * @function Spring.UnitFinishCommand
 * @param unitID integer
 */
int LuaSyncedCtrl::UnitFinishCommand(lua_State* L)
{
	CheckAllowGameChanges(L);

	CUnit* unit = ParseUnit(L, __func__, 1);
	if (unit == nullptr)
		luaL_error(L, "[%s] invalid unitID", __func__);

	CCommandAI* cai = unit->commandAI;
	if (!cai->commandQue.empty())
		cai->FinishCommand();

	return 0;
}

/***
 * @function Spring.GiveOrderToUnit
 * @param unitID integer
 * @param cmdID CMD|integer The command ID.
 * @param params CreateCommandParams? Parameters for the given command.
 * @param options CreateCommandOptions?
 * @param timeout integer? Absolute frame number. The command will be discarded after this frame. Only respected by mobile units.

 * @return boolean unitOrdered
 */
int LuaSyncedCtrl::GiveOrderToUnit(lua_State* L)
{
	CheckAllowGameChanges(L);

	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		luaL_error(L, "[%s] invalid unitID", __func__);

	Command cmd = LuaUtils::ParseCommand(L, __func__, 2);

	if (!CanControlUnit(L, unit)) {
		lua_pushboolean(L, false);
		return 1;
	}

	if (inGiveOrder >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "[%s] recursion not permitted, max depth: %d", __func__, MAX_CMD_RECURSION_DEPTH);

	inGiveOrder++;
	unit->commandAI->GiveCommand(cmd, -1, true, true);
	inGiveOrder--;

	lua_pushboolean(L, true);
	return 1;
}


/***
 * Give order to multiple units, specified by table keys.
 * 
 * @function Spring.GiveOrderToUnitMap
 * @param unitMap table<integer, any> A table with unit IDs as keys.
 * @param cmdID CMD|integer The command ID.
 * @param params CreateCommandParams? Parameters for the given command.
 * @param options CreateCommandOptions?
 * @param timeout integer? Absolute frame number. The command will be discarded after this frame. Only respected by mobile units.
 * @return integer unitsOrdered The number of units ordered.
 */
int LuaSyncedCtrl::GiveOrderToUnitMap(lua_State* L)
{
	CheckAllowGameChanges(L);

	// units
	std::vector<CUnit*> units;

	ParseUnitMap(L, __func__, 1, units);

	if (units.empty()) {
		lua_pushnumber(L, 0);
		return 1;
	}

	Command cmd = LuaUtils::ParseCommand(L, __func__, 2);

	if (inGiveOrder >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "[%s] recursion not permitted, max depth: %d", __func__, MAX_CMD_RECURSION_DEPTH);

	inGiveOrder++;
	int count = 0;
	for (CUnit* unit: units) {
		if (CanControlUnit(L, unit)) {
			unit->commandAI->GiveCommand(cmd, -1, true, true);
			count++;
		}
	}
	inGiveOrder--;

	lua_pushnumber(L, count);
	return 1;
}


/***
 *
 * @function Spring.GiveOrderToUnitArray
 * @param unitIDs integer[] An array of unit IDs.
 * @param cmdID CMD|integer The command ID.
 * @param params CreateCommandParams? Parameters for the given command.
 * @param options CreateCommandOptions?
 * @param timeout integer? Absolute frame number. The command will be discarded after this frame. Only respected by mobile units.
 * @return integer unitsOrdered The number of units ordered.
 */
int LuaSyncedCtrl::GiveOrderToUnitArray(lua_State* L)
{
	CheckAllowGameChanges(L);

	// units
	std::vector<CUnit*> units;

	ParseUnitArray(L, __func__, 1, units);

	if (units.empty()) {
		lua_pushnumber(L, 0);
		return 1;
	}

	Command cmd = LuaUtils::ParseCommand(L, __func__, 2);

	if (inGiveOrder >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "[%s] recursion not permitted, max depth: %d", __func__, MAX_CMD_RECURSION_DEPTH);

	inGiveOrder++;

	int count = 0;
	for (CUnit* unit: units) {
		if (CanControlUnit(L, unit)) {
			unit->commandAI->GiveCommand(cmd, -1, true, true);
			count++;
		}
	}

	inGiveOrder--;

	lua_pushnumber(L, count);
	return 1;
}


/***
 *
 * @function Spring.GiveOrderArrayToUnit
 * @param unitID integer
 * @param commands CreateCommand[]
 * @return boolean ordersGiven
 */
int LuaSyncedCtrl::GiveOrderArrayToUnit(lua_State* L)
{
	CheckAllowGameChanges(L);

	CUnit* const unit = ParseUnit(L, __func__, 1);
	if (unit == nullptr)
		luaL_error(L, "[%s] invalid unitID", __func__);
	if (!CanControlUnit(L, unit)) {
		lua_pushboolean(L, false);
		return 1;
	}

	std::vector<Command> commands;
	LuaUtils::ParseCommandArray(L, __func__, 2, commands);
	if (commands.empty()) {
		lua_pushboolean(L, false);
		return 1;
	}

	if (inGiveOrder >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "[%s] recursion not permitted, max depth: %d", __func__, MAX_CMD_RECURSION_DEPTH);

	inGiveOrder ++;

	for (const Command& c: commands)
		unit->commandAI->GiveCommand(c, -1, true, true);

	inGiveOrder --;

	lua_pushboolean(L, true);
	return 1;
}


/***
 * @function Spring.GiveOrderArrayToUnitMap
 * @param unitMap table<integer, any> A table with unit IDs as keys.
 * @param commands CreateCommand[]
 * @return integer unitsOrdered The number of units ordered.
 */
int LuaSyncedCtrl::GiveOrderArrayToUnitMap(lua_State* L)
{
	CheckAllowGameChanges(L);

	std::vector<CUnit*> units;
	std::vector<Command> commands;

	ParseUnitMap(L, __func__, 1, units);
	LuaUtils::ParseCommandArray(L, __func__, 2, commands);

	if (units.empty() || commands.empty()) {
		lua_pushnumber(L, 0);
		return 1;
	}

	if (inGiveOrder >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "[%s] recursion not permitted, max depth: %d", __func__, MAX_CMD_RECURSION_DEPTH);

	inGiveOrder++;

	int count = 0;
	for (CUnit* unit: units) {
		if (CanControlUnit(L, unit)) {
			for (const Command& c: commands) {
				unit->commandAI->GiveCommand(c, -1, true, true);
			}
			count++;
		}
	}
	inGiveOrder--;

	lua_pushnumber(L, count);
	return 1;
}


/***
 * @function Spring.GiveOrderArrayToUnitArray
 * @param unitIDs integer[] Array of unit IDs.
 * @param commands CreateCommand[]
 * @param pairwise boolean? (Default: `false`) When `false`, assign all commands to each unit.
 *
 * When `true`, assign commands according to index between units and cmds arrays.
 *
 * If `len(unitArray) < len(cmdArray)` only the first `len(unitArray)` commands
 * will be assigned, and vice-versa.
 *
 * @return integer unitsOrdered The number of units ordered.
 */
int LuaSyncedCtrl::GiveOrderArrayToUnitArray(lua_State* L)
{
	CheckAllowGameChanges(L);

	// units
	std::vector<CUnit*> units;
	std::vector<Command> commands;

	ParseUnitArray(L, __func__, 1, units);
	LuaUtils::ParseCommandArray(L, __func__, 2, commands);

	if (units.empty() || commands.empty()) {
		lua_pushnumber(L, 0);
		return 1;
	}

	if (inGiveOrder >= MAX_CMD_RECURSION_DEPTH)
		luaL_error(L, "[%s] recursion not permitted, max depth: %d", __func__, MAX_CMD_RECURSION_DEPTH);

	inGiveOrder++;

	int count = 0;

	if (luaL_optboolean(L, 3, false)) {
		// pairwise
		for (size_t i = 0, n = std::min(units.size(), commands.size()); i < n; ++i) {
			CUnit* unit = units[i];
			if (CanControlUnit(L, unit)) {
				unit->commandAI->GiveCommand(commands[i], -1, true, true);
				count++;
			}
		}
	} else {
		for (CUnit* unit: units) {
			if (CanControlUnit(L, unit)) {
				for (const Command& c: commands) {
					unit->commandAI->GiveCommand(c, -1, true, true);
				}
				count++;
			}
		}
	}
	inGiveOrder--;

	lua_pushnumber(L, count);
	return 1;
}


/******************************************************************************/
/******************************************************************************/

/* - no doc
 * @param x number
 * @param z number
 * @param height number
 */
/* - no doc
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number
 * @param factor number
 */
static void ParseParams(lua_State* L, const char* caller, float& factor,
		int& x1, int& z1, int& x2, int& z2, int resolution, int maxX, int maxZ)
{
	float fx1 = 0.0f;
	float fz1 = 0.0f;
	float fx2 = 0.0f;
	float fz2 = 0.0f;

	const int args = lua_gettop(L); // number of arguments
	if (args == 3) {
		fx1 = fx2 = luaL_checkfloat(L, 1);
		fz1 = fz2 = luaL_checkfloat(L, 2);
		factor    = luaL_checkfloat(L, 3);
	}
	else if (args == 5) {
		fx1    = luaL_checkfloat(L, 1);
		fz1    = luaL_checkfloat(L, 2);
		fx2    = luaL_checkfloat(L, 3);
		fz2    = luaL_checkfloat(L, 4);
		factor = luaL_checkfloat(L, 5);
		if (fx1 > fx2) {
			std::swap(fx1, fx2);
		}
		if (fz1 > fz2) {
			std::swap(fz1, fz2);
		}
	}
	else {
		luaL_error(L, "Incorrect arguments to %s()", caller);
	}

	// quantize and clamp
	x1 = std::clamp((int)(fx1 / resolution), 0, maxX);
	x2 = std::clamp((int)(fx2 / resolution), 0, maxX);
	z1 = std::clamp((int)(fz1 / resolution), 0, maxZ);
	z2 = std::clamp((int)(fz2 / resolution), 0, maxZ);

}

static inline void ParseMapParams(lua_State* L, const char* caller,
		float& factor, int& x1, int& z1, int& x2, int& z2)
{
	ParseParams(L, caller, factor, x1, z1, x2, z2, SQUARE_SIZE, mapDims.mapx, mapDims.mapy);
}

/******************************************************************************
 * Heightmap
 * @section heightmap
 * Note that x & z coords are in worldspace (Game.mapSizeX/Z), still the heightmap resolution is Game.squareSize.
******************************************************************************/


/*** 
 * Set the height of a point in the world.
 * 
 * @function Spring.LevelHeightMap
 * @param x number
 * @param z number
 * @param height number
 */
/***
 * Set the height of a rectangle area in the world.
 * 
 * @function Spring.LevelHeightMap
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number?
 * @return nil
 */
int LuaSyncedCtrl::LevelHeightMap(lua_State* L)
{
	if (mapDamage->Disabled()) {
		return 0;
	}
	float height;
	int x1, x2, z1, z2;
	ParseMapParams(L, __func__, height, x1, z1, x2, z2);

	for (int z = z1; z <= z2; z++) {
		for (int x = x1; x <= x2; x++) {
			readMap->SetHeight((z * mapDims.mapxp1) + x, height);
		}
	}

	mapDamage->RecalcArea(x1, x2, z1, z2);
	return 0;
}


/*** 
 * Add height to a point in the world.
 *
 * @function Spring.AdjustHeightMap
 * @param x number
 * @param z number
 * @param height number
 */

/***
 * Add height to a rectangle in the world.
 * 
 * @function Spring.AdjustHeightMap
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number?
 * @return nil
 */
int LuaSyncedCtrl::AdjustHeightMap(lua_State* L)
{
	if (mapDamage->Disabled()) {
		return 0;
	}

	float height;
	int x1, x2, z1, z2;

	ParseMapParams(L, __func__, height, x1, z1, x2, z2);

	for (int z = z1; z <= z2; z++) {
		for (int x = x1; x <= x2; x++) {
			readMap->AddHeight((z * mapDims.mapxp1) + x, height);
		}
	}

	mapDamage->RecalcArea(x1, x2, z1, z2);
	return 0;
}

/*** 
 * Restore map height at a point in the world.
 *
 * @function Spring.RevertHeightMap
 * @param x number
 * @param z number
 * @param height number
 */
/***
 * Restore map height of a rectangle area in the world.
 * 
 * @function Spring.RevertHeightMap
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number?
 * @return nil
 */
int LuaSyncedCtrl::RevertHeightMap(lua_State* L)
{
	if (mapDamage->Disabled()) {
		return 0;
	}
	float origFactor;
	int x1, x2, z1, z2;
	ParseMapParams(L, __func__, origFactor, x1, z1, x2, z2);

	const float* origMap = readMap->GetOriginalHeightMapSynced();
	const float* currMap = readMap->GetCornerHeightMapSynced();

	if (origFactor == 1.0f) {
		for (int z = z1; z <= z2; z++) {
			for (int x = x1; x <= x2; x++) {
				const int idx = (z * mapDims.mapxp1) + x;

				readMap->SetHeight(idx, origMap[idx]);
			}
		}
	}
	else {
		const float currFactor = (1.0f - origFactor);
		for (int z = z1; z <= z2; z++) {
			for (int x = x1; x <= x2; x++) {
				const int index = (z * mapDims.mapxp1) + x;
				const float ofh = origFactor * origMap[index];
				const float cfh = currFactor * currMap[index];
				readMap->SetHeight(index, ofh + cfh);
			}
		}
	}

	mapDamage->RecalcArea(x1, x2, z1, z2);
	return 0;
}


/*** Can only be called in `Spring.SetHeightMapFunc`
 *
 * @function Spring.AddHeightMap
 * @param x number
 * @param z number
 * @param height number
 * @return integer? newHeight
 */
int LuaSyncedCtrl::AddHeightMap(lua_State* L)
{
	if (!inHeightMap) {
		luaL_error(L, "AddHeightMap() can only be called in SetHeightMapFunc()");
	}

	const float xl = luaL_checkfloat(L, 1);
	const float zl = luaL_checkfloat(L, 2);
	const float h  = luaL_checkfloat(L, 3);

	// quantize
	const int x = (int)(xl / SQUARE_SIZE);
	const int z = (int)(zl / SQUARE_SIZE);

	// discard invalid coordinates
	if ((x < 0) || (x > mapDims.mapx) ||
	    (z < 0) || (z > mapDims.mapy)) {
		return 0;
	}

	const int index = (z * mapDims.mapxp1) + x;
	const float oldHeight = readMap->GetCornerHeightMapSynced()[index];
	heightMapAmountChanged += math::fabsf(h);

	// update RecalcArea()
	if (x < heightMapx1) { heightMapx1 = x; }
	if (x > heightMapx2) { heightMapx2 = x; }
	if (z < heightMapz1) { heightMapz1 = z; }
	if (z > heightMapz2) { heightMapz2 = z; }

	readMap->AddHeight(index, h);
	// push the new height
	lua_pushnumber(L, oldHeight + h);
	return 1;
}


/***
 *
 * @function Spring.SetHeightMap
 *
 * Can only be called in `Spring.SetHeightMapFunc`.
 *
 * @param x number
 * @param z number
 * @param height number
 * @param terraform number? (Default: `1`) Scaling factor.
 * @return integer? absHeightDiff If `0`, nothing will be changed (the terraform starts), if `1` the terraform will be finished.
 *
 */
int LuaSyncedCtrl::SetHeightMap(lua_State* L)
{
	if (!inHeightMap) {
		luaL_error(L, "SetHeightMap() can only be called in SetHeightMapFunc()");
	}

	const float xl = luaL_checkfloat(L, 1);
	const float zl = luaL_checkfloat(L, 2);
	const float h  = luaL_checkfloat(L, 3);

	// quantize
	const int x = (int)(xl / SQUARE_SIZE);
	const int z = (int)(zl / SQUARE_SIZE);

	// discard invalid coordinates
	if ((x < 0) || (x > mapDims.mapx) ||
	    (z < 0) || (z > mapDims.mapy)) {
		return 0;
	}

	const int index = (z * mapDims.mapxp1) + x;
	const float oldHeight = readMap->GetCornerHeightMapSynced()[index];
	float height = oldHeight;

	if (lua_israwnumber(L, 4)) {
		const float t = lua_tofloat(L, 4);
		height += (h - oldHeight) * t;
	} else{
		height = h;
	}

	const float heightDiff = (height - oldHeight);
	heightMapAmountChanged += math::fabsf(heightDiff);

	// update RecalcArea()
	if (x < heightMapx1) { heightMapx1 = x; }
	if (x > heightMapx2) { heightMapx2 = x; }
	if (z < heightMapz1) { heightMapz1 = z; }
	if (z > heightMapz2) { heightMapz2 = z; }

	readMap->SetHeight(index, height);
	lua_pushnumber(L, heightDiff);
	return 1;
}


/***
 * @function Spring.SetHeightMapFunc
 *
 * Example code:
 *
 * ```lua
 * function Spring.SetHeightMapFunc(function()
 *   for z=0,Game.mapSizeZ, Game.squareSize do
 *     for x=0,Game.mapSizeX, Game.squareSize do
 *       Spring.SetHeightMap( x, z, 200 + 20 * math.cos((x + z) / 90) )
 *     end
 *   end
 * end)
 * ```
 *
 * @param luaFunction function
 * @param arg number
 * @param ... number
 * @return integer? absTotalHeightMapAmountChanged
 */
int LuaSyncedCtrl::SetHeightMapFunc(lua_State* L)
{
	if (mapDamage->Disabled()) {
		return 0;
	}

	const int args = lua_gettop(L); // number of arguments
	if ((args < 1) || !lua_isfunction(L, 1)) {
		luaL_error(L, "Incorrect arguments to Spring.SetHeightMapFunc(func, ...)");
	}

	if (inHeightMap) {
		luaL_error(L, "SetHeightMapFunc() recursion is not permitted");
	}

	heightMapx1 = mapDims.mapx;
	heightMapx2 = -1;
	heightMapz1 = mapDims.mapy;
	heightMapz2 = 0;
	heightMapAmountChanged = 0.0f;

	inHeightMap = true;
	const int error = lua_pcall(L, (args - 1), 0, 0);
	inHeightMap = false;

	if (error != 0) {
		LOG_L(L_ERROR, "Spring.SetHeightMapFunc: error(%i) = %s",
				error, lua_tostring(L, -1));
		lua_error(L);
	}

	if (heightMapx2 > -1) {
		mapDamage->RecalcArea(heightMapx1, heightMapx2, heightMapz1, heightMapz2);
	}

	lua_pushnumber(L, heightMapAmountChanged);
	return 1;
}


/******************************************************************************
 * Height Map/Smooth Mesh
 * @section heightmap
******************************************************************************/

/***
 * Set the height of a point in the original map height cache.
 *
 * @function Spring.LevelOriginalHeightMap
 * @param x number
 * @param z number
 * @param height number
 */
/***
 * Set the height of a rectangle area in the original map height cache.
 *
 * @function Spring.LevelOriginalHeightMap
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number
 */
int LuaSyncedCtrl::LevelOriginalHeightMap(lua_State* L)
{
	if (mapDamage->Disabled()) {
		return 0;
	}
	float height;
	int x1, x2, z1, z2;
	ParseMapParams(L, __func__, height, x1, z1, x2, z2);

	for (int z = z1; z <= z2; z++) {
		for (int x = x1; x <= x2; x++) {
			readMap->SetOriginalHeight((z * mapDims.mapxp1) + x, height);
		}
	}

	return 0;
}

/***
 * Add height to a point in the original map height cache.
 *
 * @function Spring.AdjustOriginalHeightMap
 * @param x number
 * @param z number
 * @param height number
 */
/***
 * Add height to a rectangle area in the original map height cache.
 *
 * @function Spring.AdjustOriginalHeightMap
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number
 */
int LuaSyncedCtrl::AdjustOriginalHeightMap(lua_State* L)
{
	if (mapDamage->Disabled()) {
		return 0;
	}

	float height;
	int x1, x2, z1, z2;

	ParseMapParams(L, __func__, height, x1, z1, x2, z2);

	for (int z = z1; z <= z2; z++) {
		for (int x = x1; x <= x2; x++) {
			readMap->AddOriginalHeight((z * mapDims.mapxp1) + x, height);
		}
	}

	return 0;
}


/*** 
 * Restore original map height at a point in the world.
 *
 * @function Spring.RevertOriginalHeightMap
 * @param x number
 * @param z number
 * @param height number
 */
/***
 * Restore original map height over a rectangle in the world.
 * 
 * @function Spring.RevertOriginalHeightMap
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number?
 * @return nil
 */
int LuaSyncedCtrl::RevertOriginalHeightMap(lua_State* L)
{
	if (mapDamage->Disabled()) {
		return 0;
	}
	float origFactor;
	int x1, x2, z1, z2;
	ParseMapParams(L, __func__, origFactor, x1, z1, x2, z2);

	const float* origMap = readMap->GetMapFileHeightMapSynced();
	const float* currMap = readMap->GetOriginalHeightMapSynced();

	if (origFactor == 1.0f) {
		for (int z = z1; z <= z2; z++) {
			for (int x = x1; x <= x2; x++) {
				const int idx = (z * mapDims.mapxp1) + x;

				readMap->SetOriginalHeight(idx, origMap[idx]);
			}
		}
	}
	else {
		const float currFactor = (1.0f - origFactor);
		for (int z = z1; z <= z2; z++) {
			for (int x = x1; x <= x2; x++) {
				const int index = (z * mapDims.mapxp1) + x;
				const float ofh = origFactor * origMap[index];
				const float cfh = currFactor * currMap[index];
				readMap->SetOriginalHeight(index, ofh + cfh);
			}
		}
	}

	return 0;
}


/***
 *
 * @function Spring.AddOriginalHeightMap
 *
 * Can only be called in `Spring.SetOriginalHeightMapFunc`
 *
 * @param x number
 * @param y number
 * @param height number
 * @return nil
 */
int LuaSyncedCtrl::AddOriginalHeightMap(lua_State* L)
{
	if (!inOriginalHeightMap) {
		luaL_error(L, "AddOriginalHeightMap() can only be called in SetOriginalHeightMapFunc()");
	}

	const float xl = luaL_checkfloat(L, 1);
	const float zl = luaL_checkfloat(L, 2);
	const float h  = luaL_checkfloat(L, 3);

	// quantize
	const int x = (int)(xl / SQUARE_SIZE);
	const int z = (int)(zl / SQUARE_SIZE);

	// discard invalid coordinates
	if ((x < 0) || (x > mapDims.mapx) ||
	    (z < 0) || (z > mapDims.mapy)) {
		return 0;
	}

	const int index = (z * mapDims.mapxp1) + x;
	const float oldHeight = readMap->GetOriginalHeightMapSynced()[index];
	originalHeightMapAmountChanged += math::fabsf(h);

	readMap->AddOriginalHeight(index, h);
	// push the new height
	lua_pushnumber(L, oldHeight + h);
	return 1;
}


/***
 *
 * @function Spring.SetOriginalHeightMap
 *
 * Can only be called in `Spring.SetOriginalHeightMapFunc`
 *
 * @param x number
 * @param y number
 * @param height number
 * @param factor number?
 * @return nil
 */
int LuaSyncedCtrl::SetOriginalHeightMap(lua_State* L)
{
	if (!inOriginalHeightMap) {
		luaL_error(L, "SetOriginalHeightMap() can only be called in SetOriginalHeightMapFunc()");
	}

	const float xl = luaL_checkfloat(L, 1);
	const float zl = luaL_checkfloat(L, 2);
	const float h  = luaL_checkfloat(L, 3);

	// quantize
	const int x = (int)(xl / SQUARE_SIZE);
	const int z = (int)(zl / SQUARE_SIZE);

	// discard invalid coordinates
	if ((x < 0) || (x > mapDims.mapx) ||
	    (z < 0) || (z > mapDims.mapy)) {
		return 0;
	}

	const int index = (z * mapDims.mapxp1) + x;
	const float oldHeight = readMap->GetOriginalHeightMapSynced()[index];
	float height = oldHeight;

	if (lua_israwnumber(L, 4)) {
		const float t = lua_tofloat(L, 4);
		height += (h - oldHeight) * t;
	} else{
		height = h;
	}

	const float heightDiff = (height - oldHeight);
	originalHeightMapAmountChanged += math::fabsf(heightDiff);

	readMap->SetOriginalHeight(index, height);
	lua_pushnumber(L, heightDiff);
	return 1;
}


/***
 *
 * @function Spring.SetOriginalHeightMapFunc
 *
 * Cannot recurse on itself
 *
 * @param heightMapFunc function
 * @return nil
 */
int LuaSyncedCtrl::SetOriginalHeightMapFunc(lua_State* L)
{
	if (mapDamage->Disabled()) {
		return 0;
	}

	const int args = lua_gettop(L); // number of arguments
	if ((args < 1) || !lua_isfunction(L, 1)) {
		luaL_error(L, "Incorrect arguments to Spring.SetOriginalHeightMapFunc(func, ...)");
	}

	if (inOriginalHeightMap) {
		luaL_error(L, "SetOriginalHeightMapFunc() recursion is not permitted");
	}

	originalHeightMapAmountChanged = 0.0f;

	inOriginalHeightMap = true;
	const int error = lua_pcall(L, (args - 1), 0, 0);
	inOriginalHeightMap = false;

	if (error != 0) {
		LOG_L(L_ERROR, "Spring.SetOriginalHeightMapFunc: error(%i) = %s",
				error, lua_tostring(L, -1));
		lua_error(L);
	}

	lua_pushnumber(L, originalHeightMapAmountChanged);
	return 1;
}


static inline void ParseSmoothMeshParams(lua_State* L, const char* caller,
		float& factor, int& x1, int& z1, int& x2, int& z2)
{
	ParseParams(L, caller, factor, x1, z1, x2, z2,
			smoothGround.GetResolution(),
			smoothGround.GetMaxX() - 1,
			smoothGround.GetMaxY() - 1);

}


/***
 * @function Spring.RebuildSmoothMesh
 *
 * Heightmap changes normally take up to 25s to propagate to the smooth mesh.
 * Use to force a mapwide update immediately.
 *
 * @return nil
 */
int LuaSyncedCtrl::RebuildSmoothMesh(lua_State* L)
{
	smoothGround.MakeSmoothMesh();
	return 0;
}


/***
 * @function Spring.LevelSmoothMesh
 * @param x number
 * @param z number
 * @param height number
 */
/***
 * @function Spring.LevelSmoothMesh
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number
 */

int LuaSyncedCtrl::LevelSmoothMesh(lua_State* L)
{
	float height;
	int x1, x2, z1, z2;
	ParseSmoothMeshParams(L, __func__, height, x1, z1, x2, z2);

	for (int z = z1; z <= z2; z++) {
		for (int x = x1; x <= x2; x++) {
			const int index = (z * smoothGround.GetMaxX()) + x;
			smoothGround.SetHeight(index, height);
		}
	}

	return 0;
}


/***
 * @function Spring.AdjustSmoothMesh
 * @param x number
 * @param z number
 * @param height number
 */
/***
 * @function Spring.AdjustSmoothMesh
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param height number
 */
int LuaSyncedCtrl::AdjustSmoothMesh(lua_State* L)
{
	float height;
	int x1, x2, z1, z2;
	ParseSmoothMeshParams(L, __func__, height, x1, z1, x2, z2);

	for (int z = z1; z <= z2; z++) {
		for (int x = x1; x <= x2; x++) {
			const int index = (z * smoothGround.GetMaxX()) + x;
			smoothGround.AddHeight(index, height);
		}
	}

	return 0;
}

/***
 * @function Spring.RevertSmoothMesh
 * @param x number
 * @param z number
 * @param origFactor number
 */
/***
 * @function Spring.RevertSmoothMesh
 * @param x1 number
 * @param z1 number
 * @param x2 number
 * @param z2 number
 * @param origFactor number
 */
int LuaSyncedCtrl::RevertSmoothMesh(lua_State* L)
{
	float origFactor;
	int x1, x2, z1, z2;
	ParseSmoothMeshParams(L, __func__, origFactor, x1, z1, x2, z2);

	const float* origMap = smoothGround.GetOriginalMeshData();
	const float* currMap = smoothGround.GetMeshData();

	if (origFactor == 1.0f) {
		for (int z = z1; z <= z2; z++) {
			for (int x = x1; x <= x2; x++) {
				const int idx = (z * smoothGround.GetMaxX()) + x;
				smoothGround.SetHeight(idx, origMap[idx]);
			}
		}
	}
	else {
		const float currFactor = (1.0f - origFactor);
		for (int z = z1; z <= z2; z++) {
			for (int x = x1; x <= x2; x++) {
				const int index = (z * smoothGround.GetMaxX()) + x;
				const float ofh = origFactor * origMap[index];
				const float cfh = currFactor * currMap[index];
				smoothGround.SetHeight(index, ofh + cfh);
			}
		}
	}

	return 0;
}


/*** Can only be called in `Spring.SetSmoothMeshFunc`.
 *
 * @function Spring.AddSmoothMesh
 * @param x number
 * @param z number
 * @param height number
 * @return number? height The new height, or `nil` if coordinates are invalid.
 */
int LuaSyncedCtrl::AddSmoothMesh(lua_State* L)
{
	if (!inSmoothMesh) {
		luaL_error(L, "AddSmoothMesh() can only be called in SetSmoothMeshFunc()");
	}

	const float xl = luaL_checkfloat(L, 1);
	const float zl = luaL_checkfloat(L, 2);
	const float h  = luaL_checkfloat(L, 3);

	// quantize
	const int x = (int)(xl / smoothGround.GetResolution());
	const int z = (int)(zl / smoothGround.GetResolution());

	// discard invalid coordinates
	if ((x < 0) || (x > smoothGround.GetMaxX() - 1) ||
	    (z < 0) || (z > smoothGround.GetMaxY() - 1)) {
		return 0;
	}

	const int index = (z * smoothGround.GetMaxX()) + x;
	const float oldHeight = smoothGround.GetMeshData()[index];
	smoothMeshAmountChanged += math::fabsf(h);

	smoothGround.AddHeight(index, h);
	// push the new height
	lua_pushnumber(L, oldHeight + h);
	return 1;
}

/***Can only be called in `Spring.SetSmoothMeshFunc`.
 *
 * @function Spring.SetSmoothMesh
 * @param x number
 * @param z number
 * @param height number
 * @param terraform number? (Default: `1`)
 * @return number? The absolute height difference, or `nil` if coordinates are invalid.
 */
int LuaSyncedCtrl::SetSmoothMesh(lua_State* L)
{
	if (!inSmoothMesh) {
		luaL_error(L, "SetSmoothMesh() can only be called in SetSmoothMeshFunc()");
	}

	const float xl = luaL_checkfloat(L, 1);
	const float zl = luaL_checkfloat(L, 2);
	const float h  = luaL_checkfloat(L, 3);

	// quantize
	const int x = (int)(xl / smoothGround.GetResolution());
	const int z = (int)(zl / smoothGround.GetResolution());

	// discard invalid coordinates
	if ((x < 0) || (x > smoothGround.GetMaxX() - 1) ||
	    (z < 0) || (z > smoothGround.GetMaxY() - 1)) {
		return 0;
	}

	const int index = (z * (smoothGround.GetMaxX())) + x;
	const float oldHeight = smoothGround.GetMeshData()[index];
	float height = oldHeight;

	if (lua_israwnumber(L, 4)) {
		const float t = lua_tofloat(L, 4);
		height += (h - oldHeight) * t;
	} else{
		height = h;
	}

	const float heightDiff = (height - oldHeight);
	smoothMeshAmountChanged += math::fabsf(heightDiff);

	smoothGround.SetHeight(index, height);
	lua_pushnumber(L, heightDiff);
	return 1;
}

/***
 * @function Spring.SetSmoothMeshFunc
 * @param luaFunction function
 * @param arg any?
 * @param ... any?
 * @return number? absTotalHeightMapAmountChanged
 */
int LuaSyncedCtrl::SetSmoothMeshFunc(lua_State* L)
{
	const int args = lua_gettop(L); // number of arguments
	if ((args < 1) || !lua_isfunction(L, 1)) {
		luaL_error(L, "Incorrect arguments to Spring.SetSmoothMeshFunc(func, ...)");
	}

	if (inSmoothMesh) {
		luaL_error(L, "SetHeightMapFunc() recursion is not permitted");
	}

	smoothMeshAmountChanged = 0.0f;

	inSmoothMesh = true;
	const int error = lua_pcall(L, (args - 1), 0, 0);
	inSmoothMesh = false;

	if (error != 0) {
		LOG_L(L_ERROR, "Spring.SetSmoothMeshFunc: error(%i) = %s",
				error, lua_tostring(L, -1));
		lua_error(L);
	}

	lua_pushnumber(L, smoothMeshAmountChanged);
	return 1;
}


/******************************************************************************
 * TerrainTypes
 * @section terraintypes
******************************************************************************/


/***
 * @function Spring.SetMapSquareTerrainType
 * @param x number
 * @param z number
 * @param newType number
 * @return integer? oldType
 */
int LuaSyncedCtrl::SetMapSquareTerrainType(lua_State* L)
{
	const int hx = int(luaL_checkfloat(L, 1) / SQUARE_SIZE);
	const int hz = int(luaL_checkfloat(L, 2) / SQUARE_SIZE);

	if ((hx < 0) || (hx >= mapDims.mapx) || (hz < 0) || (hz >= mapDims.mapy)) {
		luaL_error(L, "Out of range: x = %d z = %d!", hx, hz);
		return 0;
	}

	const int tx = hx >> 1;
	const int tz = hz >> 1;

	const int ott = readMap->GetTypeMapSynced()[tz * mapDims.hmapx + tx];
	const int ntt = luaL_checkint(L, 3);

	readMap->GetTypeMapSynced()[tz * mapDims.hmapx + tx] = std::max(0, std::min(ntt, (CMapInfo::NUM_TERRAIN_TYPES - 1)));
	pathManager->TerrainChange(hx, hz,  hx + 1, hz + 1,  TERRAINCHANGE_SQUARE_TYPEMAP_INDEX);

	lua_pushnumber(L, ott);
	return 1;
}

/***
 * @function Spring.SetTerrainTypeData
 * @param typeIndex number
 * @param speedTanks number? (Default: nil)
 * @param speedKBOts number? (Default: nil)
 * @param speedHovers number? (Default: nil)
 * @param speedShips number? (Default: nil)
 * @return boolean? true
 */
int LuaSyncedCtrl::SetTerrainTypeData(lua_State* L)
{
	const int args = lua_gettop(L);
	const int tti = luaL_checkint(L, 1);

	if (tti < 0 || tti >= CMapInfo::NUM_TERRAIN_TYPES) {
		lua_pushboolean(L, false);
		return 1;
	}

	CMapInfo::TerrainType* tt = const_cast<CMapInfo::TerrainType*>(&mapInfo->terrainTypes[tti]);
	const CMapInfo::TerrainType ctt = *tt; // copy

	bool ttSpeedModChanged = false;
	bool ttHardnessChanged = false;

	if (args >= 2 && lua_isnumber(L, 2)) ttSpeedModChanged |= (ctt. tankSpeed != (tt-> tankSpeed = lua_tofloat(L, 2)));
	if (args >= 3 && lua_isnumber(L, 3)) ttSpeedModChanged |= (ctt. kbotSpeed != (tt-> kbotSpeed = lua_tofloat(L, 3)));
	if (args >= 4 && lua_isnumber(L, 4)) ttSpeedModChanged |= (ctt.hoverSpeed != (tt->hoverSpeed = lua_tofloat(L, 4)));
	if (args >= 5 && lua_isnumber(L, 5)) ttSpeedModChanged |= (ctt. shipSpeed != (tt-> shipSpeed = lua_tofloat(L, 5)));
	if (args >= 6 && lua_isnumber(L, 6)) ttHardnessChanged |= (ctt.  hardness != (tt->  hardness = lua_tofloat(L, 6)));

	if (args >= 7 && lua_isboolean(L, 7))
		tt->receiveTracks = lua_toboolean(L, 7);
	if (args >= 8 && lua_isstring(L, 8))
		tt->name = lua_tostring(L, 8);

	// hardness changes do not require repathing
	if (ttHardnessChanged)
		mapDamage->TerrainTypeHardnessChanged(tti);
	if (ttSpeedModChanged)
		mapDamage->TerrainTypeSpeedModChanged(tti);

	lua_pushboolean(L, true);
	return 1;
}


/***
 * @function Spring.SetSquareBuildingMask
 * @param x number
 * @param z number
 * @param mask number
 * @return nil
 *
 * See also buildingMask unitdef tag.
 */
int LuaSyncedCtrl::SetSquareBuildingMask(lua_State* L)
{
	const int x = luaL_checkint(L, 1);
	const int z = luaL_checkint(L, 2);
	const int mask = luaL_checkint(L, 3);

	if (mask < 0 || mask > USHRT_MAX) {
		luaL_error(L, "Incorrect value of mask: %s(%d, %d, %d)", __func__, x, z, mask);
		return 0;
	}

	if (!buildingMaskMap.SetTileMask(x, z, (std::uint16_t)mask)) {
		luaL_error(L, "Invalid values supplied: %s(%d, %d, %d)", __func__, x, z, mask);
		return 0;
	}

	return 0; //no error = success
}

/******************************************************************************/
/******************************************************************************/


/***
 * @function Spring.UnitWeaponFire
 * @param unitID integer
 * @param weaponID integer
 * @return nil
 */
int LuaSyncedCtrl::UnitWeaponFire(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const size_t idx = static_cast<size_t>(luaL_checkint(L, 2) - LUA_WEAPON_BASE_INDEX);
	if (idx >= unit->weapons.size())
		return 0;

	unit->weapons[idx]->Fire(false);
	return 0;
}


// NB: not permanent
/***
 * @function Spring.UnitWeaponHoldFire
 * @param unitID integer
 * @param weaponID integer
 * @return nil
 */
int LuaSyncedCtrl::UnitWeaponHoldFire(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const size_t idx = static_cast<size_t>(luaL_checkint(L, 2) - LUA_WEAPON_BASE_INDEX);
	if (idx >= unit->weapons.size())
		return 0;

	unit->weapons[idx]->DropCurrentTarget();
	return 0;
}

/*** Prevent collision checks from working on outdated data
 *
 * There's a rare edge case that requires units to be in specific positions
 * and being shot by specific weapons but which can result in shots ghosting
 * through the unit. This is because the unit's collision volume is stale.
 * The `movement.unitQuadPositionUpdateRate` modrule controls this behaviour
 * and can guarantee 100% correctness if set to 1, but the default value is 3
 * and large-scale games generally don't want to set it so low. This function
 * lets you guarantee success for important weapons regardless of how high
 * the normal update rate is set.
 *
 * @function Spring.ForceUnitCollisionUpdate
 * @param unitID integer
 * @return nil
 */
int LuaSyncedCtrl::ForceUnitCollisionUpdate(lua_State* L)
{
	const auto* const unit = ParseUnit(L, __func__, 1);
	if (!unit)
		return 0;
	if (!unit->moveType)
		return 0;

	unit->moveType->UpdateCollisionMap(true);
	return 0;
}

/***
 *
 * @function Spring.UnitAttach
 * @param transporterID integer
 * @param passengerID integer
 * @param pieceNum number
 * @return nil
 */
int LuaSyncedCtrl::UnitAttach(lua_State* L)
{
	CUnit* transporter = ParseUnit(L, __func__, 1);

	if (transporter == nullptr)
		return 0;

	CUnit* transportee = ParseUnit(L, __func__, 2);

	if (transportee == nullptr)
		return 0;

	if (transporter == transportee)
		return 0;

	int piece = luaL_checkint(L, 3) - 1;
	const auto& pieces = transporter->localModel.pieces;

	if (piece >= (int) pieces.size()) {
		luaL_error(L,  "invalid piece number");
		return 0;
	}

	if (piece >= 0)
		piece = pieces[piece].scriptPieceIndex;

	transporter->AttachUnit(transportee, piece, !transporter->unitDef->IsTransportUnit());
	return 0;
}


/***
 * @function Spring.UnitDetach
 * @param passengerID integer
 * @return nil
 */
int LuaSyncedCtrl::UnitDetach(lua_State* L)
{
	CUnit* transportee = ParseUnit(L, __func__, 1);

	if (transportee == nullptr)
		return 0;

	CUnit* transporter = transportee->GetTransporter();

	if (transporter == nullptr)
		return 0;

	transporter->DetachUnit(transportee);
	return 0;
}


/***
 * @function Spring.UnitDetachFromAir
 * @param passengerID integer
 * @return nil
 */
int LuaSyncedCtrl::UnitDetachFromAir(lua_State* L)
{
	CUnit* transportee = ParseUnit(L, __func__, 1);

	if (transportee == nullptr)
		return 0;

	CUnit* transporter = transportee->GetTransporter();

	if (transporter == nullptr)
		return 0;

	const int args = lua_gettop(L);
	float3 pos;
	if (args >= 4) {
		pos = float3(luaL_checkfloat(L, 2), luaL_checkfloat(L, 3), luaL_checkfloat(L, 4));
	} else {
		pos = transportee->pos;
		pos.y = CGround::GetHeightAboveWater(pos.x, pos.z);
	}

	transporter->DetachUnitFromAir(transportee, pos);

	return 0;
}


/*** Disables collisions between the two units to allow colvol intersection during the approach.
 *
 * @function Spring.SetUnitLoadingTransport
 * @param passengerID integer
 * @param transportID integer
 * @return nil
 */
int LuaSyncedCtrl::SetUnitLoadingTransport(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	if (lua_isnil(L, 2)) {
		unit->loadingTransportId = -1;
		return 0;
	}

	CUnit* transport = ParseUnit(L, __func__, 2);

	if (transport == nullptr)
		return 0;

	unit->loadingTransportId = transport->id;

	return 0;
}

/***
 * @class ProjectileParams
 * @x_helper
 * @field pos xyz
 * @field speed xyz
 * @field spread xyz
 * @field error xyz
 * @field end xyz
 * @field owner integer
 * @field team integer
 * @field ttl number
 * @field gravity number
 * @field tracking number
 * @field maxRange number
 * @field startAlpha number
 * @field endAlpha number
 * @field model string
 * @field cegTag string
 */

/***
 *
 * @function Spring.SpawnProjectile
 * @param weaponDefID integer
 * @param projectileParams ProjectileParams
 * @return integer? projectileID
 */
int LuaSyncedCtrl::SpawnProjectile(lua_State* L)
{
	ProjectileParams params;

	if ((params.weaponDef = weaponDefHandler->GetWeaponDefByID(luaL_checkint(L, 1))) == nullptr)
		return 0;

	if (!ParseProjectileParams(L, params, 2, __func__))
		return 0;

	lua_pushnumber(L, WeaponProjectileFactory::LoadProjectile(params));
	return 1;
}


/*** Silently removes projectiles (no explosion).
 *
 * @function Spring.DeleteProjectile
 * @param projectileID integer
 * @return nil
 */
int LuaSyncedCtrl::DeleteProjectile(lua_State* L)
{
	CProjectile* proj = ParseProjectile(L, __func__, 1);

	if (proj == nullptr)
		return 0;

	proj->Delete();
	return 0;
}

// Slight repetition with SetSingleDynDamagesKey, but it may be ugly to combine them
static int SetSingleDamagesKey(lua_State* L, DamageArray& damages, int index)
{
	const float value = lua_tofloat(L, index + 1);

	if (lua_isnumber(L, index)) {
		const unsigned armType = lua_toint(L, index);
		if (armType < damages.GetNumTypes())
			damages.Set(armType, value);
		return 0;
	}

	// FIXME: missing checks and updates?
	switch (hashString(lua_tostring(L, index))) {
		case hashString("paralyzeDamageTime"): {
			damages.paralyzeDamageTime = std::max((int)value, 0);
		} break;

		case hashString("impulseFactor"): {
			damages.impulseFactor = value;
		} break;
		case hashString("impulseBoost"): {
			damages.impulseBoost = value;
		} break;

		case hashString("craterMult"): {
			damages.craterMult = value;
		} break;
		case hashString("craterBoost"): {
			damages.craterBoost = value;
		} break;
		default: {
		} break;
	}

	return 0;
}


static int SetExplosionParam(lua_State* L, CExplosionParams& params, DamageArray& damages, int index)
{
	switch (hashString(lua_tostring(L, index))) {
		case hashString("damages"): {
			if (lua_istable(L, index + 1)) {
				// {key1 = value1, ...}
				for (lua_pushnil(L); lua_next(L, index) != 0; lua_pop(L, 1)) {
					if ((lua_isnumber(L, LUA_TABLE_KEY_INDEX) || lua_israwstring(L, LUA_TABLE_KEY_INDEX)) && lua_isnumber(L, LUA_TABLE_VALUE_INDEX)) {
						SetSingleDamagesKey(L, damages, LUA_TABLE_KEY_INDEX);
					}
				}
			} else {
				damages.SetDefaultDamage(lua_tofloat(L, index + 1));
			}
		} break;
		case hashString("weaponDef"): {
			params.weaponDef = weaponDefHandler->GetWeaponDefByID(lua_tofloat(L, index + 1));
		} break;
		case hashString("owner"): {
			params.owner = ParseUnit(L, __func__, index + 1);
		} break;

		case hashString("hitUnit"): {
			params.hitObject = ParseUnit(L, __func__, index + 1);
		} break;
		case hashString("hitFeature"): {
			params.hitObject = ParseFeature(L, __func__, index + 1);
		} break;
		case hashString("hitWeapon"): {
			LOG_L(L_ERROR, "SetExplosionParam(\"hitWeapon\") not implemented");
			//params.hitObject = nullptr;
		} break;

		case hashString("craterAreaOfEffect"): {
			params.craterAreaOfEffect = lua_tofloat(L, index + 1);
		} break;
		case hashString("damageAreaOfEffect"): {
			params.damageAreaOfEffect = lua_tofloat(L, index + 1);
		} break;

		case hashString("edgeEffectiveness"): {
			params.edgeEffectiveness = std::min(lua_tofloat(L, index + 1), 1.0f);
		} break;
		case hashString("explosionSpeed"): {
			params.explosionSpeed = lua_tofloat(L, index + 1);
		} break;

		case hashString("gfxMod"): {
			params.gfxMod = lua_tofloat(L, index + 1);
		} break;

		case hashString("projectileID"): {
			params.projectileID = static_cast<unsigned int>(lua_toint(L, index + 1));
		} break;

		case hashString("impactOnly"): {
			params.impactOnly = lua_toboolean(L, index + 1);
		} break;
		case hashString("ignoreOwner"): {
			params.ignoreOwner = lua_toboolean(L, index + 1);
		} break;
		case hashString("damageGround"): {
			params.damageGround = lua_toboolean(L, index + 1);
		} break;

		default: {
			luaL_error(L, "[%s] illegal explosion param \"%s\"", __func__, lua_tostring(L, index));
		} break;
	}

	return 0;
}

/***
 * Parameters for explosion.
 *
 * Please note the explosion defaults to 1 damage regardless of what it's defined in the weaponDef.
 * The weapondefID is only used for visuals and for passing into callins like UnitDamaged.
 *
 * @class ExplosionParams
 * @x_helper
 * @field weaponDef number
 * @field owner number
 * @field hitUnit number
 * @field hitFeature number
 * @field craterAreaOfEffect number
 * @field damageAreaOfEffect number
 * @field edgeEffectiveness number
 * @field explosionSpeed number
 * @field gfxMod number
 * @field impactOnly boolean
 * @field ignoreOwner boolean
 * @field damageGround boolean
 */

/***
 * @function Spring.SpawnExplosion
 * @param posX number? (Default: `0`)
 * @param posY number? (Default: `0`)
 * @param posZ number? (Default: `0`)
 * @param dirX number? (Default: `0`)
 * @param dirY number? (Default: `0`)
 * @param dirZ number? (Default: `0`)
 * @param explosionParams ExplosionParams
 * @return nil
 */
int LuaSyncedCtrl::SpawnExplosion(lua_State* L)
{
	const float3 pos = {luaL_checkfloat(L, 1      ), luaL_checkfloat(L, 2      ), luaL_checkfloat(L, 3      )};
	const float3 dir = {luaL_optfloat  (L, 4, 0.0f), luaL_optfloat  (L, 5, 0.0f), luaL_optfloat  (L, 6, 0.0f)};

	if (lua_istable(L, 7)) {
		DamageArray damages(1.0f);
		CExplosionParams params = {
			.pos                  = pos,
			.dir                  = dir,
			.damages              = damages,
			.weaponDef            = nullptr,
			.owner                = nullptr,
			.hitObject            = ExplosionHitObject(),
			.craterAreaOfEffect   = 0.0f,
			.damageAreaOfEffect   = 0.0f,
			.edgeEffectiveness    = 0.0f,
			.explosionSpeed       = 0.0f,
			.gfxMod               = 0.0f,
			.maxGroundDeformation = 0.0f,
			.impactOnly           = false,
			.ignoreOwner          = false,
			.damageGround         = false,
			.projectileID         = static_cast<uint32_t>(-1)
		};

		for (lua_pushnil(L); lua_next(L, 7) != 0; lua_pop(L, 1)) {
			SetExplosionParam(L, params, damages, -2);
		}

		helper->Explosion(params);
	} else {
		DamageArray damages(luaL_optfloat(L, 7, 1.0f));
		CExplosionParams params = {pos, dir, damages};

		// parse remaining arguments in order of expected usage frequency
		params.weaponDef  = weaponDefHandler->GetWeaponDefByID(luaL_optint(L, 16, -1));
		params.owner      = ParseUnit   (L, __func__, 18);
		params.hitObject  = ParseUnit   (L, __func__, 19);
		params.hitObject  = ParseFeature(L, __func__, 20);
		//params.hitWeapon = nullptr; // not implemented

		params.craterAreaOfEffect = luaL_optfloat(L,  8, 0.0f);
		params.damageAreaOfEffect = luaL_optfloat(L,  9, 0.0f);
		params.edgeEffectiveness  = std::min(luaL_optfloat(L, 10, 0.0f), 1.0f);
		params.explosionSpeed     = luaL_optfloat(L, 11, 0.0f);
		params.gfxMod             = luaL_optfloat(L, 12, 0.0f);
		params.maxGroundDeformation = 0.0f;

		params.impactOnly   = luaL_optboolean(L, 13, false);
		params.ignoreOwner  = luaL_optboolean(L, 14, false);
		params.damageGround = luaL_optboolean(L, 15, false);

		params.projectileID = static_cast<unsigned int>(luaL_optint(L, 17, -1));

		helper->Explosion(params);
	}

	return 0;
}

/***
 * @function Spring.SpawnCEG
 * @param cegname string
 * @param posX number? (Default: `0`)
 * @param posY number? (Default: `0`)
 * @param posZ number? (Default: `0`)
 * @param dirX number? (Default: `0`)
 * @param dirY number? (Default: `0`)
 * @param dirZ number? (Default: `0`)
 * @param radius number? (Default: `0`)
 * @param damage number? (Default: `0`)
 * @return boolean? success
 * @return integer cegID
 */
int LuaSyncedCtrl::SpawnCEG(lua_State* L)
{
	const float3 pos = {luaL_optfloat(L, 2, 0.0f), luaL_optfloat(L, 3, 0.0f), luaL_optfloat(L, 4, 0.0f)};
	const float3 dir = {luaL_optfloat(L, 5, 0.0f), luaL_optfloat(L, 6, 0.0f), luaL_optfloat(L, 7, 0.0f)};

	const float radius = luaL_optfloat(L,  8, 0.0f);
	const float damage = luaL_optfloat(L,  9, 0.0f);
	const float dmgMod = luaL_optfloat(L, 10, 1.0f);

	// args from Lua are assumed not to include the prefix
	// (Spawn*C*EG implies only custom generators can fire)
	const unsigned int cegID = lua_isstring(L, 1)? explGenHandler.LoadCustomGeneratorID(lua_tostring(L, 1)): luaL_checkint(L, 1);

	lua_pushboolean(L, explGenHandler.GenExplosion(
		cegID,
		pos,
		dir,
		damage,
		radius,
		dmgMod,
		nullptr,
		ExplosionHitObject()
	));
	lua_pushnumber(L, cegID);
	return 2;
}

/*** Equal to the UnitScript versions of EmitSFX, but takes position and direction arguments (in either unit- or piece-space) instead of a piece index.
 *
 * @function Spring.SpawnSFX
 * @param unitID integer? (Default: `0`)
 * @param sfxID integer? (Default: `0`)
 * @param posX number? (Default: `0`)
 * @param posY number? (Default: `0`)
 * @param posZ number? (Default: `0`)
 * @param dirX number? (Default: `0`)
 * @param dirY number? (Default: `0`)
 * @param dirZ number? (Default: `0`)
 * @param radius number? (Default: `0`)
 * @param damage number? (Default: `0`)
 * @param absolute boolean?
 * @return boolean? success
 */
int LuaSyncedCtrl::SpawnSFX(lua_State* L)
{
	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const float3 pos = {luaL_checkfloat(L, 3), luaL_checkfloat(L, 4), luaL_checkfloat(L, 5)};
	const float3 dir = {luaL_checkfloat(L, 6), luaL_checkfloat(L, 7), luaL_checkfloat(L, 8)};

	if (luaL_optboolean(L, 9, true)) {
		lua_pushboolean(L, unit->script->EmitAbsSFX(luaL_checkint(L, 2), pos, dir));
	} else {
		lua_pushboolean(L, unit->script->EmitRelSFX(luaL_checkint(L, 2), pos, dir));
	}

	return 1;
}

/******************************************************************************
 * Other
 * @section other
******************************************************************************/

/***
 * @function Spring.SetNoPause
 * @param noPause boolean
 * @return nil
 */
int LuaSyncedCtrl::SetNoPause(lua_State* L)
{
	if (!FullCtrl(L))
		return 0;

	// Important: only works in server mode, has no effect in client mode
	if (gameServer != nullptr)
		gameServer->SetGamePausable(!luaL_checkboolean(L, 1));

	return 0;
}


/*** Defines how often `Callins.UnitExperience` will be called.
 *
 * @function Spring.SetExperienceGrade
 * @param expGrade number
 * @param ExpPowerScale number?
 * @param ExpHealthScale number?
 * @param ExpReloadScale number?
 * @return nil
 */
int LuaSyncedCtrl::SetExperienceGrade(lua_State* L)
{
	if (!FullCtrl(L))
		return 0;

	globalUnitParams.expGrade = luaL_checkfloat(L, 1);

	// NOTE: for testing, should be using modrules.tdf
	if (gs->cheatEnabled) {
		if (lua_isnumber(L, 2))
			globalUnitParams.expPowerScale = lua_tofloat(L, 2);

		if (lua_isnumber(L, 3))
			globalUnitParams.expHealthScale = lua_tofloat(L, 3);

		if (lua_isnumber(L, 4))
			globalUnitParams.expReloadScale = lua_tofloat(L, 4);
	}
	return 0;
}


/***
 *
 * @function Spring.SetRadarErrorParams
 * @param allyTeamID integer
 * @param allyteamErrorSize number
 * @param baseErrorSize number?
 * @param baseErrorMult number?
 * @return nil
 */
int LuaSyncedCtrl::SetRadarErrorParams(lua_State* L)
{
	const int allyTeamID = lua_tonumber(L, 1);

	if (!teamHandler.IsValidAllyTeam(allyTeamID))
		return 0;

	losHandler->SetAllyTeamRadarErrorSize(allyTeamID, luaL_checknumber(L, 2));
	losHandler->SetBaseRadarErrorSize(luaL_optnumber(L, 3, losHandler->GetBaseRadarErrorSize()));
	losHandler->SetBaseRadarErrorMult(luaL_optnumber(L, 4, losHandler->GetBaseRadarErrorMult()));
	return 0;
}


/******************************************************************************/
/******************************************************************************/

static bool ParseNamedInt(lua_State* L, const string& key,
                          const string& name, int& value)
{
	if (key != name) {
		return false;
	}
	if (lua_isnumber(L, -1)) {
		value = lua_toint(L, -1);
	} else {
		luaL_error(L, "bad %s argument", name.c_str());
	}
	return true;
}


static bool ParseNamedBool(lua_State* L, const string& key,
                           const string& name, bool& value)
{
	if (key != name) {
		return false;
	}
	if (lua_isboolean(L, -1)) {
		value = (int)lua_toboolean(L, -1);
	} else {
		luaL_error(L, "bad %s argument", name.c_str());
	}
	return true;
}


static bool ParseNamedString(lua_State* L, const string& key,
                             const string& name, string& value)
{
	if (key != name) {
		return false;
	}
	if (lua_isstring(L, -1)) {
		value = lua_tostring(L, -1);
	} else {
		luaL_error(L, "bad %s argument", name.c_str());
	}
	return true;
}


static int ParseStringVector(lua_State* L, int index, vector<string>& strvec)
{
	strvec.clear();
	int i = 1;
	while (true) {
		lua_rawgeti(L, index, i);
		if (lua_isstring(L, -1)) {
			strvec.emplace_back(lua_tostring(L, -1));
			lua_pop(L, 1);
			i++;
		} else {
			lua_pop(L, 1);
			return (i - 1);
		}
	}
}


/******************************************************************************
 * Command Descriptions
 * @section commanddescriptions
 * Doesn't work in unsynced code!
******************************************************************************/

static bool ParseCommandDescription(lua_State* L, int table,
                                    SCommandDescription& cd)
{
	if (!lua_istable(L, table)) {
		luaL_error(L, "Can not parse CommandDescription");
		return false;
	}

	for (lua_pushnil(L); lua_next(L, table) != 0; lua_pop(L, 1)) {
		if (!lua_israwstring(L, LUA_TABLE_KEY_INDEX))
			continue;

		const string key = lua_tostring(L, -2);

		if (ParseNamedInt(L,    key, "id",          cd.id)         ||
		    ParseNamedInt(L,    key, "type",        cd.type)       ||
		    ParseNamedString(L, key, "name",        cd.name)       ||
		    ParseNamedString(L, key, "action",      cd.action)     ||
		    ParseNamedString(L, key, "tooltip",     cd.tooltip)    ||
		    ParseNamedString(L, key, "texture",     cd.iconname)   ||
		    ParseNamedString(L, key, "cursor",      cd.mouseicon)  ||
		    ParseNamedBool(L,   key, "queueing",    cd.queueing)   ||
		    ParseNamedBool(L,   key, "hidden",      cd.hidden)     ||
		    ParseNamedBool(L,   key, "disabled",    cd.disabled)   ||
		    ParseNamedBool(L,   key, "showUnique",  cd.showUnique) ||
		    ParseNamedBool(L,   key, "onlyTexture", cd.onlyTexture)) {
			continue; // successfully parsed a parameter
		}

		if ((key != "params") || !lua_istable(L, -1))
			luaL_error(L, "Unknown cmdDesc parameter %s", key.c_str());

		// collect the parameters
		const int paramTable = lua_gettop(L);
		ParseStringVector(L, paramTable, cd.params);
	}

	return true;
}

/***
 * @function Spring.EditUnitCmdDesc
 * @param unitID integer
 * @param cmdDescID integer
 * @param cmdArray CommandDescription
 */
int LuaSyncedCtrl::EditUnitCmdDesc(lua_State* L)
{
	if (!FullCtrl(L))
		return 0;

	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const auto& cmdDescs = unit->commandAI->GetPossibleCommands();

	const unsigned int cmdDescIdx = luaL_checkint(L, 2) - 1;

	if (cmdDescIdx >= cmdDescs.size())
		return 0;

	// note: must be a copy
	SCommandDescription cmdDesc = *cmdDescs[cmdDescIdx];

	ParseCommandDescription(L, 3, cmdDesc);
	unit->commandAI->UpdateCommandDescription(cmdDescIdx, std::move(cmdDesc));
	return 0;
}


/***
 * Insert a command description at a specific index.
 * 
 * @function Spring.InsertUnitCmdDesc
 * @param unitID integer
 * @param index integer
 * @param cmdDesc CommandDescription
 */
/***
 * Insert a command description at the last position.
 * 
 * @function Spring.InsertUnitCmdDesc
 * @param unitID integer
 * @param cmdDesc CommandDescription
 */
int LuaSyncedCtrl::InsertUnitCmdDesc(lua_State* L)
{
	if (!FullCtrl(L))
		return 0;

	const int args = lua_gettop(L); // number of arguments

	if ((args == 2) && !lua_istable(L, 2))
		luaL_error(L, "Incorrect arguments to InsertUnitCmdDesc/2");

	if ((args >= 3) && (!lua_isnumber(L, 2) || !lua_istable(L, 3)))
		luaL_error(L, "Incorrect arguments to InsertUnitCmdDesc/3");

	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	const int tableIdx = 2 + int(args >= 3);

	// insert behind last by default
	unsigned int cmdDescIdx = -1u;

	if (args >= 3)
		cmdDescIdx = lua_toint(L, 2) - 1;

	SCommandDescription cd;

	ParseCommandDescription(L, tableIdx, cd);
	unit->commandAI->InsertCommandDescription(cmdDescIdx, std::move(cd));
	return 0;
}


/***
 * @function Spring.RemoveUnitCmdDesc
 * @param unitID integer
 * @param cmdDescID integer?
 */
int LuaSyncedCtrl::RemoveUnitCmdDesc(lua_State* L)
{
	if (!FullCtrl(L))
		return 0;

	CUnit* unit = ParseUnit(L, __func__, 1);

	if (unit == nullptr)
		return 0;

	// remove last by default
	unsigned int cmdDescIdx = unit->commandAI->possibleCommands.size() - 1;

	if (lua_isnumber(L, 2))
		cmdDescIdx = lua_toint(L, 2) - 1;

	unit->commandAI->RemoveCommandDescription(cmdDescIdx);
	return 0;
}
