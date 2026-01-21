// Zork I: The Great Underground Empire
// (c) 1980 by INFOCOM, Inc.
// C port and parser (c) 2021 by Donnie Russell II

// This source code is provided for personal, educational use only.
// You are welcome to use this source code to develop your own works,
// but the story-related content belongs to the original authors of Zork.



#include "_def.h"
#include "_tables.h"



// 1-bit flags

unsigned char RugMoved;
unsigned char TrapOpen;
unsigned char ExitFound; // set this when player finds an exit from dungeon other than the trapdoor
unsigned char KitchenWindowOpen;
unsigned char GratingRevealed;
unsigned char GratingUnlocked;
unsigned char GratingOpen;
unsigned char GatesOpen;
unsigned char LowTide;
unsigned char GatesButton;
unsigned char LoudRoomQuiet;
unsigned char RainbowSolid;
unsigned char WonGame;
unsigned char MirrorBroken; // set NotLucky too
unsigned char RopeTiedToRail;
unsigned char SpiritsBanished;
unsigned char TrollAllowsPassage;
unsigned char YouAreSanta;
unsigned char YouAreInBoat;
unsigned char NotLucky;
unsigned char YouAreDead;
unsigned char SongbirdSang;
unsigned char ThiefHere;
unsigned char ThiefEngrossed;
unsigned char YouAreStaggered;
unsigned char BuoyFlag;

int NumMoves;
int LampTurnsLeft;
int MatchTurnsLeft;
int CandleTurnsLeft;
int MatchesLeft;
int ReservoirFillCountdown;
int ReservoirDrainCountdown;
int MaintenanceWaterLevel;
int DownstreamCounter;
int BellRungCountdown; // these three are for ceremony
int CandlesLitCountdown;
int BellHotCountdown;
int CaveHoleDepth;
int Score;
int NumDeaths;
int CyclopsCounter;
int CyclopsState; // 0: default  1: hungry  2: thirsty  3: asleep  4: fled
int LoadAllowed;
int PlayerStrength;
int TrollDescType;
int ThiefDescType; // 0: default  1: unconscious
int EnableCureRoutine; // countdown

unsigned char VillainAttacking[NUM_VILLAINS];
unsigned char VillainStaggered[NUM_VILLAINS];
int VillainWakingChance[NUM_VILLAINS];
int VillainStrength[NUM_VILLAINS];



//from parser.c
extern int NumStrWords;
extern char *StrWord[80];
extern int CurWord;
extern int ItObj;
extern unsigned char TimePassed;
extern unsigned char GameOver;



//*****************************************************************************
// returns 1 if event of x% chance occurred
// second parameter is used instead if it is >=0 and you're not lucky
int PercentChance(int x, int x_not_lucky)
{
  if (NotLucky && x_not_lucky >= 0) x = x_not_lucky;

  if (GetRandom(100) < x) return 1;
  else return 0;
}
//*****************************************************************************



//*****************************************************************************
void ScatterInventory(void)
{
  int obj;

  if (Obj[OBJ_LAMP].loc == INSIDE + OBJ_YOU)
    Obj[OBJ_LAMP].loc = ROOM_LIVING_ROOM;

  if (Obj[OBJ_COFFIN].loc == INSIDE + OBJ_YOU)
    Obj[OBJ_COFFIN].loc = ROOM_EGYPT_ROOM;

  Obj[OBJ_SWORD].thiefvalue = 0;


  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == INSIDE + OBJ_YOU)
  {
    int room = NUM_ROOMS;

    if (Obj[obj].thiefvalue > 0)
      for (room=1; room<NUM_ROOMS; room++)
        if ((Room[room].prop & R_BODYOFWATER) == 0 &&
            (Room[room].prop & R_LIT) == 0 &&
            GetRandom(2) == 0) break;

    if (room == NUM_ROOMS)
    {
      int above_ground[11] =
        { ROOM_WEST_OF_HOUSE, ROOM_NORTH_OF_HOUSE, ROOM_EAST_OF_HOUSE,
          ROOM_SOUTH_OF_HOUSE, ROOM_FOREST_1, ROOM_FOREST_2, ROOM_FOREST_3,
          ROOM_PATH, ROOM_CLEARING, ROOM_GRATING_CLEARING, ROOM_CANYON_VIEW };

      room = above_ground[GetRandom(11)];
    }

    Obj[obj].loc = room;
  }
}



void YoureDead(void)
{
  if (YouAreDead)
  {
    PrintCompLine("\x0a\x49\xa6\x74\x61\x6b\xbe\xa3\x9f\xe2\xd4\xd1\xab\xfc\x72\x73\xca\x89\xef\x20\x6b\x69\xdf\xd5\xb7\xce\xcf\xa3\x6c\xa9\x61\x64\xc4\xe8\x61\x64\xa4\x59\x4f\x55\xa3\xa9\xaa\x75\xfa\xa3\x9f\xe2\xd4\x74\xa4\x55\x6e\x66\xd3\x74\xf6\xaf\x65\xec\xb5\xc7\x9f\x61\x6b\xbe\xa3\x9f\xe2\xd4\xd1\xab\xfc\x72\x73\xca\x89\xe8\xe2\xb7\xc7\xde\xc7\xa4\x49\xa3\xf9\xe3\xa6\x73\x75\xfa\xa3\x9f\xe2\xd4\x74\xa4\x53\xd3\x72\x79\x2e");
    GameOver = 1;
    return;
  }

  if (NotLucky)
    PrintCompLine("\x42\x61\xab\x6c\x75\x63\x6b\xb5\x68\x75\x68\x3f");

  PrintCompLine("\x0a\x20\x20\x20\x20\x2a\x2a\x2a\x2a\x20\x88\xc0\x61\xd7\xcc\x69\xd5\x20\x20\x2a\x2a\x2a\x2a\x0a\x0a");

  NumDeaths++;
  if (NumDeaths == 3)
  {
    PrintCompLine("\x8b\x63\xcf\xbb\xec\xa3\xa9\xa3\xaa\x75\x69\x63\x69\x64\xe2\xee\xad\x69\x61\x63\xa4\x20\x57\x9e\x64\xca\x27\xa6\xe2\xd9\x77\xeb\x73\x79\xfa\xff\x69\x63\xa1\xa7\x80\xb3\x61\xd7\xb5\x73\xa7\x63\x9e\x96\xc4\x6d\x61\xc4\xcd\x72\xf9\xff\xa0\xb6\x61\x64\xd7\xe5\xd8\xac\x73\xa4\x88\xb6\xa9\x6d\x61\xa7\xa1\xf8\xdf\xb0\x9e\xa7\xc5\xe2\xcf\xab\xa7\x80\x20\x4c\x8c\xdd\x80\x20\x4c\x69\x76\x84\x44\xbf\x64\xb5\x77\xa0\xa9\x86\xb6\x66\x65\xdf\xf2\xa3\x64\xd7\xe5\xd8\xac\xa1\x6d\x61\xc4\x67\xd9\xaf\xae\xd7\xb6\x96\x6d\x2e");
    GameOver = 1;
    return;
  }

  YouAreInBoat = 0; // in case you're in it
  ExitFound = 1;
  ScatterInventory();

  if (Room[ROOM_SOUTH_TEMPLE].prop & R_DESCRIBED)
  {
    PrintCompLine("\x41\xa1\x8f\x74\x61\x6b\x9e\x92\xcb\xe0\xa6\x62\xa9\xaf\x68\xb5\x8f\x66\xf3\xea\xa9\xf5\x65\xd7\xab\xdd\x86\xb6\x62\xd8\xe8\x6e\x73\x83\x9e\x66\xf3\xf5\x9c\x70\xe0\xd6\xa1\xe0\x86\xc6\xa7\xab\x92\xd6\x6c\xd2\xef\x66\xd3\x9e\x81\x67\xaf\xbe\x8a\x48\x65\xdf\xb5\x77\xa0\xa9\x80\xaa\x70\x69\xf1\x74\xa1\x6a\xf3\xb6\xaf\x86\x8d\xcc\xd4\xc4\x8f\xd4\x74\x72\x79\xa4\x88\xb6\xd6\x6e\xd6\xa1\xbb\x9e\x64\xb2\x74\xd8\xef\x64\xa4\x82\xae\x62\x6a\x65\x63\x74\xa1\xa7\x80\xcc\xf6\x67\x65\xca\xa3\x70\xfc\xbb\xa8\xb9\xb2\xf0\x6e\x63\x74\xb5\x62\xcf\x61\xfa\xd5\x8a\x63\x6f\xd9\x72\xb5\x65\xd7\xb4\xf6\xa9\xe2\x2e\x0a");
    YouAreDead = 1;
    TrollAllowsPassage = 1;
    Obj[OBJ_LAMP].prop |= PROP_NODESC;
    Obj[OBJ_LAMP].prop |= PROP_NOTTAKEABLE;
    Obj[OBJ_YOU].prop |= PROP_LIT;
    Obj[OBJ_YOU].loc = ROOM_ENTRANCE_TO_HADES;
    PrintPlayerRoomDesc(0);
  }
  else
  {
    PrintCompLine("\x4e\xf2\xb5\xcf\x74\x27\xa1\x74\x61\x6b\x9e\xd0\xd9\x6f\x6b\xc0\xac\x65\x2e\x2e\xa4\x57\x65\xdf\xb5\x8f\x70\xc2\x62\x61\x62\xec\xcc\xbe\xac\xd7\xa3\xe3\x96\xb6\xfa\xad\x63\x65\xa4\x20\x49\x91\x27\xa6\x71\x75\xc7\x9e\x66\x69\x78\x86\x20\x75\x70\xb3\xe1\x70\xcf\xd1\xec\xb5\x62\xf7\x86\x91\x27\xa6\xcd\xd7\xfb\xd7\x72\x79\xa2\x97\x2e\x0a");
    Obj[OBJ_YOU].loc = ROOM_FOREST_1;
    PrintPlayerRoomDesc(0);
  }
}
//*****************************************************************************



//*****************************************************************************
//these functions return 1 if action completed; otherwise fall through



int GoToRoutine(int newroom)
{
  int prev_darkness;

  if (YouAreInBoat)
  {
    PrintCompLine("\xdc\x75\x27\xdf\xc0\x61\xd7\x89\x67\x65\xa6\xa5\xa6\xdd\x80\xb0\x6f\xaf\xc6\x69\x72\x73\x74\x2e");
    return 1;
  }

  prev_darkness = IsPlayerInDarkness();

  Obj[OBJ_YOU].loc = newroom;
  TimePassed = 1;

  if (IsPlayerInDarkness())
  {
    if (prev_darkness)
    {
      //kill player that tried to walk from dark to dark
      PrintCompLine("\x0a\x0a\x0a\x0a\x0a\x4f\x68\xb5\xe3\x21\x88\xc0\x61\xd7\xb7\xe2\x6b\xd5\xa8\xe5\xba\x81\x73\xfd\xd7\xf1\x9c\x66\xad\x67\xa1\xdd\xa3\xcb\xd8\x6b\x84\x67\x72\x75\x65\x21");
      YoureDead(); // ##### RIP #####
      return 1;
    }
    else PrintCompLine("\x8b\xcd\xd7\xee\x6f\xd7\xab\xa7\xbd\xa3\xcc\xbb\x6b\xeb\xfd\x63\x65\x2e");
  }

  PrintPlayerRoomDesc(0);
  return 1;
}



int GoFrom_StoneBarrow_West(void)
{
  PrintCompLine("\x49\x6e\x73\x69\xe8\x80\x20\x42\xbb\xc2\x77\x0a\x41\xa1\x8f\xd4\xd1\xb6\x81\x62\xbb\xc2\x77\xb5\x81\x64\xe9\xb6\x63\xd9\xd6\xa1\xa7\x65\x78\xd3\x61\x62\xec\xb0\x65\xce\xb9\x86\xa4\x41\xc2\xf6\xab\x8f\xc7\x87\x64\xbb\x6b\xb5\x62\xf7\xa3\xa0\x61\xab\x9a\xad\xfb\xe3\x72\x6d\xa5\xa1\xe7\xd7\x72\x6e\xb5\x62\xf1\x67\x68\x74\xec\xcb\xc7\x83\xc2\x75\x67\xde\xc7\xa1\x63\xd4\xd1\xb6\x72\xf6\xa1\xd0\xf8\xe8\xaa\x74\xa9\x61\x6d\xa4\x53\x70\xad\x6e\x84\x81\xc5\xa9\x61\xf9\x9a\xd0\x73\x6d\xe2\xea\x77\xe9\xe8\xb4\x66\xe9\x74\x62\xf1\x64\x67\x65\xb5\x8c\xef\xc9\xb9\xa3\xeb\xaf\xde\xcf\x61\x64\xa1\xa7\xbd\xa3\xcc\xbb\x6b\x9f\xf6\xed\x6c\xa4\x41\x62\x6f\xd7\x80\xb0\xf1\x64\x67\x65\xb5\x66\xd9\xaf\x84\xa7\x80\xa3\x69\x72\xb5\x9a\xd0\xfd\x72\x67\x9e\x73\x69\x67\x6e\xa4\x49\xa6\xa9\x61\x64\x73\x3a\x20\x20\x41\xdf\xc8\x9e\x77\x68\xba\xc5\x8c\xef\x66\xd3\x9e\xa2\x9a\x62\xf1\x64\x67\x9e\xcd\xd7\xb3\xe1\x70\xcf\xd1\xab\xd0\x67\xa9\xaf\x8d\xeb\xac\x69\xd9\xfe\xa3\x64\xd7\xe5\xd8\x9e\x77\xce\xfa\xc0\xe0\x9f\xbe\xd1\xab\x92\xb7\xc7\x8d\xb3\xa5\xf4\x67\x65\x8e\xc3\xcd\xd7\xee\xe0\xd1\xa9\xab\x81\x66\x69\x72\xc5\xeb\xbb\xa6\xdd\x80\x20\x5a\x4f\x52\x4b\x9f\xf1\xd9\x67\x79\x83\x6f\xd6\xb7\x68\xba\x70\xe0\xa1\x6f\xd7\xb6\xa2\x9a\x62\xf1\x64\x67\x9e\x6d\xfe\xa6\xef\xeb\xa9\x70\xbb\xd5\x89\xf6\xe8\x72\x74\x61\x6b\x9e\xad\xfb\xd7\xb4\x67\xa9\xaf\xac\xa3\x64\xd7\xe5\xd8\x9e\xa2\xaf\xb7\x69\xdf\xaa\x65\xd7\xa9\xec\x9f\xbe\xa6\x92\xaa\x6b\x69\xdf\x8d\xb0\xf4\xd7\x72\x79\x21\x0a\x0a\x85\x5a\x4f\x52\x4b\x9f\xf1\xd9\x67\xc4\x63\xca\xf0\x6e\x75\xbe\xb7\xc7\xde\x22\x5a\x4f\x52\x4b\x20\x49\x49\x3a\x82\x20\x57\x69\x7a\xbb\xab\xdd\x20\x46\xc2\x62\x6f\x7a\x7a\x22\x8d\x87\x63\xe1\x70\xcf\xd1\xab\xa7\x20\x22\x5a\x4f\x52\x4b\x20\x49\x49\x49\x3a\x82\x20\x44\xf6\x67\x65\xca\x20\x4d\xe0\xd1\x72\x2e\x22");
  GameOver = 1;
  return 1;
}



int GoFrom_WestOfHouse_Southwest(void)
{
  if (WonGame == 0) return 0;
  else return GoToRoutine(ROOM_STONE_BARROW);
}



int GoFrom_EastOfHouse_West(void)
{
  if (KitchenWindowOpen == 0)
  {
    PrintCompLine("\x85\xf8\xb9\xf2\x87\x63\xd9\xd6\x64\x2e");
    ItObj = FOBJ_KITCHEN_WINDOW;
    return 1;
  }
  else return GoToRoutine(ROOM_KITCHEN);
}



int GoFrom_Kitchen_East(void)
{
  if (KitchenWindowOpen == 0)
  {
    PrintCompLine("\x85\xf8\xb9\xf2\x87\x63\xd9\xd6\x64\x2e");
    ItObj = FOBJ_KITCHEN_WINDOW;
    return 1;
  }
  else return GoToRoutine(ROOM_EAST_OF_HOUSE);
}



int GoFrom_LivingRoom_West(void)
{
  if (CyclopsState == 4) // fled
    return GoToRoutine(ROOM_STRANGE_PASSAGE);
  else
    {PrintCompLine("\x85\x64\xe9\xb6\x9a\x6e\x61\x69\xcf\xab\x73\x68\x75\x74\x2e"); return 1;}
}



int GoFrom_Cellar_Up(void)
{
  if (TrapOpen == 0)
  {
    PrintCompLine("\x85\x74\xf4\x70\xcc\xe9\xb6\x9a\x63\xd9\xd6\x64\x2e");
    ItObj = FOBJ_TRAP_DOOR;
  }
  else
    return GoToRoutine(ROOM_LIVING_ROOM);

  return 1;
}



int GoFrom_TrollRoom_East(void)
{
  if (TrollAllowsPassage == 0) {PrintCompLine("\x85\x74\xc2\xdf\xc6\xd4\x64\xa1\x8f\xdd\xd2\xf8\xa2\xa3\xee\xd4\x61\x63\x84\x67\xbe\x74\xd8\x65\x2e"); return 1;}
  else return GoToRoutine(ROOM_EW_PASSAGE);
}



int GoFrom_TrollRoom_West(void)
{
  if (TrollAllowsPassage == 0) {PrintCompLine("\x85\x74\xc2\xdf\xc6\xd4\x64\xa1\x8f\xdd\xd2\xf8\xa2\xa3\xee\xd4\x61\x63\x84\x67\xbe\x74\xd8\x65\x2e"); return 1;}
  else return GoToRoutine(ROOM_MAZE_1);
}



int GoFrom_GratingRoom_Up(void)
{
  if (GratingOpen == 0)
  {
    PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\x63\xd9\xd6\x64\x2e");
    ItObj = FOBJ_GRATE;
  }
  else
  {
    ExitFound = 1;
    return GoToRoutine(ROOM_GRATING_CLEARING);
  }

  return 1;
}



int GoFrom_CyclopsRoom_East(void)
{
  if (CyclopsState == 4) // fled
    return GoToRoutine(ROOM_STRANGE_PASSAGE);
  else
    {PrintCompLine("\x85\xbf\xc5\xb7\xe2\xea\x9a\x73\x6f\xf5\xab\xc2\x63\x6b\x2e"); return 1;}
}



int GoFrom_CyclopsRoom_Up(void)
{
  if (CyclopsState == 3 || Obj[OBJ_CYCLOPS].loc == 0) // sleeping or dead
  {
    if (YouAreInBoat == 0) ThiefProtectsTreasure();
    return GoToRoutine(ROOM_TREASURE_ROOM);
  }
  else
    PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x64\x6f\xbe\x93\xd9\x6f\x6b\xcb\x69\x6b\x9e\xa0\x27\xdf\xcb\x65\xa6\x8f\x70\xe0\x74\x2e");

  return 1;
}



int GoFrom_ReservoirSouth_North(void)
{
  if (LowTide == 0) {PrintCompLine("\x8b\x77\xa5\x6c\xab\x64\xc2\x77\x6e\x2e"); return 1;}
  else return GoToRoutine(ROOM_RESERVOIR);
}



int GoFrom_ReservoirNorth_South(void)
{
  if (LowTide == 0) {PrintCompLine("\x8b\x77\xa5\x6c\xab\x64\xc2\x77\x6e\x2e"); return 1;}
  else return GoToRoutine(ROOM_RESERVOIR);
}



int GoFrom_EntranceToHades_South(void)
{
  if (SpiritsBanished == 0) {PrintCompLine("\x53\xe1\x9e\xa7\x76\xb2\x69\x62\xcf\xc6\xd3\x63\x9e\x70\xa9\xd7\xe5\xa1\x8f\x66\xc2\xf9\x70\xe0\x73\x84\xa2\xc2\x75\x67\xde\x81\x67\xaf\x65\x2e"); return 1;}
  else return GoToRoutine(ROOM_LAND_OF_LIVING_DEAD);
}



int GoFrom_DomeRoom_Down(void)
{
  if (RopeTiedToRail == 0) {PrintCompLine("\x8b\xe7\x6e\xe3\xa6\x67\xba\x64\xf2\xb4\xf8\xa2\xa5\xa6\x66\xf4\x63\x74\xd8\x84\x6d\xad\xc4\x62\xca\x65\x73\x2e"); return 1;}
  else return GoToRoutine(ROOM_TORCH_ROOM);
}



int GoFrom_OntoRainbowRoutine(void)
{
  if (RainbowSolid == 0) return 0;
  else return GoToRoutine(ROOM_ON_RAINBOW);
}



int GoFrom_Maze2_Down(void)
{
  PrintCompLine("\x8b\x77\xca\x27\xa6\xef\xa3\x62\xcf\x89\x67\x65\xa6\x62\x61\x63\x6b\x20\x75\x70\x89\x81\x74\xf6\xed\xea\x8f\xbb\x9e\x67\x6f\x84\xa2\xc2\x75\x67\xde\x77\xa0\xb4\xc7\xe6\x65\x74\xa1\xbd\x80\xe4\x65\x78\xa6\xc2\xe1\x2e\x0a");
  return GoToRoutine(ROOM_MAZE_4);
}



int GoFrom_Maze7_Down(void)
{
  PrintCompLine("\x8b\x77\xca\x27\xa6\xef\xa3\x62\xcf\x89\x67\x65\xa6\x62\x61\x63\x6b\x20\x75\x70\x89\x81\x74\xf6\xed\xea\x8f\xbb\x9e\x67\x6f\x84\xa2\xc2\x75\x67\xde\x77\xa0\xb4\xc7\xe6\x65\x74\xa1\xbd\x80\xe4\x65\x78\xa6\xc2\xe1\x2e\x0a");
  return GoToRoutine(ROOM_DEAD_END_1);
}



int GoFrom_Maze9_Down(void)
{
  PrintCompLine("\x8b\x77\xca\x27\xa6\xef\xa3\x62\xcf\x89\x67\x65\xa6\x62\x61\x63\x6b\x20\x75\x70\x89\x81\x74\xf6\xed\xea\x8f\xbb\x9e\x67\x6f\x84\xa2\xc2\x75\x67\xde\x77\xa0\xb4\xc7\xe6\x65\x74\xa1\xbd\x80\xe4\x65\x78\xa6\xc2\xe1\x2e\x0a");
  return GoToRoutine(ROOM_MAZE_11);
}



int GoFrom_Maze12_Down(void)
{
  PrintCompLine("\x8b\x77\xca\x27\xa6\xef\xa3\x62\xcf\x89\x67\x65\xa6\x62\x61\x63\x6b\x20\x75\x70\x89\x81\x74\xf6\xed\xea\x8f\xbb\x9e\x67\x6f\x84\xa2\xc2\x75\x67\xde\x77\xa0\xb4\xc7\xe6\x65\x74\xa1\xbd\x80\xe4\x65\x78\xa6\xc2\xe1\x2e\x0a");
  return GoToRoutine(ROOM_MAZE_5);
}



int GoFrom_GratingClearing_Down(void)
{
  if (GratingRevealed == 0)
    PrintBlockMsg(BL0);
  else
  {
    if (GratingOpen == 0)
    {
      PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\x63\xd9\xd6\x64\x2e");
      ItObj = FOBJ_GRATE;
    }
    else
      return GoToRoutine(ROOM_GRATING_ROOM);
  }

  return 1;
}



int GoFrom_LivingRoom_Down(void)
{
  if (TrapOpen)
  {
    if (YouAreInBoat)
      PrintCompLine("\xdc\x75\x27\xdf\xc0\x61\xd7\x89\x67\x65\xa6\xa5\xa6\xdd\x80\xb0\x6f\xaf\xc6\x69\x72\x73\x74\x2e");
    else
    {
      GoToRoutine(ROOM_CELLAR);
      if (YouAreDead == 0 && ExitFound == 0)
      {
        TrapOpen = 0;
        PrintCompLine("\x85\x74\xf4\x70\xcc\xe9\xb6\x63\xf4\x73\xa0\xa1\x73\x68\xf7\xb5\x8c\x8f\xa0\xbb\xaa\xe1\x65\xca\x9e\x62\xbb\xf1\x9c\x69\x74\x2e");
      }
    }
  }
  else if (RugMoved == 0)
    PrintBlockMsg(BL0);
  else
  {
    PrintCompLine("\x85\x74\xf4\x70\xcc\xe9\xb6\x9a\x63\xd9\xd6\x64\x2e");
    ItObj = FOBJ_TRAP_DOOR;
  }

  return 1;
}



int GoFrom_SouthTemple_Down(void)
{
  if (Obj[OBJ_COFFIN].loc == INSIDE + OBJ_YOU)
    {PrintCompLine("\x8b\xcd\xd7\x93\xd0\x70\xf4\x79\xac\x8a\x67\x65\x74\xf0\x9c\x81\x63\xdd\x66\xa7\xcc\xf2\xb4\x96\x72\x65\x2e"); return 1;}
  else return GoToRoutine(ROOM_TINY_CAVE);
}



int GoFrom_WhiteCliffsNorth_South(void)
{
  if (Obj[OBJ_INFLATED_BOAT].loc == INSIDE + OBJ_YOU)
    {PrintCompLine("\x85\x70\xaf\xde\x9a\xbd\xba\x6e\xbb\xc2\x77\x2e"); return 1;}
  else return GoToRoutine(ROOM_WHITE_CLIFFS_SOUTH);
}



int GoFrom_WhiteCliffsNorth_West(void)
{
  if (Obj[OBJ_INFLATED_BOAT].loc == INSIDE + OBJ_YOU)
    {PrintCompLine("\x85\x70\xaf\xde\x9a\xbd\xba\x6e\xbb\xc2\x77\x2e"); return 1;}
  else return GoToRoutine(ROOM_DAMP_CAVE);
}



int GoFrom_WhiteCliffsSouth_North(void)
{
  if (Obj[OBJ_INFLATED_BOAT].loc == INSIDE + OBJ_YOU)
    {PrintCompLine("\x85\x70\xaf\xde\x9a\xbd\xba\x6e\xbb\xc2\x77\x2e"); return 1;}
  else return GoToRoutine(ROOM_WHITE_CLIFFS_NORTH);
}



int GoFrom_TimberRoom_West(void)
{
  if (YouAreDead)
    {PrintCompLine("\x8b\xe7\x6e\xe3\xa6\xd4\xd1\xb6\xa7\x86\xb6\x63\xca\x64\xc7\x69\x6f\x6e\x2e"); return 1;}
  else if (GetNumObjectsInLocation(INSIDE + OBJ_YOU) > 0)
    {PrintCompLine("\x8b\xe7\x6e\xe3\xa6\x66\xc7\x95\xc2\x75\x67\xde\xa2\x9a\x70\xe0\x73\x61\x67\x9e\xf8\xa2\x95\xaf\xcb\x6f\x61\x64\x2e"); return 1;}
  else return GoToRoutine(ROOM_LOWER_SHAFT);
}



int GoFrom_LowerShaft_East(void)
{
  if (GetNumObjectsInLocation(INSIDE + OBJ_YOU) > 0)
    {PrintCompLine("\x8b\xe7\x6e\xe3\xa6\x66\xc7\x95\xc2\x75\x67\xde\xa2\x9a\x70\xe0\x73\x61\x67\x9e\xf8\xa2\x95\xaf\xcb\x6f\x61\x64\x2e"); return 1;}
  else return GoToRoutine(ROOM_TIMBER_ROOM);
}



int GoFrom_Kitchen_Down(void)
{
  if (YouAreSanta == 0)
    PrintCompLine("\x4f\x6e\xec\x20\x53\xad\x74\xd0\x43\xfd\xfe\xb3\xf5\x6d\x62\xa1\x64\xf2\xb4\xfa\x69\x6d\xed\x79\x73\x2e");
  else
    return GoToRoutine(ROOM_STUDIO);

  return 1;
}



int GoFrom_Studio_Up(void)
{
  int count = GetNumObjectsInLocation(INSIDE + OBJ_YOU);

  if (count == 0)
    PrintCompLine("\x47\x6f\x84\x75\x70\xfb\x6d\x70\x74\x79\x2d\xcd\xb9\xd5\x87\xd0\x62\x61\xab\x69\xe8\x61\x2e");
  else if (count < 3 && Obj[OBJ_LAMP].loc == INSIDE + OBJ_YOU)
    return GoToRoutine(ROOM_KITCHEN);
  else
    PrintCompLine("\x8b\xe7\x93\x67\x65\xa6\x75\x70\x80\xa9\xb7\xc7\xde\x77\xcd\xa6\xc9\x75\x27\xa9\xb3\xbb\x72\x79\x97\x2e");

  return 1;
}



int GoFrom_LandOfLivingDead_North(void)
{
  return GoToRoutine(ROOM_ENTRANCE_TO_HADES);
}



int GoFrom_StrangePassage_West(void)
{
  return GoToRoutine(ROOM_CYCLOPS_ROOM);
}



int GoFrom_NorthTemple_North(void)
{
  return GoToRoutine(ROOM_TORCH_ROOM);
}



int GoFrom_MineEntrance_West(void)
{
  return GoToRoutine(ROOM_SQUEEKY_ROOM);
}



int GoFrom_DamLobby_North_Or_East(void)
{
  if (MaintenanceWaterLevel > 14) {PrintCompLine("\x85\xc2\xe1\x87\x66\x75\xdf\x8a\x77\xaf\xac\x8d\x91\xe3\xa6\xef\xfb\xe5\xac\x65\x64\x2e"); return 1;}
  else return GoToRoutine(ROOM_MAINTENANCE_ROOM);
}



//A_IN and A_OUT can also be handled here
struct GOFROM_STRUCT GoFrom[] =
{
  { ROOM_STONE_BARROW         , A_WEST      , GoFrom_StoneBarrow_West       },
  { ROOM_STONE_BARROW         , A_IN        , GoFrom_StoneBarrow_West       },
  { ROOM_WEST_OF_HOUSE        , A_SOUTHWEST , GoFrom_WestOfHouse_Southwest  },
  { ROOM_WEST_OF_HOUSE        , A_IN        , GoFrom_WestOfHouse_Southwest  },
  { ROOM_EAST_OF_HOUSE        , A_WEST      , GoFrom_EastOfHouse_West       },
  { ROOM_EAST_OF_HOUSE        , A_IN        , GoFrom_EastOfHouse_West       },
  { ROOM_KITCHEN              , A_EAST      , GoFrom_Kitchen_East           },
  { ROOM_KITCHEN              , A_OUT       , GoFrom_Kitchen_East           },
  { ROOM_LIVING_ROOM          , A_WEST      , GoFrom_LivingRoom_West        },
  { ROOM_CELLAR               , A_UP        , GoFrom_Cellar_Up              },
  { ROOM_TROLL_ROOM           , A_EAST      , GoFrom_TrollRoom_East         },
  { ROOM_TROLL_ROOM           , A_WEST      , GoFrom_TrollRoom_West         },
  { ROOM_GRATING_ROOM         , A_UP        , GoFrom_GratingRoom_Up         },
  { ROOM_CYCLOPS_ROOM         , A_EAST      , GoFrom_CyclopsRoom_East       },
  { ROOM_CYCLOPS_ROOM         , A_UP        , GoFrom_CyclopsRoom_Up         },
  { ROOM_RESERVOIR_SOUTH      , A_NORTH     , GoFrom_ReservoirSouth_North   },
  { ROOM_RESERVOIR_NORTH      , A_SOUTH     , GoFrom_ReservoirNorth_South   },
  { ROOM_ENTRANCE_TO_HADES    , A_SOUTH     , GoFrom_EntranceToHades_South  },
  { ROOM_ENTRANCE_TO_HADES    , A_IN        , GoFrom_EntranceToHades_South  },
  { ROOM_DOME_ROOM            , A_DOWN      , GoFrom_DomeRoom_Down          },
  { ROOM_ARAGAIN_FALLS        , A_WEST      , GoFrom_OntoRainbowRoutine     },
  { ROOM_ARAGAIN_FALLS        , A_UP        , GoFrom_OntoRainbowRoutine     },
  { ROOM_END_OF_RAINBOW       , A_UP        , GoFrom_OntoRainbowRoutine     },
  { ROOM_END_OF_RAINBOW       , A_NORTHEAST , GoFrom_OntoRainbowRoutine     },
  { ROOM_END_OF_RAINBOW       , A_EAST      , GoFrom_OntoRainbowRoutine     },
  { ROOM_MAZE_2               , A_DOWN      , GoFrom_Maze2_Down             },
  { ROOM_MAZE_7               , A_DOWN      , GoFrom_Maze7_Down             },
  { ROOM_MAZE_9               , A_DOWN      , GoFrom_Maze9_Down             },
  { ROOM_MAZE_12              , A_DOWN      , GoFrom_Maze12_Down            },
  { ROOM_GRATING_CLEARING     , A_DOWN      , GoFrom_GratingClearing_Down   },
  { ROOM_LIVING_ROOM          , A_DOWN      , GoFrom_LivingRoom_Down        },
  { ROOM_SOUTH_TEMPLE         , A_DOWN      , GoFrom_SouthTemple_Down       },
  { ROOM_WHITE_CLIFFS_NORTH   , A_SOUTH     , GoFrom_WhiteCliffsNorth_South },
  { ROOM_WHITE_CLIFFS_NORTH   , A_WEST      , GoFrom_WhiteCliffsNorth_West  },
  { ROOM_WHITE_CLIFFS_SOUTH   , A_NORTH     , GoFrom_WhiteCliffsSouth_North },
  { ROOM_TIMBER_ROOM          , A_WEST      , GoFrom_TimberRoom_West        },
  { ROOM_LOWER_SHAFT          , A_EAST      , GoFrom_LowerShaft_East        },
  { ROOM_LOWER_SHAFT          , A_OUT       , GoFrom_LowerShaft_East        },
  { ROOM_KITCHEN              , A_DOWN      , GoFrom_Kitchen_Down           },
  { ROOM_STUDIO               , A_UP        , GoFrom_Studio_Up              },
  { ROOM_LAND_OF_LIVING_DEAD  , A_OUT       , GoFrom_LandOfLivingDead_North },
  { ROOM_STRANGE_PASSAGE      , A_IN        , GoFrom_StrangePassage_West    },
  { ROOM_NORTH_TEMPLE         , A_OUT       , GoFrom_NorthTemple_North      },
  { ROOM_MINE_ENTRANCE        , A_IN        , GoFrom_MineEntrance_West      },
  { ROOM_DAM_LOBBY            , A_NORTH     , GoFrom_DamLobby_North_Or_East },
  { ROOM_DAM_LOBBY            , A_EAST      , GoFrom_DamLobby_North_Or_East },

  { 0, 0, 0 }
};
//*****************************************************************************



//*****************************************************************************
void PrintDesc_LivingRoom(void)
{
  if ((Room[ROOM_LIVING_ROOM].prop & R_DESCRIBED) == 0)
  {
    PrintCompText("\x8b\xbb\x9e\xa7\x80\xcb\x69\x76\x84\xc2\xe1\x83\xac\x9e\x9a\xd0\x64\xe9\x72\x77\x61\xc4\xbd\x80\xfb\x61\x73\x74");

    if (CyclopsState == 4) // fled
      PrintCompText("\x9d\xba\x81\x77\xbe\xa6\x9a\xd0\x63\x79\x63\xd9\x70\x73\x2d\x73\xcd\xfc\xab\x6f\xfc\x6e\x84\xa7\xa3\xb4\x6f\x6c\xab\x77\xe9\xe8\xb4\x64\xe9\x72\xb5\x61\x62\x6f\xd7\xb7\xce\xfa\x87\x73\xe1\x9e\xc5\xf4\xb1\x9e\x67\xff\xce\x63\xcb\x65\x74\xd1\xf1\xb1\x2c\x20");
    else
      PrintCompText("\xb5\xd0\x77\xe9\xe8\xb4\x64\xe9\xb6\xf8\xa2\xaa\x74\xf4\xb1\x9e\x67\xff\xce\x63\xcb\x65\x74\xd1\xf1\x9c\xbd\x80\xb7\xbe\x74\xb5\x77\xce\xfa\xa3\x70\xfc\xbb\xa1\xbd\xb0\x9e\x6e\x61\x69\xcf\xab\x73\x68\xf7\x2c\x20");

    PrintCompText("\xd0\x74\xc2\x70\x68\xc4\xe7\xd6\x2c\x20");

    if (RugMoved)
    {
      if (TrapOpen)
        PrintCompLine("\x8c\xad\xae\xfc\xb4\x74\xf4\x70\xcc\xe9\xb6\xaf\x86\xb6\x66\xf3\x74\x2e");
      else
        PrintCompLine("\x8c\xd0\x63\xd9\xd6\xab\x74\xf4\x70\xcc\xe9\xb6\xaf\x86\xb6\x66\xf3\x74\x2e");
    }
    else
    {
      if (TrapOpen)
        PrintCompLine("\x8c\xd0\x72\x75\xc1\xec\x84\xef\x73\x69\xe8\xa3\xb4\x6f\xfc\xb4\x74\xf4\x70\xcc\xe9\x72\x2e");
      else
        PrintCompLine("\x8c\xd0\xfd\x72\x67\x9e\xd3\x69\xd4\x74\xe2\xda\x75\xc1\xa7\x80\xb3\xd4\xd1\xb6\xdd\x80\xda\xe9\x6d\x2e");
    }
  }

  if (Obj[OBJ_TROPHY_CASE].prop & PROP_OPEN)
    PrintContents(OBJ_TROPHY_CASE, "Your collection of treasures consists of:", 0);
}



void PrintDesc_EastOfHouse(void)
{
  if ((Room[ROOM_EAST_OF_HOUSE].prop & R_DESCRIBED) == 0)
  {
    PrintCompText("\x8b\xbb\x9e\xef\xce\xb9\x80\xb7\xce\xd1\xc0\xa5\xd6\xa4\x41\xeb\xaf\xde\xcf\x61\x64\xa1\xa7\xbd\x80\xc6\xd3\xbe\xa6\xbd\x80\xfb\xe0\x74\xa4\x49\xb4\xca\x9e\x63\xd3\xed\xb6\xdd\x80\xc0\xa5\xd6\x80\xa9\x87\xd0\x73\x6d\xe2\xea\xf8\xb9\xf2\xb7\xce\xfa\x87");

    if (KitchenWindowOpen)
      PrintCompLine("\x6f\xfc\x6e\x2e");
    else
      PrintCompLine("\x73\xf5\x67\x68\x74\xec\xa3\x6a\x61\x72\x2e");
  }
}



void PrintDesc_Kitchen(void)
{
  if ((Room[ROOM_KITCHEN].prop & R_DESCRIBED) == 0)
  {
    PrintCompText("\x8b\xbb\x9e\xa7\x80\x20\x6b\xc7\xfa\xd4\x8a\x81\x77\xce\xd1\xc0\xa5\xd6\xa4\x41\x9f\x61\x62\xcf\xaa\xf3\x6d\xa1\xbd\xc0\x61\xd7\xb0\xf3\xb4\xfe\xd5\xda\x65\x63\xd4\x74\xec\xc6\xd3\x80\xeb\xa9\x70\xbb\xaf\x69\xca\x8a\x66\xe9\x64\xa4\x41\xeb\xe0\x73\x61\x67\x9e\xcf\x61\x64\xa1\xbd\x80\xb7\xbe\xa6\x8c\xd0\x64\xbb\x6b\xaa\x74\x61\x69\x72\xe7\xd6\x91\xb0\x9e\xd6\xd4\xcb\xbf\x64\x84\x75\x70\x77\xbb\x64\xa4\x41\xcc\xbb\x6b\xb3\xce\x6d\xed\xc4\xcf\x61\x64\xa1\x64\xf2\xb4\x8c\xbd\x80\xfb\xe0\xa6\x9a\xd0\x73\x6d\xe2\xea\xf8\xb9\xf2\xb7\xce\xfa\x87");

    if (KitchenWindowOpen)
      PrintCompLine("\x6f\xfc\x6e\x2e");
    else
      PrintCompLine("\x73\xf5\x67\x68\x74\xec\xa3\x6a\x61\x72\x2e");
  }

  PrintContents(OBJ_KITCHEN_TABLE, "On the table you see:", 0);
}



void PrintDesc_Attic(void)
{
  if ((Room[ROOM_ATTIC].prop & R_DESCRIBED) == 0)
    PrintCompLine("\xbc\x9a\x9a\x81\xaf\xf0\x63\x83\x9e\xca\xec\xfb\x78\xc7\x87\xd0\xc5\x61\x69\x72\x77\x61\xc4\xcf\x61\x64\x84\x64\xf2\x6e\x2e");

  PrintContents(OBJ_ATTIC_TABLE, "On a table you see:", 0);
}



void PrintDesc_GratingClearing(void)
{
  if ((Room[ROOM_GRATING_CLEARING].prop & R_DESCRIBED) == 0)
    PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xb3\xcf\xbb\x97\xb5\xf8\xa2\xa3\xc6\xd3\xbe\xa6\x73\xd8\xc2\xf6\x64\x84\x8f\xca\xa3\xdf\xaa\x69\xe8\x73\xa4\x41\xeb\xaf\xde\xcf\x61\x64\xa1\x73\xa5\x74\x68\x2e");

  if (GratingRevealed)
  {
    if (GratingOpen)
      PrintCompLine("\x99\xa9\x87\xad\xae\xfc\xb4\x67\xf4\xf0\xb1\xb5\xe8\x73\x63\xd4\x64\x84\xa7\xbd\xcc\xbb\x6b\xed\x73\x73\x2e");
    else
      PrintCompLine("\x99\xa9\x87\xd0\x67\xf4\xf0\x9c\xd6\x63\xd8\x65\xec\xc6\xe0\xd1\xed\xab\xa7\xbd\x80\xe6\xc2\xf6\x64\x2e");
  }
}



void PrintDesc_GratingRoom(void)
{
  if ((Room[ROOM_GRATING_ROOM].prop & R_DESCRIBED) == 0)
    PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xaa\x6d\xe2\xea\xc2\xe1\xe4\xbf\xb6\x81\x6d\x61\x7a\x65\x83\xac\x9e\xbb\x9e\x74\xf8\xc5\xc4\x70\xe0\x73\x61\x67\xbe\xa8\xb4\x81\x69\x6d\x6d\xd5\x69\xaf\x9e\x76\x69\x63\xa7\xc7\x79\x2e");

  if (GratingOpen)
    PrintCompLine("\x41\x62\x6f\xd7\x86\x87\xad\xae\xfc\xb4\x67\xf4\xf0\x9c\xf8\xa2\xaa\xf6\xf5\x67\x68\xa6\x70\xa5\xf1\x9c\x69\x6e\x2e");
  else
    PrintCompLine("\x41\x62\x6f\xd7\x86\x87\xd0\x67\xf4\xf0\x9c\xf8\xa2\xa3\xaa\x6b\x75\xdf\x2d\xad\x64\x2d\x63\xc2\x73\x73\x62\xca\xbe\xcb\x6f\x63\x6b\x2e");
}



void PrintDesc_DamRoom(void)
{
  if ((Room[ROOM_DAM_ROOM].prop & R_DESCRIBED) == 0)
  {
    PrintCompLine("\x8b\xbb\x9e\xc5\xad\x64\x84\xca\x80\x9f\x6f\x70\x8a\x81\x46\xd9\x6f\xab\x43\xca\x74\xc2\xea\x44\x61\xf9\x23\x33\xb5\x77\xce\xfa\xb7\xe0\x20\x71\x75\xc7\x9e\xd0\xbd\xd8\xb2\xa6\xaf\x74\xf4\x63\xf0\xca\xa8\xb4\xf0\x6d\xbe\xc6\xbb\xcc\xb2\x74\xad\x74\x83\xac\x9e\xbb\x9e\x70\xaf\x68\xa1\xbd\x80\xe4\xd3\xa2\xb5\x73\xa5\xa2\xb5\x8c\x77\xbe\x74\xb5\x8c\xd0\x73\x63\xf4\x6d\x62\xcf\xcc\xf2\x6e\x2e");

    if (GatesOpen)
    {
      if (LowTide)
        PrintCompLine("\x85\x77\xaf\xac\xcb\x65\xd7\xea\xef\xce\xb9\x80\xcc\x61\xf9\x9a\xd9\x77\x3a\x82\xaa\x6c\x75\x69\x63\x9e\x67\xaf\xbe\xc0\x61\xd7\xb0\xf3\xb4\x6f\xfc\xed\x64\xa4\x57\xaf\xac\xda\xfe\xa0\xa1\xa2\xc2\x75\x67\xde\x81\x64\x61\xf9\x8c\x64\xf2\x6e\xc5\xa9\x61\x6d\x2e");
      else
        PrintCompLine("\x85\x73\x6c\x75\x69\x63\x9e\x67\xaf\xbe\xa3\xa9\xae\xfc\x6e\xb5\x8c\x77\xaf\xac\xda\xfe\xa0\xa1\xa2\xc2\x75\x67\xde\x81\x64\x61\x6d\x83\x9e\x77\xaf\xac\xcb\x65\xd7\xea\xef\xce\xb9\x80\xcc\x61\xf9\x9a\xc5\x69\xdf\xc0\x69\x67\x68\x2e");
    }
    else
    {
      if (LowTide)
        PrintCompLine("\x85\x73\x6c\x75\x69\x63\x9e\x67\xaf\xbe\xa3\xa9\xb3\xd9\xd6\x64\x83\x9e\x77\xaf\xac\xcb\x65\xd7\xea\xa7\x80\xda\xbe\xac\x76\x6f\x69\xb6\x9a\x71\x75\xc7\x9e\xd9\x77\xb5\x62\xf7\x80\xcb\x65\xd7\xea\x9a\xf1\x73\x84\x71\x75\x69\x63\x6b\x6c\x79\x2e");
      else
        PrintCompLine("\x85\x73\x6c\x75\x69\x63\x9e\x67\xaf\xbe\xae\xb4\x81\x64\x61\xf9\xbb\x9e\x63\xd9\xd6\x64\xa4\x42\x65\xce\xb9\x80\xcc\x61\x6d\xb5\x96\xa9\x91\xb0\x9e\xd6\xd4\xa3\xb7\x69\xe8\xda\xbe\xac\x76\x6f\x69\x72\xa4\x57\xaf\xac\x87\x70\xa5\xf1\x9c\x6f\xd7\xb6\x81\xbd\x70\x8a\x81\xe3\x77\xa3\x62\xad\x64\xca\xd5\xcc\x61\x6d\x2e");
    }
  }

  PrintCompText("\x99\xa9\x87\xd0\x63\xca\x74\xc2\xea\x70\xad\x65\xea\xa0\xa9\xb5\xca\xb7\xce\xfa\xa3\xcb\xbb\x67\x9e\x6d\x65\x74\xe2\xb0\x6f\x6c\xa6\x9a\x6d\xa5\xe5\xd5\xa4\x44\x69\xa9\x63\x74\xec\xa3\x62\x6f\xd7\x80\xb0\x6f\x6c\xa6\x9a\xd0\x73\x6d\xe2\xea\x67\xa9\xd4\xeb\xfd\xc5\x69\x63\xb0\x75\x62\x62\x6c\x65");

  if (GatesButton)
    PrintCompText("\xb7\xce\xfa\x87\x67\xd9\xf8\x9c\xd6\xa9\xed\x6c\x79");

  PrintCompLine("\x2e");
}



void PrintDesc_ReservoirSouth(void)
{
  if ((Room[ROOM_RESERVOIR_SOUTH].prop & R_DESCRIBED) == 0)
  {
    if (GatesOpen)
    {
      if (LowTide)
        PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xca\xc1\xc2\xe1\xb5\xbd\x80\xe4\xd3\xa2\x8a\x77\xce\xfa\xb7\xe0\xc6\xd3\x6d\xac\xec\xa3\xcb\x61\x6b\x65\xa4\x48\xf2\x65\xd7\x72\xb5\xf8\xa2\x80\xb7\xaf\xac\xcb\x65\xd7\xea\xd9\x77\xac\xd5\xb5\x96\xa9\x87\x6d\xac\x65\xec\xa3\xb7\x69\xe8\xaa\x74\xa9\x61\xf9\x72\xf6\x6e\x84\xa2\xc2\x75\x67\xde\x81\x63\xd4\xd1\xb6\xdd\x80\xda\xe9\x6d\x2e");
      else
        PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xca\xc1\xc2\xe1\x9d\xba\x81\xe3\x72\xa2\x87\xd0\xfd\x72\x67\x9e\xfd\x6b\x65\xb5\xbd\xba\xe8\x65\x70\x89\x63\xc2\x73\x73\x8e\xc3\xe3\xf0\x63\x65\xb5\x68\xf2\x65\xd7\x72\xb5\xa2\xaf\x80\xb7\xaf\xac\xcb\x65\xd7\xea\x61\x70\xfc\xbb\xa1\xbd\xb0\x9e\x64\xc2\x70\x70\x84\xaf\xa3\xda\x61\x70\x69\xab\xf4\xd1\xa4\x42\x65\x66\xd3\x9e\xd9\xb1\xb5\xc7\xee\x69\x67\x68\xa6\xef\xeb\x6f\x73\x73\x69\x62\xcf\x89\x63\xc2\x73\xa1\xbd\x80\xae\x96\xb6\x73\x69\xe8\xc6\xc2\xf9\xa0\x72\x65\x2e");
    }
    else
    {
      if (LowTide)
        PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xca\xc1\xc2\xe1\xb5\xbd\x80\xe4\xd3\xa2\x8a\x77\xce\xfa\x87\xd0\xf8\xe8\xa3\xa9\xd0\x77\xce\xfa\xb7\xe0\xc6\xd3\x6d\xac\xec\xa3\xda\xbe\xac\x76\x6f\x69\x72\xb5\x62\xf7\xe4\xf2\x87\x6d\xac\x65\xec\xa3\xaa\x74\xa9\x61\x6d\x8e\xc3\xe3\xf0\x63\x65\xb5\x68\xf2\x65\xd7\x72\xb5\xa2\xaf\x80\xcb\x65\xd7\xea\xdd\x80\xaa\x74\xa9\x61\xf9\x9a\xf1\x73\x84\x71\x75\x69\x63\x6b\xec\x8d\x95\xaf\xb0\x65\x66\xd3\x9e\xd9\x9c\xc7\xb7\x69\xdf\xb0\x9e\x69\x6d\x70\x6f\x73\x73\x69\x62\xcf\x89\x63\xc2\x73\xa1\xa0\x72\x65\x2e");
      else
        PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xca\xc1\xc2\xe1\xae\xb4\x81\x73\xa5\xa2\xaa\x68\xd3\x9e\xdd\xa3\xcb\xbb\x67\x9e\xfd\x6b\x65\xb5\x66\xbb\x9f\xe9\xcc\xf3\x70\x8d\xb7\x69\xe8\xc6\xd3\xb3\xc2\x73\x73\x97\x2e");
    }

    PrintCompLine("\x99\xa9\x87\xd0\x70\xaf\xde\xe2\xca\xc1\x81\xc5\xa9\x61\xf9\xbd\x80\xfb\xe0\xa6\xd3\xb7\xbe\x74\xb5\xd0\xc5\xf3\x70\xeb\xaf\x68\x77\x61\xc4\x63\xf5\x6d\x62\x84\x73\xa5\xa2\x77\xbe\xa6\xe2\xca\xc1\x81\xd5\x67\x9e\xdd\xa3\xb3\xcd\x73\x6d\xb5\x8c\xd0\x70\xaf\xde\xcf\x61\x64\x84\xa7\xbd\xa3\x91\xc9\xb4\xbd\x80\xaa\xa5\x96\xe0\x74\x2e");
  }
}



void PrintDesc_Reservoir(void)
{
  if ((Room[ROOM_RESERVOIR].prop & R_DESCRIBED) == 0)
  {
    if (LowTide)
    {
      if (GatesOpen == 0 && YouAreInBoat == 0)
        PrintCompLine("\x8b\xe3\xf0\x63\x9e\xa2\xaf\x80\xb7\xaf\xac\xcb\x65\xd7\xea\xa0\xa9\x87\xf1\x73\x84\xf4\x70\x69\x64\xec\x83\x9e\x63\xd8\xa9\xe5\xa1\xbb\x9e\xe2\x73\xba\xef\x63\xe1\x84\xc5\xc2\xb1\xac\xa4\x53\x74\x61\x79\x84\xa0\xa9\xaa\xf3\x6d\xa1\x71\x75\xc7\x9e\xfc\xf1\xd9\x75\x73\x21");
      else
        PrintCompLine("\x8b\xbb\x9e\xca\xb7\xcd\xa6\xfe\xd5\x89\xef\xa3\xcb\xbb\x67\x9e\xfd\x6b\x65\xb5\x62\xf7\xb7\xce\xfa\x87\xe3\x77\xa3\xcb\xbb\x67\x9e\x6d\x75\xab\x70\x69\xcf\x83\xac\x9e\xbb\x9e\x22\x73\x68\xd3\xbe\x22\x89\x81\xe3\x72\xa2\x8d\xaa\xa5\x74\x68\x2e");
    }
    else
      PrintCompLine("\x8b\xbb\x9e\xca\x80\xcb\x61\x6b\x65\xa4\x42\xbf\xfa\xbe\x91\xb0\x9e\xd6\xd4\xe4\xd3\xa2\x8d\xaa\xa5\xa2\xa4\x55\x70\xc5\xa9\x61\xf9\xd0\x73\x6d\xe2\xea\xc5\xa9\x61\xf9\xd4\xd1\x72\xa1\x81\xfd\x6b\x9e\xa2\xc2\x75\x67\xde\xd0\x6e\xbb\xc2\x77\xb3\xcf\x66\xa6\xa7\x80\xda\x6f\x63\x6b\x73\x83\x9e\x64\x61\xf9\xe7\xb4\xef\xaa\xf3\xb4\x64\xf2\x6e\xc5\xa9\x61\x6d\x2e");
  }
}



void PrintDesc_ReservoirNorth(void)
{
  if ((Room[ROOM_RESERVOIR_NORTH].prop & R_DESCRIBED) == 0)
  {
    if (GatesOpen)
    {
      if (LowTide)
        PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xbb\x67\x9e\xe7\xd7\x72\xe3\xfe\xda\xe9\x6d\xb5\x81\x73\xa5\xa2\x8a\x77\xce\xfa\xb7\xe0\xc6\xd3\x6d\xac\xec\xa3\xcb\x61\x6b\x65\xa4\x48\xf2\x65\xd7\x72\xb5\xf8\xa2\x80\xb7\xaf\xac\xcb\x65\xd7\xea\xd9\x77\xac\xd5\xb5\x96\xa9\x87\x6d\xac\x65\xec\xa3\xb7\x69\xe8\xaa\x74\xa9\x61\xf9\x72\xf6\x6e\x84\xa2\xc2\x75\x67\xde\x96\x72\x65\x2e");
      else
        PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xbb\x67\x9e\xe7\xd7\x72\xe3\xfe\xa3\xa9\x61\x9d\xba\x81\x73\xa5\xa2\x87\xd0\xf8\xe8\xcb\x61\x6b\x65\xb5\x77\x68\x6f\xd6\xb7\xaf\xac\xcb\x65\xd7\xea\x61\x70\xfc\xbb\xa1\xbd\xb0\x9e\x66\xe2\xf5\x9c\xf4\x70\x69\x64\x6c\x79\x2e");
    }
    else
    {
      if (LowTide)
        PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xb3\x61\xd7\x72\xe3\xfe\xa3\xa9\x61\xb5\xbd\x80\xaa\xa5\xa2\x8a\x77\xce\xfa\x87\xd0\xd7\x72\xc4\xf8\xe8\xaa\x74\xa9\x61\x6d\x83\x9e\xcf\xd7\xea\xdd\x80\xaa\x74\xa9\x61\xf9\x9a\xf1\x73\x84\xf4\x70\x69\x64\xec\xb5\x8c\xc7\xa3\x70\xfc\xbb\xa1\xa2\xaf\xb0\x65\x66\xd3\x9e\xd9\x9c\xc7\xb7\x69\xdf\xb0\x9e\x69\x6d\x70\x6f\x73\x73\x69\x62\xcf\x89\x63\xc2\x73\xa1\xbd\x80\xae\x96\xb6\x73\x69\x64\x65\x2e");
      else
        PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xbb\x67\x9e\xe7\xd7\x72\xe3\xfe\xda\xe9\x6d\xb5\xe3\x72\xa2\x8a\xd0\xfd\x72\x67\x9e\xfd\x6b\x65\x2e");
    }

    PrintCompLine("\x99\xa9\x87\xd0\x73\xf5\x6d\xc4\xc5\x61\x69\x72\x77\x61\xc4\xcf\x61\x76\x84\x81\xc2\xe1\x89\x81\xe3\x72\x74\x68\x2e");
  }
}



void PrintDesc_LoudRoom(void)
{
  if ((Room[ROOM_LOUD_ROOM].prop & R_DESCRIBED) == 0)
  {
    PrintCompText("\xbc\x9a\x9a\xd0\xfd\x72\x67\x9e\xc2\xe1\xb7\xc7\xde\xd0\x63\x65\x69\xf5\x9c\x77\xce\xfa\x91\xe3\xa6\xef\xcc\x65\xd1\x63\xd1\xab\x66\xc2\xf9\x81\x67\xc2\xf6\x64\x83\xac\x9e\x9a\xd0\x6e\xbb\xc2\x77\xeb\xe0\x73\x61\x67\x9e\x66\xc2\xf9\xbf\xc5\x89\x77\xbe\xa6\x8c\xd0\xc5\xca\x9e\xc5\x61\x69\x72\x77\x61\xc4\xcf\x61\x64\x84\x75\x70\x77\xbb\x64\x2e");

    if (LoudRoomQuiet || (GatesOpen == 0 && LowTide))
      PrintCompLine("\x82\xda\xe9\xf9\x9a\xf3\xf1\x9e\xa7\xa8\x74\xa1\x71\x75\x69\x65\x74\xed\x73\x73\x2e");
    else
      PrintCompLine("\x82\xda\xe9\xf9\x9a\xe8\x61\x66\xd4\x97\xec\xcb\xa5\xab\xf8\xa2\xa3\xb4\xf6\xe8\xd1\x72\x6d\xa7\xd5\xda\xfe\xce\x9c\x73\xa5\xb9\x83\x9e\x73\xa5\xb9\xaa\xf3\x6d\xa1\xbd\xda\x65\xd7\x72\xef\xf4\xd1\xc6\xc2\xf9\xe2\xea\xdd\x80\xb7\xe2\x6c\x73\xb5\x6d\x61\x6b\x84\xc7\xcc\x69\x66\x66\x69\x63\x75\x6c\xa6\x65\xd7\xb4\xbd\x95\xa7\x6b\x2e");
  }
}



void PrintDesc_DeepCanyon(void)
{
  if ((Room[ROOM_DEEP_CANYON].prop & R_DESCRIBED) == 0)
  {
    PrintCompText("\x8b\xbb\x9e\xca\x80\xaa\xa5\xa2\xfb\x64\x67\x9e\xdd\xa3\xcc\xf3\x70\x91\xc9\x6e\xa4\x50\xe0\x73\x61\x67\xbe\xcb\xbf\xab\xdd\xd2\xbd\x80\xfb\xe0\x74\xb5\xe3\x72\xa2\x77\xbe\xa6\x8c\x73\xa5\xa2\x77\xbe\x74\xa4\x41\xaa\x74\x61\x69\x72\x77\x61\xc4\xcf\x61\x64\xa1\x64\xf2\x6e\x2e");

    if (GatesOpen)
    {
      if (LowTide)
        PrintCompLine("\x88\x91\xc0\xbf\xb6\x81\x73\xa5\xb9\x8a\x66\xd9\xf8\x9c\x77\xaf\xac\xc6\xc2\xf9\xef\xd9\x77\x2e");
      else
        PrintCompLine("\x88\x91\xc0\xbf\xb6\xd0\xd9\x75\xab\xc2\xbb\x84\x73\xa5\xb9\xb5\xf5\x6b\x9e\xa2\xaf\x8a\x72\xfe\xce\x9c\x77\xaf\xac\xb5\x66\xc2\xf9\xef\xd9\x77\x2e");
    }
    else
    {
      if (LowTide)
        PrintCompText("\x0a");
      else
        PrintCompLine("\x88\x91\xc0\xbf\xb6\x81\x73\xa5\xb9\x8a\x66\xd9\xf8\x9c\x77\xaf\xac\xc6\xc2\xf9\xef\xd9\x77\x2e");
    }
  }
}



void PrintDesc_MachineRoom(void)
{
  if ((Room[ROOM_MACHINE_ROOM].prop & R_DESCRIBED) == 0)
  {
    PrintCompText("\xbc\x9a\x9a\xd0\xfd\x72\x67\x65\xb5\x63\x6f\x6c\xab\xc2\xe1\xb7\x68\x6f\xd6\xaa\x6f\xcf\xfb\x78\xc7\x87\xbd\x80\xe4\xd3\xa2\xa4\x49\xb4\xca\x9e\x63\xd3\xed\xb6\x96\xa9\x87\xd0\x6d\x61\xfa\xa7\x9e\x77\xce\xfa\x87\xa9\x6d\xa7\xb2\x63\xd4\xa6\xdd\xa3\xb3\xd9\x96\xa1\x64\x72\x79\xac\xa4\x4f\xb4\xc7\xa1\x66\x61\x63\x9e\x9a\xd0\x73\xf8\x74\xfa\xb7\xce\xfa\x87\xfd\xef\xdf\xd5\x20\x22\x53\x54\x41\x52\x54\x22\x83\x9e\x73\xf8\x74\xfa\xcc\x6f\xbe\xe4\xff\xa3\x70\xfc\xbb\x89\xef\xee\xad\x69\x70\x75\xfd\x62\xcf\xb0\xc4\xad\xc4\x68\x75\x6d\xad\xc0\x8c\x28\xf6\xcf\x73\xa1\x81\x66\x97\xac\xa1\xbb\x9e\x61\x62\xa5\xa6\x31\x2f\x31\x36\xb0\xc4\x31\x2f\x34\xa8\x6e\xfa\x29\xa4\x4f\xb4\x81\x66\xc2\xe5\x8a\x81\x6d\x61\xfa\xa7\x9e\x9a\xd0\xfd\x72\x67\x9e\xf5\x64\xb5\x77\xce\xfa\x87");

    if (Obj[OBJ_MACHINE].prop & PROP_OPEN)
      PrintCompLine("\x6f\xfc\x6e\x2e");
    else
      PrintCompLine("\x63\xd9\xd6\x64\x2e");
  }
}



void PrintDesc_AragainFalls(void)
{
  if ((Room[ROOM_ARAGAIN_FALLS].prop & R_DESCRIBED) == 0)
    PrintCompLine("\x8b\xbb\x9e\xaf\x80\x9f\x6f\x70\x8a\x41\xf4\x67\x61\xa7\x20\x46\xe2\x6c\x73\xb5\xad\xfb\xe3\x72\x6d\xa5\xa1\x77\xaf\xac\x66\xe2\xea\xf8\xa2\xa3\xcc\xc2\x70\x8a\x61\x62\xa5\xa6\x34\x35\x30\xc6\xf3\x74\x83\x9e\xca\xec\xeb\xaf\xde\xa0\xa9\x87\xca\x80\xe4\xd3\xa2\xfb\x6e\x64\x2e");

  if (RainbowSolid)
    PrintCompLine("\x41\xaa\x6f\xf5\xab\xf4\xa7\x62\xf2\xaa\x70\xad\xa1\x81\x66\xe2\x6c\x73\x2e");
  else
    PrintCompLine("\x41\xb0\xbf\xf7\x69\x66\x75\xea\xf4\xa7\x62\xf2\x91\xb0\x9e\xd6\xd4\xae\xd7\xb6\x81\x66\xe2\x6c\xa1\x8c\xbd\x80\xb7\xbe\x74\x2e");
}



void PrintDesc_WestOfHouse(void)
{
  if ((Room[ROOM_WEST_OF_HOUSE].prop & R_DESCRIBED) == 0)
  {
    PrintCompText("\x8b\xbb\x9e\xc5\xad\x64\x84\xa7\xa3\xb4\x6f\xfc\xb4\x66\x69\x65\x6c\xab\x77\xbe\xa6\xdd\xa3\xb7\xce\xd1\xc0\xa5\xd6\xb5\xf8\xa2\xa3\xb0\x6f\xbb\xe8\xab\x66\xc2\xe5\xcc\xe9\x72\x2e");

    if (WonGame)
      PrintCompLine("\x20\x41\xaa\x65\x63\xa9\xa6\x70\xaf\xde\xcf\x61\x64\xa1\x73\xa5\xa2\x77\xbe\xa6\xa7\xbd\x80\xc6\xd3\xbe\x74\x2e");
    else
      PrintCompText("\x0a");
  }
}



void PrintDesc_MirrorRoom1(void)
{
  if ((Room[ROOM_MIRROR_ROOM_1].prop & R_DESCRIBED) == 0)
    PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xbb\x67\x9e\x73\x71\x75\xbb\x9e\xc2\xe1\xb7\xc7\xde\x74\xe2\xea\x63\x65\x69\xf5\xb1\x73\xa4\x4f\xb4\x81\x73\xa5\xa2\xb7\xe2\xea\x9a\xad\xfb\xe3\x72\x6d\xa5\xa1\x6d\x69\x72\xc2\xb6\x77\xce\xfa\xc6\x69\xdf\xa1\x81\xd4\xf0\xa9\xb7\xe2\x6c\x83\xac\x9e\xbb\x9e\x65\x78\xc7\xa1\xca\x80\xae\x96\xb6\xa2\xa9\x9e\x73\x69\xe8\xa1\xdd\x80\xda\xe9\x6d\x2e");

  if (MirrorBroken)
    PrintCompLine("\x55\x6e\x66\xd3\x74\xf6\xaf\x65\xec\xb5\x81\x6d\x69\x72\xc2\xb6\xcd\xa1\xef\xd4\xcc\xbe\x74\xc2\x79\xd5\xb0\xc4\x92\xda\x65\x63\x6b\xcf\x73\x73\xed\x73\x73\x2e");
}



void PrintDesc_MirrorRoom2(void)
{
  if ((Room[ROOM_MIRROR_ROOM_2].prop & R_DESCRIBED) == 0)
    PrintCompLine("\x8b\xbb\x9e\xa7\xa3\xcb\xbb\x67\x9e\x73\x71\x75\xbb\x9e\xc2\xe1\xb7\xc7\xde\x74\xe2\xea\x63\x65\x69\xf5\xb1\x73\xa4\x4f\xb4\x81\x73\xa5\xa2\xb7\xe2\xea\x9a\xad\xfb\xe3\x72\x6d\xa5\xa1\x6d\x69\x72\xc2\xb6\x77\xce\xfa\xc6\x69\xdf\xa1\x81\xd4\xf0\xa9\xb7\xe2\x6c\x83\xac\x9e\xbb\x9e\x65\x78\xc7\xa1\xca\x80\xae\x96\xb6\xa2\xa9\x9e\x73\x69\xe8\xa1\xdd\x80\xda\xe9\x6d\x2e");

  if (MirrorBroken)
    PrintCompLine("\x55\x6e\x66\xd3\x74\xf6\xaf\x65\xec\xb5\x81\x6d\x69\x72\xc2\xb6\xcd\xa1\xef\xd4\xcc\xbe\x74\xc2\x79\xd5\xb0\xc4\x92\xda\x65\x63\x6b\xcf\x73\x73\xed\x73\x73\x2e");
}



void PrintDesc_TorchRoom(void)
{
  if ((Room[ROOM_TORCH_ROOM].prop & R_DESCRIBED) == 0)
    PrintCompLine("\xbc\x9a\x9a\xd0\xfd\x72\x67\x9e\xc2\xe1\xb7\xc7\xde\xd0\x70\xc2\x6d\xa7\xd4\xa6\x64\xe9\x72\x77\x61\xc4\xcf\x61\x64\x84\xbd\xa3\xcc\xf2\xb4\xc5\x61\x69\x72\xe7\xd6\xa4\x41\x62\x6f\xd7\x86\x87\xd0\xfd\x72\x67\x9e\x64\xe1\x65\xa4\x55\x70\xa3\xc2\xf6\xab\x81\xd5\x67\x9e\xdd\x80\xcc\xe1\x9e\x28\x32\x30\xc6\xf3\xa6\x75\x70\x29\x87\xd0\x77\xe9\xe8\xb4\xf4\x69\xf5\xb1\xa4\x49\xb4\x81\x63\xd4\xd1\xb6\xdd\x80\xda\xe9\xf9\x73\xc7\xa1\xd0\x77\xce\xd1\xee\xbb\x62\xcf\xeb\xd5\xbe\x74\x61\x6c\x2e");

  if (RopeTiedToRail)
    PrintCompLine("\x41\xeb\x69\x65\x63\x9e\xdd\xda\x6f\xfc\xcc\xbe\x63\xd4\x64\xa1\x66\xc2\xf9\x81\xf4\x69\xf5\x9c\x61\x62\x6f\xd7\xb5\xd4\x64\x84\x73\xe1\x9e\x66\x69\xd7\xc6\xf3\xa6\x61\x62\x6f\xd7\x86\xb6\xa0\x61\x64\x2e");
}



void PrintDesc_DomeRoom(void)
{
  if ((Room[ROOM_DOME_ROOM].prop & R_DESCRIBED) == 0)
    PrintCompLine("\x8b\xbb\x9e\xaf\x80\xeb\xac\x69\x70\xa0\x72\xc4\xdd\xa3\xcb\xbb\x67\x9e\x64\xe1\x65\xb5\x77\xce\xfa\xc6\xd3\x6d\xa1\x81\x63\x65\x69\xf5\x9c\xdd\xa3\xe3\x96\xb6\xc2\xe1\xb0\x65\xd9\x77\xa4\x50\xc2\xd1\x63\xf0\x9c\x8f\x66\xc2\xf9\xd0\x70\xa9\x63\x69\x70\xc7\xa5\xa1\x64\xc2\x70\x87\xd0\x77\xe9\xe8\xb4\xf4\x69\xf5\x9c\x77\xce\xfa\xb3\x69\x72\x63\xcf\xa1\x81\x64\xe1\x65\x2e");

  if (RopeTiedToRail)
    PrintCompLine("\x48\xad\x67\x84\x64\xf2\xb4\x66\xc2\xf9\x81\xf4\x69\xf5\x9c\x9a\xd0\xc2\xfc\xb7\xce\xfa\xfb\xb9\xa1\x61\x62\xa5\xa6\xd1\xb4\x66\xf3\xa6\x66\xc2\xf9\x81\x66\xd9\xd3\xb0\x65\xd9\x77\x2e");
}



void PrintDesc_CyclopsRoom(void)
{
  if ((Room[ROOM_CYCLOPS_ROOM].prop & R_DESCRIBED) == 0)
    PrintCompLine("\xbc\x9a\xc2\xe1\xc0\xe0\xa3\xb4\x65\x78\xc7\xae\xb4\x81\xe3\x72\xa2\x77\xbe\x74\xb5\x8c\xd0\xc5\x61\x69\x72\xe7\xd6\xcb\xbf\x64\x84\x75\x70\x2e");

  if (CyclopsState == 4)
    PrintCompLine("\x85\xbf\xc5\xb7\xe2\x6c\xb5\x70\xa9\x76\x69\xa5\x73\xec\xaa\x6f\xf5\x64\xb5\xe3\x77\xc0\xe0\xa3\xb3\x79\x63\xd9\x70\x73\x2d\x73\x69\x7a\xd5\xae\xfc\x6e\x84\xa7\xa8\x74\x2e");
}



void PrintDesc_UpATree(void)
{
  if ((Room[ROOM_UP_A_TREE].prop & R_DESCRIBED) == 0)
    PrintCompLine("\x8b\xbb\x9e\x61\x62\xa5\xa6\x31\x30\xc6\xf3\xa6\x61\x62\x6f\xd7\x80\xe6\xc2\xf6\xab\xed\xc5\xcf\xab\x61\x6d\xca\xc1\x73\xe1\x9e\xfd\x72\x67\x9e\x62\xf4\x6e\xfa\xbe\x83\x9e\xed\xbb\xbe\xa6\x62\xf4\x6e\xfa\xa3\x62\x6f\xd7\x86\x87\x61\x62\x6f\xd7\x86\xb6\xa9\x61\x63\x68\x2e");

  PrintPresentObjects(ROOM_PATH, "On the ground below you can see:", 1); // 1: list, no desc
}



struct OVERRIDEROOMDESC_STRUCT OverrideRoomDesc[] =
{
  { ROOM_LIVING_ROOM       , PrintDesc_LivingRoom      },
  { ROOM_EAST_OF_HOUSE     , PrintDesc_EastOfHouse     },
  { ROOM_KITCHEN           , PrintDesc_Kitchen         },
  { ROOM_ATTIC             , PrintDesc_Attic           },
  { ROOM_GRATING_CLEARING  , PrintDesc_GratingClearing },
  { ROOM_GRATING_ROOM      , PrintDesc_GratingRoom     },
  { ROOM_DAM_ROOM          , PrintDesc_DamRoom         },
  { ROOM_RESERVOIR_SOUTH   , PrintDesc_ReservoirSouth  },
  { ROOM_RESERVOIR         , PrintDesc_Reservoir       },
  { ROOM_RESERVOIR_NORTH   , PrintDesc_ReservoirNorth  },
  { ROOM_LOUD_ROOM         , PrintDesc_LoudRoom        },
  { ROOM_DEEP_CANYON       , PrintDesc_DeepCanyon      },
  { ROOM_MACHINE_ROOM      , PrintDesc_MachineRoom     },
  { ROOM_ARAGAIN_FALLS     , PrintDesc_AragainFalls    },
  { ROOM_WEST_OF_HOUSE     , PrintDesc_WestOfHouse     },
  { ROOM_MIRROR_ROOM_1     , PrintDesc_MirrorRoom1     },
  { ROOM_MIRROR_ROOM_2     , PrintDesc_MirrorRoom2     },
  { ROOM_TORCH_ROOM        , PrintDesc_TorchRoom       },
  { ROOM_DOME_ROOM         , PrintDesc_DomeRoom        },
  { ROOM_CYCLOPS_ROOM      , PrintDesc_CyclopsRoom     },
  { ROOM_UP_A_TREE         , PrintDesc_UpATree         },

  { 0, 0 }
};
//*****************************************************************************



//*****************************************************************************
// end newline handled by calling function



void PrintDesc_Ghosts(int desc_flag)
{
  if (desc_flag == 0)
    PrintCompText("\xd0\x6e\x75\x6d\xef\xb6\xdd\xe6\x68\x6f\x73\x74\x73");
  else
  {
    if (YouAreDead == 0)
      PrintCompText("\x85\x77\x61\xc4\xa2\xc2\x75\x67\xde\x81\x67\xaf\x9e\x9a\x62\xbb\xa9\xab\x62\xc4\x65\x76\x69\xea\x73\x70\x69\xf1\x74\x73\xb5\x77\x68\xba\x6a\xf3\xb6\xaf\x86\xb6\xaf\xd1\x6d\x70\x74\xa1\xbd\xeb\xe0\x73\x2e");
  }
}



void PrintDesc_Bat(int desc_flag)
{
  if (desc_flag == 0)
    PrintCompText("\xd0\x62\x61\x74");
  else
  {
    if (IsObjVisible(OBJ_GARLIC))
      PrintCompText("\x49\xb4\x81\x63\xd3\xed\xb6\xdd\x80\xda\xe9\xf9\xca\x80\xb3\x65\x69\xf5\x9c\x9a\xd0\xfd\x72\x67\x9e\x76\x61\x6d\x70\x69\xa9\xb0\xaf\xb7\x68\xba\x9a\x6f\x62\x76\x69\xa5\x73\xec\xcc\xac\xad\x67\xd5\x8d\xc0\x6f\x6c\x64\x84\xce\xa1\xe3\x73\x65\x2e");
    else
      PrintCompText("\x41\xcb\xbb\x67\x9e\x76\x61\x6d\x70\x69\xa9\xb0\xaf\xb5\xcd\xb1\x84\x66\xc2\xf9\x81\x63\x65\x69\xf5\xb1\xb5\x73\x77\xe9\x70\xa1\x64\xf2\xb4\xaf\x86\x21");
  }
}



void PrintDesc_Troll(int desc_flag)
{
  if (desc_flag == 0)
    PrintCompText("\xd0\x74\xc2\x6c\x6c");
  else
    switch (TrollDescType)
  {
    case 0: PrintCompText("\x41\xe4\xe0\x74\x79\x2d\xd9\x6f\x6b\x84\x74\xc2\xdf\xb5\x62\xf4\xb9\xb2\xce\x9c\xd0\x62\xd9\x6f\x64\xc4\x61\x78\x65\xb5\x62\xd9\x63\x6b\xa1\xe2\xea\x70\xe0\x73\x61\x67\xbe\xae\xf7\x8a\x81\xc2\x6f\x6d\x2e"); break;
    case 1: PrintCompText("\x41\xb4\xf6\x63\xca\x73\x63\x69\xa5\xa1\x74\xc2\xdf\x87\x73\x70\xf4\x77\xcf\xab\xca\x80\xc6\xd9\xd3\xa4\x41\xdf\xeb\xe0\x73\x61\x67\xbe\xae\xf7\x8a\x81\xc2\xe1\xa3\xa9\xae\xfc\x6e\x2e"); break;
    case 2: PrintCompText("\x41\xeb\xaf\xa0\xf0\xe7\xdf\xc4\x62\x61\x62\x62\xf5\x9c\x74\xc2\xdf\x87\xa0\x72\x65\x2e"); break;
    case 3: PrintCompText("\x41\x9f\xc2\xdf\x87\xa0\x72\x65\x2e"); break;
  }
}



void PrintDesc_Thief(int desc_flag)
{
  if (desc_flag == 0)
    PrintCompText("\xd0\xa2\x69\x65\x66");
  else
    switch (ThiefDescType)
  {
    case 0: PrintCompText("\x99\xa9\x87\xd0\x73\xfe\x70\x69\x63\x69\xa5\x73\x2d\xd9\x6f\x6b\x84\xa7\x64\x69\x76\x69\x64\x75\xe2\xb5\x68\x6f\x6c\x64\x84\xd0\x62\x61\x67\xb5\xcf\xad\x84\x61\x67\x61\xa7\xc5\xae\xed\xb7\xe2\x6c\xa4\x48\x9e\x9a\xbb\x6d\xd5\xb7\xc7\xde\xd0\x76\x69\x63\x69\xa5\x73\x2d\xd9\x6f\x6b\x84\xc5\x69\xcf\x74\x74\x6f\x2e"); break;
    case 1: PrintCompText("\x99\xa9\x87\xd0\x73\xfe\x70\x69\x63\x69\xa5\x73\x2d\xd9\x6f\x6b\x84\xa7\x64\x69\x76\x69\x64\x75\xe2\xcb\x79\x84\xf6\x63\xca\x73\x63\x69\xa5\xa1\xca\x80\xe6\xc2\xf6\x64\x2e"); break;
  }
}



void PrintDesc_Cyclops(int desc_flag)
{
  if (desc_flag == 0)
    PrintCompText("\xd0\x63\x79\x63\xd9\x70\x73");
  else
    switch (CyclopsState)
  {
    case 0: PrintCompText("\x41\xb3\x79\x63\xd9\x70\x73\xb5\x77\x68\xba\xd9\x6f\x6b\xa1\x70\xa9\x70\xbb\xd5\x89\xbf\xa6\x68\xd3\xd6\xa1\x28\x6d\x75\xfa\xcb\xbe\xa1\x6d\xac\x9e\x61\x64\xd7\xe5\xd8\xac\x73\x29\xb5\x62\xd9\x63\x6b\xa1\x81\xc5\x61\x69\x72\xe7\xd6\xa4\x46\xc2\xf9\xce\xa1\xc5\xaf\x9e\xdd\xc0\xbf\x6c\xa2\xb5\x8c\x81\x62\xd9\x6f\x64\xc5\x61\xa7\xa1\xca\x80\xb7\xe2\x6c\x73\xb5\x8f\x67\xaf\xa0\xb6\xa2\xaf\xc0\x9e\x9a\xe3\xa6\xd7\x72\xc4\x66\xf1\xd4\x64\xec\xb5\xa2\xa5\x67\xde\x94\xf5\x6b\xbe\xeb\x65\x6f\x70\x6c\x65\x2e"); break;
    case 1: PrintCompText("\x85\x63\x79\x63\xd9\x70\xa1\x9a\xc5\xad\x64\x84\xa7\x80\xb3\xd3\xed\x72\xb5\x65\x79\x65\x84\x8f\x63\xd9\xd6\xec\xa4\x49\xcc\xca\x27\xa6\xa2\xa7\x6b\xc0\x9e\xf5\x6b\xbe\x86\x20\xd7\x72\xc4\x6d\x75\xfa\xa4\x48\x9e\xd9\x6f\x6b\xa1\x65\x78\x74\xa9\x6d\x65\xec\xc0\xf6\x67\x72\x79\xb5\x65\xd7\xb4\x66\xd3\xa3\xb3\x79\x63\xd9\x70\x73\x2e"); break;
    case 2: PrintCompText("\x85\x63\x79\x63\xd9\x70\x73\xb5\xcd\x76\x84\xbf\xd1\xb4\x81\x68\xff\xeb\x65\x70\xfc\x72\x73\xb5\x61\x70\xfc\xbb\xa1\xbd\xb0\x9e\x67\xe0\x70\x97\xa4\x48\x9a\xd4\x66\xfd\x6d\xd5\x9f\xca\x67\x75\x9e\x70\xc2\x74\x72\x75\xe8\xa1\x66\xc2\xf9\xce\xa1\x6d\xad\x2d\x73\x69\x7a\xd5\xee\xa5\x74\x68\x2e"); break;
    case 3: PrintCompText("\x85\x63\x79\x63\xd9\x70\xa1\x9a\x73\xcf\x65\x70\x84\x62\xf5\x73\x73\x66\x75\xdf\xc4\xaf\x80\xc6\xe9\xa6\xdd\x80\xaa\x74\x61\x69\x72\x73\x2e"); break;
  }
}



void PrintDesc_InflatedBoat(int desc_flag)
{
  if (desc_flag == 0)
    PrintCompText("\xd0\x6d\x61\x67\x69\x63\xb0\x6f\x61\x74");
  else
  {
    if (YouAreInBoat)
      PrintCompText("\x8b\xbb\x9e\x73\xc7\xf0\x9c\xa7\xa3\xee\x61\x67\x69\x63\xb0\x6f\x61\x74\x2e");
    else
      PrintCompText("\x99\xa9\x87\xd0\x6d\x61\x67\x69\x63\xb0\x6f\xaf\xc0\xac\x65\x2e");
  }
}



void PrintDesc_Lamp(int desc_flag)
{
  char *name;

  if (Obj[OBJ_LAMP].prop & PROP_LIT) name = "lit brass lantern"; else name = "brass lantern";

  if (desc_flag == 0)
    {PrintCompText("\x61\x20"); PrintText(name);}
  else
  {
    if (Obj[OBJ_LAMP].prop & PROP_MOVEDDESC)
      {PrintCompText("\x99\xa9\x87\x61\x20"); PrintText(name); PrintCompText("\x20\x28\x62\xaf\xd1\x72\x79\x2d\x70\xf2\xac\xd5\x29\xc0\xac\x65\x2e");}
    else
      PrintCompText("\x41\xb0\xaf\xd1\x72\x79\x2d\x70\xf2\xac\xd5\xb0\xf4\x73\xa1\xfd\xe5\xac\xb4\x9a\xca\x80\x9f\xc2\x70\x68\xc4\xe7\x73\x65\x2e");
  }
}



void PrintDesc_Candles(int desc_flag)
{
  char *name;

  if (Obj[OBJ_CANDLES].prop & PROP_LIT) name = "pair of burning candles"; else name = "pair of candles";

  if (desc_flag == 0)
    {PrintCompText("\x61\x20"); PrintText(name);}
  else
  {
    if (Obj[OBJ_CANDLES].prop & PROP_MOVEDDESC)
      {PrintCompText("\x99\xa9\x87\x61\x20"); PrintText(name); PrintCompText("\xc0\xac\x65\x2e");}
    else
      PrintCompText("\x4f\xb4\x81\x74\x77\xba\xd4\x64\xa1\xdd\x80\xa3\x6c\x74\xbb\xa3\xa9\xb0\xd8\x6e\x84\xe7\xb9\xcf\x73\x2e");
  }
}



struct OVERRIDEOBJECTDESC_STRUCT OverrideObjectDesc[] =
{
  { OBJ_GHOSTS        , PrintDesc_Ghosts       },
  { OBJ_BAT           , PrintDesc_Bat          },
  { OBJ_TROLL         , PrintDesc_Troll        },
  { OBJ_THIEF         , PrintDesc_Thief        },
  { OBJ_CYCLOPS       , PrintDesc_Cyclops      },
  { OBJ_INFLATED_BOAT , PrintDesc_InflatedBoat },
  { OBJ_LAMP          , PrintDesc_Lamp         },
  { OBJ_CANDLES       , PrintDesc_Candles      },

  { 0, 0 }
};
//*****************************************************************************



//*****************************************************************************
void PrintUsingMsg(int obj)
{
  PrintCompText("\x28\xfe\x84");
  PrintObjectDesc(obj, 0);
  PrintCompText("\x29\x0a");
}



void PrintFutileMsg(int obj)
{
  PrintCompText("\x55\x73\x84");

  if (obj > 0 && obj < NUM_OBJECTS)
    PrintObjectDesc(obj, 0);
  else
    PrintCompText("\xa2\x61\x74");

  PrintCompText("\xb7\xa5\x6c\xab\xef\xc6\xf7\x69\xcf\x2e\x0a");
}



void TieRopeToRailingRoutine(void)
{
  if (RopeTiedToRail) {PrintCompLine("\x85\xc2\xfc\x87\xe2\xa9\x61\x64\xc4\xf0\xd5\x89\x69\x74\x2e"); return;}

  RopeTiedToRail = 1;
  Obj[OBJ_ROPE].loc = ROOM_DOME_ROOM;
  Obj[OBJ_ROPE].prop |= PROP_NODESC;
  Obj[OBJ_ROPE].prop |= PROP_NOTTAKEABLE;

  PrintCompLine("\x85\xc2\xfc\xcc\xc2\x70\xa1\x6f\xd7\xb6\x81\x73\x69\xe8\x8d\xb3\xe1\xbe\xb7\xc7\xce\xb4\xd1\xb4\x66\xf3\xa6\xdd\x80\xc6\xd9\x6f\x72\x2e");

  TimePassed = 1;
}



void DoMiscWithTo_tie_rope(int with_to)
{
  if (with_to == 0 && Obj[OBJ_YOU].loc == ROOM_DOME_ROOM) {with_to = FOBJ_RAILING; PrintCompLine("\x28\xbd\xda\x61\x69\xf5\x6e\x67\x29");}
  if (with_to == 0) {PrintCompLine("\x50\xcf\xe0\x9e\x73\xfc\x63\x69\x66\xc4\x77\xcd\xa6\xbd\x9f\x69\x9e\xc7\x9f\x6f\x2e"); return;}
  if (with_to != FOBJ_RAILING) {PrintCompLine("\x8b\xe7\x93\xf0\x9e\x81\xc2\xfc\x89\xa2\x61\x74\x2e"); return;}

  TieRopeToRailingRoutine();
}



void DoMiscWithTo_tie_railing(int with_to)
{
  if (with_to == 0 && (Obj[OBJ_ROPE].loc == INSIDE + OBJ_YOU || Obj[OBJ_ROPE].loc == ROOM_DOME_ROOM))
  {
    with_to = OBJ_ROPE;
    PrintUsingMsg(with_to);
  }
  if (with_to == 0) {PrintCompLine("\x50\xcf\xe0\x9e\x73\xfc\x63\x69\x66\xc4\x77\xcd\xa6\xbd\x9f\x69\x9e\xc7\xb7\xc7\x68\x2e"); return;}
  if (with_to != OBJ_ROPE) {PrintCompLine("\x8b\xe7\x93\xf0\x9e\x81\xf4\x69\xf5\x9c\xf8\xa2\x95\x61\x74\x2e"); return;}

  TieRopeToRailingRoutine();
}



void DoMiscWithTo_untie_rope(int with_to)
{
  if (with_to == 0 && Obj[OBJ_YOU].loc == ROOM_DOME_ROOM && RopeTiedToRail) PrintCompLine("\x28\x66\xc2\xf9\xf4\x69\xf5\x6e\x67\x29");
  if (with_to != 0 && with_to != FOBJ_RAILING) {PrintCompLine("\x85\xc2\xfc\xa8\x73\x93\xf0\xd5\x89\xa2\x61\x74\x2e"); return;}

  if (RopeTiedToRail == 0) {PrintCompLine("\x49\xa6\x9a\xe3\xa6\xf0\xd5\x89\xad\x79\xa2\x97\x2e"); return;}

  RopeTiedToRail = 0;
  Obj[OBJ_ROPE].prop &= ~PROP_NODESC;
  Obj[OBJ_ROPE].prop &= ~PROP_NOTTAKEABLE;

  PrintCompLine("\x85\xc2\xfc\x87\xe3\x77\x20\xf6\xf0\x65\x64\x2e");

  TimePassed = 1;
}



void DoMiscWithTo_turn_bolt(int with_to)
{
  int need = OBJ_WRENCH;

  if (with_to == 0 && Obj[need].loc == INSIDE + OBJ_YOU) {with_to = need; PrintUsingMsg(with_to);}
  if (with_to == 0) {PrintCompLine("\x85\x62\x6f\x6c\xa6\x77\xca\x27\xa6\x74\xd8\xb4\xf8\xa2\x86\xb6\xef\xc5\xfb\x66\x66\xd3\x74\x2e"); return;}
  if (with_to != need) {PrintFutileMsg(with_to); return;}
  if (Obj[with_to].loc != INSIDE + OBJ_YOU) {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e"); return;}

  if (GatesButton)
  {
    TimePassed = 1;
    Room[ROOM_RESERVOIR_SOUTH].prop &= ~R_DESCRIBED;

    if (GatesOpen)
    {
      GatesOpen = 0;
      Room[ROOM_LOUD_ROOM].prop &= ~R_DESCRIBED;
      ReservoirFillCountdown = 8;
      ReservoirDrainCountdown = 0;
      PrintCompLine("\x85\x73\x6c\x75\x69\x63\x9e\x67\xaf\xbe\xb3\xd9\xd6\x8d\xb7\xaf\xac\xaa\x74\xbb\x74\xa1\xbd\xb3\x6f\xdf\x65\x63\xa6\xef\xce\xb9\x80\xcc\x61\x6d\x2e");
    }
    else
    {
      GatesOpen = 1;
      ReservoirFillCountdown = 0;
      ReservoirDrainCountdown = 8;
      PrintCompLine("\x85\x73\x6c\x75\x69\x63\x9e\x67\xaf\xbe\xae\xfc\xb4\x8c\x77\xaf\xac\xeb\xa5\x72\xa1\xa2\xc2\x75\x67\xde\x81\x64\x61\x6d\x2e");
    }
  }
  else
    PrintCompLine("\x85\x62\x6f\x6c\xa6\x77\xca\x27\xa6\x74\xd8\xb4\xf8\xa2\x86\xb6\xef\xc5\xfb\x66\x66\xd3\x74\x2e");
}



void DoMiscWithTo_fix_leak(int with_to)
{
  int need = OBJ_PUTTY;

  if (MaintenanceWaterLevel <= 0) {PrintCompLine("\x41\xa6\xcf\xe0\xa6\xca\x9e\xdd\x95\x6f\xd6\xae\x62\x6a\x65\x63\x74\xa1\xb2\x93\x76\xb2\x69\x62\xcf\xc0\xac\x65\x21"); return;}

  if (with_to == 0 && Obj[need].loc == INSIDE + OBJ_YOU) {with_to = need; PrintUsingMsg(with_to);}
  if (with_to == 0) {PrintCompLine("\x46\x69\x78\xa8\xa6\xf8\xa2\xb7\xcd\x74\x3f"); return;}
  if (with_to != need) {PrintFutileMsg(with_to); return;}
  if (Obj[with_to].loc != INSIDE + OBJ_YOU) {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e"); return;}

  TimePassed = 1;
  MaintenanceWaterLevel = -1;
  PrintCompLine("\x42\xc4\x73\xe1\x9e\x6d\x69\xf4\x63\xcf\x8a\x5a\xd3\x6b\x69\xad\x9f\x65\xfa\xe3\xd9\x67\x79\xb5\x8f\xcd\xd7\xee\xad\x61\x67\xd5\x89\xc5\x6f\x70\x80\xcb\xbf\x6b\xa8\xb4\x81\x64\x61\x6d\x2e");
}



void DoMiscWithTo_inflate_fill_inflatable_boat(int with_to)
{
  int need = OBJ_PUMP;

  if (with_to == 0 && Obj[need].loc == INSIDE + OBJ_YOU) {with_to = need; PrintUsingMsg(with_to);}
  if (with_to == 0) {PrintCompLine("\x8b\x64\xca\x27\xa6\xcd\xd7\xfb\xe3\x75\x67\xde\x6c\xf6\xc1\x70\xf2\xac\x89\xa7\x66\xfd\xd1\xa8\x74\x2e"); return;}
  if (with_to != need) {PrintFutileMsg(with_to); return;}
  if (Obj[with_to].loc != INSIDE + OBJ_YOU) {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x81\x70\x75\x6d\x70\x2e"); return;}

  if (Obj[OBJ_INFLATABLE_BOAT].loc != Obj[OBJ_YOU].loc) {PrintCompLine("\x85\x62\x6f\xaf\xee\xfe\xa6\xef\xae\xb4\x81\x67\xc2\xf6\xab\xbd\xb0\x9e\xa7\x66\xfd\xd1\x64\x2e"); return;}

  TimePassed = 1;

  PrintCompLine("\x85\x62\x6f\xaf\xa8\x6e\x66\xfd\xd1\xa1\x8c\x61\x70\xfc\xbb\xa1\xd6\x61\x77\xd3\xa2\x79\x2e");
  ItObj = OBJ_INFLATED_BOAT;

  if ((Obj[OBJ_BOAT_LABEL].prop & PROP_MOVEDDESC) == 0)
    PrintCompLine("\x41\x9f\xad\xcb\x61\xef\xea\x9a\xec\x84\xa7\x73\x69\xe8\x80\xb0\x6f\x61\x74\x2e");

  Obj[OBJ_INFLATED_BOAT].loc = Obj[OBJ_INFLATABLE_BOAT].loc;
  Obj[OBJ_INFLATABLE_BOAT].loc = 0;
}



void DoMiscWithTo_inflate_fill_inflated_boat(int with_to)
{
  PrintCompLine("\x49\x6e\x66\xfd\xf0\x9c\xc7\xc6\xd8\x96\xb6\x77\xa5\x6c\xab\x70\xc2\x62\x61\x62\xec\xb0\xd8\xc5\xa8\x74\x2e");
}



void DoMiscWithTo_inflate_fill_punctured_boat(int with_to)
{
  PrintCompLine("\x4e\xba\xfa\xad\x63\x65\xa4\x53\xe1\x9e\x6d\xd3\xca\xeb\xf6\x63\x74\xd8\xd5\xa8\x74\x2e");
}



void DoMiscWithTo_deflate_inflated_boat(int with_to)
{
  if (YouAreInBoat) {PrintCompLine("\x8b\xe7\x93\xe8\x66\xfd\xd1\x80\xb0\x6f\xaf\xb7\xce\xcf\x86\x27\xa9\xa8\xb4\x69\x74\x2e"); return;}

  if (Obj[OBJ_INFLATED_BOAT].loc != Obj[OBJ_YOU].loc) {PrintCompLine("\x85\x62\x6f\xaf\xee\xfe\xa6\xef\xae\xb4\x81\x67\xc2\xf6\xab\xbd\xb0\x9e\xe8\x66\xfd\xd1\x64\x2e"); return;}

  TimePassed = 1;

  PrintCompLine("\x85\x62\x6f\xaf\xcc\x65\x66\xfd\xd1\x73\x2e");
  ItObj = OBJ_INFLATABLE_BOAT;

  Obj[OBJ_INFLATABLE_BOAT].loc = Obj[OBJ_INFLATED_BOAT].loc;
  Obj[OBJ_INFLATED_BOAT].loc = 0;
}



void DoMiscWithTo_deflate_inflatable_boat(int with_to)
{
  PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\xe8\x66\xfd\xd1\x64\x2e");
}



void DoMiscWithTo_deflate_punctured_boat(int with_to)
{
  PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\xe8\x66\xfd\xd1\x64\x2e");
}



void DoMiscWithTo_fix_punctured_boat(int with_to)
{
  int need = OBJ_PUTTY;

  if (with_to == 0 && Obj[need].loc == INSIDE + OBJ_YOU) {with_to = need; PrintUsingMsg(with_to);}
  if (with_to == 0) {PrintCompLine("\x46\x69\x78\xa8\xa6\xf8\xa2\xb7\xcd\x74\x3f"); return;}
  if (with_to != need) {PrintFutileMsg(with_to); return;}
  if (Obj[with_to].loc != INSIDE + OBJ_YOU) {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e"); return;}

  TimePassed = 1;

  Obj[OBJ_INFLATABLE_BOAT].loc = Obj[OBJ_PUNCTURED_BOAT].loc;
  Obj[OBJ_PUNCTURED_BOAT].loc = 0;

  PrintCompLine("\x57\x65\xdf\xcc\xca\x65\x83\x9e\x62\x6f\xaf\x87\xa9\x70\x61\x69\xa9\x64\x2e");
}



void DoMisc_open_grate(void);



void LockUnlockGrating(int with_to, int lock_flag)
{
  int need = OBJ_KEYS;

  if (GratingRevealed == 0) {PrintCompLine("\x41\xa6\xcf\xe0\xa6\xca\x9e\xdd\x95\x6f\xd6\xae\x62\x6a\x65\x63\x74\xa1\xb2\x93\x76\xb2\x69\x62\xcf\xc0\xac\x65\x21"); return;}

  if (with_to == 0 && Obj[need].loc == INSIDE + OBJ_YOU) {with_to = need; PrintUsingMsg(with_to);}
  if (with_to == 0) {PrintCompLine("\xdc\x75\x27\xdf\xe4\xf3\xab\xbd\x20\xfe\x9e\x73\xe1\x65\xa2\x97\x2e"); return;}
  if (with_to != need) {PrintFutileMsg(with_to); return;}
  if (Obj[with_to].loc != INSIDE + OBJ_YOU) {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e"); return;}

  if (lock_flag)
  {
    if (GratingUnlocked == 0)
      PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\xe2\xa9\x61\x64\xc4\xd9\x63\x6b\x65\x64\x2e");
    else if (Obj[OBJ_YOU].loc == ROOM_GRATING_CLEARING)
      PrintCompLine("\x8b\xe7\x93\xd9\x63\x6b\xa8\xa6\x66\xc2\xf9\xa2\x9a\x73\x69\x64\x65\x2e");
    else
    {
      int prev_darkness;

      PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\xd9\x63\x6b\x65\x64\x2e");

      TimePassed = 1;
      GratingUnlocked = 0;
      GratingOpen = 0; // grating may already be closed here

      prev_darkness = IsPlayerInDarkness();
      Room[ROOM_GRATING_ROOM].prop &= ~R_LIT; // no light spilling from grate opening
      if (IsPlayerInDarkness() != prev_darkness)
      {
        PrintNewLine();
        PrintPlayerRoomDesc(0);
      }
    }
  }
  else //unlock
  {
    if (GratingUnlocked)
      PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\xe2\xa9\x61\x64\xc4\xf6\xd9\x63\x6b\x65\x64\x2e");
    else if (Obj[OBJ_YOU].loc == ROOM_GRATING_CLEARING)
      PrintCompLine("\x8b\xe7\x93\xa9\x61\xfa\x80\xcb\x6f\x63\x6b\xc6\xc2\xf9\xa0\x72\x65\x2e");
    else
    {
      TimePassed = 1;
      GratingUnlocked = 1;
      // grating is closed here

      DoMisc_open_grate();
    }
  }
}



void DoMiscWithTo_lock_grate(int with_to)
{
  LockUnlockGrating(with_to, 1); //1: lock
}



void DoMiscWithTo_unlock_grate(int with_to)
{
  LockUnlockGrating(with_to, 0); //0: unlock
}



void ActivateObj(int obj)
{
  int prev_darkness;

  if (Obj[obj].prop & PROP_LIT)
  {
    PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\x6f\x6e\x21");
    return;
  }

  TimePassed = 1;
  PrintCompLine("\x49\x74\x27\xa1\x6f\x6e\x2e");

  prev_darkness = IsPlayerInDarkness();
  Obj[obj].prop |= PROP_LIT;
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DeactivateObj(int obj)
{
  int prev_darkness;

  if ((Obj[obj].prop & PROP_LIT) == 0)
  {
    PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\xdd\x66\x21");
    return;
  }

  TimePassed = 1;
  PrintCompLine("\x49\x74\x27\xa1\xdd\x66\x2e");

  prev_darkness = IsPlayerInDarkness();
  Obj[obj].prop &= ~PROP_LIT;
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMiscWithTo_activate_lamp(int with_to)
{
  if (with_to != 0) PrintCompLine("\x8b\xe7\x93\xfe\x9e\xa2\x61\x74\x2e");
  else if (LampTurnsLeft == 0) PrintCompLine("\x41\xb0\xd8\xed\x64\x2d\xa5\xa6\xfd\x6d\x70\xb7\xca\x27\xa6\xf5\x67\x68\x74\x2e");
  else ActivateObj(OBJ_LAMP);
}



void DoMiscWithTo_deactivate_lamp(int with_to)
{
  if (with_to != 0) PrintCompLine("\x8b\xe7\x93\xfe\x9e\xa2\x61\x74\x2e");
  else if (LampTurnsLeft == 0) PrintCompLine("\x85\xfd\x6d\x70\xc0\xe0\xa3\x6c\xa9\x61\x64\xc4\x62\xd8\xed\xab\xa5\x74\x2e");
  else DeactivateObj(OBJ_LAMP);
}



void DoMiscWithTo_activate_match(int with_to)
{
  int prev_darkness;

  if (with_to != 0)
  {
    PrintCompLine("\x8b\xe7\x93\xfe\x9e\xa2\x61\x74\x2e");
    return;
  }

  if (Obj[OBJ_MATCH].loc != INSIDE + OBJ_YOU)
  {
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e");
    return;
  }

  if (Obj[OBJ_MATCH].prop & PROP_LIT)
  {
    PrintCompLine("\x41\xee\xaf\xfa\x87\xe2\xa9\x61\x64\xc4\xf5\x74\x2e");
    return;
  }

  if (MatchesLeft <= 1)
  {
    PrintCompLine("\x49\x27\xf9\x61\x66\xf4\x69\xab\xa2\xaf\x86\xc0\x61\xd7\xda\xf6\xae\xf7\x8a\x6d\xaf\xfa\x65\x73\x2e");
    if (MatchesLeft == 0) return;
  }
  MatchesLeft--;

  TimePassed = 1;

  if (Obj[OBJ_YOU].loc == ROOM_LOWER_SHAFT ||
      Obj[OBJ_YOU].loc == ROOM_TIMBER_ROOM)
  {
    PrintCompLine("\xbc\x9a\xc2\xe1\x87\x64\xf4\x66\x74\x79\xb5\x8c\x81\x6d\xaf\xfa\xe6\x6f\xbe\xae\xf7\xa8\x6e\xc5\xad\x74\x6c\x79\x2e");
    return;
  }

  MatchTurnsLeft = 2;
  PrintCompLine("\x4f\xed\x8a\x81\x6d\xaf\xfa\xbe\xaa\x74\xbb\x74\xa1\xbd\xb0\xd8\x6e\x2e");

  prev_darkness = IsPlayerInDarkness();
  Obj[OBJ_MATCH].prop |= PROP_LIT;
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMiscWithTo_deactivate_match(int with_to)
{
  int prev_darkness;

  if (with_to != 0)
  {
    PrintCompLine("\x8b\xe7\x93\xfe\x9e\xa2\x61\x74\x2e");
    return;
  }

  if ((Obj[OBJ_MATCH].prop & PROP_LIT) == 0)
  {
    PrintCompLine("\x4e\xba\x6d\xaf\xfa\x87\xf5\x74\x2e");
    return;
  }

  TimePassed = 1;
  MatchTurnsLeft = 0;
  PrintCompLine("\x85\x6d\xaf\xfa\x87\xa5\x74\x2e");

  prev_darkness = IsPlayerInDarkness();
  Obj[OBJ_MATCH].prop &= ~PROP_LIT;
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMiscWithTo_activate_candles(int with_to)
{
  if (CandleTurnsLeft == 0)
  {
    PrintCompLine("\x41\xfd\x73\xb5\x96\xa9\x27\xa1\xe3\xa6\x6d\x75\xfa\xcb\x65\x66\xa6\xdd\x80\x91\x64\xcf\x73\xa4\x43\xac\x74\x61\xa7\xec\xe4\xff\xfb\xe3\x75\x67\xde\xbd\xb0\xd8\x6e\x2e");
    return;
  }

  if (Obj[OBJ_CANDLES].loc != INSIDE + OBJ_YOU)
  {
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x81\xe7\xb9\xcf\x73\x2e");
    return;
  }

  if (with_to == 0 &&
      Obj[OBJ_MATCH].loc == INSIDE + OBJ_YOU &&
      (Obj[OBJ_MATCH].prop & PROP_LIT))
  {
    with_to = OBJ_MATCH;
    PrintCompLine("\x28\xf8\xa2\x80\xee\xaf\x63\x68\x29");
  }

  if (with_to == 0)
  {
    PrintCompLine("\x8b\x73\x68\xa5\x6c\xab\x73\x61\xc4\x77\xcd\xa6\xbd\xcb\x69\x67\x68\xa6\x96\xf9\xf8\x74\x68\x2e");
    return;
  }

  if (with_to == OBJ_MATCH && (Obj[OBJ_MATCH].prop && PROP_LIT))
  {
    if (Obj[OBJ_MATCH].loc != INSIDE + OBJ_YOU)
      PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x81\x6d\xaf\x63\x68\x2e");
    else if (Obj[OBJ_CANDLES].prop & PROP_LIT)
      PrintCompLine("\x85\xe7\xb9\xcf\xa1\xbb\x9e\xe2\xa9\x61\x64\xc4\xf5\x74\x2e");
    else
    {
      int prev_darkness;

      TimePassed = 1;
      PrintCompLine("\x85\xe7\xb9\xcf\xa1\xbb\x9e\xf5\x74\x2e");

      if (Obj[OBJ_YOU].loc == ROOM_ENTRANCE_TO_HADES &&
          BellRungCountdown > 0 &&
          CandlesLitCountdown == 0)
      {
        PrintCompLine("\x85\x66\xfd\x6d\xbe\xc6\xf5\x63\x6b\xac\xb7\x69\x6c\x64\xec\x8d\xa3\x70\xfc\xbb\x89\x64\xad\x63\x65\x83\x9e\xbf\x72\xa2\xb0\xd4\xbf\xa2\x86\xb6\x66\xf3\xa6\x74\xa9\x6d\x62\xcf\x73\xb5\x8c\x92\xcb\x65\x67\xa1\xed\xbb\xec\xb0\x75\x63\x6b\xcf\xb0\xd4\xbf\xa2\x86\x83\x9e\x73\x70\x69\xf1\x74\xa1\x63\xf2\xac\xa3\xa6\x92\x20\xf6\xbf\x72\xa2\xec\xeb\xf2\x65\x72\x2e");

        BellRungCountdown = 0;
        CandlesLitCountdown = 3;
      }

      prev_darkness = IsPlayerInDarkness();
      Obj[OBJ_CANDLES].prop |= PROP_LIT;
      if (IsPlayerInDarkness() != prev_darkness)
      {
        PrintNewLine();
        PrintPlayerRoomDesc(1);
      }
    }
  }
  else if (with_to == OBJ_TORCH && (Obj[OBJ_TORCH].prop && PROP_LIT))
  {
    if (Obj[OBJ_TORCH].loc != INSIDE + OBJ_YOU)
      PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x81\xbd\x72\x63\x68\x2e");
    else if (Obj[OBJ_CANDLES].prop & PROP_LIT)
      PrintCompLine("\x8b\xa9\xe2\x69\x7a\x65\xb5\x6a\xfe\xa6\xa7\x9f\x69\x6d\x65\xb5\xa2\xaf\x80\x91\x64\xcf\xa1\xbb\x9e\xe2\xa9\x61\x64\xc4\xf5\x67\x68\xd1\x64\x2e");
    else
    {
      TimePassed = 1;
      Obj[OBJ_CANDLES].loc = 0;

      PrintCompLine("\x85\xa0\xaf\xc6\xc2\xf9\x81\xbd\x72\xfa\x87\x73\xba\xa7\xd1\x6e\xd6\x95\xaf\x80\x91\x64\xcf\xa1\xbb\x9e\x76\x61\x70\xd3\x69\x7a\x65\x64\x2e");
    }
  }
  else
    PrintCompLine("\x8b\xcd\xd7\x89\xf5\x67\x68\xa6\x96\xf9\xf8\xa2\xaa\xe1\x65\xa2\x84\xa2\xaf\x27\xa1\x62\xd8\x6e\x97\xb5\x8f\x6b\xe3\x77\x2e");
}



void DoMiscWithTo_deactivate_candles(int with_to)
{
  int prev_darkness;

  if (with_to != 0)
  {
    PrintCompLine("\x8b\xe7\x93\xfe\x9e\xa2\x61\x74\x2e");
    return;
  }

  if ((Obj[OBJ_CANDLES].prop & PROP_LIT) == 0)
  {
    PrintCompLine("\x85\xe7\xb9\xcf\xa1\xbb\x9e\xe3\xa6\xf5\x67\x68\xd1\x64\x2e");
    return;
  }

  TimePassed = 1;
  PrintCompLine("\x85\x66\xfd\x6d\x9e\x9a\x65\x78\xf0\xb1\x75\xb2\xa0\x64\x2e");
  Obj[OBJ_CANDLES].prop |= PROP_MOVEDDESC; // needed since unmoved description of candles says they are burning

  prev_darkness = IsPlayerInDarkness();
  Obj[OBJ_CANDLES].prop &= ~PROP_LIT;
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMiscWithTo_activate_machine(int with_to)
{
  int found, obj;

  if (with_to == 0)
    {PrintCompLine("\x49\x74\x27\xa1\xe3\xa6\x63\xcf\xbb\xc0\xf2\x89\x74\xd8\xb4\xc7\xae\xb4\xf8\xa2\x86\xb6\x62\xbb\x9e\xcd\xb9\x73\x2e"); return;}

  if (with_to != OBJ_SCREWDRIVER)
    {PrintCompLine("\x49\xa6\xd6\x65\x6d\xa1\xa2\xaf\xb7\xca\x27\xa6\x64\x6f\x2e"); return;}

  if (Obj[OBJ_SCREWDRIVER].loc != INSIDE + OBJ_YOU)
    {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x81\x73\x63\xa9\x77\x64\xf1\xd7\x72\x2e"); return;}

  if (Obj[OBJ_MACHINE].prop & PROP_OPEN)
    {PrintCompLine("\x85\x6d\x61\xfa\xa7\x9e\x64\x6f\xbe\x93\xd6\x65\xf9\xbd\xb7\xad\xa6\xbd\xcc\xba\xad\x79\xa2\x97\x2e"); return;}

  TimePassed = 1;
  PrintCompLine("\x85\x6d\x61\xfa\xa7\x9e\x63\xe1\xbe\x89\xf5\x66\x9e\x28\x66\x69\x67\xd8\xaf\x69\xd7\xec\x29\xb7\xc7\xde\xd0\x64\x61\x7a\x7a\xf5\x9c\x64\xb2\x70\xfd\xc4\xdd\xb3\x6f\xd9\xa9\xab\xf5\x67\x68\x74\xa1\x8c\x62\x69\x7a\xbb\xa9\xe4\x6f\xb2\xbe\xa4\x41\x66\xd1\xb6\xd0\x66\x65\x77\xee\xe1\xd4\x74\x73\xb5\x81\x65\x78\x63\xc7\x65\x6d\xd4\xa6\x61\x62\xaf\x65\x73\x2e");

  found = 0;
  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == INSIDE + OBJ_MACHINE)
  {
    if (found == 0) found = 1;
    if (obj == OBJ_COAL) found = 2;
    Obj[obj].loc = 0;
  }

  if (found == 2)
    Obj[OBJ_DIAMOND].loc = INSIDE + OBJ_MACHINE;
  else if (found == 1)
    Obj[OBJ_GUNK].loc = INSIDE + OBJ_MACHINE;
}



void DoMiscWithTo_dig_sand(int with_to)
{
  int need = OBJ_SHOVEL;

  if (with_to == 0 && Obj[need].loc == INSIDE + OBJ_YOU) {with_to = need; PrintUsingMsg(with_to);}
  if (with_to == 0) {PrintCompLine("\x44\x69\x67\x67\x84\xf8\xa2\x86\xb6\xcd\xb9\xa1\x9a\x73\x69\xdf\x79\x2e"); return;}
  if (with_to != need) {PrintFutileMsg(with_to); return;}
  if (Obj[with_to].loc != INSIDE + OBJ_YOU) {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e"); return;}

  TimePassed = 1;
  CaveHoleDepth++;
  switch (CaveHoleDepth)
  {
    case 1: PrintCompLine("\x8b\xd6\x65\xf9\xbd\xb0\x9e\x64\x69\x67\x67\x84\xd0\x68\x6f\xcf\xc0\xac\x65\x2e");                break;
    case 2: PrintCompLine("\x85\x68\x6f\xcf\x87\x67\x65\x74\xf0\x9c\xe8\x65\xfc\x72\xb5\x62\xf7\x95\xaf\x27\xa1\x61\x62\xa5\xa6\x69\x74\x2e");   break;
    case 3: PrintCompLine("\x8b\xbb\x9e\x73\xd8\xc2\xf6\xe8\xab\x62\xc4\xd0\x77\xe2\xea\xdd\xaa\x8c\xca\xa3\xdf\xaa\x69\xe8\x73\x2e"); break;

    case 4:
      if (Obj[OBJ_SCARAB].prop & PROP_NODESC)
      {
        Obj[OBJ_SCARAB].prop &= ~PROP_NOTTAKEABLE;
        Obj[OBJ_SCARAB].prop &= ~PROP_NODESC;

        PrintCompLine("\x8b\xe7\xb4\xd6\x9e\xd0\x73\xe7\xf4\x62\xc0\xac\x9e\xa7\x80\xaa\xad\x64\x2e");
        ItObj = OBJ_SCARAB;
      }
      else
        PrintCompLine("\x8b\x66\xa7\xab\xe3\xa2\x84\x65\x6c\x73\x65\x2e");
    break;

    default:
      CaveHoleDepth = 0;
      if (Obj[OBJ_SCARAB].loc == ROOM_SANDY_CAVE)
      {
        Obj[OBJ_SCARAB].prop |= PROP_NOTTAKEABLE;
        Obj[OBJ_SCARAB].prop |= PROP_NODESC;
      }
      PrintCompLine("\x85\x68\x6f\xcf\xb3\x6f\xdf\x61\x70\xd6\x73\xb5\x73\x6d\xff\xa0\xf1\x9c\xc9\x75\x2e");
      YoureDead(); // ##### RIP #####
    break;
  }
}



void DoMiscWithTo_fill_bottle(int with_to)
{
  if (with_to == 0 && (Room[Obj[OBJ_YOU].loc].prop & R_WATERHERE))
    {with_to = OBJ_WATER; PrintCompLine("\x28\xf8\xa2\xb7\xaf\x65\x72\x29");}

  if (with_to == 0) {PrintCompLine("\x46\x69\xdf\xa8\xa6\xf8\xa2\xb7\xcd\x74\x3f"); return;}
  if (with_to != OBJ_WATER) {PrintCompLine("\x8b\xe7\x93\x66\x69\xdf\xa8\xa6\xf8\xa2\x95\x61\x74\x21"); return;}
  if ((Room[Obj[OBJ_YOU].loc].prop & R_WATERHERE) == 0) {PrintCompLine("\x99\xa9\x27\xa1\xe3\xb7\xaf\xac\xc0\xac\x65\x21"); return;}
  if ((Obj[OBJ_BOTTLE].prop & PROP_OPEN) == 0)
  {
    PrintCompLine("\x85\x62\xff\x74\xcf\x87\x63\xd9\xd6\x64\x2e");
    ItObj = OBJ_BOTTLE;
    return;
  }
  if (Obj[OBJ_WATER].loc == INSIDE + OBJ_BOTTLE) {PrintCompLine("\x85\x62\xff\x74\xcf\x87\xe2\xa9\x61\x64\xc4\x66\x75\xdf\x8a\x77\xaf\x65\x72\x2e"); return;}

  TimePassed = 1;
  Obj[OBJ_WATER].loc = INSIDE + OBJ_BOTTLE;
  PrintCompLine("\x85\x62\xff\x74\xcf\x87\xe3\x77\xc6\x75\xdf\x8a\x77\xaf\x65\x72\x2e");
}



void AttackVillain(int obj, int with_to)
{
  if (with_to >= NUM_OBJECTS)
    {PrintCompLine("\x8b\xbb\xd4\x27\xa6\x68\x6f\x6c\x64\x84\xa2\x61\x74\x21"); return;}

  if (with_to == 0)
  {
    int i;

    for (i=2; i<NUM_OBJECTS; i++)
    {
      with_to = Obj[i].order;
      if (Obj[with_to].loc == INSIDE + OBJ_YOU &&
          (Obj[with_to].prop & PROP_WEAPON)) break;
    }

    if (i == NUM_OBJECTS) with_to = 0;
    else PrintUsingMsg(with_to);
  }

  if (obj == OBJ_BAT)
  {
    PrintCompLine("\x8b\xe7\x93\xa9\x61\xfa\xc0\x69\x6d\x3b\xc0\x65\x27\xa1\xca\x80\xb3\x65\x69\xf5\x6e\x67\x2e");
    return;
  }
  else if (obj == OBJ_GHOSTS)
  {
    if (with_to == 0) PrintCompLine("\x8b\xd6\x65\xf9\xf6\x61\x62\xcf\x89\xa7\xd1\xf4\x63\xa6\xf8\xa2\x80\xd6\xaa\x70\x69\xf1\x74\x73\x2e");
    else              PrintCompLine("\x48\xf2\x91\x86\xa3\x74\x74\x61\x63\x6b\xa3\xaa\x70\x69\xf1\xa6\xf8\xa2\xee\xaf\xac\x69\xe2\xae\x62\x6a\x65\x63\x74\x73\x3f");
    return;
  }
  else if (obj == OBJ_THIEF && (Obj[OBJ_THIEF].prop & PROP_NODESC))
  {
    PrintCompLine("\x8b\xd6\x6e\xd6\xaa\xe1\x65\xca\x9e\xed\xbb\x62\x79\xb5\x62\xf7\x91\x27\xa6\xd6\x9e\x96\x6d\x2e");
    return;
  }

  if (with_to == 0 || with_to == OBJ_YOU)
  {
    PrintCompText("\x54\x72\x79\x84\xbd\xa3\x74\x74\x61\x63\x6b\x20"); if (obj == OBJ_YOU) PrintCompText("\x92\xd6\x6c\x66"); else PrintCompText("\x69\x74");
    PrintCompLine("\xb7\xc7\xde\x92\xb0\xbb\x9e\xcd\xb9\xa1\x9a\x73\x75\x69\x63\x69\x64\x61\x6c\x2e");
    return;
  }

  if ((Obj[with_to].prop & PROP_WEAPON) == 0)
  {
    PrintCompText("\x54\x72\x79\x84\xbd\xa3\x74\x74\x61\x63\x6b\x20"); if (obj == OBJ_YOU) PrintCompText("\x92\xd6\x6c\x66"); else PrintCompText("\x69\x74");
    PrintCompLine("\xb7\xc7\xde\xa2\xaf\x87\x73\x75\x69\x63\x69\x64\x61\x6c\x2e");
    return;
  }

  TimePassed = 1;

  if (with_to == OBJ_RUSTY_KNIFE)
  {
    Obj[OBJ_RUSTY_KNIFE].loc = 0;
    PrintCompLine("\x41\xa1\x81\x6b\x6e\x69\x66\x9e\x61\x70\x70\xc2\x61\xfa\xbe\xa8\x74\xa1\x76\x69\x63\xf0\x6d\xb5\x92\xee\xa7\xab\x9a\x73\x75\x62\x6d\xac\x67\xd5\xb0\xc4\xad\xae\xd7\x72\x6d\xe0\xd1\xf1\x9c\xf8\xdf\xa4\x53\xd9\x77\xec\xb5\x92\xc0\x8c\x74\xd8\x6e\x73\xb5\xf6\xf0\xea\x81\x72\xfe\x74\xc4\x62\xfd\xe8\x87\xad\xa8\x6e\xfa\xc6\xc2\xf9\x92\xe4\x65\x63\x6b\x83\x9e\x6b\x6e\x69\x66\x9e\xd6\x65\x6d\xa1\xbd\xaa\x84\xe0\xa8\xa6\x73\x61\x76\x61\x67\x65\xec\xaa\xf5\x74\xa1\x92\x95\xc2\x61\x74\x2e");
    YoureDead(); // ##### RIP #####
    return;
  }

  if (obj == OBJ_CYCLOPS && CyclopsState == 3) // asleep
  {
    CyclopsState = 0;
    VillainAttacking[VILLAIN_CYCLOPS] = 1;
    PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x79\x61\x77\x6e\xa1\x8c\xc5\xbb\xbe\xa3\xa6\x81\xa2\x84\xa2\xaf\xb7\x6f\x6b\x9e\xce\xf9\x75\x70\x2e");
    return;
  }

  PlayerBlow(obj, with_to);
}



void DoMiscWithTo_attack_bat     (int with_to) {AttackVillain(OBJ_BAT    , with_to);}
void DoMiscWithTo_attack_ghosts  (int with_to) {AttackVillain(OBJ_GHOSTS , with_to);}
void DoMiscWithTo_attack_cyclops (int with_to) {AttackVillain(OBJ_CYCLOPS, with_to);}
void DoMiscWithTo_attack_thief   (int with_to) {AttackVillain(OBJ_THIEF  , with_to);}
void DoMiscWithTo_attack_troll   (int with_to) {AttackVillain(OBJ_TROLL  , with_to);}
void DoMiscWithTo_attack_yourself(int with_to) {AttackVillain(OBJ_YOU    , with_to);}



int CheckFlameSource(int obj, char *msg)
{
  if (Obj[obj].loc == INSIDE + OBJ_YOU &&
      (Obj[obj].prop & PROP_LIT))
  {
    PrintLine(msg);
    return obj;
  }
  return 0;
}



void BurnObj(int obj, int with)
{
  if (with == 0) with = CheckFlameSource(OBJ_MATCH  , "(with the match)");
  if (with == 0) with = CheckFlameSource(OBJ_CANDLES, "(with the candles)");
  if (with == 0) with = CheckFlameSource(OBJ_TORCH  , "(with the torch)");

  if (with == 0)
    {PrintCompLine("\x8b\x73\x68\xa5\x6c\xab\x73\x61\xc4\x77\xcd\xa6\xbd\xcb\x69\x67\x68\xa6\xc7\xb7\xc7\x68\x2e"); return;}

  if (Obj[with].loc != INSIDE + OBJ_YOU)
  {
    switch (with)
    {
      case OBJ_MATCH:   PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x81\x6d\xaf\x63\x68\x2e");   break;
      case OBJ_CANDLES: PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x81\xe7\xb9\xcf\x73\x2e"); break;
      case OBJ_TORCH:   PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x81\xbd\x72\x63\x68\x2e");   break;
      default:          PrintCompLine("\x8b\xe7\x93\xf5\x67\x68\xa6\xc7\xb7\xc7\xde\xa2\x61\x74\x21");   break;
    }
    return;
  }

  if ((Obj[with].prop & PROP_LIT) == 0)
    {PrintCompLine("\x8b\xcd\xd7\x89\xf5\x67\x68\xa6\xc7\xb7\xc7\xde\x73\xe1\x65\xa2\x84\xa2\xaf\x27\xa1\x62\xd8\x6e\x97\xb5\x8f\x6b\xe3\x77\x2e"); return;}

  if (obj == FOBJ_WHITE_HOUSE)
    {PrintCompLine("\x8b\x6d\xfe\xa6\xef\x20\x6a\x6f\x6b\x97\x2e"); return;}
  else if (obj == FOBJ_FRONT_DOOR)
    {PrintCompLine("\x8b\xe7\x6e\xe3\xa6\x62\xd8\xb4\xa2\x9a\x64\xe9\x72\x2e"); return;}
  else if (obj >= NUM_OBJECTS)
    {PrintCompLine("\x8b\xe7\x93\x62\xd8\xb4\xa2\x61\x74\x21"); return;}

  TimePassed = 1;

  if (obj == OBJ_INFLATED_BOAT && YouAreInBoat)
  {
    PrintCompLine("\x49\xa6\xe7\x74\xfa\xbe\xc6\x69\xa9\xa4\x55\x6e\x66\xd3\x74\xf6\xaf\x65\xec\xb5\x8f\x77\xac\x9e\xa7\xa8\xa6\xaf\x80\x9f\x69\x6d\x65\x2e");
    YouAreInBoat = 0;
    Obj[obj].loc = 0;
    YoureDead(); // ##### RIP #####
    return;
  }

  if (Obj[obj].loc == INSIDE + OBJ_YOU)
  {
    if (obj == OBJ_LEAVES)
      PrintCompLine("\x85\xcf\x61\xd7\xa1\x62\xd8\x6e\xb5\x8c\x73\xba\x64\xba\xc9\x75\x2e");
    else
      PrintCompLine("\x49\xa6\xe7\x74\xfa\xbe\xc6\x69\xa9\xa4\x55\x6e\x66\xd3\x74\xf6\xaf\x65\xec\xb5\x8f\x77\xac\x9e\x68\x6f\x6c\x64\x84\xc7\xa3\xa6\x81\xf0\x6d\x65\x2e");
    Obj[obj].loc = 0;
    YoureDead(); // ##### RIP #####
    return;
  }

  Obj[obj].loc = 0;

  if (obj == OBJ_LEAVES)
  {
    PrintCompLine("\x85\xcf\x61\xd7\xa1\x62\xd8\x6e\x2e");
    if (GratingRevealed == 0)
    {
      GratingRevealed = 1;
      PrintCompLine("\x49\xb4\x64\xb2\x74\xd8\x62\x84\x81\x70\x69\xcf\x8a\xcf\x61\xd7\x73\xb5\xd0\x67\xf4\xf0\x9c\x9a\xa9\xd7\xe2\x65\x64\x2e");
    }
  }
  else if (obj == OBJ_BOOK)
  {
    PrintCompLine("\x41\xb0\xe9\x6d\x84\x76\x6f\x69\x63\x9e\x73\x61\x79\xa1\x22\x57\xc2\xb1\xb5\x63\xa9\xf0\x6e\x21\x22\x8d\x86\xe4\xff\x69\x63\x9e\xa2\xaf\x86\xc0\x61\xd7\x9f\xd8\xed\xab\xa7\xbd\xa3\xeb\x69\xcf\x8a\x64\xfe\x74\xa4\x48\xf2\xb5\x49\x91\x27\xa6\x69\x6d\x61\x67\xa7\x65\x2e");
    YoureDead(); // ##### RIP #####
  }
  else
    PrintCompLine("\x49\xa6\xe7\x74\xfa\xbe\xc6\x69\xa9\x8d\x87\x63\xca\x73\x75\x6d\x65\x64\x2e");
}



void DoMiscWithTo_activate_leaves         (int with_to) {BurnObj(OBJ_LEAVES         , with_to);}
void DoMiscWithTo_activate_book           (int with_to) {BurnObj(OBJ_BOOK           , with_to);}
void DoMiscWithTo_activate_sandwich_bag   (int with_to) {BurnObj(OBJ_SANDWICH_BAG   , with_to);}
void DoMiscWithTo_activate_advertisement  (int with_to) {BurnObj(OBJ_ADVERTISEMENT  , with_to);}
void DoMiscWithTo_activate_inflated_boat  (int with_to) {BurnObj(OBJ_INFLATED_BOAT  , with_to);}
void DoMiscWithTo_activate_painting       (int with_to) {BurnObj(OBJ_PAINTING       , with_to);}
void DoMiscWithTo_activate_punctured_boat (int with_to) {BurnObj(OBJ_PUNCTURED_BOAT , with_to);}
void DoMiscWithTo_activate_inflatable_boat(int with_to) {BurnObj(OBJ_INFLATABLE_BOAT, with_to);}
void DoMiscWithTo_activate_coal           (int with_to) {BurnObj(OBJ_COAL           , with_to);}
void DoMiscWithTo_activate_boat_label     (int with_to) {BurnObj(OBJ_BOAT_LABEL     , with_to);}
void DoMiscWithTo_activate_guide          (int with_to) {BurnObj(OBJ_GUIDE          , with_to);}
void DoMiscWithTo_activate_nest           (int with_to) {BurnObj(OBJ_NEST           , with_to);}
void DoMiscWithTo_activate_white_house    (int with_to) {BurnObj(FOBJ_WHITE_HOUSE   , with_to);}
void DoMiscWithTo_activate_front_door     (int with_to) {BurnObj(FOBJ_FRONT_DOOR    , with_to);}



void DoMiscWithTo_activate_torch(int with_to)
{
  PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\x62\xd8\x6e\x97\x2e");
}



void DoMiscWithTo_deactivate_torch(int with_to)
{
  PrintCompLine("\x8b\xed\xbb\xec\xb0\xd8\xb4\x92\xc0\x8c\x74\x72\x79\x84\xbd\xfb\x78\xf0\xb1\x75\xb2\xde\x81\x66\xfd\x6d\x65\x2e");
}



void DoMiscWithTo_turn_book(int with_to)
{
  PrintCompLine("\x42\xbe\x69\xe8\xeb\x61\x67\x9e\x35\x36\x39\xb5\x96\xa9\x87\xca\xec\xae\xed\xae\x96\xb6\x70\x61\x67\x9e\xf8\xa2\xa3\x6e\xc4\xcf\x67\x69\x62\xcf\xeb\xf1\xe5\x84\xca\xa8\x74\xa4\x4d\x6f\xc5\x8a\xc7\x87\xf6\xa9\x61\x64\x61\x62\xcf\xb5\x62\xf7\x80\xaa\x75\x62\x6a\x65\x63\xa6\xd6\x65\x6d\xa1\xbd\xb0\x9e\x81\x62\xad\xb2\x68\x6d\xd4\xa6\xdd\xfb\x76\x69\x6c\xa4\x41\x70\x70\xbb\xd4\x74\xec\xb5\x63\xac\x74\x61\xa7\xe4\x6f\xb2\xbe\xb5\xf5\x67\x68\x74\x73\xb5\x8c\x70\xf4\x79\xac\xa1\xbb\x9e\x65\x66\x66\x69\xe7\x63\x69\xa5\xa1\xa7\x95\x9a\xa9\x67\xbb\x64\x2e");
}



void DoMiscWithTo_pour_water(int with_to)
{
  if (Obj[OBJ_BOTTLE].loc != INSIDE + OBJ_YOU ||
      Obj[OBJ_WATER].loc != INSIDE + OBJ_BOTTLE)
    PrintCompLine("\x8b\x64\xca\x27\xa6\xcd\xd7\xa3\x6e\xc4\x77\xaf\x65\x72\x2e");
  else if ((Obj[OBJ_BOTTLE].prop & PROP_OPEN) == 0)
    PrintCompLine("\xdc\x75\x27\xdf\xc0\x61\xd7\x89\x6f\xfc\xb4\x81\x62\xff\x74\xcf\xc6\x69\x72\x73\x74\x2e");
  else if (with_to == 0)
    PrintCompLine("\x8b\xed\xd5\x89\x70\x90\xc7\xae\xb4\x73\xe1\x65\xa2\x97\x2e");
  else
  {
    TimePassed = 1;
    Obj[OBJ_WATER].loc = 0;      

    switch (with_to)
    {
      case OBJ_HOT_BELL:
        PrintCompLine("\x85\x77\xaf\xac\xb3\xe9\x6c\xa1\x81\xef\xdf\x8d\x87\x65\x76\x61\x70\xd3\xaf\x65\x64\x2e");
        BellHotCountdown = 0;
        Obj[OBJ_BELL].loc = ROOM_ENTRANCE_TO_HADES;
        Obj[OBJ_HOT_BELL].loc = 0;
      break;

      case OBJ_TORCH:
        PrintCompLine("\x85\x77\xaf\xac\xfb\x76\x61\x70\xd3\xaf\xbe\xb0\x65\x66\xd3\x9e\xc7\xe6\x65\x74\xa1\x63\xd9\x73\x65\x2e");
      break;

      case OBJ_MATCH:
      case OBJ_CANDLES:
        if (Obj[with_to].prop & PROP_LIT)
        {
          int prev_darkness = IsPlayerInDarkness();

          PrintCompLine("\x49\xa6\x9a\x65\x78\xf0\xb1\x75\xb2\xa0\x64\x2e");
          Obj[with_to].prop &= ~PROP_LIT;
          if (with_to == OBJ_MATCH) MatchTurnsLeft = 0;

          if (IsPlayerInDarkness() != prev_darkness)
          {
            PrintNewLine();
            PrintPlayerRoomDesc(1);
          }
        }
        else
          PrintCompLine("\x85\x77\xaf\xac\xaa\x70\x69\xdf\xa1\x6f\xd7\xb6\xc7\xb5\xbd\x80\xc6\xd9\xd3\xb5\x8c\x65\x76\x61\x70\xd3\xaf\x65\x73\x2e");
      break;

      default: // note that this includes with_to >= NUM_OBJECTS
        PrintCompLine("\x85\x77\xaf\xac\xaa\x70\x69\xdf\xa1\x6f\xd7\xb6\xc7\xb5\xbd\x80\xc6\xd9\xd3\xb5\x8c\x65\x76\x61\x70\xd3\xaf\x65\x73\x2e");
      break;
    }
  }
}



void DoMiscWithTo_pour_putty(int with_to)
{
  if (Obj[OBJ_PUTTY].loc != INSIDE + OBJ_YOU &&
      ( Obj[OBJ_TUBE].loc != INSIDE + OBJ_YOU ||
        Obj[OBJ_PUTTY].loc != INSIDE + OBJ_TUBE))
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e");
  else if ((Obj[OBJ_TUBE].prop & PROP_OPEN) == 0)
    PrintCompLine("\x85\x74\x75\xef\x87\x63\xd9\xd6\x64\x2e");
  else if (with_to == 0)
    PrintCompLine("\x8b\xed\xd5\x89\x70\x90\xc7\xae\xb4\x73\xe1\x65\xa2\x97\x2e");
  else
    switch (with_to)
  {
    case FOBJ_LEAK:
      if (MaintenanceWaterLevel <= 0)
        PrintCompLine("\x41\xa6\xcf\xe0\xa6\xca\x9e\xdd\x95\x6f\xd6\xae\x62\x6a\x65\x63\x74\xa1\xb2\x93\x76\xb2\x69\x62\xcf\xc0\xac\x65\x21");
      else
      {
        TimePassed = 1;
        MaintenanceWaterLevel = -1;
        PrintCompLine("\x42\xc4\x73\xe1\x9e\x6d\x69\xf4\x63\xcf\x8a\x5a\xd3\x6b\x69\xad\x9f\x65\xfa\xe3\xd9\x67\x79\xb5\x8f\xcd\xd7\xee\xad\x61\x67\xd5\x89\xc5\x6f\x70\x80\xcb\xbf\x6b\xa8\xb4\x81\x64\x61\x6d\x2e");
      }
    break;

    case OBJ_PUNCTURED_BOAT:
      TimePassed = 1;
      Obj[OBJ_INFLATABLE_BOAT].loc = Obj[OBJ_PUNCTURED_BOAT].loc;
      Obj[OBJ_PUNCTURED_BOAT].loc = 0;
      PrintCompLine("\x57\x65\xdf\xcc\xca\x65\x83\x9e\x62\x6f\xaf\x87\xa9\x70\x61\x69\xa9\x64\x2e");
    break;

    default: // note that this includes with_to >= NUM_OBJECTS
      PrintCompLine("\xbc\xaf\xb7\xa5\x6c\xab\xef\xc6\xf7\x69\x6c\x65\x2e");
    break;
  }
}



void DoMiscWithTo_oil_bolt(int with_to)
{
  if (with_to == 0)
    PrintCompLine("\x4f\x69\xea\xc7\xb7\xc7\xde\x77\xcd\x74\x3f");
  else if (with_to != OBJ_PUTTY)
    PrintCompLine("\x8b\xe7\x93\x6f\x69\xea\xc7\xb7\xc7\xde\xa2\x61\x74\x21");
  else if (Obj[with_to].loc != INSIDE + OBJ_YOU)
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e");
  else
  {
    TimePassed = 1;
    PrintCompLine("\x48\x6d\x6d\xa4\x49\xa6\x61\x70\xfc\xbb\xa1\x81\x74\x75\xef\xb3\xca\x74\x61\xa7\xd5\xe6\x6c\x75\x65\xb5\xe3\xa6\x6f\x69\x6c\x9d\xd8\x6e\x84\x81\x62\x6f\x6c\xa6\x77\xca\x27\xa6\x67\x65\xa6\xad\xc4\xbf\x73\x69\xac\x2e\x2e\x2e\x2e");
  }
}



void DoMiscWithTo_brush_teeth(int with_to)
{
  if (with_to == 0)
    PrintCompLine("\x44\xd4\x74\xe2\xc0\x79\x67\x69\xd4\x9e\x9a\xce\x67\x68\xec\xda\x65\x63\xe1\x6d\xd4\xe8\x64\xb5\x62\xf7\x20\x49\x27\xf9\xe3\xa6\x73\xd8\x9e\x77\xcd\xa6\x8f\x77\xad\xa6\xbd\xb0\x72\xfe\xde\x96\xf9\xf8\x74\x68\x2e");
  else if (with_to != OBJ_PUTTY)
    PrintCompLine("\x41\xe4\x69\x63\x9e\x69\xe8\x61\xb5\x62\xf7\xb7\xc7\xde\xa2\x61\x74\x3f");
  else if (Obj[with_to].loc != INSIDE + OBJ_YOU)
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e");
  else
  {
    TimePassed = 1;
    PrintCompLine("\x57\x65\xdf\xb5\x8f\xd6\x65\xf9\xbd\xc0\x61\xd7\xb0\xf3\xb4\x62\x72\xfe\xce\x9c\x92\x9f\xf3\xa2\xb7\xc7\xde\x73\xe1\x9e\x73\xd3\xa6\xdd\xe6\x6c\x75\x65\xa4\x41\xa1\xd0\xa9\x73\x75\x6c\x74\xb5\x92\xee\xa5\xa2\xe6\x65\x74\xa1\x67\x6c\x75\xd5\x9f\x6f\x67\x65\x96\xb6\x28\xf8\xa2\x86\xb6\xe3\xd6\x29\x8d\x86\xcc\x69\x9e\xdd\xda\xbe\x70\x69\xf4\xbd\x72\xc4\x66\x61\x69\x6c\xd8\x65\x2e");
    YoureDead(); // ##### RIP #####
  }
}



void TieUpRoutine(int i, int with_to)
{
  if (with_to == 0 && Obj[OBJ_ROPE].loc == INSIDE + OBJ_YOU)
  {
    with_to = OBJ_ROPE;
    PrintUsingMsg(with_to);
  }
  if (with_to == 0) {PrintCompLine("\x50\xcf\xe0\x9e\x73\xfc\x63\x69\x66\xc4\x77\xcd\xa6\xbd\x9f\x69\x9e\xce\xf9\xf8\x74\x68\x2e"); return;}
  if (with_to != OBJ_ROPE) {PrintCompLine("\x8b\xe7\x93\xf0\x9e\xce\xf9\xf8\xa2\x95\x61\x74\x21"); return;}


  if (i == VILLAIN_CYCLOPS)
    PrintCompLine("\x8b\xe7\x6e\xe3\xa6\xf0\x9e\x81\x63\x79\x63\xd9\x70\x73\xb5\xa2\xa5\x67\xde\x94\x9a\x66\xc7\x89\xef\x9f\x69\x65\x64\x2e");
  else
  {
    char *name;

    if (i == VILLAIN_THIEF) name = "thief"; else name = "troll";

    if (VillainStrength[i] < 0)
    {
      PrintCompText("\xdc\xd8\xa3\x74\xd1\x6d\x70\xa6\xbd\x9f\x69\x9e\x75\x70\x80\x20"); PrintText(name); PrintCompLine("\xa3\x77\x61\x6b\xd4\xa1\xce\x6d\x2e");
      VillainStrength[i] = -VillainStrength[i];
      VillainConscious(i);
    }
    else
      {PrintCompText("\x85"); PrintText(name); PrintCompLine("\xaa\x74\x72\x75\x67\x67\xcf\xa1\x8c\x8f\xe7\x6e\xe3\xa6\xf0\x9e\xce\xf9\x75\x70\x2e");}
  }
}



void DoMiscWithTo_tie_cyclops(int with_to) {TieUpRoutine(VILLAIN_CYCLOPS, with_to);}
void DoMiscWithTo_tie_thief  (int with_to) {TieUpRoutine(VILLAIN_THIEF  , with_to);}
void DoMiscWithTo_tie_troll  (int with_to) {TieUpRoutine(VILLAIN_TROLL  , with_to);}



struct DOMISCWITH_STRUCT DoMiscWithTo[] =
{
  { A_TIE        , OBJ_ROPE            , DoMiscWithTo_tie_rope                     },
  { A_TIE        , FOBJ_RAILING        , DoMiscWithTo_tie_railing                  },
  { A_UNTIE      , OBJ_ROPE            , DoMiscWithTo_untie_rope                   },
  { A_TURN       , FOBJ_BOLT           , DoMiscWithTo_turn_bolt                    },
  { A_FIX        , FOBJ_LEAK           , DoMiscWithTo_fix_leak                     },
  { A_INFLATE    , OBJ_INFLATABLE_BOAT , DoMiscWithTo_inflate_fill_inflatable_boat },
  { A_INFLATE    , OBJ_INFLATED_BOAT   , DoMiscWithTo_inflate_fill_inflated_boat   },
  { A_INFLATE    , OBJ_PUNCTURED_BOAT  , DoMiscWithTo_inflate_fill_punctured_boat  },
  { A_FILL       , OBJ_INFLATABLE_BOAT , DoMiscWithTo_inflate_fill_inflatable_boat },
  { A_FILL       , OBJ_INFLATED_BOAT   , DoMiscWithTo_inflate_fill_inflated_boat   },
  { A_FILL       , OBJ_PUNCTURED_BOAT  , DoMiscWithTo_inflate_fill_punctured_boat  },
  { A_DEFLATE    , OBJ_INFLATED_BOAT   , DoMiscWithTo_deflate_inflated_boat        },
  { A_DEFLATE    , OBJ_INFLATABLE_BOAT , DoMiscWithTo_deflate_inflatable_boat      },
  { A_DEFLATE    , OBJ_PUNCTURED_BOAT  , DoMiscWithTo_deflate_punctured_boat       },
  { A_FIX        , OBJ_PUNCTURED_BOAT  , DoMiscWithTo_fix_punctured_boat           },
  { A_LOCK       , FOBJ_GRATE          , DoMiscWithTo_lock_grate                   },
  { A_UNLOCK     , FOBJ_GRATE          , DoMiscWithTo_unlock_grate                 },
  { A_ACTIVATE   , OBJ_LAMP            , DoMiscWithTo_activate_lamp                },
  { A_DEACTIVATE , OBJ_LAMP            , DoMiscWithTo_deactivate_lamp              },
  { A_ACTIVATE   , OBJ_MATCH           , DoMiscWithTo_activate_match               },
  { A_DEACTIVATE , OBJ_MATCH           , DoMiscWithTo_deactivate_match             },
  { A_ACTIVATE   , OBJ_CANDLES         , DoMiscWithTo_activate_candles             },
  { A_DEACTIVATE , OBJ_CANDLES         , DoMiscWithTo_deactivate_candles           },
  { A_ACTIVATE   , OBJ_MACHINE         , DoMiscWithTo_activate_machine             },
  { A_ACTIVATE   , FOBJ_MACHINE_SWITCH , DoMiscWithTo_activate_machine             },
  { A_TURN       , FOBJ_MACHINE_SWITCH , DoMiscWithTo_activate_machine             },
  { A_DIG        , FOBJ_SAND           , DoMiscWithTo_dig_sand                     },
  { A_FILL       , OBJ_BOTTLE          , DoMiscWithTo_fill_bottle                  },
  { A_ATTACK     , OBJ_BAT             , DoMiscWithTo_attack_bat                   },
  { A_ATTACK     , OBJ_GHOSTS          , DoMiscWithTo_attack_ghosts                },
  { A_ATTACK     , OBJ_CYCLOPS         , DoMiscWithTo_attack_cyclops               },
  { A_ATTACK     , OBJ_THIEF           , DoMiscWithTo_attack_thief                 },
  { A_ATTACK     , OBJ_TROLL           , DoMiscWithTo_attack_troll                 },
  { A_ATTACK     , OBJ_YOU             , DoMiscWithTo_attack_yourself              },
  { A_ACTIVATE   , OBJ_LEAVES          , DoMiscWithTo_activate_leaves              },
  { A_ACTIVATE   , OBJ_BOOK            , DoMiscWithTo_activate_book                },
  { A_ACTIVATE   , OBJ_SANDWICH_BAG    , DoMiscWithTo_activate_sandwich_bag        },
  { A_ACTIVATE   , OBJ_ADVERTISEMENT   , DoMiscWithTo_activate_advertisement       },
  { A_ACTIVATE   , OBJ_INFLATED_BOAT   , DoMiscWithTo_activate_inflated_boat       },
  { A_ACTIVATE   , OBJ_PAINTING        , DoMiscWithTo_activate_painting            },
  { A_ACTIVATE   , OBJ_PUNCTURED_BOAT  , DoMiscWithTo_activate_punctured_boat      },
  { A_ACTIVATE   , OBJ_INFLATABLE_BOAT , DoMiscWithTo_activate_inflatable_boat     },
  { A_ACTIVATE   , OBJ_COAL            , DoMiscWithTo_activate_coal                },
  { A_ACTIVATE   , OBJ_BOAT_LABEL      , DoMiscWithTo_activate_boat_label          },
  { A_ACTIVATE   , OBJ_GUIDE           , DoMiscWithTo_activate_guide               },
  { A_ACTIVATE   , OBJ_NEST            , DoMiscWithTo_activate_nest                },
  { A_ACTIVATE   , FOBJ_WHITE_HOUSE    , DoMiscWithTo_activate_white_house         },
  { A_ACTIVATE   , FOBJ_FRONT_DOOR     , DoMiscWithTo_activate_front_door          },
  { A_ACTIVATE   , OBJ_TORCH           , DoMiscWithTo_activate_torch               },
  { A_DEACTIVATE , OBJ_TORCH           , DoMiscWithTo_deactivate_torch             },
  { A_TURN       , OBJ_BOOK            , DoMiscWithTo_turn_book                    },
  { A_POUR       , OBJ_WATER           , DoMiscWithTo_pour_water                   },
  { A_POUR       , OBJ_PUTTY           , DoMiscWithTo_pour_putty                   },
  { A_OIL        , FOBJ_BOLT           , DoMiscWithTo_oil_bolt                     },
  { A_BRUSH      , OBJ_YOU             , DoMiscWithTo_brush_teeth                  },
  { A_TIE        , OBJ_CYCLOPS         , DoMiscWithTo_tie_cyclops                  },
  { A_TIE        , OBJ_THIEF           , DoMiscWithTo_tie_thief                    },
  { A_TIE        , OBJ_TROLL           , DoMiscWithTo_tie_troll                    },

  { 0, 0, 0 }
};
//*****************************************************************************



//*****************************************************************************
void GiveLunchToCyclops(void)
{
  TimePassed = 1;

  CyclopsCounter = 0;
  CyclopsState = 2; // thirsty

  Obj[OBJ_LUNCH].loc = 0;

  PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x73\x61\x79\xa1\x22\x4d\x6d\xf9\x4d\x6d\x6d\xa4\x49\xcb\x6f\xd7\xc0\xff\xeb\x65\x70\xfc\x72\x73\x21\x20\x42\xf7\xae\x68\xb5\x63\xa5\x6c\xab\x49\x20\xfe\x9e\xd0\x64\xf1\x6e\x6b\xa4\x50\xac\xcd\x70\xa1\x49\xb3\xa5\x6c\xab\x64\xf1\x6e\x6b\x80\xb0\xd9\x6f\xab\xdd\x95\xaf\x95\x97\x2e\x22\x20\x20\x46\xc2\xf9\x81\x67\xcf\x61\xf9\xa7\xc0\x9a\x65\x79\x65\xb5\xc7\xb3\xa5\x6c\xab\xef\xaa\xd8\x6d\xb2\xd5\x95\xaf\x86\xa3\xa9\x20\x22\xa2\xaf\x95\x97\x22\x2e");
}



void GiveBottleToCyclops(void)
{
  TimePassed = 1;

  if (Obj[OBJ_WATER].loc != INSIDE + OBJ_BOTTLE)
    PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\xa9\x66\xfe\xbe\x80\xfb\x6d\x70\x74\xc4\x62\xff\x74\x6c\x65\x2e");
  else if (CyclopsState != 2) // not thirsty
    PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x61\x70\x70\xbb\xd4\x74\xec\x87\xe3\xa6\xa2\x69\x72\xc5\xc4\x8c\xa9\x66\xfe\xbe\x86\xb6\x67\xd4\xac\xa5\xa1\xdd\x66\x65\x72\x2e");
  else
  {
    CyclopsState = 3; // asleep

    Obj[OBJ_WATER].loc = 0;
    Obj[OBJ_BOTTLE].loc = ROOM_CYCLOPS_ROOM;
    Obj[OBJ_BOTTLE].prop |= PROP_OPEN;

    PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x74\x61\x6b\xbe\x80\xb0\xff\x74\xcf\xb5\xfa\x65\x63\x6b\xa1\xa2\xaf\xa8\x74\x27\xa1\x6f\xfc\x6e\xb5\x8c\x64\xf1\x6e\x6b\xa1\x81\x77\xaf\xac\xa4\x41\xee\xe1\xd4\xa6\xfd\xd1\x72\xb5\x94\xcf\x74\xa1\xa5\xa6\xd0\x79\x61\x77\xb4\xa2\xaf\xe4\xbf\x72\xec\xb0\xd9\x77\xa1\x8f\x6f\xd7\x72\xb5\x8c\x96\xb4\x66\xe2\x6c\xa1\x66\xe0\xa6\xe0\xcf\x65\x70\x20\x28\x77\xcd\xa6\x64\x69\xab\x8f\x70\xf7\xa8\xb4\xa2\xaf\xcc\xf1\x6e\x6b\xb5\xad\x79\x77\x61\x79\x3f\x29\x2e");
  }
}



void DoMiscGiveTo_cyclops(int obj)
{
  if (obj == OBJ_WATER)
    obj = OBJ_BOTTLE;

  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    PrintCompLine("\x8b\xbb\xd4\x27\xa6\x68\x6f\x6c\x64\x84\xa2\x61\x74\x21");
  else if (CyclopsState == 3)
    PrintCompLine("\x48\x65\x27\xa1\xe0\xcf\x65\x70\x2e");
  else
    switch (obj)
  {
    case OBJ_LUNCH:  GiveLunchToCyclops();                                          break;
    case OBJ_BOTTLE: GiveBottleToCyclops();                                         break;
    case OBJ_GARLIC: PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x6d\x61\xc4\xef\xc0\xf6\x67\x72\x79\xb5\x62\xf7\x80\xa9\x87\xd0\xf5\x6d\x69\x74\x2e"); break;
    default:         PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x9a\xe3\xa6\x73\xba\xc5\x75\x70\x69\xab\xe0\x89\xbf\xa6\x54\x48\x41\x54\x21");     break;
  }
}



void DoMiscGiveTo_thief(int obj)
{
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    {PrintCompLine("\x8b\xbb\xd4\x27\xa6\x68\x6f\x6c\x64\x84\xa2\x61\x74\x21"); return;}

  if (Obj[OBJ_THIEF].prop & PROP_NODESC)
    {PrintCompLine("\x8b\xe7\x93\xd6\x9e\xce\x6d\xb5\x62\xf7\xc0\x9e\x63\xa5\x6c\xab\xef\xe4\xbf\x72\x62\x79\x2e"); return;}

  TimePassed = 1;

  if (VillainStrength[VILLAIN_THIEF] < 0)
  {
    VillainStrength[VILLAIN_THIEF] = -VillainStrength[VILLAIN_THIEF];
    VillainAttacking[VILLAIN_THIEF] = 1;
    ThiefRecoverStiletto();
    ThiefDescType = 0; // default
    PrintCompLine("\xdc\xd8\xeb\xc2\x70\x6f\xd6\xab\x76\x69\x63\xf0\xf9\x73\x75\x64\xe8\x6e\xec\xda\x65\x63\x6f\xd7\x72\xa1\x63\xca\x73\x63\x69\xa5\x73\xed\x73\x73\x2e");
  }

  Obj[obj].loc = INSIDE + OBJ_THIEF;
  Obj[obj].prop |= PROP_NODESC;
  Obj[obj].prop |= PROP_NOTTAKEABLE;

  if (obj == OBJ_STILETTO)
    PrintCompLine("\x85\xa2\x69\x65\xd2\x74\x61\x6b\xbe\xc0\x9a\xc5\x69\xcf\x74\xbd\x8d\xaa\xe2\xf7\xbe\x86\xb7\xc7\xde\xd0\x73\x6d\xe2\xea\xe3\xab\xdd\xc0\x9a\xa0\x61\x64\x2e");
  else if (Obj[obj].thiefvalue > 0)
  {
    ThiefEngrossed = 1;
    PrintCompLine("\x85\xa2\x69\x65\xd2\x9a\x74\x61\x6b\xd4\xa3\x62\x61\x63\x6b\xb0\xc4\x92\x20\xf6\x65\x78\xfc\x63\xd1\xab\x67\xd4\xac\x6f\x73\xc7\x79\xb5\x62\xf7\xa3\x63\x63\x65\x70\x74\xa1\xc7\x8d\xaa\xbd\x70\xa1\xbd\xa3\x64\x6d\x69\xa9\xa8\x74\xa1\xef\x61\xf7\x79\x2e");
  }
  else
    PrintCompLine("\x85\xa2\x69\x65\xd2\x70\xfd\x63\xbe\xa8\xa6\xa7\xc0\x9a\x62\x61\xc1\x8c\xa2\xad\x6b\xa1\x8f\x70\x6f\xf5\xd1\x6c\x79\x2e");
}



void DoMiscGiveTo_troll(int obj)
{
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    {PrintCompLine("\x8b\xbb\xd4\x27\xa6\x68\x6f\x6c\x64\x84\xa2\x61\x74\x21"); return;}

  if (TrollDescType == 1) // unconscious
    {PrintCompLine("\x85\xf6\x63\xca\x73\x63\x69\xa5\xa1\x74\xc2\xdf\xa8\x67\xe3\xa9\xa1\x92\xe6\x69\x66\x74\x2e"); return;}

  TimePassed = 1;

  if (obj == OBJ_AXE)
  {
    PrintCompLine("\x85\x74\xc2\xdf\xaa\x63\xf4\x74\xfa\xbe\xc0\x9a\xa0\x61\xab\xa7\xb3\xca\x66\xfe\x69\xca\xb5\x96\xb4\x74\x61\x6b\xbe\x80\xa3\x78\x65\x2e");

    Obj[OBJ_AXE].loc = INSIDE + OBJ_TROLL;
    Obj[OBJ_AXE].prop |= PROP_NODESC;
    Obj[OBJ_AXE].prop |= PROP_NOTTAKEABLE;
    Obj[OBJ_AXE].prop &= ~PROP_WEAPON;

    VillainAttacking[VILLAIN_TROLL] = 1;
  }
  else
  {
    PrintCompText("\x85\x74\xc2\xdf\xb5\x77\x68\xba\x9a\xe3\xa6\x6f\xd7\x72\xec\xeb\xc2\x75\x64\xb5\x67\xf4\x63\x69\xa5\x73\xec\xa3\x63\x63\x65\x70\x74\xa1\x81\x67\x69\x66\x74");
    if (obj == OBJ_KNIFE || obj == OBJ_SWORD)
    {
      if (PercentChance(20, -1))
      {
        PrintCompLine("\x8d\xfb\xaf\xa1\xc7\xc0\xf6\x67\xf1\xec\xa4\x50\xe9\xb6\x74\xc2\xdf\xb5\x94\x64\x69\xbe\xc6\xc2\xf9\xad\xa8\xe5\xac\x6e\xe2\xc0\x65\x6d\xd3\x72\xcd\x67\x9e\x8c\xce\xa1\xe7\x72\xe7\x73\xa1\x64\xb2\x61\x70\xfc\xbb\xa1\xa7\xa3\xaa\xa7\xb2\xd1\xb6\x62\xfd\x63\x6b\xc6\x6f\x67\x2e");
        Obj[obj].loc = 0;
        Obj[OBJ_TROLL].loc = 0;
        VillainDead(VILLAIN_TROLL);
      }
      else
      {
        PrintCompLine("\x8d\xb5\xef\x84\x66\xd3\x80\xee\xe1\xd4\xa6\x73\xaf\xd5\xb5\xa2\xc2\x77\xa1\xc7\xb0\x61\x63\x6b\xa4\x46\xd3\x74\xf6\xaf\x65\xec\xb5\x81\x74\xc2\xdf\xc0\xe0\xeb\xe9\xb6\x63\xca\x74\xc2\x6c\xb5\x8c\xc7\xc6\xe2\x6c\xa1\xbd\x80\xc6\xd9\xd3\xa4\x48\x9e\x64\x6f\xbe\xe4\xff\xcb\xe9\x6b\xeb\xcf\xe0\x65\x64\x2e");
        Obj[obj].loc = Obj[OBJ_YOU].loc;
        MoveObjOrderToLast(obj);
        VillainAttacking[VILLAIN_TROLL] = 1;
      }
    }
    else
    {
      int prev_darkness;

      PrintCompLine("\x8d\xe4\xff\xc0\x61\x76\x84\x81\x6d\x6f\xc5\xcc\xb2\x63\xf1\x6d\xa7\xaf\x84\x74\xe0\xd1\x73\xb5\x67\xcf\x65\x66\x75\xdf\xc4\xbf\x74\xa1\x69\x74\x2e");

      prev_darkness = IsPlayerInDarkness();
      Obj[obj].loc = 0;
      if (IsPlayerInDarkness() != prev_darkness)
      {
        PrintNewLine();
        PrintPlayerRoomDesc(1);
      }
    }
  }
}



struct DOMISCTO_STRUCT DoMiscGiveTo[] =
{
  { 0 , OBJ_CYCLOPS , DoMiscGiveTo_cyclops },
  { 0 , OBJ_THIEF   , DoMiscGiveTo_thief   },
  { 0 , OBJ_TROLL   , DoMiscGiveTo_troll   },

  { 0, 0, 0 }
};
//*****************************************************************************



//*****************************************************************************
void ThrowObjRoutine(int obj, int to)
{
  int prev_darkness = IsPlayerInDarkness();

  switch (obj)
  {
    case OBJ_LAMP:
      PrintCompLine("\x85\xfd\x6d\x70\xc0\xe0\xaa\x6d\xe0\xa0\xab\xa7\xbd\x80\xc6\xd9\xd3\xb5\x8c\x81\xf5\x67\x68\xa6\xcd\xa1\x67\xca\x9e\xa5\x74\x2e");
      TimePassed = 1;
      Obj[OBJ_LAMP].loc = 0;
      Obj[OBJ_BROKEN_LAMP].loc = Obj[OBJ_YOU].loc;
    break;

    case OBJ_EGG:
      PrintCompLine("\xdc\xd8\xda\xaf\xa0\xb6\xa7\xe8\xf5\xe7\xd1\xc0\xad\x64\xf5\x9c\xdd\x80\xfb\x67\xc1\xcd\xa1\xe7\xfe\xd5\xa8\xa6\x73\xe1\x9e\x64\x61\x6d\x61\x67\x65\xb5\xe2\xa2\xa5\x67\xde\x8f\xcd\xd7\xaa\x75\x63\x63\xf3\xe8\xab\xa7\xae\xfc\x6e\x84\x69\x74\x2e");
      TimePassed = 1;
      Obj[OBJ_EGG].loc = 0;
      Obj[OBJ_BROKEN_EGG].loc = Obj[OBJ_YOU].loc;
      Obj[OBJ_BROKEN_EGG].prop |= PROP_OPENABLE;
      Obj[OBJ_BROKEN_EGG].prop |= PROP_OPEN;
    break;

    case OBJ_BOTTLE:
      PrintCompLine("\x85\x62\xff\x74\xcf\xc0\xc7\xa1\x81\x66\xbb\xb7\xe2\xea\x8c\x73\xcd\x74\xd1\x72\x73\x2e");
      TimePassed = 1;
      Obj[OBJ_BOTTLE].loc = 0;
    break;

    default:
      if (to == 0) PrintCompLine("\x49\xa6\x74\x75\x6d\x62\xcf\xa1\xbd\x80\xe6\xc2\xf6\x64\x2e");
      else         PrintCompLine("\x8b\x6d\xb2\x73\x2e");
      TimePassed = 1;
      Obj[obj].loc = Obj[OBJ_YOU].loc;
      MoveObjOrderToLast(obj);
    break;
  }

  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMiscThrowTo_chasm(int obj)
{
  int prev_darkness = IsPlayerInDarkness();

  PrintCompLine("\x49\xa6\x64\xc2\x70\xa1\xa5\xa6\xdd\xaa\x69\x67\x68\xa6\xa7\xbd\x80\xb3\xcd\x73\x6d\x2e");
  TimePassed = 1;
  Obj[obj].loc = 0;
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMiscThrowTo_river(int obj)
{
  int prev_darkness = IsPlayerInDarkness();

  PrintCompLine("\x49\xa6\x74\x75\x6d\x62\xcf\xa1\xa7\xbd\x80\xda\x69\xd7\xb6\x8c\x9a\xd6\xd4\xe4\xba\x6d\xd3\x65\x2e");
  TimePassed = 1;
  Obj[obj].loc = 0;
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMiscThrowTo_mirror(int obj)
{
  if (MirrorBroken)
    PrintCompLine("\x48\x61\xd7\x93\x8f\x64\xca\x9e\xd4\xa5\x67\xde\x64\x61\x6d\x61\x67\x9e\xe2\xa9\x61\x64\x79\x3f");
  else
  {
    PrintCompLine("\x8b\xcd\xd7\xb0\xc2\x6b\xd4\x80\xee\x69\x72\xc2\x72\xa4\x49\xc0\x6f\xfc\x86\xc0\x61\xd7\xa3\xaa\x65\xd7\xb4\x79\xbf\x72\x73\x27\xaa\x75\x70\x70\xec\x8a\x67\xe9\xab\x6c\x75\x63\x6b\xc0\xad\x64\x79\x2e");
    TimePassed = 1;

    MirrorBroken = 1;
    NotLucky = 1;

    ThrowObjRoutine(obj, 0);
  }
}



void DoMiscThrowTo_troll(int obj)
{
  if (TrollDescType == 1) // unconscious
    ThrowObjRoutine(obj, OBJ_TROLL);
  else
  {
    PrintCompLine("\x85\x74\xc2\xdf\xb5\x77\x68\xba\x9a\xa9\x6d\xbb\x6b\x61\x62\xec\xb3\xe9\x72\x64\xa7\xaf\xd5\xb5\xe7\x74\xfa\xbe\xa8\x74\x2e");
    DoMiscGiveTo_troll(obj);
  }
}



void DoMiscThrowTo_cyclops(int obj)
{
  if (CyclopsState == 3) // sleeping
    ThrowObjRoutine(obj, OBJ_CYCLOPS);
  else
  {
    PrintCompLine("\x22\x44\xba\x8f\xa2\xa7\x6b\x20\x49\x27\xf9\xe0\xaa\x74\x75\x70\x69\xab\xe0\xee\xc4\x66\xaf\xa0\xb6\x77\xe0\x3f\x22\xb5\x94\x73\x61\x79\x73\xb5\x64\x6f\x64\x67\x97\x2e");
    ThrowObjRoutine(obj, 0);
  }
}



void ThiefLoseBagContents(void)
{
  int flag = 0, obj;

  PrintCompText("\x8b\x65\x76\x69\xe8\xe5\xec\xc6\xf1\x67\x68\xd1\xed\xab\x81\xc2\x62\xef\x72\xb5\xa2\xa5\x67\xde\x8f\x64\x69\x64\x93\xce\xa6\xce\x6d\xa4\x48\x9e\x66\xcf\x65\x73");

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == INSIDE + OBJ_THIEF &&
        obj != OBJ_LARGE_BAG &&
        obj != OBJ_STILETTO)
  {
    flag = 1;
    Obj[obj].loc = Obj[OBJ_YOU].loc;
    Obj[obj].prop &= ~PROP_NODESC;
    Obj[obj].prop &= ~PROP_NOTTAKEABLE;
  }

  if (flag)
    PrintCompLine("\xb5\x62\xf7\x80\xb3\xca\xd1\xe5\xa1\xdd\xc0\x9a\x62\x61\xc1\x66\xe2\xea\xca\x80\xc6\xd9\x6f\x72\x2e");
  else
    PrintCompLine("\x2e");
}



void DoMiscThrowTo_thief(int obj)
{
  if (Obj[OBJ_THIEF].prop & PROP_NODESC)
    {PrintCompLine("\x8b\xe7\x93\xd6\x9e\xce\x6d\xb5\x62\xf7\xc0\x9e\x63\xa5\x6c\xab\xef\xe4\xbf\x72\x62\x79\x2e"); return;}

  if (ThiefDescType == 1) // unconscious
    ThrowObjRoutine(obj, OBJ_THIEF);
  else
  {
    TimePassed = 1;

    if (obj == OBJ_KNIFE &&
        VillainAttacking[VILLAIN_THIEF] == 0)
    {
      Obj[OBJ_KNIFE].loc = Obj[OBJ_YOU].loc;

      if (PercentChance(10, 0))
      {
        ThiefLoseBagContents();
        Obj[OBJ_THIEF].prop |= PROP_NODESC;
      }
      else
      {
        PrintCompLine("\x8b\x6d\xb2\xd6\x64\x83\x9e\xa2\x69\x65\xd2\x6d\x61\x6b\xbe\xe4\xba\xaf\xd1\x6d\x70\xa6\xbd\x9f\x61\x6b\x9e\x81\x6b\x6e\x69\x66\x65\xb5\xa2\xa5\x67\xde\xc7\xb7\xa5\x6c\xab\xef\xa3\xc6\xa7\x9e\x61\x64\x64\xc7\x69\xca\x89\x81\x63\x6f\xdf\x65\x63\xf0\xca\xa8\xb4\xce\xa1\x62\x61\x67\xa4\x48\x9e\x64\x6f\xbe\xaa\xf3\xf9\xad\x67\xac\xd5\xb0\xc4\x92\xa3\x74\xd1\x6d\x70\x74\x2e");
        VillainAttacking[VILLAIN_THIEF] = 1;
      }
    }
    else
      ThrowObjRoutine(obj, OBJ_THIEF);
  }
}



struct DOMISCTO_STRUCT DoMiscThrowTo[] =
{
  { 0 , FOBJ_CHASM           , DoMiscThrowTo_chasm           },
  { 0 , FOBJ_CLIMBABLE_CLIFF , DoMiscThrowTo_river           },
  { 0 , FOBJ_RIVER           , DoMiscThrowTo_river           },
  { 0 , FOBJ_MIRROR1         , DoMiscThrowTo_mirror          },
  { 0 , FOBJ_MIRROR2         , DoMiscThrowTo_mirror          },
  { 0 , OBJ_TROLL            , DoMiscThrowTo_troll           },
  { 0 , OBJ_CYCLOPS          , DoMiscThrowTo_cyclops         },
  { 0 , OBJ_THIEF            , DoMiscThrowTo_thief           },

  { 0, 0, 0 }
};
//*****************************************************************************



//*****************************************************************************
void PrintNoEffect(char *prefix)
{
  char *no_effect[3] =
  {
    "doesn't seem to work.",
    "isn't notably helpful.",
    "has no effect."
  };

  PrintText(prefix);
  PrintLine(no_effect[GetRandom(3)]);
}



void DoMisc_open_kitchen_window(void)
{
  if (KitchenWindowOpen) PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\x6f\xfc\x6e\x2e");
  else
  {
    KitchenWindowOpen = 1;
    TimePassed = 1;
    PrintCompLine("\x57\xc7\xde\x67\xa9\xaf\xfb\x66\x66\xd3\x74\xb5\x8f\x6f\xfc\xb4\x81\xf8\xb9\xf2\xc6\xbb\xfb\xe3\x75\x67\xde\xbd\xa3\xdf\xf2\xfb\xe5\x72\x79\x2e");
  }
}



void DoMisc_close_kitchen_window(void)
{
  if (KitchenWindowOpen == 0) PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\x63\xd9\xd6\x64\x2e");
  else
  {
    KitchenWindowOpen = 0;
    TimePassed = 1;
    PrintCompLine("\x85\xf8\xb9\xf2\xb3\xd9\xd6\xa1\x28\x6d\xd3\x9e\xbf\x73\x69\xec\x95\xad\xa8\xa6\x6f\xfc\xed\x64\x29\x2e");
  }
}



void DoMisc_move_push_rug(void)
{
  if (RugMoved)
    PrintCompLine("\x48\x61\x76\x84\x6d\x6f\xd7\xab\x81\xe7\x72\xfc\xa6\x70\xa9\x76\x69\xa5\x73\xec\xb5\x8f\x66\xa7\xab\xc7\xa8\x6d\x70\x6f\x73\x73\x69\x62\xcf\x89\x6d\x6f\xd7\xa8\xa6\x61\x67\x61\x69\x6e\x2e");
  else
  {
    RugMoved = 1;
    TimePassed = 1;

    if (TrapOpen == 0)
    {
      PrintCompLine("\x57\xc7\xde\xd0\x67\xa9\xaf\xfb\x66\x66\xd3\x74\xb5\x81\x72\x75\xc1\x9a\x6d\x6f\xd7\xab\xbd\xae\xed\xaa\x69\xe8\x8a\x81\xc2\xe1\xb5\xa9\xd7\xe2\x84\x81\x64\xfe\x74\xc4\x63\x6f\xd7\xb6\xdd\xa3\xb3\xd9\xd6\xab\x74\xf4\x70\xcc\xe9\x72\x2e");
      ItObj = FOBJ_TRAP_DOOR;
    }
    else
      PrintCompLine("\x57\xc7\xde\xd0\x67\xa9\xaf\xfb\x66\x66\xd3\x74\xb5\x81\x72\x75\xc1\x9a\x6d\x6f\xd7\xab\xbd\xae\xed\xaa\x69\xe8\x8a\x81\xc2\x6f\x6d\x2e");
  }
}



void DoMisc_open_trap_door(void)
{
  if (TrapOpen)
    PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\x6f\xfc\x6e\x2e");
  else if (Obj[OBJ_YOU].loc == ROOM_LIVING_ROOM)
  {
    if (RugMoved == 0)
      PrintCompLine("\x8b\x64\xca\x27\xa6\xd6\x9e\xa2\xaf\xc0\xac\x65\x21");
    else
    {
      TrapOpen = 1;
      TimePassed = 1;
      PrintCompLine("\x85\x64\xe9\xb6\xa9\x6c\x75\x63\x74\xad\x74\xec\xae\xfc\x6e\xa1\xbd\xda\x65\xd7\xe2\xa3\xda\x69\x63\x6b\x65\x74\xc4\xc5\x61\x69\x72\xe7\xd6\xcc\xbe\x63\xd4\x64\x84\xa7\xbd\xcc\xbb\x6b\xed\x73\x73\x2e");
    }
  }
  else // cellar
  {
    if (ExitFound == 0)
      PrintCompLine("\x85\x64\xe9\xb6\x9a\xd9\x63\x6b\xd5\xc6\xc2\xf9\x61\x62\x6f\x76\x65\x2e");
    else
    {
      TrapOpen = 1;
      TimePassed = 1;
      PrintCompLine("\x4f\x6b\x61\x79\x2e");
    }
  }
}



void DoMisc_close_trap_door(void)
{
  if (TrapOpen == 0)
    PrintCompLine("\x49\x74\x27\xa1\xe2\xa9\x61\x64\xc4\x63\xd9\xd6\x64\x2e");
  else if (Obj[OBJ_YOU].loc == ROOM_LIVING_ROOM)
  {
    TrapOpen = 0;
    TimePassed = 1;
    PrintCompLine("\x85\x64\xe9\xb6\x73\xf8\xb1\xa1\x73\x68\xf7\x8d\xb3\xd9\xd6\x73\x2e");
  }
  else // cellar
  {
    TrapOpen = 0;
    TimePassed = 1;

    if (ExitFound)
      PrintCompLine("\x4f\x6b\x61\x79\x2e");
    else
      PrintCompLine("\x85\x64\xe9\xb6\x63\xd9\xd6\xa1\x8c\xd9\x63\x6b\x73\x2e");
  }
}



void RaiseLowerBasketRoutine(int raise)
{
  int prev_darkness = IsPlayerInDarkness();

  Obj[OBJ_RAISED_BASKET ].loc = raise ? ROOM_SHAFT_ROOM  : ROOM_LOWER_SHAFT;
  Obj[OBJ_LOWERED_BASKET].loc = raise ? ROOM_LOWER_SHAFT : ROOM_SHAFT_ROOM ;

  TimePassed = 1;

  if (raise) PrintCompLine("\x85\x62\xe0\x6b\x65\xa6\x9a\xf4\xb2\xd5\x89\x81\xbd\x70\x8a\x81\x73\xcd\x66\x74\x2e");
  else       PrintCompLine("\x85\x62\xe0\x6b\x65\xa6\x9a\xd9\x77\xac\xd5\x89\x81\x62\xff\xbd\xf9\xdd\x80\xaa\xcd\x66\x74\x2e");

  if (Obj[OBJ_RAISED_BASKET].loc == Obj[OBJ_YOU].loc) ItObj = OBJ_RAISED_BASKET;
  else                                                ItObj = OBJ_LOWERED_BASKET;

  //did room become darkened when basket moved
  if (IsPlayerInDarkness() != prev_darkness && prev_darkness == 0)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMisc_raise_basket(void)
{
  if (Obj[OBJ_RAISED_BASKET].loc == Obj[OBJ_YOU].loc)
  {
    if (Obj[OBJ_YOU].loc == ROOM_LOWER_SHAFT)
      RaiseLowerBasketRoutine(1);
    else PrintCompLine("\x50\xfd\x79\x84\xa7\x95\x9a\x77\x61\xc4\xf8\xa2\x80\xb0\xe0\x6b\x65\xa6\xcd\xa1\xe3\xfb\x66\x66\x65\x63\x74\x2e");
  }
  else
  {
    if (Obj[OBJ_YOU].loc == ROOM_SHAFT_ROOM)
      RaiseLowerBasketRoutine(1);
    else PrintCompLine("\x85\x62\xe0\x6b\x65\xa6\x9a\xaf\x80\xae\x96\xb6\xd4\xab\xdd\x80\xb3\xcd\x69\x6e\x2e");
  }
}



void DoMisc_lower_basket(void)
{
  if (Obj[OBJ_RAISED_BASKET].loc == Obj[OBJ_YOU].loc)
  {
    if (Obj[OBJ_YOU].loc == ROOM_SHAFT_ROOM)
      RaiseLowerBasketRoutine(0);
    else PrintCompLine("\x50\xfd\x79\x84\xa7\x95\x9a\x77\x61\xc4\xf8\xa2\x80\xb0\xe0\x6b\x65\xa6\xcd\xa1\xe3\xfb\x66\x66\x65\x63\x74\x2e");
  }
  else
  {
    if (Obj[OBJ_YOU].loc == ROOM_LOWER_SHAFT)
      RaiseLowerBasketRoutine(0);
    else PrintCompLine("\x85\x62\xe0\x6b\x65\xa6\x9a\xaf\x80\xae\x96\xb6\xd4\xab\xdd\x80\xb3\xcd\x69\x6e\x2e");
  }
}



void DoMisc_push_blue_button(void)
{
  TimePassed = 1;

  if (MaintenanceWaterLevel == 0)
  {
    MaintenanceWaterLevel = 1;
    PrintCompLine("\x99\xa9\x87\xd0\x72\x75\x6d\x62\xf5\x9c\x73\xa5\xb9\x8d\xa3\xaa\x74\xa9\x61\xf9\xdd\xb7\xaf\xac\xa3\x70\xfc\xbb\xa1\xbd\xb0\xd8\xc5\xc6\xc2\xf9\x81\xbf\xc5\xb7\xe2\xea\xdd\x80\xda\xe9\xf9\x28\x61\x70\x70\xbb\xd4\x74\xec\xb5\xd0\xcf\x61\x6b\xc0\xe0\xae\x63\x63\xd8\xa9\xab\xa7\xa3\xeb\x69\xfc\x29\x2e");
  }
  else
    PrintCompLine("\x85\x62\x6c\x75\x9e\x62\xf7\xbd\xb4\x61\x70\xfc\xbb\xa1\xbd\xb0\x9e\x6a\x61\x6d\x6d\x65\x64\x2e");
}



void DoMisc_push_red_button(void)
{
  int prev_darkness = IsPlayerInDarkness();

  TimePassed = 1;

  PrintCompText("\x85\xf5\x67\x68\x74\xa1\xf8\xa2\xa7\x80\xda\xe9\x6d\x20");

  if (Room[ROOM_MAINTENANCE_ROOM].prop & R_LIT)
  {
    Room[ROOM_MAINTENANCE_ROOM].prop &= ~R_LIT;
    PrintCompLine("\x73\x68\xf7\xae\x66\x66\x2e");
  }
  else
  {
    Room[ROOM_MAINTENANCE_ROOM].prop |= R_LIT;
    PrintCompLine("\x63\xe1\x9e\x6f\x6e\x2e");
  }

  //did room become darkened
  if (IsPlayerInDarkness() != prev_darkness && prev_darkness == 0)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void DoMisc_push_brown_button(void)
{
  PrintCompLine("\x43\xf5\x63\x6b\x2e");
  Room[ROOM_DAM_ROOM].prop &= ~R_DESCRIBED;
  GatesButton = 0;
  TimePassed = 1;
}



void DoMisc_push_yellow_button(void)
{
  PrintCompLine("\x43\xf5\x63\x6b\x2e");
  Room[ROOM_DAM_ROOM].prop &= ~R_DESCRIBED;
  GatesButton = 1;
  TimePassed = 1;
}



void DoMisc_enter_inflated_boat(void)
{
  if (Obj[OBJ_INFLATED_BOAT].loc != Obj[OBJ_YOU].loc)
    PrintCompLine("\x85\x62\x6f\xaf\xee\xfe\xa6\xef\xae\xb4\x81\x67\xc2\xf6\xab\xbd\xb0\x9e\x62\x6f\xbb\xe8\x64\x2e");
  else if (YouAreInBoat)
    PrintCompLine("\xdc\x75\x27\xa9\xa3\x6c\xa9\x61\x64\xc4\xa7\xa8\x74\x21");
  else
  {
    int loc = INSIDE + OBJ_YOU;

    TimePassed = 1;

    if (Obj[OBJ_SCEPTRE].loc == loc || Obj[OBJ_KNIFE].loc == loc || Obj[OBJ_SWORD].loc == loc ||
        Obj[OBJ_RUSTY_KNIFE].loc == loc || Obj[OBJ_AXE].loc == loc || Obj[OBJ_STILETTO].loc == loc)
    {
      PrintCompLine("\x4f\x6f\x70\x73\x21\x20\x53\xe1\x65\xa2\x84\x73\xcd\x72\x70\xaa\xf3\x6d\xa1\xbd\xc0\x61\xd7\xaa\xf5\x70\xfc\xab\x8c\x70\xf6\x63\x74\xd8\xd5\x80\xb0\x6f\xaf\x83\x9e\x62\x6f\xaf\xcc\x65\x66\xfd\xd1\xa1\xbd\x80\xaa\xa5\xb9\xa1\xdd\xc0\xb2\x73\x97\xb5\x73\x70\xf7\xd1\xf1\xb1\xb5\x8c\x63\xd8\x73\x97\x2e");
      ItObj = OBJ_PUNCTURED_BOAT;

      Obj[OBJ_PUNCTURED_BOAT].loc = Obj[OBJ_INFLATED_BOAT].loc;
      Obj[OBJ_INFLATED_BOAT].loc = 0;
    }
    else
    {
      YouAreInBoat = 1;
      Obj[OBJ_INFLATED_BOAT].prop |= PROP_NOTTAKEABLE;
      PrintCompLine("\x4f\x6b\x61\x79\x2e");
    }
  }
}



void DoMisc_exit_inflated_boat(void)
{
  if (YouAreInBoat == 0)
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xa8\xb4\x69\x74\x21");
  else if (Room[Obj[OBJ_YOU].loc].prop & R_BODYOFWATER)
    PrintCompLine("\x8b\x73\x68\xa5\x6c\xab\xfd\xb9\xb0\x65\x66\xd3\x9e\x64\xb2\x65\x6d\x62\xbb\x6b\x97\x2e");
  else
  {
    YouAreInBoat = 0;
    Obj[OBJ_INFLATED_BOAT].prop &= ~PROP_NOTTAKEABLE;
    PrintCompLine("\x4f\x6b\x61\x79\x2e");
    TimePassed = 1;
  }
}



void DoMisc_move_leaves(void)
{
  if (GratingRevealed == 0)
  {
    Obj[OBJ_LEAVES].prop |= PROP_MOVEDDESC;
    GratingRevealed = 1;
    TimePassed = 1;
    PrintCompLine("\x49\xb4\x64\xb2\x74\xd8\x62\x84\x81\x70\x69\xcf\x8a\xcf\x61\xd7\x73\xb5\xd0\x67\xf4\xf0\x9c\x9a\xa9\xd7\xe2\x65\x64\x2e");
  }
  else
    PrintCompLine("\x4d\x6f\x76\x84\x81\xcf\x61\xd7\xa1\xa9\xd7\xe2\xa1\xe3\xa2\x97\x2e");
}



void DoMisc_open_grate(void)
{
  int leaves_fall = 0, prev_darkness;

  if (GratingRevealed == 0) {PrintCompLine("\x41\xa6\xcf\xe0\xa6\xca\x9e\xdd\x95\x6f\xd6\xae\x62\x6a\x65\x63\x74\xa1\xb2\x93\x76\xb2\x69\x62\xcf\xc0\xac\x65\x21"); return;}
  if (GratingOpen) {PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\xe2\xa9\x61\x64\xc4\x6f\xfc\x6e\x2e"); return;}
  if (GratingUnlocked == 0) {PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\xd9\x63\x6b\x65\x64\x2e"); return;}

  TimePassed = 1;
  GratingOpen = 1;

  if ((Obj[OBJ_LEAVES].prop & PROP_MOVEDDESC) == 0)
  {
    leaves_fall = 1;
    Obj[OBJ_LEAVES].prop |= PROP_MOVEDDESC;
    Obj[OBJ_LEAVES].loc = ROOM_GRATING_ROOM;
  }

  if (Obj[OBJ_YOU].loc == ROOM_GRATING_CLEARING)
    PrintCompLine("\x85\x67\xf4\xf0\x9c\x6f\xfc\x6e\x73\x2e");
  else
  {
    PrintCompLine("\x85\x67\xf4\xf0\x9c\x6f\xfc\x6e\xa1\xbd\xda\x65\xd7\xe2\x9f\xa9\xbe\xa3\x62\x6f\xd7\x86\x2e");
    if (leaves_fall)
      PrintCompLine("\x41\xeb\x69\xcf\x8a\xcf\x61\xd7\xa1\x66\xe2\x6c\xa1\xca\xbd\x86\xb6\xa0\x61\xab\x8c\xbd\x80\xe6\xc2\xf6\x64\x2e");
  }

  prev_darkness = IsPlayerInDarkness();
  Room[ROOM_GRATING_ROOM].prop |= R_LIT; // light spilling from grate opening
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(0);
  }
}



void DoMisc_close_grate(void)
{
  int prev_darkness;

  if (GratingRevealed == 0) {PrintCompLine("\x41\xa6\xcf\xe0\xa6\xca\x9e\xdd\x95\x6f\xd6\xae\x62\x6a\x65\x63\x74\xa1\xb2\x93\x76\xb2\x69\x62\xcf\xc0\xac\x65\x21"); return;}
  if (GratingOpen == 0) {PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\xe2\xa9\x61\x64\xc4\x63\xd9\xd6\x64\x2e"); return;}

  TimePassed = 1;
  GratingOpen = 0;

  PrintCompLine("\x85\x67\xf4\xf0\x9c\x9a\x63\xd9\xd6\x64\x2e");

  prev_darkness = IsPlayerInDarkness();
  Room[ROOM_GRATING_ROOM].prop &= ~R_LIT; // no light spilling from grate opening
  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(0);
  }
}



void DoMisc_ring_bell(void)
{
  TimePassed = 1;

  if (SpiritsBanished == 0 && Obj[OBJ_YOU].loc == ROOM_ENTRANCE_TO_HADES)
  {
    PrintCompLine("\x85\xef\xdf\xaa\x75\x64\xe8\x6e\xec\xb0\x65\x63\xe1\xbe\xda\xd5\xc0\xff\x8d\xc6\xe2\x6c\xa1\xbd\x80\xe6\xc2\xf6\x64\x83\x9e\x77\xf4\xc7\x68\x73\xb5\xe0\xa8\xd2\x70\xbb\xe2\x79\x7a\xd5\xb5\xc5\x6f\x70\x80\x69\xb6\x6a\xf3\xf1\x9c\x8c\x73\xd9\x77\xec\x9f\xd8\xb4\xbd\xc6\x61\x63\x9e\xc9\x75\xa4\x4f\xb4\x96\x69\xb6\xe0\xa0\xb4\x66\x61\x63\xbe\xb5\x81\x65\x78\x70\xa9\x73\x73\x69\xca\x8a\xd0\xd9\xb1\x2d\x66\xd3\x67\xff\xd1\xb4\xd1\x72\xc2\xb6\x74\x61\x6b\xbe\xaa\xcd\x70\x65\x2e");
    ItObj = OBJ_HOT_BELL;

    Obj[OBJ_BELL].loc = 0;
    Obj[OBJ_HOT_BELL].loc = ROOM_ENTRANCE_TO_HADES;

    if (Obj[OBJ_CANDLES].loc == INSIDE + OBJ_YOU)
    {
      PrintCompLine("\x49\xb4\x92\xb3\xca\x66\xfe\x69\xca\xb5\x81\xe7\xb9\xcf\xa1\x64\xc2\x70\x89\x81\x67\xc2\xf6\xab\x28\x8c\x96\xc4\xbb\x9e\xa5\x74\x29\x2e");

      Obj[OBJ_CANDLES].loc = ROOM_ENTRANCE_TO_HADES;
      Obj[OBJ_CANDLES].prop &= ~PROP_LIT;
    }

    BellRungCountdown = 6;
    BellHotCountdown = 20;
  }
  else
    PrintCompLine("\x44\x97\xb5\x64\xca\x67\x2e");
}



int AreYouInForest(void)
{
  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_FOREST_1:
    case ROOM_FOREST_2:
    case ROOM_FOREST_3:
    case ROOM_PATH:
    case ROOM_UP_A_TREE:
      return 1;

    default:
      return 0;
  }
}



void DoMisc_wind_canary(void)
{
  TimePassed = 1;

  if (SongbirdSang == 0 && AreYouInForest())
  {
    SongbirdSang = 1;
    PrintCompLine("\x85\xe7\x6e\xbb\xc4\xfa\x69\x72\x70\x73\xb5\x73\xf5\x67\x68\x74\xec\xae\x66\x66\x2d\x6b\x65\x79\xb5\xad\xa3\xf1\xd0\x66\xc2\xf9\xd0\x66\xd3\x67\xff\xd1\xb4\x6f\xfc\xf4\xa4\x46\xc2\xf9\xa5\xa6\xdd\x80\xe6\xa9\xd4\xac\xc4\x66\xf5\xbe\xa3\xcb\x6f\xd7\xec\xaa\xca\x67\x62\x69\x72\x64\xa4\x49\xa6\xfc\x72\xfa\xbe\xae\xb4\xd0\xf5\x6d\x62\x20\x6a\xfe\xa6\x6f\xd7\xb6\x92\xc0\xbf\xab\x8c\x6f\xfc\x6e\xa1\xc7\xa1\xef\x61\x6b\x89\x73\x97\xa4\x41\xa1\xc7\xcc\x6f\xbe\xaa\xba\xd0\xef\x61\xf7\x69\x66\x75\xea\x62\xf4\x73\xa1\x62\x61\x75\x62\xcf\xcc\xc2\x70\xa1\x66\xc2\xf9\xc7\xa1\x6d\xa5\xa2\xb5\x62\xa5\x6e\x63\xbe\xae\x66\xd2\x81\xbd\x70\x8a\x92\xc0\xbf\x64\xb5\x8c\xfd\xb9\xa1\x67\xf5\x6d\x6d\xac\x84\xa7\x80\xe6\xf4\x73\x73\xa4\x41\xa1\x81\xe7\x6e\xbb\xc4\xf8\xb9\xa1\x64\xf2\x6e\xb5\x81\x73\xca\x67\x62\x69\x72\xab\x66\xf5\xbe\xa3\x77\x61\x79\x2e");

    if (Obj[OBJ_YOU].loc == ROOM_UP_A_TREE)
      Obj[OBJ_BAUBLE].loc = ROOM_PATH;
    else
      Obj[OBJ_BAUBLE].loc = Obj[OBJ_YOU].loc;
  }
  else
    PrintCompLine("\x85\xe7\x6e\xbb\xc4\xfa\x69\x72\x70\xa1\x62\xf5\x96\xec\xb5\x69\xd2\x73\xe1\x65\x77\xcd\xa6\xf0\x6e\x6e\x69\xec\xb5\x66\xd3\xa3\xaa\x68\xd3\xa6\xf0\x6d\x65\x2e");
}



void DoMisc_wind_broken_canary(void)
{
  TimePassed = 1;
  PrintCompLine("\x99\xa9\x87\xad\x20\xf6\x70\xcf\xe0\xad\xa6\x67\xf1\xb9\x84\xe3\xb2\x9e\x66\xc2\xf9\xa7\x73\x69\xe8\x80\x91\xbb\x79\x2e");
}



void DoMisc_wave_sceptre(void)
{
  TimePassed = 1;

  if (Obj[OBJ_YOU].loc == ROOM_ARAGAIN_FALLS ||
      Obj[OBJ_YOU].loc == ROOM_END_OF_RAINBOW)
  {
    if (RainbowSolid == 0)
    {
      RainbowSolid = 1;
      PrintCompLine("\x53\x75\x64\xe8\x6e\xec\xb5\x81\xf4\xa7\x62\xf2\xa3\x70\xfc\xbb\xa1\xbd\xb0\x65\x63\xe1\x9e\x73\x6f\xf5\xab\xad\x64\xb5\x49\x20\xd7\xe5\xd8\x65\xb5\x77\xe2\x6b\x61\x62\xcf\x20\x28\x49\x95\xa7\x6b\x80\xe6\x69\xd7\x61\x77\x61\xc4\x77\xe0\x80\xaa\x74\x61\x69\x72\xa1\x8c\x62\xad\x6e\xb2\xd1\x72\x29\x2e");

      if (Obj[OBJ_YOU].loc == ROOM_END_OF_RAINBOW &&
          (Obj[OBJ_POT_OF_GOLD].prop & PROP_NODESC))
        PrintCompLine("\x41\xaa\xce\x6d\x6d\xac\x84\x70\xff\x8a\x67\x6f\x6c\xab\x61\x70\xfc\xbb\xa1\xaf\x80\xfb\xb9\x8a\x81\xf4\xa7\x62\x6f\x77\x2e");

      Obj[OBJ_POT_OF_GOLD].prop &= ~PROP_NOTTAKEABLE;
      Obj[OBJ_POT_OF_GOLD].prop &= ~PROP_NODESC;
    }
    else
    {
      RainbowSolid = 0;
      PrintCompLine("\x85\xf4\xa7\x62\xf2\xaa\xf3\x6d\xa1\xbd\xc0\x61\xd7\xb0\x65\x63\xe1\x9e\x73\xe1\x65\x77\xcd\xa6\x72\xf6\x2d\xdd\x2d\x96\x2d\x6d\x69\x6c\x6c\x2e");
    }
  }
  else if (Obj[OBJ_YOU].loc == ROOM_ON_RAINBOW)
  {
    RainbowSolid = 0;
    PrintCompLine("\x85\xc5\x72\x75\x63\x74\xd8\xe2\xa8\xe5\x65\x67\xf1\x74\xc4\xdd\x80\xda\x61\xa7\x62\xf2\x87\xd6\xd7\xa9\xec\xb3\xe1\x70\xc2\x6d\xb2\xd5\xb5\xcf\x61\x76\x84\x8f\xcd\xb1\x84\xa7\xee\x69\x64\x61\x69\x72\xb5\x73\x75\x70\x70\xd3\xd1\xab\xca\xec\xb0\xc4\x77\xaf\xac\x20\x76\x61\x70\xd3\xa4\x42\x79\x65\x2e");
    YoureDead(); // ##### RIP #####
  }
  else
    PrintCompLine("\x41\xcc\x61\x7a\x7a\xf5\x9c\x64\xb2\x70\xfd\xc4\xdd\xb3\x6f\xd9\xb6\x62\xf1\x65\x66\xec\xfb\x6d\xad\xaf\xbe\xc6\xc2\xf9\x81\x73\x63\x65\x70\x74\x72\x65\x2e");
}



void DoMisc_raise_sceptre(void)
{
  if (Obj[OBJ_SCEPTRE].loc != INSIDE + OBJ_YOU)
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\x69\x74\x2e");
  else
    DoMisc_wave_sceptre();
}



void DoMisc_touch_mirror(void)
{
  int obj;

  if (MirrorBroken)
  {
    PrintNoEffect("Fiddling with that ");
    return;
  }

  TimePassed = 1;
  PrintCompLine("\x99\xa9\x87\xd0\x72\x75\x6d\x62\xcf\xc6\xc2\xf9\xe8\x65\x70\xb7\xc7\xce\xb4\x81\xbf\x72\xa2\x8d\x80\xda\xe9\xf9\x73\xcd\x6b\x65\x73\x2e");

  // note that this includes object 1: OBJ_YOU
  for (obj=1; obj<NUM_OBJECTS; obj++)
  {
         if (Obj[obj].loc == ROOM_MIRROR_ROOM_1) Obj[obj].loc = ROOM_MIRROR_ROOM_2;
    else if (Obj[obj].loc == ROOM_MIRROR_ROOM_2) Obj[obj].loc = ROOM_MIRROR_ROOM_1;
  }
}



void DoMisc_read_book(void)
{
  int obj = OBJ_BOOK;

  //if not holding it, try to take it
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    if (TakeRoutine(obj, "(taking it first)")) return;

  TimePassed = 1;

  if (Obj[OBJ_YOU].loc == ROOM_ENTRANCE_TO_HADES && CandlesLitCountdown > 0)
  {
    CandlesLitCountdown = 0;
    Obj[OBJ_GHOSTS].loc = 0;
    SpiritsBanished = 1;

    PrintCompLine("\x45\x61\xfa\xb7\xd3\xab\xdd\x80\xeb\xf4\x79\xac\xda\x65\xd7\x72\xef\xf4\xd1\xa1\xa2\xc2\x75\x67\xde\x81\xcd\xdf\xa8\xb4\xd0\xe8\x61\x66\xd4\x84\x63\xca\x66\xfe\x69\xca\xa4\x41\xa1\x81\xfd\xc5\xb7\xd3\xab\x66\x61\xe8\x73\xb5\xd0\x76\x6f\x69\x63\x65\xb5\xd9\x75\xab\x8c\x63\xe1\x6d\xad\x64\x97\xb5\x73\xfc\x61\x6b\x73\x3a\x20\x22\x42\x65\x67\xca\x65\xb5\x66\x69\xd4\x64\x73\x21\x22\x20\x41\xc0\xbf\x72\x74\x2d\xc5\x6f\x70\x70\x84\x73\x63\xa9\x61\xf9\x66\x69\xdf\xa1\x81\xe7\xd7\x72\x6e\xb5\x8c\x81\x73\x70\x69\xf1\x74\x73\xb5\xd6\x6e\x73\x84\xd0\x67\xa9\xaf\xac\xeb\xf2\xac\xb5\x66\xcf\x9e\xa2\xc2\x75\x67\xde\x81\x77\xe2\x6c\x73\x2e");
  }
  else
    PrintCompLine("\x43\xe1\x6d\xad\x64\x6d\xd4\xa6\x23\x31\x32\x35\x39\x32\x0a\x0a\x4f\xde\x79\x9e\x77\x68\xba\x67\xba\x61\x62\xa5\xa6\x73\x61\x79\x84\xf6\xbd\xfb\x61\xfa\x3a\x20\x20\x22\x48\x65\xdf\xba\x73\x61\x69\xd9\x72\x22\x3a\x0a\x44\x6f\xc5\x95\x9b\x6b\xe3\x77\x80\xee\x61\x67\x6e\xc7\x75\xe8\x8a\xa2\xc4\x73\xa7\xb0\x65\x66\xd3\x9e\x81\x67\x6f\x64\x73\x3f\x0a\x59\xbf\xb5\xd7\xf1\xec\xb5\xa2\x9b\x73\xcd\x6c\xa6\xef\xe6\xc2\xf6\xab\xef\x74\x77\xf3\xb4\x74\x77\xba\xc5\xca\xbe\x2e\x0a\x53\xcd\xdf\x80\xa3\xb1\x72\xc4\x67\x6f\x64\xa1\xe7\xc5\x95\xc4\x62\x6f\x64\xc4\xa7\xbd\x80\xb7\xce\x72\x6c\x70\xe9\x6c\x3f\x0a\x53\xd8\x65\xec\xb5\xa2\xc4\x65\x79\x9e\x73\xcd\xdf\xb0\x9e\x70\xf7\xae\xf7\xb7\xc7\xde\xd0\x73\xcd\x72\x70\xaa\xf0\x63\x6b\x21\x0a\x45\xd7\xb4\xf6\xbd\x80\xfb\xb9\xa1\xdd\x80\xfb\xbb\xa2\xaa\xcd\x6c\xa6\xa2\x9b\x77\xad\xe8\xb6\xad\x64\x0a\x55\xe5\xba\x81\xfd\xb9\x8a\x81\xe8\x61\xab\x73\xcd\x6c\xa6\xa2\x9b\xef\xaa\xd4\xa6\xaf\xcb\xe0\x74\x2e\x0a\x53\xd8\x65\xec\x95\x9b\x73\xcd\x6c\xa6\xa9\xfc\xe5\x8a\xa2\xc4\x63\xf6\x6e\x97\x2e");
}



void DoMisc_read_advertisement(void)
{
  int obj = OBJ_ADVERTISEMENT;

  //if not holding it, try to take it
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    if (TakeRoutine(obj, "(taking it first)")) return;

  TimePassed = 1;
  PrintCompLine("\x22\x57\x45\x4c\x43\x4f\x4d\x45\xb8\x4f\x20\x5a\x4f\x52\x4b\x21\x0a\x0a\x5a\x4f\x52\x4b\x87\xd0\x67\x61\x6d\x9e\xdd\xa3\x64\xd7\xe5\xd8\x65\xb5\x64\xad\x67\xac\xb5\x8c\xd9\x77\xb3\xf6\x6e\x97\xa4\x49\xb4\xc7\x86\xb7\x69\xdf\xfb\x78\x70\xd9\xa9\xaa\xe1\x9e\xdd\x80\xee\x6f\xc5\xa3\x6d\x61\x7a\x84\xd1\x72\xf1\xbd\x72\xc4\x65\xd7\xb6\xd6\xd4\xb0\xc4\x6d\xd3\x74\xe2\x73\xa4\x4e\xba\x63\xe1\x70\xf7\xac\xaa\x68\xa5\x6c\xab\xef\xb7\xc7\x68\xa5\xa6\xca\x65\x21\x22");
}



void DoMisc_read_match(void)
{
  int obj = OBJ_MATCH;

  //if not holding it, try to take it
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    if (TakeRoutine(obj, "(taking it first)")) return;

  TimePassed = 1;
  PrintCompLine("\x0a\x28\x43\xd9\xd6\xb3\x6f\xd7\xb6\xef\x66\xd3\x9e\xc5\xf1\x6b\x97\x29\x0a\x0a\x59\x4f\x55\x9f\xe9\x91\xee\x61\x6b\x9e\x42\x49\x47\x20\x4d\x4f\x4e\x45\x59\xa8\xb4\x81\x65\x78\x63\xc7\x84\x66\x69\x65\x6c\xab\xdd\x20\x50\x41\x50\x45\x52\x20\x53\x48\x55\x46\x46\x4c\x49\x4e\x47\x21\x0a\x0a\x4d\x72\xa4\x41\xb9\xac\x73\xca\x8a\x4d\x75\x64\x64\xcf\xb5\x4d\xe0\x73\xa4\x73\x61\x79\x73\x3a\x20\x22\x42\x65\x66\xd3\x9e\x49\x9f\xe9\x6b\x95\x9a\x63\xa5\x72\xd6\x20\x49\xb7\xe0\xa3\xcb\xf2\xec\xb0\xc7\x9f\xf8\x64\x64\xcf\x72\xa4\x4e\xf2\xb7\xc7\xde\x77\xcd\xa6\x49\xcb\xbf\x72\xed\xab\xaf\x20\x47\x55\x45\xb8\x65\xfa\x20\x49\xc6\xf3\xea\xa9\xe2\xec\xa8\x6d\x70\xd3\x74\xad\xa6\x8c\xe7\xb4\x6f\x62\x66\xfe\xe7\xd1\x8d\xb3\xca\x66\xfe\x9e\xf8\xa2\x80\xb0\xbe\x74\x2e\x22\x0a\x0a\x44\x72\xa4\x42\xfd\x6e\x6b\xc0\x61\xab\xa2\x9a\xbd\xaa\x61\x79\x3a\x20\x22\x54\xd4\xaa\x68\xd3\xa6\x64\x61\x79\xa1\x61\x67\xba\xe2\xea\x49\xb3\xa5\x6c\xab\xd9\x6f\x6b\xc6\xd3\x77\xbb\xab\xbd\xb7\xe0\xa3\xcc\xbf\x64\x2d\xd4\xab\x6a\x6f\x62\xa3\xa1\xd0\x64\x6f\x63\xbd\x72\xa4\x4e\xf2\x20\x49\xc0\x61\xd7\xa3\xeb\xc2\x6d\xb2\x84\x66\xf7\xd8\x9e\x8c\x6d\x61\x6b\x9e\xa9\xe2\xec\xb0\x69\xc1\x5a\xd3\x6b\x6d\x69\x64\x73\x2e\x22\x0a\x0a\x47\x55\x45\xb8\x65\xfa\x91\x27\xa6\x70\xc2\x6d\xb2\x9e\x96\xd6\xc6\xad\x74\xe0\xf0\x63\xda\xbe\x75\x6c\x74\xa1\xbd\xfb\xd7\x72\xc9\xed\xa4\x42\xf7\xb7\xa0\xb4\x8f\xbf\x72\xb4\x92\xcc\x65\x67\xa9\x9e\x66\xc2\xf9\x47\x55\x45\xb8\x65\xfa\xb5\x92\xc6\xf7\xd8\x9e\xf8\xdf\xb0\x9e\x62\xf1\x67\x68\xd1\x72\x2e");
}



void DoMisc_read_map(void)
{
  int obj = OBJ_MAP;

  //if not holding it, try to take it
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    if (TakeRoutine(obj, "(taking it first)")) return;

  TimePassed = 1;
  PrintCompLine("\x85\x6d\x61\x70\xaa\x68\xf2\xa1\xd0\x66\xd3\xbe\xa6\xf8\xa2\x95\xa9\x9e\x63\xcf\xbb\x97\x73\x83\x9e\xfd\x72\x67\xbe\xa6\x63\xcf\xbb\x84\x63\xca\x74\x61\xa7\xa1\xd0\x68\xa5\xd6\x83\xa9\x9e\x70\xaf\x68\xa1\xcf\x61\xd7\x80\xcb\xbb\x67\x9e\x63\xcf\xbb\x97\xa4\x4f\xed\x8a\x96\xd6\xeb\xaf\x68\x73\xb5\xcf\x61\x64\x84\x73\xa5\xa2\x77\xbe\x74\xb5\x9a\x6d\xbb\x6b\xd5\x20\x22\x54\xba\x53\xbd\xed\x20\x42\xbb\xc2\x77\x22\x2e");
}



void DoMisc_read_boat_label(void)
{
  int obj = OBJ_BOAT_LABEL;

  //if not holding it, try to take it
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    if (TakeRoutine(obj, "(taking it first)")) return;

  TimePassed = 1;
  PrintCompLine("\x20\x20\x21\x21\x21\x21\x46\x52\x4f\x42\x4f\x5a\x5a\x20\x4d\x41\x47\x49\x43\x20\x42\x4f\x41\x54\x20\x43\x4f\x4d\x50\x41\x4e\x59\x21\x21\x21\x21\x0a\x0a\x48\x65\xdf\x6f\xb5\x53\x61\x69\xd9\x72\x21\x0a\x0a\x49\x6e\xc5\x72\x75\x63\xf0\xca\xa1\x66\xd3\x20\xfe\x65\x3a\x0a\x0a\x20\x20\xb8\xba\x67\x65\xa6\xa7\xbd\xa3\xb0\x6f\x64\xc4\xdd\xb7\xaf\xac\xb5\x73\x61\xc4\x22\x4c\x61\xf6\xfa\x22\x2e\x0a\x20\x20\xb8\xba\x67\x65\xa6\xbd\xaa\x68\xd3\x65\xb5\x73\x61\xc4\x22\x4c\xad\x64\x22\xae\xb6\x81\x64\x69\xa9\x63\xf0\xca\xa8\xb4\x77\xce\xfa\x86\xb7\xad\xa6\xbd\xee\xad\x65\x75\xd7\xb6\x81\x62\x6f\xaf\x2e\x0a\x0a\x57\xbb\xf4\xe5\x79\x3a\x0a\x0a\x20\x98\x9a\x62\x6f\xaf\x87\x67\x75\xbb\xad\xd1\xd5\xa3\x67\x61\xa7\xc5\xa3\xdf\xcc\x65\x66\x65\x63\x74\xa1\x66\xd3\xa3\xeb\xac\x69\x6f\xab\xdd\x20\x37\x36\xee\x69\xdf\xb2\x65\x63\xca\x64\xa1\x66\xc2\xf9\x64\xaf\x9e\xdd\xeb\xd8\xfa\xe0\x9e\xd3\x20\xf6\xf0\xea\x66\x69\x72\xc5\x20\xfe\xd5\xb5\x77\xce\xfa\x65\xd7\xb6\x63\xe1\xbe\xc6\x69\x72\xc5\x2e\x0a\x0a\x57\xbb\x6e\x97\x3a\x0a\x20\x20\x98\x9a\x62\x6f\xaf\x87\x6d\x61\xe8\x8a\xa2\xa7\xeb\xfd\xc5\x69\x63\x2e\x0a\x20\x20\x20\x47\xe9\xab\x4c\x75\x63\x6b\x21");
}



void DoMisc_read_guide(void)
{
  int obj = OBJ_GUIDE;

  //if not holding it, try to take it
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    if (TakeRoutine(obj, "(taking it first)")) return;

  TimePassed = 1;
  PrintCompLine("\x22\x09\x46\xd9\x6f\xab\x43\xca\x74\xc2\xea\x44\x61\xf9\x23\x33\x0a\x0a\x46\x43\x44\x23\x33\xb7\xe0\xb3\xca\xc5\x72\x75\x63\xd1\xab\xa7\xc8\xbf\xb6\x37\x38\x33\x8a\x81\x47\xa9\xaf\x20\x55\xb9\xac\x67\xc2\xf6\xab\x45\x6d\x70\x69\xa9\x89\xcd\x72\xed\x73\xa1\x81\x6d\x69\x67\x68\x74\xc4\x46\xf1\x67\x69\xab\x52\x69\xd7\x72\x83\x9a\x77\xd3\x6b\xb7\xe0\xaa\x75\x70\x70\xd3\xd1\xab\x62\xc4\xd0\x67\xf4\xe5\x8a\x33\x37\xee\x69\xdf\x69\xca\x20\x7a\xd3\x6b\x6d\x69\x64\xa1\x66\xc2\xf9\x92\xae\x6d\x6e\x69\x70\xff\xd4\xa6\xd9\xe7\xea\x74\x79\xf4\xe5\x20\x4c\xd3\xab\x44\x69\x6d\xf8\xa6\x46\xfd\x96\x61\xab\x81\x45\x78\x63\xbe\x73\x69\xd7\x83\x9a\x69\x6d\x70\xa9\x73\x73\x69\xd7\xaa\x74\x72\x75\x63\x74\xd8\x9e\x9a\x63\xe1\x70\x6f\xd6\xab\xdd\x20\x33\x37\x30\x2c\x30\x30\x30\xb3\x75\x62\x69\x63\xc6\xf3\xa6\xdd\xb3\xca\x63\xa9\xd1\xb5\x9a\x32\x35\x36\xc6\xf3\xa6\x74\xe2\xea\xaf\x80\xb3\xd4\xd1\x72\xb5\x8c\x31\x39\x33\xc6\xf3\xa6\xf8\xe8\xa3\xa6\x81\xbd\x70\x83\x9e\xfd\x6b\x9e\x63\xa9\xaf\xd5\xb0\x65\xce\xb9\x80\xcc\x61\xf9\xcd\xa1\xd0\x76\x6f\x6c\x75\x6d\x9e\xdd\x20\x31\x2e\x37\xb0\x69\xdf\x69\xca\xb3\x75\x62\x69\x63\xc6\xf3\x74\xb5\xad\xa3\xa9\xd0\xdd\x20\x31\x32\xee\x69\xdf\x69\xca\xaa\x71\x75\xbb\x9e\x66\xf3\x74\xb5\x8c\xd0\x73\x68\xd3\x9e\xf5\xed\x8a\x33\x36\x95\xa5\x73\x8c\x66\xf3\x74\x2e\x0a\x0a\x85\x63\xca\xc5\x72\x75\x63\xf0\xca\x8a\x46\x43\x44\x23\x33\x9f\xe9\x6b\x20\x31\x31\x32\xcc\x61\x79\xa1\x66\xc2\xf9\x67\xc2\xf6\xab\x62\xa9\x61\x6b\x84\xbd\x80\xcc\xd5\x69\xe7\xf0\xca\xa4\x49\xa6\xa9\x71\x75\x69\xa9\xab\xd0\x77\xd3\x6b\xc6\xd3\x63\x9e\xdd\x20\x33\x38\x34\xaa\xfd\xd7\x73\xb5\x33\x34\xaa\xfd\xd7\xcc\xf1\xd7\x72\x73\xb5\x31\x32\xfb\xb1\xa7\xf3\x72\x73\xb5\x32\x9f\xd8\x74\xcf\xcc\x6f\xd7\x73\xb5\x8c\xd0\x70\xbb\x74\xf1\x64\x67\x9e\xa7\xa3\xeb\xbf\xb6\x74\xa9\x65\x83\x9e\x77\xd3\x6b\xb7\xe0\xee\xad\x61\x67\xd5\xb0\xc4\xd0\x63\xe1\x6d\x8c\xd1\x61\xf9\x63\xe1\x70\x6f\xd6\xab\xdd\x20\x32\x33\x34\x35\xb0\xd8\xbf\x75\x63\xf4\x74\x73\xb5\x32\x33\x34\x37\xaa\x65\x63\xa9\x74\xbb\x69\xbe\x20\x28\xaf\xcb\xbf\xc5\x9f\x77\xba\xdd\xb7\x68\xe1\xb3\xa5\x6c\xab\x74\x79\xfc\x29\xb5\x31\x32\x2c\x32\x35\x36\xeb\x61\xfc\xb6\x73\x68\x75\x66\x66\xcf\x72\x73\xb5\x35\x32\x2c\x34\x36\x39\xda\x75\x62\xef\xb6\xc5\x61\x6d\xfc\x72\x73\xb5\x32\x34\x35\x2c\x31\x39\x33\xda\xd5\x9f\x61\xfc\xeb\xc2\x63\xbe\x73\xd3\x73\xb5\x8c\xed\xbb\xec\xae\xed\xee\x69\xdf\x69\xca\xcc\xbf\xab\x74\xa9\xbe\x2e\x0a\x0a\x57\x9e\xf8\xdf\xe4\xf2\xeb\x6f\xa7\xa6\xa5\xa6\x73\xe1\x9e\xdd\x80\xee\xd3\x9e\xa7\xd1\xa9\xc5\x84\x66\xbf\x74\xd8\xbe\x8a\x46\x43\x44\x23\x33\xa3\xa1\x77\x9e\x63\xca\x64\x75\x63\xa6\x8f\xca\xa3\xe6\x75\x69\xe8\xab\xbd\xd8\x8a\x81\x66\x61\x63\x69\xf5\xf0\xbe\x3a\x0a\x0a\x20\x20\x20\x20\x20\x20\x20\x20\x31\x29\x88\xaa\x74\xbb\xa6\x92\x9f\x90\xa0\xa9\xa8\xb4\x81\x44\x61\xf9\x4c\x6f\x62\x62\x79\x8e\xc3\xf8\xdf\xe4\xff\x69\x63\x9e\xca\x86\xb6\xf1\x67\x68\xa6\xa2\xaf\x2e\x2e\x2e\x2e");
}



void DoMisc_read_tube(void)
{
  int obj = OBJ_TUBE;

  //if not holding it, try to take it
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    if (TakeRoutine(obj, "(taking it first)")) return;

  TimePassed = 1;
  PrintCompLine("\x2d\x2d\x2d\x3e\x20\x46\xc2\x62\x6f\x7a\x7a\x20\x4d\x61\x67\x69\x63\x20\x47\xf6\x6b\x20\x43\xe1\x70\xad\xc4\x3c\x2d\x2d\x2d\x0a\x09\x20\x20\x41\xdf\x2d\x50\xd8\x70\x6f\xd6\x20\x47\x75\x6e\x6b");
}



void DoMisc_read_owners_manual(void)
{
  int obj = OBJ_OWNERS_MANUAL;

  //if not holding it, try to take it
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    if (TakeRoutine(obj, "(taking it first)")) return;

  TimePassed = 1;
  PrintCompLine("\x43\xca\x67\xf4\x74\x75\xfd\xf0\xca\x73\x21\x0a\x0a\x8b\xbb\x9e\x81\x70\xf1\x76\x69\xcf\x67\xd5\xae\x77\xed\xb6\xdd\x20\x5a\x4f\x52\x4b\x20\x49\x3a\x82\x20\x47\xa9\xaf\x20\x55\xb9\xac\x67\xc2\xf6\xab\x45\x6d\x70\x69\xa9\xb5\xd0\xd6\x6c\x66\x2d\x63\xca\x74\x61\xa7\xd5\x8d\xaa\x65\x6c\x66\x2d\x6d\x61\xa7\x74\x61\xa7\x84\xf6\x69\xd7\x72\xd6\xa4\x49\xd2\xfe\xd5\x8d\xee\x61\xa7\x74\x61\xa7\xd5\xa8\xb4\x61\x63\x63\xd3\x64\xad\x63\x9e\xf8\xa2\xe4\xd3\x6d\xe2\xae\xfc\xf4\xf0\x9c\x70\xf4\x63\xf0\x63\xbe\xc6\xd3\xaa\x6d\xe2\xea\xf6\x69\xd7\x72\xd6\x73\xb5\x5a\x4f\x52\x4b\xb7\x69\xdf\xeb\xc2\x76\x69\xe8\xee\xad\xc4\x6d\xca\xa2\xa1\xdd\x9f\xc2\x75\x62\xcf\x2d\x66\xa9\x9e\x6f\xfc\xf4\xf0\x6f\x6e\x2e");
}



void DoMisc_read_prayer(void)
{
  TimePassed = 1;
  PrintCompLine("\x85\x70\xf4\x79\xac\x87\xa7\x73\x63\xf1\xef\xab\xa7\xa3\xb4\xad\x63\x69\xd4\xa6\x73\x63\xf1\x70\x74\xb5\xf4\xa9\xec\x20\xfe\xd5\x9f\x6f\x64\x61\x79\xa4\x49\xa6\xd6\x65\x6d\xa1\xbd\xb0\x9e\xd0\x70\xce\xf5\x70\x70\x69\x63\xa3\x67\x61\xa7\xc5\xaa\x6d\xe2\xea\xa7\xd6\x63\x74\x73\xb5\x61\x62\xd6\xe5\x2d\x6d\xa7\xe8\x64\xed\x73\x73\xb5\x8c\x81\x70\x69\x63\x6b\x84\x75\x70\x8d\xcc\xc2\x70\x70\x84\xdd\xaa\x6d\xe2\xea\x6f\x62\x6a\x65\x63\x74\x73\x83\x9e\x66\xa7\xe2\x20\xd7\x72\xd6\xb3\xca\x73\x69\x67\x6e\xa1\x74\xa9\x73\x70\xe0\xd6\x72\xa1\xbd\x80\xcb\x8c\xdd\x80\xcc\xbf\x64\xa4\x41\xdf\xfb\x76\x69\xe8\x6e\x63\x9e\xa7\x64\x69\xe7\xd1\xa1\xa2\xaf\x80\xb0\x65\xf5\x65\x66\xa1\xdd\x80\xa3\x6e\x63\x69\xd4\xa6\x5a\xd3\x6b\xac\xa1\x77\xac\x9e\x6f\x62\x73\x63\xd8\x65\x2e");
}



void DoMisc_read_wooden_door(void)
{
  TimePassed = 1;
  PrintCompLine("\x85\xd4\x67\xf4\x76\x97\xa1\x74\xf4\x6e\x73\xfd\xd1\x89\x22\xbc\x9a\x73\x70\x61\x63\x9e\xa7\xd1\xe5\x69\xca\xe2\xec\xcb\x65\x66\xa6\x62\xfd\x6e\x6b\x2e\x22");
}



void DoMisc_read_engravings(void)
{
  TimePassed = 1;
  PrintCompLine("\x85\xd4\x67\xf4\x76\x97\xa1\x77\xac\x9e\xa7\x63\xb2\xd5\xa8\xb4\x81\xf5\x76\x84\xc2\x63\x6b\x8a\x81\xe7\xd7\xb7\xe2\xea\x62\xc4\xad\x20\xf6\x6b\xe3\x77\xb4\xcd\xb9\x83\x65\xc4\xe8\x70\x69\x63\x74\xb5\xa7\xaa\x79\x6d\x62\x6f\xf5\x63\xc6\xd3\x6d\xb5\x81\xef\xf5\x65\x66\xa1\xdd\x80\xa3\x6e\x63\x69\xd4\xa6\x5a\xd3\x6b\xac\x73\xa4\x53\x6b\x69\xdf\x66\x75\xdf\xc4\xa7\xd1\x72\x77\x6f\xd7\xb4\xf8\xa2\x80\xb0\xe0\xda\x65\xf5\x65\x66\xa1\xbb\x9e\x65\x78\x63\xac\x70\x74\xa1\x69\xdf\xfe\x74\xf4\xf0\x9c\x81\x6d\x61\x6a\xd3\xda\x65\xf5\x67\x69\xa5\xa1\xd1\xed\x74\xa1\xdd\x95\xaf\x9f\x69\x6d\x65\xa4\x55\x6e\x66\xd3\x74\xf6\xaf\x65\xec\xb5\xd0\xfd\xd1\xb6\x61\x67\x9e\xd6\x65\x6d\xa1\xbd\xc0\x61\xd7\xb3\xca\x73\x69\xe8\xa9\xab\x96\xf9\x62\xfd\x73\x70\xa0\x6d\xa5\xa1\x8c\x6a\xfe\xa6\xe0\xaa\x6b\x69\xdf\x66\x75\xdf\xc4\x65\x78\x63\xb2\xd5\x80\x6d\x2e");
}



void DoMisc_open_egg(void)
{
  int with;

  with = GetWith(); if (with < 0) return;

  if (Obj[OBJ_EGG].loc != INSIDE + OBJ_YOU)
    {PrintCompLine("\x8b\xbb\xd4\x27\xa6\x68\x6f\x6c\x64\x84\x81\x65\x67\x67\x2e"); return;}

  if (Obj[OBJ_EGG].prop & PROP_OPEN)
    {PrintCompLine("\x85\x65\x67\xc1\x9a\xe2\xa9\x61\x64\xc4\x6f\xfc\x6e\x2e"); return;}

  if (with >= NUM_OBJECTS)
    {PrintCompLine("\x8b\xbb\xd4\x27\xa6\x68\x6f\x6c\x64\x84\xa2\x61\x74\x21"); return;}

  if (with == 0)
    {PrintCompLine("\x8b\xcd\xd7\xe4\x65\xc7\xa0\xb6\x81\xbd\x6f\x6c\xa1\xe3\xb6\x81\x65\x78\xfc\x72\xf0\x73\x65\x2e"); return;}

  if (with == OBJ_YOU)
    {PrintCompLine("\x49\xcc\xa5\x62\xa6\x8f\x63\xa5\x6c\xab\x64\xba\xa2\xaf\xb7\xc7\x68\xa5\xa6\x64\x61\x6d\x61\x67\x84\x69\x74\x2e"); return;}

  if ((Obj[with].prop & PROP_WEAPON) ||
      (Obj[with].prop & PROP_TOOL))
  {
    PrintCompLine("\x85\x65\x67\xc1\x9a\xe3\x77\xae\xfc\x6e\xb5\x62\xf7\x80\xb3\x6c\x75\x6d\x73\xa7\xbe\xa1\xdd\x86\xb6\xaf\xd1\x6d\x70\xa6\xcd\xa1\xd6\xf1\xa5\x73\xec\xb3\xe1\x70\xc2\x6d\xb2\xd5\xa8\x74\xa1\xbe\x96\xf0\x63\xa3\x70\xfc\x61\x6c\x2e");
    TimePassed = 1;

    Obj[OBJ_EGG].loc = 0;
    Obj[OBJ_BROKEN_EGG].loc = INSIDE + OBJ_YOU;
    Obj[OBJ_BROKEN_EGG].prop |= PROP_OPENABLE;
    Obj[OBJ_BROKEN_EGG].prop |= PROP_OPEN;
    return;
  }

  PrintCompLine("\x8b\xe7\x93\x6f\xfc\xb4\xc7\xb7\xc7\xde\xa2\x61\x74\x21");
}



void DoMisc_climbthrough_kitchen_window(void)
{
  if (KitchenWindowOpen == 0)
  {
    PrintCompLine("\x85\xf8\xb9\xf2\x87\x63\xd9\xd6\x64\x2e");
    ItObj = FOBJ_KITCHEN_WINDOW;
  }
  else
  {
    if (Obj[OBJ_YOU].loc == ROOM_EAST_OF_HOUSE)
      GoToRoutine(ROOM_KITCHEN);
    else
      GoToRoutine(ROOM_EAST_OF_HOUSE);
  }
}



void DoMisc_climbthrough_trap_door(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_LIVING_ROOM)
    GoFrom_LivingRoom_Down();
  else
    GoFrom_Cellar_Up();
}



void DoMisc_climbthrough_grate(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_GRATING_CLEARING)
    GoFrom_GratingClearing_Down();
  else
    GoFrom_GratingRoom_Up();
}



void DoMisc_climbthrough_slide(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_CELLAR)
    PrintBlockMsg(BLA);
  else
  {
    if (YouAreInBoat == 0) PrintCompLine("\x8b\x74\x75\x6d\x62\xcf\xcc\xf2\xb4\x81\x73\xf5\xe8\x2e\x2e\x2e\x2e\x0a");
    GoToRoutine(ROOM_CELLAR);
  }
}



void DoMisc_climbthrough_chimney(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_KITCHEN)
    GoFrom_Kitchen_Down();
  else
    GoFrom_Studio_Up();
}



void DoMisc_climbthrough_barrow_door(void)
{
  GoFrom_StoneBarrow_West();
}



void DoMisc_climbthrough_gate(void)
{
  if (SpiritsBanished == 0)
    PrintCompLine("\x85\x67\xaf\x9e\x9a\x70\xc2\xd1\x63\xd1\xab\x62\xc4\xad\xa8\x6e\x76\xb2\x69\x62\xcf\xc6\xd3\x63\x65\xa4\x49\xa6\x6d\x61\x6b\xbe\x86\xb6\xd1\x65\xa2\xa3\xfa\x9e\xbd\x9f\xa5\xfa\xa8\x74\x2e");
  else
    GoToRoutine(ROOM_LAND_OF_LIVING_DEAD);
}



void DoMisc_climbthrough_crack(void)
{
  PrintCompLine("\x8b\xe7\x93\x66\xc7\x95\xc2\x75\x67\xde\x81\x63\xf4\x63\x6b\x2e");
}



void DoMisc_enter_white_house(void)
{
  if (Obj[OBJ_YOU].loc != ROOM_EAST_OF_HOUSE)
    PrintCompLine("\x49\x91\x27\xa6\xd6\x9e\x68\xf2\x89\x67\x65\xa6\xa7\xc6\xc2\xf9\xa0\x72\x65\x2e");
  else
    DoMisc_climbthrough_kitchen_window();
}



void DoMisc_slidedown_slide(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_CELLAR)
    PrintCompLine("\xdc\x75\x27\xa9\xa3\x6c\xa9\x61\x64\xc4\xaf\x80\xb0\xff\xbd\x6d\x2e");
  else
  {
    if (YouAreInBoat == 0) PrintCompLine("\x8b\x74\x75\x6d\x62\xcf\xcc\xf2\xb4\x81\x73\xf5\xe8\x2e\x2e\x2e\x2e\x0a");
    GoToRoutine(ROOM_CELLAR);
  }
}



void DoMisc_climbup_mountain_range(void)
{
  PrintCompLine("\x44\xca\x27\xa6\x8f\xef\xf5\x65\xd7\xee\x65\x3f\x82\xee\xa5\xe5\x61\xa7\xa1\xbb\x9e\x69\x6d\x70\xe0\x73\x61\x62\x6c\x65\x21");
}



void DoMisc_climbup_white_cliff(void)
{
  PrintCompLine("\x85\x63\xf5\x66\xd2\x9a\xbd\xba\xc5\xf3\x70\xc6\xd3\xb3\xf5\x6d\x62\x97\x2e");
}



void DoMisc_climbup_tree(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_PATH)
    GoToRoutine(ROOM_UP_A_TREE);
  else
    PrintBlockMsg(BL9);
}



void DoMisc_climbdown_tree(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_PATH)
    PrintBlockMsg(BL0);
  else
    GoToRoutine(ROOM_PATH);
}



void DoMisc_climbup_chimney(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_STUDIO)
    GoFrom_Studio_Up();
  else
    PrintBlockMsg(BL0);
}



void DoMisc_climbdown_chimney(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_KITCHEN)
    GoFrom_Kitchen_Down();
  else
    PrintBlockMsg(BL0);
}



void DoMisc_climbup_ladder(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_LADDER_BOTTOM)
    GoToRoutine(ROOM_LADDER_TOP);
  else
    PrintBlockMsg(BL0);
}



void DoMisc_climbdown_ladder(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_LADDER_TOP)
    GoToRoutine(ROOM_LADDER_BOTTOM);
  else
    PrintBlockMsg(BL0);
}



void DoMisc_climbup_slide(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_CELLAR)
    PrintBlockMsg(BLA);
  else
    PrintBlockMsg(BL0);
}



void DoMisc_climbdown_slide(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_SLIDE_ROOM)
  {
    if (YouAreInBoat == 0) PrintCompLine("\x8b\x74\x75\x6d\x62\xcf\xcc\xf2\xb4\x81\x73\xf5\xe8\x2e\x2e\x2e\x2e\x0a");
    GoToRoutine(ROOM_CELLAR);
  }
  else
    PrintBlockMsg(BL0);
}



void DoMisc_climbup_climbable_cliff(void)
{
  switch (Obj[OBJ_YOU].loc)
  {
    default:                 PrintBlockMsg(BL0);             break;
    case ROOM_CLIFF_MIDDLE:  GoToRoutine(ROOM_CANYON_VIEW);  break;
    case ROOM_CANYON_BOTTOM: GoToRoutine(ROOM_CLIFF_MIDDLE); break;
  }
}



void DoMisc_climbdown_climbable_cliff(void)
{
  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_CANYON_VIEW:   GoToRoutine(ROOM_CLIFF_MIDDLE);  break;
    case ROOM_CLIFF_MIDDLE:  GoToRoutine(ROOM_CANYON_BOTTOM); break;
    default:                 PrintBlockMsg(BL0);              break;
  }
}



void DoMisc_climbup_stairs(void)
{
  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_CELLAR:          GoFrom_Cellar_Up();              break;
    case ROOM_CYCLOPS_ROOM:    GoFrom_CyclopsRoom_Up();         break;
    case ROOM_KITCHEN:         GoToRoutine(ROOM_ATTIC);         break;
    case ROOM_RESERVOIR_NORTH: GoToRoutine(ROOM_ATLANTIS_ROOM); break;
    case ROOM_ATLANTIS_ROOM:   GoToRoutine(ROOM_SMALL_CAVE);    break;
    case ROOM_LOUD_ROOM:       GoToRoutine(ROOM_DEEP_CANYON);   break;
    case ROOM_CHASM_ROOM:      GoToRoutine(ROOM_EW_PASSAGE);    break;
    case ROOM_EGYPT_ROOM:      GoToRoutine(ROOM_NORTH_TEMPLE);  break;
    case ROOM_GAS_ROOM:        GoToRoutine(ROOM_SMELLY_ROOM);   break;
    case ROOM_LADDER_TOP:      GoToRoutine(ROOM_MINE_4);        break;
    default:                   PrintBlockMsg(BL0);              break;
  }
}



void DoMisc_climbdown_stairs(void)
{
  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_LIVING_ROOM:   GoFrom_LivingRoom_Down();            break;
    case ROOM_ATTIC:         GoToRoutine(ROOM_KITCHEN);           break;
    case ROOM_TREASURE_ROOM: GoToRoutine(ROOM_CYCLOPS_ROOM);      break;
    case ROOM_SMALL_CAVE:    GoToRoutine(ROOM_ATLANTIS_ROOM);     break;
    case ROOM_TINY_CAVE:     GoToRoutine(ROOM_ENTRANCE_TO_HADES); break;
    case ROOM_EW_PASSAGE:    GoToRoutine(ROOM_CHASM_ROOM);        break;
    case ROOM_DEEP_CANYON:   GoToRoutine(ROOM_LOUD_ROOM);         break;
    case ROOM_TORCH_ROOM:    GoToRoutine(ROOM_NORTH_TEMPLE);      break;
    case ROOM_NORTH_TEMPLE:  GoToRoutine(ROOM_EGYPT_ROOM);        break;
    case ROOM_SMELLY_ROOM:   GoToRoutine(ROOM_GAS_ROOM);          break;
    default:                 PrintBlockMsg(BL0);                  break;
  }
}



void DoMisc_examine_sword(void)
{
  int glow = Obj[OBJ_SWORD].thiefvalue;

       if (glow == 1) PrintCompLine("\xdc\xd8\xaa\x77\xd3\xab\x9a\x67\xd9\xf8\x9c\xf8\xa2\xa3\xc6\x61\xa7\xa6\x62\x6c\x75\x9e\x67\xd9\x77\x2e");
  else if (glow == 2) PrintCompLine("\xdc\xd8\xaa\x77\xd3\xab\x9a\x67\xd9\xf8\x9c\xd7\x72\xc4\x62\xf1\x67\x68\x74\x6c\x79\x2e");
  else                PrintCompLine("\x8b\x64\xca\x27\xa6\xd6\x9e\xad\x79\xa2\x84\xf6\xfe\x75\x61\x6c\x2e");
}



void DoMisc_examine_match(void)
{
  if (Obj[OBJ_MATCH].prop & PROP_LIT)
    PrintCompLine("\x85\x6d\xaf\xfa\x87\x62\xd8\x6e\x97\x2e");
  else
    PrintCompLine("\x85\x6d\xaf\xfa\x62\xe9\x6b\xa8\x73\x93\xd7\x72\xc4\xa7\xd1\xa9\xc5\x97\xb5\x65\x78\x63\x65\x70\xa6\x66\xd3\xb7\xcd\x74\x27\xa1\x77\xf1\x74\xd1\xb4\xca\xa8\x74\x2e");
}



void DoMisc_examine_candles(void)
{
  PrintCompText("\x85\xe7\xb9\xcf\xa1\xbb\x65\x20");
  if (Obj[OBJ_CANDLES].prop & PROP_LIT)
    PrintCompLine("\x62\xd8\x6e\x97\x2e");
  else
    PrintCompLine("\xa5\x74\x2e");
}



void DoMisc_examine_torch(void)
{
  PrintCompLine("\x85\xbd\x72\xfa\x87\x62\xd8\x6e\x97\x2e");
}



void DoMisc_examine_thief(void)
{
  PrintCompLine("\x85\xa2\x69\x65\xd2\x9a\xd0\x73\xf5\x70\xfc\x72\xc4\xfa\xbb\x61\x63\xd1\xb6\xf8\xa2\xb0\xbf\x64\xc4\x65\x79\xbe\x95\xaf\xc6\xf5\xa6\x62\x61\x63\x6b\x8d\xc6\xd3\xa2\xa4\x48\x9e\xe7\x72\xf1\xbe\xb5\xe2\xca\xc1\xf8\xa2\xa3\xb4\xf6\x6d\xb2\x74\x61\x6b\x61\x62\xcf\xa3\x72\xc2\x67\xad\x63\x65\xb5\xd0\xfd\x72\x67\x9e\x62\x61\xc1\x6f\xd7\xb6\xce\xa1\x73\x68\xa5\x6c\xe8\xb6\x8c\xd0\x76\x69\x63\x69\xa5\xa1\xc5\x69\xcf\x74\xbd\xb5\x77\x68\x6f\xd6\xb0\xfd\xe8\x87\x61\x69\x6d\xd5\xee\xd4\x61\x63\x97\xec\xa8\xb4\x92\xcc\x69\xa9\x63\xf0\xca\xa4\x49\x27\xab\x77\xaf\xfa\xae\xf7\xa8\xd2\x49\xb7\xac\x9e\xc9\x75\x2e");
}



void DoMisc_examine_tool_chest(void)
{
  PrintCompLine("\x85\xfa\xbe\x74\xa1\xbb\x9e\xe2\xea\x65\x6d\x70\x74\x79\x2e");
}



void DoMisc_examine_board(void)
{
  PrintCompLine("\x85\x62\x6f\xbb\x64\xa1\xbb\x9e\xd6\x63\xd8\x65\xec\xc6\xe0\xd1\xed\x64\x2e");
}



void DoMisc_examine_chain(void)
{
  PrintCompLine("\x85\xfa\x61\xa7\xaa\x65\x63\xd8\xbe\xa3\xb0\xe0\x6b\x65\xa6\xf8\xa2\xa7\x80\xaa\xcd\x66\x74\x2e");
}



void DoMisc_open_tool_chest(void)
{
  PrintCompLine("\x85\xfa\xbe\x74\xa1\xbb\x9e\xe2\xa9\x61\x64\xc4\x6f\xfc\x6e\x2e");
}



void DoMisc_open_book(void)
{
  PrintCompLine("\x85\x62\xe9\x6b\x87\xe2\xa9\x61\x64\xc4\x6f\xfc\xb4\xbd\xeb\x61\x67\x9e\x35\x36\x39\x2e");
}



void DoMisc_close_book(void)
{
  PrintCompLine("\x41\xa1\xcd\x72\xab\xe0\x86\x9f\x72\x79\xb5\x81\x62\xe9\x6b\x91\xe3\xa6\xef\xb3\xd9\xd6\x64\x2e");
}



void DoMisc_open_boarded_window(void)
{
  PrintCompLine("\x85\xf8\xb9\xf2\xa1\xbb\x9e\x62\x6f\xbb\xe8\xab\x8c\xe7\x93\xef\xae\xfc\xed\x64\x2e");
}



void DoMisc_break_boarded_window(void)
{
  PrintCompLine("\x8b\xe7\x93\x62\xa9\x61\x6b\x80\xb7\xa7\x64\xf2\xa1\x6f\xfc\x6e\x2e");
}



void DoMisc_open_close_dam(void)
{
  PrintCompLine("\x53\xa5\xb9\xa1\xa9\xe0\xca\x61\x62\xcf\xb5\x62\xf7\x95\x9a\xb2\x93\x68\x6f\x77\x2e");
}



void DoMisc_ring_hot_bell(void)
{
  PrintCompLine("\x85\xef\xdf\x87\xbd\xba\x68\xff\x89\xa9\x61\x63\x68\x2e");
}



void DoMisc_read_button(void)
{
  PrintCompLine("\x99\x79\x27\xa9\xe6\xa9\x65\x6b\x89\xc9\x75\x2e");
}



void DoMisc_raise_lower_granite_wall(void)
{
  PrintCompLine("\x49\x74\x27\xa1\x73\x6f\xf5\xab\x67\xf4\x6e\xc7\x65\x2e");
}



void DoMisc_raise_lower_chain(void)
{
  PrintCompLine("\x50\xac\xcd\x70\xa1\x8f\x73\x68\xa5\x6c\xab\x64\xba\xa2\xaf\x89\x81\x62\xe0\x6b\x65\x74\x2e");
}



void DoMisc_move_chain(void)
{
  PrintCompLine("\x85\xfa\x61\xa7\x87\xd6\x63\xd8\x65\x2e");
}



void DoMisc_count_candles(void)
{
  PrintCompLine("\x4c\x65\x74\x27\xa1\xd6\x65\xb5\x68\xf2\xee\xad\xc4\x6f\x62\x6a\x65\x63\x74\xa1\xa7\xa3\xeb\x61\x69\x72\x3f\x20\x44\xca\x27\xa6\xd1\xdf\xee\x65\xb5\x49\x27\xdf\xe6\x65\xa6\x69\x74\x2e");
}



void DoMisc_count_leaves(void)
{
  PrintCompLine("\x99\xa9\xa3\xa9\x20\x36\x39\x2c\x31\x30\x35\xcb\xbf\xd7\xa1\xa0\x72\x65\x2e");
}



void DoMisc_examine_lamp(void)
{
  PrintCompText("\x85\xfd\x6d\x70\x20");

  if (LampTurnsLeft == 0)
    PrintCompLine("\xcd\xa1\x62\xd8\xed\xab\xa5\x74\x2e");
  else if (Obj[OBJ_LAMP].prop & PROP_LIT)
    PrintCompLine("\x9a\x6f\x6e\x2e");
  else
    PrintCompLine("\x9a\x74\xd8\xed\xab\xdd\x66\x2e");
}



void DoMisc_examine_troll(void)
{
  PrintDesc_Troll(1);
  PrintCompText("\x0a"); // above omits end newline
}



void DoMisc_examine_cyclops(void)
{
  if (CyclopsState == 3)
    PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x9a\x73\xcf\x65\x70\x84\xf5\x6b\x9e\xd0\x62\x61\x62\x79\xb5\xe2\xef\xc7\xa3\x20\xd7\x72\xc4\x75\x67\xec\xae\x6e\x65\x2e");
  else
    PrintCompLine("\x41\xc0\xf6\x67\x72\xc4\x63\x79\x63\xd9\x70\xa1\x9a\xc5\xad\x64\x84\xaf\x80\xc6\xe9\xa6\xdd\x80\xaa\x74\x61\x69\x72\x73\x2e");
}



void DoMisc_examine_white_house(void)
{
  PrintCompLine("\x85\x68\xa5\xd6\x87\xd0\xef\x61\xf7\x69\x66\x75\xea\x63\x6f\xd9\x6e\x69\xe2\xc0\xa5\xd6\xb7\xce\xfa\x87\x70\x61\xa7\xd1\xab\x77\xce\xd1\xa4\x49\xa6\x9a\x63\xcf\xbb\x95\xaf\x80\xae\x77\xed\x72\xa1\x6d\xfe\xa6\xcd\xd7\xb0\xf3\xb4\x65\x78\x74\xa9\x6d\x65\xec\xb7\xbf\x6c\xa2\x79\x2e");
}



void DoMisc_open_close_barrow_door(void)
{
  PrintCompLine("\x85\x64\xe9\xb6\x9a\xbd\xba\xa0\x61\x76\x79\x2e");
}



void DoMisc_open_close_studio_door(void)
{
  PrintCompLine("\x85\x64\xe9\xb6\x77\xca\x27\xa6\x62\x75\x64\x67\x65\x2e");
}



void DoMisc_open_close_bag_of_coins(void)
{
  PrintCompLine("\x85\x63\x6f\xa7\xa1\xbb\x9e\x73\x61\x66\x65\xec\xa8\x6e\x73\x69\xe8\x3b\x80\xa9\x27\xa1\xe3\xe4\xf3\xab\xbd\xcc\xba\xa2\x61\x74\x2e");
}



void DoMisc_open_close_trunk(void)
{
  PrintCompLine("\x85\x6a\x65\x77\x65\x6c\xa1\xbb\x9e\x73\x61\x66\x65\xec\xa8\x6e\x73\x69\xe8\x3b\x80\xa9\x27\xa1\xe3\xe4\xf3\xab\xbd\xcc\xba\xa2\x61\x74\x2e");
}



void DoMisc_open_close_large_bag(void)
{
  PrintCompLine("\x47\x65\x74\xf0\x9c\x63\xd9\xd6\xfb\xe3\x75\x67\xde\x77\xa5\x6c\xab\xef\xa3\xe6\xe9\xab\x74\xf1\x63\x6b\x2e");
}



void DoMisc_open_front_door(void)
{
  PrintCompLine("\x85\x64\xe9\xb6\xe7\x6e\xe3\xa6\xef\xae\xfc\xed\x64\x2e");
}



void DoMisc_count_matches(void)
{
  PrintCompText("\x8b\xcd\x76\x65\x20");

  if (MatchesLeft == 0) PrintCompText("\x6e\x6f");
  else PrintInteger(MatchesLeft);

  if (MatchesLeft == 1) PrintCompLine("\xee\xaf\x63\x68\x2e");
  else                  PrintCompLine("\xee\xaf\xfa\x65\x73\x2e");
}
  


void EatFood(int obj, char *msg)
{
  if (Obj[obj].loc != INSIDE + OBJ_YOU)
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xc0\x6f\x6c\x64\x84\xa2\x61\x74\x2e");
  else
  {
    PrintLine(msg);
    TimePassed = 1;
    Obj[obj].loc = 0;
  }
}



void DoMisc_eat_lunch(void)
{
  EatFood(OBJ_LUNCH, "Thank you very much. It really hit the spot.");
}



void DoMisc_eat_garlic(void)
{
  EatFood(OBJ_GARLIC, "What the heck! You won't make friends this way, but nobody around here is too friendly anyhow. Gulp!");
}
 


void DoMisc_drink_water(void)
{
  if (Room[Obj[OBJ_YOU].loc].prop & R_WATERHERE)
  {
    PrintCompLine("\xbc\xad\x6b\x86\x20\xd7\x72\xc4\x6d\x75\xfa\xa4\x49\xb7\xe0\xda\xaf\xa0\xb6\xa2\x69\x72\xc5\xc4\x28\x66\xc2\xf9\xe2\xea\xa2\x9a\x74\xe2\x6b\x97\xb5\x70\xc2\x62\x61\x62\xec\x29\x2e");
    TimePassed = 1;
  }
  else if (Obj[OBJ_BOTTLE].loc == Obj[OBJ_YOU].loc ||
           Obj[OBJ_BOTTLE].loc == INSIDE + OBJ_YOU)
  {
    if (Obj[OBJ_BOTTLE].loc != INSIDE + OBJ_YOU)
      PrintCompLine("\x8b\xcd\xd7\x89\xef\xc0\x6f\x6c\x64\x84\x81\x62\xff\x74\xcf\xc6\x69\x72\x73\x74\x2e");
    else if ((Obj[OBJ_BOTTLE].prop & PROP_OPEN) == 0)
      PrintCompLine("\xdc\x75\x27\xdf\xc0\x61\xd7\x89\x6f\xfc\xb4\x81\x62\xff\x74\xcf\xc6\x69\x72\x73\x74\x2e");
    else if (Obj[OBJ_WATER].loc != INSIDE + OBJ_BOTTLE)
      PrintCompLine("\x99\xa9\xa8\x73\x93\xad\xc4\x77\xaf\xac\xc0\xac\x65\x2e");
    else
    {
      PrintCompLine("\xbc\xad\x6b\x86\x20\xd7\x72\xc4\x6d\x75\xfa\xa4\x49\xb7\xe0\xda\xaf\xa0\xb6\xa2\x69\x72\xc5\xc4\x28\x66\xc2\xf9\xe2\xea\xa2\x9a\x74\xe2\x6b\x97\xb5\x70\xc2\x62\x61\x62\xec\x29\x2e");
      TimePassed = 1;
      Obj[OBJ_WATER].loc = 0;      
    }
  }
  else
    PrintCompLine("\x99\xa9\xa8\x73\x93\xad\xc4\x77\xaf\xac\xc0\xac\x65\x2e");
}



void DoMisc_climbdown_rope(void)
{
  if (RopeTiedToRail && Obj[OBJ_YOU].loc == ROOM_DOME_ROOM)
    GoToRoutine(ROOM_TORCH_ROOM);
  else
    PrintCompLine("\x85\xc2\xfc\xa8\x73\x93\xf0\xd5\x89\xad\x79\xa2\x97\x2e");
}



void DoMisc_break_mirror(void)
{
  if (MirrorBroken)
    PrintCompLine("\x48\x61\xd7\x93\x8f\x64\xca\x9e\xd4\xa5\x67\xde\x64\x61\x6d\x61\x67\x9e\xe2\xa9\x61\x64\x79\x3f");
  else
  {
    PrintCompLine("\x8b\xcd\xd7\xb0\xc2\x6b\xd4\x80\xee\x69\x72\xc2\x72\xa4\x49\xc0\x6f\xfc\x86\xc0\x61\xd7\xa3\xaa\x65\xd7\xb4\x79\xbf\x72\x73\x27\xaa\x75\x70\x70\xec\x8a\x67\xe9\xab\x6c\x75\x63\x6b\xc0\xad\x64\x79\x2e");
    TimePassed = 1;
    MirrorBroken = 1;
    NotLucky = 1;
  }
}



void DoMisc_lookin_mirror(void)
{
  if (MirrorBroken)
    PrintCompLine("\x85\x6d\x69\x72\xc2\xb6\x9a\x62\xc2\x6b\xd4\xa8\xe5\xba\x6d\xad\xc4\x70\x69\x65\x63\x65\x73\x2e");
  else
    PrintCompLine("\x99\xa9\x87\xad\x20\x75\x67\xec\xeb\xac\x73\xca\xaa\x74\xbb\x84\x62\x61\x63\x6b\xa3\xa6\xc9\x75\x2e");
}



void DoMisc_lookthrough_kitchen_window(void)
{
  PrintCompText("\x8b\xe7\xb4\xd6\x65\x20");
  if (Obj[OBJ_YOU].loc == ROOM_KITCHEN)
    PrintCompLine("\xd0\x63\xcf\xbb\xa3\xa9\xd0\xcf\x61\x64\x84\xbd\x77\xbb\x64\xa1\xd0\x66\xd3\xbe\x74\x2e");
  else
    PrintCompLine("\x77\xcd\xa6\x61\x70\xfc\xbb\xa1\xbd\xb0\x9e\xd0\x6b\xc7\xfa\x65\x6e\x2e");
}



void DoMisc_lookunder_rug(void)
{
  if (RugMoved == 0 && TrapOpen == 0)
  {
    PrintCompLine("\x55\xb9\xac\xed\xaf\xde\x81\x72\x75\xc1\x9a\xd0\x63\xd9\xd6\xab\x74\xf4\x70\xcc\xe9\x72\xa4\x41\xa1\x8f\x64\xc2\x70\x80\xb3\xd3\xed\xb6\xdd\x80\xda\x75\x67\xb5\x81\x74\xf4\x70\xcc\xe9\xb6\x9a\xca\x63\x9e\x61\x67\x61\xa7\xb3\xca\x63\xbf\xcf\xab\x66\xc2\xf9\x76\x69\x65\x77\x2e");
    TimePassed = 1;
  }
  else
    PrintCompLine("\x8b\xd6\x9e\xe3\xa2\x84\xf6\xe8\xb6\x69\x74\x2e");
}



void DoMisc_lookunder_leaves(void)
{
  if (GratingRevealed == 0)
  {
    PrintCompLine("\x55\xb9\xac\xed\xaf\xde\x81\x70\x69\xcf\x8a\xcf\x61\xd7\xa1\x9a\xd0\x67\xf4\xf0\xb1\xa4\x41\xa1\x8f\xa9\xcf\xe0\x9e\x81\xcf\x61\xd7\x73\xb5\x81\x67\xf4\xf0\x9c\x9a\xca\x63\x9e\x61\x67\x61\xa7\xb3\xca\x63\xbf\xcf\xab\x66\xc2\xf9\x76\x69\x65\x77\x2e");
    TimePassed = 1;
    GratingRevealed = 1;
    Obj[OBJ_LEAVES].prop |= PROP_MOVEDDESC;
  }
  else
    PrintCompLine("\x8b\xd6\x9e\xe3\xa2\x84\xf6\xe8\xb6\x81\xcf\x61\xd7\x73\x2e");
}



void DoMisc_lookunder_rainbow(void)
{
  PrintCompLine("\x85\x46\xf1\x67\x69\xab\x52\x69\xd7\xb6\x66\xd9\x77\xa1\xf6\xe8\xb6\x81\xf4\xa7\x62\x6f\x77\x2e");
}



void DoMisc_lookin_chimney(void)
{
  PrintCompText("\x85\xfa\x69\x6d\xed\xc4\xcf\x61\x64\x73\x20");
  if (Obj[OBJ_YOU].loc == ROOM_KITCHEN) PrintCompText("\x64\x6f\x77\x6e");
  else                                  PrintCompText("\x75\x70");
  PrintCompLine("\x77\xbb\x64\xb5\x8c\xd9\x6f\x6b\xa1\x63\xf5\x6d\x62\x61\x62\x6c\x65\x2e");
}



void DoMisc_examine_kitchen_window(void)
{
  if (KitchenWindowOpen == 0)
    PrintCompLine("\x85\xf8\xb9\xf2\x87\x73\xf5\x67\x68\x74\xec\xa3\x6a\xbb\xb5\x62\xf7\xe4\xff\xfb\xe3\x75\x67\xde\xbd\xa3\xdf\xf2\xfb\xe5\x72\x79\x2e");
  else
    PrintCompLine("\x49\x74\x27\xa1\x6f\xfc\x6e\x2e");
}



void DoMisc_lookin_bag_of_coins(void)
{
  PrintCompLine("\x99\xa9\xa3\xa9\xcb\xff\xa1\xdd\xb3\x6f\xa7\xa1\xa7\x80\x72\x65\x2e");
}



void DoMisc_lookin_trunk(void)
{
  PrintCompLine("\x99\xa9\xa3\xa9\xcb\xff\xa1\xdd\x20\x6a\x65\x77\x65\x6c\xa1\xa7\x80\x72\x65\x2e");
}



void DoMisc_squeeze_tube(void)
{
  if (Obj[OBJ_TUBE].loc != INSIDE + OBJ_YOU)
    PrintCompLine("\x8b\xbb\xd4\x27\xa6\x68\x6f\x6c\x64\x84\x81\x74\x75\x62\x65\x2e");
  else if ((Obj[OBJ_TUBE].prop & PROP_OPEN) == 0)
    PrintCompLine("\x85\x74\x75\xef\x87\x63\xd9\xd6\x64\x2e");
  else if (Obj[OBJ_PUTTY].loc != INSIDE + OBJ_TUBE)
    PrintCompLine("\x85\x74\x75\xef\x87\x61\x70\x70\xbb\xd4\x74\xec\xfb\x6d\x70\x74\x79\x2e");
  else
  {
    PrintCompLine("\x85\x76\xb2\x63\xa5\xa1\x6d\xaf\xac\x69\xe2\xae\x6f\x7a\xbe\xa8\xe5\xba\x92\xc0\xad\x64\x2e");
    TimePassed = 1;
    Obj[OBJ_PUTTY].loc = INSIDE + OBJ_YOU;
  }
}



void DoMisc_examine_raised_basket(void)
{
  PrintContents(OBJ_RAISED_BASKET, "It contains:", 1);
}



void DoMisc_examine_lowered_basket(void)
{
  PrintCompLine("\x85\x62\xe0\x6b\x65\xa6\x9a\xaf\x80\xae\x96\xb6\xd4\xab\xdd\x80\xaa\xcd\x66\x74\x2e");
}



void DoMisc_lookin_large_bag(void)
{
  if (ThiefDescType == 1) // unconscious
    PrintCompLine("\x85\x62\x61\xc1\x9a\xf6\xe8\x72\xed\xaf\xde\x81\xa2\x69\x65\x66\xb5\x73\xba\xca\x9e\xe7\x93\x73\x61\xc4\x77\xcd\x74\xb5\x69\xd2\xad\x79\xa2\x97\xb5\x9a\xa7\x73\x69\x64\x65\x2e");
  else
    PrintCompLine("\x47\x65\x74\xf0\x9c\x63\xd9\xd6\xfb\xe3\x75\x67\xde\x77\xa5\x6c\xab\xef\xa3\xe6\xe9\xab\x74\xf1\x63\x6b\x2e");
}



void DoMisc_lookthrough_grate(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_GRATING_CLEARING)
    PrintCompLine("\x8b\xd6\x9e\x64\xbb\x6b\xed\x73\xa1\xef\xd9\x77\x2e");
  else
    PrintCompLine("\x8b\xd6\x9e\x74\xa9\xbe\xa3\x62\x6f\xd7\x86\x2e");
}



void DoMisc_lookin_water(void)
{
  PrintCompLine("\x49\x74\x27\xa1\x63\xcf\xbb\x8d\xee\x69\x67\x68\xa6\xef\xeb\xff\x61\x62\x6c\x65\x2e");
}



void DoMisc_whereis_granite_wall(void)
{
  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_NORTH_TEMPLE:  PrintCompLine("\x85\x77\xbe\xa6\x77\xe2\xea\x9a\x73\x6f\xf5\xab\x67\xf4\x6e\xc7\x9e\xa0\x72\x65\x2e"); break;
    case ROOM_TREASURE_ROOM: PrintCompLine("\x85\xbf\xc5\xb7\xe2\xea\x9a\x73\x6f\xf5\xab\x67\xf4\x6e\xc7\x9e\xa0\x72\x65\x2e"); break;
    case ROOM_SLIDE_ROOM:    PrintCompLine("\x49\xa6\xca\xec\x20\x53\x41\x59\x53\x20\x22\x47\xf4\x6e\xc7\x9e\x57\xe2\x6c\x22\x2e");       break;
  }
}



void DoMisc_whereis_songbird(void)
{
  PrintCompLine("\x85\x73\xca\x67\x62\x69\x72\xab\x9a\xe3\xa6\xa0\xa9\xb0\xf7\x87\x70\xc2\x62\x61\x62\xec\xe4\xbf\x72\x62\x79\x2e");
}



void DoMisc_whereis_white_house(void)
{
  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_KITCHEN:
    case ROOM_LIVING_ROOM:
    case ROOM_ATTIC:
      PrintCompLine("\x57\x68\xc4\xe3\xa6\x66\xa7\xab\x92\xb0\xf4\xa7\x73\x3f"); // never printed because house is not in these locations
    break;

    case ROOM_EAST_OF_HOUSE:
    case ROOM_WEST_OF_HOUSE:
    case ROOM_NORTH_OF_HOUSE:
    case ROOM_SOUTH_OF_HOUSE:
      PrintCompLine("\x49\x74\x27\xa1\xf1\x67\x68\xa6\xa0\x72\x65\x21");
    break;

    case ROOM_CLEARING: PrintCompLine("\x49\xa6\xd6\x65\x6d\xa1\xbd\xb0\x9e\xbd\x80\xb7\xbe\x74\x2e");       break;
    default:            PrintCompLine("\x49\xa6\x77\xe0\xc0\xac\x9e\x6a\xfe\xa6\xd0\x6d\xa7\xf7\x9e\x61\x67\x6f\x2e\x2e\x2e\x2e"); break;
  }
}



void DoMisc_whereis_forest(void)
{
  PrintCompLine("\x8b\xe7\x6e\xe3\xa6\xd6\x9e\x81\x66\xd3\xbe\xa6\x66\xd3\x80\x9f\xa9\x65\x73\x2e");
}



void DoMisc_read_granite_wall(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_SLIDE_ROOM)
    PrintCompLine("\x49\xa6\xca\xec\x20\x53\x41\x59\x53\x20\x22\x47\xf4\x6e\xc7\x9e\x57\xe2\x6c\x22\x2e");
  else
    PrintCompLine("\x99\xa9\x27\xa1\xe3\xa2\x84\xbd\xda\xbf\x64\x2e");
}



void DoMisc_examine_zorkmid(void)
{
  PrintCompLine("\x85\x7a\xd3\x6b\x6d\x69\xab\x9a\x81\xf6\xc7\x8a\x63\xd8\xa9\x6e\x63\xc4\xdd\x80\x20\x47\xa9\xaf\x20\x55\xb9\xac\x67\xc2\xf6\xab\x45\x6d\x70\x69\x72\x65\x2e");
}



void DoMisc_examine_grue(void)
{
  PrintCompLine("\x85\x67\x72\x75\x9e\x9a\xd0\x73\xa7\xb2\xd1\x72\xb5\x6c\xd8\x6b\x84\x70\xa9\xd6\x6e\x63\x9e\xa7\x80\xcc\xbb\x6b\xeb\xfd\x63\xbe\x8a\x81\xbf\x72\xa2\xa4\x49\x74\xa1\x66\x61\x76\xd3\xc7\x9e\x64\x69\x65\xa6\x9a\x61\x64\xd7\xe5\xd8\xac\x73\xb5\x62\xf7\xa8\x74\xa1\xa7\x73\xaf\x69\x61\x62\xcf\xa3\x70\xfc\xf0\xd1\x87\xd1\x6d\xfc\xa9\xab\x62\xc4\xc7\xa1\x66\xbf\xb6\xdd\xcb\x69\x67\x68\x74\xa4\x4e\xba\x67\x72\x75\x9e\xcd\xa1\x65\xd7\xb6\xef\xd4\xaa\xf3\xb4\x62\xc4\x81\xf5\x67\x68\xa6\xdd\xcc\x61\x79\xb5\x8c\x66\x65\x77\xc0\x61\xd7\xaa\xd8\x76\x69\xd7\xab\xc7\xa1\x66\xbf\x72\x73\xe1\x9e\x6a\x61\x77\xa1\xbd\x9f\x65\xdf\x80\x9f\xe2\x65\x2e");
}



void DoMisc_whereis_zorkmid(void)
{
  PrintCompLine("\x85\xef\xc5\xb7\x61\xc4\xbd\xc6\xa7\xab\x7a\xd3\x6b\x6d\x69\x64\xa1\x9a\xbd\xe6\xba\xa5\xa6\x8c\xd9\x6f\x6b\xc6\xd3\x80\x6d\x2e");
}



void DoMisc_whereis_grue(void)
{
  PrintCompLine("\x99\xa9\x87\xe3\xe6\x72\x75\x9e\xa0\xa9\xb5\x62\xf7\x20\x49\x27\xf9\x73\xd8\x9e\x96\xa9\x87\xaf\xcb\xbf\xc5\xae\xed\xcb\xd8\x6b\x84\xa7\x80\xcc\xbb\x6b\xed\x73\xa1\xed\xbb\x62\x79\xa4\x49\xb7\xa5\x6c\x64\x93\xcf\xa6\x6d\xc4\xf5\x67\x68\xa6\x67\xba\xa5\xa6\x69\xd2\x49\xb7\xac\x9e\xc9\x75\x21");
}



void DoMisc_listento_troll(void)
{
  PrintCompLine("\x45\xd7\x72\xc4\x73\xba\xdd\xd1\xb4\x81\x74\xc2\xdf\xaa\x61\x79\xa1\x73\xe1\x65\xa2\x97\xb5\x70\xc2\x62\x61\x62\xec\x20\xf6\x63\xe1\x70\xf5\x6d\xd4\x74\xbb\x79\xb5\xa7\xc0\x9a\x67\xf7\x74\xd8\xe2\x9f\xca\x67\x75\x65\x2e");
}



void DoMisc_listento_thief(void)
{
  PrintCompLine("\x85\xa2\x69\x65\xd2\x73\x61\x79\xa1\xe3\xa2\x97\xb5\xe0\x86\xc0\x61\xd7\xe4\xff\xb0\xf3\xb4\x66\xd3\x6d\xe2\xec\xa8\xe5\xc2\x64\x75\x63\x65\x64\x2e");
}



void DoMisc_listento_cyclops(void)
{
  PrintCompLine("\x8b\xe7\xb4\xa0\xbb\xc0\x9a\xc5\xe1\x61\xfa\xda\x75\x6d\x62\xf5\x6e\x67\x2e");
}



void DoMisc_listento_forest(void)
{
  PrintCompLine("\x85\x70\xa7\xbe\x8d\x80\xc0\x65\x6d\xd9\x63\x6b\xa1\xd6\x65\xf9\xbd\xb0\x9e\x6d\xd8\x6d\xd8\x97\x2e");
}



void DoMisc_listento_songbird(void)
{
  PrintCompLine("\x8b\xe7\x93\xa0\xbb\x80\xaa\xca\x67\x62\x69\x72\xab\xe3\x77\x2e");
}



void DoMisc_cross_rainbow(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_CANYON_VIEW)
    PrintCompLine("\x46\xc2\xf9\xa0\xa9\x3f\x21\x3f");
  else if (RainbowSolid == 0)
    PrintCompLine("\x43\xad\x86\xb7\xe2\x6b\xae\xb4\x77\xaf\xac\x20\x76\x61\x70\x6f\x72\x3f");
  else
  {
    if (Obj[OBJ_YOU].loc == ROOM_ARAGAIN_FALLS)
      GoToRoutine(ROOM_END_OF_RAINBOW);
    else if (Obj[OBJ_YOU].loc == ROOM_END_OF_RAINBOW)
      GoToRoutine(ROOM_ARAGAIN_FALLS);
    else
      PrintCompLine("\xdc\x75\x27\xdf\xc0\x61\xd7\x89\x73\x61\xc4\x77\xce\xfa\xb7\x61\x79\x2e\x2e\x2e");
  }    
}



void DoMisc_cross_lake(void)
{
  PrintCompLine("\x49\x74\x27\xa1\xbd\xba\xf8\xe8\x89\x63\xc2\x73\x73\x2e");
}



void DoMisc_cross_stream(void)
{
  PrintCompLine("\x85\xff\xa0\xb6\x73\x69\xe8\x87\xd0\x73\xa0\xac\xda\x6f\x63\x6b\xb3\xf5\x66\x66\x2e");
}



void DoMisc_cross_chasm(void)
{
  PrintCompLine("\x49\x74\x27\xa1\xbd\xba\x66\xbb\x89\x6a\x75\x6d\x70\xb5\x8c\x96\xa9\x27\xa1\xe3\xb0\xf1\x64\x67\x65\x2e");
}



void DoMisc_exorcise_ghosts(void)
{
  if (SpiritsBanished == 0 &&
      Obj[OBJ_BELL].loc == INSIDE + OBJ_YOU &&
      Obj[OBJ_BOOK].loc == INSIDE + OBJ_YOU &&
      Obj[OBJ_CANDLES].loc == INSIDE + OBJ_YOU)
    PrintCompLine("\x8b\x6d\xfe\xa6\xfc\x72\x66\xd3\xf9\x81\x63\xac\x65\x6d\xca\x79\x2e");
  else
    PrintCompLine("\x8b\xbb\xd4\x27\xa6\x65\x71\x75\x69\x70\xfc\xab\x66\xd3\xa3\xb4\x65\x78\xd3\x63\xb2\x6d\x2e");
}



void DoMisc_raise_rug(void)
{
  PrintCompText("\x85\x72\x75\xc1\x9a\xbd\xba\xa0\x61\x76\xc4\xbd\xcb\x69\x66\x74");
  if (RugMoved)
    PrintCompLine("\x2e");
  else
    PrintCompLine("\xb5\x62\xf7\xa8\xb4\x74\x72\x79\x84\xbd\x9f\x61\x6b\x9e\xc7\x86\xc0\x61\xd7\xe4\xff\x69\x63\xd5\xa3\xb4\x69\x72\xa9\x67\x75\xfd\xf1\x74\xc4\xef\xed\xaf\xde\x69\x74\x2e");
}



void DoMisc_raise_trap_door(void)
{
  DoMisc_open_trap_door();
}



void DoMisc_smell_gas(void)
{
  PrintCompLine("\x49\xa6\x73\x6d\x65\xdf\xa1\xf5\x6b\x9e\x63\x6f\xe2\xe6\xe0\xa8\xb4\xa0\x72\x65\x2e");
}



void DoMisc_smell_sandwich_bag(void)
{
  if (Obj[OBJ_LUNCH].loc == INSIDE + OBJ_SANDWICH_BAG)
    PrintCompLine("\x49\xa6\x73\x6d\x65\xdf\xa1\xdd\xc0\xff\xeb\x65\x70\xfc\x72\x73\x2e");
  else
    PrintCompLine("\x49\xa6\x73\x6d\x65\xdf\xa1\xe0\x86\xb7\xa5\x6c\xab\x65\x78\xfc\x63\x74\x2e");
}



struct DOMISC_STRUCT DoMisc[] =
{
  { A_OPEN         , FOBJ_KITCHEN_WINDOW  , DoMisc_open_kitchen_window         },
  { A_CLOSE        , FOBJ_KITCHEN_WINDOW  , DoMisc_close_kitchen_window        },
  { A_MOVE         , FOBJ_RUG             , DoMisc_move_push_rug               },
  { A_PUSH         , FOBJ_RUG             , DoMisc_move_push_rug               },
  { A_OPEN         , FOBJ_TRAP_DOOR       , DoMisc_open_trap_door              },
  { A_CLOSE        , FOBJ_TRAP_DOOR       , DoMisc_close_trap_door             },
  { A_RAISE        , OBJ_RAISED_BASKET    , DoMisc_raise_basket                },
  { A_RAISE        , OBJ_LOWERED_BASKET   , DoMisc_raise_basket                },
  { A_LOWER        , OBJ_RAISED_BASKET    , DoMisc_lower_basket                },
  { A_LOWER        , OBJ_LOWERED_BASKET   , DoMisc_lower_basket                },
  { A_PUSH         , FOBJ_BLUE_BUTTON     , DoMisc_push_blue_button            },
  { A_PUSH         , FOBJ_RED_BUTTON      , DoMisc_push_red_button             },
  { A_PUSH         , FOBJ_BROWN_BUTTON    , DoMisc_push_brown_button           },
  { A_PUSH         , FOBJ_YELLOW_BUTTON   , DoMisc_push_yellow_button          },
  { A_ENTER        , OBJ_INFLATED_BOAT    , DoMisc_enter_inflated_boat         },
  { A_EXIT         , OBJ_INFLATED_BOAT    , DoMisc_exit_inflated_boat          },
  { A_MOVE         , OBJ_LEAVES           , DoMisc_move_leaves                 },
  { A_OPEN         , FOBJ_GRATE           , DoMisc_open_grate                  },
  { A_CLOSE        , FOBJ_GRATE           , DoMisc_close_grate                 },
  { A_RING         , OBJ_BELL             , DoMisc_ring_bell                   },
  { A_WIND         , OBJ_CANARY           , DoMisc_wind_canary                 },
  { A_WIND         , OBJ_BROKEN_CANARY    , DoMisc_wind_broken_canary          },
  { A_WAVE         , OBJ_SCEPTRE          , DoMisc_wave_sceptre                },
  { A_RAISE        , OBJ_SCEPTRE          , DoMisc_raise_sceptre               },
  { A_TOUCH        , FOBJ_MIRROR1         , DoMisc_touch_mirror                },
  { A_TOUCH        , FOBJ_MIRROR2         , DoMisc_touch_mirror                },
  { A_READ         , OBJ_BOOK             , DoMisc_read_book                   },
  { A_READ         , OBJ_ADVERTISEMENT    , DoMisc_read_advertisement          },
  { A_READ         , OBJ_MATCH            , DoMisc_read_match                  },
  { A_READ         , OBJ_MAP              , DoMisc_read_map                    },
  { A_READ         , OBJ_BOAT_LABEL       , DoMisc_read_boat_label             },
  { A_READ         , OBJ_GUIDE            , DoMisc_read_guide                  },
  { A_READ         , OBJ_TUBE             , DoMisc_read_tube                   },
  { A_READ         , OBJ_OWNERS_MANUAL    , DoMisc_read_owners_manual          },
  { A_READ         , FOBJ_PRAYER          , DoMisc_read_prayer                 },
  { A_READ         , FOBJ_WOODEN_DOOR     , DoMisc_read_wooden_door            },
  { A_READ         , OBJ_ENGRAVINGS       , DoMisc_read_engravings             },
  { A_OPEN         , OBJ_EGG              , DoMisc_open_egg                    },
  { A_BREAK        , OBJ_EGG              , DoMisc_open_egg                    },
  { A_PRY          , OBJ_EGG              , DoMisc_open_egg                    },
  { A_CLIMBTHROUGH , FOBJ_KITCHEN_WINDOW  , DoMisc_climbthrough_kitchen_window },
  { A_ENTER        , FOBJ_KITCHEN_WINDOW  , DoMisc_climbthrough_kitchen_window },
  { A_EXIT         , FOBJ_KITCHEN_WINDOW  , DoMisc_climbthrough_kitchen_window },
  { A_CLIMBTHROUGH , FOBJ_TRAP_DOOR       , DoMisc_climbthrough_trap_door      },
  { A_ENTER        , FOBJ_TRAP_DOOR       , DoMisc_climbthrough_trap_door      },
  { A_CLIMBTHROUGH , FOBJ_GRATE           , DoMisc_climbthrough_grate          },
  { A_ENTER        , FOBJ_GRATE           , DoMisc_climbthrough_grate          },
  { A_CLIMBTHROUGH , FOBJ_SLIDE           , DoMisc_climbthrough_slide          },
  { A_ENTER        , FOBJ_SLIDE           , DoMisc_climbthrough_slide          },
  { A_CLIMBTHROUGH , FOBJ_CHIMNEY         , DoMisc_climbthrough_chimney        },
  { A_ENTER        , FOBJ_CHIMNEY         , DoMisc_climbthrough_chimney        },
  { A_CLIMBTHROUGH , FOBJ_BARROW_DOOR     , DoMisc_climbthrough_barrow_door    },
  { A_ENTER        , FOBJ_BARROW_DOOR     , DoMisc_climbthrough_barrow_door    },
  { A_ENTER        , FOBJ_BARROW          , DoMisc_climbthrough_barrow_door    },
  { A_CLIMBTHROUGH , FOBJ_GATE            , DoMisc_climbthrough_gate           },
  { A_ENTER        , FOBJ_GATE            , DoMisc_climbthrough_gate           },
  { A_CLIMBTHROUGH , FOBJ_CRACK           , DoMisc_climbthrough_crack          },
  { A_ENTER        , FOBJ_CRACK           , DoMisc_climbthrough_crack          },
  { A_ENTER        , FOBJ_WHITE_HOUSE     , DoMisc_enter_white_house           },
  { A_SLIDEDOWN    , FOBJ_SLIDE           , DoMisc_slidedown_slide             },
  { A_CLIMBUP      , FOBJ_MOUNTAIN_RANGE  , DoMisc_climbup_mountain_range      },
  { A_CLIMB        , FOBJ_MOUNTAIN_RANGE  , DoMisc_climbup_mountain_range      },
  { A_CLIMBUP      , FOBJ_WHITE_CLIFF     , DoMisc_climbup_white_cliff         },
  { A_CLIMB        , FOBJ_WHITE_CLIFF     , DoMisc_climbup_white_cliff         },
  { A_CLIMBUP      , FOBJ_TREE            , DoMisc_climbup_tree                },
  { A_CLIMB        , FOBJ_TREE            , DoMisc_climbup_tree                },
  { A_CLIMBDOWN    , FOBJ_TREE            , DoMisc_climbdown_tree              },
  { A_CLIMBUP      , FOBJ_CHIMNEY         , DoMisc_climbup_chimney             },
  { A_CLIMB        , FOBJ_CHIMNEY         , DoMisc_climbup_chimney             },
  { A_CLIMBDOWN    , FOBJ_CHIMNEY         , DoMisc_climbdown_chimney           },
  { A_CLIMBUP      , FOBJ_LADDER          , DoMisc_climbup_ladder              },
  { A_CLIMB        , FOBJ_LADDER          , DoMisc_climbup_ladder              },
  { A_CLIMBDOWN    , FOBJ_LADDER          , DoMisc_climbdown_ladder            },
  { A_CLIMBUP      , FOBJ_SLIDE           , DoMisc_climbup_slide               },
  { A_CLIMB        , FOBJ_SLIDE           , DoMisc_climbup_slide               },
  { A_CLIMBDOWN    , FOBJ_SLIDE           , DoMisc_climbdown_slide             },
  { A_CLIMBUP      , FOBJ_CLIMBABLE_CLIFF , DoMisc_climbup_climbable_cliff     },
  { A_CLIMB        , FOBJ_CLIMBABLE_CLIFF , DoMisc_climbup_climbable_cliff     },
  { A_CLIMBDOWN    , FOBJ_CLIMBABLE_CLIFF , DoMisc_climbdown_climbable_cliff   },
  { A_CLIMBUP      , FOBJ_STAIRS          , DoMisc_climbup_stairs              },
  { A_CLIMB        , FOBJ_STAIRS          , DoMisc_climbup_stairs              },
  { A_CLIMBDOWN    , FOBJ_STAIRS          , DoMisc_climbdown_stairs            },
  { A_EXAMINE      , OBJ_SWORD            , DoMisc_examine_sword               },
  { A_EXAMINE      , OBJ_MATCH            , DoMisc_examine_match               },
  { A_EXAMINE      , OBJ_CANDLES          , DoMisc_examine_candles             },
  { A_EXAMINE      , OBJ_TORCH            , DoMisc_examine_torch               },
  { A_EXAMINE      , OBJ_THIEF            , DoMisc_examine_thief               },
  { A_EXAMINE      , OBJ_TOOL_CHEST       , DoMisc_examine_tool_chest          },
  { A_EXAMINE      , FOBJ_BOARD           , DoMisc_examine_board               },
  { A_EXAMINE      , FOBJ_CHAIN           , DoMisc_examine_chain               },
  { A_OPEN         , OBJ_TOOL_CHEST       , DoMisc_open_tool_chest             },
  { A_OPEN         , OBJ_BOOK             , DoMisc_open_book                   },
  { A_CLOSE        , OBJ_BOOK             , DoMisc_close_book                  },
  { A_OPEN         , FOBJ_BOARDED_WINDOW  , DoMisc_open_boarded_window         },
  { A_BREAK        , FOBJ_BOARDED_WINDOW  , DoMisc_break_boarded_window        },
  { A_OPEN         , FOBJ_DAM             , DoMisc_open_close_dam              },
  { A_CLOSE        , FOBJ_DAM             , DoMisc_open_close_dam              },
  { A_RING         , OBJ_HOT_BELL         , DoMisc_ring_hot_bell               },
  { A_READ         , FOBJ_YELLOW_BUTTON   , DoMisc_read_button                 },
  { A_READ         , FOBJ_BROWN_BUTTON    , DoMisc_read_button                 },
  { A_READ         , FOBJ_RED_BUTTON      , DoMisc_read_button                 },
  { A_READ         , FOBJ_BLUE_BUTTON     , DoMisc_read_button                 },
  { A_RAISE        , FOBJ_GRANITE_WALL    , DoMisc_raise_lower_granite_wall    },
  { A_LOWER        , FOBJ_GRANITE_WALL    , DoMisc_raise_lower_granite_wall    },
  { A_RAISE        , FOBJ_CHAIN           , DoMisc_raise_lower_chain           },
  { A_LOWER        , FOBJ_CHAIN           , DoMisc_raise_lower_chain           },
  { A_MOVE         , FOBJ_CHAIN           , DoMisc_move_chain                  },
  { A_COUNT        , OBJ_CANDLES          , DoMisc_count_candles               },
  { A_COUNT        , OBJ_LEAVES           , DoMisc_count_leaves                },
  { A_EXAMINE      , OBJ_LAMP             , DoMisc_examine_lamp                },
  { A_EXAMINE      , OBJ_TROLL            , DoMisc_examine_troll               },
  { A_EXAMINE      , OBJ_CYCLOPS          , DoMisc_examine_cyclops             },
  { A_EXAMINE      , FOBJ_WHITE_HOUSE     , DoMisc_examine_white_house         },
  { A_OPEN         , FOBJ_BARROW_DOOR     , DoMisc_open_close_barrow_door      },
  { A_CLOSE        , FOBJ_BARROW_DOOR     , DoMisc_open_close_barrow_door      },
  { A_OPEN         , FOBJ_STUDIO_DOOR     , DoMisc_open_close_studio_door      },
  { A_CLOSE        , FOBJ_STUDIO_DOOR     , DoMisc_open_close_studio_door      },
  { A_OPEN         , OBJ_BAG_OF_COINS     , DoMisc_open_close_bag_of_coins     },
  { A_CLOSE        , OBJ_BAG_OF_COINS     , DoMisc_open_close_bag_of_coins     },
  { A_OPEN         , OBJ_TRUNK            , DoMisc_open_close_trunk            },
  { A_CLOSE        , OBJ_TRUNK            , DoMisc_open_close_trunk            },
  { A_OPEN         , OBJ_LARGE_BAG        , DoMisc_open_close_large_bag        },
  { A_CLOSE        , OBJ_LARGE_BAG        , DoMisc_open_close_large_bag        },
  { A_OPEN         , FOBJ_FRONT_DOOR      , DoMisc_open_front_door             },
  { A_COUNT        , OBJ_MATCH            , DoMisc_count_matches               },
  { A_OPEN         , OBJ_MATCH            , DoMisc_count_matches               },
  { A_EAT          , OBJ_LUNCH            , DoMisc_eat_lunch                   },
  { A_EAT          , OBJ_GARLIC           , DoMisc_eat_garlic                  },
  { A_DRINK        , OBJ_WATER            , DoMisc_drink_water                 },
  { A_CLIMBDOWN    , OBJ_ROPE             , DoMisc_climbdown_rope              },
  { A_BREAK        , FOBJ_MIRROR1         , DoMisc_break_mirror                },
  { A_BREAK        , FOBJ_MIRROR2         , DoMisc_break_mirror                },
  { A_LOOKIN       , FOBJ_MIRROR1         , DoMisc_lookin_mirror               },
  { A_LOOKIN       , FOBJ_MIRROR2         , DoMisc_lookin_mirror               },
  { A_EXAMINE      , FOBJ_MIRROR1         , DoMisc_lookin_mirror               },
  { A_EXAMINE      , FOBJ_MIRROR2         , DoMisc_lookin_mirror               },
  { A_LOOKTHROUGH  , FOBJ_KITCHEN_WINDOW  , DoMisc_lookthrough_kitchen_window  },
  { A_LOOKIN       , FOBJ_KITCHEN_WINDOW  , DoMisc_lookthrough_kitchen_window  },
  { A_LOOKUNDER    , FOBJ_RUG             , DoMisc_lookunder_rug               },
  { A_LOOKUNDER    , OBJ_LEAVES           , DoMisc_lookunder_leaves            },
  { A_LOOKUNDER    , FOBJ_RAINBOW         , DoMisc_lookunder_rainbow           },
  { A_LOOKIN       , FOBJ_CHIMNEY         , DoMisc_lookin_chimney              },
  { A_EXAMINE      , FOBJ_CHIMNEY         , DoMisc_lookin_chimney              },
  { A_EXAMINE      , FOBJ_KITCHEN_WINDOW  , DoMisc_examine_kitchen_window      },
  { A_LOOKIN       , OBJ_BAG_OF_COINS     , DoMisc_lookin_bag_of_coins         },
  { A_EXAMINE      , OBJ_BAG_OF_COINS     , DoMisc_lookin_bag_of_coins         },
  { A_LOOKIN       , OBJ_TRUNK            , DoMisc_lookin_trunk                },
  { A_EXAMINE      , OBJ_TRUNK            , DoMisc_lookin_trunk                },
  { A_SQUEEZE      , OBJ_TUBE             , DoMisc_squeeze_tube                },
  { A_EXAMINE      , OBJ_RAISED_BASKET    , DoMisc_examine_raised_basket       },
  { A_EXAMINE      , OBJ_LOWERED_BASKET   , DoMisc_examine_lowered_basket      },
  { A_LOOKIN       , OBJ_LARGE_BAG        , DoMisc_lookin_large_bag            },
  { A_EXAMINE      , OBJ_LARGE_BAG        , DoMisc_lookin_large_bag            },
  { A_LOOKTHROUGH  , FOBJ_GRATE           , DoMisc_lookthrough_grate           },
  { A_LOOKIN       , FOBJ_GRATE           , DoMisc_lookthrough_grate           },
  { A_EXAMINE      , OBJ_WATER            , DoMisc_lookin_water                },
  { A_LOOKIN       , OBJ_WATER            , DoMisc_lookin_water                },
  { A_WHEREIS      , FOBJ_GRANITE_WALL    , DoMisc_whereis_granite_wall        },
  { A_WHEREIS      , FOBJ_SONGBIRD        , DoMisc_whereis_songbird            },
  { A_WHEREIS      , FOBJ_WHITE_HOUSE     , DoMisc_whereis_white_house         },
  { A_WHEREIS      , FOBJ_FOREST          , DoMisc_whereis_forest              },
  { A_READ         , FOBJ_GRANITE_WALL    , DoMisc_read_granite_wall           },
  { A_EXAMINE      , OBJ_ZORKMID          , DoMisc_examine_zorkmid             },
  { A_EXAMINE      , OBJ_GRUE             , DoMisc_examine_grue                },
  { A_WHEREIS      , OBJ_ZORKMID          , DoMisc_whereis_zorkmid             },
  { A_WHEREIS      , OBJ_GRUE             , DoMisc_whereis_grue                },
  { A_LISTENTO     , OBJ_TROLL            , DoMisc_listento_troll              },
  { A_LISTENTO     , OBJ_THIEF            , DoMisc_listento_thief              },
  { A_LISTENTO     , OBJ_CYCLOPS          , DoMisc_listento_cyclops            },
  { A_LISTENTO     , FOBJ_FOREST          , DoMisc_listento_forest             },
  { A_LISTENTO     , FOBJ_SONGBIRD        , DoMisc_listento_songbird           },
  { A_CROSS        , FOBJ_RAINBOW         , DoMisc_cross_rainbow               },
  { A_CROSS        , FOBJ_LAKE            , DoMisc_cross_lake                  },
  { A_CROSS        , FOBJ_STREAM          , DoMisc_cross_stream                },
  { A_CROSS        , FOBJ_CHASM           , DoMisc_cross_chasm                 },
  { A_EXORCISE     , OBJ_GHOSTS           , DoMisc_exorcise_ghosts             },
  { A_RAISE        , FOBJ_RUG             , DoMisc_raise_rug                   },
  { A_RAISE        , FOBJ_TRAP_DOOR       , DoMisc_raise_trap_door             },
  { A_SMELL        , FOBJ_GAS             , DoMisc_smell_gas                   },
  { A_SMELL        , OBJ_SANDWICH_BAG     , DoMisc_smell_sandwich_bag          },

  { 0, 0, 0 }
};
//*****************************************************************************



//*****************************************************************************
void PrintRandomFun(void)
{
  switch (GetRandom(4))
  {
    case 0: PrintCompLine("\x56\xac\xc4\x67\xe9\x64\xa4\x4e\xf2\x86\x91\xe6\xba\xbd\x80\xaa\x65\x63\xca\xab\x67\xf4\x64\x65\x2e"); break;
    case 1: PrintCompLine("\x41\xa9\x86\xfb\x6e\x6a\x6f\x79\x84\x92\xd6\x6c\x66\x3f");                     break;
    case 2: PrintCompLine("\x57\xa0\xf3\xf3\xf3\xf3\x65\x21\x21\x21\x21\x21");                              break;
    case 3: PrintCompLine("\x44\xba\x8f\x65\x78\xfc\x63\xa6\x6d\x9e\xbd\xa3\x70\x70\xfd\x75\x64\x3f");                   break;
  }
}



void PrintRandomJumpDeath(void)
{
  switch (GetRandom(3))
  {
    case 0: PrintCompLine("\x8b\x73\x68\xa5\x6c\xab\xcd\xd7\xcb\xe9\x6b\xd5\xb0\x65\x66\xd3\x9e\x8f\xcf\x61\xfc\x64\x2e");                   break;
    case 1: PrintCompLine("\x49\xb4\x81\x6d\x6f\x76\x69\xbe\xb5\x92\xcb\x69\x66\x9e\x77\xa5\x6c\xab\xef\xeb\xe0\x73\x84\xef\x66\xd3\x9e\x92\xfb\x79\x65\x73\x2e"); break;
    case 2: PrintCompLine("\x47\xac\xca\x69\x6d\x6f\x2e\x2e\x2e");                                                 break;
  }
}



void DoJump(void)
{
  int obj = 0;

  if (MatchCurWord("across") || MatchCurWord("from") || MatchCurWord("in") ||
      MatchCurWord("into") || MatchCurWord("off") || MatchCurWord("over"))
  {
    obj = GetAllObjFromInput(Obj[OBJ_YOU].loc); if (obj <= 0) return;

    if (obj == FOBJ_SCENERY_NOTVIS || obj == FOBJ_NOTVIS)
      {PrintCompLine("\xbc\xaf\xa8\x73\x93\x76\xb2\x69\x62\xcf\xc0\xac\x65\x21"); return;}
    else if (obj == FOBJ_AMB)
      {PrintCompLine("\x8b\xed\xd5\x89\xef\xee\xd3\x9e\x73\xfc\x63\x69\x66\x69\x63\x2e"); return;}
    else if (obj == OBJ_YOU)
      {PrintCompLine("\x53\xac\x69\xa5\x73\xec\x3f\x21"); return;}
  }

  if (obj == 0 || obj >= NUM_OBJECTS)
  {
    switch (Obj[OBJ_YOU].loc)
    {
      case ROOM_KITCHEN:
      case ROOM_EAST_OF_CHASM:
      case ROOM_RESERVOIR:
      case ROOM_CHASM_ROOM:
      case ROOM_DOME_ROOM:
      case ROOM_SOUTH_TEMPLE:
      case ROOM_ARAGAIN_FALLS:
      case ROOM_SHAFT_ROOM:
        PrintCompLine("\xbc\x9a\x77\xe0\xe4\xff\xa3\x20\xd7\x72\xc4\x73\x61\x66\x9e\x70\xfd\x63\x9e\xbd\x9f\x72\xc4\x6a\x75\x6d\x70\x97\x2e");
        PrintRandomJumpDeath();
        YoureDead(); // ##### RIP #####
      break;

      case ROOM_UP_A_TREE:
        PrintCompLine("\x49\xb4\xd0\x66\xbf\xa6\xdd\x20\xf6\x61\x63\x63\xfe\xbd\x6d\xd5\xcc\xbb\x97\xb5\x8f\x6d\xad\x61\x67\x9e\xbd\xcb\x8c\xca\x86\xb6\x66\xf3\xa6\xf8\xa2\xa5\xa6\x6b\x69\xdf\x84\x92\xd6\x6c\x66\x2e\x0a");
        GoToRoutine(ROOM_PATH);
      break;

      default:
        PrintRandomFun();
      break;
    }
  }
  else if (Obj[obj].loc == Obj[OBJ_YOU].loc)
  {
    if (Obj[obj].prop & PROP_ACTOR)
      PrintCompLine("\x49\xa6\x9a\xbd\xba\x62\x69\xc1\xbd\x20\x6a\x75\x6d\x70\xae\xd7\x72\x2e");
    else
      PrintRandomFun();
  }
  else
    PrintCompLine("\xbc\xaf\xb7\xa5\x6c\xab\xef\xa3\xe6\xe9\xab\x74\xf1\x63\x6b\x2e");
}



void DoSleep(void)
{
  PrintCompLine("\x99\xa9\x27\xa1\xe3\xa2\x84\xbd\xaa\xcf\x65\x70\xae\x6e\x2e");
}



void DoDisembark(void)
{
  if (YouAreInBoat == 0)
    PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xa3\x62\x6f\xbb\xab\xad\x79\xa2\x97\x21");
  else
    DoMisc_exit_inflated_boat();
}



void BoatGoToRoutine(int newroom)
{
  int prev_darkness;

  if ((Room[newroom].prop & R_BODYOFWATER) == 0)
    PrintCompLine("\x85\x6d\x61\x67\x69\x63\xb0\x6f\xaf\xb3\xe1\xbe\x89\xd0\xa9\xc5\xae\xb4\x81\x73\x68\xd3\x65\x2e\x0a");

  Obj[OBJ_INFLATED_BOAT].loc = newroom;

  prev_darkness = IsPlayerInDarkness();

  Obj[OBJ_YOU].loc = newroom;
  TimePassed = 1;

  if (IsPlayerInDarkness())
  {
    if (prev_darkness)
    {
      //kill player that tried to go from dark to dark
      PrintCompLine("\x0a\x0a\x0a\x0a\x0a\x4f\x68\xb5\xe3\x21\x88\xc0\x61\xd7\xb7\xe2\x6b\xd5\xa8\xe5\xba\x81\x73\xfd\xd7\xf1\x9c\x66\xad\x67\xa1\xdd\xa3\xcb\xd8\x6b\x84\x67\x72\x75\x65\x21");
      YoureDead(); // ##### RIP #####
      return;
    }
    else PrintCompLine("\x8b\xcd\xd7\xee\x6f\xd7\xab\xa7\xbd\xa3\xcc\xbb\x6b\xeb\xfd\x63\x65\x2e");
  }

  PrintPlayerRoomDesc(0);
}



void DoLaunch(void)
{
  int i;
  int launch_from[8] = {ROOM_DAM_BASE, ROOM_WHITE_CLIFFS_NORTH, ROOM_WHITE_CLIFFS_SOUTH, ROOM_SHORE,
                        ROOM_SANDY_BEACH, ROOM_RESERVOIR_SOUTH, ROOM_RESERVOIR_NORTH, ROOM_STREAM_VIEW};
  int   launch_to[8] = {ROOM_RIVER_1, ROOM_RIVER_3, ROOM_RIVER_4, ROOM_RIVER_5, ROOM_RIVER_4, ROOM_RESERVOIR,
                        ROOM_RESERVOIR, ROOM_IN_STREAM};

  if (Room[Obj[OBJ_YOU].loc].prop & R_BODYOFWATER)
  {
    PrintCompText("\x8b\xbb\x9e\xca\x80\x20");
    if (Obj[OBJ_YOU].loc == ROOM_RESERVOIR)
      PrintCompText("\xa9\xd6\x72\x76\x6f\x69\x72");
    else if (Obj[OBJ_YOU].loc == ROOM_IN_STREAM)
      PrintCompText("\xc5\xa9\x61\x6d");
    else
      PrintCompText("\xf1\x76\x65\x72");
    PrintCompLine("\xb5\xd3\xc0\x61\xd7\x86\xc6\xd3\x67\xff\xd1\x6e\x3f");
    return;
  }

  if (YouAreInBoat == 0) {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xa8\xb4\x81\x62\x6f\x61\x74\x21"); return;}

  for (i=0; i<8; i++)
    if (Obj[OBJ_YOU].loc == launch_from[i]) break;
  if (i == 8) {PrintCompLine("\x8b\xe7\x93\xfd\xf6\xfa\xa8\xa6\xa0\x72\x65\x2e"); return;}

  DownstreamCounter = -1; // start at -1 to account for this turn
  BoatGoToRoutine(launch_to[i]);
}



void DoLand(void)
{
  if ((Room[Obj[OBJ_YOU].loc].prop & R_BODYOFWATER) == 0)
    {PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xae\xb4\x81\x77\xaf\x65\x72\x21"); return;}

  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_RESERVOIR : PrintCompLine("\x8b\xe7\xb4\xfd\xb9\xfb\xc7\xa0\xb6\xbd\x80\xe4\xd3\xa2\xae\xb6\x81\x73\xa5\x74\x68\x2e"); break;
    case ROOM_RIVER_2   : PrintCompLine("\x99\xa9\x87\xe3\xaa\x61\x66\x9e\xfd\xb9\x84\x73\x70\xff\xc0\xac\x65\x2e");            break;
    case ROOM_RIVER_4   : PrintCompLine("\x8b\xe7\xb4\xfd\xb9\xfb\xc7\xa0\xb6\xbd\x80\xfb\xe0\xa6\xd3\x80\xb7\xbe\x74\x2e");   break;

    case ROOM_IN_STREAM : BoatGoToRoutine(ROOM_STREAM_VIEW       ); break;
    case ROOM_RIVER_1   : BoatGoToRoutine(ROOM_DAM_BASE          ); break;
    case ROOM_RIVER_3   : BoatGoToRoutine(ROOM_WHITE_CLIFFS_NORTH); break;
    case ROOM_RIVER_5   : BoatGoToRoutine(ROOM_SHORE             ); break;

    default: PrintCompLine("\xdc\x75\x27\xa9\xe4\xff\xae\xb4\x81\x77\xaf\x65\x72\x21"); break;
  }
}



void DoEcho(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_LOUD_ROOM &&
      LoudRoomQuiet == 0 &&
      (GatesOpen || LowTide == 0))
  {
    LoudRoomQuiet = 1;
    Obj[OBJ_BAR].prop &= ~PROP_SACRED;
    PrintCompLine("\x85\x61\x63\xa5\xc5\x69\x63\xa1\xdd\x80\xda\xe9\xf9\xfa\xad\x67\x9e\x73\x75\x62\x74\x6c\x79\x2e");
    TimePassed = 1;
  }
  else
    PrintCompLine("\x45\xfa\xba\x65\xfa\x6f\x2e\x2e\x2e");
}



void DoPray(void)
{
  TimePassed = 1;

  if (Obj[OBJ_YOU].loc == ROOM_SOUTH_TEMPLE)
  {
    if (YouAreDead)
    {
      PrintCompLine("\x46\xc2\xf9\x81\x64\xb2\x74\xad\x63\x9e\x81\x73\xa5\xb9\x8a\xd0\xd9\xed\x9f\x72\x75\x6d\xfc\xa6\x9a\xa0\xbb\x64\x83\x9e\xc2\xe1\xb0\x65\x63\xe1\xbe\x20\xd7\x72\xc4\x62\xf1\x67\x68\xa6\x8c\x8f\x66\xf3\xea\x64\xb2\x65\x6d\x62\x6f\x64\x69\xd5\xa4\x49\xb4\xd0\x6d\xe1\xd4\x74\xb5\x81\x62\xf1\x67\x68\x74\xed\x73\xa1\x66\x61\xe8\xa1\x8c\x8f\x66\xa7\xab\x92\xd6\x6c\xd2\xf1\x73\x84\xe0\xa8\xd2\x66\xc2\xf9\xd0\xd9\x9c\x73\xcf\x65\x70\xb5\xe8\x65\x70\xa8\xb4\x81\x77\xe9\x64\x73\xa4\x49\xb4\x81\x64\xb2\x74\xad\x63\x9e\x8f\xe7\xb4\x66\x61\xa7\x74\xec\xc0\xbf\xb6\xd0\x73\xca\x67\x62\x69\x72\xab\x8c\x81\x73\xa5\xb9\xa1\xdd\x80\xc6\xd3\xbe\x74\x2e\x0a");
      YouAreDead = 0;
      if (Obj[OBJ_TROLL].loc == ROOM_TROLL_ROOM)
        TrollAllowsPassage = 0;
      Obj[OBJ_LAMP].prop &= ~PROP_NODESC;
      Obj[OBJ_LAMP].prop &= ~PROP_NOTTAKEABLE;
      Obj[OBJ_YOU].prop &= ~PROP_LIT;
    }
    else
      YouAreInBoat = 0; // in case you're in it

    ExitFound = 1;
    GoToRoutine(ROOM_FOREST_1);
  }
  else
  {
    if (YouAreDead)
      PrintCompLine("\xdc\xd8\xeb\xf4\x79\xac\xa1\xbb\x9e\xe3\xa6\xa0\xbb\x64\x2e");
    else
      PrintCompLine("\x49\xd2\x8f\x70\xf4\xc4\xd4\xa5\x67\x68\xb5\x92\xeb\xf4\x79\xac\xa1\x6d\x61\xc4\xef\xa3\x6e\x73\x77\xac\x65\x64\x2e");
  }
}



void DoVersion(void)
{
  char buffer[128];

  strcpy(buffer, "Compiled on ");
  strcat(buffer, __DATE__);
  strcat(buffer, " @ ");
  strcat(buffer, __TIME__);

  PrintLine(buffer);
}



void DoDiagnose(void)
{
  int wounds, count, death_dist = PlayerFightStrength(0) + PlayerStrength;

  if (EnableCureRoutine == 0) wounds = 0;
  else                        wounds = -PlayerStrength;

  if (wounds == 0)
    PrintCompLine("\x8b\xbb\x9e\xa7\xeb\xac\x66\x65\x63\xa6\xa0\xe2\x74\x68\x2e");
  else
  {
    PrintCompText("\x8b\xcd\x76\x65\x20");
         if (wounds == 1) PrintCompText("\xd0\xf5\x67\x68\xa6\x77\xa5\x6e\x64");
    else if (wounds == 2) PrintCompText("\xd0\xd6\xf1\xa5\xa1\x77\xa5\x6e\x64");
    else if (wounds == 3) PrintCompText("\xd6\xd7\xf4\xea\x77\xa5\x6e\x64\x73");
    else                  PrintCompText("\xd6\xf1\xa5\xa1\x77\xa5\x6e\x64\x73");
    PrintCompText("\xb5\x77\xce\xfa\xb7\x69\xdf\xb0\x9e\x63\xd8\xd5\xa3\x66\xd1\x72\x20");
    count = CURE_WAIT * (wounds - 1) + EnableCureRoutine;
    PrintInteger(count);
    if (count == 1) PrintCompLine("\xee\x6f\x76\x65\x2e");
    else            PrintCompLine("\xee\x6f\xd7\x73\x2e");
  }

  PrintCompText("\x8b\xe7\x6e\x20");
       if (death_dist == 0) PrintCompLine("\x65\x78\xfc\x63\xa6\xe8\xaf\xde\x73\xe9\x6e\x2e");
  else if (death_dist == 1) PrintCompLine("\xef\x20\x6b\x69\xdf\xd5\xb0\xc4\xca\x9e\x6d\xd3\x9e\xf5\x67\x68\xa6\x77\xa5\x6e\x64\x2e");
  else if (death_dist == 2) PrintCompLine("\xef\x20\x6b\x69\xdf\xd5\xb0\xc4\xd0\xd6\xf1\xa5\xa1\x77\xa5\x6e\x64\x2e");
  else if (death_dist == 3) PrintCompLine("\x73\xd8\x76\x69\xd7\xae\xed\xaa\xac\x69\xa5\xa1\x77\xa5\x6e\x64\x2e");
  else                      PrintCompLine("\x73\xd8\x76\x69\xd7\xaa\x65\xd7\xf4\xea\x77\xa5\xb9\x73\x2e");

  if (NumDeaths != 0)
  {
    PrintCompText("\x8b\xcd\xd7\xb0\xf3\xb4\x6b\x69\xdf\x65\x64\x20");
    if (NumDeaths == 1) PrintCompLine("\xca\x63\x65\x2e");
    else                PrintCompLine("\x74\xf8\x63\x65\x2e");
  }
}



void DoOdysseus(void)
{
  if (Obj[OBJ_YOU].loc != ROOM_CYCLOPS_ROOM || Obj[OBJ_CYCLOPS].loc == 0)
    PrintCompLine("\x57\xe0\x93\x94\xd0\x73\x61\x69\xd9\x72\x3f");
  else if (CyclopsState == 3)
    PrintCompLine("\x4e\xba\xfe\x9e\x74\xe2\x6b\x84\xbd\xc0\x69\x6d\xa4\x48\x65\x27\xa1\x66\xe0\xa6\xe0\xcf\x65\x70\x2e");
  else
  {
    CyclopsState = 4;
    Obj[OBJ_CYCLOPS].loc = 0;
    PrintCompLine("\x85\x63\x79\x63\xd9\x70\x73\xb5\xa0\xbb\x84\x81\x6e\x61\x6d\x9e\xdd\xc0\x9a\x66\xaf\xa0\x72\x27\xa1\xe8\x61\x64\xec\xe4\x65\x6d\xbe\xb2\xb5\x66\xcf\xbe\x80\xda\xe9\xf9\x62\xc4\x6b\xe3\x63\x6b\x84\x64\xf2\xb4\x81\x77\xe2\xea\xca\x80\xfb\xe0\xa6\xdd\x80\xda\xe9\x6d\x2e");
    TimePassed = 1;
    ExitFound = 1;
  }
}



void DoSwim(void)
{
  if (Room[Obj[OBJ_YOU].loc].prop & R_WATERHERE)
    PrintCompLine("\x53\xf8\x6d\x6d\x84\xb2\x93\xfe\x75\xe2\xec\xa3\xdf\xf2\xd5\xa8\xb4\x81\x64\xf6\x67\x65\x6f\x6e\x2e");
  else
    PrintCompLine("\x47\xba\x6a\x75\x6d\x70\xa8\xb4\xd0\xfd\x6b\x65\x21");
}



void DoIntro(void)
{
  PrintCompLine("\x57\x65\x6c\x63\xe1\x9e\xbd\x20\x5a\xd3\x6b\x20\x49\x3a\x82\x20\x47\xa9\xaf\x20\x55\xb9\xac\x67\xc2\xf6\xab\x45\x6d\x70\x69\xa9\x21\x0a\x28\x63\x29\x20\x31\x39\x38\x30\xb0\xc4\x49\x4e\x46\x4f\x43\x4f\x4d\xb5\x49\x6e\x63\x2e\x0a\x20\x20\x43\xeb\xd3\xa6\x8c\x70\xbb\xd6\xb6\x28\x63\x29\x20\x32\x30\x32\x31\xb0\xc4\x44\xca\x6e\x69\x9e\x52\xfe\xd6\xdf\x20\x49\x49\x0a\x0a");
}
//*****************************************************************************



//*****************************************************************************
void ActorResponse(int obj, int odysseus)
{
  switch (obj)
  {
    case OBJ_CYCLOPS:
      if (odysseus)
        DoOdysseus();
      else
        PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x70\xa9\x66\xac\xa1\xbf\xf0\x9c\xbd\xee\x61\x6b\x84\x63\xca\xd7\x72\x73\xaf\x69\x6f\x6e\x2e");
    break;

    case OBJ_GHOSTS:
      PrintCompLine("\x85\x73\x70\x69\xf1\x74\xa1\x6a\xf3\xb6\xd9\x75\x64\xec\x8d\xa8\x67\xe3\xa9\x86\x2e");
    break;

    case OBJ_BAT:
      PrintCompLine("\x20\x20\x20\x20\x46\x77\xf3\x70\x21\x0a\x20\x20\x20\x20\x46\x77\xf3\x70\x21\x0a\x20\x20\x20\x20\x46\x77\xf3\x70\x21\x0a\x20\x20\x20\x20\x46\x77\xf3\x70\x21\x0a\x20\x20\x20\x20\x46\x77\xf3\x70\x21\x0a\x20\x20\x20\x20\x46\x77\xf3\x70\x21");
    break;

    case OBJ_THIEF:
      PrintCompLine("\x85\xa2\x69\x65\xd2\x9a\xd0\xc5\xc2\xb1\xb5\x73\x69\xcf\xe5\x9f\x79\x70\x65\x2e");
    break;

    case OBJ_TROLL:
      PrintCompLine("\x85\x74\xc2\xdf\xa8\x73\x93\x6d\x75\xfa\x8a\xd0\x63\xca\xd7\x72\x73\xaf\x69\xca\xe2\xb2\x74\x2e");
    break;
  }
}



int VerifyActor(int obj)
{
  if (obj == FOBJ_SCENERY_NOTVIS || obj == FOBJ_NOTVIS)
    {PrintCompLine("\x53\xac\x69\xa5\x73\xec\x3f\x21"); return 1;}
  else if (obj == FOBJ_AMB)
    {PrintCompLine("\x8b\xed\xd5\x89\xef\xee\xd3\x9e\x73\xfc\x63\x69\x66\x69\x63\xa3\x62\xa5\xa6\x77\x68\xba\x8f\x77\xad\xa6\xbd\x9f\xe2\x6b\x9f\x6f\x2e"); return 1;}
  else if (obj == OBJ_YOU || obj >= NUM_OBJECTS)
    {PrintCompLine("\x53\xac\x69\xa5\x73\xec\x3f\x21"); return 1;}
  else if ((Obj[obj].prop & PROP_ACTOR) == 0)
    {PrintCompLine("\x53\xac\x69\xa5\x73\xec\x3f\x21"); return 1;}
  else if (Obj[obj].loc != Obj[OBJ_YOU].loc)
    {PrintCompLine("\xbc\xaf\xeb\xac\x73\xca\xa8\x73\x93\x76\xb2\x69\x62\xcf\xc0\xac\x65\x21"); return 1;}

  return 0;
}



// actor, *** until end of input

void DoCommandActor(int obj)
{
  int odysseus = 0;

  while (CurWord < NumStrWords)
  {
    if (MatchCurWord("odysseus") || MatchCurWord("ulysses"))
      odysseus = 1;
    else
      CurWord++;
  }

  if (VerifyActor(obj) == 0)
    ActorResponse(obj, odysseus);
}



// talkto/ask/tell actor (about) (***)

void DoTalkTo(void)
{
  int obj, odysseus = 0;

  obj = GetAllObjFromInput(Obj[OBJ_YOU].loc); if (obj <= 0) return;
  if (VerifyActor(obj)) return;

  while (CurWord < NumStrWords)
  {
    if (MatchCurWord("then"))
      {CurWord--; break;} // end of this turn's command; back up so "then" can be matched later
    else if (MatchCurWord("odysseus") || MatchCurWord("ulysses"))
      odysseus = 1;
    else
      CurWord++;
  }

  ActorResponse(obj, odysseus);
}



// greet/hello (,) actor

void DoGreet(void)
{
  int obj, odysseus = 0;

  MatchCurWord("and");
  obj = GetAllObjFromInput(Obj[OBJ_YOU].loc); if (obj <= 0) return;
  if (VerifyActor(obj)) return;

  if (obj == OBJ_THIEF && ThiefDescType == 1) // unconscious
    PrintCompLine("\x85\xa2\x69\x65\x66\xb5\xef\x84\xd1\x6d\x70\xd3\xbb\x69\xec\xa8\x6e\xe7\x70\x61\x63\xc7\xaf\xd5\xb5\x9a\xf6\x61\x62\xcf\x89\x61\x63\x6b\xe3\x77\xcf\x64\x67\x9e\x92\xe6\xa9\x65\xf0\x9c\xf8\xa2\xc0\x9a\xfe\x75\xe2\xe6\xf4\x63\x69\xa5\x73\xed\x73\x73\x2e");
  else if (obj == OBJ_TROLL && TrollDescType == 1) // unconscious
    PrintCompLine("\x55\x6e\x66\xd3\x74\xf6\xaf\x65\xec\xb5\x81\x74\xc2\xdf\x91\x27\xa6\xa0\xbb\x86\x2e");
  else
    ActorResponse(obj, odysseus);
}



// say *** (to actor)

void DoSay(void)
{
  int obj = 0, odysseus = 0, temp;

  while (CurWord < NumStrWords)
  {
    if (MatchCurWord("to"))
      {CurWord--; break;} // back up so "to" can be matched below
    else if (MatchCurWord("odysseus") || MatchCurWord("ulysses"))
      odysseus = 1;
    else
      CurWord++;
  }

  if (MatchCurWord("to"))
  {
    obj = GetAllObjFromInput(Obj[OBJ_YOU].loc); if (obj <= 0) return;
  }

  if (obj == 0)
  {
    // look for exactly one actor in player's room who is described (thief can be invisible)
    for (temp=2; temp<NUM_OBJECTS; temp++)
      if ((Obj[temp].prop & PROP_ACTOR) &&
          (Obj[temp].prop & PROP_NODESC) == 0 &&
          Obj[temp].loc == Obj[OBJ_YOU].loc)
        {if (obj == 0) obj = temp; else {obj = 0; break;}}
    if (obj == 0) // more than one or no actors
      {PrintCompLine("\x8b\xed\xd5\x89\x73\xfc\x63\x69\x66\xc4\x77\x68\xba\xbd\x9f\xe2\x6b\x9f\x6f\x2e"); return;}
  }

  if (VerifyActor(obj) == 0)
    ActorResponse(obj, odysseus);
}
//*****************************************************************************



//*****************************************************************************

// handle things like water and boats

int ActionDirectionRoutine(int newroom)
{
  if (Room[Obj[OBJ_YOU].loc].prop & R_BODYOFWATER)
  {
    //move from water to land or water

    if ((Room[newroom].prop & R_BODYOFWATER) == 0)
      PrintCompLine("\x85\x6d\x61\x67\x69\x63\xb0\x6f\xaf\xb3\xe1\xbe\x89\xd0\xa9\xc5\xae\xb4\x81\x73\x68\xd3\x65\x2e\x0a");

    Obj[OBJ_INFLATED_BOAT].loc = newroom;

    DownstreamCounter = -1; // in case of moving to water; start at -1 to account for this turn
  }
  else
  {
    //move from land
    if (YouAreInBoat)
    {
      PrintCompLine("\xdc\x75\x27\xdf\xc0\x61\xd7\x89\x67\x65\xa6\xa5\xa6\xdd\x80\xb0\x6f\xaf\xc6\x69\x72\x73\x74\x2e");
      return 1;
    }
  }
  return 0;
}
//*****************************************************************************



//*****************************************************************************

// returns 0 if action not intercepted

int InterceptActionWhenDead(int action)
{
  if (YouAreDead == 0)
    return 0;

  if (action == A_GO || (action >= A_NORTH && action <= A_OUT))
    return 0;

  switch (action)
  {
    case A_QUIT: case A_RESTART: case A_RESTORE: case A_SAVE:
    case A_BRIEF: case A_VERBOSE: case A_SUPERBRIEF: case A_VERSION:
    case A_PRAY:
      return 0;

    // A_BURN
    case A_OPEN: case A_CLOSE: case A_EAT: case A_DRINK: case A_INFLATE: case A_DEFLATE:
    case A_TURN: case A_TIE: case A_UNTIE: case A_TOUCH:
      PrintCompLine("\x45\xd7\xb4\x73\x75\xfa\xa3\xb4\x61\x63\xf0\xca\x87\xef\xc9\xb9\x86\xb6\xe7\x70\x61\x62\x69\xf5\xf0\x65\x73\x2e");
    break;

    case A_SCORE:
      PrintCompLine("\xdc\x75\x27\xa9\xcc\xbf\x64\x21\x20\x48\xf2\x91\x86\x95\xa7\x6b\x8a\x92\xaa\x63\xd3\x65\x3f");
    break;

    case A_DIAGNOSE:
      PrintCompLine("\x8b\xbb\x9e\xe8\x61\x64\x2e");
    break;

    case A_WAIT:
      PrintCompLine("\x4d\x69\x67\x68\xa6\xe0\xb7\x65\xdf\x8e\x75\x27\xd7\xe6\xff\xa3\xb4\x65\xd1\x72\x6e\xc7\x79\x2e");
    break;

    case A_ACTIVATE:
      PrintCompLine("\x8b\xed\xd5\xe4\xba\xf5\x67\x68\xa6\xbd\xe6\x75\x69\xe8\x86\x2e");
    break;

    case A_TAKE:
      PrintCompLine("\xdc\xd8\xc0\x8c\x70\xe0\xd6\xa1\xa2\xc2\x75\x67\xde\xc7\xa1\x6f\x62\x6a\x65\x63\x74\x2e");
    break;

    case A_BREAK:
      PrintCompLine("\x41\xdf\xaa\x75\xfa\xa3\x74\x74\x61\x63\x6b\xa1\xbb\x9e\x76\x61\xa7\xa8\xb4\x92\xb3\xca\x64\xc7\x69\x6f\x6e\x2e");
    break;

    // A_THROW
    case A_DROP: case A_INVENTORY:
      PrintCompLine("\x8b\xcd\xd7\xe4\xba\x70\x6f\x73\xd6\x73\x73\x69\xca\x73\x2e");
    break;

    case A_LOOK:
      PrintPlayerRoomDesc(1);
      PrintCompText("\x85\xc2\xe1\xcb\xe9\x6b\xa1\xc5\xf4\xb1\x9e\x8c\xf6\xbf\x72\xa2\x6c\x79");
      if (GetNumObjectsInLocation(Obj[OBJ_YOU].loc) == 0)
        PrintCompLine("\x2e");
      else
        PrintCompLine("\x8d\xae\x62\x6a\x65\x63\x74\xa1\x61\x70\xfc\xbb\xa8\xb9\xb2\xf0\x6e\x63\x74\x2e");
      if ((Room[Obj[OBJ_YOU].loc].prop & R_LIT) == 0)
        PrintCompLine("\x41\x6c\xa2\xa5\x67\xde\x96\xa9\x87\xe3\xcb\x69\x67\x68\x74\xb5\x81\xc2\xe1\xaa\xf3\x6d\xa1\x64\x69\x6d\xec\xa8\xdf\x75\x6d\xa7\xaf\x65\x64\x2e");
    break;

    default:
      PrintCompLine("\x8b\xe7\x93\x65\xd7\xb4\x64\xba\xa2\x61\x74\x2e");
    break;
  }

  return 1;
}



// returns 0 if action not intercepted

int InterceptActionInLoudRoom(int action)
{
  if (Obj[OBJ_YOU].loc != ROOM_LOUD_ROOM)
    return 0;

  if (LoudRoomQuiet || (GatesOpen == 0 && LowTide))
    return 0; // room not loud

  if ((action >= A_NORTH && action <= A_OUT) || action == A_GO ||
      action == A_SAVE || action == A_RESTORE || action == A_QUIT ||
      action == A_ECHO)
    return 0; // let these commands through

  if (NumStrWords >= 1)
  {
    PrintText(StrWord[0]);
    PrintCompText("\x20");
    PrintText(StrWord[0]);
    PrintCompLine("\x2e\x2e\x2e");
  }
  else
    PrintCompLine("\x2e\x2e\xa4\x2e\x2e\x2e");

  return 1;
}



int InterceptAction(int action)
{
  if (InterceptActionWhenDead(action))   return 1;
  if (InterceptActionInLoudRoom(action)) return 1;

  return 0;
}
//*****************************************************************************



//*****************************************************************************

// returns 0 if take should go ahead

int InterceptTakeObj(int obj)
{
  switch (obj)
  {
    case OBJ_BAT:           PrintCompLine("\x8b\xe7\x93\xa9\x61\xfa\xc0\x69\x6d\x3b\xc0\x65\x27\xa1\xca\x80\xb3\x65\x69\xf5\x6e\x67\x2e"); return 1;
    case OBJ_CYCLOPS:       PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x64\x6f\xbe\x93\x74\x61\x6b\x9e\x6b\xa7\x64\xec\x89\xef\x84\x67\xf4\x62\xef\x64\x2e"); TimePassed = 1; return 1;
    case OBJ_THIEF:         PrintCompLine("\x4f\x6e\x63\x9e\x8f\x67\xff\xc0\x69\x6d\xb5\x77\xcd\xa6\x77\xa5\x6c\xab\x8f\x64\xba\xf8\xa2\xc0\x69\x6d\x3f"); return 1;
    case OBJ_TROLL:         PrintCompLine("\x85\x74\xc2\xdf\xaa\x70\xc7\xa1\xa7\x86\xb6\x66\x61\x63\x65\xb5\x67\x72\xf6\xf0\x9c\x22\x42\x65\x74\xd1\xb6\x6c\x75\x63\x6b\xe4\x65\x78\xa6\xf0\x6d\x65\x22\xa8\xb4\xd0\xf4\x96\xb6\x62\xbb\x62\xbb\xa5\xa1\x61\x63\x63\xd4\x74\x2e"); TimePassed = 1; return 1;
    case OBJ_MACHINE:       PrintCompLine("\x49\xa6\x9a\x66\xbb\x9f\xe9\xcb\xbb\x67\x9e\xbd\xb3\xbb\x72\x79\x2e"); return 1;
    case OBJ_TROPHY_CASE:   PrintCompLine("\x85\x74\xc2\x70\x68\xc4\xe7\xd6\x87\xd6\x63\xd8\x65\xec\xc6\xe0\xd1\xed\xab\xbd\x80\xb7\xe2\x6c\x2e"); return 1;
    case OBJ_MAILBOX:       PrintCompLine("\x49\xa6\x9a\xd6\x63\xd8\x65\xec\xa3\x6e\xfa\xd3\x65\x64\x2e"); return 1;
    case OBJ_KITCHEN_TABLE: PrintCompLine("\x8b\xe7\x93\x74\x61\x6b\x9e\x81\x74\x61\x62\x6c\x65\x2e"); return 1;
    case OBJ_ATTIC_TABLE:   PrintCompLine("\x8b\xe7\x93\x74\x61\x6b\x9e\x81\x74\x61\x62\x6c\x65\x2e"); return 1;
    case OBJ_HOT_BELL:      PrintCompLine("\x85\xef\xdf\x87\xd7\x72\xc4\x68\xff\x8d\x91\xe3\xa6\xef\x9f\x61\x6b\x65\x6e\x2e"); return 1;

    case OBJ_WATER:
      if ((Room[Obj[OBJ_YOU].loc].prop & R_WATERHERE) == 0 &&
           !(IsObjVisible(OBJ_BOTTLE) &&
             (Obj[OBJ_BOTTLE].prop & PROP_OPEN) &&
             Obj[OBJ_WATER].loc == INSIDE + OBJ_BOTTLE))
        PrintCompLine("\x99\xa9\x27\xa1\xe3\xb7\xaf\xac\xc0\xac\x65\x21");
      else
        PrintCompLine("\x85\x77\xaf\xac\xaa\xf5\x70\xa1\xa2\xc2\x75\x67\xde\x92\xc6\x97\xac\x73\x2e");
      return 1;

    case OBJ_TOOL_CHEST:
      PrintCompLine("\x85\xfa\xbe\x74\xa1\xbb\x9e\x73\xba\x72\xfe\x74\xc4\x8c\x63\xd3\xc2\xe8\xab\xa2\xaf\x80\xc4\x63\x72\x75\x6d\x62\xcf\xb7\xa0\xb4\x8f\xbd\x75\xfa\x80\x6d\x2e");
      Obj[OBJ_TOOL_CHEST].loc = 0;
      return 1;

    case OBJ_ROPE:
      if (RopeTiedToRail)
        {PrintCompLine("\x85\xc2\xfc\x87\xf0\xd5\x89\x81\xf4\x69\xf5\x6e\x67\x2e"); return 1;}
    break;

    case OBJ_RUSTY_KNIFE:
      if (Obj[OBJ_SWORD].loc == INSIDE + OBJ_YOU)
        PrintCompLine("\x41\xa1\x8f\xbd\x75\xfa\x80\xda\xfe\x74\xc4\x6b\x6e\x69\x66\x65\xb5\x92\xaa\x77\xd3\xab\x67\x69\xd7\xa1\xd0\x73\x97\xcf\xeb\x75\x6c\xd6\x8a\x62\xf5\xb9\x84\x62\x6c\x75\x9e\xf5\x67\x68\x74\x2e");
    break;

    case OBJ_CHALICE:
      if (Obj[OBJ_CHALICE].loc == ROOM_TREASURE_ROOM &&
          Obj[OBJ_THIEF].loc == ROOM_TREASURE_ROOM &&
          (Obj[OBJ_THIEF].prop & PROP_NODESC) == 0 &&
          VillainAttacking[VILLAIN_THIEF] &&
          ThiefDescType != 1) // not unconscious
        {PrintCompLine("\xdc\x75\x27\xab\xef\xaa\x74\x61\x62\xef\xab\xa7\x80\xb0\x61\x63\x6b\xc6\x69\x72\x73\x74\x2e"); return 1;}
    break;

    case OBJ_LARGE_BAG:
      if (ThiefDescType == 1) // unconscious
        PrintCompLine("\x53\x61\x64\xec\xc6\xd3\x86\xb5\x81\xc2\x62\xef\xb6\x63\x6f\xdf\x61\x70\xd6\xab\xca\x9f\x6f\x70\x8a\x81\x62\x61\x67\x9d\x72\x79\x84\xbd\x9f\x61\x6b\x9e\xc7\xb7\xa5\x6c\xab\x77\x61\x6b\x9e\xce\x6d\x2e");
      else
        PrintCompLine("\x85\x62\x61\xc1\xf8\xdf\xb0\x9e\x74\x61\x6b\xd4\xae\xd7\xb6\xce\xa1\xe8\x61\xab\x62\x6f\x64\x79\x2e");
      return 1;
  }

  return 0;
}



// if player is inside vehicle, return vehicle obj; otherwise return 0

int GetPlayersVehicle(void)
{
  if (YouAreInBoat)
    return OBJ_INFLATED_BOAT;
  else
    return 0;
}



void MoveTreasuresToLandOfLivingDead(int loc)
{
  int obj;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == loc &&
        (Obj[obj].prop & PROP_NODESC) == 0 &&
        (Obj[obj].prop & PROP_SACRED) == 0 &&
        Obj[obj].thiefvalue > 0)
  {
    Obj[obj].loc = ROOM_LAND_OF_LIVING_DEAD;
    Obj[obj].prop |= PROP_MOVEDDESC;
  }
}



// returns 1 if intercepted

int InterceptTakeFixedObj(int obj)
{
  switch (obj)
  {
    case FOBJ_BOARD:        PrintCompLine("\x85\x62\x6f\xbb\x64\xa1\xbb\x9e\xd6\x63\xd8\x65\xec\xc6\xe0\xd1\xed\x64\x2e");                          return 1;
    case FOBJ_SONGBIRD:     PrintCompLine("\x85\x73\xca\x67\x62\x69\x72\xab\x9a\xe3\xa6\xa0\xa9\xb0\xf7\x87\x70\xc2\x62\x61\x62\xec\xe4\xbf\x72\x62\x79\x2e");           return 1;
    case FOBJ_BODIES:       PrintCompLine("\x41\xc6\xd3\x63\x9e\x6b\xf3\x70\xa1\x8f\x66\xc2\xf9\x74\x61\x6b\x84\x81\x62\x6f\x64\x69\x65\x73\x2e");                  return 1;
    case FOBJ_RUG:          PrintCompLine("\x85\x72\x75\xc1\x9a\x65\x78\x74\xa9\x6d\x65\xec\xc0\xbf\x76\xc4\x8c\xe7\x6e\xe3\xa6\xef\xb3\xbb\xf1\x65\x64\x2e");          return 1;
    case FOBJ_NAILS:        PrintCompLine("\x85\x6e\x61\x69\x6c\x73\xb5\xe8\x65\x70\xec\xa8\x6d\xef\x64\xe8\xab\xa7\x80\xcc\xe9\x72\xb5\xe7\x6e\xe3\xa6\xef\xda\x65\x6d\x6f\xd7\x64\x2e"); return 1;
    case FOBJ_GRANITE_WALL: PrintCompLine("\x49\x74\x27\xa1\x73\x6f\xf5\xab\x67\xf4\x6e\xc7\x65\x2e");                                        return 1;
    case FOBJ_CHAIN:        PrintCompLine("\x85\xfa\x61\xa7\x87\xd6\x63\xd8\x65\x2e");                                       return 1;

    case FOBJ_BOLT:
    case FOBJ_BUBBLE:
      PrintCompLine("\x49\xa6\x9a\xad\xa8\xe5\x65\x67\xf4\xea\x70\xbb\xa6\xdd\x80\xb3\xca\x74\xc2\xea\x70\xad\x65\x6c\x2e");
      return 1;

    case FOBJ_MIRROR2:
    case FOBJ_MIRROR1:
      PrintCompLine("\x85\x6d\x69\x72\xc2\xb6\x9a\x6d\xad\xc4\xf0\x6d\xbe\x86\xb6\x73\x69\x7a\x65\xa4\x47\x69\xd7\x20\x75\x70\x2e");
      return 1;

    case FOBJ_BONES:
      PrintCompLine("\x41\xe6\x68\x6f\xc5\xa3\x70\xfc\xbb\xa1\xa7\x80\xda\xe9\xf9\x8c\x9a\x61\x70\x70\xe2\xcf\xab\xaf\x86\xb6\xe8\xd6\x63\xf4\xf0\xca\x8a\x81\xa9\x6d\x61\xa7\xa1\xdd\xa3\xc6\x65\xdf\xf2\xa3\x64\xd7\xe5\xd8\xac\xa4\x48\x9e\xe7\xc5\xa1\xd0\x63\xd8\xd6\xae\xb4\x92\x20\x76\xe2\x75\x61\x62\xcf\xa1\x8c\x62\xad\xb2\xa0\xa1\x96\xf9\xbd\x80\x20\x4c\x8c\xdd\x80\x20\x4c\x69\x76\x84\x44\xbf\x64\x83\x9e\x67\x68\x6f\xc5\xcb\xbf\xd7\x73\xb5\x6d\xf7\xd1\xf1\x9c\x6f\x62\x73\x63\xd4\xc7\x69\x65\x73\x2e");
      MoveTreasuresToLandOfLivingDead(Obj[OBJ_YOU].loc);
      MoveTreasuresToLandOfLivingDead(INSIDE + OBJ_YOU);
      return 1;
  }

  return 0;
}



int InterceptTakeOutOf(int container)
{
  switch (container)
  {
    case OBJ_LARGE_BAG:
    {
      PrintCompLine("\x49\xa6\x77\xa5\x6c\xab\xef\xa3\xe6\xe9\xab\x74\xf1\x63\x6b\x2e");
      return 1;
    }
  }

  return 0;
}



// test flag:  1 if no changes should be made (yet)
// multi flag: 1 if obj name should be printed

// returns:
//   1:  intercepted, and obj MUST leave inventory, unless container is full
//  -1:  intercepted and calling function should immediately return

int InterceptDropPutObj(int obj, int container, int test, int multi)
{
  switch (container)
  {
    case OBJ_LOWERED_BASKET:
    {
      if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
      PrintCompLine("\x85\x62\xe0\x6b\x65\xa6\x9a\xaf\x80\xae\x96\xb6\xd4\xab\xdd\x80\xaa\xcd\x66\x74\x2e");
      return -1;
    }

    case OBJ_CHALICE:
    {
      if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
      PrintCompLine("\x8b\xe7\x6e\x27\x74\xa4\x49\x74\x27\xa1\xe3\xa6\xd0\xd7\x72\xc4\x67\xe9\xab\xfa\xe2\x69\x63\x65\xb5\x9a\x69\x74\x3f");
      return -1;
    }

    case OBJ_LARGE_BAG:
    {
      if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
      PrintCompLine("\x49\xa6\x77\xa5\x6c\xab\xef\xa3\xe6\xe9\xab\x74\xf1\x63\x6b\x2e");
      return -1;
    }

    case FOBJ_GRATE:
    {
      if (Obj[obj].size > 20)
      {
        if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
        PrintCompLine("\x49\xa6\x77\xca\x27\xa6\x66\xc7\x95\xc2\x75\x67\xde\x81\x67\xf4\xf0\x6e\x67\x2e");
        return -1;
      }
      else if (Obj[OBJ_YOU].loc != ROOM_GRATING_CLEARING)
      {
        if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
        PrintCompLine("\x49\xa6\x77\xca\x27\xa6\x67\xba\xa2\xc2\x75\x67\x68\x2e");
        return -1;
      }    

      if (test == 0)
      {
        if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
        PrintCompLine("\x49\xa6\x67\x6f\xbe\x95\xc2\x75\x67\xde\x81\x67\xf4\xf0\x9c\xa7\xbd\x80\xcc\xbb\x6b\xed\x73\xa1\xef\xd9\x77\x2e");

        Obj[obj].loc = ROOM_GRATING_ROOM;
        MoveObjOrderToLast(obj);
        TimePassed = 1;
      }

      return 1;
    }

    case FOBJ_SLIDE:
    {
      if (test == 0)
      {
        if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
        if (Obj[OBJ_YOU].loc == ROOM_SLIDE_ROOM)
          PrintCompLine("\x49\xa6\x66\xe2\x6c\xa1\xa7\xbd\x80\xaa\xf5\xe8\x8d\x87\x67\xca\x65\x2e");
        else
          PrintCompLine("\x49\xa6\x66\xe2\x6c\xa1\xa7\xbd\x80\xaa\xf5\x64\x65\x2e");
  
        Obj[obj].loc = ROOM_CELLAR;
        MoveObjOrderToLast(obj);
        TimePassed = 1;
      }

      return 1;
    }

    case FOBJ_RIVER:
    case OBJ_WATER:
    {
      if ((Room[Obj[OBJ_YOU].loc].prop & R_WATERHERE) == 0)
      {
        if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
        PrintCompLine("\x99\xa9\xa8\x73\x93\xad\xc4\x77\xaf\xac\xc0\xac\x65\x21");
        return -1;
      }

      if (obj == OBJ_INFLATED_BOAT)
      {
        if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
        PrintCompLine("\x8b\x73\x68\xa5\x6c\xab\x67\x65\xa6\xa7\x80\xb0\x6f\xaf\x80\xb4\xfd\xf6\xfa\xa8\x74\x2e");
        return -1;
      }

      if (test == 0)
      {
        if (multi) {PrintObjectDesc(obj, 0); PrintCompText("\x3a\x20");}
        if (Obj[obj].prop & PROP_INFLAMMABLE)
          PrintCompLine("\x49\xa6\x66\xd9\xaf\xa1\x66\xd3\xa3\xee\xe1\xd4\x74\xb5\x96\xb4\x73\xa7\x6b\x73\x2e");
        else
          PrintCompLine("\x49\xa6\x73\x70\xfd\x73\xa0\xa1\xa7\xbd\x80\xb7\xaf\xac\x8d\x87\x67\xca\x9e\x66\xd3\x65\xd7\x72\x2e");

        Obj[obj].loc = 0;
        TimePassed = 1;
      }

      return 1;
    }
  }

  if (container >= NUM_OBJECTS)
  {
    PrintCompLine("\x8b\xe7\x93\x70\xf7\xa3\x6e\x79\xa2\x84\xa7\xbd\x95\x61\x74\x21");
    return -1;
  }

  return 0; // not intercepted
}

//*****************************************************************************



//*****************************************************************************
int IsActorInRoom(int room)
{
  int obj;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == room &&
        (Obj[obj].prop & PROP_ACTOR) &&
        (Obj[obj].prop & PROP_NODESC) == 0)
    return 1;

  return 0;
}



// thiefvalue for sword indicates glow level

void SwordRoutine(void)
{
  int glow, new_glow, i, room;

  if (Obj[OBJ_SWORD].loc != INSIDE + OBJ_YOU) return;

  glow = Obj[OBJ_SWORD].thiefvalue;
  new_glow = 0;

  if (IsActorInRoom(Obj[OBJ_YOU].loc))
    new_glow = 2;
  else
    for (i=0; i<10; i++)
  {
    room = RoomPassages[Obj[OBJ_YOU].loc].passage[i];
    if (room > 0 && room < NUM_ROOMS && IsActorInRoom(room))
    {
      new_glow = 1;
      break;
    }
  }

  if (new_glow != glow)
  {
         if (new_glow == 0) PrintCompLine("\xdc\xd8\xaa\x77\xd3\xab\x9a\xe3\xcb\xca\x67\xac\xe6\xd9\xf8\x6e\x67\x2e");
    else if (new_glow == 1) PrintCompLine("\xdc\xd8\xaa\x77\xd3\xab\x9a\x67\xd9\xf8\x9c\xf8\xa2\xa3\xc6\x61\xa7\xa6\x62\x6c\x75\x9e\x67\xd9\x77\x2e");
    else                    PrintCompLine("\xdc\xd8\xaa\x77\xd3\xab\xcd\xa1\xef\x67\xf6\x89\x67\xd9\x77\x20\xd7\x72\xc4\x62\xf1\x67\x68\x74\x6c\x79\x2e");
    Obj[OBJ_SWORD].thiefvalue = new_glow;
  }
}



void LampDrainRoutine(void)
{
  if (Obj[OBJ_LAMP].loc == 0) return; // destroyed by machine or lost

  if ((Obj[OBJ_LAMP].prop & PROP_LIT) == 0) return;

  if (LampTurnsLeft > 0) LampTurnsLeft--;

  if (IsObjVisible(OBJ_LAMP) && (Obj[OBJ_LAMP].prop & PROP_NODESC) == 0)
    switch (LampTurnsLeft)
  {
    case 100: PrintCompLine("\x85\xfd\x6d\x70\xa3\x70\xfc\xbb\xa1\xd0\x62\xc7\xcc\x69\x6d\x6d\x65\x72\x2e");     break;
    case  70: PrintCompLine("\x85\xfd\x6d\x70\x87\xe8\x66\xa7\xc7\x65\xec\xcc\x69\x6d\x6d\xac\xe4\x6f\x77\x2e"); break;
    case  15: PrintCompLine("\x85\xfd\x6d\x70\x87\xed\xbb\xec\xae\x75\x74\x2e");            break;
  }

  if (LampTurnsLeft == 0)
  {
    int prev_darkness;

    prev_darkness = IsPlayerInDarkness();
    Obj[OBJ_LAMP].prop &= ~PROP_LIT;
    if (IsPlayerInDarkness() != prev_darkness)
    {
      PrintNewLine();
      PrintPlayerRoomDesc(1);
    }
  }
}



// also handles candles put out by dropping or draft

void CandlesShrinkRoutine(void)
{
  int prev_darkness;

  if (Obj[OBJ_CANDLES].loc == 0) return; // destroyed by machine or lost

  if ((Obj[OBJ_CANDLES].prop & PROP_MOVEDDESC) == 0) return; // still sitting on altar

  if ((Obj[OBJ_CANDLES].prop & PROP_LIT) == 0) return; // not lit

  if (CandleTurnsLeft > 0) CandleTurnsLeft--;

  if (IsObjVisible(OBJ_CANDLES))
    switch (CandleTurnsLeft)
  {
    case 20: PrintCompLine("\x85\xe7\xb9\xcf\xa1\x67\xc2\x77\xaa\x68\xd3\xd1\x72\x2e");                           break;
    case 10: PrintCompLine("\x85\xe7\xb9\xcf\xa1\xbb\x9e\xef\x63\xe1\x84\x71\x75\xc7\x9e\x73\x68\xd3\x74\x2e");               break;
    case  5: PrintCompLine("\x85\xe7\xb9\xcf\xa1\x77\xca\x27\xa6\xfd\xc5\xcb\xca\xc1\xe3\x77\x2e");                    break;
    case  0: PrintCompLine("\xdc\x75\x27\xab\xef\x74\xd1\xb6\xcd\xd7\xee\xd3\x9e\xf5\x67\x68\xa6\xa2\xad\xc6\xc2\xf9\x81\xe7\xb9\xcf\x73\x2e"); break;
  }

  prev_darkness = IsPlayerInDarkness();

  if (CandleTurnsLeft == 0)
    Obj[OBJ_CANDLES].prop &= ~PROP_LIT;
  else
  {
    if (Obj[OBJ_CANDLES].loc != INSIDE + OBJ_YOU)
    {
      Obj[OBJ_CANDLES].prop &= ~PROP_LIT;
      if (IsObjVisible(OBJ_CANDLES)) PrintCompLine("\x85\xe7\xb9\xcf\xa1\x67\xba\xa5\x74\x2e");
    }
    else if (Obj[OBJ_YOU].loc == ROOM_TINY_CAVE && PercentChance(50, 80))
    {
      Obj[OBJ_CANDLES].prop &= ~PROP_LIT;
      if (IsObjVisible(OBJ_CANDLES)) PrintCompLine("\x41\xe6\xfe\xa6\xdd\xb7\xa7\xab\x62\xd9\x77\xa1\xa5\xa6\x92\x91\x64\xcf\x73\x21");
    }
  }

  if (IsPlayerInDarkness() != prev_darkness)
  {
    PrintNewLine();
    PrintPlayerRoomDesc(1);
  }
}



void ReservoirFillRoutine(void)
{
  if (ReservoirFillCountdown == 0) return;
      ReservoirFillCountdown--;
  if (ReservoirFillCountdown > 0) return;

  Room[ROOM_RESERVOIR  ].prop |= R_BODYOFWATER;
  Room[ROOM_DEEP_CANYON].prop &= ~R_DESCRIBED;
  Room[ROOM_LOUD_ROOM  ].prop &= ~R_DESCRIBED;

  LowTide = 0;

  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_RESERVOIR:
      if (YouAreInBoat)
        PrintCompLine("\x85\x62\x6f\xaf\xcb\x69\x66\x74\xa1\x67\xd4\x74\xec\xae\xf7\x8a\x81\x6d\x75\xab\x8c\x9a\xe3\x77\xc6\xd9\xaf\x84\xca\x80\xda\xbe\xac\x76\x6f\x69\x72\x2e");
      else
      {
        PrintCompLine("\x8b\xbb\x9e\xf5\x66\xd1\xab\x75\x70\xb0\xc4\x81\xf1\x73\x84\xf1\xd7\x72\x21\x88\x9f\x72\xc4\xbd\xaa\xf8\x6d\xb5\x62\xf7\x80\xb3\xd8\xa9\xe5\xa1\xbb\x9e\xbd\xba\xc5\xc2\xb1\x8e\xc3\x63\xe1\x9e\x63\xd9\xd6\x72\xb5\x63\xd9\xd6\xb6\xbd\x80\xa3\x77\xbe\xe1\x9e\xc5\x72\x75\x63\x74\xd8\x9e\xdd\x20\x46\xd9\x6f\xab\x43\xca\x74\xc2\xea\x44\x61\xf9\x23\x33\x83\x9e\x64\x61\xf9\xef\x63\x6b\xca\xa1\xbd\x86\x83\x9e\xc2\xbb\x8a\x81\x77\xaf\xac\xe4\xbf\x72\xec\xcc\xbf\x66\xd4\xa1\xc9\x75\xb5\x62\xf7\x86\xda\x65\x6d\x61\xa7\xb3\xca\x73\x63\x69\xa5\xa1\xe0\x86\x9f\x75\x6d\x62\xcf\xae\xd7\xb6\x81\x64\x61\xf9\xbd\x77\xbb\xab\x92\xb3\xac\x74\x61\xa7\xcc\xe9\xf9\x61\x6d\xca\xc1\x81\xc2\x63\x6b\xa1\xaf\xa8\x74\xa1\x62\xe0\x65\x2e");
        YoureDead(); // ##### RIP #####
      }
    break;

    case ROOM_DEEP_CANYON:
      PrintCompLine("\x41\xaa\xa5\xb9\xb5\xf5\x6b\x9e\xa2\xaf\x8a\x66\xd9\xf8\x9c\x77\xaf\xac\xb5\xc5\xbb\x74\xa1\xbd\xb3\xe1\x9e\x66\xc2\xf9\xef\xd9\x77\x2e");
    break;

    case ROOM_LOUD_ROOM:
      if (LoudRoomQuiet == 0)
      {
        int random_room[3] = {ROOM_DAMP_CAVE, ROOM_ROUND_ROOM, ROOM_DEEP_CANYON};

        PrintCompLine("\x41\xdf\x8a\xd0\x73\x75\x64\xe8\x6e\xb5\xad\xa3\xfd\x72\x6d\x97\xec\xcb\xa5\xab\xc2\xbb\x84\x73\xa5\xb9\xc6\x69\xdf\xa1\x81\xc2\xe1\xa4\x46\x69\xdf\xd5\xb7\xc7\xde\x66\xbf\x72\xb5\x8f\x73\x63\xf4\x6d\x62\xcf\xa3\x77\x61\x79\x2e\x0a");
        YouAreInBoat = 0; // in case you're in it
        GoToRoutine(random_room[GetRandom(3)]);
      }
    break;

    case ROOM_RESERVOIR_NORTH:
    case ROOM_RESERVOIR_SOUTH:
      PrintCompLine("\x8b\xe3\xf0\x63\x9e\xa2\xaf\x80\xb7\xaf\xac\xcb\x65\xd7\xea\xcd\xa1\xf1\xd6\xb4\xbd\x80\xeb\x6f\xa7\xa6\xa2\xaf\xa8\xa6\x9a\x69\x6d\x70\x6f\x73\x73\x69\x62\xcf\x89\x63\xc2\x73\x73\x2e");
    break;
  }
}



void ReservoirDrainRoutine(void)
{
  if (ReservoirDrainCountdown == 0) return;
      ReservoirDrainCountdown--;
  if (ReservoirDrainCountdown > 0) return;

  Room[ROOM_RESERVOIR  ].prop &= ~R_BODYOFWATER;
  Room[ROOM_DEEP_CANYON].prop &= ~R_DESCRIBED;
  Room[ROOM_LOUD_ROOM  ].prop &= ~R_DESCRIBED;

  LowTide = 1;

  switch (Obj[OBJ_YOU].loc)
  {
    case ROOM_RESERVOIR:
      if (YouAreInBoat)
        PrintCompLine("\x85\x77\xaf\xac\xcb\x65\xd7\xea\xcd\xa1\x64\xc2\x70\xfc\xab\xbd\x80\xeb\x6f\xa7\xa6\xaf\xb7\xce\xfa\x80\xb0\x6f\xaf\x91\xe4\xba\xd9\xb1\xac\xaa\x74\x61\xc4\x61\x66\xd9\xaf\xa4\x49\xa6\x73\xa7\x6b\xa1\xa7\xbd\x80\xee\x75\x64\x2e");
    break;

    case ROOM_DEEP_CANYON:
      PrintCompLine("\x85\xc2\xbb\x8a\x72\xfe\xce\x9c\x77\xaf\xac\x87\x71\x75\x69\x65\xd1\xb6\xe3\x77\x2e");
    break;

    case ROOM_RESERVOIR_NORTH:
    case ROOM_RESERVOIR_SOUTH:
      PrintCompLine("\x85\x77\xaf\xac\xcb\x65\xd7\xea\x9a\xe3\x77\x20\x71\x75\xc7\x9e\xd9\x77\xc0\xac\x9e\x8c\x8f\x63\xa5\x6c\xab\xbf\x73\x69\xec\xb3\xc2\x73\xa1\x6f\xd7\xb6\xbd\x80\xae\x96\xb6\x73\x69\x64\x65\x2e");
    break;
  }
}



void SinkingObjectsRoutine(void)
{
  int obj, i;
  int check_room[7] = {ROOM_RESERVOIR, ROOM_IN_STREAM, ROOM_RIVER_1, ROOM_RIVER_2, ROOM_RIVER_3,
                       ROOM_RIVER_4, ROOM_RIVER_5};

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (obj != OBJ_INFLATED_BOAT && obj != OBJ_BUOY && obj != OBJ_THIEF)
      for (i=0; i<7; i++)
        if (Obj[obj].loc == check_room[i])
  {
    if ((Room[check_room[i]].prop & R_BODYOFWATER) && (Obj[obj].prop & PROP_NODESC) == 0)
    {
      // if room is filled with water and object hasn't sunk, sink object
      Obj[obj].prop |= PROP_NODESC;
      Obj[obj].prop |= PROP_NOTTAKEABLE;
    }
    else if ((Room[check_room[i]].prop & R_BODYOFWATER) == 0 && (Obj[obj].prop & PROP_NODESC))
    {
      // if room is not filled with water and object has sunk, unsink object
      Obj[obj].prop &= ~PROP_NODESC;
      Obj[obj].prop &= ~PROP_NOTTAKEABLE;
    }
  }
}



void LoudRoomRoutine(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_LOUD_ROOM && LoudRoomQuiet == 0 && GatesOpen && LowTide == 0)
  {
    int random_room[3] = {ROOM_DAMP_CAVE, ROOM_ROUND_ROOM, ROOM_DEEP_CANYON};

    PrintCompLine("\x49\xa6\x9a\xf6\xef\xbb\x61\x62\xec\xcb\xa5\xab\xa0\xa9\xb5\xf8\xa2\xa3\xb4\xbf\x72\x2d\x73\x70\xf5\x74\xf0\x9c\xc2\xbb\xaa\xf3\x6d\x84\xbd\xb3\xe1\x9e\x66\xc2\xf9\xe2\xea\xbb\xa5\xb9\x86\x83\xac\x9e\x9a\xd0\x70\xa5\xb9\x84\xa7\x86\xb6\xa0\x61\xab\x77\xce\xfa\xb7\xca\x27\xa6\xc5\x6f\x70\xa4\x57\xc7\xde\xd0\x74\xa9\x6d\xd4\x64\xa5\xa1\x65\x66\x66\xd3\x74\xb5\x8f\x73\x63\xf4\x6d\x62\xcf\xae\xf7\x8a\x81\xc2\xe1\x2e\x0a");

    YouAreInBoat = 0; // in case you're in it

    GoToRoutine(random_room[GetRandom(3)]);
  }
}



void MaintenanceLeakRoutine(void)
{
  if (MaintenanceWaterLevel <= 0 || MaintenanceWaterLevel > 16) return;

  if (Obj[OBJ_YOU].loc == ROOM_MAINTENANCE_ROOM)
  {
    char *water_level_msg[9] =
    {
      "up to your ankles.",
      "up to your shin.",
      "up to your knees.",
      "up to your hips.",
      "up to your waist.",
      "up to your chest.",
      "up to your neck.",
      "over your head.",
      "high in your lungs."
    };

    PrintCompText("\x85\x77\xaf\xac\xcb\x65\xd7\xea\xa0\xa9\x87\xe3\x77\x20");

    PrintLine(water_level_msg[MaintenanceWaterLevel / 2]);
  }

  MaintenanceWaterLevel++;
  if (MaintenanceWaterLevel > 16 && Obj[OBJ_YOU].loc == ROOM_MAINTENANCE_ROOM)
  {
    PrintCompLine("\x49\x27\xf9\x61\x66\xf4\x69\xab\x8f\xcd\xd7\xcc\xca\x9e\x64\xc2\x77\xed\xab\x92\xd6\x6c\x66\x2e");
    if (YouAreInBoat)
      switch (Obj[OBJ_YOU].loc)
    {
      case ROOM_MAINTENANCE_ROOM:
      case ROOM_DAM_ROOM:
      case ROOM_DAM_LOBBY:
        PrintCompLine("\x85\xf1\x73\x84\x77\xaf\xac\xb3\xbb\xf1\xbe\x80\xb0\x6f\xaf\xae\xd7\xb6\x81\x64\x61\x6d\xb5\x64\xf2\xb4\x81\xf1\xd7\x72\xb5\x8c\x6f\xd7\xb6\x81\x66\xe2\x6c\x73\x9d\x73\x6b\xb5\x74\x73\x6b\x2e");
      break;
    }
    YoureDead(); // ##### RIP #####
  }
}



void BoatPuncturedRoutine(void)
{
  int i, flag, pointy_obj[6] = {OBJ_SCEPTRE, OBJ_KNIFE, OBJ_SWORD, OBJ_RUSTY_KNIFE, OBJ_AXE, OBJ_STILETTO};

  flag = 0;
  for (i=0; i<6; i++)
    if (Obj[pointy_obj[i]].loc == INSIDE + OBJ_INFLATED_BOAT)
  {
    flag = 1;
    Obj[pointy_obj[i]].loc = Obj[OBJ_INFLATED_BOAT].loc;
  }
  if (flag == 0) return;

  PrintCompLine("\x49\xa6\xd6\x65\x6d\xa1\xa2\xaf\xaa\xe1\x65\xa2\x84\x70\x6f\xa7\x74\xc4\x64\x69\x64\x93\x61\x67\xa9\x9e\xf8\xa2\x80\xb0\x6f\xaf\xb5\xe0\xfb\x76\x69\xe8\x6e\x63\xd5\xb0\xc4\x81\xd9\x75\xab\xce\x73\x73\x84\xe3\xb2\x9e\xb2\x73\x75\x84\x96\xa9\x66\xc2\x6d\xa4\x57\xc7\xde\xd0\x70\xaf\xa0\xf0\x63\xaa\x70\xf7\xd1\x72\xb5\x81\x62\x6f\xaf\xcc\x65\x66\xfd\xd1\x73\xb5\xcf\x61\x76\x84\x8f\xf8\xa2\xa5\x74\x2e");

  Obj[OBJ_PUNCTURED_BOAT].loc = Obj[OBJ_INFLATED_BOAT].loc;
  Obj[OBJ_INFLATED_BOAT].loc = 0;

  if (YouAreInBoat) YouAreInBoat = 0;

  if (Room[Obj[OBJ_YOU].loc].prop & R_BODYOFWATER)
  {
    if (Obj[OBJ_YOU].loc == ROOM_RESERVOIR || Obj[OBJ_YOU].loc == ROOM_IN_STREAM)
      PrintCompLine("\x41\xe3\x96\xb6\x70\xaf\xa0\xf0\x63\xaa\x70\xf7\xd1\x72\xb5\xa2\x9a\xf0\x6d\x9e\x66\xc2\xf9\xc9\x75\xb5\xa0\xf4\x6c\x64\xa1\x92\xcc\xc2\x77\x6e\x97\x2e");
    else
      PrintCompLine("\x49\xb4\xff\xa0\xb6\x77\xd3\x64\x73\xb5\x66\x69\x67\x68\xf0\x9c\x81\x66\x69\xac\x63\x9e\x63\xd8\xa9\xe5\xa1\xdd\x80\x20\x46\xf1\x67\x69\xab\x52\x69\xd7\x72\x8e\xc3\x6d\xad\x61\x67\x9e\xbd\xc0\x6f\x6c\xab\x92\xae\x77\xb4\x66\xd3\xa3\xb0\xc7\xb5\x62\xf7\x80\xb4\x8f\xbb\x9e\xe7\x72\xf1\xd5\xae\xd7\xb6\xd0\x77\xaf\xac\x66\xe2\xea\x8c\xa7\xbd\xaa\xe1\x9e\x6e\xe0\x74\xc4\xc2\x63\x6b\x73\xa4\x4f\x75\x63\x68\x21");
    YoureDead(); // ##### RIP #####
  }
}



void BuoyRoutine(void)
{
  if (BuoyFlag == 0 && Obj[OBJ_BUOY].loc == INSIDE + OBJ_YOU)
  {
    BuoyFlag = 1;
    PrintCompLine("\x8b\xe3\xf0\x63\x9e\x73\xe1\x65\xa2\x84\x66\xf6\x6e\xc4\x61\x62\xa5\xa6\x81\x66\xf3\xea\xdd\x80\xb0\x75\x6f\x79\x2e");
  }
}



void DownstreamRoutine(void)
{
  int i;
  int  float_from[5] = {ROOM_RIVER_1, ROOM_RIVER_2, ROOM_RIVER_3, ROOM_RIVER_4, ROOM_RIVER_5};
  int    float_to[5] = {ROOM_RIVER_2, ROOM_RIVER_3, ROOM_RIVER_4, ROOM_RIVER_5, 0};
  int float_speed[5] = {4, 4, 3, 2, 1};

  for (i=0; i<5; i++)
    if (Obj[OBJ_YOU].loc == float_from[i]) break;
  if (i == 5) return;

  DownstreamCounter++;
  if (DownstreamCounter < float_speed[i]) return;

  if (float_to[i] == 0)
  {
    PrintCompLine("\x55\x6e\x66\xd3\x74\xf6\xaf\x65\xec\xb5\x81\x6d\x61\x67\x69\x63\xb0\x6f\xaf\xcc\x6f\xbe\x93\x70\xc2\x76\x69\xe8\xeb\xc2\xd1\x63\xf0\xca\xc6\xc2\xf9\x81\xc2\x63\x6b\xa1\x8c\x62\xa5\x6c\xe8\x72\xa1\xca\x9e\x6d\xf3\x74\xa1\xaf\x80\xb0\xff\xbd\xf9\xdd\xb7\xaf\xac\x66\xe2\x6c\x73\xa4\x49\x6e\x63\x6c\x75\x64\x84\xa2\x9a\xca\x65\x2e");
    YoureDead(); // ##### RIP #####
    return;
  }

  PrintCompLine("\x85\x66\xd9\x77\x8a\x81\xf1\xd7\xb6\xe7\x72\xf1\xbe\x86\xcc\xf2\x6e\xc5\xa9\x61\x6d\x2e\x0a");
  DownstreamCounter = 0;
  BoatGoToRoutine(float_to[i]);
}



void BatRoomRoutine(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_BAT_ROOM && IsObjVisible(OBJ_GARLIC) == 0)
  {
    int random_room[8] = {ROOM_MINE_1, ROOM_MINE_2, ROOM_MINE_3, ROOM_MINE_4, ROOM_LADDER_TOP,
                          ROOM_LADDER_BOTTOM, ROOM_SQUEEKY_ROOM, ROOM_MINE_ENTRANCE};

    PrintCompLine("\x20\x20\x20\x20\x46\x77\xf3\x70\x21\x0a\x20\x20\x20\x20\x46\x77\xf3\x70\x21\x0a\x20\x20\x20\x20\x46\x77\xf3\x70\x21\x0a\x85\x62\xaf\xe6\xf4\x62\xa1\x8f\x62\xc4\x81\x73\x63\x72\x75\x66\xd2\xdd\x86\xb6\xed\x63\x6b\x8d\xcb\x69\x66\x74\xa1\x8f\x61\x77\x61\x79\x2e\x2e\x2e\x2e\x0a");
    GoToRoutine(random_room[GetRandom(8)]);
  }
}



void LeavesTakenRoutine(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_GRATING_CLEARING &&
      Obj[OBJ_LEAVES].loc != ROOM_GRATING_CLEARING &&
      GratingRevealed == 0)
  {
    GratingRevealed = 1;
    PrintCompLine("\x57\xc7\xde\x81\xcf\x61\xd7\xa1\x6d\x6f\xd7\x64\xb5\xd0\x67\xf4\xf0\x9c\x9a\xa9\xd7\xe2\x65\x64\x2e");
  }

  // also reveal grating just by being in grating room
  if (Obj[OBJ_YOU].loc == ROOM_GRATING_ROOM)
    GratingRevealed = 1;
}



// must call before match routine

void GasRoomRoutine(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_GAS_ROOM)
  {
    int match   = (Obj[OBJ_MATCH  ].loc == INSIDE + OBJ_YOU && (Obj[OBJ_MATCH  ].prop & PROP_LIT));
    int candles = (Obj[OBJ_CANDLES].loc == INSIDE + OBJ_YOU && (Obj[OBJ_CANDLES].prop & PROP_LIT));
    int torch   = (Obj[OBJ_TORCH  ].loc == INSIDE + OBJ_YOU && (Obj[OBJ_TORCH  ].prop & PROP_LIT));
    int type = 0; // 1: lighted  2: carried

    if (match && MatchTurnsLeft == 2)
      type = 1;
    else if (match || candles || torch)
      type = 2;

    if (type)
    {
      if (type == 1)
        PrintCompLine("\x48\xf2\xaa\x61\xab\x66\xd3\xa3\xb4\xe0\x70\x69\xf1\x9c\x61\x64\xd7\xe5\xd8\xac\x89\xf5\x67\x68\xa6\xd0\x6d\xaf\xfa\xa8\xb4\xd0\xc2\xe1\xb7\xce\xfa\xda\xf3\x6b\xa1\xdd\xe6\xe0\xa4\x46\xd3\x74\xf6\xaf\x65\xec\xb5\x96\xa9\x87\x6a\xfe\xf0\x63\x9e\xa7\x80\xb7\xd3\x6c\x64\x2e");
      else
        PrintCompLine("\x4f\xde\xe8\xbb\xa4\x49\xa6\x61\x70\xfc\xbb\xa1\xa2\xaf\x80\xaa\x6d\x65\xdf\xb3\xe1\x84\x66\xc2\xf9\xa2\x9a\xc2\xe1\xb7\xe0\xb3\x6f\xe2\xe6\xe0\xa4\x49\xb7\xa5\x6c\xab\xcd\xd7\x95\xa5\x67\x68\xa6\x74\xf8\x63\x9e\x61\x62\xa5\xa6\xe7\x72\x72\x79\x84\x66\xfd\x6d\x84\x6f\x62\x6a\x65\x63\x74\xa1\xa7\xc0\xac\x65\x2e");
      PrintCompLine("\x0a\x20\x20\x20\x20\x20\x20\x2a\x2a\x20\x42\x4f\x4f\x4f\x4f\x4f\x4f\x4f\x4f\x4f\x4f\x4f\x4d\x20\x2a\x2a");
      YoureDead(); // ##### RIP #####
    }
  }
}



void MatchRoutine(void)
{
  if (MatchTurnsLeft == 0) return;

  MatchTurnsLeft--;
  if (MatchTurnsLeft == 0)
  {
    int prev_darkness;

    if (IsObjVisible(OBJ_MATCH))
      PrintCompLine("\x85\x6d\xaf\xfa\xc0\xe0\xe6\xca\x9e\xa5\x74\x2e");

    prev_darkness = IsPlayerInDarkness();
    Obj[OBJ_MATCH].prop &= ~PROP_LIT;
    if (IsPlayerInDarkness() != prev_darkness)
    {
      PrintNewLine();
      PrintPlayerRoomDesc(1);
    }
  }
}



void CeremonyBroken(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_ENTRANCE_TO_HADES)
    PrintCompLine("\x85\xd1\x6e\x73\x69\xca\x8a\xa2\x9a\x63\xac\x65\x6d\xca\xc4\x9a\x62\xc2\x6b\xd4\xb5\x8c\x81\x77\xf4\xc7\x68\x73\xb5\x61\x6d\xfe\xd5\xb0\xf7\xaa\xcd\x6b\xd4\xa3\xa6\x92\xb3\x6c\x75\x6d\x73\xc4\xaf\xd1\x6d\x70\x74\xb5\xa9\x73\x75\x6d\x9e\x96\x69\xb6\xce\xe8\xa5\xa1\x6a\xf3\xf1\x6e\x67\x2e");
}



void BellRungRoutine(void)
{
  if (BellRungCountdown == 0) return;
      BellRungCountdown--;
  if (BellRungCountdown == 0)
    CeremonyBroken();
}



void CandlesLitRoutine(void)
{
  if (CandlesLitCountdown == 0) return;
      CandlesLitCountdown--;
  if (CandlesLitCountdown == 0)
    CeremonyBroken();
}



void BellHotRoutine(void)
{
  if (BellHotCountdown == 0) return;
      BellHotCountdown--;
  if (BellHotCountdown == 0)
  {
    if (Obj[OBJ_YOU].loc == ROOM_ENTRANCE_TO_HADES)
      PrintCompLine("\x85\xef\xdf\xa3\x70\xfc\xbb\xa1\xbd\xc0\x61\xd7\xb3\xe9\xcf\xab\x64\xf2\x6e\x2e");

    Obj[OBJ_BELL].loc = ROOM_ENTRANCE_TO_HADES;
    Obj[OBJ_HOT_BELL].loc = 0;
  }
}



void HoldingGunkRoutine(void)
{
  if (Obj[OBJ_GUNK].loc == INSIDE + OBJ_YOU)
  {
    Obj[OBJ_GUNK].loc = 0;
    PrintCompLine("\x85\x73\xfd\xc1\x77\xe0\xda\xaf\xa0\xb6\xa7\x73\x75\x62\xc5\xad\xf0\xe2\xb5\x8c\x63\x72\x75\x6d\x62\xcf\xa1\xa7\xbd\xcc\xfe\xa6\xaf\x86\xb6\xbd\x75\x63\x68\x2e");
  }
}



void InRoomOnRainbowRoutine(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_ON_RAINBOW)
    ExitFound = 1;
}



void DomeRoomRoutine(void)
{
  if (Obj[OBJ_YOU].loc == ROOM_DOME_ROOM && YouAreDead)
  {
    PrintCompLine("\x41\xa1\x8f\xd4\xd1\xb6\x81\x64\xe1\x9e\x8f\x66\xf3\xea\xd0\xc5\xc2\x9c\x70\x75\xdf\xa3\xa1\x69\xd2\x66\xc2\xf9\xd0\xf8\xb9\xcc\xf4\xf8\x9c\x8f\x6f\xd7\xb6\x81\xf4\x69\xf5\x9c\x8c\x64\xf2\x6e\x2e\x0a");
    GoToRoutine(ROOM_TORCH_ROOM);
  }
}



void UpATreeRoutine(void)
{
  int obj, other_fell = 0, count = 0;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == ROOM_UP_A_TREE)
  {
    if (obj == OBJ_NEST)
    {
      if (Obj[obj].prop & PROP_MOVEDDESC)
      {
        count++;
        Obj[obj].loc = ROOM_PATH;
        if (Obj[OBJ_EGG].loc == INSIDE + OBJ_NEST)
        {
          other_fell = 1;
          Obj[OBJ_EGG].loc = 0;
          Obj[OBJ_BROKEN_EGG].loc = ROOM_PATH;
        }
      }
    }
    else if (obj == OBJ_EGG)
    {
      other_fell = 2;
      count++;
      Obj[OBJ_EGG].loc = 0;
      Obj[OBJ_BROKEN_EGG].loc = ROOM_PATH;
      Obj[OBJ_BROKEN_EGG].prop |= PROP_OPENABLE;
      Obj[OBJ_BROKEN_EGG].prop |= PROP_OPEN;
    }
    else
    {
      count++;
      Obj[obj].loc = ROOM_PATH;
    }
  }

  if (count == 1 && other_fell == 0)
    PrintCompLine("\x49\xa6\x66\xe2\x6c\xa1\xbd\x80\xe6\xc2\xf6\x64\x2e");
  else if (count > 1)
    PrintCompLine("\x99\xc4\x66\xe2\xea\xbd\x80\xe6\xc2\xf6\x64\x2e");

  if (other_fell == 1)
    PrintCompLine("\x85\xed\xc5\xc6\xe2\x6c\xa1\xbd\x80\xe6\xc2\xf6\x64\xb5\x8c\x81\x65\x67\xc1\x73\x70\x69\xdf\xa1\xa5\xa6\xdd\xa8\x74\xb5\xd6\xf1\xa5\x73\xec\xcc\x61\x6d\x61\x67\x65\x64\x2e");
  else if (other_fell == 2)
    PrintCompLine("\x85\x65\x67\xc1\x66\xe2\x6c\xa1\xbd\x80\xe6\xc2\xf6\xab\x8c\x73\x70\xf1\xb1\xa1\x6f\xfc\x6e\xb5\xd6\xf1\xa5\x73\xec\xcc\x61\x6d\x61\x67\x65\x64\x2e");
}



void SongbirdRoutine(void)
{
  if (AreYouInForest() && PercentChance(15, -1))
    PrintCompLine("\x8b\xa0\xbb\xa8\xb4\x81\x64\xb2\x74\xad\x63\x9e\x81\xfa\x69\x72\x70\x84\xdd\xa3\xaa\xca\xc1\x62\x69\x72\x64\x2e");
}



void WaterSpilledRoutine(void)
{
  if (Obj[OBJ_WATER].loc == Obj[OBJ_YOU].loc)
  {
    Obj[OBJ_WATER].loc = 0;
    PrintCompLine("\x85\x77\xaf\xac\xaa\x70\x69\xdf\xa1\xbd\x80\xc6\xd9\xd3\x8d\xfb\x76\x61\x70\xd3\xaf\x65\x73\x2e");
  }
}



void CyclopsRoomRoutine(void)
{
  if (Obj[OBJ_YOU].loc != ROOM_CYCLOPS_ROOM)
    {CyclopsCounter = 0; return;}

  if (CyclopsState >= 3 ||                  // asleep or fled
      VillainAttacking[VILLAIN_CYCLOPS] ||  // attacking
      Obj[OBJ_CYCLOPS].loc == 0)            // dead
    return;

  CyclopsCounter++;

  if (CyclopsState >= 1) // hungry or thirsty
  {
    switch (CyclopsCounter - 1)
    {
      case 0: PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\xd6\x65\x6d\xa1\x73\xe1\x65\x77\xcd\xa6\x61\x67\xc7\xaf\x65\x64\x2e"); break;
      case 1: PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x61\x70\xfc\xbb\xa1\xbd\xb0\x9e\x67\x65\x74\xf0\x9c\x6d\xd3\x9e\x61\x67\xc7\xaf\x65\x64\x2e"); break;
      case 2: PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x9a\x6d\x6f\x76\x84\x61\x62\xa5\xa6\x81\xc2\xe1\xb5\xd9\x6f\x6b\x84\x66\xd3\xaa\xe1\x65\xa2\x97\x2e"); break;
      case 3: PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x77\xe0\xcb\xe9\x6b\x84\x66\xd3\xaa\xe2\xa6\x8c\xfc\x70\xfc\x72\xa4\x4e\xba\x64\xa5\x62\xa6\x96\xc4\xbb\x9e\x63\xca\x64\x69\x6d\xd4\x74\xa1\x66\xd3\xc0\x9a\x75\x70\x63\xe1\x84\x73\x6e\x61\x63\x6b\x2e"); break;
      case 4: PrintCompLine("\x85\x63\x79\x63\xd9\x70\xa1\x9a\x6d\x6f\x76\x84\xbd\x77\xbb\xab\x8f\xa7\xa3\xb4\xf6\x66\xf1\xd4\x64\xec\xee\xad\xed\x72\x2e"); break;
      case 5: PrintCompLine("\x8b\xcd\xd7\x9f\x77\xba\xfa\x6f\x69\x63\xbe\x3a\x20\x31\xa4\x4c\xbf\xd7\x20\x20\x32\xa4\x42\x65\x63\xe1\x9e\x64\xa7\xed\x72\x2e"); break;
      case 6: PrintCompLine("\x85\x63\x79\x63\xd9\x70\x73\xb5\xf0\xa9\xab\xdd\xa3\xdf\x8a\x92\xe6\x61\x6d\xbe\x8d\x9f\xf1\x63\x6b\xac\x79\xb5\x67\xf4\x62\xa1\x8f\x66\x69\x72\x6d\xec\xa4\x41\xa1\x94\xf5\x63\x6b\xa1\xce\xa1\xfa\x6f\x70\x73\xb5\x94\x73\x61\x79\xa1\x22\x4d\x6d\x6d\xa4\x4a\xfe\xa6\xf5\x6b\x9e\x4d\xe1\x20\xfe\xd5\x89\x6d\x61\x6b\x9e\x27\x65\x6d\x2e\x22\x20\x49\x74\x27\xa1\x6e\x69\x63\x9e\xbd\xb0\x9e\x61\x70\x70\xa9\x63\x69\xaf\x65\x64\x2e"); break;
    }

    if (CyclopsCounter == 7)
      YoureDead(); // ##### RIP #####
  }
  else if (CyclopsCounter == 5)
  {
    CyclopsCounter = 0;
    CyclopsState = 1; // hungry
  }
}

//-----------------------------------------------------------------------------

void ScoreUpdateRoutine(void)
{
  int i, old_score;

  old_score = Score;


  for (i=0; i<NUM_TREASURESCORES; i++)
  {
    int loc = Obj[TreasureScore[i].obj].loc;

    if (loc == INSIDE + OBJ_YOU && (TreasureScore[i].flags & 1) == 0)
    {
      TreasureScore[i].flags |= 1;
      Score += TreasureScore[i].take_value;
    }
    else if (loc == INSIDE + OBJ_TROPHY_CASE && (TreasureScore[i].flags & 2) == 0)
    {
      TreasureScore[i].flags |= 2;
      Score += TreasureScore[i].case_value;
    }
  }


  for (i=0; i<NUM_ROOMSCORES; i++)
    if (Obj[OBJ_YOU].loc == RoomScore[i].room && RoomScore[i].flag == 0)
  {
    RoomScore[i].flag = 1;
    Score += RoomScore[i].value;
  }


  if (Score - old_score > 0)
  {
    // PrintCompText("\x5b\xdc\xd8\xaa\x63\xd3\x9e\x6a\xfe\xa6\x77\xd4\xa6\x75\x70\xb0\x79\x20");
    // PrintInteger(Score - old_score);
    // if (Score - old_score == 1) PrintCompLine("\xeb\x6f\xa7\x74\x2e\x5d");
    // else                        PrintCompLine("\xeb\x6f\xa7\x74\x73\x2e\x5d");
  }


  if (Score == SCORE_MAX && WonGame == 0)
  {
    WonGame = 1;
    Obj[OBJ_MAP].prop &= ~PROP_NODESC;
    Obj[OBJ_MAP].prop &= ~PROP_NOTTAKEABLE;
    Room[ROOM_WEST_OF_HOUSE].prop &= ~R_DESCRIBED;
    PrintCompLine("\x41\xb4\xe2\x6d\x6f\xc5\xa8\x6e\x61\x75\x64\x69\x62\xcf\x20\x76\x6f\x69\x63\x9e\x77\xce\x73\xfc\x72\xa1\xa7\x86\xb6\xbf\x72\xb5\x22\x4c\xe9\x6b\x89\x92\x9f\xa9\xe0\xd8\xbe\xc6\xd3\x80\xc6\xa7\xe2\xaa\x65\x63\xa9\x74\x2e\x22");
  }
}

//-----------------------------------------------------------------------------

//run event routines after each action that set time-passed flag
void RunEventRoutines(void)
{
  SwordRoutine();
  LampDrainRoutine();
  CandlesShrinkRoutine();
  ReservoirFillRoutine();
  ReservoirDrainRoutine();
  SinkingObjectsRoutine(); // must be called after reservoir fill/drain routines
  LoudRoomRoutine();
  MaintenanceLeakRoutine();
  BoatPuncturedRoutine();
  BuoyRoutine(); // should be called before downstream routine because of message order
  DownstreamRoutine();
  BatRoomRoutine();
  LeavesTakenRoutine();
  GasRoomRoutine(); // must be called before match routine
  MatchRoutine();
  BellRungRoutine();
  CandlesLitRoutine();
  BellHotRoutine();
  HoldingGunkRoutine();
  InRoomOnRainbowRoutine();
  DomeRoomRoutine();
  UpATreeRoutine();
  SongbirdRoutine();
  WaterSpilledRoutine();
  CyclopsRoomRoutine();
  ScoreUpdateRoutine();

  VillainsRoutine();
}
//*****************************************************************************



//*****************************************************************************
int GetScore(void)
{
  return Score;
}



int GetMaxScore(void)
{
  return SCORE_MAX;
}



char *GetRankName(void)
{
       if (Score == 350) return "Master Adventurer";
  else if (Score >  330) return "Wizard";
  else if (Score >  300) return "Master";
  else if (Score >  200) return "Adventurer";
  else if (Score >  100) return "Junior Adventurer";
  else if (Score >   50) return "Novice Adventurer";
  else if (Score >   25) return "Amateur Adventurer";
  else                   return "Beginner";
}

//*****************************************************************************



//*****************************************************************************

#define SAVESTATE  {                                               \
  READWRITE(p, &RugMoved          , sizeof(unsigned char));        \
  READWRITE(p, &TrapOpen          , sizeof(unsigned char));        \
  READWRITE(p, &ExitFound         , sizeof(unsigned char));        \
  READWRITE(p, &KitchenWindowOpen , sizeof(unsigned char));        \
  READWRITE(p, &GratingRevealed   , sizeof(unsigned char));        \
  READWRITE(p, &GratingUnlocked   , sizeof(unsigned char));        \
  READWRITE(p, &GratingOpen       , sizeof(unsigned char));        \
  READWRITE(p, &GatesOpen         , sizeof(unsigned char));        \
  READWRITE(p, &LowTide           , sizeof(unsigned char));        \
  READWRITE(p, &GatesButton       , sizeof(unsigned char));        \
  READWRITE(p, &LoudRoomQuiet     , sizeof(unsigned char));        \
  READWRITE(p, &RainbowSolid      , sizeof(unsigned char));        \
  READWRITE(p, &WonGame           , sizeof(unsigned char));        \
  READWRITE(p, &MirrorBroken      , sizeof(unsigned char));        \
  READWRITE(p, &RopeTiedToRail    , sizeof(unsigned char));        \
  READWRITE(p, &SpiritsBanished   , sizeof(unsigned char));        \
  READWRITE(p, &TrollAllowsPassage, sizeof(unsigned char));        \
  READWRITE(p, &YouAreSanta       , sizeof(unsigned char));        \
  READWRITE(p, &YouAreInBoat      , sizeof(unsigned char));        \
  READWRITE(p, &NotLucky          , sizeof(unsigned char));        \
  READWRITE(p, &YouAreDead        , sizeof(unsigned char));        \
  READWRITE(p, &SongbirdSang      , sizeof(unsigned char));        \
  READWRITE(p, &ThiefHere         , sizeof(unsigned char));        \
  READWRITE(p, &ThiefEngrossed    , sizeof(unsigned char));        \
  READWRITE(p, &YouAreStaggered   , sizeof(unsigned char));        \
  READWRITE(p, &BuoyFlag          , sizeof(unsigned char));        \
  READWRITE(p, &NumMoves               , sizeof(int));             \
  READWRITE(p, &LampTurnsLeft          , sizeof(int));             \
  READWRITE(p, &MatchTurnsLeft         , sizeof(int));             \
  READWRITE(p, &CandleTurnsLeft        , sizeof(int));             \
  READWRITE(p, &MatchesLeft            , sizeof(int));             \
  READWRITE(p, &ReservoirFillCountdown , sizeof(int));             \
  READWRITE(p, &ReservoirDrainCountdown, sizeof(int));             \
  READWRITE(p, &MaintenanceWaterLevel  , sizeof(int));             \
  READWRITE(p, &DownstreamCounter      , sizeof(int));             \
  READWRITE(p, &BellRungCountdown      , sizeof(int));             \
  READWRITE(p, &CandlesLitCountdown    , sizeof(int));             \
  READWRITE(p, &BellHotCountdown       , sizeof(int));             \
  READWRITE(p, &CaveHoleDepth          , sizeof(int));             \
  READWRITE(p, &Score                  , sizeof(int));             \
  READWRITE(p, &NumDeaths              , sizeof(int));             \
  READWRITE(p, &CyclopsCounter         , sizeof(int));             \
  READWRITE(p, &CyclopsState           , sizeof(int));             \
  READWRITE(p, &LoadAllowed            , sizeof(int));             \
  READWRITE(p, &PlayerStrength         , sizeof(int));             \
  READWRITE(p, &TrollDescType          , sizeof(int));             \
  READWRITE(p, &ThiefDescType          , sizeof(int));             \
  READWRITE(p, &EnableCureRoutine      , sizeof(int));             \
  for (i=0; i<NUM_VILLAINS; i++) {                                 \
    READWRITE(p, &   VillainAttacking[i], sizeof(unsigned char));  \
    READWRITE(p, &   VillainStaggered[i], sizeof(unsigned char));  \
    READWRITE(p, &VillainWakingChance[i], sizeof(int));            \
    READWRITE(p, &    VillainStrength[i], sizeof(int)); }          \
  for (i=0; i<NUM_TREASURESCORES; i++)                             \
    READWRITE(p, &TreasureScore[i].flags, sizeof(unsigned char));  \
  for (i=0; i<NUM_ROOMSCORES; i++)                                 \
    READWRITE(p, &RoomScore[i].flag, sizeof(unsigned char));       \
  for (i=0; i<NUM_ROOMS; i++)                                      \
    READWRITE(p, &Room[i].prop, sizeof(unsigned short));           \
  for (i=0; i<NUM_OBJECTS; i++) {                                  \
    READWRITE(p, &Obj[i].loc       , sizeof(unsigned short));      \
    READWRITE(p, &Obj[i].order     , sizeof(unsigned short));      \
    READWRITE(p, &Obj[i].prop      , sizeof(unsigned short));      \
    READWRITE(p, &Obj[i].thiefvalue, sizeof(unsigned char)); } }



int GetSaveStateSize(void)
{
  int i, p = 0;

#define READWRITE(p,q,size)  {p += size;}
  SAVESTATE
#undef READWRITE
  return p;
}



void ReadSaveState(char *p)
{
  int i;

#define READWRITE(p,q,size)  {memcpy((p), (q), (size)); p += size;}
  SAVESTATE
#undef READWRITE
}



void WriteSaveState(char *p)
{
  int i;

#define READWRITE(p,q,size)  {memcpy((q), (p), (size)); p += size;}
  SAVESTATE
#undef READWRITE
}



void InitGameState(void)
{
  int i;

  RugMoved           = 0;
  TrapOpen           = 0;
  ExitFound          = 0;
  KitchenWindowOpen  = 0;
  GratingRevealed    = 0;
  GratingUnlocked    = 0;
  GratingOpen        = 0;
  GatesOpen          = 0;
  LowTide            = 0;
  GatesButton        = 0;
  LoudRoomQuiet      = 0;
  RainbowSolid       = 0;
  WonGame            = 0;
  MirrorBroken       = 0;
  RopeTiedToRail     = 0;
  SpiritsBanished    = 0;
  TrollAllowsPassage = 0;
  YouAreSanta        = 0;
  YouAreInBoat       = 0;
  NotLucky           = 0;
  YouAreDead         = 0;
  SongbirdSang       = 0;
  ThiefHere          = 0;
  ThiefEngrossed     = 0;
  YouAreStaggered    = 0;
  BuoyFlag           = 0;

  NumMoves                = 0;
  LampTurnsLeft           = 200;
  MatchTurnsLeft          = 0;
  CandleTurnsLeft         = 40;
  MatchesLeft             = 6;
  ReservoirFillCountdown  = 0;
  ReservoirDrainCountdown = 0;
  MaintenanceWaterLevel   = 0;
  DownstreamCounter       = 0;
  BellRungCountdown       = 0;
  CandlesLitCountdown     = 0;
  BellHotCountdown        = 0;
  CaveHoleDepth           = 0;
  Score                   = 0;
  NumDeaths               = 0;
  CyclopsCounter          = 0;
  CyclopsState            = 0;
  LoadAllowed             = 100;
  PlayerStrength          = 0;
  TrollDescType           = 0;
  ThiefDescType           = 0;
  EnableCureRoutine       = 0;

  for (i=0; i<NUM_VILLAINS; i++)
  {
    VillainAttacking[i]    = 0;
    VillainStaggered[i]    = 0;
    VillainWakingChance[i] = 0;
  }

  VillainStrength[VILLAIN_TROLL  ] = 2;
  VillainStrength[VILLAIN_THIEF  ] = 5;
  VillainStrength[VILLAIN_CYCLOPS] = 10000;

  for (i=0; i<NUM_TREASURESCORES; i++)
    TreasureScore[i].flags = 0;

  for (i=0; i<NUM_ROOMSCORES; i++)
    RoomScore[i].flag = 0;

  for (i=0; i<NUM_ROOMS; i++)
    Room[i].prop = Room[i].init_prop;

  for (i=0; i<NUM_OBJECTS; i++)
  {
    Obj[i].loc = Obj[i].init_loc;
    Obj[i].order = i;
    Obj[i].prop = 0;
    Obj[i].thiefvalue = Obj[i].init_thiefvalue;
  }

  Obj[OBJ_CYCLOPS        ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_GHOSTS         ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_BAT            ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_THIEF          ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_TROLL          ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_LOWERED_BASKET ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_RAISED_BASKET  ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_TROPHY_CASE    ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_MACHINE        ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_MAILBOX        ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_KITCHEN_TABLE  ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_ATTIC_TABLE    ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_TRUNK          ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_HOT_BELL       ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_POT_OF_GOLD    ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_SCARAB         ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_MAP            ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_TOOL_CHEST     ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_ENGRAVINGS     ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_WATER          ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_STILETTO       ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_LARGE_BAG      ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_AXE            ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_ZORKMID        ].prop |= PROP_NOTTAKEABLE;
  Obj[OBJ_GRUE           ].prop |= PROP_NOTTAKEABLE;

  Obj[OBJ_THIEF          ].prop |= PROP_NODESC;
  Obj[OBJ_TROPHY_CASE    ].prop |= PROP_NODESC;
  Obj[OBJ_MACHINE        ].prop |= PROP_NODESC;
  Obj[OBJ_KITCHEN_TABLE  ].prop |= PROP_NODESC;
  Obj[OBJ_ATTIC_TABLE    ].prop |= PROP_NODESC;
  Obj[OBJ_TRUNK          ].prop |= PROP_NODESC;
  Obj[OBJ_POT_OF_GOLD    ].prop |= PROP_NODESC;
  Obj[OBJ_SCARAB         ].prop |= PROP_NODESC;
  Obj[OBJ_MAP            ].prop |= PROP_NODESC;
  Obj[OBJ_STILETTO       ].prop |= PROP_NODESC;
  Obj[OBJ_LARGE_BAG      ].prop |= PROP_NODESC;
  Obj[OBJ_AXE            ].prop |= PROP_NODESC;
  Obj[OBJ_ZORKMID        ].prop |= PROP_NODESC;
  Obj[OBJ_GRUE           ].prop |= PROP_NODESC;

  Obj[OBJ_TROPHY_CASE    ].prop |= PROP_OPENABLE;
  Obj[OBJ_MACHINE        ].prop |= PROP_OPENABLE;
  Obj[OBJ_MAILBOX        ].prop |= PROP_OPENABLE;
  Obj[OBJ_SANDWICH_BAG   ].prop |= PROP_OPENABLE;
  Obj[OBJ_BOTTLE         ].prop |= PROP_OPENABLE;
  Obj[OBJ_COFFIN         ].prop |= PROP_OPENABLE;
  Obj[OBJ_BUOY           ].prop |= PROP_OPENABLE;
  Obj[OBJ_LARGE_BAG      ].prop |= PROP_OPENABLE;
  Obj[OBJ_TUBE           ].prop |= PROP_OPENABLE;

  Obj[OBJ_KITCHEN_TABLE  ].prop |= PROP_OPEN;
  Obj[OBJ_ATTIC_TABLE    ].prop |= PROP_OPEN;
  Obj[OBJ_RAISED_BASKET  ].prop |= PROP_OPEN;
  Obj[OBJ_LOWERED_BASKET ].prop |= PROP_OPEN;
  Obj[OBJ_INFLATED_BOAT  ].prop |= PROP_OPEN;
  Obj[OBJ_NEST           ].prop |= PROP_OPEN;
  Obj[OBJ_LARGE_BAG      ].prop |= PROP_OPEN;
  Obj[OBJ_CHALICE        ].prop |= PROP_OPEN;
  Obj[OBJ_THIEF          ].prop |= PROP_OPEN;
  Obj[OBJ_TROLL          ].prop |= PROP_OPEN;
  Obj[OBJ_WATER          ].prop |= PROP_OPEN;

  Obj[OBJ_TORCH          ].prop |= PROP_LIT;
  Obj[OBJ_CANDLES        ].prop |= PROP_LIT;

  Obj[OBJ_SCEPTRE        ].prop |= PROP_INSIDEDESC;
  Obj[OBJ_MAP            ].prop |= PROP_INSIDEDESC;
  Obj[OBJ_EGG            ].prop |= PROP_INSIDEDESC;
  Obj[OBJ_CANARY         ].prop |= PROP_INSIDEDESC;
  Obj[OBJ_BROKEN_CANARY  ].prop |= PROP_INSIDEDESC;
  Obj[OBJ_SANDWICH_BAG   ].prop |= PROP_INSIDEDESC;
  Obj[OBJ_BOTTLE         ].prop |= PROP_INSIDEDESC;
  Obj[OBJ_KNIFE          ].prop |= PROP_INSIDEDESC;

  Obj[OBJ_ROPE           ].prop |= PROP_SACRED;
  Obj[OBJ_COFFIN         ].prop |= PROP_SACRED;
  Obj[OBJ_BAR            ].prop |= PROP_SACRED;

  Obj[OBJ_WATER          ].prop |= PROP_EVERYWHERE;
  Obj[OBJ_ZORKMID        ].prop |= PROP_EVERYWHERE;
  Obj[OBJ_GRUE           ].prop |= PROP_EVERYWHERE;

  Obj[OBJ_AXE            ].prop |= PROP_WEAPON;
  Obj[OBJ_STILETTO       ].prop |= PROP_WEAPON;
  Obj[OBJ_RUSTY_KNIFE    ].prop |= PROP_WEAPON;
  Obj[OBJ_SWORD          ].prop |= PROP_WEAPON;
  Obj[OBJ_KNIFE          ].prop |= PROP_WEAPON;
  Obj[OBJ_SCEPTRE        ].prop |= PROP_WEAPON;

  Obj[OBJ_CYCLOPS        ].prop |= PROP_ACTOR;
  Obj[OBJ_GHOSTS         ].prop |= PROP_ACTOR;
  Obj[OBJ_BAT            ].prop |= PROP_ACTOR;
  Obj[OBJ_THIEF          ].prop |= PROP_ACTOR;
  Obj[OBJ_TROLL          ].prop |= PROP_ACTOR;

  Obj[OBJ_PUMP           ].prop |= PROP_TOOL;
  Obj[OBJ_SCREWDRIVER    ].prop |= PROP_TOOL;
  Obj[OBJ_KEYS           ].prop |= PROP_TOOL;
  Obj[OBJ_SHOVEL         ].prop |= PROP_TOOL;
  Obj[OBJ_PUTTY          ].prop |= PROP_TOOL;
  Obj[OBJ_WRENCH         ].prop |= PROP_TOOL;

  Obj[OBJ_LEAVES         ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_BOOK           ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_SANDWICH_BAG   ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_ADVERTISEMENT  ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_INFLATED_BOAT  ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_PAINTING       ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_PUNCTURED_BOAT ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_INFLATABLE_BOAT].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_COAL           ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_BOAT_LABEL     ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_GUIDE          ].prop |= PROP_INFLAMMABLE;
  Obj[OBJ_NEST           ].prop |= PROP_INFLAMMABLE;

  Obj[OBJ_KITCHEN_TABLE  ].prop |= PROP_SURFACE;
  Obj[OBJ_ATTIC_TABLE    ].prop |= PROP_SURFACE;

  ItObj = OBJ_MAILBOX;
}
//*****************************************************************************
