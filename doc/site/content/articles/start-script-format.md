+++
title = 'Start Script Format'
date = 2025-05-10T01:21:55-07:00
draft = false
+++
```
// Property keys are case insensitive

// == IP Support ==
// IP properties accept:
// - IP v4 addresses like 192.168.0.20 or 123.123.123.123
// - IP v6 addresses like 2001:0db8:85a3:8a2e:: or fe80::1
// - IP v4 any addresses 0.0.0.0 (allows incoming connections on all local IPv4 IPs)
// - IP v6 any addresses ::      (allows incoming connections on all local IPv6 IPs)
// - NO host-names

// A minimal client example
// If spring is started as client, it needs the following information to work properly
[GAME]
{
	HostIP=xxx.xxx.xxx.xxx; // which IP the server is hosting on. see "IP Support" at the start of this document.
	HostPort=xxx;       // (optional) default is 8452
	SourcePort=0;       // (optional) default is 0. set this if you want a different source port (as client), 0 means OS-select and should be preferred.

	MyPlayerName=somename;
	MyPasswd=secretpassword;
	IsHost=0;           // tell the engine this is a client
}

// A host example
// note that the same values for clients also need to be set here
[GAME]
{
	MapName=;           // with .smf extension
	GameType=Balanced Annihilation V6.81; // either primary mod NAME, rapid tag name or archive name
	GameStartDelay=4;   // optional, in seconds (unsigned int), default: 4
	                    // The number of seconds till the game starts,
	                    // counting from the moment when all players are connected and ready.
	StartPosType=x;     // 0 fixed, 1 random, 2 choose in game, 3 choose before game (see StartPosX)

	DemoFile=demo.sdfz; // if set this game is a multiplayer demo replay
	SaveFile=save.ssf;  // if set this game is a continuation of a saved game
	RecordDemo=1;       // set to 0 to disable demo file recording

	HostIP=xxx.xxx.xxx.xxx; // (optional) which IP to host on. omit or use an empty value to host on any local IP. see "IP Support" at the start of this document.
	HostPort=xxx;       // (optional) default is 8452. where clients are going to connect to.
	SourcePort=0;       // no effect for host
	AutohostIP=xxx.xxx.xxx.xxx; // communicate with spring, specify which IP the autohost is listening on. see "IP Support" at the start of this document.
	AutohostPort=X;     // communicate with spring, specify the port you are listening (as host)

	MyPlayerName=somename; // our ingame-name (needs to match one players Name= field)

	IsHost=1;           // 0: no server will be started in this instance
	                    // 1: start a server
	NumPlayers=x;       // not mandatory, but can be used for debugging purposes
	NumTeams=y;         // same here, set this to check if the script is right
	NumAllyTeams=z;     // see above

	FixedRNGSeed = 1; // Initialize the synced RNG with a specific seed for reproducible runs, eg. for benchmarking or cutscenes.
                          // Default value 0 will generate a random seed.

	// A player (controls a team, player 0 is the host only if IsHost is not set)
	[PLAYER0]
	{
		Name=name;      // pure info, eg. the account name from the lobby server
		Password=secretpassword; // player can only connect if he set MyPasswd accordingly
		Spectator=0;
		Team=number;    // the team this player controls
		IsFromDemo=0;   // use only in combination with Demofile (see above)
		CountryCode=;   // country of the player, if known (nl/de/it etc.)
		Rank=-1;
	}
	// more players

	// A skirmish AI (controls a team)
	[AI0]
	{
		Name=name;     // [optional] pure info, eg. the name set in the lobby
		               // the name actually used in game will be:
		               // "${Name} (owner: ${player.Name})"
		ShortName=RAI; // shortName of the Skirmish AI library or name of the
		               // LUA AI that controls this team.
		               // see spring.exe --list-skirmish-ais for possible values
		Team=number;   // the team this AI controls
		Host=number;   // the player whichs computer this AI runs on
		               // eg. for [PLAYER0] above, this would be 0
		Version=0.1;   // [optional] version of this Skirmish AI
		[OPTIONS]      // [optional] contains AI specific options
		{
			difficultyLevel=1;
		}
	}
	// more skirmish AIs

	// players in this will share the same units (start with one commander etc.)
	[TEAM0]
	{
		TeamLeader=x;   // player number that is the "leader"
		                // if this is an AI controlled team, TeamLeader is the
		                // player number of the AI controlling team
		                // see AI.Host
		AllyTeam=number;
		RgbColor=red green blue;  // red green blue in range [0-1]
		Side=Arm/Core;  // other sides possible with user mods i suppose
		Handicap=0;         // DEPRECATED, see Advantage (below). Advantage as percentage, common: [-100, 100], valid: [-100, FLOAT_MAX]
		Advantage=0;        // Advantage factor (meta value). Currently only affects IncomeMultiplier (below), common: [-1.0, 1.0], valid: [-1.0, FLOAT_MAX]
		IncomeMultiplier=1; // multiplication factor for collected resources, common: [0.0, 2.0], valid: [0.0, FLOAT_MAX]
		StartPosX=0;    // Use these in combination with StartPosType=3
		StartPosZ=0;    // range is in map coordinates as returned by unitsync (NOT like StartRectTop et al)
		LuaAI=name;     // name of the LUA AI that controls this team
		// Either a [PLAYER] or an [AI] is controlling this team, or LuaAI is set.
		// DEPRECATED: The TeamLeader field indicates which computer the Skirmish AI will run on.
	}
	//more teams

	// teams in ally team share los etc and cant break alliance, every team must be in exactly one ally team
	[ALLYTEAM0]
	{
		NumAllies=0;
		Ally0=(AllyTeam number); // means that this team is allied with the other, not necessarily the reverse

		StartRectTop=0;    // Use these in combination with StartPosType=2
		StartRectLeft=0;   //   (ie. select in map)
		StartRectBottom=1; // range is 0-1: 0 is left or top edge,
		StartRectRight=1;  //   1 is right or bottom edge
	}
	// more ally teams

	// something for selecting which unit files to disable or restrict

	NumRestrictions=xx;

	[RESTRICT]
	{
		Unit0=armah;
		Limit0=0;       // use 0 for all units that should be completely disabled
		Unit1=corvp;
		Limit1=50;      // >0 can be used for limiting, like build restrictions in TA
		//...
	}
	[MODOPTIONS]        // values for the options defined in ModOptions.lua
	{
		StartMetal=1000;
		StartEnergy=1000;
		MaxUnits=500;       // per team
		GameMode=x;         // 0 cmdr dead->game continues, 1 cmdr dead->game ends, 2 lineage, 3 openend
		LimitDGun=0;        // limit dgun to fixed radius around startpos?
		DisableMapDamage=0; // disable map craters?
		GhostedBuildings=1; // ghost enemy buildings after losing los on them
		NoHelperAIs=0;      // are GroupAIs and other helper AIs allowed?
		LuaGaia=1;          // Use LuaGaia?
		LuaRules=1;         // Use LuaRules?
		FixedAllies=1;      // Are ingame alliances allowed?
		MaxSpeed=3;         // speed limits at game start
		MinSpeed=0.3;
	}
	[MAPOPTIONS]        // values for the options defined in MapOptions.lua
	{
		PropX=1000;
	}
}
```