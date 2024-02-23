#include "ic_door.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

CDoor::CDoor(CGameContext *pGameContext, vec2 Pos, vec2 PosTo) :
	CPlacedObject(pGameContext, CGameWorld::ENTTYPE_DOOR),
	m_Open{true}
{
	m_Pos = Pos;
	m_Pos2 = PosTo;

	m_InfClassObjectFlags = INFCLASS_OBJECT_FLAG_HAS_SECOND_POSITION;

	SetOpen(false);
	GameWorld()->InsertEntity(this);
}

void CDoor::Destroy()
{
	SetOpen(true);

	CPlacedObject::Destroy();
}

void CDoor::SetCollisions(bool Set)
{
	const vec2 DoorVector = m_Pos2 - m_Pos;
	const float Distance = length(DoorVector);

	int PrevIndex = -1;
	auto SetOncePerTile = [&](vec2 Pos)
	{
		int Index = GameServer()->Collision()->GetPureMapIndex(Pos);
		if (Index == PrevIndex)
			return;

		PrevIndex = Index;
		GameServer()->Collision()->SetDoorCollisionAt(Index, Set);
	};

	SetOncePerTile(m_Pos);
	if(Distance > TileSizeF)
	{
		float Step = TileSize / 2;
		vec2 NormalizedDoor = DoorVector / Distance;
		float ProcessedDistance = Step;
		while(ProcessedDistance < Distance)
		{
			SetOncePerTile(m_Pos + NormalizedDoor * ProcessedDistance);
			ProcessedDistance += Step;
		}
	}
	SetOncePerTile(m_Pos2);
}

void CDoor::Reset()
{
	MarkForDestroy();
}

void CDoor::Snap(int SnappingClientId)
{
	const std::optional<CViewParams> ViewParams = GetViewParams(GameServer(), SnappingClientId);

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClientId);

	vec2 To = m_Pos;
	vec2 From = IsOpen() ? m_Pos : m_Pos2;
	int StartTick = 0;

	if(SnappingClientVersion < VERSION_DDNET_ENTITY_NETOBJS)
	{
		StartTick = Server()->Tick();
	}

	const bool ForcedShowOpen = Config()->m_SvShowOpenDoors && IsOpen();
	if(ForcedShowOpen)
	{
		From = m_Pos2;
	}
	int MaxY = Collision()->GetHeight() * TileSize;
	if((From != To) && ViewParams.has_value())
	{
		if(m_Pos.x == m_Pos2.x)
		{
			const bool AutoextendTop = ((m_Pos.y < 32) || (m_Pos2.y < 32));
			const bool AutoextendBottom = ((m_Pos.y >= MaxY) || (m_Pos2.y >= MaxY));

			if(AutoextendTop)
			{
				const auto TopY = ViewParams->ViewPos.y - ViewParams->ShowDistance.y - 1000;
				if(To.y < 32)
				{
					To.y = TopY;
				}
				else
				{
					From.y = TopY;
				}
			}
			if(AutoextendBottom)
			{
				const auto BottomY = ViewParams->ViewPos.y + ViewParams->ShowDistance.y + 1000;
				if(To.y >= MaxY)
				{
					To.y = BottomY;
				}
				else
				{
					From.y = BottomY;
				}
			}
		}
	}

	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion), GetId(), To, From, StartTick, -1, ForcedShowOpen ? LASERTYPE_FREEZE : LASERTYPE_DOOR);
	// TODO: CGameContext::SnapSwitchers()
}

void CDoor::SetOpen(bool Open)
{
	if (m_Open == Open)
		return;

	m_Open = Open;
	SetCollisions(!Open);
}
