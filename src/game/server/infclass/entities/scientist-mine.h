/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_SCIENTIST_MINE_H
#define GAME_SERVER_ENTITIES_SCIENTIST_MINE_H

#include "infc-placed-object.h"

class CScientistMine : public CPlacedObject
{
public:
	enum
	{
		NUM_SIDE = 12,
		NUM_PARTICLES = 12,
		NUM_IDS = NUM_SIDE + NUM_PARTICLES,
	};
	
public:
	static int EntityId;

	CScientistMine(CGameContext *pGameContext, vec2 Pos, int Owner);
	~CScientistMine() override;

	void Snap(int SnappingClient) override;
	void TickPaused() override;
	void Tick() override;

	void Explode(int DetonatedBy);

private:
	int m_Ids[NUM_IDS];
	
public:
	int m_StartTick;
};

#endif
