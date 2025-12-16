/*
 * USBOT0.c - US Bot AI Script (Reconstructed)
 * Decompiled from USBOT0.SCR using vcdecomp
 *
 * This is a multiplayer bot AI for Capture The Flag mode.
 * The bot can be either US or VC side depending on initialization.
 */

#include "inc\sc_global.h"
#include "inc\sc_def.h"

// ============================================================================
// GLOBAL VARIABLES (mapped from data segment)
// ============================================================================

// State machine and status - MUST initialize all globals!
int gphase = 0;                 // data[1]  - Current AI phase/state (GP_CREATE = 0)
int timer = 0;                  // data[2]  - General purpose timer
int enemyside = 0;              // data[3]  - Enemy side (fixed: was swapped)
int myside = 0;                 // data[4]  - Bot's side (SC_P_SIDE_US or SC_P_SIDE_VC)

// Position data
c_Vector3 origpos;              // data[8]  - Original spawn position (struct, zeros OK)
c_Vector3 myflag;               // data[11] - Position of own flag
dword pl_id = 0;                // data[12] - Player handle (MUST be global!)
c_Vector3 enflag;               // data[14] - Position of enemy flag
c_Vector3 mycurflag;            // data[17] - Current position of own flag
c_Vector3 encurflag;            // data[20] - Current position of enemy flag

// Flag status
int enflagstat = 0;             // data[23] - Enemy flag status
float tickvalue = 0.0f;         // data[24] - Tick timing value
float origz = 0.0f;             // data[25] - Original Z rotation

// Timers and orders
float standingtimer = 0.0f;     // data[118] - Standing timer
float endtimer = 2.0f;          // data[119] - End timer threshold (starts at 2.0f, then rand()%5 after trigger)
float orderstimer = 0.0f;       // data[120] - Orders timer
int myorder = 0;                // data[121] - Current order
int priority = 0;               // data[122] - Priority level

// Additional state
int wasenflag = 0;              // Enemy flag was seen
float respawntimer = 0.0f;      // Respawn countdown
float walktimer = 0.0f;         // Walking timer
int firstoff = 0;               // First offset flag
int timeblock = 0;              // Time block flag
int amidead = 0;                // Death status
float inittimer = 0.0f;         // data[323] - Timer for delayed InitBot in gphase 1

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void SetupVC(s_SC_P_Create *pinfo);
void SetupUS(s_SC_P_Create *pinfo);
void CreateBot(s_SC_P_info *info);
void InitBot(s_SC_P_info *info);
int GetMoveDirection(dword player);
int CheckNearbyPlayers(c_Vector3 *pos);
int IsNear3DHelper(dword player, c_Vector3 *targetpos, float radius);
dword FindNearestEnemy(dword player);
dword FindNearestEngagedEnemy(dword player);
int IsNearFlag(dword player);
void DoPatrol(dword player);
void DoAttack(dword player);
int ProcessOrderExtended(s_SC_P_info *info, int order);
void MainAILoop(s_SC_P_info *info);
void SetupBattleMode(s_SC_P_info *info);
void UpdateEnemyTracking(s_SC_P_info *info);
void UpdateFlagPositions(void);
int ProcessOrder(s_SC_P_info *info, int order);
void ProcessState(s_SC_P_info *info, int state);
int ScriptMain(s_SC_P_info *info);

// ============================================================================
// SIDE SETUP FUNCTIONS
// ============================================================================

/*
 * SetupVC - Configure bot for Vietcong side
 * func_0001 @1-34
 *
 * Sets up pinfo structure fields and global side variables for VC.
 * Disassembly shows field assignments at offsets:
 *   - offset 96 (data[24]) = inifile
 *   - offset 64 (data[16]) = name_nr = 2506
 *   - offset 176 (data[44]) = weap_pistol = 9
 *   - offset 192 (data[48]) = weap_main1 = 2
 *   - offset 224 (data[56]) = weap_slot1 = 50
 */
void SetupVC(s_SC_P_Create *pinfo) {
    pinfo->inifile = "Ini\\Players\\net_vc_nvaofficer.ini";
    pinfo->name_nr = 2506;       // VC officer name ID
    pinfo->weap_pistol = 9;      // Weapon ID for pistol slot
    pinfo->weap_main1 = 2;       // Weapon ID for main weapon
    pinfo->weap_slot1 = 50;      // Equipment/slot ID
    myside = SC_P_SIDE_VC;       // 1 - stored to data[4]
    enemyside = SC_P_SIDE_US;    // 0 - stored to data[3]
}

/*
 * SetupUS - Configure bot for US side
 * func_0035 @35-68
 *
 * Sets up pinfo structure fields and global side variables for US.
 * Same field offsets as SetupVC but with US ini file and different weapon IDs.
 */
void SetupUS(s_SC_P_Create *pinfo) {
    pinfo->inifile = "Ini\\Players\\net_us_sf1.ini";
    pinfo->name_nr = 2499;       // US SF name ID
    pinfo->weap_pistol = 7;      // Weapon ID for pistol slot
    pinfo->weap_main1 = 1;       // Weapon ID for main weapon
    pinfo->weap_slot1 = 59;      // Equipment/slot ID
    myside = SC_P_SIDE_US;       // 0 - stored to data[4]
    enemyside = SC_P_SIDE_VC;    // 1 - stored to data[3]
}

// ============================================================================
// BOT CREATION
// ============================================================================

/*
 * CreateBot - Create the AI player
 * func_0069 @69-146
 *
 * Disassembly analysis:
 *   - ASP 39: allocate s_SC_P_Create (156 bytes = 39 dwords)
 *   - ASP 20: allocate eqp array (80 bytes = 20 dwords)
 *   - pinfo.type = 2 (SC_P_TYPE_AI)
 *   - pinfo.side = 0 (SC_P_SIDE_US) - hardcoded!
 *   - Condition: if (pinfo.side != 0) SetupVC else SetupUS
 *   - Since side=0, this bot ALWAYS calls SetupUS (it's US bot)
 *   - CRITICAL: Result stored to pl_id, NOT global variable!
 */
void CreateBot(s_SC_P_info *info) {
    s_SC_P_Create pinfo;
    s_SC_P_CreateEqp eqp[20];

    // Clear structures
    SC_ZeroMem(&pinfo, 156);  // sizeof(s_SC_P_Create) - hardcoded due to compiler bug

    // Set player type as AI
    pinfo.type = SC_P_TYPE_AI;
    pinfo.side = SC_P_SIDE_US;  // Hardcoded to US (0) in disassembly!
    pinfo.group = 0;
    pinfo.member_id = 0;

    // Setup equipment array
    SC_ZeroMem(&eqp, 80);  // 20 * sizeof(s_SC_P_CreateEqp) - hardcoded due to compiler bug
    pinfo.eqp = eqp;
    pinfo.eqps = 0;
    // NOTE: pinfo.flags = 0 (after ZeroMem) - originál NENASTAVUJE flags!

    // Choose setup based on pinfo.side (NOT myside!)
    // Disasm: JZ label_0112 means "if (pinfo.side == 0) goto SetupUS"
    if (pinfo.side != SC_P_SIDE_US) {
        SetupVC(&pinfo);
    } else {
        SetupUS(&pinfo);
    }

    // Set icon and recover position
    pinfo.icon_name = "nhut";
    pinfo.weap_knife = 0;
    pinfo.recover_pos = info->pos;  // Engine provides spawn position!

    // Create the player - MUST set info->pl_id for engine!
    info->pl_id = SC_P_Create(&pinfo);
    pl_id = info->pl_id;  // Also store in global for internal use
    gphase = 1;  // Move to InitBot phase
}

// ============================================================================
// BOT INITIALIZATION
// ============================================================================

/*
 * InitBot - Initialize AI properties based on difficulty
 * func_0147 @147-460
 */
void InitBot(s_SC_P_info *info) {
    s_SC_P_AI_props aiprops;
    int difficulty;
    void *flagnode;

    // Get initial position and rotation
    SC_P_GetPos(pl_id, &origpos);
    SC_P_EnableSearchDeathBodies(pl_id, FALSE);
    origz = SC_P_GetRot(pl_id);

    // Set AI mode to battle
    SC_P_Ai_SetMode(pl_id, SC_P_AI_MODE_BATTLE);
    SC_P_Ai_EnableShooting(pl_id, TRUE);

    // Get default AI properties
    SC_ZeroMem(&aiprops, 128);  // sizeof(s_SC_P_AI_props) - hardcoded due to compiler bug
    SC_P_Ai_GetProps(pl_id, &aiprops);

    // Adjust properties based on difficulty
    difficulty = SC_ggi(SGI_DIFFICULTY);

    switch (difficulty) {
        case 0:  // Easy
            aiprops.shoot_imprecision = 1.0f;
            aiprops.reaction_time = 1.2f;
            aiprops.aimtime_max = 2.0f;
            aiprops.aimtime_canshoot = 0.8f;
            break;

        case 1:  // Medium
            aiprops.shoot_imprecision = 0.7f;
            aiprops.reaction_time = 0.4f;
            aiprops.aimtime_max = 1.4f;
            aiprops.aimtime_canshoot = 0.5f;
            break;

        case 2:  // Hard
            aiprops.shoot_imprecision = 0.3f;
            aiprops.berserk = 0.3f;
            aiprops.reaction_time = 0.2f;
            aiprops.aimtime_max = 1.0f;
            aiprops.aimtime_canshoot = 0.3f;
            break;

        case 3:  // Vietcong (hardest) - ONLY case 3, no default!
            // FIXED: Disasm lines 286-289 show that unknown difficulty values (4+)
            // skip to label_0306 without setting any values - they use GetProps defaults
            aiprops.shoot_imprecision = 0.1f;
            aiprops.berserk = 0.3f;
            aiprops.reaction_time = 0.1f;
            break;

        // NOTE: No default case - unknown difficulty values use SC_P_Ai_GetProps() defaults
    }

    // Common AI properties for all difficulties
    // Note: berserk, shoot_imprecision, reaction_time are multiplied by 1.0f (no-op)
    // This keeps the values set in the difficulty switch above
    aiprops.berserk *= 1.0f;              // Keep value from difficulty switch
    aiprops.shoot_imprecision *= 1.0f;    // Keep value from difficulty switch
    aiprops.extend_searchway = FALSE;     // offset 24
    aiprops.shortdistance_fight = 0.0f;   // offset 28
    aiprops.max_vis_distance = 120.0f;    // offset 0
    aiprops.view_angle = 3.0f;            // offset 32
    aiprops.view_angle_near = 4.0f;       // offset 36
    aiprops.reaction_time *= 1.0f;        // Keep value from difficulty switch
    aiprops.scout = 1.0f;                 // offset 96
    aiprops.hear_distance_max = 120.0f;   // offset 48
    aiprops.hear_imprecision = 1.0f;      // offset 40

    // Apply AI properties
    SC_P_Ai_SetProps(pl_id, &aiprops);
    SC_P_SetSpeachDist(pl_id, 30.0f);
    SC_P_Ai_SetBattleMode(pl_id, SC_P_AI_BATTLEMODE_ATTACK);

    // Get flag positions based on side
    if (myside == SC_P_SIDE_VC) {
        // VC side: our flag is flag_vc, enemy flag is flag_us
        flagnode = SC_NOD_Get(NULL, "flag_vc");
        SC_NOD_GetWorldPos(flagnode, &myflag);

        flagnode = SC_NOD_Get(NULL, "flag_us");
        SC_NOD_GetWorldPos(flagnode, &enflag);
    } else {
        // US side: our flag is flag_us, enemy flag is flag_vc
        flagnode = SC_NOD_Get(NULL, "flag_us");
        SC_NOD_GetWorldPos(flagnode, &myflag);

        flagnode = SC_NOD_Get(NULL, "flag_vc");
        SC_NOD_GetWorldPos(flagnode, &enflag);
    }

    // Copy initial flag positions
    mycurflag = myflag;
    encurflag = enflag;

    // Initialize state
    enflagstat = 0;
    tickvalue = 0.0f;
    gphase = 2;  // Move to SetupBattleMode phase (original value)
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/*
 * GetMoveDirection - Check if player is moving
 * func_0461 @461-485
 * Returns: 1 if moving (direction length > 1.0), 0 if stationary
 *
 * FIXED: Disasm analysis shows lines 486-510 (enemy/danger checks) are DEAD CODE.
 * After RET at line 485, there's no way to reach lines 486+.
 * The original function only checks direction vector length.
 */
int GetMoveDirection(dword player) {
    c_Vector3 dir;
    float len;

    SC_P_GetDir(player, &dir);
    len = SC_VectorLen(&dir);

    if (len > 1.0f) {
        return 1;  // Moving
    }

    return 0;  // Stationary

    // NOTE: Lines 486-510 in disasm (enemy/danger checks) are unreachable dead code
    // They follow a RET instruction with no label, so they were never executed
}

/*
 * CheckNearbyPlayers - Check if any players are nearby
 * func_0511 @511-546
 * Returns: 1 if players found, 0 if none
 *
 * Disassembly analysis:
 * - Takes position parameter (c_Vector3*)
 * - Creates search sphere with radius 2.0f
 * - Calls SC_GetPls to find nearby players
 */
int CheckNearbyPlayers(c_Vector3 *pos) {
    s_sphere searchsphere;
    dword players[64];
    dword count;

    searchsphere.pos = *pos;
    searchsphere.rad = 2.0f;

    count = 64;
    SC_GetPls(&searchsphere, players, &count);

    if (count > 0) {
        return 0;  // Players found - spawn blocked
    }
    return 1;  // No players nearby - can spawn
}

/*
 * IsNear3DHelper - Check if player is near a position
 * func_1298 @1298-1312
 * Returns: Result of SC_IsNear3D
 *
 * Disassembly:
 * - Gets player position
 * - Calls SC_IsNear3D with position and radius
 */
int IsNear3DHelper(dword player, c_Vector3 *targetpos, float radius) {
    c_Vector3 playerpos;

    SC_P_GetPos(player, &playerpos);
    return SC_IsNear3D(&playerpos, targetpos, radius);
}

/*
 * FindNearestEnemy - Find closest enemy player
 * func_0547 @547-685
 * Returns: Player ID of nearest enemy, or 0 if none
 */
dword FindNearestEnemy(dword player) {
    s_sphere searchsphere;
    s_SC_P_getinfo pinfo;
    dword players[64];
    dword count;
    dword nearest;
    float mindist, dist;
    int i;
    c_Vector3 pos;

    if (!player) return 0;

    // Setup search sphere around player
    SC_P_GetPos(player, &pos);
    searchsphere.pos = pos;
    searchsphere.rad = 1000.0f;

    count = 64;
    SC_GetPls(&searchsphere, players, &count);

    mindist = 10000.0f;
    nearest = 0;

    for (i = 0; i < count; i++) {
        SC_P_GetInfo(players[i], &pinfo);

        // Check if enemy side
        if (pinfo.side != myside) {
            if (SC_P_IsReady(players[i]) && SC_P_GetActive(players[i])) {
                dist = SC_P_GetDistance(player, players[i]);
                if (dist < mindist) {
                    mindist = dist;
                    nearest = players[i];
                }
            }
        }
    }

    return nearest;
}

/*
 * FindNearestEngagedEnemy - Find closest enemy bot who is in combat
 * func_0686 @686-779
 * Returns: Player ID of nearest enemy bot in combat, or 0 if none
 *
 * FIXED: Disasm line 708 shows GCP data[3] which is ENEMYSIDE, not myside!
 * This function finds enemy bots (member IDs 10-17) who have enemies,
 * NOT allied bots. The original name "FindNearestAlly" was wrong.
 */
dword FindNearestEngagedEnemy(dword player) {
    dword target;
    float mindist, dist;
    dword nearest;
    int i;

    mindist = 10000.0f;
    nearest = 0;

    // FIXED: Check ENEMY bots with member IDs 10-17 (NOT allies!)
    for (i = 0; i < 8; i++) {
        target = SC_P_GetBySideGroupMember(enemyside, 0, 10 + i);  // FIXED: enemyside!

        if (target && SC_P_GetActive(target) && SC_P_IsReady(target)) {
            // Check if this enemy has enemies (is engaged in combat)
            if (SC_P_Ai_GetEnemies(target)) {
                dist = SC_P_GetDistance(player, target);
                if (dist < mindist) {
                    mindist = dist;
                    nearest = target;
                }
            }
        }
    }

    return nearest;
}

/*
 * IsNearFlag - Check if player is near own flag
 * func_0780 @780-910
 * Returns: 1 if should patrol (tickvalue != 0, not near flag, or allies > 1), 0 otherwise
 *
 * FIXED: Disasm analysis shows:
 *   - First checks tickvalue != 0, returns 1 if true
 *   - Then checks SC_IsNear2D, returns 1 if NOT near
 *   - Finally returns (allies > 1) ? 1 : 0
 */
int IsNearFlag(dword player) {
    s_sphere searchsphere;
    s_SC_P_getinfo pinfo;
    dword players[64];
    dword count;
    c_Vector3 pos;
    int allies;
    int i;

    // FIXED: Check tickvalue first - if non-zero, return early
    if (tickvalue != 0.0f) {
        return 1;
    }

    SC_P_GetPos(player, &pos);

    // FIXED: Check if near own flag - if NOT near, return 1 (not 0)
    if (!SC_IsNear2D(&pos, &myflag, 30.0f)) {
        return 1;  // Not near flag
    }

    // Count allies near flag
    searchsphere.pos = myflag;
    searchsphere.rad = 20.0f;
    count = 64;
    SC_GetPls(&searchsphere, players, &count);

    allies = 0;
    for (i = 0; i < count; i++) {
        SC_P_GetInfo(players[i], &pinfo);
        if (pinfo.side == myside) {
            if (SC_P_IsReady(players[i]) && SC_P_GetActive(players[i])) {
                allies++;
            }
        }
    }

    // FIXED: Return 1 if more than 1 ally, 0 otherwise (not allies count)
    if (allies > 1) {
        return 1;
    }
    return 0;
}

// ============================================================================
// AI BEHAVIOR FUNCTIONS
// ============================================================================

/*
 * DoPatrol - Execute patrol behavior
 * func_0911 @911-1062
 *
 * CORRECTED: Disasm shows this first calls IsNearFlag(), not just checks player
 * - If IsNearFlag returns non-zero: patrol based on enflagstat
 * - If zero: random patrol or hide
 */
void DoPatrol(dword player) {
    s_sphere searchsphere;
    c_Vector3 waypoint;
    int found;
    static int patrol_state = 0;  // data[169] in disasm - tracks patrol state

    // FIXED: Check IsNearFlag first, not just player existence
    if (IsNearFlag(player)) {
        // We're near our own flag with allies
        // Check enemy flag status and decide patrol behavior
        if (enflagstat != 0 && patrol_state == 0) {
            // Enemy flag not at base, start tracking
            patrol_state = enflagstat;
            searchsphere.pos = enflag;
            searchsphere.rad = 35.0f;

            found = SC_GetRndWp(&searchsphere, &waypoint);
            SC_P_Ai_SetMoveMode(player, SC_P_AI_MOVEMODE_WALK);

            if (found) {
                SC_P_Ai_Go(player, &waypoint);
            } else {
                SC_P_Ai_Go(player, &searchsphere.pos);
            }
        }

        // If enemy flag at base, clear state and patrol there
        if (enflagstat == 0) {
            patrol_state = 0;
            searchsphere.pos = enflag;
            searchsphere.rad = 30.0f;

            found = SC_GetRndWp(&searchsphere, &waypoint);
            SC_P_Ai_SetMoveMode(player, SC_P_AI_MOVEMODE_WALK);

            if (found) {
                SC_P_Ai_Go(player, &waypoint);
            } else {
                SC_P_Ai_Go(player, &searchsphere.pos);
            }
        }
    } else {
        // Not near flag - random patrol or hide
        if (rand() % 10 == 0) {
            searchsphere.pos = myflag;  // FIXED: disasm uses myflag (data[11])
            searchsphere.rad = 15.0f;

            found = SC_GetRndWp(&searchsphere, &waypoint);
            SC_P_Ai_SetMoveMode(player, SC_P_AI_MOVEMODE_WALK);

            if (found) {
                SC_P_Ai_Go(player, &waypoint);
            } else {
                SC_P_Ai_Go(player, &searchsphere.pos);
            }
        } else {
            SC_P_Ai_HideYourself(player, &myflag, 15.0f);  // FIXED: uses myflag
        }
    }
}

/*
 * DoAttack - Execute attack/assist behavior
 * func_1063 @1063-1190
 *
 * CORRECTED based on disassembly:
 * - First checks ally (func_0686), if ally has enemies go there
 * - Then rand()%3: if != 0, find nearest ENEMY (func_0547) and go there
 * - If rand()%3 == 0, random movement around self
 */
void DoAttack(dword player) {
    s_sphere searchsphere;
    c_Vector3 waypoint, pos;
    dword target;
    int found;

    // First: Find engaged enemy bot and move towards the fight
    // FIXED: This function finds ENEMIES in combat, not allies
    target = FindNearestEngagedEnemy(player);

    if (target) {
        // If enemy bot is engaged, move towards the battle
        if (SC_P_Ai_GetEnemies(target)) {
            SC_P_GetPos(target, &pos);
            searchsphere.pos = pos;
            searchsphere.rad = 20.0f;

            SC_P_Ai_SetMoveMode(player, SC_P_AI_MOVEMODE_WALK);
            found = SC_GetRndWp(&searchsphere, &waypoint);

            if (found) {
                SC_P_Ai_Go(player, &waypoint);
            } else {
                SC_P_Ai_Go(player, &pos);
            }
            return;
        }
    }

    // FIXED: Condition was inverted and wrong function was called
    // Disasm: JZ label_1165 means if (rand()%3 == 0) skip to random, else find enemy
    if (rand() % 3 != 0) {
        // FIXED: Must call FindNearestEnemy (func_0547), not FindNearestAlly!
        target = FindNearestEnemy(player);
        if (target) {
            SC_P_GetPos(target, &pos);
            searchsphere.pos = pos;
            searchsphere.rad = 25.0f;

            SC_P_Ai_SetMoveMode(player, SC_P_AI_MOVEMODE_WALK);
            found = SC_GetRndWp(&searchsphere, &waypoint);

            if (found) {
                SC_P_Ai_Go(player, &waypoint);
            } else {
                SC_P_Ai_Go(player, &pos);
            }
        }
    } else {
        // Move randomly near current position (rand()%3 == 0)
        SC_P_GetPos(player, &pos);
        searchsphere.pos = pos;
        searchsphere.rad = 20.0f;

        SC_P_Ai_SetMoveMode(player, SC_P_AI_MOVEMODE_WALK);
        found = SC_GetRndWp(&searchsphere, &waypoint);

        // FIXED: Disasm shows no else branch here - only Go if found
        if (found) {
            SC_P_Ai_Go(player, &waypoint);
        }
    }
}

/*
 * SetupBattleMode - Configure battle mode
 * func_1191 @1191-1242
 */
void SetupBattleMode(s_SC_P_info *info) {
    int roundtime;

    SC_P_Ai_SetMode(pl_id, SC_P_AI_MODE_BATTLE);
    SC_P_Ai_SetBattleMode(pl_id, SC_P_AI_BATTLEMODE_ATTACK);
    myorder = 0;  // Reset order state (NOT gphase!)

    // Check round time
    roundtime = SC_ggi(540);  // SGI for round time

    // FIXED: Changed > 0 to >= 0 to match gphase 0 condition
    if (roundtime >= 0) {
        gphase = 3;
        SC_P_SetActive(pl_id, TRUE);
    } else {
        gphase = 500;  // Waiting for round start
        SC_P_SetActive(pl_id, FALSE);
    }
}

/*
 * UpdateEnemyTracking - Track standing time and trigger attack when idle
 * func_1243 @1243-1297
 *
 * FIXED: Disasm line 1387 shows SC_P_Ai_GetEnemies (NOT GetSureEnemies!)
 * This is different from MainAILoop case 1 which uses GetSureEnemies.
 *
 * Disassembly analysis:
 * - If NO enemies (enemies == 0), accumulate elapsed_time to standingtimer
 * - If moving (GetMoveDirection returns 1), reset standingtimer
 * - If standingtimer > threshold, reset and call DoAttack
 */
void UpdateEnemyTracking(s_SC_P_info *info) {
    dword enemies;

    // FIXED: Use SC_P_Ai_GetEnemies, NOT SC_P_Ai_GetSureEnemies
    enemies = SC_P_Ai_GetEnemies(pl_id);

    // If NO enemies, accumulate standing time
    if (enemies == 0) {
        standingtimer += info->elapsed_time;
    }
    // Note: NO else branch in disassembly!

    // If moving, reset standing time
    if (GetMoveDirection(pl_id)) {
        standingtimer = 0.0f;
    }

    // If standing too long without enemies, attack
    // Threshold (endtimer) starts at 2.0f, then becomes rand()%5 after first trigger
    if (standingtimer > endtimer) {
        standingtimer = 0.0f;
        endtimer = (float)(rand() % 5);  // data[119] - same variable as threshold
        DoAttack(pl_id);
    }
}

/*
 * UpdateFlagPositions - Update flag positions for CTF
 * func_1313 @1313-1505
 *
 * Disassembly analysis:
 * - Each case is independent (no fall-through)
 * - case 0: Copy base position to current (encurflag = enflag)
 * - case 1: Get carrier position via SC_MP_GetPlofHandle
 * - case 2: Get dropped item position via SC_Item_Find
 */
void UpdateFlagPositions(void) {
    int flagstat;
    dword carrier;
    dword item;

    // Check enemy flag status (our attack target)
    flagstat = SC_ggi(511);  // Enemy flag status

    // FIXED: Only update if status CHANGED (disasm lines 1330-1333, 1407-1410)
    // Original had: if (flagstat != 0)
    // Disasm shows: if (flagstat != enflagstat) - only update when status changes
    if (flagstat != enflagstat) {
        switch (flagstat) {
            case 0:  // At base - copy base position
                encurflag = enflag;
                break;

            case 1:  // Being carried - get carrier position
                carrier = SC_ggi(513);  // Flag carrier ID
                carrier = SC_MP_GetPlofHandle(carrier);
                if (carrier) {
                    SC_P_GetPos(carrier, &encurflag);
                }
                break;

            case 2:  // Dropped - get item position
                item = SC_Item_Find(146);  // Enemy flag item
                if (item) {
                    SC_Item_GetPos(item, &encurflag);
                }
                break;
        }
        enflagstat = flagstat;  // Save new status (disasm line 1409)
    }

    // Check own flag status (our defense target)
    flagstat = SC_ggi(510);  // Own flag status

    // FIXED: Only update if status CHANGED (disasm lines 1424-1427)
    // Original had: if (flagstat != 0)
    // Disasm shows: if (flagstat != tickvalue) - tickvalue reused as previous own flag status
    if (flagstat != tickvalue) {
        switch (flagstat) {
            case 0:  // At base - copy base position
                mycurflag = myflag;
                break;

            case 1:  // Being carried - get carrier position
                carrier = SC_ggi(512);  // Flag carrier ID
                carrier = SC_MP_GetPlofHandle(carrier);
                if (carrier) {
                    SC_P_GetPos(carrier, &mycurflag);
                }
                break;

            case 2:  // Dropped - get item position
                item = SC_Item_Find(145);  // Own flag item
                if (item) {
                    SC_Item_GetPos(item, &mycurflag);
                }
                break;
        }
        tickvalue = flagstat;  // Save new status (disasm shows data[24] is reused for this)
    }
}

// ============================================================================
// ORDER PROCESSING
// ============================================================================

/*
 * ProcessOrder - Execute order for bot (func_1506)
 * func_1506 @1506-1597
 * Returns: 1 if order processed, 0 if not
 *
 * Assembly analysis:
 * - Uses IsNear3DHelper (func_1298) for distance checks
 * - Case 0/1: checks mycurflag with 15.0f radius
 * - Case 2: checks mycurflag with 30.0f radius
 */
int ProcessOrder(s_SC_P_info *info, int order) {
    switch (order) {
        case 0:
        case 1:
            // Check if near own current flag
            if (IsNear3DHelper(pl_id, &mycurflag, 15.0f)) {
                SC_P_Ai_SetBattleModeExt(pl_id, SC_P_AI_BATTLEMODE_GOTO, &encurflag);
                myorder = 7;
                SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                SC_P_Ai_Go(pl_id, &encurflag);
                return 1;
            }
            break;

        case 2:
            // Check if near own current flag with larger radius
            if (IsNear3DHelper(pl_id, &mycurflag, 30.0f)) {
                SC_P_Ai_SetBattleModeExt(pl_id, SC_P_AI_BATTLEMODE_GOTO, &encurflag);
                myorder = 9;
                SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                SC_P_Ai_Go(pl_id, &encurflag);
                return 1;
            }
            break;

        default:
            break;
    }
    return 0;
}

/*
 * ProcessState - Execute state for bot (func_1722)
 * func_1722 @1722-2087
 *
 * KEY DISCOVERY: In case 0/1 and case 9/10, this function calls
 * ProcessOrderExtended first, and if it returns 0, then calls ProcessOrder.
 * This sequential calling pattern was missing from the original reconstruction.
 */
void ProcessState(s_SC_P_info *info, int state) {
    s_sphere searchsphere;
    c_Vector3 waypoint;
    int flagstat;
    int found;

    switch (state) {
        case 0:
        case 1:
            // Set myorder = 1, then call both functions sequentially
            myorder = 1;
            if (!ProcessOrderExtended(info, state)) {
                ProcessOrder(info, state);  // Only called if ProcessOrderExtended returns 0
            }
            break;

        case 2:  // Attack enemy flag
            if (enflagstat == 0) {
                if (IsNear3DHelper(pl_id, &encurflag, 8.0f)) {
                    SC_P_Ai_SetBattleMode(pl_id, SC_P_AI_BATTLEMODE_ATTACK);
                    SC_P_Ai_SetMode(pl_id, 0);  // SC_P_AI_MODE_PEACE = 0
                    SC_P_Ai_Go(pl_id, &encurflag);
                    SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                    SC_P_Ai_SetMovePos(pl_id, SC_P_AI_MOVEPOS_STAND);
                    myorder = 3;
                }
            }
            break;

        case 3:  // Check enemy flag status
            flagstat = SC_ggi(511);
            if (flagstat == 1) {
                SC_P_Ai_SetMode(pl_id, SC_P_AI_MODE_BATTLE);
                myorder = 1;

                if (IsNear3DHelper(pl_id, &enflag, 15.0f)) {
                    SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                    searchsphere.pos = encurflag;
                    searchsphere.rad = 30.0f;

                    found = SC_GetRndWp(&searchsphere, &waypoint);
                    if (found) {
                        SC_P_Ai_Go(pl_id, &waypoint);
                    } else {
                        SC_P_Ai_Go(pl_id, &searchsphere.pos);
                    }
                }
            }
            break;

        case 4:  // Defend own flag
            flagstat = SC_ggi(510);
            if (flagstat == 0) {
                if (IsNear3DHelper(pl_id, &myflag, 8.0f)) {
                    SC_P_Ai_SetBattleMode(pl_id, SC_P_AI_BATTLEMODE_ATTACK);
                    SC_P_Ai_SetMode(pl_id, 0);  // SC_P_AI_MODE_PEACE = 0
                    SC_P_Ai_Go(pl_id, &myflag);
                    SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                    SC_P_Ai_SetMovePos(pl_id, SC_P_AI_MOVEPOS_STAND);
                    myorder = 5;
                }
            }
            break;

        case 5:  // Check own flag status
            flagstat = SC_ggi(510);
            if (flagstat != 0) {
                SC_P_Ai_SetMode(pl_id, SC_P_AI_MODE_BATTLE);
                myorder = 4;
            }

            flagstat = SC_ggi(511);  // Second SC_ggi call
            if (flagstat != 1) {
                SC_P_Ai_SetMode(pl_id, SC_P_AI_MODE_BATTLE);
                myorder = 1;
            }
            break;

        case 7:  // Moving to enemy flag
            flagstat = SC_ggi(510);
            if (flagstat != 1) {
                SC_P_Ai_SetMode(pl_id, SC_P_AI_MODE_BATTLE);
                myorder = 1;
            }
            break;

        case 9:
        case 10:
            // State tracking - set myorder based on enflagstat
            // FIXED: Only set myorder = 7 if enflagstat == 1, not for all != 0
            // Disasm lines 2043-2064 show: if (==0) myorder=1; else if (==1) myorder=7; else unchanged
            if (enflagstat == 0) {
                myorder = 1;
            } else if (enflagstat == 1) {
                myorder = 7;
            }
            // if enflagstat is anything else (2+), myorder remains unchanged

            // Call both functions sequentially if myorder != 9
            if (myorder != 9) {
                if (!ProcessOrderExtended(info, state)) {
                    ProcessOrder(info, state);
                }
            }
            break;

        default:
            break;
    }
}

/*
 * ProcessOrderExtended - Extended order processing (func_1598)
 * func_1598 @1598-1721
 * Returns: 1 if order processed, 0 if not
 *
 * FIXED: Disasm analysis (lines 1603-1658):
 * - Case 0 and 2 share code (1603-1606 checks ==0, 1608-1611 checks ==2, both jump to 1612)
 * - Case 1 is separate (1655-1658)
 * Note: disasm uses data[231] as internal flag (separate from myorder)
 */
int ProcessOrderExtended(s_SC_P_info *info, int order) {
    s_sphere searchsphere;
    c_Vector3 waypoint;
    int found;
    static int internal_flag = 0;  // data[231] in disasm

    switch (order) {
        case 0:
        case 2:  // FIXED: Disasm shows case 0 and 2 share code
            // Set internal flag
            internal_flag = 1;
            // Check if near enemy current flag
            if (IsNear3DHelper(pl_id, &encurflag, 30.0f)) {
                SC_P_Ai_SetBattleModeExt(pl_id, SC_P_AI_BATTLEMODE_GOTO, &encurflag);
                myorder = 2;
                SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                SC_P_Ai_Go(pl_id, &encurflag);
                return 1;
            }
            break;

        case 1:  // FIXED: Disasm shows case 1 is separate
            // Only process if internal flag is set
            if (internal_flag != 0) {
                // Check if near enemy flag base
                if (IsNear3DHelper(pl_id, &enflag, 15.0f)) {
                    internal_flag = 0;  // Clear the flag
                    SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);

                    // Get random waypoint near enemy current flag
                    searchsphere.pos = encurflag;
                    searchsphere.rad = 30.0f;

                    found = SC_GetRndWp(&searchsphere, &waypoint);
                    if (found) {
                        SC_P_Ai_Go(pl_id, &waypoint);
                    } else {
                        SC_P_Ai_Go(pl_id, &searchsphere.pos);
                    }
                    return 0;
                }
            }
            break;

        default:
            break;
    }

    return 0;
}

/*
 * MainAILoop - Main AI behavior loop
 * func_2088 @2088-2403
 *
 * This is the main AI decision loop that:
 * - Updates flag positions
 * - Processes current state
 * - Handles movement based on myorder
 * - Updates timers
 *
 * Big switch on myorder value (1-10, 319)
 */
void MainAILoop(s_SC_P_info *info) {
    float elapsed;
    int found;
    s_sphere searchsphere;
    c_Vector3 waypoint;

    // Update flag positions and process state
    UpdateFlagPositions();
    ProcessState(info, myorder);

    // Update walk timer (disasm: data[205] -= info->elapsed_time)
    walktimer -= info->elapsed_time;

    switch (myorder) {
        case 0:
            // myorder == 0: Skip processing (just break)
            break;

        case 1:
            // Patrol mode - check for enemies
            // Disasm: if (!SC_P_Ai_GetSureEnemies) standingtimer += elapsed_time
            if (!SC_P_Ai_GetSureEnemies(pl_id)) {
                standingtimer += info->elapsed_time;
            }

            if (GetMoveDirection(pl_id)) {
                standingtimer = 0.0f;
            }

            // Disasm: threshold is 2.0f (1073741824), then endtimer gets rand()%5
            if (standingtimer > endtimer) {
                standingtimer = 0.0f;
                endtimer = (float)(rand() % 5);  // Same variable as threshold!
                DoPatrol(pl_id);
            }
            break;

        case 2:
            // Move to enemy current flag
            if (walktimer < 0.0f) {
                walktimer = 10.0f;  // Reset timer
                if (!GetMoveDirection(pl_id)) {
                    SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                    SC_P_Ai_Go(pl_id, &encurflag);
                }
            }
            break;

        case 3:
            // Move to enemy current flag (variant)
            if (!GetMoveDirection(pl_id)) {
                SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                SC_P_Ai_Go(pl_id, &encurflag);
            }
            break;

        case 4:
            // Move to own flag
            if (walktimer < 0.0f) {
                walktimer = 1.0f;  // Reset timer
                if (!GetMoveDirection(pl_id)) {
                    SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                    SC_P_Ai_Go(pl_id, &myflag);
                }
            }
            break;

        case 5:
            // Move to own flag (variant)
            if (!GetMoveDirection(pl_id)) {
                SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                SC_P_Ai_Go(pl_id, &myflag);
            }
            break;

        case 6:
            // Skip
            break;

        case 7:
            // Move to own current flag
            if (walktimer < 0.0f) {
                walktimer = 5.0f;  // Reset timer
                if (!GetMoveDirection(pl_id)) {
                    SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                    SC_P_Ai_Go(pl_id, &mycurflag);
                }
            }
            break;

        case 8:
            // Skip
            break;

        case 9:
            // Move to own current flag (variant)
            if (walktimer < 0.0f) {
                walktimer = 5.0f;  // Reset timer
                if (!GetMoveDirection(pl_id)) {
                    SC_P_Ai_Go(pl_id, &mycurflag);  // Note: no SetMoveMode call here
                }
            }
            break;

        case 10:
            // Move to own current flag (another variant)
            if (!GetMoveDirection(pl_id)) {
                SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                SC_P_Ai_Go(pl_id, &mycurflag);
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// MAIN SCRIPT ENTRY POINT
// ============================================================================

/*
 * ScriptMain - Main entry point called by engine
 * func @2404-2772
 *
 * Main state machine:
 * - gphase 0: Waiting for initialization
 * - gphase 1: CreateBot
 * - gphase 2: InitBot
 * - gphase 3: Active/Running
 * - gphase 500: Deactivated (waiting for round)
 * - gphase 1000: Dead
 * - gphase 1001: Dead, respawn timer received
 *
 * Returns: 1 to continue execution, 0 to stop
 */
int ScriptMain(s_SC_P_info *info) {
    int message;
    int param1;
    float elapsed;
    int gamestate;
    s_sphere searchsphere;
    dword players[64];
    dword count;
    c_Vector3 waypoint;
    int found;

    message = info->message;
    param1 = info->param1;
    elapsed = info->elapsed_time;

    switch (message) {
        case 4:  // SC_P_MES_INTERACT_GETTEXT - Human player pointing at this player
        case 5:  // SC_P_MES_INTERACT_DO - Human player used interaction
            // Nothing to do on interact
            enemyside = 0;
            return 1;

        case 1:  // SC_P_MES_HIT - Got hit, do nothing
            return 1;

        case 3:  // SC_P_MES_DOANIMEND - Animation ended, do nothing
            return 1;

        case 7:  // SC_P_MES_KILLED - Player died
            SC_MP_ScriptMessage(100, pl_id);
            gphase = 1000;
            return 1;

        case 2:  // SC_P_MES_EVENT - Script message received
            // Process based on param1 (message type from other scripts)
            switch (param1) {
                case 100:  // Setup battle order - go to own flag
                    myorder = 4;
                    SC_P_Ai_SetBattleModeExt(pl_id, SC_P_AI_BATTLEMODE_GOTO, &myflag);
                    SC_P_Ai_SetMoveMode(pl_id, SC_P_AI_MOVEMODE_WALK);
                    SC_P_Ai_Go(pl_id, &myflag);
                    SC_P_Ai_SetMode(pl_id, SC_P_AI_MODE_BATTLE);
                    break;

                case 666:  // Respawn time received
                    // FIXED: Disasm lines 2513-2518 show info->param2 is used, not elapsed
                    // LADR [sp-4], DADR data[8], DCP 4, ITOF -> respawntimer = (float)param2
                    respawntimer = (float)info->param2;
                    if (gphase != 1000) {
                        SC_message("SOMETHING IS WRONG! Received respawntime, but not dead! %d %d", gphase, amidead);
                    } else {
                        gphase = 1001;
                    }
                    break;

                case 2000:  // Round started
                    inittimer = 0.0f;  // Reset init timer
                    gphase = 1;
                    break;

                case 3000:  // Toggle active state
                    // FIXED: Disasm checks info at offset 8 (param2), not global amidead
                    // Lines 2551-2554: LADR [sp-4], DADR data[8], DCP 4, JZ
                    if (info->param2 != 0) {
                        SC_P_SetActive(pl_id, TRUE);
                        inittimer = 0.0f;  // Reset init timer
                        gphase = 1;
                    } else {
                        gphase = 500;
                        SC_P_SetActive(pl_id, FALSE);
                    }
                    break;

                case 4000:  // Force deactivate
                    gphase = 500;
                    SC_P_SetActive(pl_id, FALSE);
                    // Note: disasm shows only gphase and SetActive here
                    break;

                default:
                    break;
            }
            return 1;

        case 0:  // SC_P_MES_TIME - Regular tick
        default:
            break;
    }

    // Set tick timing value at start of every DOTICK
    tickvalue = 0.2f;
    info->next_exe_time = tickvalue;  // MUST set this for engine!

    // Regular tick processing based on gphase
    // Note: SC_P_IsReady check is now inside case 1, like original bot scripts
    switch (gphase) {
        case 0:
            // Create bot immediately - like all working original bot scripts
            // Original USBOT0.SCR had a SC_ggi(540) check that fails when SGI 540 = 0
            // In CTF4X4.c, SGI 540 is GVAR_BOTCOUNT_US which starts at 0
            CreateBot(info);
            info->next_exe_time = 0.05f;
            return 1;

        case 1:
            // Wait for bot to be ready, then initialize
            // Like original bot scripts: check IsReady inside the case, not outside
            if (!SC_P_IsReady(pl_id)) {
                info->next_exe_time = 0.05f;
                return 1;  // Not ready yet, try again
            }
            InitBot(info);  // Sets gphase = 2
            info->next_exe_time = tickvalue;
            return 1;

        case 2:
            // Setup battle mode
            SetupBattleMode(info);  // Sets gphase = 3 or 500
            info->next_exe_time = tickvalue;
            return 1;

        case 3:
            // Active mode - run AI based on gamestate
            // Disassembly shows: gamestate==5 → MainAILoop, gamestate==3 → UpdateEnemyTracking
            gamestate = SC_ggi(499);
            if (gamestate == 5) {
                MainAILoop(info);
            } else if (gamestate == 3) {
                UpdateEnemyTracking(info);
            }
            break;

        case 500:
            // Deactivated - disasm shows no explicit handling for gphase 500
            // (falls through to end - just waits)
            break;

        case 1000:
            // Dead - disasm shows gphase 1000 just skips (no action)
            // Waiting for message 666 to receive respawn time
            break;

        case 1001:
            // Dead - respawn timer received, try to respawn
            respawntimer -= elapsed;
            if (respawntimer < 0.0f) {
                // Check if spawn point is clear before respawning
                if (CheckNearbyPlayers(&origpos)) {
                    // FIXED: Disasm shows return value is discarded (LLD + SSP pops it)
                    // gphase = 1 is always set regardless of recover result
                    SC_MP_RecoverAiPlayer(pl_id, &origpos, origz);
                    inittimer = 0.0f;  // Reset init timer for respawn
                    gphase = 1;  // Back to init phase
                }
            }
            break;

        default:
            break;
    }

    info->next_exe_time = tickvalue;
    return 1;
}

// ============================================================================
// SCRIPT ENTRY POINTS (called by engine)
// ============================================================================

/*
 * _init - Script initialization (called once at load)
 */
void _init(void) {
    // Empty - initialization is done in CreateBot
}
