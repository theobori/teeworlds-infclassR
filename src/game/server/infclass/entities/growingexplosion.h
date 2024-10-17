/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef GAME_SERVER_ENTITIES_GROWINGEXPLOSION_H
#define GAME_SERVER_ENTITIES_GROWINGEXPLOSION_H

#include "infcentity.h"

#include <game/server/entity.h>
#include <game/server/entities/character.h>

#include <vector>

enum class EDamageType;
enum class TAKEDAMAGEMODE;

enum class EGrowingExplosionEffect
{
	INVALID,
	FREEZE_INFECTED,
	POISON_INFECTED,
	ELECTRIFY_INFECTED,
	LOVE_INFECTED,
	BOOM_INFECTED,
	HEAL_HUMANS,
};

class CGrowingExplosion : public CInfCEntity
{
public:
	CGrowingExplosion(CGameContext *pGameContext, vec2 Pos, vec2 Dir, int Owner, int Radius, EGrowingExplosionEffect ExplosionEffect);
	CGrowingExplosion(CGameContext *pGameContext, vec2 Pos, vec2 Dir, int Owner, int Radius, EDamageType DamageType);

	void Tick() override;
	void TickPaused() override;

	void SetDamage(int Damage);
	int GetActualDamage();

	void SetTriggeredBy(int CID);

private:
	void ProcessMercenaryBombHit(CInfClassCharacter *pCharacter);

	int m_MaxGrowing;
	int m_GrowingMap_Length;
	int m_GrowingMap_Size;
	int m_VisualizedTiles{};
	EDamageType m_DamageType;
	int m_TriggeredByCid;
	TAKEDAMAGEMODE m_TakeDamageMode;
	
	vec2 m_SeedPos;
	int m_SeedX;
	int m_SeedY;
	int m_StartTick;
	std::vector<int> m_pGrowingMap;
	std::vector<vec2> m_pGrowingMapVec;
	EGrowingExplosionEffect m_ExplosionEffect = EGrowingExplosionEffect::INVALID;
	bool m_Hit[MAX_CLIENTS];
	std::optional<int> m_Damage;
};

#endif // GAME_SERVER_ENTITIES_GROWINGEXPLOSION_H
