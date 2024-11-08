/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_INFC_ENTITY_H
#define GAME_SERVER_ENTITIES_INFC_ENTITY_H

#include <game/server/entity.h>

inline constexpr int TileSize = 32;
inline constexpr float TileSizeF = 32.0f;

template <class T, int StackCapacity>
class icArray;

class CGameContext;
class CInfClassCharacter;
class CInfClassGameController;
class CInfClassPlayerClass;

using EntityFilter = bool (*)(const CEntity *);

class CInfCEntity : public CEntity
{
public:
	CInfCEntity(CGameContext *pGameContext, int ObjectType, vec2 Pos = vec2(), int Owner = -1,
	            int ProximityRadius=0);

	CInfClassGameController *GameController();
	int GetOwner() const { return m_Owner; }
	CInfClassCharacter *GetOwnerCharacter();
	CInfClassPlayerClass *GetOwnerClass();

	static EntityFilter GetOwnerFilterFunction(int Owner);
	EntityFilter GetOwnerFilterFunction();

	static EntityFilter GetExceptEntitiesFilterFunction(const icArray<const CEntity *, 10> &aEntities);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;

	void SetPos(const vec2 &Position);
	void SetAnimatedPos(const vec2 &Pivot, const vec2 &RelPosition, int PosEnv);

	vec2 GetVelocity() const { return m_Velocity; }
	void SetVelocity(vec2 Velocity) { m_Velocity = Velocity; }

	float GetLifespan() const;
	void SetLifespan(float Lifespan);
	void ResetLifespan();
	std::optional<int> GetEndTick() const { return m_EndTick; }

protected:
	virtual bool DoSnapForClient(int SnappingClient);
	void SyncPosition();

	int m_Owner = 0;
	vec2 m_Velocity{};
	std::optional<int> m_EndTick;
	vec2 m_Pivot;
	vec2 m_RelPosition;
	int m_PosEnv = -1;
};

#endif // GAME_SERVER_ENTITIES_INFC_ENTITY_H
