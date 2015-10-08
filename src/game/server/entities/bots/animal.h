/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_BOT_ANIMAL_H
#define GAME_SERVER_ENTITIES_BOT_ANIMAL_H

#include <game/server/entity.h>
#include <game/generated/server_data.h>
#include <game/generated/protocol.h>
#include <game/block_manager.h> // MineTee

#include <game/gamecore.h>
#include "../character.h"

class CAnimal : public CCharacter
{
	MACRO_ALLOC_POOL_ID()

public:
	//character's size
	static const int ms_PhysSize = 28;

	CAnimal(CGameWorld *pWorld);

	virtual void Tick();
	virtual bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon);

private:
	vec2 m_BotLastPos;
    int m_BotDir;
    int m_BotStuckCount;
    int m_BotClientIDFix;
    float m_BotLastStuckTime;
    float m_BotTimePlayerFound;
    float m_BotTimeGrounded;
    float m_BotTimeLastOption;
    float m_BotTimeLastDamage;
    float m_BotTimeLastSound;
    bool m_BotJumpTry;

	void TickBotAI();
};

#endif
