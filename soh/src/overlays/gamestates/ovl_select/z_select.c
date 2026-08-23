/*
 * File: z_select.c
 * Overlay: ovl_select
 * Description: Debug Scene Select Menu
 */

#include <libultraship/libultra.h>
#include "global.h"
#include "vt.h"
void Sram_InitDebugSave(void);

void Select_LoadTitle(SelectContext* this) {
    this->state.running = false;
    SET_NEXT_GAMESTATE(&this->state, Title_Init, TitleContext);
}

void Select_LoadGame(SelectContext* this, s32 entranceIndex) {
    osSyncPrintf(VT_FGCOL(BLUE));
    osSyncPrintf("\n\n\nＦＩＬＥ＿ＮＯ＝%x\n\n\n", gSaveContext.fileNum);
    osSyncPrintf(VT_RST);
    if (gSaveContext.fileNum == 0xFF) {
        Sram_InitDebugSave();
        gSaveContext.magicFillTarget = gSaveContext.magic;
        gSaveContext.magic = 0;
        gSaveContext.magicCapacity = 0;
        gSaveContext.magicLevel = gSaveContext.magic;
    }
    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }
    gSaveContext.forceRisingButtonAlphas = gSaveContext.unk_13E8 = gSaveContext.unk_13EA = gSaveContext.unk_13EC = 0;
    Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
    gSaveContext.entranceIndex = entranceIndex;

    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gWeatherMode = 0;
    this->state.running = false;
    SET_NEXT_GAMESTATE(&this->state, Play_Init, PlayState);
}

static SceneSelectEntry sScenes[] = {
    { " 1:SPOT00", " 1:Hyrule Field", " 1:Hylianische Steppe", " 1:Plaine d'Hyrule", Select_LoadGame,
      ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN },
    { " 2:SPOT01", " 2:Kakariko Village", " 2:Kakariko", " 2:Village Cocorico", Select_LoadGame,
      ENTR_KAKARIKO_VILLAGE_FRONT_GATE },
    { " 3:SPOT02", " 3:Graveyard", " 3:Friedhof", " 3:Cimetiere", Select_LoadGame, ENTR_GRAVEYARD_ENTRANCE },
    { " 4:SPOT03", " 4:Zora's River", " 4:Zora-Fluss", " 4:Riviere Zora", Select_LoadGame, ENTR_ZORAS_RIVER_WEST_EXIT },
    { " 5:SPOT04", " 5:Kokiri Forest", " 5:Kokiri-Wald", " 5:Foret Kokiri", Select_LoadGame, ENTR_KOKIRI_FOREST_0 },
    { " 6:SPOT05", " 6:Sacred Forest Meadow", " 6:Heilige Lichtung", " 6:Bosquet Sacre", Select_LoadGame,
      ENTR_SACRED_FOREST_MEADOW_SOUTH_EXIT },
    { " 7:SPOT06", " 7:Lake Hylia", " 7:Hylia-See", " 7:Lac Hylia", Select_LoadGame, ENTR_LAKE_HYLIA_NORTH_EXIT },
    { " 8:SPOT07", " 8:Zora's Domain", " 8:Zoras Reich", " 8:Domaine Zora", Select_LoadGame,
      ENTR_ZORAS_DOMAIN_ENTRANCE },
    { " 9:SPOT08", " 9:Zora's Fountain", " 9:Zoras Quelle", " 9:Fontaine Zora", Select_LoadGame,
      ENTR_ZORAS_FOUNTAIN_JABU_JABU_BLUE_WARP },
    { "10:SPOT09", "10:Gerudo Valley", "10:Gerudotal", "10:Vallee Gerudo", Select_LoadGame,
      ENTR_GERUDO_VALLEY_EAST_EXIT },
    { "11:SPOT10", "11:Lost Woods", "11:Verlorene Waelder", "11:Bois Perdus", Select_LoadGame,
      ENTR_LOST_WOODS_SOUTH_EXIT },
    { "12:SPOT11", "12:Desert Colossus", "12:Wuestenkoloss", "12:Colosse du Desert", Select_LoadGame,
      ENTR_DESERT_COLOSSUS_EAST_EXIT },
    { "13:SPOT12", "13:Gerudo's Fortress", "13:Gerudo-Festung", "13:Forteresse Gerudo", Select_LoadGame,
      ENTR_GERUDOS_FORTRESS_EAST_EXIT },
    { "14:SPOT13", "14:Haunted Wasteland", "14:Gespensterwueste", "14:Desert Hante", Select_LoadGame,
      ENTR_HAUNTED_WASTELAND_EAST_EXIT },
    { "15:SPOT15", "15:Hyrule Castle", "15:Schloss Hyrule", "15:Chateau d'Hyrule", Select_LoadGame,
      ENTR_CASTLE_GROUNDS_SOUTH_EXIT },
    { "16:SPOT16", "16:Death Mountain Trail", "16:Pfad zum Todesberg", "16:Chemin du Peril", Select_LoadGame,
      ENTR_DEATH_MOUNTAIN_TRAIL_BOTTOM_EXIT },
    { "17:SPOT17", "17:Death Mountain Crater", "17:Todeskrater", "17:Cratere du Peril", Select_LoadGame,
      ENTR_DEATH_MOUNTAIN_CRATER_UPPER_EXIT },
    { "18:SPOT18", "18:Goron City", "18:Goronia", "18:Village Goron", Select_LoadGame, ENTR_GORON_CITY_UPPER_EXIT },
    { "19:SPOT20", "19:Lon Lon Ranch", "19:Lon Lon-Farm", "19:Ranch Lon Lon", Select_LoadGame,
      ENTR_LON_LON_RANCH_ENTRANCE },
    { "20:" GFXP_HIRAGANA "ﾄｷﾉﾏ", "20:Temple Of Time", "20:Zitadelle der Zeit", "20:Temple du Temps", Select_LoadGame,
      ENTR_TEMPLE_OF_TIME_ENTRANCE },
    { "21:" GFXP_HIRAGANA "ｹﾝｼﾞｬﾉﾏ", "21:Chamber of Sages", "21:Halle der Weisen", "21:Sanctuaire des Sages",
      Select_LoadGame, ENTR_CHAMBER_OF_THE_SAGES_0 },
    { "22:" GFXP_HIRAGANA "ｼｬﾃｷｼﾞｮｳ", "22:Shooting Gallery", "22:Schiessbude", "22:Jeu d'adresse", Select_LoadGame,
      ENTR_SHOOTING_GALLERY_0 },
    { "23:" GFXP_KATAKANA "ﾊｲﾗﾙ" GFXP_HIRAGANA "ﾆﾜ" GFXP_KATAKANA "ｹﾞｰﾑ", "23:Castle Courtyard Game",
      "23:Burghof - Wachen", "23:Cour du Chateau (Infilration)", Select_LoadGame, ENTR_CASTLE_COURTYARD_GUARDS_DAY_0 },
    { "24:" GFXP_HIRAGANA "ﾊｶｼﾀﾄﾋﾞｺﾐｱﾅ", "24:Grave 1", "24:Grab 1", "24:Tombe 1", Select_LoadGame,
      ENTR_REDEAD_GRAVE_0 },
    { "25:" GFXP_HIRAGANA "ﾊｶｼﾀﾄﾋﾞｺﾐｱﾅ 2", "25:Grave 2", "25:Grab 2", "25:Tombe 2", Select_LoadGame,
      ENTR_GRAVE_WITH_FAIRYS_FOUNTAIN_0 },
    { "26:" GFXP_HIRAGANA "ｵｳｹ ﾉ ﾊｶｱﾅ", "26:Royal Family's Tomb", "26:Koenigsgrab", "26:Tombe Royale", Select_LoadGame,
      ENTR_ROYAL_FAMILYS_TOMB_0 },
    { "27:" GFXP_HIRAGANA "ﾀﾞｲﾖｳｾｲﾉｲｽﾞﾐ", "27:Great Fairy's Fountain (Upgrades)", "27:Feen-Quelle (Upgrades)",
      "27:Fontaine Royale des Fees (Amel.)", Select_LoadGame, ENTR_GREAT_FAIRYS_FOUNTAIN_MAGIC_DMT },
    { "28:" GFXP_HIRAGANA "ﾄﾋﾞｺﾐ ﾖｳｾｲ ｱﾅ", "28:Fairy's Fountain (Grotto)", "28:Feen-Brunnen (Grotte)",
      "28:Fontaines des Fees (Grotte)", Select_LoadGame, ENTR_FAIRYS_FOUNTAIN_0 },
    { "29:" GFXP_HIRAGANA "ﾏﾎｳｾｷ ﾖｳｾｲﾉｲｽﾞﾐ", "29:Great Fairy's Fountain (Magic)", "29:Feen-Quelle (Magie)",
      "29:Fontaine Royale des Fees (Magie)", Select_LoadGame, ENTR_GREAT_FAIRYS_FOUNTAIN_SPELLS_FARORES_ZF },
    { "30:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ｻｲｼｭｳｾﾝ", "30:Ganon's Tower - Collapsing", "30:Ganons Turm - Einsturz",
      "30:Tour de Ganon - Effondrement", Select_LoadGame, ENTR_GANONS_TOWER_COLLAPSE_EXTERIOR_0 },
    { "31:" GFXP_KATAKANA "ﾊｲﾗﾙ" GFXP_HIRAGANA "ﾅｶﾆﾜ", "31:Castle Courtyard", "31:Burghof - Zelda",
      "31:Cour du Chateau", Select_LoadGame, ENTR_CASTLE_COURTYARD_ZELDA_0 },
    { "32:" GFXP_HIRAGANA "ﾂﾘﾎﾞﾘ", "32:Fishing Pond", "32:Fischweiher", "32:Etang", Select_LoadGame,
      ENTR_FISHING_POND_0 },
    { "33:" GFXP_KATAKANA "ﾎﾞﾑﾁｭｳﾎﾞｰﾘﾝｸﾞ", "33:Bombchu Bowling Alley", "33:Minenbowlingbahn", "33:Bowling Teigneux",
      Select_LoadGame, ENTR_BOMBCHU_BOWLING_ALLEY_0 },
    { "34:" GFXP_KATAKANA "ﾛﾝﾛﾝ" GFXP_HIRAGANA "ﾎﾞｸｼﾞｮｳ ｿｳｺ 1", "34:Lon Lon Ranch House", "34:Lon Lon-Farm Haus",
      "34:Maison du Ranch Lon Lon", Select_LoadGame, ENTR_LON_LON_BUILDINGS_TALONS_HOUSE },
    { "35:" GFXP_KATAKANA "ﾛﾝﾛﾝ" GFXP_HIRAGANA "ﾎﾞｸｼﾞｮｳ ｿｳｺ 2", "35:Lon Lon Ranch Silo", "35:Lon Lon-Farm Silo",
      "35:Silo du Ranch Lon Lon", Select_LoadGame, ENTR_LON_LON_BUILDINGS_TOWER },
    { "36:" GFXP_HIRAGANA "ﾐﾊﾘ ｺﾞﾔ", "36:Guard House", "36:Wachposten", "36:Maison de Garde", Select_LoadGame,
      ENTR_MARKET_GUARD_HOUSE_0 },
    { "37:" GFXP_HIRAGANA "ﾏﾎｳ ﾉ ｸｽﾘﾔ", "37:Potion Shop", "37:Magie-Laden", "37:Apothicaire", Select_LoadGame,
      ENTR_POTION_SHOP_GRANNY_0 },
    { "38:" GFXP_HIRAGANA "ﾀｶﾗﾊﾞｺﾔ", "38:Treasure Chest Game", "38:Truhenlotterie", "38:Chasse aux Tresors",
      Select_LoadGame, ENTR_TREASURE_BOX_SHOP_0 },
    { "39:" GFXP_HIRAGANA "ｷﾝ " GFXP_KATAKANA "ｽﾀﾙﾁｭﾗ ﾊｳｽ", "39:House Of Skulltula", "39:Skulltulas Haus",
      "39:Maison des Skulltulas", Select_LoadGame, ENTR_HOUSE_OF_SKULLTULA_0 },
    { "40:" GFXP_HIRAGANA "ｼﾞｮｳｶﾏﾁ ｲﾘｸﾞﾁ", "40:Entrance to Market", "40:Eingang zum Marktplatz",
      "40:Entree de la Place du Marche", Select_LoadGame, ENTR_MARKET_ENTRANCE_NORTH_EXIT },
    { "41:" GFXP_HIRAGANA "ｼﾞｮｳｶﾏﾁ", "41:Market", "41:Marktplatz", "41:Place du Marche", Select_LoadGame,
      ENTR_MARKET_SOUTH_EXIT },
    { "42:" GFXP_HIRAGANA "ｳﾗﾛｼﾞ", "42:Back Alley", "42:Seitenstrasse", "42:Ruelle", Select_LoadGame,
      ENTR_BACK_ALLEY_DAY_0 },
    { "43:" GFXP_HIRAGANA "ﾄｷﾉｼﾝﾃﾞﾝ ﾏｴ", "43:Temple of Time Exterior", "43:Vor der Zitadelle der Zeit",
      "43:Exterieur du Temple du Temps", Select_LoadGame, ENTR_TEMPLE_OF_TIME_EXTERIOR_DAY_GOSSIP_STONE_EXIT },
    { "44:" GFXP_HIRAGANA "ﾘﾝｸﾉｲｴ", "44:Link's House", "44:Links Haus", "44:Cabane de Link", Select_LoadGame,
      ENTR_LINKS_HOUSE_CHILD_SPAWN },
    { "45:" GFXP_KATAKANA "ｶｶﾘｺ" GFXP_HIRAGANA "ﾑﾗﾉﾅｶﾞﾔ", "45:Kakariko House 1", "45:Kakariko Haus 1",
      "45:Maison du Village Cocorico 1", Select_LoadGame, ENTR_KAKARIKO_CENTER_GUEST_HOUSE_0 },
    { "46:" GFXP_HIRAGANA "ｳﾗﾛｼﾞﾉ ｲｴ", "46:Back Alley House 1", "46:Seitenstrasse Haus 1", "46:Maison de la Ruelle 1",
      Select_LoadGame, ENTR_BACK_ALLEY_MAN_IN_GREEN_HOUSE },
    { "47:" GFXP_HIRAGANA "ｺｷﾘﾉﾑﾗ ﾓﾉｼﾘｷｮｳﾀﾞｲﾉｲｴ", "47:House of the Know-it-All Brothers",
      "47:Haus der Allwissenden Brueder", "47:Cabane des Freres Je-Sais-Tout", Select_LoadGame,
      ENTR_KNOW_IT_ALL_BROS_HOUSE_0 },
    { "48:" GFXP_HIRAGANA "ｺｷﾘﾉﾑﾗ ﾌﾀｺﾞﾉｲｴ", "48:House of Twins", "48:Haus der Zwillinge", "48:Cabane des Jumeaux",
      Select_LoadGame, ENTR_TWINS_HOUSE_0 },
    { "49:" GFXP_HIRAGANA "ｺｷﾘﾉﾑﾗ " GFXP_KATAKANA "ﾐﾄﾞ" GFXP_HIRAGANA "ﾉｲｴ", "49:Mido's House", "49:Midos Haus",
      "49:Cabane du Grand Mido", Select_LoadGame, ENTR_MIDOS_HOUSE_0 },
    { "50:" GFXP_HIRAGANA "ｺｷﾘﾉﾑﾗ " GFXP_KATAKANA "ｻﾘｱ" GFXP_HIRAGANA "ﾉｲｴ", "50:Saria's House", "50:Salias Haus",
      "50:Cabane de Saria", Select_LoadGame, ENTR_SARIAS_HOUSE_0 },
    { "51:" GFXP_HIRAGANA "ｳﾏｺﾞﾔ", "51:Stable", "51:Stall", "51:Etable", Select_LoadGame, ENTR_STABLE_0 },
    { "52:" GFXP_HIRAGANA "ﾊｶﾓﾘﾉｲｴ", "52:Grave Keeper's Hut", "52:Huette des Totengraebers", "52:Cabane du Fossoyeur",
      Select_LoadGame, ENTR_GRAVEKEEPERS_HUT_0 },
    { "53:" GFXP_HIRAGANA "ｳﾗﾛｼﾞ ｲﾇｵﾊﾞｻﾝﾉｲｴ", "53:Dog Lady's House", "53:Haus der Hunde-Dame",
      "53:Maison de la Dame du Chien", Select_LoadGame, ENTR_DOG_LADY_HOUSE_0 },
    { "54:" GFXP_HIRAGANA "ｶｶﾘｺﾑﾗ " GFXP_KATAKANA "ｲﾝﾊﾟ" GFXP_HIRAGANA "ﾉｲｴ", "54:Impa's House", "54:Impas Haus",
      "54:Maison d'Impa", Select_LoadGame, ENTR_IMPAS_HOUSE_FRONT },
    { "55:" GFXP_KATAKANA "ﾊｲﾘｱ" GFXP_HIRAGANA " ｹﾝｷｭｳｼﾞｮ", "55:Lakeside Laboratory", "55:Hylia-See Laboratorium",
      "55:Laboratoire du Lac", Select_LoadGame, ENTR_LAKESIDE_LABORATORY_0 },
    { "56:" GFXP_KATAKANA "ﾃﾝﾄ", "56:Running Man's Tent", "56:Zelt des Rennlaeufers", "56:Tente du Marathonien",
      Select_LoadGame, ENTR_CARPENTERS_TENT_0 },
    { "57:" GFXP_HIRAGANA "ﾀﾃﾉﾐｾ", "57:Bazaar", "57:Basar", "57:Bazar", Select_LoadGame, ENTR_BAZAAR_0 },
    { "58:" GFXP_HIRAGANA "ｺｷﾘｿﾞｸﾉﾐｾ", "58:Kokiri Shop", "58:Kokiri-Laden", "58:Boutique Kokiri", Select_LoadGame,
      ENTR_KOKIRI_SHOP_0 },
    { "59:" GFXP_KATAKANA "ｺﾞﾛﾝ" GFXP_HIRAGANA "ﾉﾐｾ", "59:Goron Shop", "59:Goronen-Laden", "59:Boutique Goron",
      Select_LoadGame, ENTR_GORON_SHOP_0 },
    { "60:" GFXP_KATAKANA "ｿﾞｰﾗ" GFXP_HIRAGANA "ﾉﾐｾ", "60:Zora Shop", "60:Zora-Laden", "60:Boutique Zora",
      Select_LoadGame, ENTR_ZORA_SHOP_0 },
    { "61:" GFXP_KATAKANA "ｶｶﾘｺ" GFXP_HIRAGANA "ﾑﾗ  ｸｽﾘﾔ", "61:Closed Shop", "61:Geschlossener Laden",
      "61:Boutique Fermee", Select_LoadGame, ENTR_POTION_SHOP_KAKARIKO_FRONT },
    { "62:" GFXP_HIRAGANA "ｼﾞｮｳｶﾏﾁ ｸｽﾘﾔ", "62:Potion Shop", "62:Magie-Laden", "62:Apothicaire (Boutique)",
      Select_LoadGame, ENTR_POTION_SHOP_MARKET_0 },
    { "63:" GFXP_HIRAGANA "ｳﾗﾛｼﾞ ﾖﾙﾉﾐｾ", "63:Bombchu Shop", "63:Krabbelminen-Laden", "63:Boutique de Missiles Teigneux",
      Select_LoadGame, ENTR_BOMBCHU_SHOP_0 },
    { "64:" GFXP_HIRAGANA "ｵﾒﾝﾔ", "64:Happy Mask Shop", "64:Maskenhaendler", "64:Foire aux Masques", Select_LoadGame,
      ENTR_HAPPY_MASK_SHOP_0 },
    { "65:" GFXP_KATAKANA "ｹﾞﾙﾄﾞ" GFXP_HIRAGANA "ﾉｼｭｳﾚﾝｼﾞｮｳ", "65:Gerudo Training Ground", "65:Gerudo-Trainingsarena",
      "65:Gymnase Gerudo", Select_LoadGame, ENTR_GERUDO_TRAINING_GROUND_ENTRANCE },
    { "66:" GFXP_HIRAGANA "ﾖｳｾｲﾉｷﾉ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ", "66:Inside the Deku Tree", "66:Im Deku-Baum",
      "66:Arbre Mojo", Select_LoadGame, ENTR_DEKU_TREE_ENTRANCE },
    { "67:" GFXP_HIRAGANA "ﾖｳｾｲﾉｷﾉ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ ﾎﾞｽ", "67:Gohma's Lair", "67:Gohmas Verlies",
      "67:Repaire de Gohma", Select_LoadGame, ENTR_DEKU_TREE_BOSS_ENTRANCE },
    { "68:" GFXP_KATAKANA "ﾄﾞﾄﾞﾝｺﾞ ﾀﾞﾝｼﾞｮﾝ", "68:Dodongo's Cavern", "68:Dodongos Hoehle", "68:Caverne Dodongo",
      Select_LoadGame, ENTR_DODONGOS_CAVERN_ENTRANCE },
    { "69:" GFXP_KATAKANA "ﾄﾞﾄﾞﾝｺﾞ ﾀﾞﾝｼﾞｮﾝ ﾎﾞｽ", "69:King Dodongo's Lair", "69:Koenig Dodongos Verlies",
      "69:Repaire du Roi Dodongo", Select_LoadGame, ENTR_DODONGOS_CAVERN_BOSS_ENTRANCE },
    { "70:" GFXP_HIRAGANA "ｷｮﾀﾞｲｷﾞｮ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ", "70:Inside Jabu-Jabu's Belly", "70:Jabu-Jabus Bauch",
      "70:Ventre de Jabu-Jabu", Select_LoadGame, ENTR_JABU_JABU_ENTRANCE },
    { "71:" GFXP_HIRAGANA "ｷｮﾀﾞｲｷﾞｮ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ ﾎﾞｽ", "71:Barinade's Lair", "71:Barinades Verlies",
      "71:Repaire de Barinade", Select_LoadGame, ENTR_JABU_JABU_BOSS_ENTRANCE },
    { "72:" GFXP_HIRAGANA "ﾓﾘﾉｼﾝﾃﾞﾝ", "72:Forest Temple", "72:Waldtempel", "72:Temple de la Foret", Select_LoadGame,
      ENTR_FOREST_TEMPLE_ENTRANCE },
    { "73:" GFXP_HIRAGANA "ﾓﾘﾉｼﾝﾃﾞﾝ " GFXP_KATAKANA "ﾎﾞｽ", "73:Phantom Ganon's Lair", "73:Phantom-Ganons Verlies",
      "73:Repaire de Ganon Spectral", Select_LoadGame, ENTR_FOREST_TEMPLE_BOSS_ENTRANCE },
    { "74:" GFXP_HIRAGANA "ｲﾄﾞｼﾀ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ", "74:Bottom of the Well", "74:Grund des Brunnens",
      "74:Puits", Select_LoadGame, ENTR_BOTTOM_OF_THE_WELL_ENTRANCE },
    { "75:" GFXP_HIRAGANA "ﾊｶｼﾀ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ", "75:Shadow Temple", "75:Schattentempel",
      "75:Temple de l'Ombre", Select_LoadGame, ENTR_SHADOW_TEMPLE_ENTRANCE },
    { "76:" GFXP_HIRAGANA "ﾊｶｼﾀ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ ﾎﾞｽ", "76:Bongo Bongo's Lair", "76:Bongo Bongos Verlies",
      "76:Repaire de Bongo Bongo", Select_LoadGame, ENTR_SHADOW_TEMPLE_BOSS_ENTRANCE },
    { "77:" GFXP_HIRAGANA "ﾋﾉｼﾝﾃﾞﾝ", "77:Fire Temple", "77:Feuertempel", "77:Temple du Feu", Select_LoadGame,
      ENTR_FIRE_TEMPLE_ENTRANCE },
    { "78:" GFXP_HIRAGANA "ﾋﾉｼﾝﾃﾞﾝ " GFXP_KATAKANA "ﾎﾞｽ", "78:Volvagia's Lair", "78:Volvagias Verlies",
      "78:Repaire de Volcania", Select_LoadGame, ENTR_FIRE_TEMPLE_BOSS_ENTRANCE },
    { "79:" GFXP_HIRAGANA "ﾐｽﾞﾉｼﾝﾃﾞﾝ", "79:Water Temple", "79:Wassertempel", "79:Temple de l'Eau", Select_LoadGame,
      ENTR_WATER_TEMPLE_ENTRANCE },
    { "80:" GFXP_HIRAGANA "ﾐｽﾞﾉｼﾝﾃﾞﾝ " GFXP_KATAKANA "ﾎﾞｽ", "80:Morpha's Lair", "80:Morphas Verlies",
      "80:Repaire de Morpha", Select_LoadGame, ENTR_WATER_TEMPLE_BOSS_ENTRANCE },
    { "81:" GFXP_HIRAGANA "ｼﾞｬｼﾝｿﾞｳ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ", "81:Spirit Temple", "81:Geistertempel",
      "81:Temple de l'Esprit", Select_LoadGame, ENTR_SPIRIT_TEMPLE_ENTRANCE },
    { "82:" GFXP_HIRAGANA "ｼﾞｬｼﾝｿﾞｳ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ ｱｲｱﾝﾅｯｸ", "82:Iron Knuckle's Lair",
      "82:Eisenprinz' Verlies", "82:Repaire du Hache Viande", Select_LoadGame, ENTR_SPIRIT_TEMPLE_BOSS_ENTRANCE },
    { "83:" GFXP_HIRAGANA "ｼﾞｬｼﾝｿﾞｳ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ ﾎﾞｽ", "83:Twinrova's Lair", "83:Killa Ohmaz' Verlies",
      "83:Repaire du Duo Malefique", Select_LoadGame, ENTR_SPIRIT_TEMPLE_BOSS_2 },
    { "84:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ﾉﾄｳ", "84:Stairs to Ganondorf's Lair",
      "84:Treppen zu Ganondorfs Verlies", "84:Repaire de Ganondorf (Escaliers)", Select_LoadGame, ENTR_GANONS_TOWER_0 },
    { "85:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ﾉﾄｳ" GFXP_KATAKANA "ﾎﾞｽ", "85:Ganondorf's Lair",
      "85:Ganondorfs Verlies", "85:Repaire de Ganondorf", Select_LoadGame, ENTR_GANONDORF_BOSS_0 },
    { "86:" GFXP_HIRAGANA "ｺｵﾘﾉﾄﾞｳｸﾂ", "86:Ice Cavern", "86:Eishoehle", "86:Caverne Polaire", Select_LoadGame,
      ENTR_ICE_CAVERN_ENTRANCE },
    { "87:" GFXP_HIRAGANA "ﾊｶｼﾀ" GFXP_KATAKANA "ﾘﾚｰ", "87:Dampe Grave Relay Game", "87:Boris' Grab Staffellauf",
      "87:Tombe d'Igor", Select_LoadGame, ENTR_WINDMILL_AND_DAMPES_GRAVE_GRAVE },
    { "88:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ﾁｶ " GFXP_KATAKANA "ﾀﾞﾝｼﾞｮﾝ", "88:Inside Ganon's Castle",
      "88:In Ganons Schloss", "88:Tour de Ganon", Select_LoadGame, ENTR_INSIDE_GANONS_CASTLE_ENTRANCE },
    { "89:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ｻｲｼｭｳｾﾝ " GFXP_KATAKANA "ﾃﾞﾓ & ﾊﾞﾄﾙ", "89:Ganon's Lair",
      "89:Ganons Verlies", "89:Repaire de Ganon", Select_LoadGame, ENTR_GANON_BOSS_0 },
    { "90:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ﾉﾄｳ ｿﾉｺﾞ 1", "90:Escaping Ganon's Castle 1",
      "90:Flucht aus Ganons Schloss 1", "90:Fuite du Chateau de Ganon 1", Select_LoadGame,
      ENTR_GANONS_TOWER_COLLAPSE_INTERIOR_0 },
    { "91:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ﾉﾄｳ ｿﾉｺﾞ 2", "91:Escaping Ganon's Castle 2",
      "91:Flucht aus Ganons Schloss 2", "91:Fuite du Chateau de Ganon 2", Select_LoadGame,
      ENTR_GANONS_TOWER_COLLAPSE_INTERIOR_2 },
    { "92:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ﾉﾄｳ ｿﾉｺﾞ 3", "92:Escaping Ganon's Castle 3",
      "92:Flucht aus Ganons Schloss 3", "92:Fuite du Chateau de Ganon 3", Select_LoadGame,
      ENTR_GANONS_TOWER_COLLAPSE_INTERIOR_4 },
    { "93:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ﾉﾄｳ ｿﾉｺﾞ 4", "93:Escaping Ganon's Castle 4",
      "93:Flucht aus Ganons Schloss 4", "93:Fuite du Chateau de Ganon 4", Select_LoadGame,
      ENTR_GANONS_TOWER_COLLAPSE_INTERIOR_6 },
    { "94:" GFXP_KATAKANA "ｶﾞﾉﾝ" GFXP_HIRAGANA "ﾁｶ ｿﾉｺﾞ", "94:Escaping Ganon's Castle 5",
      "94:Flucht aus Ganons Schloss 5", "94:Fuite du Chateau de Ganon 5", Select_LoadGame,
      ENTR_INSIDE_GANONS_CASTLE_COLLAPSE_0 },
    { "95:" GFXP_KATAKANA "ｹﾞﾙﾄﾞ" GFXP_HIRAGANA "ﾂｳﾛ 1-2", "95:Thieves' Hideout 1-2", "95:Diebesversteck 1-2",
      "95:Repaire des Voleurs 1-2", Select_LoadGame, ENTR_THIEVES_HIDEOUT_0 },
    { "96:" GFXP_KATAKANA "ｹﾞﾙﾄﾞ" GFXP_HIRAGANA "ﾂｳﾛ 3-4 9-10", "96:Thieves' Hideout 3-4 9-10",
      "96:Diebesversteck 3-4 9-10", "96:Repaire des Voleurs 3-4 9-10", Select_LoadGame, ENTR_THIEVES_HIDEOUT_2 },
    { "97:" GFXP_KATAKANA "ｹﾞﾙﾄﾞ" GFXP_HIRAGANA "ﾂｳﾛ 5-6", "97:Thieves' Hideout 5-6", "97:Diebesversteck 5-6",
      "97:Repaire des Voleurs 5-6", Select_LoadGame, ENTR_THIEVES_HIDEOUT_4 },
    { "98:" GFXP_KATAKANA "ｹﾞﾙﾄﾞ" GFXP_HIRAGANA "ﾂｳﾛ 7-8", "98:Thieves' Hideout 7-8", "98:Diebesversteck 7-8",
      "98:Repaire des Voleurs 7-8", Select_LoadGame, ENTR_THIEVES_HIDEOUT_6 },
    { "99:" GFXP_KATAKANA "ｹﾞﾙﾄﾞ" GFXP_HIRAGANA "ﾂｳﾛ 11-12", "99:Thieves' Hideout 11-12", "99:Diebesversteck 11-12",
      "99:Repaire des Voleurs 11-12", Select_LoadGame, ENTR_THIEVES_HIDEOUT_10 },
    { "100:" GFXP_KATAKANA "ｹﾞﾙﾄﾞ" GFXP_HIRAGANA "ﾂｳﾛ 13", "100:Thieves' Hideout 13", "100:Diebesversteck 13",
      "100:Repaire des Voleurs 13", Select_LoadGame, ENTR_THIEVES_HIDEOUT_12 },
    { "101:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 0", "101:Grotto 0", "101:Grotte 0", "101:Grotte 0", Select_LoadGame,
      ENTR_GROTTOS_0 },
    { "102:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 1", "102:Grotto 1", "102:Grotte 1", "102:Grotte 1", Select_LoadGame,
      ENTR_GROTTOS_1 },
    { "103:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 2", "103:Grotto 2", "103:Grotte 2", "103:Grotte 2", Select_LoadGame,
      ENTR_GROTTOS_2 },
    { "104:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 3", "104:Grotto 3", "104:Grotte 3", "104:Grotte 3", Select_LoadGame,
      ENTR_GROTTOS_3 },
    { "105:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 4", "105:Grotto 4", "105:Grotte 4", "105:Grotte 4", Select_LoadGame,
      ENTR_GROTTOS_4 },
    { "106:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 5", "106:Grotto 5", "106:Grotte 5", "106:Grotte 5", Select_LoadGame,
      ENTR_GROTTOS_5 },
    { "107:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 6", "107:Grotto 6", "107:Grotte 6", "107:Grotte 6", Select_LoadGame,
      ENTR_GROTTOS_6 },
    { "108:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 7", "108:Grotto 7", "108:Grotte 7", "108:Grotte 7", Select_LoadGame,
      ENTR_GROTTOS_7 },
    { "109:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 8", "109:Grotto 8", "109:Grotte 8", "109:Grotte 8", Select_LoadGame,
      ENTR_GROTTOS_8 },
    { "110:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 9", "110:Grotto 9", "110:Grotte 9", "110:Grotte 9", Select_LoadGame,
      ENTR_GROTTOS_9 },
    { "111:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 10", "111:Grotto 10", "111:Grotte 10", "111:Grotte 10", Select_LoadGame,
      ENTR_GROTTOS_10 },
    { "112:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 11", "112:Grotto 11", "112:Grotte 11", "112:Grotte 11", Select_LoadGame,
      ENTR_GROTTOS_11 },
    { "113:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 12", "113:Grotto 12", "113:Grotte 12", "113:Grotte 12", Select_LoadGame,
      ENTR_GROTTOS_12 },
    { "114:" GFXP_HIRAGANA "ｶｸｼﾄﾋﾞｺﾐｱﾅ 13", "114:Grotto 13", "114:Grotte 13", "114:Grotte 13", Select_LoadGame,
      ENTR_GROTTOS_13 },
    { "115:" GFXP_KATAKANA "ﾊｲﾗﾙ ﾃﾞﾓ", "115:Goddess Cutscene Environment", "115:Goettinnen Cutscene Umgebung",
      "115:Carte de la scene des Deesses", Select_LoadGame, ENTR_CUTSCENE_MAP_0 },
    { "116:" GFXP_HIRAGANA "ﾍﾞｯｼﾂ (ﾀｶﾗﾊﾞｺ" GFXP_KATAKANA "ﾜｰﾌﾟ)", "116:Test Room", "116:Test Raum", "116:Salle de Test",
      Select_LoadGame, ENTR_BESITU_0 },
    { "117:" GFXP_HIRAGANA "ｻｻ" GFXP_KATAKANA "ﾃｽﾄ", "117:SRD Map", "117:SRD Karte", "117:Carte SRD", Select_LoadGame,
      ENTR_SASATEST_0 },
    { "118:" GFXP_KATAKANA "ﾃｽﾄﾏｯﾌﾟ", "118:Test Map", "118:Test Karte", "118:Carte de Test", Select_LoadGame,
      ENTR_TEST01_0 },
    { "119:" GFXP_KATAKANA "ﾃｽﾄﾙｰﾑ", "119:Treasure Chest Warp", "119:Schatzkisten Teleport", "119:Coffres debug",
      Select_LoadGame, ENTR_TESTROOM_0 },
    { "120:" GFXP_HIRAGANA "ﾁｭｳ" GFXP_KATAKANA "ｽﾀﾛﾌｫｽ" GFXP_HIRAGANA "ﾍﾞﾔ", "120:Stalfos Miniboss Room",
      "120:Stalfos-Ritter Miniboss Raum", "120:Salle de Miniboss Stalfos", Select_LoadGame, ENTR_SYOTES_0 },
    { "121:" GFXP_KATAKANA "ﾎﾞｽｽﾀﾛﾌｫｽ" GFXP_HIRAGANA "ﾍﾞﾔ", "121:Stalfos Boss Room", "121:Stalfos-Ritter Boss Raum",
      "121:Salle de Boss Stalfos", Select_LoadGame, ENTR_SYOTES2_0 },
    { "122:Sutaru", "122:Dark Link Room", "122:Schwarzer Link Raum", "122:Salle de Dark Link", Select_LoadGame,
      ENTR_SUTARU_0 },
    { "123:jikkenjyou", "123:Shooting Gallery Duplicate", "123:Schiessbude (Duplikat)", "123:Jeu d'adresse (Duplicata)",
      Select_LoadGame, ENTR_TEST_SHOOTING_GALLERY_0 },
    { "124:depth" GFXP_KATAKANA "ﾃｽﾄ", "124:depth test", "124:Tiefen Test", "124:Test de Profondeur", Select_LoadGame,
      ENTR_DEPTH_TEST_0 },
    { "125:" GFXP_KATAKANA "ﾊｲﾗﾙ" GFXP_HIRAGANA "ﾆﾜ" GFXP_KATAKANA "ｹﾞｰﾑ2", "125:Hyrule Garden Game (Broken)",
      "125:Burghof - Wachen-Minispiel (Kaputt)", "125:Cour du Chateau (Non fonctionnel)", Select_LoadGame,
      ENTR_HAIRAL_NIWA2_0 },
    { "title", "title", "Titelbildschirm", "Ecran-titre", Select_LoadTitle, 0 },
};

void Select_UpdateMenu(SelectContext* this) {
    Input* input = &this->state.input[0];
    s32 pad;
    SceneSelectEntry* selectedScene;

    if (this->verticalInputAccumulator == 0) {
        if (CHECK_BTN_ALL(input->press.button, BTN_A) || CHECK_BTN_ALL(input->press.button, BTN_START)) {
            selectedScene = &this->scenes[this->currentScene];
            if (selectedScene->loadFunc != NULL) {
                selectedScene->loadFunc(this, selectedScene->entranceIndex);
            }
        }

        if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
            if (LINK_AGE_IN_YEARS == YEARS_ADULT) {
                gSaveContext.linkAge = 1;
            } else {
                gSaveContext.linkAge = 0;
            }
        }

        if (CHECK_BTN_ALL(input->press.button, BTN_Z)) {
            if (gSaveContext.cutsceneIndex == 0x8000) {
                gSaveContext.cutsceneIndex = 0;
            } else if (gSaveContext.cutsceneIndex == 0) {
                gSaveContext.cutsceneIndex = 0xFFF0;
            } else if (gSaveContext.cutsceneIndex == 0xFFF0) {
                gSaveContext.cutsceneIndex = 0xFFF1;
            } else if (gSaveContext.cutsceneIndex == 0xFFF1) {
                gSaveContext.cutsceneIndex = 0xFFF2;
            } else if (gSaveContext.cutsceneIndex == 0xFFF2) {
                gSaveContext.cutsceneIndex = 0xFFF3;
            } else if (gSaveContext.cutsceneIndex == 0xFFF3) {
                gSaveContext.cutsceneIndex = 0xFFF4;
            } else if (gSaveContext.cutsceneIndex == 0xFFF4) {
                gSaveContext.cutsceneIndex = 0xFFF5;
            } else if (gSaveContext.cutsceneIndex == 0xFFF5) {
                gSaveContext.cutsceneIndex = 0xFFF6;
            } else if (gSaveContext.cutsceneIndex == 0xFFF6) {
                gSaveContext.cutsceneIndex = 0xFFF7;
            } else if (gSaveContext.cutsceneIndex == 0xFFF7) {
                gSaveContext.cutsceneIndex = 0xFFF8;
            } else if (gSaveContext.cutsceneIndex == 0xFFF8) {
                gSaveContext.cutsceneIndex = 0xFFF9;
            } else if (gSaveContext.cutsceneIndex == 0xFFF9) {
                gSaveContext.cutsceneIndex = 0xFFFA;
            } else if (gSaveContext.cutsceneIndex == 0xFFFA) {
                gSaveContext.cutsceneIndex = 0x8000;
            }
        } else if (CHECK_BTN_ALL(input->press.button, BTN_R)) {
            if (gSaveContext.cutsceneIndex == 0x8000) {
                gSaveContext.cutsceneIndex = 0xFFFA;
            } else if (gSaveContext.cutsceneIndex == 0) {
                gSaveContext.cutsceneIndex = 0x8000;
            } else if (gSaveContext.cutsceneIndex == 0xFFF0) {
                gSaveContext.cutsceneIndex = 0;
            } else if (gSaveContext.cutsceneIndex == 0xFFF1) {
                gSaveContext.cutsceneIndex = 0xFFF0;
            } else if (gSaveContext.cutsceneIndex == 0xFFF2) {
                gSaveContext.cutsceneIndex = 0xFFF1;
            } else if (gSaveContext.cutsceneIndex == 0xFFF3) {
                gSaveContext.cutsceneIndex = 0xFFF2;
            } else if (gSaveContext.cutsceneIndex == 0xFFF4) {
                gSaveContext.cutsceneIndex = 0xFFF3;
            } else if (gSaveContext.cutsceneIndex == 0xFFF5) {
                gSaveContext.cutsceneIndex = 0xFFF4;
            } else if (gSaveContext.cutsceneIndex == 0xFFF6) {
                gSaveContext.cutsceneIndex = 0xFFF5;
            } else if (gSaveContext.cutsceneIndex == 0xFFF7) {
                gSaveContext.cutsceneIndex = 0xFFF6;
            } else if (gSaveContext.cutsceneIndex == 0xFFF8) {
                gSaveContext.cutsceneIndex = 0xFFF7;
            } else if (gSaveContext.cutsceneIndex == 0xFFF9) {
                gSaveContext.cutsceneIndex = 0xFFF8;
            } else if (gSaveContext.cutsceneIndex == 0xFFFA) {
                gSaveContext.cutsceneIndex = 0xFFF9;
            }
        }

        gSaveContext.nightFlag = 0;
        if (gSaveContext.cutsceneIndex == 0) {
            gSaveContext.nightFlag = 1;
        }

        // user can change "opt", but it doesn't do anything
        if (CHECK_BTN_ALL(input->press.button, BTN_CUP)) {
            this->opt--;
        }
        if (CHECK_BTN_ALL(input->press.button, BTN_CDOWN)) {
            this->opt++;
        }

        if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
            if (this->lockUp == true) {
                this->timerUp = 0;
            }
            if (this->timerUp == 0) {
                this->timerUp = 20;
                this->lockUp = true;
                Audio_PlaySoundGeneral(NA_SE_IT_SWORD_IMPACT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                this->verticalInput = R_UPDATE_RATE;
            }
        }

        if (CHECK_BTN_ALL(input->cur.button, BTN_DUP) && this->timerUp == 0) {
            Audio_PlaySoundGeneral(NA_SE_IT_SWORD_IMPACT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            this->verticalInput = R_UPDATE_RATE * 3;
        }

        if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
            if (this->lockDown == true) {
                this->timerDown = 0;
            }
            if (this->timerDown == 0) {
                this->timerDown = 20;
                this->lockDown = true;
                Audio_PlaySoundGeneral(NA_SE_IT_SWORD_IMPACT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                this->verticalInput = -R_UPDATE_RATE;
            }
        }

        if (CHECK_BTN_ALL(input->cur.button, BTN_DDOWN) && (this->timerDown == 0)) {
            Audio_PlaySoundGeneral(NA_SE_IT_SWORD_IMPACT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            this->verticalInput = -R_UPDATE_RATE * 3;
        }

        if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT) || CHECK_BTN_ALL(input->cur.button, BTN_DLEFT)) {
            Audio_PlaySoundGeneral(NA_SE_IT_SWORD_IMPACT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            this->verticalInput = R_UPDATE_RATE;
        }

        if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT) || CHECK_BTN_ALL(input->cur.button, BTN_DRIGHT)) {
            Audio_PlaySoundGeneral(NA_SE_IT_SWORD_IMPACT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            this->verticalInput = -R_UPDATE_RATE;
        }
    }

    if (CHECK_BTN_ALL(input->press.button, BTN_L)) {
        this->pageDownIndex++;
        this->pageDownIndex =
            (this->pageDownIndex + ARRAY_COUNT(this->pageDownStops)) % ARRAY_COUNT(this->pageDownStops);
        this->currentScene = this->topDisplayedScene = this->pageDownStops[this->pageDownIndex];
    }

    this->verticalInputAccumulator += this->verticalInput;

    if (this->verticalInputAccumulator < -7) {
        this->verticalInput = 0;
        this->verticalInputAccumulator = 0;

        this->currentScene++;
        this->currentScene = (this->currentScene + this->count) % this->count;

        if (this->currentScene == ((this->topDisplayedScene + this->count + 19) % this->count)) {
            this->topDisplayedScene++;
            this->topDisplayedScene = (this->topDisplayedScene + this->count) % this->count;
        }
    }

    if (this->verticalInputAccumulator > 7) {
        this->verticalInput = 0;
        this->verticalInputAccumulator = 0;

        if (this->currentScene == this->topDisplayedScene) {
            this->topDisplayedScene -= 2;
            this->topDisplayedScene = (this->topDisplayedScene + this->count) % this->count;
        }

        this->currentScene--;
        this->currentScene = (this->currentScene + this->count) % this->count;

        if (this->currentScene == ((this->topDisplayedScene + this->count) % this->count)) {
            this->topDisplayedScene--;
            this->topDisplayedScene = (this->topDisplayedScene + this->count) % this->count;
        }
    }

    this->currentScene = (this->currentScene + this->count) % this->count;
    this->topDisplayedScene = (this->topDisplayedScene + this->count) % this->count;

    dREG(80) = this->currentScene;
    dREG(81) = this->topDisplayedScene;
    dREG(82) = this->pageDownIndex;

    if (this->timerUp != 0) {
        this->timerUp--;
    }

    if (this->timerUp == 0) {
        this->lockUp = false;
    }

    if (this->timerDown != 0) {
        this->timerDown--;
    }

    if (this->timerDown == 0) {
        this->lockDown = false;
    }
}

void Select_PrintMenu(SelectContext* this, GfxPrint* printer) {
    s32 scene;
    s32 i;
    char* name;

    GfxPrint_SetColor(printer, 255, 155, 150, 255);
    GfxPrint_SetPos(printer, 12, 2);
    GfxPrint_Printf(printer, "ZELDA MAP SELECT");
    GfxPrint_SetColor(printer, 255, 255, 255, 255);

    for (i = 0; i < 20; i++) {
        GfxPrint_SetPos(printer, 9, i + 4);

        scene = (this->topDisplayedScene + i + this->count) % this->count;
        if (scene == this->currentScene) {
            GfxPrint_SetColor(printer, 255, 20, 20, 255);
        } else {
            GfxPrint_SetColor(printer, 200, 200, 55, 255);
        }

        
            name = this->scenes[scene].japaneseName;
        
        // name = this->scenes[scene].name;
        if (name == NULL) {
            name = "**Null**";
        }

        GfxPrint_Printf(printer, "%s", name);
    };

    GfxPrint_SetColor(printer, 155, 55, 150, 255);

    GfxPrint_SetPos(printer, 20, 26);

    GfxPrint_Printf(printer, "OPT=%d", this->opt);
}

static SceneSelectLoadingMessages sLoadingMessages[] = {
    { GFXP_HIRAGANA "ｼﾊﾞﾗｸｵﾏﾁｸﾀﾞｻｲ", "Please wait a minute", "Bitte warte eine Minute",
      "Veuillez patienter une minute" },
    { GFXP_HIRAGANA "ﾁｮｯﾄ ﾏｯﾃﾈ", "Hold on a sec",
      "Warte mal 'ne Sekunde"
      "Une seconde, ca arrive" },
    { GFXP_KATAKANA "ｳｪｲﾄ ｱ ﾓｰﾒﾝﾄ", "Wait a moment", "Warte einen Moment", "Patientez un instant" },
    { GFXP_KATAKANA "ﾛｰﾄﾞ" GFXP_HIRAGANA "ﾁｭｳ", "Loading", "Ladevorgang", "Chargement" },
    { GFXP_HIRAGANA "ﾅｳ ﾜｰｷﾝｸﾞ", "Now working", "Verarbeite", "Au travail" },
    { GFXP_HIRAGANA "ｲﾏ ﾂｸｯﾃﾏｽ", "Now creating", "Erstelle...", "En cours de creation..." },
    { GFXP_HIRAGANA "ｺｼｮｳｼﾞｬﾅｲﾖ", "It's not broken", "Es ist nicht kaputt", "C'est pas casse!" },
    { GFXP_KATAKANA "ｺｰﾋｰ ﾌﾞﾚｲｸ", "Coffee Break", "Kaffee-Pause", "Pause Cafe" },
    { GFXP_KATAKANA "Bﾒﾝｦｾｯﾄｼﾃｸﾀﾞｻｲ", "Please set B side", "Please set B side", "Please set B side" },
    { GFXP_HIRAGANA "ｼﾞｯﾄ" GFXP_KATAKANA "ｶﾞﾏﾝ" GFXP_HIRAGANA "ﾉ" GFXP_KATAKANA "ｺ" GFXP_HIRAGANA "ﾃﾞｱｯﾀ",
      "Be patient, now", "Ube dich in Geduld", "Veuillez patientez" },
    { GFXP_HIRAGANA "ｲﾏｼﾊﾞﾗｸｵﾏﾁｸﾀﾞｻｲ", "Please wait just a minute", "Bitte warte noch eine Minute",
      "Patientez un peu" },
    { GFXP_HIRAGANA "ｱﾜﾃﾅｲｱﾜﾃﾅｲ｡ﾋﾄﾔｽﾐﾋﾄﾔｽﾐ｡", "Don't panic, don't panic. Take a break, take a break.",
      "Keine Panik! Nimm dir eine Auszeit", "Pas de panique. Prenez une pause." },
    { "Enough! My ship sails in the morning!", "Enough! My ship sails in the morning!",
      "Enough! My ship sails in the morning!", "Enough! My ship sails in the morning!" },
};

void Select_PrintLoadingMessage(SelectContext* this, GfxPrint* printer) {
    s32 randomMsg;

    GfxPrint_SetPos(printer, 10, 15);
    GfxPrint_SetColor(printer, 255, 255, 255, 255);
    randomMsg = Rand_ZeroOne() * ARRAY_COUNT(sLoadingMessages);
    
        GfxPrint_Printf(printer, "%s", sLoadingMessages[randomMsg].japaneseMessage);
    
}

static SceneSelectAgeLabels sAgeLabels[] = {
    { GFXP_HIRAGANA "17(ﾜｶﾓﾉ)", "17(Adult)", "17(Erwachsener)", "17(Adulte)" }, // "17(young)
    { GFXP_HIRAGANA "5(ﾜｶｽｷﾞ)", "5(Child)", "5(Kind)", "5(Enfant)" },           // "5(very young)
};

void Select_PrintAgeSetting(SelectContext* this, GfxPrint* printer, s32 age) {
    GfxPrint_SetPos(printer, 4, 26);
    GfxPrint_SetColor(printer, 255, 255, 55, 255);
    
        GfxPrint_Printf(printer, "Age:%s", sAgeLabels[age].japaneseAge);
    
}

void Select_PrintCutsceneSetting(SelectContext* this, GfxPrint* printer, u16 csIndex) {
    char* cutsceneLabels[13][4] = {
        { GFXP_HIRAGANA " ﾖﾙ " GFXP_KATAKANA "ｺﾞﾛﾝ", "Day", "Tag", "Jour" },
        { GFXP_HIRAGANA "ｵﾋﾙ " GFXP_KATAKANA "ｼﾞｬﾗ", "Night", "Nacht", "Nuit" },
        { "ﾃﾞﾓ00", "Demo00", "Demo00", "Demo00" },
        { "ﾃﾞﾓ01", "Demo01", "Demo01", "Demo01" },
        { "ﾃﾞﾓ02", "Demo02", "Demo02", "Demo02" },
        { "ﾃﾞﾓ03", "Demo03", "Demo03", "Demo03" },
        { "ﾃﾞﾓ04", "Demo04", "Demo04", "Demo04" },
        { "ﾃﾞﾓ05", "Demo05", "Demo05", "Demo05" },
        { "ﾃﾞﾓ06", "Demo06", "Demo06", "Demo06" },
        { "ﾃﾞﾓ07", "Demo07", "Demo07", "Demo07" },
        { "ﾃﾞﾓ08", "Demo08", "Demo08", "Demo08" },
        { "ﾃﾞﾓ09", "Demo09", "Demo09", "Demo09" },
        { "ﾃﾞﾓ0A", "Demo0A", "Demo0A", "Demo0A" },
    };

    char* label;
    int lang =
        (0) ? (gSaveContext.language + 1) % 4 : 0;

    GfxPrint_SetPos(printer, 4, 25);
    GfxPrint_SetColor(printer, 255, 255, 55, 255);

    switch (csIndex) {
        case 0:
            label = cutsceneLabels[1][lang];
            gSaveContext.dayTime = 0;
            break;
        case 0x8000:
            // clang-format off
            gSaveContext.dayTime = 0x8000; label = cutsceneLabels[0][lang];
            // clang-format on
            break;
        case 0xFFF0:
            // clang-format off
            gSaveContext.dayTime = 0x8000; label = cutsceneLabels[2][lang];
            // clang-format on
            break;
        case 0xFFF1:
            label = cutsceneLabels[3][lang];
            break;
        case 0xFFF2:
            label = cutsceneLabels[4][lang];
            break;
        case 0xFFF3:
            label = cutsceneLabels[5][lang];
            break;
        case 0xFFF4:
            label = cutsceneLabels[6][lang];
            break;
        case 0xFFF5:
            label = cutsceneLabels[7][lang];
            break;
        case 0xFFF6:
            label = cutsceneLabels[8][lang];
            break;
        case 0xFFF7:
            label = cutsceneLabels[9][lang];
            break;
        case 0xFFF8:
            label = cutsceneLabels[10][lang];
            break;
        case 0xFFF9:
            label = cutsceneLabels[11][lang];
            break;
        case 0xFFFA:
            label = cutsceneLabels[12][lang];
            break;
    };

    gSaveContext.skyboxTime = gSaveContext.dayTime;
    GfxPrint_Printf(printer, "Stage:" GFXP_KATAKANA "%s", label);
}

void Select_DrawMenu(SelectContext* this) {
    GraphicsContext* gfxCtx = this->state.gfxCtx;
    GfxPrint printer;

    OPEN_DISPS(gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x00, NULL);
    Gfx_SetupFrame(gfxCtx, 0, 0, 0);
    SET_FULLSCREEN_VIEWPORT(&this->view);
    func_800AAA50(&this->view, 0xF);
    Gfx_SetupDL_28Opa(gfxCtx);

    // printer = alloca(sizeof(GfxPrint));
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, POLY_OPA_DISP);
    Select_PrintMenu(this, &printer);
    Select_PrintAgeSetting(this, &printer, ((void)0, gSaveContext.linkAge));
    Select_PrintCutsceneSetting(this, &printer, ((void)0, gSaveContext.cutsceneIndex));
    POLY_OPA_DISP = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);

    CLOSE_DISPS(gfxCtx);
}

void Select_DrawLoadingScreen(SelectContext* this) {
    GraphicsContext* gfxCtx = this->state.gfxCtx;
    GfxPrint printer;

    OPEN_DISPS(gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x00, NULL);
    Gfx_SetupFrame(gfxCtx, 0, 0, 0);
    SET_FULLSCREEN_VIEWPORT(&this->view);
    func_800AAA50(&this->view, 0xF);
    Gfx_SetupDL_28Opa(gfxCtx);

    // printer = alloca(sizeof(GfxPrint));
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, POLY_OPA_DISP);
    Select_PrintLoadingMessage(this, &printer);
    POLY_OPA_DISP = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);

    CLOSE_DISPS(gfxCtx);
}

void Select_Draw(SelectContext* this) {
    GraphicsContext* gfxCtx = this->state.gfxCtx;

    OPEN_DISPS(gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x00, NULL);
    Gfx_SetupFrame(gfxCtx, 0, 0, 0);
    SET_FULLSCREEN_VIEWPORT(&this->view);
    func_800AAA50(&this->view, 0xF);

    if (!this->state.running) {
        Select_DrawLoadingScreen(this);
    } else {
        Select_DrawMenu(this);
    }

    CLOSE_DISPS(gfxCtx);
}

void Select_Main(GameState* thisx) {
    SelectContext* this = (SelectContext*)thisx;

    Select_UpdateMenu(this);
    Select_Draw(this);
}

void Select_Destroy(GameState* thisx) {
    osSyncPrintf("%c", BEL);
    // "view_cleanup will hang, so it won't be called"
    osSyncPrintf("*** view_cleanupはハングアップするので、呼ばない ***\n");
}

void Select_Init(GameState* thisx) {
    SelectContext* this = (SelectContext*)thisx;
    size_t size;

    this->state.main = Select_Main;
    this->state.destroy = Select_Destroy;
    this->scenes = sScenes;
    this->topDisplayedScene = 0;
    this->currentScene = 0;
    this->pageDownStops[0] = 0;  // Hyrule Field
    this->pageDownStops[1] = 19; // Temple Of Time
    this->pageDownStops[2] = 37; // Treasure Chest Game
    this->pageDownStops[3] = 51; // Gravekeeper's Hut
    this->pageDownStops[4] = 59; // Zora Shop
    this->pageDownStops[5] = 73; // Bottom of the Well
    this->pageDownStops[6] = 91; // Escaping Ganon's Tower 3
    this->pageDownIndex = 0;
    this->opt = 0;
    this->count = ARRAY_COUNT(sScenes);
    View_Init(&this->view, this->state.gfxCtx);
    this->view.flags = (0x08 | 0x02);
    this->verticalInputAccumulator = 0;
    this->verticalInput = 0;
    this->timerUp = 0;
    this->timerDown = 0;
    this->lockUp = 0;
    this->lockDown = 0;
    this->unk_234 = 0;

    size = (uintptr_t)_z_select_staticSegmentRomEnd - (uintptr_t)_z_select_staticSegmentRomStart;

    if ((dREG(80) >= 0) && (dREG(80) < this->count)) {
        this->currentScene = dREG(80);
        this->topDisplayedScene = dREG(81);
        this->pageDownIndex = dREG(82);
    }

    R_UPDATE_RATE = 1;
#if !defined(_MSC_VER) && !defined(__GNUC__)
    this->staticSegment = GAMESTATE_ALLOC_MC(&this->state, size);
    DmaMgr_SendRequest1(this->staticSegment, _z_select_staticSegmentRomStart, size, __FILE__, __LINE__);
#endif
    gSaveContext.cutsceneIndex = 0x8000;
    gSaveContext.linkAge = 1;
    gSaveContext.nightFlag = 0;
    gSaveContext.dayTime = 0x8000;

}
