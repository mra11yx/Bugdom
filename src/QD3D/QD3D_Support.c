/****************************/
/*   	QD3D SUPPORT.C	    */
/* (c)1997-99 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include <SDL_opengl.h>
#include "game.h"


/****************************/
/*    PROTOTYPES            */
/****************************/

static void CreateLights(QD3DLightDefType *lightDefPtr);
static void DrawNormal(void);


/****************************/
/*    CONSTANTS             */
/****************************/



/*********************/
/*    VARIABLES      */
/*********************/


SDL_GLContext					gGLContext = NULL;
RenderStats						gRenderStats;

int								gWindowWidth				= GAME_VIEW_WIDTH;
int								gWindowHeight				= GAME_VIEW_HEIGHT;

float	gFramesPerSecond = DEFAULT_FPS;				// this is used to maintain a constant timing velocity as frame rates differ
float	gFramesPerSecondFrac = 1/DEFAULT_FPS;



		/* DEBUG STUFF */
		
static TQ3Point3D		gNormalWhere;
static TQ3Vector3D		gNormal;




/******************** QD3D: BOOT ******************************/
//
// NOTE: The QuickDraw3D libraries should be included in the project as a "WEAK LINK" so that I can
// 		get an error if the library can't load.  Otherwise, the Finder reports a useless error to the user.
//

void QD3D_Boot(void)
{
}



//=======================================================================================================
//=============================== VIEW WINDOW SETUP STUFF ===============================================
//=======================================================================================================


/*********************** QD3D: NEW VIEW DEF **********************/
//
// fills a view def structure with default values.
//

void QD3D_NewViewDef(QD3DSetupInputType *viewDef, WindowPtr theWindow)
{
TQ3ColorRGBA		clearColor = {0,0,0,1};
TQ3Point3D			cameraFrom = { 0, 40, 200.0 };
TQ3Point3D			cameraTo = { 0, 0, 0 };
TQ3Vector3D			cameraUp = { 0.0, 1.0, 0.0 };
TQ3ColorRGB			ambientColor = { 1.0, 1.0, .8 };
TQ3Vector3D			fillDirection1 = { 1, -1, .3 };
TQ3Vector3D			fillDirection2 = { -.8, .8, -.2 };

	Q3Vector3D_Normalize(&fillDirection1,&fillDirection1);
	Q3Vector3D_Normalize(&fillDirection2,&fillDirection2);

	if (theWindow == nil)
		viewDef->view.useWindow 	=	false;							// assume going to pixmap
	else
		viewDef->view.useWindow 	=	true;							// assume going to window
	viewDef->view.displayWindow 	= theWindow;
//	viewDef->view.rendererType 		= kQ3RendererTypeOpenGL;
	viewDef->view.clearColor 		= clearColor;
	viewDef->view.paneClip.left 	= 0;
	viewDef->view.paneClip.right 	= 0;
	viewDef->view.paneClip.top 		= 0;
	viewDef->view.paneClip.bottom 	= 0;
	viewDef->view.dontClear			= false;

	viewDef->styles.interpolation 	= kQ3InterpolationStyleVertex; 
	viewDef->styles.backfacing 		= kQ3BackfacingStyleRemove; 
	viewDef->styles.fill			= kQ3FillStyleFilled; 
	viewDef->styles.usePhong 		= false; 

	viewDef->camera.from 			= cameraFrom;
	viewDef->camera.to 				= cameraTo;
	viewDef->camera.up 				= cameraUp;
	viewDef->camera.hither 			= 10;
	viewDef->camera.yon 			= 3000;
	viewDef->camera.fov 			= .9;

	viewDef->lights.ambientBrightness = 0.1;
	viewDef->lights.ambientColor 	= ambientColor;
	viewDef->lights.numFillLights 	= 2;
	viewDef->lights.fillDirection[0] = fillDirection1;
	viewDef->lights.fillDirection[1] = fillDirection2;
	viewDef->lights.fillColor[0] 	= ambientColor;
	viewDef->lights.fillColor[1] 	= ambientColor;
	viewDef->lights.fillBrightness[0] = .9;
	viewDef->lights.fillBrightness[1] = .2;
	
	viewDef->lights.useFog 		= true;
	viewDef->lights.fogStart 	= .8;
	viewDef->lights.fogEnd 		= 1.0;
	viewDef->lights.fogDensity	= 1.0;
	viewDef->lights.fogMode		= kQ3FogModePlaneBasedLinear;
	viewDef->lights.useCustomFogColor = false;
	viewDef->lights.fogColor	= clearColor;
	
	viewDef->enableMultisamplingByDefault = true;
}

/************** SETUP QD3D WINDOW *******************/

void QD3D_SetupWindow(QD3DSetupInputType *setupDefPtr, QD3DSetupOutputType **outputHandle)
{
TQ3Vector3D	v = {0,0,0};
QD3DSetupOutputType	*outputPtr;

			/* ALLOC MEMORY FOR OUTPUT DATA */

	*outputHandle = (QD3DSetupOutputType *)AllocPtr(sizeof(QD3DSetupOutputType));
	outputPtr = *outputHandle;
	GAME_ASSERT(outputPtr);

				/* CREATE & SET DRAW CONTEXT */

	GAME_ASSERT_MESSAGE(!gGLContext, "stale GL context not destroyed before calling SetupWindow");

	gGLContext = SDL_GL_CreateContext(gSDLWindow);									// also makes it current
	GAME_ASSERT(gGLContext);

				/* PASS BACK INFO */

	outputPtr->window 				= setupDefPtr->view.displayWindow;	// remember which window
	outputPtr->paneClip 			= setupDefPtr->view.paneClip;
	outputPtr->aspectRatio			= 1.0f;								// aspect ratio is set at every frame depending on window size
	outputPtr->needScissorTest 		= setupDefPtr->view.paneClip.left != 0 || setupDefPtr->view.paneClip.right != 0
									|| setupDefPtr->view.paneClip.bottom != 0 || setupDefPtr->view.paneClip.top != 0;
	outputPtr->hither 				= setupDefPtr->camera.hither;		// remember hither/yon
	outputPtr->yon 					= setupDefPtr->camera.yon;
	outputPtr->fov					= setupDefPtr->camera.fov;
	outputPtr->enableMultisamplingByDefault = setupDefPtr->enableMultisamplingByDefault;

	outputPtr->currentCameraUpVector	= setupDefPtr->camera.up;
	outputPtr->currentCameraLookAt		= setupDefPtr->camera.to;
	outputPtr->currentCameraCoords		= setupDefPtr->camera.from;

	outputPtr->isActive = true;								// it's now an active structure
	
	outputPtr->lightList = setupDefPtr->lights;				// copy light list
	
	QD3D_MoveCameraFromTo(outputPtr,&v,&v);					// call this to set outputPtr->currentCameraCoords & camera matrix



				/* SET UP OPENGL RENDERER PROPERTIES NOW THAT WE HAVE A CONTEXT */

	SDL_GL_SetSwapInterval(gGamePrefs.vsync ? 1 : 0);

	CreateLights(&setupDefPtr->lights);

	glAlphaFunc(GL_GREATER, 0.4999f);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Normalize normal vectors. Required so lighting looks correct on scaled meshes.
	glEnable(GL_NORMALIZE);

	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);									// CCW is front face

	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	Render_InitState();
//	Render_Alloc2DCover(GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);

	if (setupDefPtr->lights.useFog)
	{
		Render_EnableFog(
				setupDefPtr->camera.hither,
				setupDefPtr->camera.yon,
				setupDefPtr->lights.fogStart,
				setupDefPtr->lights.fogEnd,
				setupDefPtr->lights.useCustomFogColor ? setupDefPtr->lights.fogColor : setupDefPtr->view.clearColor);
	}
	else
		Render_DisableFog();

	glClearColor(setupDefPtr->view.clearColor.r, setupDefPtr->view.clearColor.g, setupDefPtr->view.clearColor.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	CHECK_GL_ERROR();

}


/***************** QD3D_DisposeWindowSetup ***********************/
//
// Disposes of all data created by QD3D_SetupWindow
//

void QD3D_DisposeWindowSetup(QD3DSetupOutputType **dataHandle)
{
QD3DSetupOutputType	*data;

	data = *dataHandle;
	GAME_ASSERT(data);										// see if this setup exists

//	Render_Dispose2DCover(); // Source port addition - release backdrop GL texture

	SDL_GL_DeleteContext(gGLContext);						// dispose GL context
	gGLContext = nil;

	data->isActive = false;									// now inactive
	
		/* FREE MEMORY & NIL POINTER */
		
	DisposePtr((Ptr)data);
	*dataHandle = nil;
}


/********************* CREATE LIGHTS ************************/

static void CreateLights(QD3DLightDefType *lightDefPtr)
{
			/************************/
			/* CREATE AMBIENT LIGHT */
			/************************/

	if (lightDefPtr->ambientBrightness != 0)						// see if ambient exists
	{
		GLfloat ambient[4] =
		{
			lightDefPtr->ambientBrightness * lightDefPtr->ambientColor.r,
			lightDefPtr->ambientBrightness * lightDefPtr->ambientColor.g,
			lightDefPtr->ambientBrightness * lightDefPtr->ambientColor.b,
			1
		};
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	}

			/**********************/
			/* CREATE FILL LIGHTS */
			/**********************/

	for (int i = 0; i < lightDefPtr->numFillLights; i++)
	{
		static GLfloat lightamb[4] = { 0.0, 0.0, 0.0, 1.0 };
		GLfloat lightVec[4];
		GLfloat	diffuse[4];

					/* SET FILL DIRECTION */

		Q3Vector3D_Normalize(&lightDefPtr->fillDirection[i], &lightDefPtr->fillDirection[i]);
		lightVec[0] = -lightDefPtr->fillDirection[i].x;		// negate vector because OGL is stupid
		lightVec[1] = -lightDefPtr->fillDirection[i].y;
		lightVec[2] = -lightDefPtr->fillDirection[i].z;
		lightVec[3] = 0;									// when w==0, this is a directional light, if 1 then point light
		glLightfv(GL_LIGHT0+i, GL_POSITION, lightVec);

					/* SET COLOR */

		glLightfv(GL_LIGHT0+i, GL_AMBIENT, lightamb);

		diffuse[0] = lightDefPtr->fillColor[i].r * lightDefPtr->fillBrightness[i];
		diffuse[1] = lightDefPtr->fillColor[i].g * lightDefPtr->fillBrightness[i];
		diffuse[2] = lightDefPtr->fillColor[i].b * lightDefPtr->fillBrightness[i];
		diffuse[3] = 1;

		glLightfv(GL_LIGHT0+i, GL_DIFFUSE, diffuse);

		glEnable(GL_LIGHT0+i);								// enable the light
	}
}


/******************* QD3D DRAW SCENE *********************/

void QD3D_DrawScene(QD3DSetupOutputType *setupInfo, void (*drawRoutine)(const QD3DSetupOutputType *))
{
	GAME_ASSERT(setupInfo);
	GAME_ASSERT(setupInfo->isActive);									// make sure it's legit

			/* START RENDERING */

	int mkc = SDL_GL_MakeCurrent(gSDLWindow, gGLContext);
	GAME_ASSERT_MESSAGE(mkc == 0, SDL_GetError());

	Render_StartFrame();

			/* SET UP SCISSOR TEST */

	if (setupInfo->needScissorTest)
	{
		// Set scissor
		TQ3Area pane	= Render_GetAdjustedViewportRect(setupInfo->paneClip, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
		int paneWidth	= pane.max.x-pane.min.x;
		int paneHeight	= pane.max.y-pane.min.y;
		setupInfo->aspectRatio = paneWidth / (paneHeight + .001f);
		Render_SetViewport(true, pane.min.x, gWindowHeight-pane.max.y, paneWidth, paneHeight);
	}
	else
	{
		setupInfo->aspectRatio = gWindowWidth / (gWindowHeight + .001f);
		Render_SetViewport(false, 0, 0, gWindowWidth, gWindowHeight);
	}

			/* SET UP CAMERA */

	CalcCameraMatrixInfo(setupInfo);						// update camera matrix


			/***************/
			/* RENDER LOOP */
			/***************/

	if (drawRoutine)
		drawRoutine(setupInfo);
	
	QD3D_SetMultisampling(false);


			/******************/
			/* DONE RENDERING */
			/*****************/

	Render_EndFrame();

	SubmitInfobarOverlay();			// draw 2D elements on top

#if ALLOW_FADE
	if (gGammaFadeFactor < 1.0f)
	{
		Render_DrawFadeOverlay(gGammaFadeFactor);
	}
#endif

	SDL_GL_SwapWindow(gSDLWindow);
}


//=======================================================================================================
//=============================== CAMERA STUFF ==========================================================
//=======================================================================================================

#pragma mark -

/*************** QD3D_UpdateCameraFromTo ***************/

void QD3D_UpdateCameraFromTo(QD3DSetupOutputType *setupInfo, TQ3Point3D *from, TQ3Point3D *to)
{
	setupInfo->currentCameraCoords = *from;					// set camera coords
	setupInfo->currentCameraLookAt = *to;					// set camera look at
	UpdateListenerLocation();
}


/*************** QD3D_UpdateCameraFrom ***************/

void QD3D_UpdateCameraFrom(QD3DSetupOutputType *setupInfo, TQ3Point3D *from)
{
	setupInfo->currentCameraCoords = *from;					// set camera coords
	UpdateListenerLocation();
}


/*************** QD3D_MoveCameraFromTo ***************/

void QD3D_MoveCameraFromTo(QD3DSetupOutputType *setupInfo, TQ3Vector3D *moveVector, TQ3Vector3D *lookAtVector)
{
	setupInfo->currentCameraCoords.x += moveVector->x;		// set camera coords
	setupInfo->currentCameraCoords.y += moveVector->y;
	setupInfo->currentCameraCoords.z += moveVector->z;

	setupInfo->currentCameraLookAt.x += lookAtVector->x;	// set camera look at
	setupInfo->currentCameraLookAt.y += lookAtVector->y;
	setupInfo->currentCameraLookAt.z += lookAtVector->z;

	UpdateListenerLocation();
}


//=======================================================================================================
//=============================== TEXTURE MAP STUFF =====================================================
//=======================================================================================================

#pragma mark -

/**************** QD3D GET TEXTURE MAP ***********************/
//
// Loads a numbered TGA file inside :Data:Images:Textures as an OpenGL texture.
//
// INPUT: textureRezID = TGA file number to get.
//			flags = see RendererTextureFlags
//
// OUTPUT: OpenGL texture name.
//

GLuint QD3D_LoadTextureFile(int textureRezID, int flags)
{
char					path[128];
FSSpec					spec;
uint8_t*				pixelData = nil;
TGAHeader				header;
OSErr					err;

	snprintf(path, sizeof(path), ":Images:Textures:%d.tga", textureRezID);

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, path, &spec);

			/* LOAD RAW ARGB DATA FROM TGA FILE */

	err = ReadTGA(&spec, &pixelData, &header, true);
	GAME_ASSERT(err == noErr);

	GAME_ASSERT(header.bpp == 32);
	GAME_ASSERT(header.imageType == TGA_IMAGETYPE_CONVERTED_ARGB);

			/* PRE-PROCESS IMAGE */

	int internalFormat = GL_RGB;

	if (flags & kRendererTextureFlags_SolidBlackIsAlpha)
	{
		for (int p = 0; p < 4 * header.width * header.height; p += 4)
		{
			bool isBlack = !pixelData[p+1] && !pixelData[p+2] && !pixelData[p+3];
			pixelData[p+0] = isBlack? 0x00: 0xFF;
		}

		// Apply edge padding to avoid seams
		TQ3Pixmap pm;
		pm.image = pixelData;
		pm.width = header.width;
		pm.height = header.height;
		pm.rowBytes = header.width * (header.bpp / 8);
		pm.pixelSize = 0;
		pm.pixelType = kQ3PixelTypeARGB32;
		pm.bitOrder = kQ3EndianBig;
		pm.byteOrder = kQ3EndianBig;
		Q3Pixmap_ApplyEdgePadding(&pm);

		internalFormat = GL_RGBA;
	}
	else if (flags & kRendererTextureFlags_GrayscaleIsAlpha)
	{
		for (int p = 0; p < 4 * header.width * header.height; p += 4)
		{
			// put Blue into Alpha & leave map white
			pixelData[p+0] = pixelData[p+3];	// put blue into alpha
			pixelData[p+1] = 255;
			pixelData[p+2] = 255;
			pixelData[p+3] = 255;
		}
		internalFormat = GL_RGBA;
	}
	else if (flags & kRendererTextureFlags_KeepOriginalAlpha)
	{
		internalFormat = GL_RGBA;
	}
	else
	{
		internalFormat = GL_RGB;
	}

			/* LOAD TEXTURE */

	GLuint glTextureName = Render_LoadTexture(
			internalFormat,
			header.width,
			header.height,
			GL_BGRA,
			GL_UNSIGNED_INT_8_8_8_8,
			pixelData,
			flags
			);

			/* CLEAN UP */

	DisposePtr((Ptr) pixelData);

	return glTextureName;
}


#pragma mark -

//=======================================================================================================
//=============================== MISC ==================================================================
//=======================================================================================================

/************** QD3D CALC FRAMES PER SECOND *****************/

void	QD3D_CalcFramesPerSecond(void)
{
UnsignedWide	wide;
unsigned long	now;
static	unsigned long then = 0;


			/* DO REGULAR CALCULATION */
			
	Microseconds(&wide);
	now = wide.lo;
	if (then != 0)
	{
		gFramesPerSecond = 1000000.0f/(float)(now-then);
		if (gFramesPerSecond < DEFAULT_FPS)			// (avoid divide by 0's later)
			gFramesPerSecond = DEFAULT_FPS;

		if (gFramesPerSecond < 9.0f)					// this is the minimum we let it go
			gFramesPerSecond = 9.0f;
		
	}
	else
		gFramesPerSecond = DEFAULT_FPS;
		
//	gFramesPerSecondFrac = 1/gFramesPerSecond;		// calc fractional for multiplication
	gFramesPerSecondFrac = __fres(gFramesPerSecond);	
	
	then = now;										// remember time	
}


#pragma mark -


/************************ SET MULTISAMPLING ************************/

void QD3D_SetMultisampling(Boolean enable)
{
#if ALLOW_MSAA
	static bool multisamplingEnabled = false;
	
	if (multisamplingEnabled == enable)
	{
		// no-op
	}
	else if (!enable && multisamplingEnabled)			// If we want to disable, always do it if MSAA was currently active
	{
		glDisable(GL_MULTISAMPLE);
		multisamplingEnabled = false;
	}
	else if (gGamePrefs.antiAliasing)				// otherwise only honor request if prefs allow MSAA
	{
		if (enable)
			glEnable(GL_MULTISAMPLE);
		else
			glDisable(GL_MULTISAMPLE);
		multisamplingEnabled = enable;
	}
#endif
}


#pragma mark -

/********************* SHOW NORMAL **************************/

void ShowNormal(TQ3Point3D *where, TQ3Vector3D *normal)
{
	gNormalWhere = *where;
	gNormal = *normal;

}

/********************* DRAW NORMAL **************************/

static void DrawNormal(void)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3LineData	line;

	line.lineAttributeSet = nil;

	line.vertices[0].attributeSet = nil;
	line.vertices[0].point = gNormalWhere;

	line.vertices[1].attributeSet = nil;
	line.vertices[1].point.x = gNormalWhere.x + gNormal.x * 400.0f;
	line.vertices[1].point.y = gNormalWhere.y + gNormal.y * 400.0f;
	line.vertices[1].point.z = gNormalWhere.z + gNormal.z * 400.0f;

	Q3Line_Submit(&line, view);
#endif
}

