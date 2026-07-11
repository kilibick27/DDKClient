/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_CONTROLS_H

#include <base/vmath.h>

#include <engine/client.h>
#include <engine/console.h>

#include <generated/protocol.h>

#include <game/client/component.h>

class CControls : public CComponent
{
public:
	float GetMinMouseDistance() const;
	float GetMaxMouseDistance() const;

	enum class EMouseInputType
	{
		ABSOLUTE,
		RELATIVE,
		AUTOMATED,
	};

	// Auto aim hook: shared hook range used by both the catch logic (CControls) and the
	// FOV rendering (CPlayers). The cone half-angle comes from cl_auto_aim_fov / 2.
	static constexpr float ms_AutoAimHookRange = 375.0f;

	// DDK Auto Hammer: default max center-to-center distance (px) at which a tee is considered
	// within hammer reach. The live value is configurable via ddk_auto_hammer_range. Server hammer
	// reach when aimed straight at a tee is ~35px (ProjStartPos 21px ahead + 14px hit radius).
	static constexpr float ms_AutoHammerRange = 64.0f;

	vec2 m_aMousePos[NUM_DUMMIES];
	vec2 m_aMousePosOnAction[NUM_DUMMIES];
	vec2 m_aTargetPos[NUM_DUMMIES];

	// DDK Auto Aim Hook "Silent" mode: current smoothed aim direction, turned toward the target
	// by a limited rate per tick instead of snapping. (0, 0) means "not yet seeded".
	vec2 m_aAutoAimSmoothDir[NUM_DUMMIES];
	// Whether the hook button was already held last tick, so a fresh press can reseed
	// m_aAutoAimSmoothDir instantly instead of turning toward the new target from a stale
	// direction left over from a previous, unrelated hook press.
	bool m_aAutoAimHookWasHeld[NUM_DUMMIES] = {};

	EMouseInputType m_aMouseInputType[NUM_DUMMIES];

	int m_aAmmoCount[NUM_WEAPONS];

	int64_t m_LastSendTime;
	CNetObj_PlayerInput m_aInputData[NUM_DUMMIES];
	CNetObj_PlayerInput m_aLastData[NUM_DUMMIES];
	int m_aInputDirectionLeft[NUM_DUMMIES];
	int m_aInputDirectionRight[NUM_DUMMIES];
	int m_aShowHookColl[NUM_DUMMIES];

	// TClient
	CNetObj_PlayerInput m_aFastInput[NUM_DUMMIES];
	bool m_FastInputHookAction = false;
	bool m_FastInputFireAction = false;
	bool m_WeaponsGot = false;
	int m_aSnapTapAppliedDirection[NUM_DUMMIES];
	int m_aSnapTapLastPressedDirection[NUM_DUMMIES];
	int64_t m_aSnapTapLastPressedTime[NUM_DUMMIES];
	int m_aSnapTapPrevLeft[NUM_DUMMIES];
	int m_aSnapTapPrevRight[NUM_DUMMIES];
	int m_AutoFollowTargetId;
	int m_DummyAutoHookTargetId;
	// DDK Auto Hammer: last predicted tick a synthetic press was injected, per dummy so switching
	// dummies doesn't reset/share the cooldown.
	int m_aAutoHammerLastFireTick[NUM_DUMMIES] = {};
	// Fast Hammer (manual hold): last predicted tick a synthetic press was injected while the
	// player held fire themselves, per dummy so switching dummies doesn't reset/share the cooldown.
	int m_aFastHammerLastFireTick[NUM_DUMMIES] = {};

	// DDK AimHelper Visuals: ClientId of the tee the auto hook is currently targeting, -1 if none.
	int m_AutoAimHookTargetId = -1;

	CControls();
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	void OnConsoleInit() override;
	virtual void OnPlayerDeath();

	int SnapInput(int *pData);
	void ClampMousePos();
	void ResetInput(int Dummy);
	bool CheckNewInput();
	void GoresMode();

	private:
		bool UseGammaInputMovement() const;
		void UpdateSnapTapState(int Dummy, bool LeftPressed, bool RightPressed);
		int ResolveMovementDirection(int Dummy, bool LeftPressed, bool RightPressed);
		int ResolveSnapTapDirection(int Dummy, bool LeftPressed, bool RightPressed);
		bool IsSnapTapActive() const;
	static void ConKeyInputState(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputSet(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData);
};
#endif
