/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DOOR_H
#define GAME_SERVER_ENTITIES_DOOR_H

#include <game/server/infclass/entities/infc-placed-object.h>

class CGameWorld;

class CDoor : public CPlacedObject
{
public:
	CDoor(CGameContext *pGameContext, vec2 Pos, vec2 PosTo);

	void Destroy() override;

	void Reset() override;
	void Snap(int SnappingClientId) override;

	bool IsOpen() const { return m_Open; }
	void SetOpen(bool Open);

protected:
	void SetCollisions(bool Set);
	bool m_Open = false;
};

#endif // GAME_SERVER_ENTITIES_DOOR_H
