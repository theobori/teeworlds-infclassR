/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>

#include <game/server/infclass/classes/infcplayerclass.h>

#include "infccharacter.h"
#include "slug-slime.h"

int CSlugSlime::EntityId = CGameWorld::ENTTYPE_SLUG_SLIME;

CSlugSlime::CSlugSlime(CGameContext *pGameContext, vec2 Pos, int Owner) :
	CPlacedObject(pGameContext, EntityId, Pos, Owner)
{
	m_StartTick = Server()->Tick();
	m_EndTick = m_StartTick + Server()->TickSpeed() * Config()->m_InfSlimeDuration;
	GameWorld()->InsertEntity(this);
}

void CSlugSlime::Tick()
{
	if(m_MarkedForDestroy) return;

	if(Server()->Tick() >= m_EndTick)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// Find other players
	for(CInfClassCharacter *p = (CInfClassCharacter*) GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CInfClassCharacter *)p->TypeNext())
	{
		if(!GameServer()->Collision()->AreConnected(p->m_Pos, m_Pos, 84.0f))
			continue; // not in reach
		
		p->GetClass()->OnSlimeEffect(m_Owner);
	}

	int ExistsForTicks = Server()->Tick() - m_StartTick;
	if((ExistsForTicks % 20) == 0)
	{
		GameServer()->CreateDeath(m_Pos, m_Owner);
	}
}

void CSlugSlime::TickPaused()
{
	m_StartTick++;
	m_EndTick++;
}

void CSlugSlime::Snap(int SnappingClient)
{
	// Do not snap Slime at all to prevent possible crash on owner indicator rendering
	// (Infclass clients up to v0.1.8 can handle up to 8 indicators and CRASH on overflow
	if constexpr (true)
		return;

	if(!DoSnapForClient(SnappingClient))
		return;

	if(Server()->GetClientInfclassVersion(SnappingClient))
	{
		CNetObj_InfClassObject *pInfClassObject = SnapInfClassObject();
		if(!pInfClassObject)
			return;

		pInfClassObject->m_StartTick = m_StartTick;
		pInfClassObject->m_EndTick = m_EndTick;
	}
}

void CSlugSlime::Replenish(int PlayerID)
{
	m_Owner = PlayerID;
	m_EndTick = Server()->Tick() + Server()->TickSpeed() * Config()->m_InfSlimeDuration;
}
