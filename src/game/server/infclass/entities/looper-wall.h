#ifndef GAME_SERVER_ENTITIES_LOOPER_WALL_H
#define GAME_SERVER_ENTITIES_LOOPER_WALL_H

#include "infc-placed-object.h"

class CLooperWall : public CPlacedObject
{
public:
	static int EntityId;

	static constexpr int NUM_PARTICLES = 18;

public:
	CLooperWall(CGameContext *pGameContext, vec2 Pos, int Owner);
	~CLooperWall() override;

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void OnHitInfected(CInfClassCharacter *pCharacter);

	void PrepareSnapData();

	int m_Ids[2]{};
	int m_EndPointIds[2]{};
	int m_ParticleIds[NUM_PARTICLES]{};
	int m_SnapStartTick{};
};

#endif
