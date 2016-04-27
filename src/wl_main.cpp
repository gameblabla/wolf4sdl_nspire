// WL_MAIN.C
#include <unistd.h>

#include "wl_def.h"
#pragma hdrstop
#include "wl_atmos.h"
#ifdef _TINSPIRE
#include <libndls.h>
#endif


/*
=============================================================================

                             WOLFENSTEIN 3-D

                        An Id Software production

                             by John Carmack

=============================================================================
*/

extern byte signon[];

/*
=============================================================================

                             LOCAL CONSTANTS

=============================================================================
*/


#define FOCALLENGTH     (0x5700l)               // in global coordinates
#define VIEWGLOBAL      0x10000                 // globals visable flush to wall

#define VIEWWIDTH       256                     // size of view window
#define VIEWHEIGHT      144

/*
=============================================================================

                            GLOBAL VARIABLES

=============================================================================
*/

char    str[80];
int     dirangle[9] = {0,ANGLES/8,2*ANGLES/8,3*ANGLES/8,4*ANGLES/8,
                       5*ANGLES/8,6*ANGLES/8,7*ANGLES/8,ANGLES};

//
// proejection variables
//
fixed    focallength;
unsigned screenofs;
int      viewscreenx, viewscreeny;
int      viewwidth;
int      viewheight;
short    centerx;
int      shootdelta;           // pixels away from centerx a target can be
fixed    scale;
int32_t  heightnumerator;


void    Quit (const char *error,...);

boolean startgame;
boolean loadedgame;
int     mouseadjustment;

char    configdir[256] = "";
char    configname[20] = "config.";

//
// Command line parameter variables
//
boolean param_debugmode = false;
boolean param_nowait = false;
int     param_difficulty = 1;           // default is "normal"
int     param_tedlevel = -1;            // default is not to start a level
int     param_joystickindex = 0;

int     param_joystickhat = -1;
int     param_samplerate = 44100;
int     param_audiobuffer = 2048 / (44100 / param_samplerate);


int     param_mission = 0;
boolean param_goodtimes = false;
boolean param_ignorenumchunks = false;

/*
=============================================================================

                            LOCAL VARIABLES

=============================================================================
*/


/*
====================
=
= ReadConfig
=
====================
*/

void ReadConfig(void)
{
    char configpath[300];

#ifdef _arch_dreamcast
    DC_LoadFromVMU(configname);
#endif

    if(configdir[0])
    {
        snprintf(configpath, sizeof(configpath), "%s/%s", configdir, configname);
	}
    else
    {
        strcpy(configpath, configname);
        strcat(configpath, ".tns");
	}
        
    const int file = open(configpath, O_RDONLY | O_BINARY);
    if (file != -1)
    {
        //
        // valid config file
        //
        word tmp;
        read(file,&tmp,sizeof(tmp));
        if(tmp!=0xfefa)
        {
            close(file);
            goto noconfig;
        }
        read(file,Scores,sizeof(HighScore) * MaxScores);

        read(file,dirscan,sizeof(dirscan));
        read(file,buttonscan,sizeof(buttonscan));

        read(file,&viewsize,sizeof(viewsize));
        read(file,&mouseadjustment,sizeof(mouseadjustment));

        close(file);


        // make sure values are correct
        
        if(mouseadjustment<0) mouseadjustment=0;
        else if(mouseadjustment>9) mouseadjustment=9;

        if(viewsize<4) viewsize=4;
        else if(viewsize>21) viewsize=21;

        MainMenu[6].active=1;
        MainItems.curpos=0;
    }
    else
    {
        //
        // no config file, so select by hardware
        //
noconfig:
        viewsize = 21;                          // start with a good size
        mouseadjustment=5;
    }
}

/*
====================
=
= WriteConfig
=
====================
*/

void WriteConfig(void)
{
    char configpath[300];

#ifdef _arch_dreamcast
    fs_unlink(configname);
#endif

    if(configdir[0])
    {
        snprintf(configpath, sizeof(configpath), "%s/%s.tns", configdir, configname);
	}
    else
    {
        strcpy(configpath, configname);
        strcat(configpath, ".tns");
	}

    const int file = open(configpath, O_CREAT | O_WRONLY | O_BINARY, 0644);
    if (file != -1)
    {
        word tmp=0xfefa;
        write(file,&tmp,sizeof(tmp));
        write(file,Scores,sizeof(HighScore) * MaxScores);

        write(file,dirscan,sizeof(dirscan));
        write(file,buttonscan,sizeof(buttonscan));

        write(file,&viewsize,sizeof(viewsize));
        write(file,&mouseadjustment,sizeof(mouseadjustment));

        close(file);
    }
#ifdef _arch_dreamcast
    DC_SaveToVMU(configname, NULL);
#endif
}




//===========================================================================

/*
=====================
=
= NewGame
=
= Set up new game to start from the beginning
=
=====================
*/

void NewGame (int difficulty,int episode)
{
    memset (&gamestate,0,sizeof(gamestate));
    gamestate.difficulty = difficulty;
    gamestate.weapon = gamestate.bestweapon
            = gamestate.chosenweapon = wp_pistol;
    gamestate.health = 100;
    gamestate.ammo = STARTAMMO;
    gamestate.lives = 3;
    gamestate.nextextra = EXTRAPOINTS;
    gamestate.episode=episode;

    startgame = true;
}

//===========================================================================

void DiskFlopAnim(int x,int y)
{
    static int8_t which=0;
    if (!x && !y)
        return;
    VWB_DrawPic(x,y,C_DISKLOADING1PIC+which);
    VW_UpdateScreen();
    which^=1;
}


int32_t DoChecksum(byte *source,unsigned size,int32_t checksum)
{
    unsigned i;

    for (i=0;i<size-1;i++)
    checksum += source[i]^source[i+1];

    return checksum;
}


/*
==================
=
= SaveTheGame
=
==================
*/

extern statetype s_grdstand;
extern statetype s_player;

boolean SaveTheGame(FILE *file,int x,int y)
{
//    struct diskfree_t dfree;
//    int32_t avail,size,checksum;
    int checksum;
    objtype *ob;
    objtype nullobj;
    statobj_t nullstat;

/*    if (_dos_getdiskfree(0,&dfree))
        Quit("Error in _dos_getdiskfree call");

    avail = (int32_t)dfree.avail_clusters *
                  dfree.bytes_per_sector *
                  dfree.sectors_per_cluster;

    size = 0;
    for (ob = player; ob ; ob=ob->next)
        size += sizeof(*ob);
    size += sizeof(nullobj);

    size += sizeof(gamestate) +
            sizeof(LRstruct)*LRpack +
            sizeof(tilemap) +
            sizeof(actorat) +
            sizeof(laststatobj) +
            sizeof(statobjlist) +
            sizeof(doorposition) +
            sizeof(pwallstate) +
            sizeof(pwalltile) +
            sizeof(pwallx) +
            sizeof(pwally) +
            sizeof(pwalldir) +
            sizeof(pwallpos);

    if (avail < size)
    {
        Message(STR_NOSPACE1"\n"STR_NOSPACE2);
        return false;
    }*/

    checksum = 0;

    DiskFlopAnim(x,y);
    fwrite(&gamestate,sizeof(gamestate),1,file);
    checksum = DoChecksum((byte *)&gamestate,sizeof(gamestate),checksum);

    DiskFlopAnim(x,y);
    fwrite(&LevelRatios[0],sizeof(LRstruct)*LRpack,1,file);
    checksum = DoChecksum((byte *)&LevelRatios[0],sizeof(LRstruct)*LRpack,checksum);

    DiskFlopAnim(x,y);
    fwrite(tilemap,sizeof(tilemap),1,file);
    checksum = DoChecksum((byte *)tilemap,sizeof(tilemap),checksum);
    DiskFlopAnim(x,y);

    int i;
    for(i=0;i<MAPSIZE;i++)
    {
        for(int j=0;j<MAPSIZE;j++)
        {
            word actnum;
            objtype *objptr=actorat[i][j];
            if(ISPOINTER(objptr))
                actnum=0x8000 | (word)(objptr-objlist);
            else
                actnum=(word)(uintptr_t)objptr;
            fwrite(&actnum,sizeof(actnum),1,file);
            checksum = DoChecksum((byte *)&actnum,sizeof(actnum),checksum);
        }
    }

    fwrite (areaconnect,sizeof(areaconnect),1,file);
    fwrite (areabyplayer,sizeof(areabyplayer),1,file);

    // player object needs special treatment as it's in WL_AGENT.CPP and not in
    // WL_ACT2.CPP which could cause problems for the relative addressing

    ob = player;
    DiskFlopAnim(x,y);
    memcpy(&nullobj,ob,sizeof(nullobj));
    nullobj.state=(statetype *) ((uintptr_t)nullobj.state-(uintptr_t)&s_player);
    fwrite(&nullobj,sizeof(nullobj),1,file);
    ob = ob->next;

    DiskFlopAnim(x,y);
    for (; ob ; ob=ob->next)
    {
        memcpy(&nullobj,ob,sizeof(nullobj));
        nullobj.state=(statetype *) ((uintptr_t)nullobj.state-(uintptr_t)&s_grdstand);
        fwrite(&nullobj,sizeof(nullobj),1,file);
    }
    nullobj.active = ac_badobject;          // end of file marker
    DiskFlopAnim(x,y);
    fwrite(&nullobj,sizeof(nullobj),1,file);

    DiskFlopAnim(x,y);
    word laststatobjnum=(word) (laststatobj-statobjlist);
    fwrite(&laststatobjnum,sizeof(laststatobjnum),1,file);
    checksum = DoChecksum((byte *)&laststatobjnum,sizeof(laststatobjnum),checksum);

    DiskFlopAnim(x,y);
    for(i=0;i<MAXSTATS;i++)
    {
        memcpy(&nullstat,statobjlist+i,sizeof(nullstat));
        nullstat.visspot=(byte *) ((uintptr_t) nullstat.visspot-(uintptr_t)spotvis);
        fwrite(&nullstat,sizeof(nullstat),1,file);
        checksum = DoChecksum((byte *)&nullstat,sizeof(nullstat),checksum);
    }

    DiskFlopAnim(x,y);
    fwrite (doorposition,sizeof(doorposition),1,file);
    checksum = DoChecksum((byte *)doorposition,sizeof(doorposition),checksum);
    DiskFlopAnim(x,y);
    fwrite (doorobjlist,sizeof(doorobjlist),1,file);
    checksum = DoChecksum((byte *)doorobjlist,sizeof(doorobjlist),checksum);

    DiskFlopAnim(x,y);
    fwrite (&pwallstate,sizeof(pwallstate),1,file);
    checksum = DoChecksum((byte *)&pwallstate,sizeof(pwallstate),checksum);
    fwrite (&pwalltile,sizeof(pwalltile),1,file);
    checksum = DoChecksum((byte *)&pwalltile,sizeof(pwalltile),checksum);
    fwrite (&pwallx,sizeof(pwallx),1,file);
    checksum = DoChecksum((byte *)&pwallx,sizeof(pwallx),checksum);
    fwrite (&pwally,sizeof(pwally),1,file);
    checksum = DoChecksum((byte *)&pwally,sizeof(pwally),checksum);
    fwrite (&pwalldir,sizeof(pwalldir),1,file);
    checksum = DoChecksum((byte *)&pwalldir,sizeof(pwalldir),checksum);
    fwrite (&pwallpos,sizeof(pwallpos),1,file);
    checksum = DoChecksum((byte *)&pwallpos,sizeof(pwallpos),checksum);

    //
    // WRITE OUT CHECKSUM
    //
    fwrite (&checksum,sizeof(checksum),1,file);

    fwrite (&lastgamemusicoffset,sizeof(lastgamemusicoffset),1,file);

    return(true);
}

//===========================================================================

/*
==================
=
= LoadTheGame
=
==================
*/

boolean LoadTheGame(FILE *file,int x,int y)
{
    int32_t checksum,oldchecksum;
    objtype nullobj;
    statobj_t nullstat;

    checksum = 0;

    DiskFlopAnim(x,y);
    fread (&gamestate,sizeof(gamestate),1,file);
    checksum = DoChecksum((byte *)&gamestate,sizeof(gamestate),checksum);

    DiskFlopAnim(x,y);
    fread (&LevelRatios[0],sizeof(LRstruct)*LRpack,1,file);
    checksum = DoChecksum((byte *)&LevelRatios[0],sizeof(LRstruct)*LRpack,checksum);

    DiskFlopAnim(x,y);
    SetupGameLevel ();

    DiskFlopAnim(x,y);
    fread (tilemap,sizeof(tilemap),1,file);
    checksum = DoChecksum((byte *)tilemap,sizeof(tilemap),checksum);

    DiskFlopAnim(x,y);

    int actnum=0, i;
    for(i=0;i<MAPSIZE;i++)
    {
        for(int j=0;j<MAPSIZE;j++)
        {
            fread (&actnum,sizeof(word),1,file);
            checksum = DoChecksum((byte *) &actnum,sizeof(word),checksum);
            if(actnum&0x8000)
                actorat[i][j]=objlist+(actnum&0x7fff);
            else
                actorat[i][j]=(objtype *)(uintptr_t) actnum;
        }
    }

    fread (areaconnect,sizeof(areaconnect),1,file);
    fread (areabyplayer,sizeof(areabyplayer),1,file);

    InitActorList ();
    DiskFlopAnim(x,y);
    fread (player,sizeof(*player),1,file);
    player->state=(statetype *) ((uintptr_t)player->state+(uintptr_t)&s_player);

    while (1)
    {
        DiskFlopAnim(x,y);
        fread (&nullobj,sizeof(nullobj),1,file);
        if (nullobj.active == ac_badobject)
            break;
        GetNewActor ();
        nullobj.state=(statetype *) ((uintptr_t)nullobj.state+(uintptr_t)&s_grdstand);
        // don't copy over the links
        memcpy (newobj,&nullobj,sizeof(nullobj)-8);
    }

    DiskFlopAnim(x,y);
    word laststatobjnum;
    fread (&laststatobjnum,sizeof(laststatobjnum),1,file);
    laststatobj=statobjlist+laststatobjnum;
    checksum = DoChecksum((byte *)&laststatobjnum,sizeof(laststatobjnum),checksum);

    DiskFlopAnim(x,y);
    for(i=0;i<MAXSTATS;i++)
    {
        fread(&nullstat,sizeof(nullstat),1,file);
        checksum = DoChecksum((byte *)&nullstat,sizeof(nullstat),checksum);
        nullstat.visspot=(byte *) ((uintptr_t)nullstat.visspot+(uintptr_t)spotvis);
        memcpy(statobjlist+i,&nullstat,sizeof(nullstat));
    }

    DiskFlopAnim(x,y);
    fread (doorposition,sizeof(doorposition),1,file);
    checksum = DoChecksum((byte *)doorposition,sizeof(doorposition),checksum);
    DiskFlopAnim(x,y);
    fread (doorobjlist,sizeof(doorobjlist),1,file);
    checksum = DoChecksum((byte *)doorobjlist,sizeof(doorobjlist),checksum);

    DiskFlopAnim(x,y);
    fread (&pwallstate,sizeof(pwallstate),1,file);
    checksum = DoChecksum((byte *)&pwallstate,sizeof(pwallstate),checksum);
    fread (&pwalltile,sizeof(pwalltile),1,file);
    checksum = DoChecksum((byte *)&pwalltile,sizeof(pwalltile),checksum);
    fread (&pwallx,sizeof(pwallx),1,file);
    checksum = DoChecksum((byte *)&pwallx,sizeof(pwallx),checksum);
    fread (&pwally,sizeof(pwally),1,file);
    checksum = DoChecksum((byte *)&pwally,sizeof(pwally),checksum);
    fread (&pwalldir,sizeof(pwalldir),1,file);
    checksum = DoChecksum((byte *)&pwalldir,sizeof(pwalldir),checksum);
    fread (&pwallpos,sizeof(pwallpos),1,file);
    checksum = DoChecksum((byte *)&pwallpos,sizeof(pwallpos),checksum);

    if (gamestate.secretcount)      // assign valid floorcodes under moved pushwalls
    {
        word *map, *obj; word tile, sprite;
        map = mapsegs[0]; obj = mapsegs[1];
        for (y=0;y<mapheight;y++)
            for (x=0;x<mapwidth;x++)
            {
                tile = *map++; sprite = *obj++;
                if (sprite == PUSHABLETILE && !tilemap[x][y]
                    && (tile < AREATILE || tile >= (AREATILE+NUMMAPS)))
                {
                    if (*map >= AREATILE)
                        tile = *map;
                    if (*(map-1-mapwidth) >= AREATILE)
                        tile = *(map-1-mapwidth);
                    if (*(map-1+mapwidth) >= AREATILE)
                        tile = *(map-1+mapwidth);
                    if ( *(map-2) >= AREATILE)
                        tile = *(map-2);

                    *(map-1) = tile; *(obj-1) = 0;
                }
            }
    }

    Thrust(0,0);    // set player->areanumber to the floortile you're standing on

    fread (&oldchecksum,sizeof(oldchecksum),1,file);

    fread (&lastgamemusicoffset,sizeof(lastgamemusicoffset),1,file);
    if(lastgamemusicoffset<0) lastgamemusicoffset=0;


    if (oldchecksum != checksum)
    {
        Message(STR_SAVECHT1"\n"
                STR_SAVECHT2"\n"
                STR_SAVECHT3"\n"
                STR_SAVECHT4);

        IN_ClearKeysDown();
        IN_Ack();

        gamestate.oldscore = gamestate.score = 0;
        gamestate.lives = 1;
        gamestate.weapon =
            gamestate.chosenweapon =
            gamestate.bestweapon = wp_pistol;
        gamestate.ammo = 8;
    }

    return true;
}

//===========================================================================

/*
==========================
=
= ShutdownId
=
= Shuts down all ID_?? managers
=
==========================
*/

void ShutdownId (void)
{
    US_Shutdown ();         // This line is completely useless...
    PM_Shutdown ();
    IN_Shutdown ();
    VW_Shutdown ();
    CA_Shutdown ();
#if defined(GP2X_940)
    GP2X_Shutdown();
#endif
}


//===========================================================================

/*
==================
=
= BuildTables
=
= Calculates:
=
= scale                 projection constant
= sintable/costable     overlapping fractional tables
=
==================
*/

const float radtoint = (float)(FINEANGLES/2/PI);

void BuildTables (void)
{
    //
    // calculate fine tangents
    //

    int i;
    for(i=0;i<FINEANGLES/8;i++)
    {
        double tang=tan((i+0.5)/radtoint);
        finetangent[i]=(int32_t)(tang*GLOBAL1);
        finetangent[FINEANGLES/4-1-i]=(int32_t)((1/tang)*GLOBAL1);
    }

    //
    // costable overlays sintable with a quarter phase shift
    // ANGLES is assumed to be divisable by four
    //

    float angle=0;
    float anglestep=(float)(PI/2/ANGLEQUAD);
    for(i=0; i<ANGLEQUAD; i++)
    {
        fixed value=(int32_t)(GLOBAL1*sin(angle));
        sintable[i]=sintable[i+ANGLES]=sintable[ANGLES/2-i]=value;
        sintable[ANGLES-i]=sintable[ANGLES/2+i]=-value;
        angle+=anglestep;
    }
    sintable[ANGLEQUAD] = 65536;
    sintable[3*ANGLEQUAD] = -65536;

#if defined(USE_STARSKY) || defined(USE_RAIN) || defined(USE_SNOW)
    Init3DPoints();
#endif
}

//===========================================================================


/*
====================
=
= CalcProjection
=
= Uses focallength
=
====================
*/

void CalcProjection (int32_t focal)
{
    int     i;
    int    intang;
    float   angle;
    double  tang;
    int     halfview;
    double  facedist;

    focallength = focal;
    facedist = focal+MINDIST;
    halfview = viewwidth/2;                                 // half view in pixels

    //
    // calculate scale value for vertical height calculations
    // and sprite x calculations
    //
    scale = (fixed) (halfview*facedist/(VIEWGLOBAL/2));

    //
    // divide heightnumerator by a posts distance to get the posts height for
    // the heightbuffer.  The pixel height is height>>2
    //
    heightnumerator = (TILEGLOBAL*scale)>>6;

    //
    // calculate the angle offset from view angle of each pixel's ray
    //

    for (i=0;i<halfview;i++)
    {
        // start 1/2 pixel over, so viewangle bisects two middle pixels
        tang = (int32_t)i*VIEWGLOBAL/viewwidth/facedist;
        angle = (float) atan(tang);
        intang = (int) (angle*radtoint);
        pixelangle[halfview-1-i] = intang;
        pixelangle[halfview+i] = -intang;
    }
}



//===========================================================================

/*
===================
=
= SetupWalls
=
= Map tile values to scaled pics
=
===================
*/

void SetupWalls (void)
{
    int     i;

    horizwall[0]=0;
    vertwall[0]=0;

    for (i=1;i<MAXWALLTILES;i++)
    {
        horizwall[i]=(i-1)*2;
        vertwall[i]=(i-1)*2+1;
    }
}

//===========================================================================

/*
==========================
=
= SignonScreen
=
==========================
*/

void SignonScreen (void)                        // VGA version
{
    VL_SetVGAPlaneMode ();

    VL_MungePic (signon,320,200);
    VL_MemToScreen (signon,320,200,0,0);
}


/*
==========================
=
= FinishSignon
=
==========================
*/

void FinishSignon (void)
{
#ifndef SPEAR
    VW_Bar (0,189,300,11,VL_GetPixel(0,0));
    WindowX = 0;
    WindowW = 320;
    PrintY = 190;

    #ifndef JAPAN
    SETFONTCOLOR(14,4);

    #ifdef SPANISH
    US_CPrint ("Oprima una tecla");
    #else
    US_CPrint ("Press a key");
    #endif

    #endif

    VH_UpdateScreen();

    if (!param_nowait)
        IN_Ack ();

    #ifndef JAPAN
    VW_Bar (0,189,300,11,VL_GetPixel(0,0));

    PrintY = 190;
    SETFONTCOLOR(10,4);

    #ifdef SPANISH
    US_CPrint ("pensando...");
    #else
    US_CPrint ("Working...");
    #endif

    VH_UpdateScreen();
    #endif

    SETFONTCOLOR(0,15);
#else
    VH_UpdateScreen();

    if (!param_nowait)
        VW_WaitVBL(3*70);
#endif
}

//===========================================================================

/*
=====================
=
= InitDigiMap
=
=====================
*/

// channel mapping:
//  -1: any non reserved channel
//   0: player weapons
//   1: boss weapons


#ifndef SPEAR
CP_iteminfo MusicItems={CTL_X,CTL_Y,6,0,32};
CP_itemtype MusicMenu[]=
    {
        {1,"Get Them!",0},
        {1,"Searching",0},
        {1,"P.O.W.",0},
        {1,"Suspense",0},
        {1,"War March",0},
        {1,"Around The Corner!",0},

        {1,"Nazi Anthem",0},
        {1,"Lurking...",0},
        {1,"Going After Hitler",0},
        {1,"Pounding Headache",0},
        {1,"Into the Dungeons",0},
        {1,"Ultimate Conquest",0},

        {1,"Kill the S.O.B.",0},
        {1,"The Nazi Rap",0},
        {1,"Twelfth Hour",0},
        {1,"Zero Hour",0},
        {1,"Ultimate Conquest",0},
        {1,"Wolfpack",0}
    };
#else
CP_iteminfo MusicItems={CTL_X,CTL_Y-20,9,0,32};
CP_itemtype MusicMenu[]=
    {
        {1,"Funky Colonel Bill",0},
        {1,"Death To The Nazis",0},
        {1,"Tiptoeing Around",0},
        {1,"Is This THE END?",0},
        {1,"Evil Incarnate",0},
        {1,"Jazzin' Them Nazis",0},
        {1,"Puttin' It To The Enemy",0},
        {1,"The SS Gonna Get You",0},
        {1,"Towering Above",0}
    };
#endif

#ifndef SPEARDEMO
void DoJukebox(void)
{
    IN_ClearKeysDown();
    return;
}
#endif

/*
==========================
=
= InitGame
=
= Load a few things right away
=
==========================
*/

static void InitGame()
{
#ifndef SPEARDEMO
    boolean didjukebox=false;
#endif
    // initialize SDL
    SDL_Init(SDL_INIT_VIDEO);
    SignonScreen ();

    VH_Startup ();
    IN_Startup ();
    PM_Startup ();
    CA_Startup ();
    US_Startup ();

//
// build some tables
//
    ReadConfig ();
    SetupSaveGames();

//
// HOLDING DOWN 'M' KEY?
//
#ifndef SPEARDEMO
    if (Keyboard[sc_M])
    {
        DoJukebox();
        didjukebox=true;
    }
    else
#endif

//
// draw intro screen stuff
//
    IntroScreen ();

//
// load in and lock down some basic chunks
//

    CA_CacheGrChunk(STARTFONT);
    CA_CacheGrChunk(STATUSBARPIC);

    LoadLatchMem ();
    BuildTables ();          // trig tables
    SetupWalls ();

    NewViewSize (viewsize);

//
// initialize variables
//
    InitRedShifts ();
#ifndef SPEARDEMO
    if(!didjukebox)
#endif
        FinishSignon();
}

//===========================================================================

/*
==========================
=
= SetViewSize
=
==========================
*/

boolean SetViewSize (unsigned width, unsigned height)
{
    viewwidth = width&~15;                  // must be divisable by 16
    viewheight = height&~1;                 // must be even
    centerx = viewwidth/2-1;
    shootdelta = viewwidth/10;
    if((unsigned) viewheight == screenHeight)
        viewscreenx = viewscreeny = screenofs = 0;
    else
    {
        viewscreenx = (screenWidth-viewwidth) / 2;
        viewscreeny = (screenHeight-STATUSLINES-viewheight)/2;
        screenofs = viewscreeny*screenWidth+viewscreenx;
    }

//
// calculate trace angles and projection constants
//
    CalcProjection (FOCALLENGTH);

    return true;
}


void ShowViewSize (int width)
{
    int oldwidth,oldheight;

    oldwidth = viewwidth;
    oldheight = viewheight;
    
    switch(width)
    {
		case 21:
			viewwidth = screenWidth;
			viewheight = screenHeight;
			VWB_BarScaledCoord (0, 0, screenWidth, screenHeight, 0);
		break;
		case 20:
			viewwidth = screenWidth;
			viewheight = screenHeight - STATUSLINES;
			DrawPlayBorder ();
		break;
		default:
			viewwidth = width*16*screenWidth/320;
			viewheight = (int) (width*16*HEIGHTRATIO*screenHeight/200);
			DrawPlayBorder ();
		break;
	}

    viewwidth = oldwidth;
    viewheight = oldheight;
}


void NewViewSize (int width)
{
    viewsize = width;
    if(viewsize == 21)
        SetViewSize(screenWidth, screenHeight);
    else if(viewsize == 20)
        SetViewSize(screenWidth, screenHeight - STATUSLINES);
    else
        SetViewSize(width*16*screenWidth/320, (unsigned) (width*16*HEIGHTRATIO*screenHeight/200));
}



//===========================================================================

/*
==========================
=
= Quit
=
==========================
*/

void Quit (const char *errorStr, ...)
{
    char error[256];
    if(errorStr != NULL)
    {
        va_list vlist;
        va_start(vlist, errorStr);
        vsprintf(error, errorStr, vlist);
        va_end(vlist);
    }
    else error[0] = 0;

    if (!pictable)  // don't try to display the red box before it's loaded
    {
        ShutdownId();
        if (error && *error)
        {
            puts(error);
            VW_WaitVBL(100);
        }
        exit(1);
    }
    
    if (!error || !*error)
    {
        WriteConfig ();
    }

    ShutdownId ();

    if (error && *error)
    {
        puts(error);
        VW_WaitVBL(200);
        exit(1);
    }

    exit(0);
}

//===========================================================================



/*
=====================
=
= DemoLoop
=
=====================
*/


static void DemoLoop()
{
    int LastDemo = 0;

//
// check for launch from ted
//
    if (param_tedlevel != -1)
    {
        param_nowait = true;
        EnableEndGameMenuItem();
        NewGame(param_difficulty,0);

#ifndef SPEAR
        gamestate.episode = param_tedlevel/10;
        gamestate.mapon = param_tedlevel%10;
#else
        gamestate.episode = 0;
        gamestate.mapon = param_tedlevel;
#endif
        GameLoop();
        Quit (NULL);
    }


//
// main game cycle
//

#ifndef DEMOTEST

    #ifndef UPLOAD

        #ifndef GOODTIMES
        #ifndef SPEAR
        #ifndef JAPAN
        if (!param_nowait)
            NonShareware();
        #endif
        #else
            #ifndef GOODTIMES
            #ifndef SPEARDEMO
            extern void CopyProtection(void);
            if(!param_goodtimes)
                CopyProtection();
            #endif
            #endif
        #endif
        #endif
    #endif


#ifndef JAPAN
    if (!param_nowait)
        PG13 ();
#endif

#endif

    while (1)
    {
        while (!param_nowait)
        {
//
// title page
//
#ifndef DEMOTEST

#ifdef SPEAR
            SDL_Color pal[256];
            CA_CacheGrChunk (TITLEPALETTE);
            VL_ConvertPalette(grsegs[TITLEPALETTE], pal, 256);

            CA_CacheGrChunk (TITLE1PIC);
            VWB_DrawPic (0,0,TITLE1PIC);
            UNCACHEGRCHUNK (TITLE1PIC);

            CA_CacheGrChunk (TITLE2PIC);
            VWB_DrawPic (0,80,TITLE2PIC);
            UNCACHEGRCHUNK (TITLE2PIC);
            VW_UpdateScreen ();
            VL_FadeIn(0,255,pal,30);

            UNCACHEGRCHUNK (TITLEPALETTE);
#else
            CA_CacheScreen (TITLEPIC);
            VW_UpdateScreen ();
            VW_FadeIn();
#endif
            if (IN_UserInput(TickBase*15))
                break;
            VW_FadeOut();
//
// credits page
//
            CA_CacheScreen (CREDITSPIC);
            VW_UpdateScreen();
            VW_FadeIn ();
            if (IN_UserInput(TickBase*10))
                break;
            VW_FadeOut ();
//
// high scores
//
            DrawHighScores ();
            VW_UpdateScreen ();
            VW_FadeIn ();

            if (IN_UserInput(TickBase*10))
                break;
#endif
//
// demo
//

            #ifndef SPEARDEMO
            PlayDemo (LastDemo++%4);
            #else
            PlayDemo (0);
            #endif

            if (playstate == ex_abort)
                break;
            VW_FadeOut();
            if(screenHeight % 200 != 0)
                VL_ClearScreen(0);
        }

        VW_FadeOut ();

#ifdef DEBUGKEYS
        if (Keyboard[sc_Tab] && param_debugmode)
            RecordDemo ();
        else
            US_ControlPanel (0);
#else
        US_ControlPanel (0);
#endif

        if (startgame || loadedgame)
        {
            GameLoop ();
            if(!param_nowait)
            {
                VW_FadeOut();
            }
        }
    }
}


/*
==========================
=
= main
=
==========================
*/

int main (int argc, char *argv[])
{
	#ifdef _TINSPIRE
	enable_relative_paths(argv);
	#endif
    CheckForEpisodes();

    InitGame();

    DemoLoop();

    Quit("Demo loop exited???");
    return 0;
}
