#include "infcentity.h"

#include <base/tl/ic_array.h>

#include <game/animation.h>
#include <game/server/gamecontext.h>
#include <game/server/infclass/entities/infccharacter.h>
#include <game/server/infclass/infcgamecontroller.h>

static int FilterOwnerId = -1;
static icArray<const CEntity *, 10> aFilterEntities;

static bool OwnerFilter(const CEntity *pEntity)
{
	const CInfCEntity *pInfEntity = static_cast<const CInfCEntity *>(pEntity);
	return pInfEntity->GetOwner() == FilterOwnerId;
}

static bool ExceptEntitiesFilter(const CEntity *pEntity)
{
	return !aFilterEntities.Contains(pEntity);
}

CInfCEntity::CInfCEntity(CGameContext *pGameContext, int ObjectType, vec2 Pos, int Owner,
                         int ProximityRadius)
	: CEntity(pGameContext->GameWorld(), ObjectType, Pos, ProximityRadius)
	, m_Owner(Owner)
{
}

CInfClassGameController *CInfCEntity::GameController()
{
	return static_cast<CInfClassGameController*>(GameServer()->m_pController);
}

CInfClassCharacter *CInfCEntity::GetOwnerCharacter()
{
	return GameController()->GetCharacter(GetOwner());
}

CInfClassPlayerClass *CInfCEntity::GetOwnerClass()
{
	CInfClassCharacter *pCharacter = GetOwnerCharacter();
	if (pCharacter)
		return pCharacter->GetClass();

	return nullptr;
}

EntityFilter CInfCEntity::GetOwnerFilterFunction(int Owner)
{
	FilterOwnerId = Owner;
	return OwnerFilter;
}

EntityFilter CInfCEntity::GetOwnerFilterFunction()
{
	return GetOwnerFilterFunction(GetOwner());
}

EntityFilter CInfCEntity::GetExceptEntitiesFilterFunction(const icArray<const CEntity *, 10> &aEntities)
{
	aFilterEntities = aEntities;
	return ExceptEntitiesFilter;
}

void CInfCEntity::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CInfCEntity::Tick()
{
	if(m_EndTick.has_value() && (Server()->Tick() >= m_EndTick))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(m_PosEnv >= 0)
	{
		SyncPosition();
	}
	else
	{
		m_Pos += m_Velocity;
	}
}

void CInfCEntity::TickPaused()
{
	if (m_EndTick.has_value())
		++m_EndTick.value();
}

void CInfCEntity::SetPos(const vec2 &Position)
{
	m_Pos = Position;
}

void CInfCEntity::SetAnimatedPos(const vec2 &Pivot, const vec2 &RelPosition, int PosEnv)
{
	m_Pivot = Pivot;
	m_RelPosition = RelPosition;
	m_PosEnv = PosEnv;
}

float CInfCEntity::GetLifespan() const
{
	if (!m_EndTick.has_value())
		return -1;

	int RemainingTicks = m_EndTick.value_or(0) - Server()->Tick();
	return RemainingTicks <= 0 ? 0 : RemainingTicks / static_cast<float>(Server()->TickSpeed());
}

void CInfCEntity::SetLifespan(float Lifespan)
{
	m_EndTick = Server()->Tick() + Server()->TickSpeed() * Lifespan;
}

void CInfCEntity::ResetLifespan()
{
	m_EndTick.reset();
}

bool CInfCEntity::DoSnapForClient(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return false;

	return true;
}

void CInfCEntity::SyncPosition()
{
	vec2 Position(0.0f, 0.0f);
	float Angle = 0.0f;
	if(m_PosEnv >= 0)
	{
		GetAnimationTransform(GameController()->GetTime(), m_PosEnv, GameServer()->Layers(), Position, Angle);
	}

	float x = (m_RelPosition.x * cosf(Angle) - m_RelPosition.y * sinf(Angle));
	float y = (m_RelPosition.x * sinf(Angle) + m_RelPosition.y * cosf(Angle));

	SetPos(Position + m_Pivot + vec2(x, y));
}
