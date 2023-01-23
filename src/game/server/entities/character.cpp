/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.				*/

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <new>
#include <iostream>

#include "character.h"
#include "projectile.h"

#include <game/server/teams.h>

int CCharacter::EntityId = CGameWorld::ENTTYPE_CHARACTER;

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld, IConsole *pConsole) :
	CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), CCharacterCore::PhysicalSize()),
	m_pConsole(pConsole)
{
	m_Health = 0;
	m_Armor = 0;
	
/* INFECTION MODIFICATION START ***************************************/
	m_MaxArmor = 10;

	m_FlagID = Server()->SnapNewID();
	m_HeartID = Server()->SnapNewID();
	m_CursorID = Server()->SnapNewID();
	m_AntiFireTime = 0;
	m_PainSoundTimer = 0;
	m_IsFrozen = false;
	m_IsInSlowMotion = false;
	m_FrozenTime = -1;
	m_DartLifeSpan = -1;
	m_InfZoneTick = -1;
	m_InAirTick = 0;
	m_InWater = 0;
	m_ProtectionTick = 0;
	m_WaterJumpLifeSpan = 0;
	m_NinjaVelocityBuff = 0;
	m_NinjaStrengthBuff = 0;
	m_NinjaAmmoBuff = 0;
	m_HasIndicator = false;
/* INFECTION MODIFICATION END *****************************************/
}

CCharacter::~CCharacter()
{
	FreeChildSnapIDs();
}

void CCharacter::Reset()
{	
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	SetActiveWeapon(WEAPON_GUN);
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	m_Core.Reset();
	m_Core.Init(&GameServer()->m_World.m_Core, GameServer()->Collision());
	m_Core.m_Pos = GetPos();
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;

	GameServer()->m_pController->OnCharacterSpawn(this);

	DDRaceInit();

	return true;
}

void CCharacter::Destroy()
{	
	if(m_pPlayer)
		GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_ActiveWeapon)
		return;

	m_LastWeapon = m_ActiveWeapon;
	m_QueuedWeapon = -1;
	SetActiveWeapon(W);
	GameServer()->CreateSound(GetPos(), SOUND_WEAPON_SWITCH);

	if(m_ActiveWeapon < 0 || m_ActiveWeapon >= NUM_WEAPONS)
		SetActiveWeapon(0);
}

bool CCharacter::IsGrounded() const
{
	if(Collision()->CheckPoint(GetPos().x + m_ProximityRadius / 2, GetPos().y + m_ProximityRadius / 2 + 5))
		return true;
	if(Collision()->CheckPoint(GetPos().x - m_ProximityRadius / 2, GetPos().y + m_ProximityRadius / 2 + 5))
		return true;
	return false;
}

void CCharacter::HandleWaterJump()
{
	if(m_InWater)
	{
		m_Core.m_Jumped &= ~2;
		m_Core.m_TriggeredEvents &= ~COREEVENT_GROUND_JUMP;
		m_Core.m_TriggeredEvents &= ~COREEVENT_AIR_JUMP;

		if(m_Input.m_Jump && m_DartLifeSpan <= 0 && m_WaterJumpLifeSpan <=0)
		{
			m_WaterJumpLifeSpan = Server()->TickSpeed()/2;
			m_DartLifeSpan = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
			vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));
			m_DartDir = Direction;
			m_DartOldVelAmount = length(m_Core.m_Vel);
			
			m_Core.m_TriggeredEvents |= COREEVENT_AIR_JUMP;
		}
	}
	
	m_WaterJumpLifeSpan--;
	
	m_DartLifeSpan--;
	
	if(m_DartLifeSpan == 0)
	{
		//~ m_Core.m_Vel = m_DartDir * 5.0f;
		m_Core.m_Vel = m_DartDir*m_DartOldVelAmount;
	}
	
	if(m_DartLifeSpan > 0)
	{
		m_Core.m_Vel = m_DartDir * 15.0f;
		GameServer()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(m_ProximityRadius, m_ProximityRadius), 0.f);
		m_Core.m_Vel = vec2(0.f, 0.f);
	}	
}

void CCharacter::DoWeaponSwitch()
{
/* INFECTION MODIFICATION START ***************************************/
	if(m_QueuedWeapon == -1 || !m_aWeapons[m_QueuedWeapon].m_Got)
		return;
/* INFECTION MODIFICATION END *****************************************/

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
}

void CCharacter::HandleWeapons()
{
	if(IsFrozen())
		return;
		
	//ninja
	HandleNinja();

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();
}

/* INFECTION MODIFICATION START ***************************************/
void CCharacter::SetAntiFire()
{
	m_AntiFireTime = Server()->TickSpeed() * Config()->m_InfAntiFireTime / 1000;
}

/* INFECTION MODIFICATION END *****************************************/

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	INFWEAPON InfWID = GetInfWeaponID(Weapon);
	int MaxAmmo = Server()->GetMaxAmmo(InfWID);

	if(InfWID == INFWEAPON::NINJA_GRENADE)
		MaxAmmo = minimum(MaxAmmo + m_NinjaAmmoBuff, 10);
	
	if(Ammo < 0)
		Ammo = MaxAmmo;
	
	if(m_aWeapons[Weapon].m_Ammo < MaxAmmo || !m_aWeapons[Weapon].m_Got)
	{
		m_aWeapons[Weapon].m_Got = true;
		m_aWeapons[Weapon].m_Ammo = minimum(MaxAmmo, Ammo);
		//dbg_msg("TEST", "TRUE")
		return true;
	}
	return false;
}

void CCharacter::SetActiveWeapon(int Weapon)
{
	m_ActiveWeapon = Weapon;
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::NoAmmo()
{
	// 125ms is a magical limit of how fast a human can click
	m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
	if(m_LastNoAmmoSound + Server()->TickSpeed() * 0.5 <= Server()->Tick())
	{
		GameServer()->CreateSound(GetPos(), SOUND_WEAPON_NOAMMO);
		m_LastNoAmmoSound = Server()->Tick();
	}
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	// set emote
	if(m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = m_pPlayer->GetDefaultEmote();
		m_EmoteStop = -1;
	}

	m_Core.m_Id = GetPlayer()->GetCID();
/* INFECTION MODIFICATION START ***************************************/
	PreCoreTick();

	m_Core.m_Input = m_Input;
	
	CCharacterCore::CParams CoreTickParams(&m_pPlayer->m_NextTuningParams);
	//~ CCharacterCore::CParams CoreTickParams(&GameWorld()->m_Core.m_Tuning);

	if(PrivateGetPlayerClass() == PLAYERCLASS_SPIDER)
	{
		CoreTickParams.m_HookGrabTime = g_Config.m_InfSpiderHookTime*SERVER_TICK_SPEED;
	}
	if(PrivateGetPlayerClass() == PLAYERCLASS_BAT)
	{
		CoreTickParams.m_HookGrabTime = g_Config.m_InfBatHookTime*SERVER_TICK_SPEED;
	}

	CoreTickParams.m_HookMode = GetEffectiveHookMode();
	
	m_Core.Tick(true, &CoreTickParams);
	
	//Hook protection
	if(m_Core.m_HookedPlayer >= 0)
	{
		CPlayer *pHookedPlayer = GameServer()->m_apPlayers[m_Core.m_HookedPlayer];
		if(pHookedPlayer)
		{
			if(IsZombie() == pHookedPlayer->IsZombie() && pHookedPlayer->HookProtectionEnabled())
			{
				m_Core.m_HookedPlayer = -1;
				m_Core.m_HookState = HOOK_RETRACTED;
				m_Core.m_HookPos = GetPos();
			}
		}
	}
	
	HandleWaterJump();
	HandleWeapons();

	PostCoreTick();

/* INFECTION MODIFICATION END *****************************************/

	// Previnput
	m_PrevInput = m_Input;
}

void CCharacter::TickDefered()
{
	// advance the dummy
	{
		CCharacterCore::CParams CoreTickParams(&GameWorld()->m_Core.m_Tuning);
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision(), &Teams()->m_Core);
		m_ReckoningCore.m_Id = m_pPlayer->GetCID();
		m_ReckoningCore.Tick(false, &CoreTickParams);
		m_ReckoningCore.Move(&CoreTickParams);
		m_ReckoningCore.Quantize();
	}

	CCharacterCore::CParams CoreTickParams(&m_pPlayer->m_NextTuningParams);
	
	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.m_Id = m_pPlayer->GetCID();
	m_Core.Move(&CoreTickParams);
	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_ReckoningTick+Server()->TickSpeed()*3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_DamageTakenTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart > -1)
		++m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
		
/* INFECTION MODIFICATION START ***************************************/
	++m_HookDmgTick;
/* INFECTION MODIFICATION END *****************************************/
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 10)
		return false;

	SetHealthArmor(m_Health + Amount, m_Armor);

	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= m_MaxArmor)
		return false;

	SetHealthArmor(m_Health, m_Armor + Amount);

	return true;
}

bool CCharacter::IncreaseOverallHp(int Amount)
{
	int MissingHealth = 10 - m_Health;
	int MissingArmor = m_MaxArmor - m_Armor;
	int ExtraHealthAmount = clamp<int>(Amount, 0, MissingHealth);
	int ExtraArmorAmount = clamp<int>(Amount - ExtraHealthAmount, 0, MissingArmor);

	if((ExtraHealthAmount > 0) || (ExtraArmorAmount > 0))
	{
		SetHealthArmor(m_Health + ExtraHealthAmount, m_Armor + ExtraArmorAmount);
		return true;
	}

	return false;
}

void CCharacter::SetHealthArmor(int HealthAmount, int ArmorAmount)
{
	int TotalBefore = m_Health + m_Armor;

	m_Health = clamp<int>(HealthAmount, 0, 10);
	m_Armor = clamp<int>(ArmorAmount, 0, m_MaxArmor);

	int TotalAfter = m_Health + m_Armor;

	OnTotalHealthChanged(TotalAfter - TotalBefore);
}

int CCharacter::GetHealthArmorSum()
{
	return m_Health + m_Armor;
}

void CCharacter::SetMaxArmor(int Amount)
{
	m_MaxArmor = Amount;
}

void CCharacter::Die(int Killer, int Weapon)
{
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon, TAKEDAMAGEMODE Mode)
{
	return false;
}

//TODO: Move the emote stuff to a function
void CCharacter::SnapCharacter(int SnappingClient, int ID)
{
	CCharacterCore *pCore;
	int Tick, Weapon = m_ActiveWeapon;

	// write down the m_Core
	if(!m_ReckoningTick || GameServer()->m_World.m_Paused)
	{
		Tick = 0;
		pCore = &m_Core;
	}
	else
	{
		Tick = m_ReckoningTick;
		pCore = &m_SendCore;
	}

	int EmoteNormal = m_pPlayer->GetDefaultEmote();

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, ID, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;
	pCharacter->m_Tick = Tick;
	pCore->Write(pCharacter);
	if(pCharacter->m_HookedPlayer != -1)
	{
		if(!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
			pCharacter->m_HookedPlayer = -1;
	}
	pCharacter->m_Emote = m_EmoteType;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;

/* INFECTION MODIFICATION START ***************************************/
	if(GetInfWeaponID(m_ActiveWeapon) == INFWEAPON::NINJA_HAMMER)
	{
		Weapon = WEAPON_NINJA;
	}

	if(PrivateGetPlayerClass() == PLAYERCLASS_SPIDER)
	{
		pCharacter->m_HookTick -= (g_Config.m_InfSpiderHookTime - 1) * SERVER_TICK_SPEED-SERVER_TICK_SPEED/5;
		if(pCharacter->m_HookTick < 0)
			pCharacter->m_HookTick = 0;
	}
	if(PrivateGetPlayerClass() == PLAYERCLASS_BAT)
	{
		pCharacter->m_HookTick -= (g_Config.m_InfBatHookTime - 1) * SERVER_TICK_SPEED - SERVER_TICK_SPEED/5;
		if(pCharacter->m_HookTick < 0)
			pCharacter->m_HookTick = 0;
	}
/* INFECTION MODIFICATION END *****************************************/
	pCharacter->m_AttackTick = m_AttackTick;
	pCharacter->m_Direction = m_Input.m_Direction;
	pCharacter->m_Weapon = Weapon;

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID))
	{
		pCharacter->m_Health = m_Health;
		pCharacter->m_Armor = clamp<int>(m_Armor, 0, 10);
		if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0)
			pCharacter->m_AmmoCount = m_aWeapons[m_ActiveWeapon].m_Ammo;
	}

/* INFECTION MODIFICATION START ***************************************/
	if(GetInfWeaponID(m_ActiveWeapon) == INFWEAPON::MERCENARY_GUN)
	{
		pCharacter->m_AmmoCount /= (Server()->GetMaxAmmo(INFWEAPON::MERCENARY_GUN) / 10);
	}
/* INFECTION MODIFICATION END *****************************************/

	if(pCharacter->m_Emote == EmoteNormal)
	{
		if(250 - ((Server()->Tick() - m_LastAction)%(250)) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}

	pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;
}

void CCharacter::Snap(int SnappingClient)
{
	int ID = m_pPlayer->GetCID();

	if(!Server()->Translate(ID, SnappingClient))
		return;

	if(NetworkClipped(SnappingClient))
		return;

	SnapCharacter(SnappingClient, ID);
}

bool CCharacter::CanCollide(int ClientID)
{
	return Teams()->m_Core.CanCollide(GetPlayer()->GetCID(), ClientID);
}

int CCharacter::Team()
{
	return Teams()->m_Core.Team(m_pPlayer->GetCID());
}

void CCharacter::HandleSkippableTiles(int Index)
{
#if 0
	if(GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}
#endif

	if(Index < 0)
		return;

	// handle speedup tiles
	if(GameServer()->Collision()->IsSpeedup(Index))
	{
		vec2 Direction, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		GameServer()->Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if(Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed / 5);
		}
		else
		{
			if(MaxSpeed > 0 && MaxSpeed < 5)
				MaxSpeed = 5;
			if(MaxSpeed > 0)
			{
				if(Direction.x > 0.0000001f)
					SpeederAngle = -atan(Direction.y / Direction.x);
				else if(Direction.x < 0.0000001f)
					SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
				else if(Direction.y > 0.0000001f)
					SpeederAngle = asin(1.0f);
				else
					SpeederAngle = asin(-1.0f);

				if(SpeederAngle < 0)
					SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

				if(TempVel.x > 0.0000001f)
					TeeAngle = -atan(TempVel.y / TempVel.x);
				else if(TempVel.x < 0.0000001f)
					TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
				else if(TempVel.y > 0.0000001f)
					TeeAngle = asin(1.0f);
				else
					TeeAngle = asin(-1.0f);

				if(TeeAngle < 0)
					TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

				TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
				if(abs((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if(abs((int)SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;

			m_Core.m_Vel = ClampVel(m_MoveRestrictions, TempVel);
		}
	}
}

int CCharacter::NetworkClipped(int SnappingClient) const
{
	return CEntity::NetworkClipped(SnappingClient, m_Pos) && (m_Core.m_HookState == HOOK_IDLE || CEntity::NetworkClipped(SnappingClient, m_Core.m_HookPos));
}

/* INFECTION MODIFICATION START ***************************************/
vec2 CCharacter::GetDirection() const
{
	return normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));
}

int CCharacter::PrivateGetPlayerClass() const
{
	if(!m_pPlayer)
		return PLAYERCLASS_NONE;
	else
		return m_pPlayer->GetClass();
}

void CCharacter::FreeChildSnapIDs()
{
	if(m_FlagID >= 0)
	{
		Server()->SnapFreeID(m_FlagID);
		m_FlagID = -1;
	}
	if(m_HeartID >= 0)
	{
		Server()->SnapFreeID(m_HeartID);
		m_HeartID = -1;
	}
	if(m_CursorID >= 0)
	{
		Server()->SnapFreeID(m_CursorID);
		m_CursorID = -1;
	}
}

bool CCharacter::IsZombie() const
{
	return m_pPlayer->IsZombie();
}

bool CCharacter::IsHuman() const
{
	return m_pPlayer->IsHuman();
}

bool CCharacter::IsInLove() const
{
	return m_LoveTick > 0;
}

void CCharacter::LoveEffect(float Time)
{
	if(m_LoveTick <= 0)
		m_LoveTick = Server()->TickSpeed() * Time;
}

void CCharacter::HallucinationEffect()
{
	if(m_HallucinationTick <= 0)
		m_HallucinationTick = Server()->TickSpeed()*5;
}

void CCharacter::SlipperyEffect()
{
	if(m_SlipperyTick <= 0)
		m_SlipperyTick = Server()->TickSpeed()/2;
}

void CCharacter::Freeze(float Time, int Player, FREEZEREASON Reason)
{
	if(m_IsFrozen && m_FreezeReason == FREEZEREASON_UNDEAD)
		return;
	
	m_IsFrozen = true;
	m_FrozenTime = Server()->TickSpeed()*Time;
	m_FreezeReason = Reason;
	
	m_LastFreezer = Player;

	m_Core.m_FreezeStart = Server()->Tick();
}

void CCharacter::Unfreeze()
{
	m_IsFrozen = false;
	m_FrozenTime = -1;
	
	if(m_FreezeReason == FREEZEREASON_UNDEAD)
	{
		m_Health = 10.0;
	}
	
	if(m_pPlayer)
	{
		GameServer()->ClearBroadcast(m_pPlayer->GetCID(), BROADCAST_PRIORITY_EFFECTSTATE);
	}
	GameServer()->CreatePlayerSpawn(GetPos());
}

bool CCharacter::IsFrozen() const
{
	return m_IsFrozen;
}

bool CCharacter::IsInSlowMotion() const
{
	return m_SlowMotionTick > 0;
}

// duration in centiSec (10 == 1 second)
void CCharacter::SlowMotionEffect(float Duration, int FromCID)
{
	if(Duration == 0)
		return;
	Duration *= 0.1f;
	int NewSlowTick = Server()->TickSpeed()*Duration;
	if(m_SlowMotionTick <= 0)
	{
		m_SlowMotionTick = NewSlowTick;
		m_SlowEffectApplicant = FromCID;
		m_IsInSlowMotion = true;
		m_Core.m_Vel *= 0.4f;
	}
}

INFWEAPON CCharacter::GetInfWeaponID(int WID) const
{
	if(WID == WEAPON_HAMMER)
	{
		switch(PrivateGetPlayerClass())
		{
			case PLAYERCLASS_NINJA:
				return INFWEAPON::NINJA_HAMMER;
			default:
				return INFWEAPON::HAMMER;
		}
	}
	else if(WID == WEAPON_GUN)
	{
		switch(PrivateGetPlayerClass())
		{
			case PLAYERCLASS_MERCENARY:
				return INFWEAPON::MERCENARY_GUN;
			default:
				return INFWEAPON::GUN;
		}
		return INFWEAPON::GUN;
	}
	else if(WID == WEAPON_SHOTGUN)
	{
		switch(PrivateGetPlayerClass())
		{
			case PLAYERCLASS_MEDIC:
				return INFWEAPON::MEDIC_SHOTGUN;
			case PLAYERCLASS_HERO:
				return INFWEAPON::HERO_SHOTGUN;
			case PLAYERCLASS_BIOLOGIST:
				return INFWEAPON::BIOLOGIST_SHOTGUN;
			default:
				return INFWEAPON::SHOTGUN;
		}
	}
	else if(WID == WEAPON_GRENADE)
	{
		switch(PrivateGetPlayerClass())
		{
			case PLAYERCLASS_MERCENARY:
				return INFWEAPON::MERCENARY_GRENADE;
			case PLAYERCLASS_MEDIC:
				return INFWEAPON::MEDIC_GRENADE;
			case PLAYERCLASS_SOLDIER:
				return INFWEAPON::SOLDIER_GRENADE;
			case PLAYERCLASS_NINJA:
				return INFWEAPON::NINJA_GRENADE;
			case PLAYERCLASS_SCIENTIST:
				return INFWEAPON::SCIENTIST_GRENADE;
			case PLAYERCLASS_HERO:
				return INFWEAPON::HERO_GRENADE;
			case PLAYERCLASS_LOOPER:
				return INFWEAPON::LOOPER_GRENADE;
			default:
				return INFWEAPON::GRENADE;
		}
	}
	else if(WID == WEAPON_LASER)
	{
		switch(PrivateGetPlayerClass())
		{
			case PLAYERCLASS_ENGINEER:
				return INFWEAPON::ENGINEER_LASER;
			case PLAYERCLASS_NINJA:
				return INFWEAPON::BLINDING_LASER;
			case PLAYERCLASS_LOOPER:
				return INFWEAPON::LOOPER_LASER;
			case PLAYERCLASS_SCIENTIST:
				return INFWEAPON::SCIENTIST_LASER;
			case PLAYERCLASS_SNIPER:
				return INFWEAPON::SNIPER_LASER;
			case PLAYERCLASS_HERO:
				return INFWEAPON::HERO_LASER;
			case PLAYERCLASS_BIOLOGIST:
				return INFWEAPON::BIOLOGIST_LASER;
			case PLAYERCLASS_MEDIC:
				return INFWEAPON::MEDIC_LASER;
			case PLAYERCLASS_MERCENARY:
				return INFWEAPON::MERCENARY_LASER;
			default:
				return INFWEAPON::LASER;
		}
	}
	else if(WID == WEAPON_NINJA)
	{
		return INFWEAPON::NINJA;
	}
	else
	{
		return INFWEAPON::NONE;
	}
}

int CCharacter::GetEffectiveHookMode() const
{
	if(m_Core.m_HookedPlayer >= 0)
		return 0;

	return m_HookMode;
}

/* INFECTION MODIFICATION END *****************************************/

void CCharacter::PostCoreTick()
{
	// following jump rules can be overridden by tiles, like Refill Jumps, Stopper and Wall Jump
	if(m_Core.m_Jumps == -1)
	{
		// The player has only one ground jump, so his feet are always dark
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_Jumps == 0)
	{
		// The player has no jumps at all, so his feet are always dark
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0)
	{
		// If the player has only one jump, each jump is the last one
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)
	{
		// The player has not yet used up all his jumps, so his feet remain light
		m_Core.m_Jumped = 1;
	}

	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	HandleSkippableTiles(CurrentIndex);
	m_MoveRestrictions = GameServer()->Collision()->GetMoveRestrictions(nullptr, this, m_Pos, 18.0f, CurrentIndex);
}

void CCharacter::SetTeams(CGameTeams *pTeams)
{
	m_pTeams = pTeams;
	m_Core.SetTeamsCore(&m_pTeams->m_Core);
}

void CCharacter::DDRaceInit()
{
	m_Core.m_Id = GetPlayer()->GetCID();
}
