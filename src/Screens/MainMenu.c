/****************************/
/*   	MAIN MENU.C		    */
/* (c)1999 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "3dmath.h"

extern	float				gFramesPerSecondFrac,gFramesPerSecond;
extern	TQ3Point3D			gCoord;
extern	WindowPtr			gCoverWindow;
extern	NewObjectDefinitionType	gNewObjectDefinition;
extern	QD3DSetupOutputType		*gGameViewInfoPtr;
extern	Boolean		gSongPlayingFlag,gDisableAnimSounds,gRestoringSavedGame;
extern	FSSpec		gDataSpec;
extern	PrefsType	gGamePrefs;
extern	u_short		gLevelType;
extern	TQ3Matrix4x4	gCameraWindowToWorldMatrix;

/****************************/
/*    PROTOTYPES            */
/****************************/

static void MoveMenuCamera(void);
static void SetupMainMenu(void);
static void WalkSpiderToIcon(void);


/****************************/
/*    CONSTANTS             */
/****************************/


/******************* MENU *************************/

enum
{
	MENU_MObjType_AboutIcon,
	MENU_MObjType_Background,
	MENU_MObjType_ScoresIcon,
	MENU_MObjType_PlayIcon,
	MENU_MObjType_QuitIcon,
	MENU_MObjType_RestoreIcon,
	MENU_MObjType_SettingsIcon,
	MENU_MObjType_Cyc,
	MENU_MObjType_MainMenu
};



/*********************/
/*    VARIABLES      */
/*********************/

static double		gCamDX,gCamDY,gCamDZ;
static TQ3Point3D	gCamCenter = { -40, 40, 250.0 };
ObjNode				*gMenuIcons[NUM_MENU_ICONS];
static ObjNode		*gSpider;
static u_long		gMenuSelection;

/********************** DO MAIN MENU *************************/
//
// OUTPUT: true == loop to title
//

Boolean DoMainMenu(void)
{
Point		mouse,oldMouse;
float		timer;
Boolean		loop = false;

start_again:

	timer = 0;
	PlaySong(SONG_MENU,true);

			/*********/
			/* SETUP */
			/*********/
	
	gRestoringSavedGame = false;
	SetupMainMenu();
			
				/****************/
				/* PROCESS LOOP */
				/****************/
			
	gDisableAnimSounds = true;
	
tryagain:			
	QD3D_CalcFramesPerSecond();
	QD3D_CalcFramesPerSecond();
		
	InitCursor();
	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);	
	while(true)	
	{
//		EventRecord theEvent;
	
//		WaitNextEvent(everyEvent,&theEvent, 0, 0);	//----------
	
		MoveObjects();
		MoveMenuCamera();
		QD3D_DrawScene(gGameViewInfoPtr,DrawObjects);

			/* UPDATE CURSOR */
			
		SetPort(GetWindowPort(gCoverWindow));
		oldMouse = mouse;
		GetMouse(&mouse);

		
				/* SEE IF USER CLICKED SOMETHING */
				
		if (Button())
		{
			TQ3Point2D	pt;
			
			pt.x = mouse.h;
			pt.y = mouse.v;
			
			if (PickMainMenuIcon(pt, &gMenuSelection))
				break;
		}
		
		QD3D_CalcFramesPerSecond();
		DoSDLMaintenance();
		UpdateInput();									// keys get us out
		
				/* UPDATE TIMER */
				
		if ((oldMouse.h != mouse.h) || (mouse.v != oldMouse.v))		// reset timer if mouse moved
			timer = 0;
		else
		{
			timer += gFramesPerSecondFrac;
			if (timer > 20.0f)
			{
				gMenuSelection = 1000;
				loop = true;
				goto getout;
			}
		}
	}
	
		/***********************/
		/* WALK SPIDER TO ICON */
		/***********************/
		
	WalkSpiderToIcon();
	
	
		/********************/
		/* HANDLE SELECTION */
		/********************/
	
	Q3View_Sync(gGameViewInfoPtr->viewObject);					// make sure rendering is done before we do anything

	switch(gMenuSelection)
	{
		case	0:				// ABOUT
				break;

		case	1:				// SCORES
				break;

		case	2:				// PLAY
				break;

		case	3:				// QUIT
				CleanQuit();
				break;

		case	4:				// RESTORE
				if (LoadSavedGame())					// try loading saved game
					goto tryagain;
				break;

		case	5:				// SETTINGS
				DoSettingsDialog();
				goto tryagain;
	}
	
			/***********/
			/* CLEANUP */
			/***********/

getout:
	HideCursor();
	DeleteAllObjects();
	FreeAllSkeletonFiles(-1);
	DeleteAll3DMFGroups();
	QD3D_DisposeWindowSetup(&gGameViewInfoPtr);		
	GammaFadeOut();
	GameScreenToBlack();
	gDisableAnimSounds = false;

			/* SEE WHAT TO DO */
			
	switch(gMenuSelection)
	{
		case	0:				// ABOUT
				DoAboutScreens();
				goto start_again;
				
		case	1:				// HIGH SCORES
				ShowHighScoresScreen(0);
				goto start_again;
	}
	
	GammaFadeOut();
	GameScreenToBlack();
	
	return(loop);
}


/****************** SETUP MAIN MENU **************************/

static void SetupMainMenu(void)
{
FSSpec					spec;
QD3DSetupInputType		viewDef;
TQ3Point3D				cameraTo = {0, 0, 0 };
TQ3ColorRGB				lightColor = { 1.0, 1.0, .9 };
TQ3Vector3D				fillDirection1 = { 1, -.4, -.8 };			// key
TQ3Vector3D				fillDirection2 = { -.7, -.2, -.9 };			// fill
short					i;
ObjNode					*newObj;

	gCamDX = 10; gCamDY = -5, gCamDZ = 1;
	GameScreenToBlack();

			/*************/
			/* MAKE VIEW */
			/*************/

	QD3D_NewViewDef(&viewDef, gCoverWindow);
	
	viewDef.camera.hither 			= 20;
	viewDef.camera.yon 				= 1000;
	viewDef.camera.fov 				= .9;
	viewDef.styles.usePhong 		= false;
	viewDef.camera.from.x 			= gCamCenter.x + 10;
	viewDef.camera.from.y 			= gCamCenter.y - 35;
	viewDef.camera.from.z 			= gCamCenter.z;
	viewDef.camera.to	 			= cameraTo;
	
	viewDef.lights.numFillLights 	= 2;
	viewDef.lights.ambientBrightness = 0.3;
	viewDef.lights.fillDirection[0] = fillDirection1;
	viewDef.lights.fillDirection[1] = fillDirection2;
	viewDef.lights.fillColor[0] 	= lightColor;
	viewDef.lights.fillColor[1] 	= lightColor;
	viewDef.lights.fillBrightness[0] = 1.1;
	viewDef.lights.fillBrightness[1] = .2;
	
	
	viewDef.view.clearColor.r = 
	viewDef.view.clearColor.g = 
	viewDef.view.clearColor.b = 1;
				
	QD3D_SetupWindow(&viewDef, &gGameViewInfoPtr);

			/************/
			/* LOAD ART */
			/************/
			
	LoadASkeleton(SKELETON_TYPE_SPIDER);
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:MainMenu.3dmf", &spec);
	LoadGrouped3DMF(&spec,MODEL_GROUP_MENU);	


			/***************/
			/* MAKE SPIDER */
			/***************/
		
	gNewObjectDefinition.type 		= SKELETON_TYPE_SPIDER;
	gNewObjectDefinition.animNum 	= 0;
	gNewObjectDefinition.scale 		= .2;
	gNewObjectDefinition.coord.y 	= -40;
	gNewObjectDefinition.coord.x 	= 0;
	gNewObjectDefinition.coord.z 	= 7;
	gNewObjectDefinition.slot 		= 100;
	gNewObjectDefinition.flags 		= 0;
	gNewObjectDefinition.moveCall 	= nil;
	gNewObjectDefinition.rot 		= 0;
	gSpider 						= MakeNewSkeletonObject(&gNewObjectDefinition);	
	gSpider->Rot.x = PI/2;	
	UpdateObjectTransforms(gSpider);


			/*******************/
			/* MAKE BACKGROUND */
			/*******************/
			
			/* WEB */
			
	gNewObjectDefinition.group 		= MODEL_GROUP_MENU;	
	gNewObjectDefinition.type 		= MENU_MObjType_Background;	
	gNewObjectDefinition.coord.z 	= 0;
	gNewObjectDefinition.flags 		= STATUS_BIT_NOTRICACHE; 
	gNewObjectDefinition.moveCall 	= nil;
	gNewObjectDefinition.rot 		= 0;
	gNewObjectDefinition.scale 		= .3;
	gNewObjectDefinition.slot 		= 1000;
	newObj = MakeNewDisplayGroupObject(&gNewObjectDefinition);
	MakeObjectTransparent(newObj, .4);

			/* CYC */
			
	gNewObjectDefinition.slot 		= 100;
	gNewObjectDefinition.type 		= MENU_MObjType_Cyc;	
	gNewObjectDefinition.flags 		= STATUS_BIT_NOFOG|STATUS_BIT_NULLSHADER;
	MakeNewDisplayGroupObject(&gNewObjectDefinition);
	

			/* MAIN MENU TEXT */
			
	gNewObjectDefinition.coord.x 	= 0;
	gNewObjectDefinition.coord.y 	= 100;
	gNewObjectDefinition.coord.z 	= 0;
	gNewObjectDefinition.type 		= MENU_MObjType_MainMenu;	
	gNewObjectDefinition.flags 		= 0; 
	MakeNewDisplayGroupObject(&gNewObjectDefinition);




			/**************/
			/* MAKE ICONS */
			/**************/
			
	for (i = 0; i < NUM_MENU_ICONS; i++)
	{
		TQ3StyleObject	pick;
		
		static short iconType[NUM_MENU_ICONS] = {MENU_MObjType_AboutIcon,MENU_MObjType_ScoresIcon,
									MENU_MObjType_PlayIcon,MENU_MObjType_QuitIcon,
									MENU_MObjType_RestoreIcon,MENU_MObjType_SettingsIcon};
									
		TQ3Point3D	iconCoords[NUM_MENU_ICONS] = {
									-90,-67,4,			// about
									10,50,4,			// scores
									-90,30,4,			// play
									30,-110,4,			// quit
									100,-40,4,			// restore
									80,30,4};			// settings
	
		gNewObjectDefinition.type 		= iconType[i];	
		gNewObjectDefinition.coord		= iconCoords[i];
		gNewObjectDefinition.flags 		= 0;
		gNewObjectDefinition.moveCall 	= nil;
		gNewObjectDefinition.rot 		= 0;
		gNewObjectDefinition.scale 		= .25;
		gMenuIcons[i] = MakeNewDisplayGroupObject(&gNewObjectDefinition);

					/* ADD PICK ID TO ICON */
					
		pick = Q3PickIDStyle_New(i);					
		AttachGeometryToDisplayGroupObject(gMenuIcons[i], pick);
		Q3Object_Dispose(pick);
	}
	
	MakeFadeEvent(true);
	
}


/****************** MOVE MENU CAMERA *********************/

static void MoveMenuCamera(void)
{
TQ3Point3D	p;
TQ3Vector3D	grav;
float		d,fps = gFramesPerSecondFrac;

	p = gGameViewInfoPtr->currentCameraCoords;
	
	grav.x = gCamCenter.x - p.x;						// calc vector to center
	grav.y = gCamCenter.y - p.y;
	grav.z = gCamCenter.z - p.z;	
	Q3Vector3D_Normalize(&grav,&grav);
	
	d =  Q3Point3D_Distance(&p, &gCamCenter);
	if (d != 0.0f)
	{
		if (d < 20.0f)
			d = 20.0f;
		d = 1.0f / (d);		// calc 1/distance to center
	}
	else
		d = 10;
	
	gCamDX += grav.x * fps * d * 600.0f;
	gCamDY += grav.y * fps * d * 600.0f;
	gCamDZ += grav.z * fps * d * 600.0f;

	p.x += gCamDX * fps;
	p.y += gCamDY * fps;
	p.z += gCamDZ * fps;
	
	QD3D_UpdateCameraFrom(gGameViewInfoPtr, &p);	
}


/********** WALK SPIDER TO ICON *************/

static void WalkSpiderToIcon(void)
{
Boolean	b = Button();


	MorphToSkeletonAnim(gSpider->Skeleton, 2, 6);

	while(true)	
	{		
		if (Button())			// see if user wants to hurry up
		{
			if (!b)
				break;
		}
		else
			b = false;
	
			
		/* SEE IF CLOSE ENOUGH */
		
		if (CalcDistance(gSpider->Coord.x, gSpider->Coord.y,
						 gMenuIcons[gMenuSelection]->Coord.x, gMenuIcons[gMenuSelection]->Coord.y) < 20.0f)
			break;

		
		/* MOVE SPIDER */
		
		TurnObjectTowardTargetZ(gSpider, gMenuIcons[gMenuSelection]->Coord.x,
								gMenuIcons[gMenuSelection]->Coord.y, 1.0);
		gSpider->Coord.x += -sin(gSpider->Rot.z) * gFramesPerSecondFrac * 40.0f;
		gSpider->Coord.y += cos(gSpider->Rot.z) * gFramesPerSecondFrac * 40.0f;
								
		UpdateObjectTransforms(gSpider);
		
		/* DO OTHER STUFF */
		
		MoveObjects();
		MoveMenuCamera();
		QD3D_DrawScene(gGameViewInfoPtr,DrawObjects);		
		QD3D_CalcFramesPerSecond();
		DoSDLMaintenance();
		UpdateInput();									// keys get us out
	}

	MorphToSkeletonAnim(gSpider->Skeleton, 0, 4);
}








