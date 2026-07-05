/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc)
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc)
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc)
#endif

// Dummy control variables
MACRO_CONFIG_INT(ClDummy, cl_dummy, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether you control your player (0) or your dummy (1)")
MACRO_CONFIG_INT(ClDummyHammer, cl_dummy_hammer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy is hammering for a hammerfly")
MACRO_CONFIG_INT(ClDummyResetOnSwitch, cl_dummy_resetonswitch, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Whether dummy or player should stop pressing keys when you switch (0 = off, 1 = dummy, 2 = player)")
MACRO_CONFIG_INT(ClDummyRestoreWeapon, cl_dummy_restore_weapon, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Whether dummy should switch to last weapon after hammerfly")
MACRO_CONFIG_INT(ClDummyCopyMoves, cl_dummy_copy_moves, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Whether dummy should copy your moves")
MACRO_CONFIG_INT(ClDummyCopyMovesWithHammer, cl_dummy_copy_moves_with_hammer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Whether dummy should copy your moves and continue during hammer")
MACRO_CONFIG_INT(ClDummyDeepfly, cl_dummy_deepfly, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Deepfly mode: when you hit dummy, it immediately hits you back once")
MACRO_CONFIG_INT(ClDummyAutoHook, cl_dummy_auto_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy automatically hooks nearest tee (only works with copy moves enabled)")

// More controllable dummy command
MACRO_CONFIG_INT(ClDummyControl, cl_dummy_control, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether you can control dummy at the same time (cl_dummy_jump, cl_dummy_fire, cl_dummy_hook)")
MACRO_CONFIG_INT(ClDummyJump, cl_dummy_jump, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy is jumping (requires cl_dummy_control 1)")
MACRO_CONFIG_INT(ClDummyFire, cl_dummy_fire, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy is firing (requires cl_dummy_control 1)")
MACRO_CONFIG_INT(ClDummyHook, cl_dummy_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy is hooking (requires cl_dummy_control 1)")
