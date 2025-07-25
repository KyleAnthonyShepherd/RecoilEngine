/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef UNIT_H
#define UNIT_H

#include <vector>

#include "Sim/Objects/SolidObject.h"
#include "Sim/Misc/Resource.h"
#include "Sim/Weapons/WeaponTarget.h"
#include "System/Matrix44f.h"
#include "System/type2.h"

// Added only to calculate the size of unit memory buffers.
// change as appropriate if the largest type changes.

// for amtMemBuffer
#include "Sim/MoveTypes/GroundMoveType.h"

// for smtMemBuffer
#include "Sim/MoveTypes/ScriptMoveType.h"

// for caiMemBuffer
#include "Sim/Units/CommandAI/BuilderCAI.h"

// for usMemBuffer
#include "Sim/Units/Scripts/LuaUnitScript.h"


class CPlayer;
class CCommandAI;
class CGroup;
class CMissileProjectile;
class AMoveType;
class CWeapon;
class CUnitScript;
class DamageArray;
class DynDamageArray;
struct SolidObjectDef;
struct UnitDef;
struct UnitLoadParams;
struct SLosInstance;

namespace icon {
	class CIconData;
}

// LOS state bits
static constexpr uint8_t LOS_INLOS     = (1 << 0);  // the unit is currently in the los of the allyteam
static constexpr uint8_t LOS_INRADAR   = (1 << 1);  // the unit is currently in radar from the allyteam
static constexpr uint8_t LOS_PREVLOS   = (1 << 2);  // the unit has previously been in los from the allyteam
static constexpr uint8_t LOS_CONTRADAR = (1 << 3);  // the unit has continuously been in radar since it was last inlos by the allyteam

static constexpr uint8_t LOS_MASK_SHIFT = 4;

// LOS mask bits  (masked bits are not automatically updated)
static constexpr uint8_t LOS_INLOS_MASK     = (LOS_INLOS     << LOS_MASK_SHIFT);  // do not update LOS_INLOS
static constexpr uint8_t LOS_INRADAR_MASK   = (LOS_INRADAR   << LOS_MASK_SHIFT);  // do not update LOS_INRADAR
static constexpr uint8_t LOS_PREVLOS_MASK   = (LOS_PREVLOS   << LOS_MASK_SHIFT);  // do not update LOS_PREVLOS
static constexpr uint8_t LOS_CONTRADAR_MASK = (LOS_CONTRADAR << LOS_MASK_SHIFT);  // do not update LOS_CONTRADAR

static constexpr uint8_t LOS_ALL_BITS = \
	(LOS_INLOS      | LOS_INRADAR      | LOS_PREVLOS      | LOS_CONTRADAR);
static constexpr uint8_t LOS_ALL_MASK_BITS = \
	(LOS_INLOS_MASK | LOS_INRADAR_MASK | LOS_PREVLOS_MASK | LOS_CONTRADAR_MASK);


class CUnit : public CSolidObject
{
public:
	CR_DECLARE(CUnit)

	CUnit();
	virtual ~CUnit();

	static void InitStatic();

	void SanityCheck() const;
	void UpdatePrevFrameTransform() override;

	virtual void PreInit(const UnitLoadParams& params);
	virtual void PostInit(const CUnit* builder);

	virtual void Update();
	virtual void SlowUpdate();

	const SolidObjectDef* GetDef() const { return ((const SolidObjectDef*) unitDef); }

	virtual void DoDamage(const DamageArray& damages, const float3& impulse, CUnit* attacker, int weaponDefID, int projectileID);
	virtual void DoWaterDamage();
	virtual void FinishedBuilding(bool postInit);

	void ApplyDamage(CUnit* attacker, const DamageArray& damages, float& baseDamage, float& experienceMod);
	void ApplyImpulse(const float3& impulse);

	bool AttackUnit(CUnit* unit, bool isUserTarget, bool wantManualFire, bool fpsMode = false);
	bool AttackGround(const float3& pos, bool isUserTarget, bool wantManualFire, bool fpsMode = false);
	void DropCurrentAttackTarget();

	int GetBlockingMapID() const { return id; }

	void ChangeLos(int losRad, int airRad);

	void TurnIntoNanoframe();

	// negative amount=reclaim, return= true -> build power was successfully applied
	bool AddBuildPower(CUnit* builder, float amount);

	virtual void Activate();
	virtual void Deactivate();

	void ForcedMove(const float3& newPos);

	void DeleteScript();
	void EnableScriptMoveType();
	void DisableScriptMoveType();

	CMatrix44f GetTransformMatrix(bool synced = false, bool fullread = false) const override final;

	void DependentDied(CObject* o);

	bool AllowedReclaim(CUnit* builder) const;

	bool UseMetal(float metal);
	void AddMetal(float metal, bool useIncomeMultiplier = true);
	bool UseEnergy(float energy);
	void AddEnergy(float energy, bool useIncomeMultiplier = true);
	bool AddHarvestedMetal(float metal);

	void SetStorage(const SResourcePack& newstorage);
	bool HaveResources(const SResourcePack& res) const;
	bool UseResources(const SResourcePack& res);
	void AddResources(const SResourcePack& res, bool useIncomeMultiplier = true);
	bool IssueResourceOrder(SResourceOrder* order);

	// push the new wind to the script
	void UpdateWind(float x, float z, float strength);

	void UpdateTransportees();
	void ReleaseTransportees(CUnit* attacker, bool selfDestruct, bool reclaimed);
	void TransporteeKilled(const CObject* o);

	void AddExperience(float exp);

	void SetMass(float newMass);

	void DoSeismicPing(float pingSize);

	void CalculateTerrainType();
	void UpdateTerrainType();
	void UpdatePhysicalState(float eps);

	float3 GetErrorVector(int allyteam) const;
	float3 GetErrorPos(int allyteam, bool aiming = false) const { return (aiming? aimPos: midPos) + GetErrorVector(allyteam); }
	float3 GetObjDrawErrorPos(int allyteam) const { return (GetObjDrawMidPos() + GetErrorVector(allyteam)); }

	float3 GetLuaErrorVector(int allyteam, bool fullRead) const { return (fullRead? ZeroVector: GetErrorVector(allyteam)); }
	float3 GetLuaErrorPos(int allyteam, bool fullRead) const { return (midPos + GetLuaErrorVector(allyteam, fullRead)); }

	void UpdatePosErrorParams(bool updateError, bool updateDelta);

	bool UsingScriptMoveType() const { return (prevMoveType != nullptr); }
	bool UnderFirstPersonControl() const { return (fpsControlPlayer != nullptr); }

	bool FloatOnWater() const;

	bool IsNeutral() const { return neutral; }
	bool IsCloaked() const { return isCloaked; }
	bool IsStunned() const { return stunned; }
	bool IsIdle() const;

	bool HaveTarget() const { return (curTarget.type != Target_None); }
	bool CanUpdateWeapons() const {
		return (forceUseWeapons || (allowUseWeapons && !onTempHoldFire && !isDead && !beingBuilt && !IsStunned()));
	}

	void SetNeutral(bool b);
	void SetStunned(bool stun);

	bool GetPosErrorBit(int at) const {
		return (posErrorMask[at / 32] & (1 << (at % 32)));
	}
	void SetPosErrorBit(int at, int bit) {
		posErrorMask[at / 32] |=  ((1 << (at % 32)) * (bit == 1));
		posErrorMask[at / 32] &= ~((1 << (at % 32)) * (bit == 0));
	}

	bool IsInLosForAllyTeam(int allyTeam) const { return ((losStatus[allyTeam] & LOS_INLOS) != 0); }

	void SetLosStatus(int allyTeam, unsigned short newStatus);
	unsigned short CalcLosStatus(int allyTeam);
	void UpdateLosStatus(int allyTeam);

	void SetLeavesGhost(bool newLeavesGhost, bool leaveDeadGhost);

	void UpdateWeapons();
	void UpdateWeaponVectors();

	void SlowUpdateWeapons();
	void SlowUpdateKamikaze(bool scanForTargets);
	void SlowUpdateCloak(bool stunCheck);

	bool ScriptCloak();
	bool ScriptDecloak(const CSolidObject* object, const CWeapon* weapon);
	bool GetNewCloakState(bool checkStun);

	enum ChangeType {
		ChangeGiven,
		ChangeCaptured
	};
	virtual bool ChangeTeam(int team, ChangeType type);
	virtual void StopAttackingAllyTeam(int ally);

	//Transporter stuff
	CR_DECLARE_SUB(TransportedUnit)

	struct TransportedUnit {
		CR_DECLARE_STRUCT(TransportedUnit)
		CUnit* unit;
		int piece;
	};

	bool SetSoloBuilder(CUnit* builder, const UnitDef* buildeeDef);
	void SetLastAttacker(CUnit* attacker);

	void SetTransporter(CUnit* trans) { transporter = trans; }
	CUnit* GetTransporter() const { return transporter; }

	bool AttachUnit(CUnit* unit, int piece, bool force = false);
	bool CanTransport(const CUnit* unit) const;

	bool DetachUnit(CUnit* unit);
	bool DetachUnitCore(CUnit* unit);
	bool DetachUnitFromAir(CUnit* unit, const float3& pos); // orders <unit> to move to <pos> after detach

	bool CanLoadUnloadAtPos(const float3& wantedPos, const CUnit* unit, float* wantedHeightPtr = nullptr) const;
	float GetTransporteeWantedHeight(const float3& wantedPos, const CUnit* unit, bool* ok = nullptr) const;
	short GetTransporteeWantedHeading(const CUnit* unit) const;

public:
	void KilledScriptFinished(int wreckLevel) { deathScriptFinished = true; delayedWreckLevel = wreckLevel; }
	void ForcedKillUnit(CUnit* attacker, bool selfDestruct, bool reclaimed, int weaponDefID = 0);
	virtual void KillUnit(CUnit* attacker, bool selfDestruct, bool reclaimed, int weaponDefID = 0);
	virtual void IncomingMissile(CMissileProjectile* missile);
	CFeature* CreateWreck(int wreckLevel, int smokeTime);

	void TempHoldFire(int cmdID);
	void SetHoldFire(bool b) { onTempHoldFire = b; }

	// start this unit in free fall from parent unit
	void Drop(const float3& parentPos, const float3& parentDir, CUnit* parent);
	void PostLoad();
protected:
	void ChangeTeamReset();
	void UpdateResources();
	float GetFlankingDamageBonus(const float3& attackDir);

public: // unsynced methods
	bool SetGroup(CGroup* newGroup, bool fromFactory = false, bool autoSelect = true);

	const CGroup* GetGroup() const;
	      CGroup* GetGroup();

	bool GetIsIcon() const { return HasDrawFlag(DrawFlags::SO_DRICON_FLAG); }
	void SetIsIcon(bool b) {
		if (b)
			AddDrawFlag(DrawFlags::SO_DRICON_FLAG);
		else
			DelDrawFlag(DrawFlags::SO_DRICON_FLAG);
	}
public:
	static float ExperienceScale(float limExperience, float experienceWeight) {
		// limExperience ranges from 0.0 to 0.9999...
		return std::max(0.0f, 1.0f - (limExperience * experienceWeight));
	}

public:
	const UnitDef* unitDef = nullptr;

	// Our shield weapon, NULL if we have none
	CWeapon* shieldWeapon = nullptr;
	// Our weapon with stockpiled ammo, NULL if we have none
	CWeapon* stockpileWeapon = nullptr;

	const DynDamageArray* selfdExpDamages = nullptr;
	const DynDamageArray* deathExpDamages = nullptr;

	CUnit* soloBuilder = nullptr;
	CUnit* lastAttacker = nullptr;
	// transport that the unit is currently in
	CUnit* transporter = nullptr;

	// player who is currently FPS'ing this unit
	CPlayer* fpsControlPlayer = nullptr;

	AMoveType* moveType = nullptr;
	AMoveType* prevMoveType = nullptr;

	CCommandAI* commandAI = nullptr;
	CUnitScript* script = nullptr;

	// current attackee
	SWeaponTarget curTarget;


	// sufficient for the largest UnitScript (CLuaUnitScript)
	uint8_t usMemBuffer[sizeof(CLuaUnitScript)];
	// sufficient for the largest AMoveType (CGroundMoveType)
	// need two buffers since ScriptMoveType might be enabled
	uint8_t amtMemBuffer[sizeof(CGroundMoveType)];
	uint8_t smtMemBuffer[sizeof(CScriptMoveType)];
	// sufficient for the largest CommandAI type (CBuilderCAI)
	// knowing the exact CAI object size here is not required;
	// static asserts will catch any overflow
	uint8_t caiMemBuffer[sizeof(CBuilderCAI)];


	std::vector<CWeapon*> weapons;

	// which squares the unit can currently observe, per los-type
	std::array<SLosInstance*, /*ILosType::LOS_TYPE_COUNT*/ 7> los{{nullptr}};

	// indicates the los/radar status each allyteam has on this unit
	// should technically be MAX_ALLYTEAMS, but #allyteams <= #teams
	std::array<uint8_t, /*MAX_TEAMS*/ 255> losStatus{{0}};
	// bit-mask indicating which allyteams see this unit with positional error
	std::array<uint32_t, /*MAX_TEAMS/32*/ 8> posErrorMask{{1}};

	// quads the unit is part of
	std::vector<int> quads;

	std::vector<TransportedUnit> transportedUnits;
	// incoming projectiles for which flares can cause retargeting
	std::array<CMissileProjectile*, /*MAX_INCOMING_MISSILES*/ 8> incomingMissiles{{nullptr}};

	float3 lastMuzzleFlameDir = UpVector;
	// units take less damage when attacked from this dir (encourage flanking fire)
	float3 flankingBonusDir = RgtVector;

	// used for radar inaccuracy etc
	float3 posErrorVector;
	float3 posErrorDelta;


	int featureDefID = -1; // FeatureDef id of the wreck we spawn on death

	// indicate the relative power of the unit, used for experience calculations etc
	float power = 100.0f;

	// 0.0-1.0
	float buildProgress = 0.0f;
	// if (health - this) is negative the unit is stunned
	float paralyzeDamage = 0.0f;
	// how close this unit is to being captured
	float captureProgress = 0.0f;
	float experience = 0.0f;
	// approaches 1 as experience approaches infinity
	float limExperience = 0.0f;


	// how much terraforming is left to do
	float terraformLeft = 0.0f;
	// How much reapir power has been added to this recently
	float repairAmount = 0.0f;

	// last frame unit was attacked by other unit
	int lastAttackFrame = -200;
	// last time this unit fired a weapon
	int lastFireWeapon = 0;

	// if we arent built on for a while start decaying
	int lastNanoAdd = 0;
	int lastFlareDrop = 0;

	// id of transport that the unit is about to be {un}loaded by
	int loadingTransportId = -1;
	int unloadingTransportId = -1;
	bool requestRemoveUnloadTransportId = false;

	int transportCapacityUsed = 0;
	float transportMassUsed = 0.0f;


	// the wreck level the unit will eventually create when it has died
	int delayedWreckLevel = -1;

	// how long the unit has been inactive
	unsigned int restTime = 0;

	float reloadSpeed = 1.0f;
	float maxRange = 0.0f;

	// used to determine muzzle flare size
	float lastMuzzleFlameSize = 0.0f;

	int armorType = 0;
	// what categories the unit is part of (bitfield)
	unsigned int category = 0;

	int mapSquare = -1;

	// set los to this when finished building
	int realLosRadius = 0;
	int realAirLosRadius = 0;

	int losRadius = 0;
	int airLosRadius = 0;

	int radarRadius = 0;
	int sonarRadius = 0;
	int jammerRadius = 0;
	int sonarJamRadius = 0;
	int seismicRadius = 0;

	float seismicSignature = 0.0f;
	float decloakDistance = 0.0f;


	// only when the unit is active
	SResourcePack resourcesCondUse;
	SResourcePack resourcesCondMake;

	// always applied
	SResourcePack resourcesUncondUse;
	SResourcePack resourcesUncondMake;

	// costs per UNIT_SLOWUPDATE_RATE frames
	SResourcePack resourcesUse;

	// incomes per UNIT_SLOWUPDATE_RATE frames
	SResourcePack resourcesMake;

	// variables used for calculating unit resource usage
	SResourcePack resourcesUseI;
	SResourcePack resourcesMakeI;
	SResourcePack resourcesUseOld;
	SResourcePack resourcesMakeOld;

	// the amount of storage the unit contributes to the team
	SResourcePack storage;

	// per unit storage (gets filled on reclaim and needs then to be unloaded at some storage building -> 2nd part is lua's job)
	SResourcePack harvestStorage;
	SResourcePack harvested;

	SResourcePack cost = {100.0f, 0.0f};

	// how much metal the unit currently extracts from the ground
	float metalExtract = 0.0f;

	float buildTime = 100.0f;

	// decaying value of how much damage the unit has taken recently (for severity of death)
	float recentDamage = 0.0f;

	int fireState = 0;
	int moveState = 0;

	// for units being dropped from transports (parachute drops)
	float fallSpeed = 0.2f;

	/**
	 * 0 = no flanking bonus
	 * 1 = global coords, mobile
	 * 2 = unit coords, mobile
	 * 3 = unit coords, locked
	 */
	int flankingBonusMode = 0;

	// how much the lowest damage direction of the flanking bonus can turn upon an attack (zeroed when attacked, slowly increases)
	float  flankingBonusMobility = 10.0f;
	// how much ability of the flanking bonus direction to move builds up each frame
	float  flankingBonusMobilityAdd = 0.01f;
	// average factor to multiply damage by
	float  flankingBonusAvgDamage = 1.4f;
	// (max damage - min damage) / 2
	float  flankingBonusDifDamage = 0.5f;

	float armoredMultiple = 1.0f;
	// multiply all damage the unit take with this
	float curArmorMultiple = 1.0f;

	int nextPosErrorUpdate = 1;

	int lastTerrainType = -1;
	// Used for calling setSFXoccupy which TA scripts want
	int curTerrainType = 0;

	int selfDCountdown = 0;

	// the damage value passed to CEGs spawned by this unit's script
	int cegDamage = 0;


	// if the unit is in it's 'on'-state
	bool activated = false;
	// prevent damage from hitting an already dead unit (causing multi wreck etc)
	bool isDead = false;

	bool armoredState = false;

	bool stealth = false;
	bool sonarStealth = false;

	// used by constructing units
	bool inBuildStance = false;
	// tells weapons that support it to try to use a high trajectory
	bool useHighTrajectory = false;
	// used by landed gunships to block weapon Update()'s, also by builders to
	// prevent weapon SlowUpdate()'s and Attack{Unit,Ground}()'s during certain
	// commands
	bool onTempHoldFire = false;

	// Lua overrides for CanUpdateWeapons
	bool forceUseWeapons = false;
	bool allowUseWeapons =  true;

	// signals if script has finished executing Killed and the unit can be deleted
	bool deathScriptFinished = false;

	// if true, unit will not be automatically fired upon unless attacker's fireState is set to > FIREATWILL
	bool neutral = false;
	// if unit is currently incompletely constructed (implies buildProgress < 1)
	bool beingBuilt = true;
	// if the updir is straight up or align to the ground vector
	bool upright = true;
	// whether the ground below this unit has been terraformed
	bool groundLevelled = true;

	// true if the unit is currently cloaked (has enough energy etc)
	bool isCloaked = false;
	// true if the unit currently wants to be cloaked
	bool wantCloak = false;
	// true if the unit leaves static ghosts
	bool leavesGhost = false;

	// unsynced vars
	bool noMinimap = false;
	bool leaveTracks = false;

	bool isSelected = false;
	// if true, unit can not be added to groups by a player (UNSYNCED)
	bool noGroup = false;

	float iconRadius = 0.0f;

	icon::CIconData* myIcon = nullptr;

	bool drawIcon = true;
private:
	// if we are stunned by a weapon or for other reason, access via IsStunned/SetStunned(bool)
	bool stunned = false;
};

struct GlobalUnitParams {
	CR_DECLARE_STRUCT(GlobalUnitParams)

	float empDeclineRate = 0.0f;
	float expMultiplier  = 0.0f;
	float expPowerScale  = 0.0f;
	float expHealthScale = 0.0f;
	float expReloadScale = 0.0f;
	float expGrade       = 0.0f;
};

extern GlobalUnitParams globalUnitParams;

#endif // UNIT_H
