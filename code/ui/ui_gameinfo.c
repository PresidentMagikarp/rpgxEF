// Copyright (C) 1999-2000 Id Software, Inc.
//
//
// gameinfo.c
//

#include "ui_local.h"

//
// arena and bot info
//

#define DIRLIST_SIZE 	16384
#define POOLSIZE	48 * DIRLIST_SIZE

//#define POOLSIZE	128 * 1024

int				ui_numBots;
static char		*ui_botInfos[MAX_BOTS];

static int		ui_numArenas;
static char		*ui_arenaInfos[MAX_ARENAS];

static int		ui_numSinglePlayerArenas;
static int		ui_numSpecialSinglePlayerArenas;

static char		dirlist[DIRLIST_SIZE];

static char		memoryPool[POOLSIZE];
static int		allocPoint, outOfMemory;

/*
===============
UI_Alloc
===============
*/
void *UI_Alloc(int size) {
    char	*p;

    if (allocPoint + size > POOLSIZE) {
        outOfMemory = qtrue;
        return NULL;
    }

    p = &memoryPool[allocPoint];

    allocPoint += (size + 31) & ~31;

    return p;
}

/*
===============
UI_InitMemory
===============
*/
void UI_InitMemory(void) {
    allocPoint = 0;
    outOfMemory = qfalse;
}

/*
===============
UI_ParseInfos
===============
*/
int UI_ParseInfos(char *buf, int max, char *infos[]) {
    char	*token;
    int		count;
    char	key[MAX_TOKEN_CHARS];
    char	info[MAX_INFO_STRING];

    count = 0;

    while (1) {
        token = COM_Parse(&buf);
        if (!token[0]) {
            break;
        }
        if (strcmp(token, "{")) {
            Com_Printf("Missing { in info file\n");
            break;
        }

        if (count == max) {
            Com_Printf("Max infos exceeded\n");
            break;
        }

        info[0] = '\0';
        while (1) {
            token = COM_ParseExt(&buf, qtrue);
            if (!token[0]) {
                Com_Printf("Unexpected end of info file\n");
                break;
            }
            if (!strcmp(token, "}")) {
                break;
            }
            Q_strncpyz(key, token, sizeof(key));

            token = COM_ParseExt(&buf, qfalse);
            if (!token[0]) {
                strcpy(token, "<NULL>");
            }
            Info_SetValueForKey(info, key, token);
        }
        //NOTE: extra space for arena number
        infos[count] = UI_Alloc(strlen(info) + strlen("\\num\\") + strlen(va("%d", MAX_ARENAS)) + 1);
        if (infos[count]) {
            strcpy(infos[count], info);
            count++;
        }
    }
    return count;
}

/*
===============
UI_LoadArenasFromFile
===============
*/
static void UI_LoadArenasFromFile(char *filename) {
    int				len;
    fileHandle_t	f;
    char			buf[MAX_ARENAS_TEXT];

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        trap_Print(va(S_COLOR_RED "file not found: %s\n", filename));
        return;
    }
    if (len >= MAX_ARENAS_TEXT) {
        trap_Print(va(S_COLOR_RED "file too large: %s is %i, max allowed is %i", filename, len, MAX_ARENAS_TEXT));
        trap_FS_FCloseFile(f);
        return;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);

    ui_numArenas += UI_ParseInfos(buf, MAX_ARENAS - ui_numArenas, &ui_arenaInfos[ui_numArenas]);
}

/*
===============
UI_LoadArenas
===============
*/
static void UI_LoadArenas(void) {
    int			numdirs;
    vmCvar_t	arenasFile;
    char		filename[128];
    char*		dirptr;
    int			i, n;
    int			dirlen;
    char		*type;
    char		*tag;
    int			singlePlayerNum, specialNum, otherNum;

    ui_numArenas = 0;

    trap_Cvar_Register(&arenasFile, "g_arenasFile", "", CVAR_INIT | CVAR_ROM);
    if (*arenasFile.string)
    {
        UI_LoadArenasFromFile(arenasFile.string);
    } else
    {
        UI_LoadArenasFromFile("scripts/arenas.txt");
    }

    // get all arenas from .arena files
    numdirs = trap_FS_GetFileList("scripts", ".arena", dirlist, DIRLIST_SIZE);
    dirptr = dirlist;
    for (i = 0; i < numdirs && ui_numArenas < 1024; i++, dirptr += dirlen + 1)
    {
        dirlen = strlen(dirptr);
        strcpy(filename, "scripts/");
        strcat(filename, dirptr);
        UI_LoadArenasFromFile(filename);
    }
    trap_Print(va("%i arenas parsed\n", ui_numArenas));
    if (outOfMemory) trap_Print(S_COLOR_YELLOW"WARNING: not enough memory in pool to load all arenas\n");

    // set initial numbers
    for (n = 0; n < ui_numArenas; n++) {
        Info_SetValueForKey(ui_arenaInfos[n], "num", va("%i", n));
    }

    // go through and count single players levels
    ui_numSinglePlayerArenas = 0;
    ui_numSpecialSinglePlayerArenas = 0;
    for (n = 0; n < ui_numArenas; n++) {
        // determine type
        type = Info_ValueForKey(ui_arenaInfos[n], "type");

        // if no type specified, it will be treated as "ffa"
        if (!*type) {
            continue;
        }

        if (strstr(type, "single")) {
            // check for special single player arenas (training, final)
            tag = Info_ValueForKey(ui_arenaInfos[n], "special");
            if (*tag) {
                ui_numSpecialSinglePlayerArenas++;
                continue;
            }

            ui_numSinglePlayerArenas++;
        }
    }

    //	n = ui_numSinglePlayerArenas % ARENAS_PER_TIER;
    //	if( n != 0 ) {
    //		ui_numSinglePlayerArenas -= n;
    //		trap_Print( va( "%i arenas ignored to make count divisible by %i\n", n, ARENAS_PER_TIER ) );
    //	}

        // go through once more and assign number to the levels
    singlePlayerNum = 0;
    specialNum = singlePlayerNum + ui_numSinglePlayerArenas;
    otherNum = specialNum + ui_numSpecialSinglePlayerArenas;
    for (n = 0; n < ui_numArenas; n++) {
        // determine type
        type = Info_ValueForKey(ui_arenaInfos[n], "type");

        // if no type specified, it will be treated as "ffa"
        if (*type) {
            if (strstr(type, "single")) {
                // check for special single player arenas (training, final)
                tag = Info_ValueForKey(ui_arenaInfos[n], "special");
                if (*tag) {
                    Info_SetValueForKey(ui_arenaInfos[n], "num", va("%i", specialNum++));
                    continue;
                }

                Info_SetValueForKey(ui_arenaInfos[n], "num", va("%i", singlePlayerNum++));
                continue;
            }
        }

        Info_SetValueForKey(ui_arenaInfos[n], "num", va("%i", otherNum++));
    }
}

/*
===============
UI_GetArenaInfoByNumber
===============
*/
const char *UI_GetArenaInfoByNumber(int num) {
    int		n;
    char	*value;

    if (num < 0 || num >= ui_numArenas) {
        trap_Print(va(S_COLOR_RED "Invalid arena number: %i\n", num));
        return NULL;
    }

    for (n = 0; n < ui_numArenas; n++) {
        value = Info_ValueForKey(ui_arenaInfos[n], "num");
        if (*value && atoi(value) == num) {
            return ui_arenaInfos[n];
        }
    }

    return NULL;
}

/*
===============
UI_GetArenaInfoByMap
===============
*/
const char *UI_GetArenaInfoByMap(const char *map) {
    int			n;

    for (n = 0; n < ui_numArenas; n++) {
        if (Q_stricmp(Info_ValueForKey(ui_arenaInfos[n], "map"), map) == 0) {
            return ui_arenaInfos[n];
        }
    }

    return NULL;
}

/*
===============
UI_GetSpecialArenaInfo
===============
*/
const char *UI_GetSpecialArenaInfo(const char *tag) {
    int			n;

    for (n = 0; n < ui_numArenas; n++) {
        if (Q_stricmp(Info_ValueForKey(ui_arenaInfos[n], "special"), tag) == 0) {
            return ui_arenaInfos[n];
        }
    }

    return NULL;
}

/*
===============
UI_LoadBotsFromFile
===============
*/
static void UI_LoadBotsFromFile(char *filename) {
    int				len;
    fileHandle_t	f;
    char			buf[MAX_BOTS_TEXT];

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        trap_Print(va(S_COLOR_RED "file not found: %s\n", filename));
        return;
    }
    if (len >= MAX_BOTS_TEXT) {
        trap_Print(va(S_COLOR_RED "file too large: %s is %i, max allowed is %i", filename, len, MAX_BOTS_TEXT));
        trap_FS_FCloseFile(f);
        return;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);

    ui_numBots += UI_ParseInfos(buf, MAX_BOTS - ui_numBots, &ui_botInfos[ui_numBots]);
    if (outOfMemory) trap_Print(S_COLOR_YELLOW"WARNING: not enough memory in pool to load all bots\n");
}

/*
===============
UI_LoadBots
===============
*/
static void UI_LoadBots(void) {
    vmCvar_t	botsFile;
    int			numdirs;
    char		filename[128];
    char*		dirptr;
    int			i;
    int			dirlen;

    ui_numBots = 0;

    trap_Cvar_Register(&botsFile, "g_botsFile", "", CVAR_INIT | CVAR_ROM);
    if (*botsFile.string) {
        UI_LoadBotsFromFile(botsFile.string);
    } else {
        UI_LoadBotsFromFile("scripts/bots.txt");
    }

    // get all bots from .bot files
    numdirs = trap_FS_GetFileList("scripts", ".bot", dirlist, DIRLIST_SIZE);
    dirptr = dirlist;
    for (i = 0; i < numdirs; i++, dirptr += dirlen + 1)
    {
        dirlen = strlen(dirptr);
        strcpy(filename, "scripts/");
        strcat(filename, dirptr);
        UI_LoadBotsFromFile(filename);
    }
    trap_Print(va("%i bots parsed\n", ui_numBots));
}

/*
===============
UI_GetBotInfoByNumber
===============
*/
char *UI_GetBotInfoByNumber(int num) {
    if (num < 0 || num >= ui_numBots) {
        trap_Print(va(S_COLOR_RED "Invalid bot number: %i\n", num));
        return NULL;
    }
    return ui_botInfos[num];
}

/*
===============
UI_GetBotInfoByName
===============
*/
char *UI_GetBotInfoByName(const char *name) {
    int		n;
    char	*value;

    for (n = 0; n < ui_numBots; n++) {
        value = Info_ValueForKey(ui_botInfos[n], "name");
        if (!Q_stricmp(value, name)) {
            return ui_botInfos[n];
        }
    }

    return NULL;
}

//
// single player game info
//

/*
===============
UI_GetBestScore

Returns the player's best finish on a given level, 0 if the have not played the level
===============
*/
void UI_GetBestScore(int level, int *score, int *skill) {
    int		n;
    int		skillScore;
    int		bestScore;
    int		bestScoreSkill;
    char	arenaKey[16];
    char	scores[MAX_INFO_VALUE];

    if (!score || !skill) {
        return;
    }

    if (level < 0 || level > ui_numArenas) {
        return;
    }

    bestScore = 0;
    bestScoreSkill = 0;

    for (n = 1; n <= 5; n++) {
        trap_Cvar_VariableStringBuffer(va("g_spScores%i", n), scores, MAX_INFO_VALUE);

        Com_sprintf(arenaKey, sizeof(arenaKey), "l%i", level);
        skillScore = atoi(Info_ValueForKey(scores, arenaKey));

        if (skillScore < 1 || skillScore > 8) {
            continue;
        }

        if (!bestScore || skillScore <= bestScore) {
            bestScore = skillScore;
            bestScoreSkill = n;
        }
    }

    *score = bestScore;
    *skill = bestScoreSkill;
}

/*
===============
UI_SetBestScore

Set the player's best finish for a level
===============
*/
void UI_SetBestScore(int level, int score) {
    int		skill;
    int		oldScore;
    char	arenaKey[16];
    char	scores[MAX_INFO_VALUE];

    // validate score
    if (score < 1 || score > 8) {
        return;
    }

    // validate skill
    skill = (int)trap_Cvar_VariableValue("g_spSkill");
    if (skill < 1 || skill > 5) {
        return;
    }

    // get scores
    trap_Cvar_VariableStringBuffer(va("g_spScores%i", skill), scores, MAX_INFO_VALUE);

    // see if this is better
    Com_sprintf(arenaKey, sizeof(arenaKey), "l%i", level);
    oldScore = atoi(Info_ValueForKey(scores, arenaKey));
    if (oldScore && oldScore <= score) {
        return;
    }

    // update scores
    Info_SetValueForKey(scores, arenaKey, va("%i", score));
    trap_Cvar_Set(va("g_spScores%i", skill), scores);
}

/*
===============
UI_LogAwardData
===============
*/
void UI_LogAwardData(int award, int data) {
    char	key[16];
    char	awardData[MAX_INFO_VALUE];
    int		oldValue;

    if (data == 0) {
        return;
    }

    if (award >= AWARD_MAX) {
        trap_Print(va(S_COLOR_RED "Bad award %i in UI_LogAwardData\n", award));
        return;
    }

    trap_Cvar_VariableStringBuffer("g_spAwards", awardData, sizeof(awardData));

    Com_sprintf(key, sizeof(key), "a%i", award);
    oldValue = atoi(Info_ValueForKey(awardData, key));

    Info_SetValueForKey(awardData, key, va("%i", oldValue + data));
    trap_Cvar_Set("g_spAwards", awardData);
}

/*
===============
UI_GetAwardLevel
===============
*/
int UI_GetAwardLevel(int award) {
    char	key[16];
    char	awardData[MAX_INFO_VALUE];

    trap_Cvar_VariableStringBuffer("g_spAwards", awardData, sizeof(awardData));

    Com_sprintf(key, sizeof(key), "a%i", award);
    return atoi(Info_ValueForKey(awardData, key));
}

/*
===============
UI_TierCompleted
===============
*/
int UI_TierCompleted(int levelWon) {
    int			level;
    int			n;
    int			tier;
    int			score;
    int			skill;
    int			maxarenas;
    const char	*info;

    tier = levelWon / ARENAS_PER_TIER;
    level = tier * ARENAS_PER_TIER;

    if (tier == UI_GetNumSPTiers()) {
        info = UI_GetSpecialArenaInfo("training");
        if (levelWon == atoi(Info_ValueForKey(info, "num"))) {
            return 0;
        }
        info = UI_GetSpecialArenaInfo("final");
        if (!info || levelWon == atoi(Info_ValueForKey(info, "num"))) {
            return tier + 1;
        }
        return -1;
    }

    if (uis.demoversion) {
        maxarenas = 2;
    } else {
        maxarenas = ARENAS_PER_TIER;
    }
    for (n = 0; n < maxarenas; n++, level++) {
        UI_GetBestScore(level, &score, &skill);
        if (score != 1) {
            return -1;
        }
    }
    return tier + 1;
}

/*
===============
UI_GetCurrentGame

Returns the next level the player has not won
===============
*/
int UI_GetCurrentGame(int curLevel) {
    int		level;
    int		rank;
    int		skill;
    const char *info;

    info = UI_GetSpecialArenaInfo("training");
    if (info) {
        level = atoi(Info_ValueForKey(info, "num"));
        UI_GetBestScore(level, &rank, &skill);
        if (!rank || rank > 1) {
            return level;
        }
    }

    // kef 7/31/00 -- we think we'd like to just send you to the next map, not the first map you haven't won

    if (curLevel == -1)
    {
        // -1 is a special value, the meaning of which is not currently clear to me
        for (level = 0; level < ui_numSinglePlayerArenas; level++) {
            UI_GetBestScore(level, &rank, &skill);
            if (!rank || rank > 1)
            {
                return level;
            }
        }
        info = UI_GetSpecialArenaInfo("final");
        if (!info) {
            return -1;
        }
        return atoi(Info_ValueForKey(info, "num"));
    } else if ((curLevel >= 0) && (curLevel < (ui_numSinglePlayerArenas - 1)))
    {
        // go to next map
        return curLevel + 1;
    } else if (curLevel == (ui_numSinglePlayerArenas - 1))
    {
        // finished final map...back to the beginning
        return 0;
    } else
    {
        // bogus value for curLevel
        info = UI_GetSpecialArenaInfo("final");
        if (!info) {
            return -1;
        }
        return atoi(Info_ValueForKey(info, "num"));
    }

    /*
        if ((curLevel != -1) && (curLevel == (ui_numSinglePlayerArenas-1)))
        {
            return 0;
        }

        for( level = 0; level < ui_numSinglePlayerArenas; level++ ) {
            UI_GetBestScore( level, &rank, &skill );
            if ( !rank || rank > 1 )
            {
                if (curLevel != -1)
                {
                    if (level > curLevel)
                    {
                        return level;
                    }
                    continue;
                }
                return level;
            }
        }

        info = UI_GetSpecialArenaInfo( "final" );
        if( !info ) {
            return -1;
        }
        return atoi( Info_ValueForKey( info, "num" ) );
    */
}

/*
===============
UI_NewGame

Clears the scores and sets the difficutly level
===============
*/
void UI_NewGame(void) {
    trap_Cvar_Set("g_spScores1", "");
    trap_Cvar_Set("g_spScores2", "");
    trap_Cvar_Set("g_spScores3", "");
    trap_Cvar_Set("g_spScores4", "");
    trap_Cvar_Set("g_spScores5", "");
    trap_Cvar_Set("g_spAwards", "");
    trap_Cvar_Set("g_spVideos", "");
}

/*
===============
UI_GetNumArenas
===============
*/
int UI_GetNumArenas(void) {
    return ui_numArenas;
}

/*
===============
UI_GetNumSPArenas
===============
*/
int UI_GetNumSPArenas(void) {
    return ui_numSinglePlayerArenas;
}

/*
===============
UI_GetNumSPTiers
===============
*/
int UI_GetNumSPTiers(void)
{
    int remainder;

    remainder = ui_numSinglePlayerArenas % ARENAS_PER_TIER;

    if (remainder)
    {
        remainder = 1;
    }

    return ((ui_numSinglePlayerArenas / ARENAS_PER_TIER) + remainder);
}

/*
===============
UI_GetNumBots
===============
*/
int UI_GetNumBots(void) {
    return ui_numBots;
}

/*
===============
UI_InitGameinfo
===============
*/
void UI_InitGameinfo(void) {
    UI_InitMemory();
    UI_LoadArenas();
    UI_LoadBots();

    if ((trap_Cvar_VariableValue("fs_restrict")) || (ui_numSpecialSinglePlayerArenas == 0 && ui_numSinglePlayerArenas == 2)) {
        uis.demoversion = qtrue;
    } else {
        uis.demoversion = qfalse;
    }
}