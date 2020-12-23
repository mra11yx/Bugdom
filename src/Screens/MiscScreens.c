/****************************/
/*   	MISCSCREENS.C	    */
/* (c)1999 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/


extern	float				gFramesPerSecondFrac,gFramesPerSecond;
extern	WindowPtr			gCoverWindow;
extern	FSSpec		gDataSpec;
extern	u_short		gLevelType,gAreaNum,gRealLevel;
extern	Boolean		gGameOverFlag,gAbortedFlag;
extern	QD3DSetupOutputType		*gGameViewInfoPtr;
extern	ObjNode		*gPlayerObj;
extern	short		gTextureRezfile;
extern	NewObjectDefinitionType	gNewObjectDefinition;
extern	Boolean		gSongPlayingFlag,gResetSong,gDisableAnimSounds,gSongPlayingFlag;
extern	PrefsType	gGamePrefs;
extern	SDL_Window*	gSDLWindow;

/****************************/
/*    PROTOTYPES            */
/****************************/

static void MovePangeaLogoPart(ObjNode *theNode);
static void MoveLogoBG(ObjNode *theNode);



/****************************/
/*    CONSTANTS             */
/****************************/



/*********************/
/*    VARIABLES      */
/*********************/



/************************ DO PAUSED ******************************/

void DoPaused(void)
{
PicHandle	pict;
Rect		r = { 0, 0, 0, 0 };
int			i,n;
int			windowW = -1;
int			windowH = -1;
int			pictW = -1;
int			pictH = -1;
float		dummy;

	Pomme_PauseAllChannels(true);

	Q3View_Sync(gGameViewInfoPtr->viewObject);					// make sure rendering is done before we do anything

	SetPort(GetWindowPort(gCoverWindow));
	
	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);	
	InitCursor();
	
			/*******************/
			/* LET USER SELECT */
			/*******************/
			
	i = -1;
	do
	{
		SDL_GetWindowSize(gSDLWindow, &windowW, &windowH);

		Point	where;
		GetMouse(&where);
		
		if (where.v < (r.top + 94)*(windowH/480.0f))		// see if resume
			n = 0;
		else
		if (where.v < (r.top + 110)*(windowH/480.0f))		// see if end game
			n = 1;
		else												// quit app
			n = 2;
		
		if (n != i)
		{
			i = n;
			pict = GetPicture(1500+i);						// load this pict
			if (pict == nil)
			{
			    OSErr   iErr = ResError();
				DoAlert("DoPaused: GetPicture failed!  Probably ran out of memory.");
				ShowSystemErr_NonFatal(iErr);
				goto bail;
			}
			HLock((Handle)pict);
			r = (*pict)->picFrame;							// get size of pict
			pictW = r.right-r.left;
			pictH = r.bottom-r.top;
			OffsetRect(&r, (640-pictW)/2, (480-pictH)/2);
			DrawPicture(pict, &r);							// draw pict
			ReleaseResource((Handle)pict);					// nuke pict
			PlayEffect(EFFECT_SELECT);				
		}
	
	
		UpdateInput();
		if (GetNewKeyState(kKey_Pause))						// see if ESC out
		{
			n = 0;
			break;
		}

		QD3D_DrawScene(gGameViewInfoPtr, DrawTerrain);
		SubmitInfobarOverlay();
		Overlay_SubmitQuad(
				r.left, r.top, r.right-r.left, r.bottom-r.top,
				(640.0f-pictW)/2.0f/640.0f,
				(480.0f-pictH)/2.0f/480.0f,
				pictW/640.0f,
				pictH/480.0f
		);
		DoSDLMaintenance();
	
	}while(!FlushMouseButtonPress());
	while(FlushMouseButtonPress());							// wait for button up
	

			/* HANDLE RESULT */
			
	switch(n)
	{
		case	1:
				gGameOverFlag = gAbortedFlag = true;
				break;
				
		case	2:
				CleanQuit();
	
	}

			/* CLEANUP */
			
bail:			
	HideCursor();
	
	QD3D_CalcFramesPerSecond();
	QD3D_CalcFramesPerSecond();
	
	gPlayerObj->AccelVector.x =					// neutralize this
	gPlayerObj->AccelVector.y = 0;

	GetMouseDelta(&dummy,&dummy);				// read this once to prevent mouse boom

	Pomme_PauseAllChannels(false);
}



/********************* DO LEVEL CHEAT DIALOG **********************/

Boolean DoLevelCheatDialog(void)
{
	const SDL_MessageBoxButtonData buttons[] =
	{
		{ 0, 0, "1" },
		{ 0, 1, "2" },
		{ 0, 2, "3" },
		{ 0, 3, "4" },
		{ 0, 4, "5" },
		{ 0, 5, "6" },
		{ 0, 6, "7" },
		{ 0, 7, "8" },
		{ 0, 8, "9" },
		{ 0, 9, "10" },
		{ 0, 10, "Quit" },
	};

	const SDL_MessageBoxData messageboxdata =
	{
		SDL_MESSAGEBOX_INFORMATION,		// .flags
		gSDLWindow,						// .window
		"LEVEL SELECT",					// .title
		"PICK A LEVEL",					// .message
		SDL_arraysize(buttons),			// .numbuttons
		buttons,						// .buttons
		NULL							// .colorScheme
	};

	int buttonid;
	if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0)
	{
		DoAlert(SDL_GetError());
		CleanQuit();
		return false;
	}

	switch (buttonid) {
		case 10: // quit
			CleanQuit();
			return false;
		default: // resume
			gRealLevel = buttonid;
			break;
	}

	return false;

#if 0
DialogPtr 		myDialog;
short			itemHit;
Boolean			dialogDone = false;

	SetPort(GetWindowPort(gCoverWindow));
	BackColor(blackColor);
	FlushEvents ( everyEvent, REMOVE_ALL_EVENTS);
	InitCursor();
	myDialog = GetNewDialog(132,nil,MOVE_TO_FRONT);
	if (!myDialog)
		DoFatalAlert("DoLevelCheatDialog: GetNewDialog failed!");
	GammaFadeIn();
	
				/* DO IT */
				
	while(!dialogDone)
	{
		ModalDialog(nil, &itemHit);
		switch (itemHit)
		{
			case 	1:									// hit Quit
					CleanQuit();
					
			default:									// selected a level
					gRealLevel = (itemHit - 2);
					dialogDone = true;				
		}
	}
	DisposeDialog(myDialog);
	HideCursor();
	GammaFadeOut();
	GameScreenToBlack();
#endif
	return(false);
}


/*************** SLIDESHOW (source port refactor) **********************/

enum SlideshowEntryOpcode
{
	SLIDESHOW_STOP,
	SLIDESHOW_FILE
};

struct SlideshowEntry
{
	int opcode;
	const char* imagePath;
	void (*postDrawCallback)(void);
};

static void Slideshow(const struct SlideshowEntry* slides, bool doFade)
{
	FSSpec spec;

	Overlay_BeginExclusive();

	for (int i = 0; slides[i].opcode != SLIDESHOW_STOP; i++)
	{
		const struct SlideshowEntry* slide = &slides[i];

		if (slide->opcode == SLIDESHOW_FILE)
		{
			OSErr result;
			result = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, slide->imagePath, &spec);
			GAME_ASSERT_MESSAGE(result == noErr, slide->imagePath);
			DrawPictureToScreen(&spec, 0, 0);
		}
		else
		{
			DoAlert("unsupported slideshow opcode");
			continue;
		}

		if (slide->postDrawCallback)
		{
			slide->postDrawCallback();
		}

		GammaFadeIn();

		do
		{
			UpdateInput();
			DoSoundMaintenance();
			Overlay_SubmitQuad(
					0, 0, 640, 480,
					0.0f, 0.0f, 1.0f, 1.0f);
			QD3D_CalcFramesPerSecond(); // required for DoSDLMaintenance to properly cap the framerate
			DoSDLMaintenance();
		} while (!FlushMouseButtonPress() && !GetNewKeyState(kVK_Return) && !GetNewKeyState(kVK_Escape) && !GetNewKeyState(kVK_Space));
	}

	if (doFade)
	{
		GammaFadeOut();
		GameScreenToBlack();
	}

	Overlay_EndExclusive();
}


#pragma mark -

/********************** DO PANGEA LOGO **********************************/

void DoPangeaLogo(void)
{
OSErr		err;
ObjNode		*backObj;
TQ3Point3D			cameraFrom = { 0, 0, 70.0 };
QD3DSetupInputType		viewDef;
FSSpec			spec;
TQ3ColorRGB				lightColor = { 1.0, 1.0, .9 };
TQ3Vector3D				fillDirection1 = { 1, -.4, -.8 };			// key
TQ3Vector3D				fillDirection2 = { -.7, -.2, -.9 };			// fill

float			fotime = 0;
Boolean			fo = false;

	GameScreenToBlack();

			/* MAKE VIEW */

	QD3D_NewViewDef(&viewDef, gCoverWindow);
	viewDef.camera.hither 			= 10;
	viewDef.camera.yon 				= 2000;
	viewDef.camera.fov 				= .9;
	viewDef.camera.from				= cameraFrom;
	viewDef.view.clearColor.r 		= 0;
	viewDef.view.clearColor.g 		= 0;
	viewDef.view.clearColor.b		= 0;
	viewDef.styles.usePhong 		= false;

	viewDef.lights.numFillLights 	= 2;
	viewDef.lights.ambientBrightness = 0.3;
	viewDef.lights.fillDirection[0] = fillDirection1;
	viewDef.lights.fillDirection[1] = fillDirection2;
	viewDef.lights.fillColor[0] 	= lightColor;
	viewDef.lights.fillColor[1] 	= lightColor;
	viewDef.lights.fillBrightness[0] = 1.0;
	viewDef.lights.fillBrightness[1] = .2;

	viewDef.lights.fogStart 	= .5;
	viewDef.lights.fogEnd 		= 1.0;

	QD3D_SetupWindow(&viewDef, &gGameViewInfoPtr);


			/* LOAD ART */
			
	err = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:Pangea.3dmf", &spec);		// load other models
	LoadGrouped3DMF(&spec,MODEL_GROUP_TITLE);	




			/***************/
			/* MAKE MODELS */
			/***************/
				
			/* BACKGROUND */
			
	gNewObjectDefinition.group 		= MODEL_GROUP_PANGEA;	
	gNewObjectDefinition.type 		= 1;
	gNewObjectDefinition.coord.x 	= 0;
	gNewObjectDefinition.coord.y 	= 0;
#ifdef FORMAC
	gNewObjectDefinition.coord.z 	= 150;
#else
	gNewObjectDefinition.coord.z 	= -180;	
#endif	
	gNewObjectDefinition.flags 		= STATUS_BIT_NULLSHADER;
	gNewObjectDefinition.slot 		= 50;
	gNewObjectDefinition.moveCall 	= MoveLogoBG;
	gNewObjectDefinition.rot 		= 0;
	gNewObjectDefinition.scale 		= 10;
	MakeNewDisplayGroupObject(&gNewObjectDefinition);


			/* LOGO */
		
	gNewObjectDefinition.group 		= MODEL_GROUP_PANGEA;	
	gNewObjectDefinition.type 		= 0;
	gNewObjectDefinition.coord.x 	= 0;
	gNewObjectDefinition.coord.y 	= 0;
#ifdef FORMAC
	gNewObjectDefinition.coord.z 	= -20;	
#else
	gNewObjectDefinition.coord.z 	= -100;	
#endif	
	gNewObjectDefinition.flags 		= STATUS_BIT_REFLECTIONMAP;
	gNewObjectDefinition.slot 		= 50;
	gNewObjectDefinition.moveCall 	= MovePangeaLogoPart;
	gNewObjectDefinition.rot 		= PI/2;
	gNewObjectDefinition.scale 		= .18;
	backObj = MakeNewDisplayGroupObject(&gNewObjectDefinition);
	
	
	MakeFadeEvent(true);	
	
			/*************/
			/* MAIN LOOP */
			/*************/
			
	PlaySong(SONG_PANGEA,false);			
	QD3D_CalcFramesPerSecond();					

	while((!gResetSong) && gSongPlayingFlag)					// wait until song stops
	{
		QD3D_CalcFramesPerSecond();					
	
		UpdateInput();
		if (AreAnyNewKeysPressed() || FlushMouseButtonPress())
		{
			if (!fo)
				GammaFadeOut();
			break;
		}

		MoveObjects();
		
		CalcEnvironmentMappingCoords(&gGameViewInfoPtr->currentCameraCoords);
		QD3D_DrawScene(gGameViewInfoPtr,DrawObjects);	
		
		fotime += gFramesPerSecondFrac;
		if ((fotime > 7.0f) && (!fo))
		{
			MakeFadeEvent(false);
			fo = true;
		}		

		DoSDLMaintenance();
	}


			/***********/
			/* CLEANUP */
			/***********/
			
	DeleteAllObjects();
	Free3DMFGroup(MODEL_GROUP_TITLE);
	QD3D_DisposeWindowSetup(&gGameViewInfoPtr);		
	KillSong();
	GameScreenToBlack();
}


/********************* MOVE PANGEA LOGO PART ***************************/

static void MovePangeaLogoPart(ObjNode *theNode)
{
//	theNode->Coord.z += gFramesPerSecondFrac * 45;		
	theNode->Rot.y -= gFramesPerSecondFrac * .3;		
	UpdateObjectTransforms(theNode);
}


/****************** MOVE LOGO BG ******************/

static void MoveLogoBG(ObjNode *theNode)
{
	theNode->Coord.z += 15.0f * gFramesPerSecondFrac;
	UpdateObjectTransforms(theNode);
}

#pragma mark -


/*************** SHOW INTRO SCRENES **********************/
//
// Shows actual charity info
//

void ShowIntroScreens(void)
{
	const struct SlideshowEntry slides[] =
	{
		{ SLIDESHOW_FILE, ":Images:Boot.pict", NULL },
		{ SLIDESHOW_STOP, NULL, NULL },
	};
	Slideshow(slides, true);
}
