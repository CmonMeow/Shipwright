/**
 * Compact actor registry for the test01 multiplayer runtime.
 *
 * Link is created by the scene start-position command. The remaining built-in
 * actors are native presentation adapters used by arrows and fishing. Network
 * render actors register dynamically after these entries.
 */
/* 0x0000 */ DEFINE_ACTOR_INTERNAL(Player, ACTOR_PLAYER, ALLOCTYPE_NORMAL)
/* 0x0001 */ DEFINE_ACTOR(En_Arrow, ACTOR_EN_ARROW, ALLOCTYPE_PERMANENT)
/* 0x0002 */ DEFINE_ACTOR(En_Fish, ACTOR_EN_FISH, ALLOCTYPE_NORMAL)
/* 0x0003 */ DEFINE_ACTOR(Fishing, ACTOR_FISHING, ALLOCTYPE_NORMAL)
