// Zork I: The Great Underground Empire
// (c) 1980 by INFOCOM, Inc.
// C port and parser (c) 2021 by Donnie Russell II

// This source code is provided for personal, educational use only.
// You are welcome to use this source code to develop your own works,
// but the story-related content belongs to the original authors of Zork.



#include "_def.h"
#include "_tables.h"



#define HERO  0  // never set



int VillainObj[NUM_VILLAINS] = {OBJ_TROLL, OBJ_THIEF, OBJ_CYCLOPS};
char *VillainName[NUM_VILLAINS] = {"troll", "thief", "cyclops"};
int VillainBestWeaponAgainst[NUM_VILLAINS] = {OBJ_SWORD, OBJ_KNIFE, 0};
int VillainBestWeaponAgainstAdvantage[NUM_VILLAINS] = {1, 1, 0};



extern unsigned char RopeTiedToRail;
extern unsigned char TrollAllowsPassage;
extern unsigned char YouAreDead;
extern unsigned char ThiefHere;
extern unsigned char ThiefEngrossed;
extern unsigned char YouAreStaggered;

extern int Score;
extern int LoadAllowed;
extern int PlayerStrength;
extern int TrollDescType;
extern int ThiefDescType;
extern int EnableCureRoutine;

extern unsigned char VillainAttacking[NUM_VILLAINS];
extern unsigned char VillainStaggered[NUM_VILLAINS];
extern int VillainWakingChance[NUM_VILLAINS];
extern int VillainStrength[NUM_VILLAINS];



//*****************************************************************************

void ThiefRecoverStiletto(void)
{
  if (Obj[OBJ_STILETTO].loc == Obj[OBJ_THIEF].loc)
  {
    Obj[OBJ_STILETTO].loc = INSIDE + OBJ_THIEF;
    Obj[OBJ_STILETTO].prop |= PROP_NODESC;
    Obj[OBJ_STILETTO].prop |= PROP_NOTTAKEABLE;
  }
}



int ThiefRob(int loc, int prob)
{
  int flag = 0, obj;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == loc &&
        (Obj[obj].prop & PROP_NODESC) == 0 &&
        (Obj[obj].prop & PROP_SACRED) == 0 &&
        Obj[obj].thiefvalue > 0 &&
        (prob < 0 || PercentChance(prob, -1)))
  {
    flag = 1;
    Obj[obj].loc = INSIDE + OBJ_THIEF;
    Obj[obj].prop |= PROP_MOVEDDESC;
    Obj[obj].prop |= PROP_NODESC;
    Obj[obj].prop |= PROP_NOTTAKEABLE;
  }

  return flag;
}



int PlayerFightStrength(int adjust)
{
  int s =  STRENGTH_MIN + (STRENGTH_MAX - STRENGTH_MIN) * Score / SCORE_MAX;

  if (adjust)
    s += PlayerStrength;

  return s;
}



int VillainFightStrength(int i, int player_weapon)
{
  int obj, strength;

  obj = VillainObj[i];
  strength = VillainStrength[i];

  if (strength >= 0)
  {
    if (obj == OBJ_THIEF && ThiefEngrossed)
    {
      ThiefEngrossed = 0;
      if (strength > 2) strength = 2;
    }

    if (player_weapon &&
        (Obj[player_weapon].prop & PROP_WEAPON) &&
        player_weapon == VillainBestWeaponAgainst[i])
    {
      strength -= VillainBestWeaponAgainstAdvantage[i];
      if (strength < 1) strength = 1;
    }
  }

  return strength;
}



int ThiefWinning(void)
{
  int vs = VillainStrength[VILLAIN_THIEF];
  int ps = vs - PlayerFightStrength(1);

       if (ps >  3) return PercentChance(90, -1);
  else if (ps >  0) return PercentChance(75, -1);
  else if (ps == 0) return PercentChance(50, -1);
  else if (vs >  1) return PercentChance(25, -1);
  else              return PercentChance(10, -1);
}



int ThiefVsAdventurer(int here)
{
  int prev_darkness;
  int robbed = 0; // 1: player  2: room

  if (YouAreDead == 0 && Obj[OBJ_YOU].loc == ROOM_TREASURE_ROOM)
  {
  }
  else if (ThiefHere == 0)
  {
    if (YouAreDead == 0 && here == 0 && PercentChance(30, -1))
    {
      if (Obj[OBJ_STILETTO].loc == INSIDE + OBJ_THIEF)
      {
        Obj[OBJ_THIEF].prop &= ~PROP_NODESC;
        PrintCompLine("\x53\xe1\x65\xca\x9e\xe7\x72\x72\x79\x84\xd0\xfd\x72\x67\x9e\x62\x61\xc1\x9a\xe7\x73\x75\xe2\xec\xcb\xbf\x6e\x84\x61\x67\x61\xa7\xc5\xae\xed\x8a\x81\x77\xe2\x6c\xa1\xa0\xa9\xa4\x48\x9e\x64\x6f\xbe\xe4\xff\xaa\xfc\x61\x6b\xb5\x62\xf7\xa8\xa6\x9a\x63\xcf\xbb\xc6\xc2\xf9\xce\xa1\xe0\xfc\x63\xa6\xa2\xaf\x80\xb0\x61\xc1\xf8\xdf\xb0\x9e\x74\x61\x6b\xd4\xae\x6e\xec\xae\xd7\xb6\xce\xa1\xe8\x61\xab\x62\x6f\x64\x79\x2e");
        ThiefHere = 1;
        return 1;
      }
      else
      {
        Obj[OBJ_STILETTO].loc = INSIDE + OBJ_THIEF;
        Obj[OBJ_STILETTO].prop |= PROP_NODESC;
        Obj[OBJ_STILETTO].prop |= PROP_NOTTAKEABLE;
        Obj[OBJ_THIEF].prop &= ~PROP_NODESC;
        PrintCompLine("\x8b\x66\xf3\xea\xd0\xf5\x67\x68\xa6\x66\x97\xac\x2d\xbd\x75\xfa\xb5\x8c\x74\xd8\x6e\x97\xb5\xe3\xf0\x63\x9e\xd0\x67\xf1\x6e\x6e\x84\x66\x69\x67\xd8\x9e\x68\x6f\x6c\x64\x84\xd0\xfd\x72\x67\x9e\x62\x61\xc1\xa7\xae\xed\xc0\x8c\x8c\xd0\xc5\x69\xcf\x74\xbd\xa8\xb4\x81\xff\xa0\x72\x2e");
        ThiefHere = 1;
        return 1;
      }
    }
    else if (here && VillainAttacking[VILLAIN_THIEF] && ThiefWinning() == 0)
    {
      PrintCompLine("\xdc\xd8\xae\x70\x70\xca\xd4\x74\xb5\xe8\xd1\x72\x6d\xa7\x84\x64\xb2\x63\xa9\xf0\xca\x89\xef\x80\xb0\x65\x74\xd1\xb6\x70\xbb\xa6\xdd\x20\x76\xe2\xd3\xb5\xe8\x63\x69\xe8\xa1\xbd\x9f\xac\x6d\xa7\xaf\x9e\xa2\x9a\xf5\x74\x74\xcf\xb3\xca\x74\xa9\xd1\x6d\x70\x73\xa4\x57\xc7\xde\xd0\x72\x75\x65\x66\x75\xea\xe3\xab\xdd\xc0\x9a\xa0\x61\x64\xb5\x94\xc5\x65\x70\xa1\x62\x61\x63\x6b\x77\xbb\xab\xa7\xbd\x80\xe6\xd9\xe1\x8d\xcc\xb2\x61\x70\xfc\xbb\x73\x2e");
      Obj[OBJ_THIEF].prop |= PROP_NODESC;
      VillainAttacking[VILLAIN_THIEF] = 0;
      ThiefRecoverStiletto();
      return 1;
    }
    else if (here && VillainAttacking[VILLAIN_THIEF] && PercentChance(90, -1))
      return 0;
    else if (here && PercentChance(30, -1))
    {
      PrintCompLine("\x85\x68\x6f\x6c\xe8\xb6\xdd\x80\xcb\xbb\x67\x9e\x62\x61\xc1\x6a\xfe\xa6\xcf\x66\x74\xb5\xd9\x6f\x6b\x84\x64\xb2\x67\xfe\xd1\x64\xa4\x46\xd3\x74\xf6\xaf\x65\xec\xb5\x94\xbd\x6f\x6b\xe4\xff\xce\x6e\x67\x2e");
      Obj[OBJ_THIEF].prop |= PROP_NODESC;
      ThiefRecoverStiletto();
      return 1;
    }
    else if (PercentChance(70, -1))
      return 0;
    else if (YouAreDead == 0)
    {
      prev_darkness = IsPlayerInDarkness();

           if (ThiefRob(Obj[OBJ_YOU].loc, 100))  robbed = 2; // room
      else if (ThiefRob(INSIDE + OBJ_YOU,  -1))  robbed = 1; // player

      ThiefHere = 1;

      if (robbed && here == 0)
      {
        PrintCompText("\x41\xaa\xf3\x64\x79\x2d\xd9\x6f\x6b\x84\xa7\x64\x69\x76\x69\x64\x75\xe2\xb7\xc7\xde\xd0\xfd\x72\x67\x9e\x62\x61\xc1\x6a\xfe\xa6\x77\xad\xe8\xa9\xab\xa2\xc2\x75\x67\xde\x81\xc2\xe1\xa4\x4f\xb4\x81\x77\x61\xc4\xa2\xc2\x75\x67\x68\xb5\x94\x71\x75\x69\x65\x74\xec\xa3\x62\xc5\xf4\x63\xd1\xab\x73\xe1\x9e\x76\xe2\x75\x61\x62\xcf\xa1\x66\xc2\x6d\x20");

        if (robbed == 2)
          PrintCompText("\x81\xc2\x6f\x6d");
        else
          PrintCompText("\x92\xeb\x6f\x73\xd6\x73\x73\x69\x6f\x6e");

        PrintCompLine("\xb5\x6d\x75\x6d\x62\xf5\x9c\x73\xe1\x65\xa2\x84\x61\x62\xa5\xa6\x22\x44\x6f\x84\xf6\xbd\xae\x96\x72\xa1\xef\x66\xd3\x65\x2e\x2e\x2e\x22");

        if (IsPlayerInDarkness() != prev_darkness)
          PrintCompLine("\x85\xa2\x69\x65\xd2\xd6\x65\x6d\xa1\xbd\xc0\x61\xd7\xcb\x65\x66\xa6\x8f\xa7\x80\xcc\xbb\x6b\x2e");
      }
      else if (here)
      {
        ThiefRecoverStiletto();

        if (robbed)
        {
          PrintCompText("\x85\xa2\x69\x65\xd2\x6a\xfe\xa6\xcf\x66\x74\xb5\xc5\x69\xdf\xb3\xbb\x72\x79\x84\xce\xa1\xfd\x72\x67\x9e\x62\x61\x67\x8e\xc3\x6d\x61\xc4\xe3\xa6\xcd\xd7\xe4\xff\x69\x63\xd5\x95\xaf\xc0\x65\x20");

          if (robbed == 2)
            PrintCompLine("\x61\x70\x70\xc2\x70\xf1\xaf\xd5\x80\x20\x76\xe2\x75\x61\x62\xcf\xa1\xa7\x80\xda\xe9\x6d\x2e");
          else
            PrintCompLine("\xc2\x62\xef\xab\x8f\x62\xf5\xb9\xc6\x69\x72\x73\x74\x2e");

          if (IsPlayerInDarkness() != prev_darkness)
            PrintCompLine("\x85\xa2\x69\x65\xd2\xd6\x65\x6d\xa1\xbd\xc0\x61\xd7\xcb\x65\x66\xa6\x8f\xa7\x80\xcc\xbb\x6b\x2e");
        }
        else
          PrintCompLine("\x85\xa2\x69\x65\x66\xb5\x66\xa7\x64\x84\xe3\xa2\x84\xdd\x20\x76\xe2\x75\x65\xb5\xcf\x66\xa6\x64\xb2\x67\xfe\xd1\x64\x2e");

        Obj[OBJ_THIEF].prop |= PROP_NODESC;
        here = 0;
        return 1;
      }
      else
      {
        PrintCompLine("\x41\x20\x22\xcf\xad\x8d\xc0\xf6\x67\x72\x79\x22\xe6\xd4\x74\xcf\x6d\xad\x20\x6a\xfe\xa6\x77\xad\xe8\xa9\xab\xa2\xc2\x75\x67\x68\xb5\xe7\x72\x72\x79\x84\xd0\xfd\x72\x67\x9e\x62\x61\x67\xa4\x46\xa7\x64\x84\xe3\xa2\x84\xdd\x20\x76\xe2\x75\x65\xb5\x94\xcf\x66\xa6\x64\xb2\x67\x72\xf6\x74\xcf\x64\x2e");
        return 1;
      }
    }
  }
  else
  {
    if (here)
    {
      if (PercentChance(30, -1))
      {
        prev_darkness = IsPlayerInDarkness();

             if (ThiefRob(Obj[OBJ_YOU].loc, 100))  robbed = 2; // room
        else if (ThiefRob(INSIDE + OBJ_YOU,  -1))  robbed = 1; // player

        if (robbed)
        {
          PrintCompText("\x85\xa2\x69\x65\xd2\x6a\xfe\xa6\xcf\x66\x74\xb5\xc5\x69\xdf\xb3\xbb\x72\x79\x84\xce\xa1\xfd\x72\x67\x9e\x62\x61\x67\x8e\xc3\x6d\x61\xc4\xe3\xa6\xcd\xd7\xe4\xff\x69\x63\xd5\x95\xaf\xc0\x65\x20");

          if (robbed == 2)
            PrintCompLine("\x61\x70\x70\xc2\x70\xf1\xaf\xd5\x80\x20\x76\xe2\x75\x61\x62\xcf\xa1\xa7\x80\xda\xe9\x6d\x2e");
          else
            PrintCompLine("\xc2\x62\xef\xab\x8f\x62\xf5\xb9\xc6\x69\x72\x73\x74\x2e");

          if (IsPlayerInDarkness() != prev_darkness)
            PrintCompLine("\x85\xa2\x69\x65\xd2\xd6\x65\x6d\xa1\xbd\xc0\x61\xd7\xcb\x65\x66\xa6\x8f\xa7\x80\xcc\xbb\x6b\x2e");
        }
        else
          PrintCompLine("\x85\xa2\x69\x65\x66\xb5\x66\xa7\x64\x84\xe3\xa2\x84\xdd\x20\x76\xe2\x75\x65\xb5\xcf\x66\xa6\x64\xb2\x67\xfe\xd1\x64\x2e");

        Obj[OBJ_THIEF].prop |= PROP_NODESC;
        here = 0;
        ThiefRecoverStiletto();
      }
    }
  }

  return 0;
}



int ThiefDepositBooty(int room)
{
  int flag = 0, obj;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == INSIDE + OBJ_THIEF &&
        Obj[obj].thiefvalue > 0 &&
        obj != OBJ_STILETTO &&
        obj != OBJ_LARGE_BAG)
  {
    flag = 1;
    Obj[obj].loc = room;
    if (obj == OBJ_EGG)
      Obj[OBJ_EGG].prop |= PROP_OPEN;
  }

  return flag;
}



int ThiefDropJunk(int room)
{
  int flag = 0, obj;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == INSIDE + OBJ_THIEF &&
        Obj[obj].thiefvalue == 0 &&
        PercentChance(30, -1) &&
        obj != OBJ_STILETTO &&
        obj != OBJ_LARGE_BAG)
  {
    if (flag == 0 && room == Obj[OBJ_YOU].loc)
    {
      flag = 1;
      PrintCompLine("\x85\xc2\x62\xef\x72\xb5\x72\x75\x6d\x6d\x61\x67\x84\xa2\xc2\x75\x67\xde\xce\xa1\x62\x61\x67\xb5\x64\xc2\x70\xfc\xab\xd0\x66\x65\x77\xa8\xd1\x6d\xa1\x94\x66\xa5\xb9\x20\x76\xe2\x75\x65\xcf\x73\x73\x2e");
    }
    Obj[obj].loc = room;
    Obj[obj].prop &= ~PROP_NODESC;
    Obj[obj].prop &= ~PROP_NOTTAKEABLE;
  }

  return flag;
}



void ThiefHackTreasures(void)
{
  int obj;

  ThiefRecoverStiletto();

  Obj[OBJ_THIEF].prop |= PROP_NODESC;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == ROOM_TREASURE_ROOM &&
        obj != OBJ_CHALICE &&
        obj != OBJ_THIEF)
  {
    Obj[obj].prop &= ~PROP_NODESC;
    Obj[obj].prop &= ~PROP_NOTTAKEABLE;
  }
}



void ThiefRobMaze(int room)
{
  int obj;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == room &&
        (Obj[obj].prop & PROP_NODESC) == 0 &&
        (Obj[obj].prop & PROP_NOTTAKEABLE) == 0 &&
        PercentChance(40, -1))
  {
    PrintCompLine("\x8b\xa0\xbb\xb5\xdd\xd2\xa7\x80\xcc\xb2\x74\xad\x63\x65\xb5\x73\xe1\x65\xca\x9e\x73\x61\x79\x84\x22\x4d\x79\xb5\x49\xb7\xca\xe8\xb6\x77\xcd\xa6\xa2\x9a\x66\xa7\x9e\xc7\x65\xf9\x9a\x64\x6f\x84\xa0\xa9\x2e\x22");
    if (PercentChance(60, 80))
    {
      Obj[obj].loc = INSIDE + OBJ_THIEF;
      Obj[obj].prop |= PROP_MOVEDDESC;
      Obj[obj].prop |= PROP_NODESC;
      Obj[obj].prop |= PROP_NOTTAKEABLE;
    }
    break;
  }
}



void ThiefStealJunk(int room)
{
  int obj;

  for (obj=2; obj<NUM_OBJECTS; obj++)
    if (Obj[obj].loc == room &&
        Obj[obj].thiefvalue == 0 &&
        (Obj[obj].prop & PROP_NODESC) == 0 &&
        (Obj[obj].prop & PROP_NOTTAKEABLE) == 0 &&
        (Obj[obj].prop & PROP_SACRED) == 0 &&
        (obj == OBJ_STILETTO || PercentChance(10, -1)))
  {
    Obj[obj].loc = INSIDE + OBJ_THIEF;
    Obj[obj].prop |= PROP_MOVEDDESC;
    Obj[obj].prop |= PROP_NODESC;
    Obj[obj].prop |= PROP_NOTTAKEABLE;

    if (obj == OBJ_ROPE) // will never happen because it's sacred
      RopeTiedToRail = 0;

    if (room == Obj[OBJ_YOU].loc)
      PrintCompLine("\x8b\x73\x75\x64\xe8\x6e\xec\xe4\xff\x69\x63\x9e\xa2\xaf\xaa\xe1\x65\xa2\x84\x76\xad\xb2\xa0\x64\x2e");

    break;
  }
}



void ThiefRoutine(void)
{
  int room, here, once = 0;


  // if thief is dead or unconcious
  if (Obj[OBJ_THIEF].loc == 0 ||
      ThiefDescType == 1) // unconcious
    return;


  for (;;) // used only to allow use of break instead of goto
  {
    room = Obj[OBJ_THIEF].loc;
    here = ((Obj[OBJ_THIEF].prop & PROP_NODESC) == 0);

    if (here)
      room = Obj[OBJ_THIEF].loc;

    if (room == ROOM_TREASURE_ROOM &&
        room != Obj[OBJ_YOU].loc)
    {
      if (here)
      {
        here = 0;
        ThiefHackTreasures();
      }
      ThiefDepositBooty(ROOM_TREASURE_ROOM);
    }
    else if (Obj[OBJ_YOU].loc == room &&
             (Room[room].prop & R_LIT) == 0 &&
             Obj[OBJ_TROLL].loc != Obj[OBJ_YOU].loc)
    {
      if (ThiefVsAdventurer(here))
        break; // break out of for(;;)

      if (Obj[OBJ_THIEF].prop & PROP_NODESC)
        here = 0;
    }
    else
    {
      if (Obj[OBJ_THIEF].loc == room &&
          (Obj[OBJ_THIEF].prop & PROP_NODESC) == 0)
      {
        Obj[OBJ_THIEF].prop |= PROP_NODESC;
        here = 0;
      }

      if (Room[room].prop & R_DESCRIBED)
      {
        ThiefRob(room, 75);

        if ((Room[room].prop & R_MAZE) &&
            (Room[Obj[OBJ_YOU].loc].prop & R_MAZE))
          ThiefRobMaze(room);
        else
          ThiefStealJunk(room);
      }
    }

    once = 1-once;
    if (once && here == 0)
    {
      ThiefRecoverStiletto();

      for (;;)
      {
        room++;
        if (room == NUM_ROOMS) room = 1;

        if ((Room[room].prop & R_SACRED) == 0 &&
            (Room[room].prop & R_BODYOFWATER) == 0)
        {
          Obj[OBJ_THIEF].loc = room;
          Obj[OBJ_THIEF].prop |= PROP_NODESC;
          VillainAttacking[VILLAIN_THIEF] = 0;
          ThiefHere = 0;
          break;
        }
      }
    }

    break; // break out of for(;;)
  }

  if (room != ROOM_TREASURE_ROOM)
    ThiefDropJunk(room);
}

//*****************************************************************************



//*****************************************************************************

void PrintWeaponName(int weapon)
{
  switch (weapon)
  {
    case OBJ_STILETTO:    PrintCompText("\xc5\x69\xcf\x74\x74\x6f");    break;
    case OBJ_AXE:         PrintCompText("\x62\xd9\x6f\x64\xc4\x61\x78\x65");  break;
    case OBJ_SWORD:       PrintCompText("\x73\x77\x6f\x72\x64");       break;
    case OBJ_KNIFE:       PrintCompText("\x6e\xe0\x74\xc4\x6b\x6e\x69\x66\x65"); break;
    case OBJ_RUSTY_KNIFE: PrintCompText("\x72\xfe\x74\xc4\x6b\x6e\x69\x66\x65"); break;
  }
}



const short BlowMsgOffset[10 * 4] =
{
  0, 6, 11, 14, 18, 22, 27, 29, 30, 31,
  0, 4,  5,  8, 12, 15, 19, 22, 24, 25,
  0, 4,  6,  9, 13, 17, 20, 23, 26, 28,
  0, 2,  3,  4,  6,  8, 10, 12, 13, 14
};



// i:       0 - NUM_VILLAINS-1
// blow:    1 - 9
// weapon:  OBJ_*

void PrintBlowRemark(int player_flag, int i, int blow, int weapon)
{
  int j, index, num, msg;

  j = player_flag ? 0 : 1+i;
  index = 10*j + (blow-1);
  num = BlowMsgOffset[index+1] - BlowMsgOffset[index];
  msg = 100*j + BlowMsgOffset[index] + GetRandom(num);

  switch (msg)
  {
    case 100*0 +  0: PrintCompText("\xdc\x75\x72\x20"); PrintWeaponName(weapon); PrintCompText("\xee\xb2\xd6\xa1\x81"); PrintText(VillainName[i]); PrintCompText("\xb0\xc4\xad\xa8\x6e\x63\x68\x2e"); break;
    case 100*0 +  1: PrintCompText("\x41\xe6\xe9\xab\x73\xfd\x73\x68\xb5\x62\xf7\xa8\xa6\x6d\xb2\xd6\xa1\x81"); PrintText(VillainName[i]); PrintCompText("\xb0\xc4\xd0\x6d\x69\x6c\x65\x2e"); break;
    case 100*0 +  2: PrintCompText("\x8b\xfa\xbb\x67\x65\xb5\x62\xf7\x80\x20"); PrintText(VillainName[i]); PrintCompText("\x20\x6a\x75\x6d\x70\xa1\x6e\x69\x6d\x62\xec\xa3\x73\x69\x64\x65\x2e"); break;
    case 100*0 +  3: PrintCompText("\x43\xfd\xb1\x21\x20\x43\xf4\x73\x68\x21\x82\x20"); PrintText(VillainName[i]); PrintCompText("\xeb\xbb\xf1\x65\x73\x2e"); break;
    case 100*0 +  4: PrintCompText("\x41\x20\x71\x75\x69\x63\x6b\xaa\x74\xc2\x6b\x65\xb5\x62\xf7\x80\x20"); PrintText(VillainName[i]); PrintCompText("\x87\xca\xe6\x75\xbb\x64\x2e"); break;
    case 100*0 +  5: PrintCompText("\x41\xe6\xe9\xab\xc5\xc2\x6b\x65\xb5\x62\xf7\xa8\x74\x27\xa1\xbd\xba\x73\xd9\x77\x3b\x80\x20"); PrintText(VillainName[i]); PrintCompText("\xcc\x6f\x64\x67\x65\x73\x2e"); break;
    case 100*0 +  6: PrintCompText("\xdc\x75\x72\x20"); PrintWeaponName(weapon); PrintCompText("\xb3\xf4\x73\xa0\xa1\x64\xf2\x6e\xb5\x6b\xe3\x63\x6b\x84\x81"); PrintText(VillainName[i]); PrintCompText("\xa8\xe5\xba\x64\xa9\x61\x6d\xfd\x6e\x64\x2e"); break;
    case 100*0 +  7: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x87\x62\xaf\xd1\xa9\xab\xa7\xbd\x20\xf6\x63\xca\x73\x63\x69\xa5\x73\xed\x73\x73\x2e"); break;
    case 100*0 +  8: PrintCompText("\x41\xc6\xd8\x69\xa5\xa1\x65\x78\xfa\xad\x67\x65\xb5\x8c\x81"); PrintText(VillainName[i]); PrintCompText("\x87\x6b\xe3\x63\x6b\xd5\xae\x75\x74\x21"); break;
    case 100*0 +  9: PrintCompText("\x85\xcd\x66\xa6\xdd\x86\x72\x20"); PrintWeaponName(weapon); PrintCompText("\x20\x6b\xe3\x63\x6b\xa1\xa5\xa6\x81"); PrintText(VillainName[i]); PrintCompText("\x2e"); break;
    case 100*0 + 10: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x87\x6b\xe3\x63\x6b\xd5\xae\x75\x74\x21"); break;
    case 100*0 + 11: PrintCompText("\x49\x74\x27\xa1\x63\xd8\x74\x61\xa7\xa1\x66\xd3\x80\x20"); PrintText(VillainName[i]); PrintCompText("\xa3\xa1\x92\x20"); PrintWeaponName(weapon); PrintCompText("\xda\x65\x6d\x6f\xd7\xa1\xce\xa1\xa0\x61\x64\x2e"); break;
    case 100*0 + 12: PrintCompText("\x85\x66\xaf\xe2\xb0\xd9\x77\xaa\x74\xf1\x6b\xbe\x80\x20"); PrintText(VillainName[i]); PrintCompText("\xaa\x71\x75\xbb\x9e\xa7\x80\xc0\xbf\x72\x74\x3a\x20\x48\x9e\x64\x69\x65\x73\x2e"); break;
    case 100*0 + 13: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x9f\x61\x6b\xbe\xa3\xc6\xaf\xe2\xb0\xd9\x77\x8d\xaa\x6c\x75\x6d\x70\xa1\xbd\x80\xc6\xd9\xd3\xcc\xbf\x64\x2e"); break;
    case 100*0 + 14: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x87\xc5\x72\x75\x63\x6b\xae\xb4\x81\xbb\x6d\x3b\xb0\xd9\x6f\xab\xef\x67\xa7\xa1\xbd\x9f\xf1\x63\x6b\xcf\xcc\xf2\x6e\x2e"); break;
    case 100*0 + 15: PrintCompText("\xdc\x75\x72\x20"); PrintWeaponName(weapon); PrintCompText("\xeb\xa7\x6b\xa1\x81"); PrintText(VillainName[i]); PrintCompText("\xae\xb4\x81\x77\xf1\xc5\xb5\x62\xf7\xa8\x74\x27\xa1\xe3\xa6\xd6\xf1\xa5\x73\x2e"); break;
    case 100*0 + 16: PrintCompText("\xdc\xd8\xaa\x74\xc2\x6b\x9e\xfd\xb9\x73\xb5\x62\xf7\xa8\xa6\x77\xe0\xae\x6e\xec\x80\xc6\xfd\xa6\xdd\x80\xb0\xfd\x64\x65\x2e"); break;
    case 100*0 + 17: PrintCompText("\x85\x62\xd9\x77\xcb\xad\x64\x73\xb5\x6d\x61\x6b\x84\xd0\x73\xcd\xdf\xf2\xe6\xe0\xde\xa7\x80\x20"); PrintText(VillainName[i]); PrintCompText("\x27\xa1\xbb\x6d\x21"); break;
    case 100*0 + 18: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\xda\x65\x63\x65\x69\xd7\xa1\xd0\xe8\x65\x70\xe6\xe0\xde\xa7\xc0\x9a\x73\x69\x64\x65\x2e"); break;
    case 100*0 + 19: PrintCompText("\x41\xaa\x61\x76\x61\x67\x9e\x62\xd9\x77\xae\xb4\x81\xa2\x69\x67\x68\x21\x82\x20"); PrintText(VillainName[i]); PrintCompText("\x87\xc5\xf6\xed\xab\x62\xf7\x91\xaa\xf0\xdf\xc6\x69\x67\x68\x74\x21"); break;
    case 100*0 + 20: PrintCompText("\x53\xfd\x73\x68\x21\x88\xb6\x62\xd9\x77\xcb\xad\x64\x73\x21\x98\xaf\xae\xed\xc0\xc7\xa3\xb4\xbb\xd1\x72\x79\xb5\xc7\xb3\xa5\x6c\xab\xef\xaa\xac\x69\xa5\x73\x21"); break;
    case 100*0 + 21: PrintCompText("\x53\xfd\x73\x68\x21\x88\xb6\xc5\xc2\x6b\x9e\x63\xca\xed\x63\x74\x73\x21\x98\x9a\x63\xa5\x6c\xab\xef\xaa\xac\x69\xa5\x73\x21"); break;
    case 100*0 + 22: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x87\xc5\x61\x67\x67\xac\xd5\xb5\x8c\x64\xc2\x70\xa1\xbd\xc0\x9a\x6b\xed\x65\x73\x2e"); break;
    case 100*0 + 23: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x87\x6d\xe1\xd4\x74\xbb\x69\xec\xcc\xb2\xd3\x69\xd4\xd1\xab\x8c\xe7\x93\x66\x69\x67\x68\xa6\x62\x61\x63\x6b\x2e"); break;
    case 100*0 + 24: PrintCompText("\x85\x66\xd3\x63\x9e\xdd\x86\xb6\x62\xd9\x77\x20\x6b\xe3\x63\x6b\xa1\x81"); PrintText(VillainName[i]); PrintCompText("\xb0\x61\x63\x6b\xb5\xc5\xf6\xed\x64\x2e"); break;
    case 100*0 + 25: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x87\x63\xca\x66\xfe\xd5\x8d\x91\x27\xa6\x66\x69\x67\x68\xa6\x62\x61\x63\x6b\x2e"); break;
    case 100*0 + 26: PrintCompText("\x85\x71\x75\x69\x63\x6b\xed\x73\xa1\xdd\x86\xb6\xa2\x72\xfe\xa6\x6b\xe3\x63\x6b\xa1\x81"); PrintText(VillainName[i]); PrintCompText("\xb0\x61\x63\x6b\xb5\xc5\xf6\xed\x64\x2e"); break;
    case 100*0 + 27: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x27\xa1\x77\xbf\x70\xca\x87\x6b\xe3\x63\x6b\xd5\x89\x81\x66\xd9\xd3\xb5\xcf\x61\x76\x84\xce\xf9\xf6\xbb\x6d\x65\x64\x2e"); break;
    case 100*0 + 28: PrintCompText("\x85"); PrintText(VillainName[i]); PrintCompText("\x87\x64\xb2\xbb\x6d\xd5\xb0\xc4\xd0\x73\x75\x62\x74\xcf\xc6\x65\xa7\xa6\x70\xe0\xa6\xce\xa1\x67\x75\xbb\x64\x2e"); break;
    case 100*0 + 29: PrintCompText("\x55\x6e\xfe\x65\x64"); break;
    case 100*0 + 30: PrintCompText("\x55\x6e\xfe\x65\x64"); break;

    case 100*1 +  0: PrintCompText("\x85\x74\xc2\xdf\xaa\xf8\xb1\xa1\xce\xa1\x61\x78\x65\xb5\x62\xf7\xa8\xa6\x6d\xb2\xd6\x73\x2e"); break;
    case 100*1 +  1: PrintCompText("\x85\x74\xc2\xdf\x27\xa1\x61\x78\x9e\x62\xbb\x65\xec\xee\xb2\xd6\xa1\x92\xfb\x61\x72\x2e"); break;
    case 100*1 +  2: PrintCompText("\x85\x61\x78\x9e\x73\x77\xf3\x70\xa1\x70\xe0\xa6\xe0\x86\x20\x6a\x75\x6d\x70\xa3\x73\x69\x64\x65\x2e"); break;
    case 100*1 +  3: PrintCompText("\x85\x61\x78\x9e\x63\xf4\x73\xa0\xa1\x61\x67\x61\xa7\xc5\x80\xda\x6f\x63\x6b\xb5\xa2\xc2\xf8\x9c\x73\x70\xbb\x6b\x73\x21"); break;
    case 100*1 +  4: PrintCompText("\x85\x66\xfd\xa6\xdd\x80\x9f\xc2\xdf\x27\xa1\x61\x78\x9e\xce\x74\xa1\x8f\xe8\xf5\xe7\xd1\xec\xae\xb4\x81\xa0\x61\x64\xb5\x6b\xe3\x63\x6b\x84\x8f\xa5\x74\x2e"); break;
    case 100*1 +  5: PrintCompText("\x85\x74\xc2\xdf\xe4\xbf\x74\xec\xda\x65\x6d\x6f\xd7\xa1\x92\xc0\xbf\x64\x2e"); break;
    case 100*1 +  6: PrintCompText("\x85\x74\xc2\xdf\x27\xa1\x61\x78\x9e\xc5\xc2\x6b\x9e\x63\xcf\x61\xd7\xa1\x8f\x66\xc2\xf9\x81\x6e\x61\xd7\x89\x81\xfa\x6f\x70\x73\x2e"); break;
    case 100*1 +  7: PrintCompText("\x85\x74\xc2\xdf\x27\xa1\x61\x78\x9e\xa9\x6d\x6f\xd7\xa1\x92\xc0\xbf\x64\x2e"); break;
    case 100*1 +  8: PrintCompText("\x85\x61\x78\x9e\x67\x65\x74\xa1\x8f\xf1\x67\x68\xa6\xa7\x80\xaa\x69\xe8\xa4\x4f\x75\x63\x68\x21"); break;
    case 100*1 +  9: PrintCompText("\x85\x66\xfd\xa6\xdd\x80\x9f\xc2\xdf\x27\xa1\x61\x78\x9e\x73\x6b\xa7\xa1\x61\x63\xc2\x73\xa1\x92\xc6\xd3\xbf\x72\x6d\x2e"); break;
    case 100*1 + 10: PrintCompText("\x85\x74\xc2\xdf\x27\xa1\x73\xf8\x9c\xe2\x6d\x6f\xc5\x20\x6b\xe3\x63\x6b\xa1\x8f\x6f\xd7\xb6\xe0\x86\xb0\xbb\x65\xec\xeb\xbb\x72\xc4\xa7\x9f\x69\x6d\x65\x2e"); break;
    case 100*1 + 11: PrintCompText("\x85\x74\xc2\xdf\xaa\xf8\xb1\xa1\xce\xa1\x61\x78\x65\xb5\x8c\xc7\xe4\x69\x63\x6b\xa1\x92\xa3\x72\xf9\xe0\x86\xcc\x6f\x64\x67\x65\x2e"); break;
    case 100*1 + 12: PrintCompText("\x85\x74\xc2\xdf\xb3\xcd\x72\x67\xbe\xb5\x8c\xce\xa1\x61\x78\x9e\x73\xfd\x73\xa0\xa1\x8f\xca\x86\x72\x20"); PrintWeaponName(weapon); PrintCompText("\xa3\x72\x6d\x2e"); break;
    case 100*1 + 13: PrintCompText("\x41\xb4\x61\x78\x9e\xc5\xc2\x6b\x9e\x6d\x61\x6b\xbe\xa3\xcc\xf3\x70\xb7\xa5\xb9\xa8\xb4\x92\xcb\x65\x67\x2e"); break;
    case 100*1 + 14: PrintCompText("\x85\x74\xc2\xdf\x27\xa1\x61\x78\x9e\x73\xf8\xb1\xa1\x64\xf2\x6e\xb5\x67\xe0\xce\x9c\x92\xaa\x68\xa5\x6c\xe8\x72\x2e"); break;
    case 100*1 + 15: PrintCompText("\x85\x74\xc2\xdf\xc0\xc7\xa1\x8f\xf8\xa2\xa3\xe6\xfd\x6e\x63\x84\x62\xd9\x77\xb5\x8c\x8f\xbb\x9e\x6d\xe1\xd4\x74\xbb\x69\xec\xaa\x74\xf6\xed\x64\x2e"); break;
    case 100*1 + 16: PrintCompText("\x85\x74\xc2\xdf\xaa\xf8\xb1\x73\x3b\x80\xb0\xfd\xe8\x9f\xd8\x6e\xa1\xca\x86\xb6\xbb\x6d\xd3\xb0\xf7\xb3\xf4\x73\xa0\xa1\x62\xc2\x61\x64\x73\x69\xe8\xa8\xe5\xba\x92\xc0\xbf\x64\x2e"); break;
    case 100*1 + 17: PrintCompText("\x8b\xc5\x61\x67\x67\xac\xb0\x61\x63\x6b\x20\xf6\xe8\xb6\xd0\xcd\x69\xea\xdd\xa3\x78\x9e\xc5\xc2\x6b\x65\x73\x2e"); break;
    case 100*1 + 18: PrintCompText("\x85\x74\xc2\xdf\x27\xa1\x6d\x69\x67\x68\x74\xc4\x62\xd9\x77\xcc\xc2\x70\xa1\x8f\xbd\x86\xb6\x6b\xed\x65\x73\x2e"); break;
    case 100*1 + 19: PrintCompText("\x85\x61\x78\x9e\xce\x74\xa1\x92\x20"); PrintWeaponName(weapon); PrintCompText("\x8d\x20\x6b\xe3\x63\x6b\xa1\xc7\xaa\x70\xa7\x6e\x97\x2e"); break;
    case 100*1 + 20: PrintCompText("\x85\x74\xc2\xdf\xaa\xf8\xb1\x73\xb5\x8f\x70\xbb\x72\x79\xb5\x62\xf7\x80\xc6\xd3\x63\x9e\xdd\xc0\x9a\x62\xd9\x77\x20\x6b\xe3\x63\x6b\xa1\x92\x20"); PrintWeaponName(weapon); PrintCompText("\xa3\x77\x61\x79\x2e"); break;
    case 100*1 + 21: PrintCompText("\x85\x61\x78\x9e\x6b\xe3\x63\x6b\xa1\x92\x20"); PrintWeaponName(weapon); PrintCompText("\xae\xf7\x8a\x92\xc0\xad\x64\xa4\x49\xa6\x66\xe2\x6c\xa1\xbd\x80\xc6\xd9\x6f\x72\x2e"); break;
    case 100*1 + 22: PrintCompText("\x85\x74\xc2\xdf\xc0\xbe\xc7\xaf\xbe\xb5\x66\x97\xac\x84\xce\xa1\x61\x78\x65\x2e"); break;
    case 100*1 + 23: PrintCompText("\x85\x74\xc2\xdf\xaa\x63\xf4\x74\xfa\xbe\xc0\x9a\xa0\x61\xab\x72\x75\x6d\xa7\xaf\x69\xd7\xec\x3a\x20\x20\x4d\x69\x67\x68\xa6\x8f\xef\xee\x61\x67\x69\xe7\xdf\xc4\x70\xc2\xd1\x63\xd1\x64\xb5\x94\x77\xca\xe8\x72\x73\x3f"); break;
    case 100*1 + 24: PrintCompText("\x43\xca\x71\x75\xac\x84\xce\xa1\x66\xbf\x72\x73\xb5\x81\x74\xc2\xdf\xeb\xf7\xa1\x8f\xbd\xcc\xbf\x74\x68\x2e"); break;

    case 100*2 +  0: PrintCompText("\x85\xa2\x69\x65\xd2\xc5\x61\x62\xa1\xe3\x6e\xfa\xe2\xad\x74\xec\xb7\xc7\xde\xce\xa1\xc5\x69\xcf\x74\xbd\x8d\xee\xb2\xd6\x73\x2e"); break;
    case 100*2 +  1: PrintCompText("\x8b\x64\x6f\x64\x67\x9e\xe0\x80\x95\x69\x65\xd2\x63\xe1\xbe\xa8\xb4\xd9\x77\x2e"); break;
    case 100*2 +  2: PrintCompText("\x8b\x70\xbb\x72\xc4\xd0\xf5\x67\x68\x74\x6e\x84\xa2\x72\xfe\x74\xb5\x8c\x81\xa2\x69\x65\xd2\x73\xe2\xf7\xbe\x86\xb7\xc7\xde\xd0\x67\xf1\xf9\xe3\x64\x2e"); break;
    case 100*2 +  3: PrintCompText("\x85\xa2\x69\x65\xd2\x74\xf1\xbe\x89\x73\xed\x61\x6b\xeb\xe0\xa6\x92\xe6\x75\xbb\x64\xb5\x62\xf7\x86\x9f\xf8\xc5\xa3\x77\x61\x79\x2e"); break;
    case 100*2 +  4: PrintCompText("\x53\xce\x66\xf0\x9c\xa7\x80\xee\x69\x64\xc5\x8a\xd0\xa2\x72\xfe\x74\xb5\x81\xa2\x69\x65\xd2\x6b\xe3\x63\x6b\xa1\x8f\xf6\x63\xca\x73\x63\x69\xa5\xa1\xf8\xa2\x80\xc0\x61\x66\xa6\xdd\xc0\x9a\xc5\x69\xcf\x74\x74\x6f\x2e"); break;
    case 100*2 +  5: PrintCompText("\x85\xa2\x69\x65\xd2\x6b\xe3\x63\x6b\xa1\x8f\xa5\x74\x2e"); break;
    case 100*2 +  6: PrintCompText("\x46\xa7\xb2\xce\x9c\x8f\xdd\x66\xb5\x81\xa2\x69\x65\xd2\xa7\xd6\x72\x74\xa1\xce\xa1\x62\xfd\xe8\xa8\xe5\xba\x92\xc0\xbf\x72\x74\x2e"); break;
    case 100*2 +  7: PrintCompText("\x85\xa2\x69\x65\xd2\x63\xe1\xbe\xa8\xb4\x66\xc2\xf9\x81\x73\x69\xe8\xb5\x66\x65\xa7\x74\x73\xb5\x8c\xa7\xd6\x72\x74\xa1\x81\x62\xfd\xe8\xa8\xe5\xba\x92\xda\x69\x62\x73\x2e"); break;
    case 100*2 +  8: PrintCompText("\x85\xa2\x69\x65\xd2\x62\xf2\xa1\x66\xd3\x6d\xe2\xec\xb5\xf4\xb2\xbe\xc0\x9a\xc5\x69\xcf\x74\xbd\xb5\x8c\xf8\xa2\xa3\xb7\x72\xc4\x67\xf1\x6e\xb5\xd4\x64\xa1\x81\x62\xaf\x74\xcf\x8d\x86\xb6\xf5\x66\x65\x2e"); break;
    case 100*2 +  9: PrintCompText("\x41\x20\x71\x75\x69\x63\x6b\x95\x72\xfe\xa6\x70\xa7\x6b\xa1\x92\xcb\x65\x66\xa6\xbb\x6d\xb5\x8c\x62\xd9\x6f\xab\xc5\xbb\x74\xa1\xbd\x9f\xf1\x63\x6b\xcf\xcc\xf2\x6e\x2e"); break;
    case 100*2 + 10: PrintCompText("\x85\xa2\x69\x65\xd2\x64\xf4\x77\xa1\x62\xd9\x6f\x64\xb5\xf4\x6b\x84\xce\xa1\xc5\x69\xcf\x74\xbd\xa3\x63\xc2\x73\xa1\x92\xa3\x72\x6d\x2e"); break;
    case 100*2 + 11: PrintCompText("\x85\xc5\x69\xcf\x74\xbd\xc6\xfd\x73\xa0\xa1\x66\xe0\xd1\xb6\xa2\xad\x86\x91\xc6\x6f\xdf\xf2\xb5\x8c\x62\xd9\x6f\xab\x77\x65\xdf\xa1\x66\xc2\xf9\x92\xcb\x65\x67\x2e"); break;
    case 100*2 + 12: PrintCompText("\x85\xa2\x69\x65\xd2\x73\xd9\x77\xec\xa3\x70\x70\xc2\x61\xfa\xbe\xb5\xc5\xf1\x6b\xbe\xcb\x69\x6b\x9e\xd0\x73\x6e\x61\x6b\x65\xb5\x8c\xcf\x61\xd7\xa1\x8f\x77\xa5\xb9\x65\x64\x2e"); break;
    case 100*2 + 13: PrintCompText("\x85\xa2\x69\x65\xd2\xc5\xf1\x6b\xbe\xcb\x69\x6b\x9e\xd0\x73\x6e\x61\x6b\x65\x21\x82\xda\xbe\x75\x6c\xf0\x9c\x77\xa5\xb9\x87\xd6\xf1\xa5\x73\x2e"); break;
    case 100*2 + 14: PrintCompText("\x85\xa2\x69\x65\xd2\xc5\x61\x62\xa1\xd0\xe8\x65\x70\xb3\xf7\xa8\xb4\x92\x20\x75\x70\xfc\xb6\xbb\x6d\x2e"); break;
    case 100*2 + 15: PrintCompText("\x85\xc5\x69\xcf\x74\xbd\x9f\xa5\xfa\xbe\x86\xb6\x66\xd3\x65\xa0\x61\x64\xb5\x8c\x81\x62\xd9\x6f\xab\x6f\x62\x73\x63\xd8\xbe\x86\xb6\x76\xb2\x69\x6f\x6e\x2e"); break;
    case 100*2 + 16: PrintCompText("\x85\xa2\x69\x65\xd2\xc5\xf1\x6b\xbe\xa3\xa6\x92\xb7\xf1\xc5\xb5\x8c\x73\x75\x64\xe8\x6e\xec\x86\xb6\x67\xf1\x70\x87\x73\xf5\x70\xfc\x72\xc4\xf8\xa2\xb0\xd9\x6f\x64\x2e"); break;
    case 100*2 + 17: PrintCompText("\x85\x62\xf7\xa6\xdd\xc0\x9a\xc5\x69\xcf\x74\xbd\xb3\xf4\x63\x6b\xa1\x8f\xca\x80\xaa\x6b\x75\xdf\xb5\x8c\x8f\xc5\x61\x67\x67\xac\xb0\x61\x63\x6b\x2e"); break;
    case 100*2 + 18: PrintCompText("\x85\xa2\x69\x65\xd2\xf4\x6d\xa1\x81\xcd\x66\xa6\xdd\xc0\x9a\x62\xfd\xe8\xa8\xe5\xba\x92\xaa\xbd\x6d\x61\xfa\xb5\xcf\x61\x76\x84\x8f\xa5\xa6\xdd\xb0\xa9\xaf\x68\x2e"); break;
    case 100*2 + 19: PrintCompText("\x85\xa2\x69\x65\xd2\xaf\x74\x61\x63\x6b\x73\xb5\x8c\x8f\x66\xe2\xea\x62\x61\x63\x6b\xcc\xbe\xfc\xf4\xd1\x6c\x79\x2e"); break;
    case 100*2 + 20: PrintCompText("\x41\xcb\xca\x67\xb5\x96\xaf\xf1\xe7\xea\x73\xfd\x73\x68\x8e\xc3\xe7\x74\xfa\xa8\xa6\xca\x86\x72\x20"); PrintWeaponName(weapon); PrintCompText("\xb5\x62\xf7\x80\x95\x69\x65\xd2\x74\xf8\xc5\xa1\xce\xa1\x6b\x6e\x69\x66\x65\xb5\x8c\x81"); PrintWeaponName(weapon); PrintCompText("\xe6\x6f\xbe\xc6\xec\x97\x2e"); break;
    case 100*2 + 21: PrintCompText("\x85\xa2\x69\x65\xd2\xed\xaf\xec\xc6\xf5\x70\xa1\x92\x20"); PrintWeaponName(weapon); PrintCompText("\xae\xf7\x8a\x92\xc0\xad\x64\x73\xb5\x8c\xc7\xcc\xc2\x70\xa1\xbd\x80\xc6\xd9\x6f\x72\x2e"); break;
    case 100*2 + 22: PrintCompText("\x8b\x70\xbb\x72\xc4\xd0\xd9\x77\x95\x72\xfe\x74\xb5\x8c\x92\x20"); PrintWeaponName(weapon); PrintCompText("\xaa\xf5\x70\xa1\xa5\xa6\xdd\x86\xb6\xcd\x6e\x64\x2e"); break;
    case 100*2 + 23: PrintCompText("\x85\xa2\x69\x65\x66\xb5\xd0\x6d\xad\x8a\x73\x75\xfc\xf1\xd3\xb0\xa9\xd5\x97\xb5\x70\x61\xfe\xbe\xc6\xd3\xa3\xee\xe1\xd4\xa6\xbd\xb3\xca\x73\x69\xe8\xb6\x81\x70\xc2\x70\xf1\x65\x74\xc4\xdd\xc6\xa7\xb2\xce\x9c\x8f\xdd\x66\x2e"); break;
    case 100*2 + 24: PrintCompText("\x85\xa2\x69\x65\xd2\x61\x6d\xfe\xbe\xc0\x69\x6d\xd6\x6c\xd2\x62\xc4\xd6\xbb\xfa\x84\x92\xeb\x6f\x63\x6b\x65\x74\x73\x2e"); break;
    case 100*2 + 25: PrintCompText("\x85\xa2\x69\x65\xd2\xd4\xd1\x72\x74\x61\xa7\xa1\xce\x6d\xd6\x6c\xd2\x62\xc4\xf1\x66\xf5\x9c\x92\xeb\x61\x63\x6b\x2e"); break;
    case 100*2 + 26: PrintCompText("\x85\xa2\x69\x65\x66\xb5\x66\xd3\x67\x65\x74\xf0\x9c\xce\xa1\xbe\xd6\xe5\x69\xe2\xec\xe6\xd4\xd1\x65\xea\x75\x70\x62\xf1\xb1\x97\xb5\x63\xf7\xa1\x92\x95\xc2\x61\x74\x2e"); break;
    case 100*2 + 27: PrintCompText("\x85\xa2\x69\x65\x66\xb5\xd0\x70\xf4\x67\x6d\xaf\xb2\x74\xb5\x64\xb2\x70\xaf\xfa\xbe\x86\xa3\xa1\xd0\xa2\xa9\xaf\x89\xce\xa1\xf5\xd7\xf5\x68\xe9\x64\x2e"); break;

    case 100*3 +  0: PrintCompText("\x85\x43\x79\x63\xd9\x70\xa1\x6d\xb2\xd6\x73\xb5\x62\xf7\x80\xb0\x61\x63\x6b\x77\xe0\xde\xe2\x6d\x6f\xc5\x20\x6b\xe3\x63\x6b\xa1\x8f\x6f\xd7\x72\x2e"); break;
    case 100*3 +  1: PrintCompText("\x85\x43\x79\x63\xd9\x70\xa1\x72\xfe\xa0\xa1\xc9\x75\xb5\x62\xf7\xda\xf6\xa1\xa7\xbd\x80\xb7\xe2\x6c\x2e"); break;
    case 100*3 +  2: PrintCompText("\x85\x43\x79\x63\xd9\x70\xa1\xd6\xb9\xa1\x8f\x63\xf4\x73\xce\x9c\xbd\x80\xc6\xd9\xd3\xb5\xf6\x63\xca\x73\x63\x69\xa5\x73\x2e"); break;
    case 100*3 +  3: PrintCompText("\x85\x43\x79\x63\xd9\x70\xa1\x62\xa9\x61\x6b\xa1\x92\xe4\x65\x63\x6b\xb7\xc7\xde\xd0\x6d\xe0\x73\x69\xd7\xaa\x6d\xe0\x68\x2e"); break;
    case 100*3 +  4: PrintCompText("\x41\x20\x71\x75\x69\x63\x6b\xeb\xf6\xfa\xb5\x62\xf7\xa8\xa6\x77\xe0\xae\x6e\xec\xa3\xe6\xfd\x6e\x63\x84\x62\xd9\x77\x2e"); break;
    case 100*3 +  5: PrintCompText("\x41\xe6\xfd\x6e\x63\x84\x62\xd9\x77\xc6\xc2\xf9\x81\x43\x79\x63\xd9\x70\x73\x27\xc6\xb2\x74\x2e"); break;
    case 100*3 +  6: PrintCompText("\x85\x6d\xca\xc5\xac\xaa\x6d\xe0\xa0\xa1\xce\xa1\x68\x75\x67\x9e\x66\xb2\xa6\xa7\xbd\x86\xb6\xfa\xbe\x74\xb5\x62\xa9\x61\x6b\x84\xd6\xd7\xf4\xea\xf1\x62\x73\x2e"); break;
    case 100*3 +  7: PrintCompText("\x85\x43\x79\x63\xd9\x70\xa1\xe2\x6d\x6f\xc5\x20\x6b\xe3\x63\x6b\xa1\x81\xf8\xb9\xae\xf7\x8a\x8f\xf8\xa2\xa3\x20\x71\x75\x69\x63\x6b\xeb\xf6\x63\x68\x2e"); break;
    case 100*3 +  8: PrintCompText("\x85\x43\x79\x63\xd9\x70\xa1\xfd\xb9\xa1\xd0\x70\xf6\xfa\x95\xaf\x20\x6b\xe3\x63\x6b\xa1\x81\xf8\xb9\xae\xf7\x8a\xc9\x75\x2e"); break;
    case 100*3 +  9: PrintCompText("\x48\xf3\x64\xcf\x73\xa1\xdd\x86\xb6\x77\xbf\x70\xca\x73\xb5\x81\x43\x79\x63\xd9\x70\xa1\xbd\x73\xd6\xa1\x8f\x61\x67\x61\xa7\xc5\x80\xda\x6f\x63\x6b\xb7\xe2\xea\xdd\x80\xda\xe9\x6d\x2e"); break;
    case 100*3 + 10: PrintCompText("\x85\x43\x79\x63\xd9\x70\xa1\x67\xf4\x62\xa1\x92\x20"); PrintWeaponName(weapon); PrintCompText("\xb5\x74\xe0\xd1\xa1\xc7\xb5\x8c\xa2\xc2\x77\xa1\xc7\x89\x81\x67\xc2\xf6\xab\xa7\xcc\xb2\x67\xfe\x74\x2e"); break;
    case 100*3 + 11: PrintCompText("\x85\x6d\xca\xc5\xac\xe6\xf4\x62\xa1\x8f\xca\x80\xb7\xf1\xc5\xb5\x73\x71\x75\xf3\x7a\xbe\xb5\x8c\x8f\x64\xc2\x70\x86\x72\x20"); PrintWeaponName(weapon); PrintCompText("\xa8\xb4\x70\x61\x69\x6e\x2e"); break;
    case 100*3 + 12: PrintCompText("\x85\x43\x79\x63\xd9\x70\xa1\xd6\x65\x6d\xa1\xf6\x61\x62\xcf\x89\xe8\x63\x69\xe8\xb7\xa0\x96\xb6\xbd\xb0\xc2\x69\xea\xd3\xaa\xd1\x77\xc0\x9a\x64\xa7\xed\x72\x2e"); break;
    case 100*3 + 13: PrintCompText("\x85\x43\x79\x63\xd9\x70\x73\xb5\xe3\xaa\x70\xd3\x74\x73\x6d\xad\xb5\x64\xb2\x70\xaf\xfa\xbe\xc0\x9a\xf6\x63\xca\x73\x63\x69\xa5\xa1\x76\x69\x63\xf0\x6d\x2e"); break;
  }

  PrintCompText("\x0a");
}

//-----------------------------------------------------------------------------

enum
{
  BLOW_NULL,
  BLOW_MISSED,         // attacker misses
  BLOW_UNCONSCIOUS,    // defender unconscious
  BLOW_KILLED,         // defender dead
  BLOW_LIGHT_WOUND,    // defender lightly wounded
  BLOW_SERIOUS_WOUND,  // defender seriously wounded
  BLOW_STAGGER,        // defender staggered (miss turn)
  BLOW_LOSE_WEAPON,    // defender loses weapon
  BLOW_HESITATE,       // hesitates (miss on free swing)
  BLOW_SITTING_DUCK    // sitting duck (crunch!)
};



const unsigned char BlowForDefense1[13] =
{
  BLOW_MISSED, BLOW_MISSED, BLOW_MISSED, BLOW_MISSED, BLOW_STAGGER, BLOW_STAGGER,
  BLOW_UNCONSCIOUS, BLOW_UNCONSCIOUS, BLOW_KILLED, BLOW_KILLED, BLOW_KILLED,
  BLOW_KILLED, BLOW_KILLED
};

const unsigned char BlowForDefense2[22] =
{
  BLOW_MISSED, BLOW_MISSED, BLOW_MISSED, BLOW_MISSED, BLOW_MISSED, BLOW_STAGGER,
  BLOW_STAGGER, BLOW_LIGHT_WOUND, BLOW_LIGHT_WOUND, BLOW_MISSED, BLOW_MISSED, BLOW_MISSED,
  BLOW_MISSED, BLOW_STAGGER, BLOW_STAGGER, BLOW_LIGHT_WOUND, BLOW_LIGHT_WOUND,
  BLOW_LIGHT_WOUND, BLOW_UNCONSCIOUS, BLOW_KILLED, BLOW_KILLED, BLOW_KILLED
};

const unsigned char BlowForDefense3[31] =
{
  BLOW_MISSED, BLOW_MISSED, BLOW_MISSED, BLOW_MISSED, BLOW_MISSED, BLOW_STAGGER, BLOW_STAGGER,
  BLOW_LIGHT_WOUND, BLOW_LIGHT_WOUND, BLOW_SERIOUS_WOUND, BLOW_SERIOUS_WOUND, BLOW_MISSED,
  BLOW_MISSED, BLOW_MISSED, BLOW_STAGGER, BLOW_STAGGER, BLOW_LIGHT_WOUND, BLOW_LIGHT_WOUND,
  BLOW_LIGHT_WOUND, BLOW_SERIOUS_WOUND, BLOW_SERIOUS_WOUND, BLOW_SERIOUS_WOUND, BLOW_MISSED,
  BLOW_STAGGER, BLOW_STAGGER, BLOW_LIGHT_WOUND, BLOW_LIGHT_WOUND, BLOW_LIGHT_WOUND,
  BLOW_SERIOUS_WOUND, BLOW_SERIOUS_WOUND, BLOW_SERIOUS_WOUND
};



int GetBlow(int attack, int defense)
{
  int blow = 0;

  if (defense == 1)
  {
    int j = attack - 1, offset[3] = {0, 2, 4};

    if (j < 0) j = 0; else if (j > 2) j = 2;
    blow = BlowForDefense1[offset[j] + GetRandom(9)];
  }
  else if (defense == 2)
  {
    int j = attack - 1, offset[4] = {0, 9, 11, 13};

    if (j < 0) j = 0; else if (j > 3) j = 3;
    blow = BlowForDefense2[offset[j] + GetRandom(9)];
  }
  else if (defense > 2)
  {
    int j = attack - defense + 2, offset[5] = {0, 2, 11, 13, 22};

    if (j < 0) j = 0; else if (j > 4) j = 4;
    blow = BlowForDefense3[offset[j] + GetRandom(9)];
  }

  return blow;
}



// obj is player or villain obj
int FindWeapon(int obj)
{
  int i, weapon[5] = {OBJ_STILETTO, OBJ_AXE, OBJ_SWORD, OBJ_KNIFE, OBJ_RUSTY_KNIFE};

  for (i=0; i<5; i++)
    if (Obj[weapon[i]].loc == INSIDE + obj) return weapon[i];

  return 0;
}



int PlayerResult(int defense, int blow, int original_defense)
{
  PlayerStrength = (defense == 0) ? -10000 : (defense - original_defense);

  if (defense - original_defense < 0)
    EnableCureRoutine = CURE_WAIT;

  if (PlayerFightStrength(1) <= 0)
  {
    PlayerStrength = 1 - PlayerFightStrength(0);
    PrintCompLine("\x49\xa6\x61\x70\xfc\xbb\xa1\xa2\xaf\x95\xaf\xcb\xe0\xa6\x62\xd9\x77\xb7\xe0\x9f\xe9\xee\x75\xfa\xc6\xd3\x86\xa4\x49\x27\xf9\x61\x66\xf4\x69\xab\x8f\xbb\x9e\xe8\x61\x64\x2e");
    YoureDead(); // ##### RIP #####
    return 0;
  }
  else
    return blow;
}



int VillainBlow(int i, int youre_out)
{
  int attack, defense, original_defense, blow, defense_weapon, next_weapon;

  YouAreStaggered = 0;

  if (VillainStaggered[i])
  {
    VillainStaggered[i] = 0;
    PrintCompText("\x85");
    PrintText(VillainName[i]);
    PrintCompLine("\xaa\xd9\x77\xec\xda\x65\x67\x61\xa7\xa1\xce\xa1\x66\xf3\x74\x2e");
    return 1;
  }

  attack = VillainFightStrength(i, 0); // don't specify player weapon here

  defense = PlayerFightStrength(1);
  if (defense <= 0) return 1;

  original_defense = PlayerFightStrength(0);

  defense_weapon = FindWeapon(OBJ_YOU);

  blow = GetBlow(attack, defense);

  if (youre_out)
  {
    if (blow == BLOW_STAGGER)
      blow = BLOW_HESITATE;
    else
      blow = BLOW_SITTING_DUCK;
  }

  if (blow == BLOW_STAGGER && defense_weapon && PercentChance(25, HERO ? 10 : 50))
    blow = BLOW_LOSE_WEAPON;

  PrintBlowRemark(0, i, blow, defense_weapon); // 0: villain blow


  if (blow == BLOW_MISSED || blow == BLOW_HESITATE)
    {}
  else if (blow == BLOW_UNCONSCIOUS)
    {}
  else if (blow == BLOW_KILLED || blow == BLOW_SITTING_DUCK)
    defense = 0;
  else if (blow == BLOW_LIGHT_WOUND)
  {
    defense--; if (defense < 0) defense = 0;
    if (LoadAllowed > 50) LoadAllowed -= 10;
  }
  else if (blow == BLOW_SERIOUS_WOUND)
  {
    defense -= 2; if (defense < 0) defense = 0;
    if (LoadAllowed > 50) LoadAllowed -= 20;
  }
  else if (blow == BLOW_STAGGER)
    YouAreStaggered = 1;
  else
  {
    Obj[defense_weapon].loc = Obj[OBJ_YOU].loc;

    next_weapon = FindWeapon(OBJ_YOU);
    if (next_weapon)
    {
      PrintCompText("\x46\xd3\x74\xf6\xaf\x65\xec\xb5\x8f\xc5\x69\xdf\xc0\x61\xd7\x20\x61\x20");
      PrintWeaponName(next_weapon);
      PrintCompLine("\x2e");
    }
  }


  return PlayerResult(defense, blow, original_defense);
}

//-----------------------------------------------------------------------------

int VillainBusy(int i)
{
  if (i == VILLAIN_TROLL)
  {
    if (Obj[OBJ_AXE].loc == INSIDE + OBJ_TROLL)
    {
    }
    else if (Obj[OBJ_AXE].loc == Obj[OBJ_YOU].loc && PercentChance(75, 90))
    {
      Obj[OBJ_AXE].loc = INSIDE + OBJ_TROLL;
      Obj[OBJ_AXE].prop |= PROP_NODESC;
      Obj[OBJ_AXE].prop |= PROP_NOTTAKEABLE;
      Obj[OBJ_AXE].prop &= ~PROP_WEAPON;

      TrollDescType = 0; // default

      if (Obj[OBJ_TROLL].loc == Obj[OBJ_YOU].loc)
        PrintCompLine("\x85\x74\xc2\xdf\xb5\xad\x67\xac\xd5\x8d\xc0\x75\x6d\x69\xf5\xaf\xd5\xb5\xa9\x63\x6f\xd7\x72\xa1\xce\xa1\x77\xbf\x70\xca\xa4\x48\x9e\x61\x70\xfc\xbb\xa1\xbd\xc0\x61\xd7\xa3\xb4\x61\x78\x9e\xbd\xe6\xf1\xb9\xb7\xc7\xde\xc9\x75\x2e");
      return 1;
    }
    else if (Obj[OBJ_TROLL].loc == Obj[OBJ_YOU].loc)
    {
      TrollDescType = 2; // unarmed
      PrintCompLine("\x85\x74\xc2\xdf\xb5\x64\xb2\xbb\x6d\xd5\xb5\x63\xf2\xac\xa1\xa7\x9f\xac\xc2\x72\xb5\x70\xcf\x61\x64\x84\x66\xd3\xc0\x9a\xf5\x66\x9e\xa7\x80\xe6\xf7\x74\xd8\xe2\x9f\xca\x67\x75\x9e\xdd\x80\x9f\xc2\xdf\x73\x2e");
      return 1;
    }
  }
  else if (i == VILLAIN_THIEF)
  {
    if (Obj[OBJ_STILETTO].loc == INSIDE + OBJ_THIEF)
    {
    }
    else if (Obj[OBJ_STILETTO].loc == Obj[OBJ_THIEF].loc)
    {
      Obj[OBJ_STILETTO].loc = INSIDE + OBJ_THIEF;
      Obj[OBJ_STILETTO].prop |= PROP_NODESC;
      Obj[OBJ_STILETTO].prop |= PROP_NOTTAKEABLE;
      if (Obj[OBJ_THIEF].loc == Obj[OBJ_YOU].loc)
        PrintCompLine("\x85\xc2\x62\xef\x72\xb5\x73\xe1\x65\x77\xcd\xa6\x73\xd8\x70\xf1\xd6\xab\xaf\x95\x9a\x74\xd8\xb4\xdd\xfb\xd7\xe5\x73\xb5\x6e\x69\x6d\x62\xec\xda\x65\x74\xf1\x65\xd7\xa1\xce\xa1\xc5\x69\xcf\x74\x74\x6f\x2e");
      return 1;
    }
  }

  return 0;
}



void VillainDead(int i)
{
  if (i == VILLAIN_TROLL)
  {
    if (Obj[OBJ_AXE].loc == INSIDE + OBJ_TROLL)
    {
      Obj[OBJ_AXE].loc = Obj[OBJ_YOU].loc;
      Obj[OBJ_AXE].prop &= ~PROP_NODESC;
      Obj[OBJ_AXE].prop &= ~PROP_NOTTAKEABLE;
      Obj[OBJ_AXE].prop |= PROP_WEAPON;
    }
    TrollAllowsPassage = 1;
  }
  else if (i == VILLAIN_THIEF)
  {
    int flag;

    Obj[OBJ_STILETTO].loc = Obj[OBJ_YOU].loc;
    Obj[OBJ_STILETTO].prop &= ~PROP_NODESC;
    Obj[OBJ_STILETTO].prop &= ~PROP_NOTTAKEABLE;

    flag = ThiefDepositBooty(Obj[OBJ_YOU].loc);

    if (Obj[OBJ_YOU].loc == ROOM_TREASURE_ROOM)
    {
      int obj;

      for (obj=2; obj<NUM_OBJECTS; obj++)
        if (Obj[obj].loc == ROOM_TREASURE_ROOM &&
            obj != OBJ_CHALICE &&
            obj != OBJ_THIEF)
      {
        Obj[obj].prop &= ~PROP_NODESC;
        Obj[obj].prop &= ~PROP_NOTTAKEABLE;
      }

      Obj[OBJ_CHALICE].prop |= PROP_NODESC;
      PrintPresentObjects(ROOM_TREASURE_ROOM, "As the thief dies, the power of his magic decreases, and his treasures reappear:", 1); // 1: list, no desc
      Obj[OBJ_CHALICE].prop &= ~PROP_NODESC;

      PrintCompLine("\x85\xfa\xe2\x69\x63\x9e\x9a\xe3\x77\xaa\x61\x66\x9e\xbd\x9f\x61\x6b\x65\x2e");
    }
    else if (flag)
      PrintCompLine("\x48\x9a\x62\xe9\x74\xc4\xa9\x6d\x61\xa7\x73\x2e");
  }
}



int VillainStrikeFirst(int i)
{
  if (i == VILLAIN_TROLL)
  {
    if (PercentChance(33, -1))
    {
      VillainAttacking[i] = 1;
      return 1;
    }
  }
  else if (i == VILLAIN_THIEF)
  {
    if (ThiefHere && (Obj[OBJ_THIEF].prop & PROP_NODESC) == 0 && PercentChance(20, -1))
    {
      VillainAttacking[i] = 1;
      return 1;
    }
  }

  return 0;
}



void VillainUnconcious(int i)
{
  if (i == VILLAIN_TROLL)
  {
    VillainAttacking[i] = 0;

    if (Obj[OBJ_AXE].loc == INSIDE + OBJ_TROLL)
    {
      Obj[OBJ_AXE].loc = Obj[OBJ_YOU].loc;
      Obj[OBJ_AXE].prop &= ~PROP_NODESC;
      Obj[OBJ_AXE].prop &= ~PROP_NOTTAKEABLE;
      Obj[OBJ_AXE].prop |= PROP_WEAPON;
    }

    TrollDescType = 1; // unconcious
    TrollAllowsPassage = 1;
  }
  else if (i == VILLAIN_THIEF)
  {
    VillainAttacking[i] = 0;

    Obj[OBJ_STILETTO].loc = Obj[OBJ_YOU].loc;
    Obj[OBJ_STILETTO].prop &= ~PROP_NODESC;
    Obj[OBJ_STILETTO].prop &= ~PROP_NOTTAKEABLE;

    ThiefDescType = 1; // unconcious
  }
}



void VillainConscious(int i)
{
  if (i == VILLAIN_TROLL)
  {
    if (Obj[OBJ_TROLL].loc == Obj[OBJ_YOU].loc)
    {
      VillainAttacking[i] = 1;
      PrintCompLine("\x85\x74\xc2\xdf\xaa\xf0\x72\x73\xb5\x71\x75\x69\x63\x6b\xec\xda\xbe\x75\x6d\x84\xd0\x66\x69\x67\x68\xf0\x9c\xc5\xad\x63\x65\x2e");
    }

    if (Obj[OBJ_AXE].loc == INSIDE + OBJ_TROLL)
      TrollDescType = 0; // default
    else if (Obj[OBJ_AXE].loc == ROOM_TROLL_ROOM)
    {
      Obj[OBJ_AXE].loc = INSIDE + OBJ_TROLL;
      Obj[OBJ_AXE].prop |= PROP_NODESC;
      Obj[OBJ_AXE].prop |= PROP_NOTTAKEABLE;
      Obj[OBJ_AXE].prop &= ~PROP_WEAPON;
      TrollDescType = 0; // default
    }
    else
      TrollDescType = 3; // simple description

    TrollAllowsPassage = 0;
  }
  else if (i == VILLAIN_THIEF)
  {
    if (Obj[OBJ_THIEF].loc == Obj[OBJ_YOU].loc)
    {
      VillainAttacking[i] = 1;
      PrintCompLine("\x85\xc2\x62\xef\xb6\xa9\x76\x69\xd7\x73\xb5\x62\xf1\x65\x66\xec\xc6\x65\x69\x67\x6e\x84\x63\xca\xf0\x6e\x75\xd5\x20\xf6\x63\xca\x73\x63\x69\xa5\x73\xed\x73\x73\xb5\xad\x64\xb5\x77\xa0\xb4\x94\xd6\xbe\xc0\x9a\x6d\xe1\xd4\x74\xb5\x73\x63\xf4\x6d\x62\xcf\xa1\x61\x77\x61\xc4\x66\xc2\xf9\xc9\x75\x2e");
    }

    ThiefDescType = 0; // default
    ThiefRecoverStiletto();
  }
}



void FightRoutine(void)
{
  int i, obj, youre_attacked = 0, youre_out = 0;

  if (YouAreDead)
    return;

  for (i=0; i<NUM_VILLAINS; i++)
  {
    obj = VillainObj[i];

    if (Obj[obj].loc == Obj[OBJ_YOU].loc &&
        (Obj[obj].prop & PROP_NODESC) == 0)
    {
      if (obj == OBJ_THIEF && ThiefEngrossed)
        ThiefEngrossed = 0;
      else if (VillainStrength[i] < 0)
      {
        if (VillainWakingChance[i] != 0 &&
            PercentChance(VillainWakingChance[i], -1))
        {
          VillainWakingChance[i] = 0;
          if (VillainStrength[i] < 0)
          {
            VillainStrength[i] = -VillainStrength[i];
            VillainConscious(i);
          }
        }
        else
          VillainWakingChance[i] += 25;
      }
      else if (VillainAttacking[i] || VillainStrikeFirst(i))
        youre_attacked = 1;
    }
    else
    {
      if (VillainAttacking[i])
        VillainBusy(i);
      if (obj == OBJ_THIEF)
        ThiefEngrossed = 0;
      YouAreStaggered = 0;
      VillainStaggered[i] = 0;
      VillainAttacking[i] = 0;
      if (VillainStrength[i] < 0)
      {
        VillainStrength[i] = -VillainStrength[i];
        VillainConscious(i);
      }
    }
  }

  if (youre_attacked)
    for (;;)
  {
    for (i=0; i<NUM_VILLAINS; i++)
    {
      if (VillainAttacking[i] == 0) {}
      else if (VillainBusy(i)) {}
      else
      {
        int blow = VillainBlow(i, youre_out);

        if (blow == 0) return;
        else if (blow == BLOW_UNCONSCIOUS)
          youre_out = 1 + 1+GetRandom(3);
      }
    }

    if (youre_out) youre_out--;
    if (youre_out == 0) break;
  }
}

//*****************************************************************************



//*****************************************************************************

void CureRoutine(void)
{
  if (EnableCureRoutine == 0) return;
  EnableCureRoutine--;
  if (EnableCureRoutine != 0) return;

       if (PlayerStrength > 0) PlayerStrength = 0;
  else if (PlayerStrength < 0) PlayerStrength++;

  if (PlayerStrength < 0)
  {
    if (LoadAllowed < LOAD_MAX)
      LoadAllowed += 10;
    EnableCureRoutine = CURE_WAIT;
  }
  else
  {
    LoadAllowed = LOAD_MAX;
    EnableCureRoutine = 0;
  }
}

//*****************************************************************************



//*****************************************************************************

void VillainsRoutine(void)
{
  ThiefRoutine();
  FightRoutine();
  CureRoutine();
}

//*****************************************************************************



//*****************************************************************************

void VillainResult(int i, int defense, int blow)
{
  VillainStrength[i] = defense;

  if (defense == 0)
  {
    PrintCompText("\x41\x6c\x6d\x6f\xc5\xa3\xa1\x73\xe9\xb4\xe0\x80\x20");
    PrintText(VillainName[i]);
    PrintCompLine("\xb0\xa9\xaf\xa0\xa1\xce\xa1\xfd\xc5\xb0\xa9\xaf\x68\xb5\xd0\x63\xd9\x75\xab\xdd\xaa\xa7\xb2\xd1\xb6\x62\xfd\x63\x6b\xc6\x6f\xc1\xd4\xd7\xd9\x70\xa1\xce\x6d\xb5\x8c\x77\xa0\xb4\x81\x66\x6f\xc1\xf5\x66\x74\x73\xb5\x81\xe7\x72\xe7\x73\xa1\xcd\xa1\x64\xb2\x61\x70\xfc\xbb\x65\x64\x2e");

    VillainAttacking[i] = 0;
    Obj[VillainObj[i]].loc = 0;

    VillainDead(i);
  }
  else if (blow == BLOW_UNCONSCIOUS)
    VillainUnconcious(i);
}



// obj is thing being attacked by player

void PlayerBlow(int obj, int player_weapon)
{
  int i, attack, defense, defense_weapon, blow;

  for (i=0; i<NUM_VILLAINS; i++)
    if (VillainObj[i] == obj) break;

  if (i < NUM_VILLAINS)
    VillainAttacking[i] = 1;

  if (YouAreStaggered)
  {
    YouAreStaggered = 0;
    PrintCompLine("\x8b\xbb\x9e\xc5\x69\xdf\xda\x65\x63\x6f\xd7\xf1\x9c\x66\xc2\xf9\xa2\xaf\xcb\xe0\xa6\x62\xd9\x77\xb5\x73\xba\x92\xa3\x74\x74\x61\x63\x6b\x87\xa7\x65\x66\x66\x65\x63\xf0\x76\x65\x2e");
    return;
  }

  if (obj == OBJ_YOU)
  {
    PrintCompLine("\x57\x65\xdf\xb5\x8f\xa9\xe2\xec\xcc\x69\xab\xc7\x95\xaf\x9f\x69\x6d\x65\xa4\x49\xa1\x73\x75\x69\x63\x69\xe8\xeb\x61\xa7\xcf\x73\x73\x3f");
    YoureDead(); // ##### RIP #####
    return;
  }

  attack = PlayerFightStrength(1);
  if (attack < 1) attack = 1;

  if (i < NUM_VILLAINS)
    defense = VillainFightStrength(i, player_weapon);
  else
    defense = 0;

  if (defense == 0) // catches case of i == NUM_VILLAINS
  {
    PrintCompLine("\x41\x74\x74\x61\x63\x6b\x84\xa2\xaf\x87\x70\x6f\xa7\x74\xcf\x73\x73\x2e");
    return;
  }

  defense_weapon = FindWeapon(obj);

  if ((defense_weapon == 0 && obj != OBJ_CYCLOPS) || defense < 0)
  {
    PrintCompText("\x85");
    if (defense < 0) PrintCompText("\xf6\x63\xca\x73\x63\x69\xa5\x73\x20");
    else             PrintCompText("\xf6\xbb\x6d\x65\x64\x20");
    PrintText(VillainName[i]);
    PrintCompLine("\x91\xe3\xa6\xe8\x66\xd4\xab\xce\x6d\xd6\x6c\x66\x3a\x20\x48\x9e\x64\x69\x65\x73\x2e");
    blow = BLOW_KILLED;
  }
  else
  {
    blow = GetBlow(attack, defense);

    if (blow == BLOW_STAGGER && defense_weapon && PercentChance(25, -1))
      blow = BLOW_LOSE_WEAPON;

    PrintBlowRemark(1, i, blow, player_weapon); // 1: player blow
  }


  if (blow == BLOW_MISSED || blow == BLOW_HESITATE)
  {
  }
  else if (blow == BLOW_UNCONSCIOUS)
    defense = -defense;
  else if (blow == BLOW_KILLED || blow == BLOW_SITTING_DUCK)
    defense = 0;
  else if (blow == BLOW_LIGHT_WOUND)
  {
    defense--;
    if (defense < 0) defense = 0;
  }
  else if (blow == BLOW_SERIOUS_WOUND)
  {
    defense -= 2;
    if (defense < 0) defense = 0;
  }
  else if (blow == BLOW_STAGGER)
    VillainStaggered[i] = 1;
  else
  {
    Obj[defense_weapon].loc = Obj[OBJ_YOU].loc;
    Obj[defense_weapon].prop &= ~PROP_NODESC;
    Obj[defense_weapon].prop &= ~PROP_NOTTAKEABLE;
    Obj[defense_weapon].prop |= PROP_WEAPON;
  }


  VillainResult(i, defense, blow);
}

//*****************************************************************************



//*****************************************************************************

// call just before player enters treasure room

void ThiefProtectsTreasure(void)
{
  int obj, flag = 0;

  // if thief is dead or unconcious
  if (Obj[OBJ_THIEF].loc == 0 ||
      ThiefDescType == 1) // unconcious
    return;

  if (Obj[OBJ_THIEF].loc != ROOM_TREASURE_ROOM)
  {
    PrintCompLine("\x8b\xa0\xbb\xa3\xaa\x63\xa9\x61\xf9\xdd\xa3\xb1\x75\xb2\xde\xe0\x86\x20\x76\x69\x6f\xfd\xd1\x80\xda\x6f\x62\xef\x72\x27\xa1\xce\xe8\x61\x77\x61\x79\xa4\x55\x73\x84\x70\xe0\x73\x61\x67\xbe\x20\xf6\x6b\xe3\x77\xb4\xbd\x86\xb5\x94\x72\xfe\xa0\xa1\xbd\xa8\x74\xa1\xe8\x66\xd4\x73\x65\x2e");
  
    Obj[OBJ_THIEF].loc = ROOM_TREASURE_ROOM;
    Obj[OBJ_THIEF].prop &= ~PROP_NODESC;
  
    VillainAttacking[VILLAIN_THIEF] = 1;
  
    for (obj=2; obj<NUM_OBJECTS; obj++)
      if (Obj[obj].loc == ROOM_TREASURE_ROOM &&
          obj != OBJ_CHALICE &&
          obj != OBJ_THIEF)
    {
      if (flag == 0)
      {
        flag = 1;
        PrintCompLine("\x85\xa2\x69\x65\xd2\x67\xbe\x74\xd8\xbe\xee\x79\xc5\xac\x69\xa5\x73\xec\xb5\x8c\x81\x74\xa9\xe0\xd8\xbe\xa8\xb4\x81\xc2\xe1\xaa\x75\x64\xe8\x6e\xec\x20\x76\xad\xb2\x68\x2e");
      }
  
      Obj[obj].prop |= PROP_NODESC;
      Obj[obj].prop |= PROP_NOTTAKEABLE;
    }

    PrintCompText("\x0a");
  }
}

//*****************************************************************************
