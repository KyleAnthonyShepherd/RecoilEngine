function gadget:GetInfo()
	return {
		name    = "BuildPower AI Example",
		desc    = "Example gadget: auto-targets for units with BuildPower weapons",
		author  = "Example",
		date    = "2026",
		license = "GNU GPL, v2 or later",
		layer   = 0,
		enabled = true,
	}
end

if not gadgetHandler:IsSyncedCode() then
	return
end

----------------------------------------------------------------
-- Speedups / constants
----------------------------------------------------------------

local spGetUnitHealth       = Spring.GetUnitHealth
local spGetUnitAllyTeam     = Spring.GetUnitAllyTeam
local spGetUnitIsDead       = Spring.GetUnitIsDead
local spGetUnitDefID        = Spring.GetUnitDefID
local spGetUnitsInSphere    = Spring.GetUnitsInSphere
local spGetFeaturesInSphere = Spring.GetFeaturesInSphere
local spGetUnitPosition     = Spring.GetUnitPosition
local spGetFeatureHealth    = Spring.GetFeatureHealth
local spGetFeatureDefID     = Spring.GetFeatureDefID
local spSetUnitBuildPowerTarget = Spring.SetUnitBuildPowerTarget

local BP_NONE      = BP_NONE
local BP_REPAIR    = BP_REPAIR
local BP_BUILD     = BP_BUILD
local BP_RECLAIM   = BP_RECLAIM
local BP_CAPTURE   = BP_CAPTURE
local BP_RESURRECT = BP_RESURRECT

----------------------------------------------------------------
-- State: tracked units with at least one BuildPower weapon
----------------------------------------------------------------

-- bpUnits[unitID] = { weapons = { [weaponNum] = weaponDefID, ... } }
local bpUnits = {}

----------------------------------------------------------------
-- Cache which weapon slots are BuildPower on each UnitDef
----------------------------------------------------------------

-- unitDefBPWeapons[unitDefID] = { weaponNum1, weaponNum2, ... } (1-indexed)
local unitDefBPWeapons = {}

for udID, ud in pairs(UnitDefs) do
	local slots = {}
	for i, wp in ipairs(ud.weapons) do
		local wd = WeaponDefs[wp.weaponDef]
		if wd and wd.type == "BuildPower" then
			slots[#slots + 1] = i -- 1-indexed weapon slot
		end
	end
	if #slots > 0 then
		unitDefBPWeapons[udID] = slots
	end
end

----------------------------------------------------------------
-- Unit tracking
----------------------------------------------------------------

function gadget:UnitFinished(unitID, unitDefID, unitTeam)
	local slots = unitDefBPWeapons[unitDefID]
	if not slots then
		return
	end

	local weapons = {}
	for _, wNum in ipairs(slots) do
		local wd = UnitDefs[unitDefID].weapons[wNum]
		weapons[wNum] = wd.weaponDef -- store the WeaponDef ID
	end

	bpUnits[unitID] = { weapons = weapons }
end

function gadget:UnitDestroyed(unitID)
	bpUnits[unitID] = nil
end

----------------------------------------------------------------
-- Per-weapon targeting decision
----------------------------------------------------------------

local function FindTargetForWeapon(unitID, weaponNum, wdID, ux, uy, uz, allyTeam, range)
	local wd = WeaponDefs[wdID]

	-- 1) Repair: find damaged allied units in range
	if wd.bpCanRepair then
		local allies = spGetUnitsInSphere(ux, uy, uz, range, allyTeam)
		if allies then
			for _, aID in ipairs(allies) do
				if aID ~= unitID then
					local hp, maxHP, _, _, buildProg = spGetUnitHealth(aID)
					if hp and hp < maxHP and buildProg == 1.0 then
						return aID, BP_REPAIR
					end
				end
			end
		end
	end

	-- 2) Build/assist: find allied units under construction
	if wd.bpCanBuild then
		local allies = spGetUnitsInSphere(ux, uy, uz, range, allyTeam)
		if allies then
			for _, aID in ipairs(allies) do
				if aID ~= unitID then
					local _, _, _, _, buildProg = spGetUnitHealth(aID)
					if buildProg and buildProg < 1.0 then
						return aID, BP_BUILD
					end
				end
			end
		end
	end

	-- 3) Reclaim: find reclaimable features
	if wd.bpCanReclaim then
		local features = spGetFeaturesInSphere(ux, uy, uz, range)
		if features then
			for _, fID in ipairs(features) do
				local fDefID = spGetFeatureDefID(fID)
				local fDef = fDefID and FeatureDefs[fDefID]
				if fDef and fDef.reclaimable then
					local fIDEncoded = fID + Game.maxUnits
					return fIDEncoded, BP_RECLAIM
				end
			end
		end
	end

	-- 4) Resurrect: find resurrectible feature corpses
	if wd.bpCanResurrect then
		local features = spGetFeaturesInSphere(ux, uy, uz, range)
		if features then
			for _, fID in ipairs(features) do
				local fDefID = spGetFeatureDefID(fID)
				local fDef = fDefID and FeatureDefs[fDefID]
				if fDef and fDef.resurrectable ~= 0 then
					local fIDEncoded = fID + Game.maxUnits
					return fIDEncoded, BP_RESURRECT
				end
			end
		end
	end

	return nil, BP_NONE
end

----------------------------------------------------------------
-- Main loop — runs every second
----------------------------------------------------------------

function gadget:GameFrame(frame)
	if frame % 30 ~= 0 then
		return
	end

	for unitID, data in pairs(bpUnits) do
		if spGetUnitIsDead(unitID) then
			bpUnits[unitID] = nil
		else
			local ux, uy, uz = spGetUnitPosition(unitID)
			local allyTeam = spGetUnitAllyTeam(unitID)

			for weaponNum, wdID in pairs(data.weapons) do
				local range = WeaponDefs[wdID].range or 300

				local targetID, action = FindTargetForWeapon(
					unitID, weaponNum, wdID,
					ux, uy, uz, allyTeam, range
				)

				if targetID then
					spSetUnitBuildPowerTarget(unitID, weaponNum, targetID, action)
				else
					-- clear weapon target if nothing found
					spSetUnitBuildPowerTarget(unitID, weaponNum, nil)
				end
			end
		end
	end
end
