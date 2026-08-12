/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "controls.h"

#include <base/math.h>
#include <base/time.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/components/scoreboard.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/gamecore.h>

#include <algorithm>
#include <cmath>

CControls::CControls()
{
	mem_zero(&m_aLastData, sizeof(m_aLastData));
	mem_zero(m_aSnapTapAppliedDirection, sizeof(m_aSnapTapAppliedDirection));
	mem_zero(m_aSnapTapLastPressedDirection, sizeof(m_aSnapTapLastPressedDirection));
	mem_zero(m_aSnapTapLastPressedTime, sizeof(m_aSnapTapLastPressedTime));
	mem_zero(m_aSnapTapPrevLeft, sizeof(m_aSnapTapPrevLeft));
	mem_zero(m_aSnapTapPrevRight, sizeof(m_aSnapTapPrevRight));
	std::fill(std::begin(m_aMousePos), std::end(m_aMousePos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aMousePosOnAction), std::end(m_aMousePosOnAction), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aTargetPos), std::end(m_aTargetPos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aAutoAimSmoothDir), std::end(m_aAutoAimSmoothDir), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aMouseInputType), std::end(m_aMouseInputType), EMouseInputType::ABSOLUTE);
	m_AutoFollowTargetId = -1;
	m_DummyAutoHookTargetId = -1;
	std::fill(std::begin(m_aAutoHammerLastFireTick), std::end(m_aAutoHammerLastFireTick), -999999);
	std::fill(std::begin(m_aFastHammerLastFireTick), std::end(m_aFastHammerLastFireTick), -999999);
	m_AutoAimHookTargetId = -1;
}

void CControls::OnReset()
{
	ResetInput(0);
	ResetInput(1);

	for(int &AmmoCount : m_aAmmoCount)
		AmmoCount = 0;

	m_LastSendTime = 0;
	
	// Reset Auto Hammer tick counters when changing servers
	// Use -999999 so the first hammer fire is immediate on new server
	std::fill(std::begin(m_aAutoHammerLastFireTick), std::end(m_aAutoHammerLastFireTick), -999999);
	std::fill(std::begin(m_aFastHammerLastFireTick), std::end(m_aFastHammerLastFireTick), -999999);
}

void CControls::ResetInput(int Dummy)
{
	m_aLastData[Dummy].m_Direction = 0;
	// simulate releasing the fire button
	if((m_aLastData[Dummy].m_Fire & 1) != 0)
		m_aLastData[Dummy].m_Fire++;
	m_aLastData[Dummy].m_Fire &= INPUT_STATE_MASK;
	m_aLastData[Dummy].m_Jump = 0;
	m_aLastData[Dummy].m_Hook = 0;
	m_aInputData[Dummy] = m_aLastData[Dummy];

	m_aInputDirectionLeft[Dummy] = 0;
	m_aInputDirectionRight[Dummy] = 0;
	m_aSnapTapAppliedDirection[Dummy] = 0;
	m_aSnapTapLastPressedDirection[Dummy] = 0;
	m_aSnapTapLastPressedTime[Dummy] = 0;
	m_aSnapTapPrevLeft[Dummy] = 0;
	m_aSnapTapPrevRight[Dummy] = 0;
}

void CControls::OnPlayerDeath()
{
	for(int &AmmoCount : m_aAmmoCount)
		AmmoCount = 0;
}

struct CInputState
{
	CControls *m_pControls;
	int *m_apVariables[NUM_DUMMIES];
};

void CControls::ConKeyInputState(IConsole::IResult *pResult, void *pUserData)
{
	CInputState *pState = (CInputState *)pUserData;

	if(pState->m_pControls->GameClient()->m_GameInfo.m_BugDDRaceInput && pState->m_pControls->GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	*pState->m_apVariables[g_Config.m_ClDummy] = pResult->GetInteger(0);
}

void CControls::ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData)
{
	CInputState *pState = (CInputState *)pUserData;

	if((pState->m_pControls->GameClient()->m_GameInfo.m_BugDDRaceInput && pState->m_pControls->GameClient()->m_Snap.m_SpecInfo.m_Active) || pState->m_pControls->GameClient()->m_Spectator.IsActive())
		return;

	int *pVariable = pState->m_apVariables[g_Config.m_ClDummy];
	if(((*pVariable) & 1) != pResult->GetInteger(0))
		(*pVariable)++;
	*pVariable &= INPUT_STATE_MASK;
}

struct CInputSet
{
	CControls *m_pControls;
	int *m_apVariables[NUM_DUMMIES];
	int m_Value;
};

void CControls::ConKeyInputSet(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	if(pResult->GetInteger(0))
	{
		*pSet->m_apVariables[g_Config.m_ClDummy] = pSet->m_Value;
	}
}

void CControls::ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	ConKeyInputCounter(pResult, pSet);
	pSet->m_pControls->m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = 0;
}

void CControls::OnConsoleInit()
{
	// game commands
	{
		static CInputState s_State = {this, {&m_aInputDirectionLeft[0], &m_aInputDirectionLeft[1]}};
		Console()->Register("+left", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Move left");
	}
	{
		static CInputState s_State = {this, {&m_aInputDirectionRight[0], &m_aInputDirectionRight[1]}};
		Console()->Register("+right", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Move right");
	}
	{
		static CInputState s_State = {this, {&m_aInputData[0].m_Jump, &m_aInputData[1].m_Jump}};
		Console()->Register("+jump", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Jump");
	}
	{
		static CInputState s_State = {this, {&m_aInputData[0].m_Hook, &m_aInputData[1].m_Hook}};
		Console()->Register("+hook", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Hook");
	}
	{
		static CInputState s_State = {this, {&m_aInputData[0].m_Fire, &m_aInputData[1].m_Fire}};
		Console()->Register("+fire", "", CFGFLAG_CLIENT, ConKeyInputCounter, &s_State, "Fire");
	}
	{
		static CInputState s_State = {this, {&m_aShowHookColl[0], &m_aShowHookColl[1]}};
		Console()->Register("+showhookcoll", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Show Hook Collision");
	}

	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 1};
		Console()->Register("+weapon1", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to hammer");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 2};
		Console()->Register("+weapon2", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to gun");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 3};
		Console()->Register("+weapon3", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to shotgun");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 4};
		Console()->Register("+weapon4", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to grenade");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 5};
		Console()->Register("+weapon5", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to laser");
	}

	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_NextWeapon, &m_aInputData[1].m_NextWeapon}, 0};
		Console()->Register("+nextweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, &s_Set, "Switch to next weapon");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_PrevWeapon, &m_aInputData[1].m_PrevWeapon}, 0};
		Console()->Register("+prevweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, &s_Set, "Switch to previous weapon");
	}
}

void CControls::OnMessage(int Msg, void *pRawMsg)
{
	if(Msg == NETMSGTYPE_SV_WEAPONPICKUP)
	{
		CNetMsg_Sv_WeaponPickup *pMsg = (CNetMsg_Sv_WeaponPickup *)pRawMsg;
		if(g_Config.m_ClAutoswitchWeapons)
			m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = pMsg->m_Weapon + 1;
		// We don't really know ammo count, until we'll switch to that weapon, but any non-zero count will suffice here
		m_aAmmoCount[maximum(0, pMsg->m_Weapon % NUM_WEAPONS)] = 10;
	}
}

bool CControls::IsSnapTapActive() const
{
	return g_Config.m_BcSnapTap != 0 &&
		!GameClient()->IsSnapTapBlockedByCommunity();
}

bool CControls::UseGammaInputMovement() const
{
	return false;
}

void CControls::UpdateSnapTapState(int Dummy, bool LeftPressed, bool RightPressed)
{
	const int64_t Now = time_get();
	if(LeftPressed && !m_aSnapTapPrevLeft[Dummy])
	{
		m_aSnapTapLastPressedDirection[Dummy] = -1;
		m_aSnapTapLastPressedTime[Dummy] = Now;
	}
	if(RightPressed && !m_aSnapTapPrevRight[Dummy])
	{
		m_aSnapTapLastPressedDirection[Dummy] = 1;
		m_aSnapTapLastPressedTime[Dummy] = Now;
	}

	m_aSnapTapPrevLeft[Dummy] = LeftPressed ? 1 : 0;
	m_aSnapTapPrevRight[Dummy] = RightPressed ? 1 : 0;
}

int CControls::ResolveMovementDirection(int Dummy, bool LeftPressed, bool RightPressed)
{
	UpdateSnapTapState(Dummy, LeftPressed, RightPressed);

	if(IsSnapTapActive() || !UseGammaInputMovement())
		return ResolveSnapTapDirection(Dummy, LeftPressed, RightPressed);

	int Direction = 0;
	if(LeftPressed && !RightPressed)
		Direction = -1;
	if(!LeftPressed && RightPressed)
		Direction = 1;
	return Direction;
}
int CControls::ResolveSnapTapDirection(int Dummy, bool LeftPressed, bool RightPressed)
{
	if(LeftPressed == RightPressed)
	{
		if(!LeftPressed)
		{
			m_aSnapTapAppliedDirection[Dummy] = 0;
			return 0;
		}

		if(!IsSnapTapActive())
			return 0;

		int CandidateDirection = m_aSnapTapLastPressedDirection[Dummy];
		if(CandidateDirection != -1 && CandidateDirection != 1)
			CandidateDirection = m_aSnapTapAppliedDirection[Dummy] != 0 ? m_aSnapTapAppliedDirection[Dummy] : -1;

		if(m_aSnapTapAppliedDirection[Dummy] == 0)
		{
			m_aSnapTapAppliedDirection[Dummy] = CandidateDirection;
		}
		else if(m_aSnapTapAppliedDirection[Dummy] != CandidateDirection)
		{
			const int64_t Delay = (time_freq() * (int64_t)g_Config.m_BcSnapTapDelay) / 1000;
			if(time_get() - m_aSnapTapLastPressedTime[Dummy] >= Delay)
				m_aSnapTapAppliedDirection[Dummy] = CandidateDirection;
		}

		return m_aSnapTapAppliedDirection[Dummy];
	}

	m_aSnapTapAppliedDirection[Dummy] = LeftPressed ? -1 : 1;
	return m_aSnapTapAppliedDirection[Dummy];
}

void CControls::GoresMode()
{
	// if turning off kog mode and it was on before, rebind to previous bind
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_BcGoresMode || GameClient()->m_BestClient.IsComponentDisabled(CBestClient::COMPONENT_GAMEPLAY_GORES_MODE))
		return;

	CCharacterCore Core = GameClient()->m_PredictedPrevChar;

	if(g_Config.m_BcGoresModeDisableIfWeapons)
	{
		if(Core.m_aWeapons[WEAPON_GRENADE].m_Got || Core.m_aWeapons[WEAPON_LASER].m_Got || Core.m_aWeapons[WEAPON_SHOTGUN].m_Got)
			m_WeaponsGot = true;
		if((!Core.m_aWeapons[WEAPON_GRENADE].m_Got && !Core.m_aWeapons[WEAPON_LASER].m_Got && !Core.m_aWeapons[WEAPON_SHOTGUN].m_Got) && m_WeaponsGot)
			m_WeaponsGot = false;

		if(m_WeaponsGot)
			return;
	}

	if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == 0)
		GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = WEAPON_GUN + 1;
}

int CControls::SnapInput(int *pData)
{
	GoresMode();

	// update player state
	if(GameClient()->m_Chat.IsActive())
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags = PLAYERFLAG_CHATTING;
	else if(GameClient()->m_Menus.IsActive())
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags = PLAYERFLAG_IN_MENU;
	else
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags = PLAYERFLAG_PLAYING;

	if(GameClient()->m_Scoreboard.IsActive())
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags |= PLAYERFLAG_SCOREBOARD;

	if(Client()->ServerCapAnyPlayerFlag() && GameClient()->m_Controls.m_aShowHookColl[g_Config.m_ClDummy])
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags |= PLAYERFLAG_AIM;

	if(Client()->ServerCapAnyPlayerFlag() && GameClient()->m_Camera.CamType() == CCamera::CAMTYPE_SPEC)
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags |= PLAYERFLAG_SPEC_CAM;

	switch(m_aMouseInputType[g_Config.m_ClDummy])
	{
	case CControls::EMouseInputType::AUTOMATED:
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags |= PLAYERFLAG_INPUT_ABSOLUTE;
		break;
	case CControls::EMouseInputType::ABSOLUTE:
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags |= PLAYERFLAG_INPUT_ABSOLUTE | PLAYERFLAG_INPUT_MANUAL;
		break;
	case CControls::EMouseInputType::RELATIVE:
		m_aInputData[g_Config.m_ClDummy].m_PlayerFlags |= PLAYERFLAG_INPUT_MANUAL;
		break;
	}

	// TClient
	if(g_Config.m_TcHideChatBubbles && Client()->RconAuthed())
		for(auto &InputData : m_aInputData)
			InputData.m_PlayerFlags &= ~PLAYERFLAG_CHATTING;

	if(g_Config.m_BcSilentTyping)
		for(auto &InputData : m_aInputData)
			InputData.m_PlayerFlags &= ~PLAYERFLAG_CHATTING;

	if(g_Config.m_TcNameplatePingCircle)
		for(auto &InputData : m_aInputData)
			InputData.m_PlayerFlags |= PLAYERFLAG_SCOREBOARD;

	bool Send = m_aLastData[g_Config.m_ClDummy].m_PlayerFlags != m_aInputData[g_Config.m_ClDummy].m_PlayerFlags;

	m_aLastData[g_Config.m_ClDummy].m_PlayerFlags = m_aInputData[g_Config.m_ClDummy].m_PlayerFlags;

	// we freeze the input if chat or menu is activated
	if(!(m_aInputData[g_Config.m_ClDummy].m_PlayerFlags & PLAYERFLAG_PLAYING))
	{
		if(!GameClient()->m_GameInfo.m_BugDDRaceInput)
			ResetInput(g_Config.m_ClDummy);

		mem_copy(pData, &m_aInputData[g_Config.m_ClDummy], sizeof(m_aInputData[0]));

		// set the target anyway though so that we can keep seeing our surroundings,
		// even if chat or menu are activated
		vec2 Pos = GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];
		if(g_Config.m_TcScaleMouseDistance && !GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			const int MaxDistance = g_Config.m_ClDyncam ? g_Config.m_ClDyncamMaxDistance : g_Config.m_ClMouseMaxDistance;
			if(MaxDistance > 5 && MaxDistance < 1000) // Don't scale if angle bind or reduces precision
				Pos *= 1000.0f / (float)MaxDistance;
		}
		m_aInputData[g_Config.m_ClDummy].m_TargetX = (int)Pos.x;
		m_aInputData[g_Config.m_ClDummy].m_TargetY = (int)Pos.y;

		if(!m_aInputData[g_Config.m_ClDummy].m_TargetX && !m_aInputData[g_Config.m_ClDummy].m_TargetY)
			m_aInputData[g_Config.m_ClDummy].m_TargetX = 1;

		// send once a second just to be sure
		Send = Send || time_get() > m_LastSendTime + time_freq();
	}
	else
	{
		// TClient
		vec2 Pos;
		if(g_Config.m_ClSubTickAiming && m_aMousePosOnAction[g_Config.m_ClDummy] != vec2(0.0f, 0.0f))
		{
			Pos = GameClient()->m_Controls.m_aMousePosOnAction[g_Config.m_ClDummy];
			m_aMousePosOnAction[g_Config.m_ClDummy] = vec2(0.0f, 0.0f);
		}
		else
			Pos = GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];

		m_FastInputHookAction = false;
		m_FastInputFireAction = false;

		if(g_Config.m_TcScaleMouseDistance && !GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			const int MaxDistance = g_Config.m_ClDyncam ? g_Config.m_ClDyncamMaxDistance : g_Config.m_ClMouseMaxDistance;
			if(MaxDistance > 5 && MaxDistance < 1000) // Don't scale if angle bind or reduces precision
				Pos *= 1000.0f / (float)MaxDistance;
		}
		m_aInputData[g_Config.m_ClDummy].m_TargetX = (int)Pos.x;
		m_aInputData[g_Config.m_ClDummy].m_TargetY = (int)Pos.y;

		if(!m_aInputData[g_Config.m_ClDummy].m_TargetX && !m_aInputData[g_Config.m_ClDummy].m_TargetY)
			m_aInputData[g_Config.m_ClDummy].m_TargetX = 1;

		// set direction
		const bool LeftPressed = m_aInputDirectionLeft[g_Config.m_ClDummy] != 0;
		const bool RightPressed = m_aInputDirectionRight[g_Config.m_ClDummy] != 0;
		m_aInputData[g_Config.m_ClDummy].m_Direction = ResolveMovementDirection(g_Config.m_ClDummy, LeftPressed, RightPressed);

		// Auto aim hook to nearest player.
		// Antiping-aware: when player prediction is active the hook leads the tee (intercept point
		// from its predicted velocity) instead of aiming at its current position, so it hooks ahead
		// of where the tee is flying. Any tee whose hook line is blocked by tiles is skipped, so the
		// helper falls through to the nearest tee that is actually reachable. If no tee is reachable,
		// the aim is left untouched (you hook wherever you are looking, no auto-aim).
		// Target selection (and the Visuals highlight) run continuously while cl_auto_aim is on, so
		// the AimHelper Visuals block can show who is about to get hooked before the hook button is
		// even pressed; only the actual input redirection below is gated on the hook being held.
		if(g_Config.m_ClAutoAim)
		{
			int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
			if(LocalClientId >= 0 && GameClient()->m_Snap.m_pLocalCharacter)
			{
				vec2 LocalPos = GameClient()->m_LocalCharacterPos;
				vec2 AimDir = normalize(vec2(m_aInputData[g_Config.m_ClDummy].m_TargetX, m_aInputData[g_Config.m_ClDummy].m_TargetY));

				const bool UseAntiPing = GameClient()->AntiPingPlayers();

				float MinDistance = -1.0f;
				vec2 BestTarget;
				int BestTargetId = -1;
				bool FoundTarget = false;

				// Frozen-friend priority tier: when ddk_hook_friend_priority is on, any frozen
				// friend that is in range/cone/reachable wins over the normal nearest tee, so the
				// hook goes to unfreeze a teammate first.
				float MinFriendDistance = -1.0f;
				vec2 BestFriendTarget;
				int BestFriendTargetId = -1;
				bool FoundFriendTarget = false;

				const float MaxHookRange = ms_AutoAimHookRange;
				const float MaxAngle = (g_Config.m_ClAutoAimFov / 2.0f) * pi / 180.0f;

				// Sweeps points around a tee's hitbox to find one our hook line can reach without
				// crossing solid tiles. The server grabs a hook as soon as its line's closest approach
				// to the tee comes within PhysicalSize+2px (see CCharacterCore::Move), so landing the
				// ray anywhere on the hitbox boundary is enough — not just the center. The sweep starts
				// at the point facing straight at `From` (offset 0, the closest possible part of the
				// tee) and alternates outward left/right around the circle, so the first reachable point
				// found is always the nearest possible part of the tee, letting the hook reach around a
				// corner block that only covers the tee's near side.
				// Two solid tiles placed diagonally opposite each other in a 2x2 block (e.g. top-left
				// and bottom-right solid, the other diagonal empty) only truly touch at one corner
				// point, leaving a pixel-precise gap a hook can thread through even though the angular
				// sweep above won't find it (it only tries offsets around the tee's hitbox, not along
				// the blocking tiles). Aims THROUGH the gap rather than at it: the candidate direction
				// is extended past the corner and only accepted if it also comes within hook-grab
				// range of the tee on the other side, so the hook actually reaches the target instead
				// of just poking into the empty tile next to the corner.
				auto FindDiagonalHookGap = [this](vec2 From, vec2 To, vec2 *pOutRelative) -> bool {
					const int Tile = 32;
					const int MinX = std::min(From.x, To.x) / Tile - 1;
					const int MaxX = std::max(From.x, To.x) / Tile + 1;
					const int MinY = std::min(From.y, To.y) / Tile - 1;
					const int MaxY = std::max(From.y, To.y) / Tile + 1;

					const float GrabRadius = CCharacterCore::PhysicalSize();
					const float ToDist = length(To - From);

					struct SCorner { int i, j; float DistSq; };
					static SCorner s_aCorners[200];
					int NumCandidates = 0;
					for(int i = MinX; i <= MaxX && NumCandidates < 200; i++)
					{
						for(int j = MinY; j <= MaxY && NumCandidates < 200; j++)
						{
							const float dx = (float)(i * Tile) - From.x;
							const float dy = (float)(j * Tile) - From.y;
							s_aCorners[NumCandidates++] = {i, j, dx * dx + dy * dy};
						}
					}

					// Nearest corners first so the closest usable gap wins.
					std::sort(s_aCorners, s_aCorners + NumCandidates, [](const SCorner &a, const SCorner &b) { return a.DistSq < b.DistSq; });

					for(int k = 0; k < NumCandidates; k++)
					{
						const float Cx = (float)(s_aCorners[k].i * Tile);
						const float Cy = (float)(s_aCorners[k].j * Tile);

						const bool Tl = Collision()->IsSolid((int)(Cx - Tile / 2), (int)(Cy - Tile / 2));
						const bool Tr = Collision()->IsSolid((int)(Cx + Tile / 2), (int)(Cy - Tile / 2));
						const bool Bl = Collision()->IsSolid((int)(Cx - Tile / 2), (int)(Cy + Tile / 2));
						const bool Br = Collision()->IsSolid((int)(Cx + Tile / 2), (int)(Cy + Tile / 2));

						const bool DiagNwSe = Tl && Br && !Tr && !Bl;
						const bool DiagNeSw = Tr && Bl && !Tl && !Br;
						if(!DiagNwSe && !DiagNeSw)
							continue;

						// Walk the offset from the corner outward, pixel by pixel, on both sides of the
						// gap. Neighboring solid tiles beyond this immediate 2x2 block can narrow the
						// actually-usable gap well below a full tile, so the nearest working pixel to
						// the corner isn't always a fixed distance away — keep trying until one clears.
						const float MaxOffset = 14.0f;
						for(float Off = 1.0f; Off <= MaxOffset; Off += 1.0f)
						{
							vec2 aGapPoints[2];
							if(DiagNwSe)
							{
								aGapPoints[0] = vec2(Cx + Off, Cy - Off);
								aGapPoints[1] = vec2(Cx - Off, Cy + Off);
							}
							else
							{
								aGapPoints[0] = vec2(Cx - Off, Cy - Off);
								aGapPoints[1] = vec2(Cx + Off, Cy + Off);
							}

							for(vec2 GapPoint : aGapPoints)
							{
								const vec2 GapVec = GapPoint - From;
								const float GapDist = length(GapVec);
								if(GapDist < 1.0f)
									continue;
								const vec2 Dir = GapVec / GapDist;

								// Where along this ray does it pass closest to the tee, and how close does
								// it get? The gap has to lie before that point, not past it or behind us.
								const float ClosestT = dot(To - From, Dir);
								if(ClosestT < GapDist)
									continue;
								const vec2 ClosestPoint = From + Dir * ClosestT;
								if(distance(ClosestPoint, To) > GrabRadius)
									continue;

								// Aim a bit past the closest-approach point so the hook's flight line
								// actually reaches in that close to the tee, not just up to the gap.
								const vec2 AimPoint = From + Dir * std::min(ClosestT + GrabRadius, ToDist + GrabRadius);
								if(!Collision()->IntersectLineTeleHook(From, AimPoint, nullptr, nullptr))
								{
									*pOutRelative = AimPoint - From;
									return true;
								}
							}
						}
					}
					return false;
				};

				auto FindHookableHitboxPoint = [this, &FindDiagonalHookGap](vec2 From, vec2 Center, vec2 *pOutRelative) -> bool {
					const vec2 ToCenter = Center - From;
					const float CenterDist = length(ToCenter);
					// Too close for a hitbox-radius offset to make sense; aim straight at the center.
					if(CenterDist <= CCharacterCore::PhysicalSize())
					{
						if(Collision()->IntersectLineTeleHook(From, Center, nullptr, nullptr))
							return false;
						*pOutRelative = ToCenter;
						return true;
					}

					// Edge Scan off: just try a straight shot to the center at any range, no
					// hitbox-edge sweep or diagonal-gap search. Without this, turning Edge Scan off
					// would leave only the "too close" case above able to find a target, so the hook
					// would stop aiming almost entirely instead of falling back to plain center-aim.
					if(!g_Config.m_DdkHookEdgeScan)
					{
						if(Collision()->IntersectLineTeleHook(From, Center, nullptr, nullptr))
							return false;
						*pOutRelative = ToCenter;
						return true;
					}

					const vec2 NearDir = ToCenter / CenterDist;
					const float Radius = CCharacterCore::PhysicalSize();
					static const float s_aSweepDeg[] = {0.0f, 15.0f, -15.0f, 30.0f, -30.0f, 45.0f, -45.0f,
						60.0f, -60.0f, 90.0f, -90.0f, 120.0f, -120.0f, 150.0f, -150.0f, 180.0f};

					for(float OffsetDeg : s_aSweepDeg)
					{
						const vec2 EdgeDir = OffsetDeg == 0.0f ? NearDir : rotate(NearDir, OffsetDeg);
						const vec2 Candidate = Center - EdgeDir * Radius;
						if(!Collision()->IntersectLineTeleHook(From, Candidate, nullptr, nullptr))
						{
							*pOutRelative = Candidate - From;
							return true;
						}
					}

					// Diagonal gap: two solid tiles touching only at one corner (e.g. top-left +
					// bottom-right solid) leave a pixel-precise gap the sweep above won't find, since
					// it only tries offsets around the tee's hitbox, not along the blocking tiles.
					return FindDiagonalHookGap(From, Center, pOutRelative);
				};

				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(i == LocalClientId)
						continue;
					if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
						continue;

					// Friend/frozen based filtering for the DDK AimHelper toggles.
					const bool IsFriend = GameClient()->m_aClients[i].m_Friend;
					const bool IsFrozen = GameClient()->m_aClients[i].m_FreezeEnd > 0 || GameClient()->m_aClients[i].m_DeepFrozen;
					// A frozen friend that priority mode wants to grab (to unfreeze them). This
					// overrides "don't hook friends" so teammates can still be pulled out of freeze.
					const bool FrozenFriendPriority = g_Config.m_DdkHookFriendPriority && IsFriend && IsFrozen;
					if(g_Config.m_DdkHookFriendsOnly && !IsFriend)
						continue;
					if(g_Config.m_DdkHookNoFriends && IsFriend && !FrozenFriendPriority)
						continue;

					// Tee position/velocity: use the antiping prediction when available so the hook can
					// lead a flying tee; otherwise fall back to the raw snapshot position (no lead).
					vec2 TargetPos;
					vec2 TargetVel = vec2(0.0f, 0.0f);
					if(UseAntiPing)
					{
						TargetPos = GameClient()->m_aClients[i].m_Predicted.m_Pos;
						TargetVel = GameClient()->m_aClients[i].m_Predicted.m_Vel;
					}
					else
					{
						const CNetObj_Character &Char = GameClient()->m_Snap.m_aCharacters[i].m_Cur;
						TargetPos = vec2(Char.m_X, Char.m_Y);
					}

					vec2 ToTarget = TargetPos - LocalPos;
					float Distance = length(ToTarget);
					if(Distance > MaxHookRange || Distance < 1.0f)
						continue;

					// FOV cone check against the tee's current position.
					vec2 ToTargetNorm = normalize(ToTarget);
					float DotProduct = dot(AimDir, ToTargetNorm);
					float Angle = std::acos(std::clamp(DotProduct, -1.0f, 1.0f));
					if(Angle > MaxAngle)
						continue;

					// Lead the hook: estimate how many ticks the hook needs to reach the tee and advance
					// the tee along its predicted velocity by that much. The hook grows by HookFireSpeed
					// pixels per tick; a few iterations converge on the intercept point.
					vec2 AimPos = TargetPos;
					if(UseAntiPing)
					{
						const float HookFireSpeed = (float)GameClient()->m_aClients[i].m_Predicted.m_Tuning.m_HookFireSpeed;
						if(HookFireSpeed > 0.0f)
						{
							for(int It = 0; It < 3; ++It)
							{
								const float TravelTicks = distance(LocalPos, AimPos) / HookFireSpeed;
								AimPos = TargetPos + TargetVel * TravelTicks;
							}
						}
					}

					// Reachability: sweep points around the tee's hitbox for one our hook line can reach
					// without crossing solid tiles, starting from the point nearest to us. The server
					// grabs a hook as soon as its line comes within PhysicalSize+2px of the tee, so any
					// point on the hitbox boundary works — this lets us hook around a corner block that
					// only covers the near side of the tee, instead of giving up on it entirely. If
					// nothing on the hitbox is reachable, this tee can't be hooked at all — skip it so a
					// further but reachable tee is chosen instead.
					vec2 EdgeTarget;
					if(!FindHookableHitboxPoint(LocalPos, AimPos, &EdgeTarget))
						continue;

					if(MinDistance < 0.0f || Distance < MinDistance)
					{
						MinDistance = Distance;
						BestTarget = EdgeTarget;
						BestTargetId = i;
						FoundTarget = true;
					}
					if(FrozenFriendPriority && (MinFriendDistance < 0.0f || Distance < MinFriendDistance))
					{
						MinFriendDistance = Distance;
						BestFriendTarget = EdgeTarget;
						BestFriendTargetId = i;
						FoundFriendTarget = true;
					}
				}
				// Frozen friends win over the plain nearest tee when priority mode is enabled.
				bool HaveTarget = FoundFriendTarget || FoundTarget;
				vec2 ChosenTarget = FoundFriendTarget ? BestFriendTarget : BestTarget;
				// DDK AimHelper Visuals: remember which tee is actually being targeted this tick
				// so CPlayers can outline/fill its hitbox.
				m_AutoAimHookTargetId = HaveTarget ? (FoundFriendTarget ? BestFriendTargetId : BestTargetId) : -1;

				// Track hook-button rising edges so Silent mode (below) can tell a fresh press from
				// a continued hold, regardless of whether a target is currently available.
				const int Dummy = g_Config.m_ClDummy;
				const bool HookHeldNow = m_aInputData[Dummy].m_Hook != 0;
				const bool HookJustPressed = HookHeldNow && !m_aAutoAimHookWasHeld[Dummy];
				m_aAutoAimHookWasHeld[Dummy] = HookHeldNow;

				// Input redirection only applies while the hook button is actually held, so aiming
				// around normally never gets your view snapped; the Visuals highlight above already
				// reflects the target regardless of hook state.
				if(HaveTarget && HookHeldNow)
				{
					const float ChosenLen = length(ChosenTarget);

					// Accuracy: rotate the aim vector by a random angle that grows as accuracy drops,
					// so a lower setting visibly lowers the effective hit rate instead of aiming perfectly.
					// rotate() takes degrees (it converts to radians internally).
					if(g_Config.m_DdkHookAccuracy < 100 && ChosenLen > 0.0f)
					{
						const float MaxJitterDeg = (100 - g_Config.m_DdkHookAccuracy) * 0.25f;
						const float JitterDeg = random_float(-MaxJitterDeg, MaxJitterDeg);
						ChosenTarget = rotate(ChosenTarget, JitterDeg);
					}

					// Silent: turn toward the (possibly jittered) target by a limited rate per tick
					// instead of snapping instantly, so the hook looks like a legitimate flick.
					if(g_Config.m_DdkHookSilent)
					{
						vec2 &SmoothDir = m_aAutoAimSmoothDir[Dummy];
						// A fresh hook press always snaps straight to the target instead of turning
						// from whatever direction was left over from a previous, unrelated press -
						// otherwise the first tick or two of every press aims short of the real
						// target and the hook misses.
						if((SmoothDir.x == 0.0f && SmoothDir.y == 0.0f) || HookJustPressed)
						{
							SmoothDir = ChosenTarget;
						}
						else
						{
							const float SilentTurnDegPerTick = 45.0f;
							const float MaxTurn = SilentTurnDegPerTick * pi / 180.0f;
							const float FromAngle = angle(SmoothDir);
							const float ToAngle = angle(ChosenTarget);
							float DeltaAngle = std::fmod(ToAngle - FromAngle + pi * 3.0f, pi * 2.0f) - pi;
							DeltaAngle = std::clamp(DeltaAngle, -MaxTurn, MaxTurn);
							const float NewLen = length(ChosenTarget);
							SmoothDir = direction(FromAngle + DeltaAngle) * (NewLen > 0.0f ? NewLen : ChosenLen);
						}
						ChosenTarget = SmoothDir;

						// Stretch the aim vector out to the normal max mouse-reach distance instead of
						// stopping exactly at the tee. Only the angle matters for aiming/hooking, so
						// this makes the sent cursor position look like an ordinary full-reach mouse
						// aim instead of a suspiciously short vector landing exactly on the target.
						const float SilentLen = length(ChosenTarget);
						if(SilentLen > 0.0f)
							ChosenTarget = ChosenTarget / SilentLen * GetMaxMouseDistance();
					}
					else
					{
						// Not using Silent: keep the smoothing state cleared so re-enabling it later
						// starts fresh instead of resuming a stale sweep.
						m_aAutoAimSmoothDir[Dummy] = vec2(0.0f, 0.0f);
					}

					m_aInputData[Dummy].m_TargetX = (int)ChosenTarget.x;
					m_aInputData[Dummy].m_TargetY = (int)ChosenTarget.y;
				}
				// A (0,0) target is interpreted by the game as aiming straight up; nudge X so the
				// hook keeps pointing at the tee instead of snapping vertical. Only applies while
				// actually redirecting input, same gating as the block above.
				if(HaveTarget && m_aInputData[g_Config.m_ClDummy].m_Hook &&
					!m_aInputData[g_Config.m_ClDummy].m_TargetX && !m_aInputData[g_Config.m_ClDummy].m_TargetY)
					m_aInputData[g_Config.m_ClDummy].m_TargetX = 1;
			}
			else
			{
				// No local character to aim from: nothing to highlight.
				m_AutoAimHookTargetId = -1;
			}
		}
		else
		{
			// Auto aim off: no target is being actively selected or highlighted.
			m_AutoAimHookTargetId = -1;
		}

		// Fast Hammer: while the player is physically holding fire themselves, hit fast at their own
		// aim - this takes priority over DDK Auto Hammer's targeting below so a manual swing never
		// gets redirected onto some other enemy mid-press. Releasing fire hands control back to Auto
		// Hammer's own targeting (if enabled), which finds and swings at nearby enemies per its own
		// settings below - it just never fires blindly at empty air with no target in range.
		bool DdkHammerFireHeldManually = (m_aInputData[g_Config.m_ClDummy].m_Fire & 1) != 0 &&
			GameClient()->m_Snap.m_pLocalCharacter &&
			GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_HAMMER;

		if(g_Config.m_DdkAutoHammerFast && DdkHammerFireHeldManually)
		{
			// Just re-trigger a fresh counted press every tick at the current aim. The hammer isn't
			// full-auto server-side, so a plain hold would otherwise only swing once (see
			// HandleWeapons/CountInput). Bumping the fire counter by 2 preserves the held parity
			// while registering exactly one fresh press per tick.
			// Use the predicted tick (not the last-acked snapshot tick): this function runs once per
			// predicted-tick advance, but the acked tick only moves when a snapshot actually arrives,
			// so any latency hiccup would stall it for multiple predicted ticks and silently drop
			// swings.
			const int Now = Client()->PredGameTick(g_Config.m_ClDummy);
			if(Now - m_aFastHammerLastFireTick[g_Config.m_ClDummy] >= 1)
			{
				m_aInputData[g_Config.m_ClDummy].m_Fire = (m_aInputData[g_Config.m_ClDummy].m_Fire + 2) & INPUT_STATE_MASK;
				m_aFastHammerLastFireTick[g_Config.m_ClDummy] = Now;
			}
		}
		// DDK Auto Hammer: hammer the nearest enemy within melee reach. Friend/frozen filtering is
		// configurable. The targeted swing (which redirects aim and switches to hammer) only runs
		// while the hook is NOT held, so it never disturbs a hook. Runs whenever fire isn't being
		// held manually (or Fast Hammer is off), so releasing fire falls back to auto-targeting.
		else if(g_Config.m_DdkAutoHammer)
		{
			int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
			if(LocalClientId >= 0 && GameClient()->m_Snap.m_pLocalCharacter)
			{
				vec2 LocalPos = GameClient()->m_LocalCharacterPos;
				float MinDistance = -1.0f;
				vec2 BestTarget;
				bool FoundTarget = false;

				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(i == LocalClientId)
						continue;
					if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
						continue;
					// Friend/frozen filtering is configurable: by default skip friends and don't waste
					// hits on already-frozen tees, but either can be turned off in the DDK menu.
					if(!g_Config.m_DdkAutoHammerHitFriends && GameClient()->m_aClients[i].m_Friend)
						continue;
					if(g_Config.m_DdkAutoHammerSkipFrozen &&
						(GameClient()->m_aClients[i].m_FreezeEnd > 0 || GameClient()->m_aClients[i].m_DeepFrozen))
						continue;

					const CNetObj_Character &Char = GameClient()->m_Snap.m_aCharacters[i].m_Cur;
					vec2 TargetPos = vec2(Char.m_X, Char.m_Y);
					vec2 ToTarget = TargetPos - LocalPos;
					float Distance = length(ToTarget);
					if(Distance > (float)g_Config.m_DdkAutoHammerRange || Distance < 1.0f)
						continue;
					// A wall between us means the hammer can't reach the tee.
					if(Collision()->IntersectLine(LocalPos, TargetPos, nullptr, nullptr))
						continue;

					if(MinDistance < 0.0f || Distance < MinDistance)
					{
						MinDistance = Distance;
						BestTarget = ToTarget;
						FoundTarget = true;
					}
				}

				if(FoundTarget && !m_aInputData[g_Config.m_ClDummy].m_Hook)
				{
					// Aim at the enemy and switch to hammer so the swing connects.
					m_aInputData[g_Config.m_ClDummy].m_TargetX = (int)BestTarget.x;
					m_aInputData[g_Config.m_ClDummy].m_TargetY = (int)BestTarget.y;
					if(!m_aInputData[g_Config.m_ClDummy].m_TargetX && !m_aInputData[g_Config.m_ClDummy].m_TargetY)
						m_aInputData[g_Config.m_ClDummy].m_TargetX = 1;
					m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = WEAPON_HAMMER + 1;

					// Rate-limit swings to roughly the server hammer fire delay so we don't spam.
					// Hammer is not full-auto, so each swing needs a fresh counted press. Bumping the
					// fire counter by 2 registers exactly one press (see CountInput) while preserving
					// parity, so it never corrupts the real fire-key state. Only fire once the weapon
					// has actually switched to hammer.
					// Fast hammer: once a real target is found in range, bump one counted press every
					// tick for maximum spam against it; otherwise rate-limit to the server fire delay.
					// (There's no fallback for "no target found" - that used to fire blindly every
					// tick regardless of whether fire was held, which is exactly the bug this fixes.)
					// Use the predicted tick, same reasoning as Fast Hammer above: the acked snapshot
					// tick can stall across latency hiccups and silently starve the swing cooldown.
					const int HammerCooldownTicks = g_Config.m_DdkAutoHammerFast ? 1 : maximum(1, Client()->GameTickSpeed() / 8);
					const int Now = Client()->PredGameTick(g_Config.m_ClDummy);
					if(Now - m_aAutoHammerLastFireTick[g_Config.m_ClDummy] >= HammerCooldownTicks &&
						GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_HAMMER)
					{
						m_aInputData[g_Config.m_ClDummy].m_Fire = (m_aInputData[g_Config.m_ClDummy].m_Fire + 2) & INPUT_STATE_MASK;
						m_aAutoHammerLastFireTick[g_Config.m_ClDummy] = Now;
					}
				}
			}
		}

		// Auto follow nearest tee
		if(g_Config.m_ClAutoFollow)
		{
			int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
			if(LocalClientId >= 0)
			{
				vec2 LocalPos = GameClient()->m_LocalCharacterPos;
				float MinDistance = -1.0f;
				int NearestId = -1;

				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(i == LocalClientId)
						continue;
					if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
						continue;
					const CNetObj_Character &Char = GameClient()->m_Snap.m_aCharacters[i].m_Cur;
					const CNetObj_Character &PrevChar = GameClient()->m_Snap.m_aCharacters[i].m_Prev;
					vec2 OtherPos = mix(
						vec2(PrevChar.m_X, PrevChar.m_Y),
						vec2(Char.m_X, Char.m_Y),
						Client()->IntraGameTick(g_Config.m_ClDummy));
					float Distance = distance(LocalPos, OtherPos);
					if(MinDistance < 0.0f || Distance < MinDistance)
					{
						MinDistance = Distance;
						NearestId = i;
					}
				}
				if(NearestId >= 0)
				{
					m_AutoFollowTargetId = NearestId;
					const CNetObj_Character &TargetChar = GameClient()->m_Snap.m_aCharacters[NearestId].m_Cur;
					const CNetObj_Character &TargetPrevChar = GameClient()->m_Snap.m_aCharacters[NearestId].m_Prev;
					vec2 TargetPos = mix(
						vec2(TargetPrevChar.m_X, TargetPrevChar.m_Y),
						vec2(TargetChar.m_X, TargetChar.m_Y),
						Client()->IntraGameTick(g_Config.m_ClDummy));
					float XDiff = TargetPos.x - LocalPos.x;
					float YDiff = TargetPos.y - LocalPos.y;
					const float AlignmentThreshold = 0.03f;
					if(std::abs(XDiff) > AlignmentThreshold)
						m_aInputData[g_Config.m_ClDummy].m_Direction = (XDiff > 0) ? 1 : -1;
					else
						m_aInputData[g_Config.m_ClDummy].m_Direction = 0;
					if(YDiff < -32.0f && std::abs(XDiff) <= AlignmentThreshold * 10.0f)
						m_aInputData[g_Config.m_ClDummy].m_Jump = 1;
				}
			}
		}
		else
		{
			m_AutoFollowTargetId = -1;
		}

		// dummy copy moves
		if(g_Config.m_ClDummyCopyMoves || g_Config.m_ClDummyCopyMovesWithHammer)
		{
			CNetObj_PlayerInput *pDummyInput = &GameClient()->m_DummyInput;

			// Don't copy any input to dummy when spectating others
			if(!GameClient()->m_Snap.m_SpecInfo.m_Active || GameClient()->m_Snap.m_SpecInfo.m_SpectatorId < 0)
			{
				pDummyInput->m_Direction = m_aInputData[g_Config.m_ClDummy].m_Direction;
				if(g_Config.m_ClDummyCopyMoves)
				{
					pDummyInput->m_Hook = m_aInputData[g_Config.m_ClDummy].m_Hook;
					pDummyInput->m_Jump = m_aInputData[g_Config.m_ClDummy].m_Jump;
				}
				else if(g_Config.m_ClDummyCopyMovesWithHammer)
				{
					pDummyInput->m_Jump = m_aInputData[g_Config.m_ClDummy].m_Jump;
				}

				// Auto hook nearest tee (only with old copy moves enabled)
				if(g_Config.m_ClDummyAutoHook && m_aInputData[g_Config.m_ClDummy].m_Hook && g_Config.m_ClDummyCopyMoves)
				{
					int DummyId = GameClient()->m_aLocalIds[!g_Config.m_ClDummy];
					int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
					if(DummyId >= 0 && LocalClientId >= 0)
					{
						if(GameClient()->m_Snap.m_aCharacters[DummyId].m_Active)
						{
							const CNetObj_Character &DummyChar = GameClient()->m_Snap.m_aCharacters[DummyId].m_Cur;
							vec2 DummyPos = vec2(DummyChar.m_X, DummyChar.m_Y);
							const float MaxHookRange = 375.0f;
							if(m_DummyAutoHookTargetId < 0)
							{
								float MinDistance = -1.0f;
								int BestTargetId = -1;
								for(int i = 0; i < MAX_CLIENTS; i++)
								{
									if(i == DummyId || i == LocalClientId)
										continue;
									if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
										continue;
									const CNetObj_Character &OtherChar = GameClient()->m_Snap.m_aCharacters[i].m_Cur;
									vec2 OtherPos = vec2(OtherChar.m_X, OtherChar.m_Y);
									float Distance = distance(DummyPos, OtherPos);
									if(Distance <= MaxHookRange && (MinDistance < 0.0f || Distance < MinDistance))
									{
										MinDistance = Distance;
										BestTargetId = i;
									}
								}
								m_DummyAutoHookTargetId = BestTargetId;
							}
							if(m_DummyAutoHookTargetId >= 0 && m_DummyAutoHookTargetId < MAX_CLIENTS)
							{
								if(GameClient()->m_Snap.m_aCharacters[m_DummyAutoHookTargetId].m_Active)
								{
									const CNetObj_Character &TargetChar = GameClient()->m_Snap.m_aCharacters[m_DummyAutoHookTargetId].m_Cur;
									vec2 TargetPos = vec2(TargetChar.m_X, TargetChar.m_Y);
									float Distance = distance(DummyPos, TargetPos);
									if(Distance <= MaxHookRange)
									{
										pDummyInput->m_Hook = 1;
										vec2 HookDir = TargetPos - DummyPos;
										pDummyInput->m_TargetX = (int)HookDir.x;
										pDummyInput->m_TargetY = (int)HookDir.y;
									}
									else
									{
										m_DummyAutoHookTargetId = -1;
										pDummyInput->m_Hook = m_aInputData[g_Config.m_ClDummy].m_Hook;
										pDummyInput->m_TargetX = m_aInputData[g_Config.m_ClDummy].m_TargetX;
										pDummyInput->m_TargetY = m_aInputData[g_Config.m_ClDummy].m_TargetY;
									}
								}
								else
								{
									m_DummyAutoHookTargetId = -1;
									pDummyInput->m_Hook = m_aInputData[g_Config.m_ClDummy].m_Hook;
									pDummyInput->m_TargetX = m_aInputData[g_Config.m_ClDummy].m_TargetX;
									pDummyInput->m_TargetY = m_aInputData[g_Config.m_ClDummy].m_TargetY;
								}
							}
							else
							{
								pDummyInput->m_Hook = m_aInputData[g_Config.m_ClDummy].m_Hook;
								pDummyInput->m_TargetX = m_aInputData[g_Config.m_ClDummy].m_TargetX;
								pDummyInput->m_TargetY = m_aInputData[g_Config.m_ClDummy].m_TargetY;
							}
						}
					}
				}
				else
				{
					m_DummyAutoHookTargetId = -1;
				}

				pDummyInput->m_PlayerFlags = m_aInputData[g_Config.m_ClDummy].m_PlayerFlags;
				if(!g_Config.m_ClDummyAutoHook || !g_Config.m_ClDummyCopyMoves)
				{
					pDummyInput->m_TargetX = m_aInputData[g_Config.m_ClDummy].m_TargetX;
					pDummyInput->m_TargetY = m_aInputData[g_Config.m_ClDummy].m_TargetY;
				}
				pDummyInput->m_WantedWeapon = m_aInputData[g_Config.m_ClDummy].m_WantedWeapon;
				if(!g_Config.m_ClDummyControl)
					pDummyInput->m_Fire += m_aInputData[g_Config.m_ClDummy].m_Fire - m_aLastData[g_Config.m_ClDummy].m_Fire;
				pDummyInput->m_NextWeapon += m_aInputData[g_Config.m_ClDummy].m_NextWeapon - m_aLastData[g_Config.m_ClDummy].m_NextWeapon;
				pDummyInput->m_PrevWeapon += m_aInputData[g_Config.m_ClDummy].m_PrevWeapon - m_aLastData[g_Config.m_ClDummy].m_PrevWeapon;
			}

			m_aInputData[!g_Config.m_ClDummy] = *pDummyInput;
		}

		if(g_Config.m_ClDummyControl)
		{
			CNetObj_PlayerInput *pDummyInput = &GameClient()->m_DummyInput;
			pDummyInput->m_Jump = g_Config.m_ClDummyJump;

			if(g_Config.m_ClDummyFire)
				pDummyInput->m_Fire = g_Config.m_ClDummyFire;
			else if((pDummyInput->m_Fire & 1) != 0)
				pDummyInput->m_Fire++;

			pDummyInput->m_Hook = g_Config.m_ClDummyHook;
		}

		// DDK: Auto Follow Nearest — move dummy X toward nearest player
		if(g_Config.m_DdkAutoFollowNearest && Client()->DummyConnected())
		{
			const int DummyId = GameClient()->m_aLocalIds[!g_Config.m_ClDummy];
			const int LocalId = GameClient()->m_Snap.m_LocalClientId;

			if(DummyId >= 0 && GameClient()->m_Snap.m_aCharacters[DummyId].m_Active)
			{
				const float DummyX = GameClient()->m_Snap.m_aCharacters[DummyId].m_Cur.m_X;

				// Find nearest other player by X distance
				float MinDist = -1.0f;
				float TargetX = DummyX;
				for(int i = 0; i < MAX_CLIENTS; ++i)
				{
					if(i == DummyId || i == LocalId)
						continue;
					if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
						continue;
					const float OtherX = GameClient()->m_Snap.m_aCharacters[i].m_Cur.m_X;
					const float Dist = absolute(OtherX - DummyX);
					if(MinDist < 0.0f || Dist < MinDist)
					{
						MinDist = Dist;
						TargetX = OtherX;
					}
				}

				if(MinDist >= 0.0f)
				{
					const float Diff = TargetX - DummyX;
					CNetObj_PlayerInput *pDummyInput = &GameClient()->m_DummyInput;
					if(Diff > 3.0f)
						pDummyInput->m_Direction = 1;
					else if(Diff < -3.0f)
						pDummyInput->m_Direction = -1;
					else
						pDummyInput->m_Direction = 0;
				}
			}
		}

		// stress testing
		if(g_Config.m_DbgStress)
		{
			float t = Client()->LocalTime();
			mem_zero(&m_aInputData[g_Config.m_ClDummy], sizeof(m_aInputData[0]));

			m_aInputData[g_Config.m_ClDummy].m_Direction = ((int)t / 2) & 1;
			m_aInputData[g_Config.m_ClDummy].m_Jump = ((int)t);
			m_aInputData[g_Config.m_ClDummy].m_Fire = ((int)(t * 10));
			m_aInputData[g_Config.m_ClDummy].m_Hook = ((int)(t * 2)) & 1;
			m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = ((int)t) % NUM_WEAPONS;
			m_aInputData[g_Config.m_ClDummy].m_TargetX = (int)(std::sin(t * 3) * 100.0f);
			m_aInputData[g_Config.m_ClDummy].m_TargetY = (int)(std::cos(t * 3) * 100.0f);
		}

		// check if we need to send input
		Send = Send || m_aInputData[g_Config.m_ClDummy].m_Direction != m_aLastData[g_Config.m_ClDummy].m_Direction;
		Send = Send || m_aInputData[g_Config.m_ClDummy].m_Jump != m_aLastData[g_Config.m_ClDummy].m_Jump;
		Send = Send || m_aInputData[g_Config.m_ClDummy].m_Fire != m_aLastData[g_Config.m_ClDummy].m_Fire;
		Send = Send || m_aInputData[g_Config.m_ClDummy].m_Hook != m_aLastData[g_Config.m_ClDummy].m_Hook;
		Send = Send || m_aInputData[g_Config.m_ClDummy].m_WantedWeapon != m_aLastData[g_Config.m_ClDummy].m_WantedWeapon;
		Send = Send || m_aInputData[g_Config.m_ClDummy].m_NextWeapon != m_aLastData[g_Config.m_ClDummy].m_NextWeapon;
		Send = Send || m_aInputData[g_Config.m_ClDummy].m_PrevWeapon != m_aLastData[g_Config.m_ClDummy].m_PrevWeapon;
		Send = Send || time_get() > m_LastSendTime + time_freq() / 25; // send at least 25 Hz
		Send = Send || (GameClient()->m_Snap.m_pLocalCharacter && GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_NINJA && (m_aInputData[g_Config.m_ClDummy].m_Direction || m_aInputData[g_Config.m_ClDummy].m_Jump || m_aInputData[g_Config.m_ClDummy].m_Hook));
	}

	// copy and return size
	m_aLastData[g_Config.m_ClDummy] = m_aInputData[g_Config.m_ClDummy];

	if(!Send)
		return 0;

	m_LastSendTime = time_get();
	mem_copy(pData, &m_aInputData[g_Config.m_ClDummy], sizeof(m_aInputData[0]));
	return sizeof(m_aInputData[0]);
}

void CControls::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(g_Config.m_ClAutoswitchWeaponsOutOfAmmo && !GameClient()->m_GameInfo.m_UnlimitedAmmo && GameClient()->m_Snap.m_pLocalCharacter)
	{
		// Keep track of ammo count, we know weapon ammo only when we switch to that weapon, this is tracked on server and protocol does not track that
		m_aAmmoCount[maximum(0, GameClient()->m_Snap.m_pLocalCharacter->m_Weapon % NUM_WEAPONS)] = GameClient()->m_Snap.m_pLocalCharacter->m_AmmoCount;
		// Autoswitch weapon if we're out of ammo
		if(m_aInputData[g_Config.m_ClDummy].m_Fire % 2 != 0 &&
			GameClient()->m_Snap.m_pLocalCharacter->m_AmmoCount == 0 &&
			GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_HAMMER &&
			GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_NINJA)
		{
			int Weapon;
			for(Weapon = WEAPON_LASER; Weapon > WEAPON_GUN; Weapon--)
			{
				if(Weapon == GameClient()->m_Snap.m_pLocalCharacter->m_Weapon)
					continue;
				if(m_aAmmoCount[Weapon] > 0)
					break;
			}
			if(Weapon != GameClient()->m_Snap.m_pLocalCharacter->m_Weapon)
				m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = Weapon + 1;
		}
	}

	// update target pos
	if(GameClient()->m_Snap.m_pGameInfoObj && !GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		// make sure to compensate for smooth dyncam to ensure the cursor stays still in world space if zoomed
		vec2 DyncamOffsetDelta = GameClient()->m_Camera.m_DyncamTargetCameraOffset - GameClient()->m_Camera.m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy];
		float Zoom = GameClient()->m_Camera.m_Zoom;
		m_aTargetPos[g_Config.m_ClDummy] = GameClient()->m_LocalCharacterPos + m_aMousePos[g_Config.m_ClDummy] - DyncamOffsetDelta + DyncamOffsetDelta / Zoom;
	}
	else if(GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_UsePosition)
	{
		m_aTargetPos[g_Config.m_ClDummy] = GameClient()->m_Snap.m_SpecInfo.m_Position + m_aMousePos[g_Config.m_ClDummy];
	}
	else
	{
		m_aTargetPos[g_Config.m_ClDummy] = m_aMousePos[g_Config.m_ClDummy];
	}
}

bool CControls::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
		return false;

	if(CursorType == IInput::CURSOR_JOYSTICK && g_Config.m_InpControllerAbsolute && GameClient()->m_Snap.m_pGameInfoObj && !GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		vec2 AbsoluteDirection;
		if(Input()->GetActiveJoystick()->Absolute(&AbsoluteDirection.x, &AbsoluteDirection.y))
		{
			m_aMousePos[g_Config.m_ClDummy] = AbsoluteDirection * GetMaxMouseDistance();
			GameClient()->m_Controls.m_aMouseInputType[g_Config.m_ClDummy] = CControls::EMouseInputType::ABSOLUTE;
		}
		return true;
	}

	float Factor = 1.0f;
	if(g_Config.m_ClDyncam && g_Config.m_ClDyncamMousesens)
	{
		Factor = g_Config.m_ClDyncamMousesens / 100.0f;
	}
	else
	{
		switch(CursorType)
		{
		case IInput::CURSOR_MOUSE:
			Factor = g_Config.m_InpMousesens / 100.0f;
			break;
		case IInput::CURSOR_JOYSTICK:
			Factor = g_Config.m_InpControllerSens / 100.0f;
			break;
		default:
			dbg_assert_failed("CControls::OnCursorMove CursorType %d", (int)CursorType);
		}
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId < 0)
		Factor *= GameClient()->m_Camera.m_Zoom;

	m_aMousePos[g_Config.m_ClDummy] += vec2(x, y) * Factor;
	GameClient()->m_Controls.m_aMouseInputType[g_Config.m_ClDummy] = CControls::EMouseInputType::RELATIVE;
	ClampMousePos();
	return true;
}

void CControls::ClampMousePos()
{
	if(GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId < 0)
	{
		m_aMousePos[g_Config.m_ClDummy].x = std::clamp(m_aMousePos[g_Config.m_ClDummy].x, -201.0f * 32, (Collision()->GetWidth() + 201.0f) * 32.0f);
		m_aMousePos[g_Config.m_ClDummy].y = std::clamp(m_aMousePos[g_Config.m_ClDummy].y, -201.0f * 32, (Collision()->GetHeight() + 201.0f) * 32.0f);
	}
	else
	{
		const float MouseMin = GetMinMouseDistance();
		const float MouseMax = GetMaxMouseDistance();

		float MouseDistance = length(m_aMousePos[g_Config.m_ClDummy]);
		if(MouseDistance < 0.001f)
		{
			m_aMousePos[g_Config.m_ClDummy].x = 0.001f;
			m_aMousePos[g_Config.m_ClDummy].y = 0;
			MouseDistance = 0.001f;
		}
		if(MouseDistance < MouseMin)
			m_aMousePos[g_Config.m_ClDummy] = normalize_pre_length(m_aMousePos[g_Config.m_ClDummy], MouseDistance) * MouseMin;
		MouseDistance = length(m_aMousePos[g_Config.m_ClDummy]);
		if(MouseDistance > MouseMax)
			m_aMousePos[g_Config.m_ClDummy] = normalize_pre_length(m_aMousePos[g_Config.m_ClDummy], MouseDistance) * MouseMax;

		if(g_Config.m_TcLimitMouseToScreen)
		{
			float Width, Height;
			Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), 1.0f, &Width, &Height);
			Height /= 2.0f;
			Width /= 2.0f;
			if(g_Config.m_TcLimitMouseToScreen == 2)
				Width = Height;
			m_aMousePos[g_Config.m_ClDummy].y = std::clamp(m_aMousePos[g_Config.m_ClDummy].y, -Height, Height);
			m_aMousePos[g_Config.m_ClDummy].x = std::clamp(m_aMousePos[g_Config.m_ClDummy].x, -Width, Width);
		}
	}
}

float CControls::GetMinMouseDistance() const
{
	return g_Config.m_ClDyncam ? g_Config.m_ClDyncamMinDistance : g_Config.m_ClMouseMinDistance;
}

float CControls::GetMaxMouseDistance() const
{
	float CameraMaxDistance = 200.0f;
	float FollowFactor = (g_Config.m_ClDyncam ? g_Config.m_ClDyncamFollowFactor : g_Config.m_ClMouseFollowfactor) / 100.0f;
	float DeadZone = g_Config.m_ClDyncam ? g_Config.m_ClDyncamDeadzone : g_Config.m_ClMouseDeadzone;
	float MaxDistance = g_Config.m_ClDyncam ? g_Config.m_ClDyncamMaxDistance : g_Config.m_ClMouseMaxDistance;
	return minimum((FollowFactor != 0 ? CameraMaxDistance / FollowFactor + DeadZone : MaxDistance), MaxDistance);
}

bool CControls::CheckNewInput()
{
	if(g_Config.m_TcFastInput && g_Config.m_BcFastInputMode == 4 && g_Config.m_BcSaikoPlusAmount > 0)
	{
		CNetObj_PlayerInput TestInput = m_aInputData[g_Config.m_ClDummy];
		TestInput.m_Direction = 0;
		if(m_aInputDirectionLeft[g_Config.m_ClDummy] && !m_aInputDirectionRight[g_Config.m_ClDummy])
			TestInput.m_Direction = -1;
		if(!m_aInputDirectionLeft[g_Config.m_ClDummy] && m_aInputDirectionRight[g_Config.m_ClDummy])
			TestInput.m_Direction = 1;

		bool NewInput = false;
		if(m_aFastInput[g_Config.m_ClDummy].m_Direction != TestInput.m_Direction)
			NewInput = true;
		if(m_aFastInput[g_Config.m_ClDummy].m_Hook != TestInput.m_Hook)
			NewInput = true;
		if(m_aFastInput[g_Config.m_ClDummy].m_Fire != TestInput.m_Fire)
			NewInput = true;
		if(m_aFastInput[g_Config.m_ClDummy].m_Jump != TestInput.m_Jump)
			NewInput = true;
		if(m_aFastInput[g_Config.m_ClDummy].m_NextWeapon != TestInput.m_NextWeapon)
			NewInput = true;
		if(m_aFastInput[g_Config.m_ClDummy].m_PrevWeapon != TestInput.m_PrevWeapon)
			NewInput = true;
		if(m_aFastInput[g_Config.m_ClDummy].m_WantedWeapon != TestInput.m_WantedWeapon)
			NewInput = true;

		if(g_Config.m_ClSubTickAiming)
		{
			TestInput.m_TargetX = (int)m_aMousePos[g_Config.m_ClDummy].x;
			TestInput.m_TargetY = (int)m_aMousePos[g_Config.m_ClDummy].y;
		}

		m_aFastInput[g_Config.m_ClDummy] = TestInput;

		return NewInput;
	}

	bool NewInput[2] = {};
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		CNetObj_PlayerInput TestInput = m_aInputData[Dummy];
		if(Dummy == g_Config.m_ClDummy)
		{
			const bool LeftPressed = m_aInputDirectionLeft[Dummy] != 0;
			const bool RightPressed = m_aInputDirectionRight[Dummy] != 0;
			TestInput.m_Direction = ResolveMovementDirection(Dummy, LeftPressed, RightPressed);
		}

		if(m_aFastInput[Dummy].m_Direction != TestInput.m_Direction)
			NewInput[Dummy] = true;
		if(m_aFastInput[Dummy].m_Hook != TestInput.m_Hook)
			NewInput[Dummy] = true;
		if(m_aFastInput[Dummy].m_Fire != TestInput.m_Fire)
			NewInput[Dummy] = true;
		if(m_aFastInput[Dummy].m_Jump != TestInput.m_Jump)
			NewInput[Dummy] = true;
		if(m_aFastInput[Dummy].m_NextWeapon != TestInput.m_NextWeapon)
			NewInput[Dummy] = true;
		if(m_aFastInput[Dummy].m_PrevWeapon != TestInput.m_PrevWeapon)
			NewInput[Dummy] = true;
		if(m_aFastInput[Dummy].m_WantedWeapon != TestInput.m_WantedWeapon)
			NewInput[Dummy] = true;

		bool SetMousePos = false;
		// We need to be careful about how we manage the mouse position to avoid mispredicted hooks and fires
		// on the first tick that they activate before we know what mouse position we actually sent to the server
		if(Dummy == g_Config.m_ClDummy)
		{
			if(m_aFastInput[Dummy].m_Hook == 0 && TestInput.m_Hook == 1)
			{
				m_FastInputHookAction = true;
				SetMousePos = true;
			}
			if(m_aFastInput[Dummy].m_Fire != TestInput.m_Fire && TestInput.m_Fire % 2 == 1)
			{
				m_FastInputFireAction = true;
				SetMousePos = true;
			}
			if(!m_FastInputHookAction && !m_FastInputFireAction)
			{
				SetMousePos = true;
			}
		}

		if(SetMousePos)
		{
			TestInput.m_TargetX = (int)m_aMousePos[Dummy].x;
			TestInput.m_TargetY = (int)m_aMousePos[Dummy].y;
		}
		else
		{
			TestInput.m_TargetX = m_aFastInput[Dummy].m_TargetX;
			TestInput.m_TargetY = m_aFastInput[Dummy].m_TargetY;
		}

		m_aFastInput[Dummy] = TestInput;
	}

	if(NewInput[0] || NewInput[1])
		return true;
	else
		return false;
}
