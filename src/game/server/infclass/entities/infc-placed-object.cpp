#include "infc-placed-object.h"

#include "infccharacter.h"

#include <engine/server.h>
#include <game/server/infclass/infcgamecontroller.h>

CPlacedObject::CPlacedObject(CGameContext *pGameContext, int ObjectType, vec2 Pos, int Owner, int ProximityRadius)
	: CInfCEntity(pGameContext, ObjectType, Pos, Owner, ProximityRadius)
{
	m_InfClassObjectId = Server()->SnapNewId();
	m_InfClassObjectType = INFCLASS_OBJECT_TYPE_CUSTOM;
}

CPlacedObject::~CPlacedObject()
{
	Server()->SnapFreeId(m_InfClassObjectId);
}

void CPlacedObject::SetSecondPosition(vec2 Position)
{
	if(m_MaxLength.value() && distance(m_Pos, Position) > m_MaxLength.value())
	{
		m_Pos2 = m_Pos + normalize(Position - m_Pos) * m_MaxLength.value();
	}
	else
	{
		m_Pos2 = Position;
	}

	m_InfClassObjectFlags = INFCLASS_OBJECT_FLAG_HAS_SECOND_POSITION;
}

void CPlacedObject::Tick()
{
	CInfCEntity::Tick();

	if(m_Pos2.has_value())
	{
		m_Pos2.value() += m_Velocity;
	}
}

bool CPlacedObject::DoSnapForClient(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) && (!m_Pos2.has_value() || NetworkClipped(SnappingClient, m_Pos2.value())))
		return false;

	CInfClassCharacter *pCharacter = GameController()->GetCharacter(SnappingClient);
	if(pCharacter && pCharacter->IsBlind())
		return false;

	return true;
}

CNetObj_InfClassObject *CPlacedObject::SnapInfClassObject()
{
	CNetObj_InfClassObject *pInfClassObject = Server()->SnapNewItem<CNetObj_InfClassObject>(m_InfClassObjectId);
	if(!pInfClassObject)
		return nullptr;

	pInfClassObject->m_X = m_Pos.x;
	pInfClassObject->m_Y = m_Pos.y;

	pInfClassObject->m_Type = m_InfClassObjectType;
	pInfClassObject->m_Flags = m_InfClassObjectFlags;

	pInfClassObject->m_Owner = GetOwner();

	pInfClassObject->m_StartTick = 0;
	pInfClassObject->m_EndTick = m_EndTick.value_or(0);

	if(m_Pos2.has_value())
	{
		pInfClassObject->m_X2 = m_Pos2.value().x;
		pInfClassObject->m_Y2 = m_Pos2.value().y;
	}
	else
	{
		pInfClassObject->m_X2 = 0;
		pInfClassObject->m_Y2 = 0;
	}

	pInfClassObject->m_ProximityRadius = f2fx(GetProximityRadius());

	return pInfClassObject;
}
