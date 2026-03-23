/****************************/
/*   ENEMY: QUEENBEE.C		*/
/* (c)1999 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "game.h"


/****************************/
/*    PROTOTYPES            */
/****************************/

static void MoveQueenBee(ObjNode *theNode);
static void MoveQueenBee_Waiting(ObjNode *theNode);
static void  MoveQueenBee_Spitting(ObjNode *theNode);
static void  MoveQueenBee_Death(ObjNode *theNode);
static void UpdateQueenBee(ObjNode *theNode);
static void FindQueenBases(void);
static void  MoveQueenBee_Fly(ObjNode *theNode);
static void ShootSpit(ObjNode *bee);
static void MoveQueenSpit(ObjNode *theNode);
static void  MoveQueenBee_OnButt(ObjNode *theNode);
static void ShootQueenFireball(ObjNode *bee);
static void MoveQueenFireball(ObjNode *theNode);
static void ExplodeQueenFireball(ObjNode *theNode);


/****************************/
/*    CONSTANTS             */
/****************************/

#define	QUEENBEE_KNOCKDOWN_SPEED	1400.0f					// speed ball needs to go to knock this down

#define	QUEENBEE_SCALE				1.5f

#define	QUEENBEE_CHASE_DIST			900.0f
#define	QUEENBEE_ATTACK_DIST		300.0f


#define QUEENBEE_TURN_SPEED			2.0f
#define QUEENBEE_WALK_SPEED			600.0f

#define	QUEENBEE_FOOT_OFFSET		-90.0f

#define	QUEENBEE_HOVER_HEIGHT		400.0f;


enum
{
	QUEENBEE_MODE_FLYTOMID,
	QUEENBEE_MODE_HOVER,
	QUEENBEE_MODE_LAND
};


		/* ANIMS */
enum
{
	QUEENBEE_ANIM_WAIT,
	QUEENBEE_ANIM_SPITTING,
	QUEENBEE_ANIM_FLY,
	QUEENBEE_ANIM_ONBUTT,
	QUEENBEE_ANIM_DEATH
};


#define	MAX_QUEEN_BASES		15

#define	QUEENBEE_JOINT_HEAD	2

#define	SPIT_SPEED		500.0f


/*********************/
/*    VARIABLES      */
/*********************/

ObjNode		*gTheQueen;
int			gNumQueensAlive = 0;

static short 		gNumQueenBases;
static TQ3Point2D	gQueenBase[MAX_QUEEN_BASES];
static Byte			gQueenBaseID[MAX_QUEEN_BASES];
static TQ3Point3D	gMidPoint,gEndPoint;


#define	WaitTimer	SpecialF[0]
#define	SpitTimer	SpecialF[0]
#define	ButtTimer	SpecialF[1]
#define	SpitNowFlag	Flag[0]
#define	CanSpit		Flag[1]
#define	DeathTimer	SpecialF[2]
#define	CurrentBase	SpecialL[0]		// per-queen patrol base index

#define	HasSpawned	Flag[0]
#define	PollenTimer	SpecialF[1]
#define	WobbleX		SpecialF[2]
#define	WobbleY		SpecialF[3]
#define	WobbleZ		SpecialF[4]
#define	WobbleBase	SpecialF[5]

// Fireball-specific (used on fireball ObjNode, not queen bee)
#define	SparkTimer	SpecialF[0]
#define	PGroupA		SpecialL[0]
#define	PGroupB		SpecialL[1]

/************************ ADD QUEENBEE ENEMY *************************/
//
// A skeleton character
//
// NOTE: the parm[3] bits are used as special Queen Bee flags
//

static ObjNode *SpawnOneQueen(TerrainItemEntryType *itemPtr, long x, long z, short startBase, float waitDelay)
{
ObjNode	*newObj;

	gNumQueensAlive++;
	gTheQueen = newObj = MakeEnemySkeleton(SKELETON_TYPE_QUEENBEE, x, z, QUEENBEE_SCALE);
	if (newObj == nil)
		return(nil);
	newObj->TerrainItemPtr = itemPtr;

	SetSkeletonAnim(newObj->Skeleton, QUEENBEE_ANIM_WAIT);

	newObj->Coord.y		= GetTerrainHeightAtCoord(x, z, FLOOR) - QUEENBEE_FOOT_OFFSET;
	newObj->MoveCall	= MoveQueenBee;
	newObj->Health		= QUEENBEE_HEALTH;
	newObj->Damage		= 0;
	newObj->Kind		= ENEMY_KIND_QUEENBEE;
	newObj->WaitTimer	= waitDelay;
	newObj->CurrentBase	= startBase;

	SetObjectCollisionBounds(newObj, 100, QUEENBEE_FOOT_OFFSET, -130, 130, 130, -130);
	AttachShadowToObject(newObj, 13, 13, false);

	gNumEnemies++;
	gNumEnemyOfKind[ENEMY_KIND_QUEENBEE]++;
	return(newObj);
}


Boolean AddEnemy_QueenBee(TerrainItemEntryType *itemPtr, long x, long z)
{
	if (itemPtr->parm[0] != 0)			// only base #0 triggers queen spawn
		return(true);

			/* SCAN ITEM LIST FOR QUEEN BASE OBJECTS */

	FindQueenBases();

	x = gQueenBase[0].x;										// start at 1st base
	z = gQueenBase[0].y;

	SpawnOneQueen(itemPtr, x - 200, z, 0, 3.0f);				// queen 1

	if (gRealLevel == LEVEL_NUM_HELL)
		SpawnOneQueen(itemPtr, x + 200, z, 1, 5.0f);			// queen 2 (Hell only), staggered entry

	return(true);
}


/********************* MOVE QUEENBEE **************************/

static void MoveQueenBee(ObjNode *theNode)
{
static	void(*myMoveTable[])(ObjNode *) =
				{
					MoveQueenBee_Waiting,
					MoveQueenBee_Spitting,
					MoveQueenBee_Fly,
					MoveQueenBee_OnButt,
					MoveQueenBee_Death,
				};

	/* note: we dont track the queen since she is always active and never goes away! */

	GetObjectInfo(theNode);
	myMoveTable[theNode->Skeleton->AnimNum](theNode);	
}



/********************** MOVE QUEENBEE: WAITING ******************************/

static void  MoveQueenBee_Waiting(ObjNode *theNode)
{
float	fps = gFramesPerSecondFrac;
	
			/* AIM AT PLAYER */
			
	TurnObjectTowardTarget(theNode, &gCoord, gMyCoord.x, gMyCoord.z, QUEENBEE_TURN_SPEED, false);			


				/* MOVE */
				
	gDelta.y -= ENEMY_GRAVITY*fps;
	MoveEnemy(theNode);
				

			/* DO ENEMY COLLISION */
				
	if (DoEnemyCollisionDetect(theNode,DEFAULT_ENEMY_COLLISION_CTYPES))
		return;


			/* SEE IF TIME TO SPIT HONEY */
			
	theNode->WaitTimer -= fps;
	if (theNode->WaitTimer <= 0.0f)
	{
		MorphToSkeletonAnim(theNode->Skeleton, QUEENBEE_ANIM_SPITTING, 6);
		theNode->SpitTimer = 2;
		theNode->SpitNowFlag = false;
		theNode->CanSpit = true;
	}


	UpdateQueenBee(theNode);		
}



/********************** MOVE QUEENBEE: SPITTING ******************************/

static void  MoveQueenBee_Spitting(ObjNode *theNode)
{
float		fps;
u_short		b;

	fps = gFramesPerSecondFrac;

	gDelta.y -= ENEMY_GRAVITY*fps;				// add gravity
	MoveEnemy(theNode);


				/* DO ENEMY COLLISION */
				
	if (DoEnemyCollisionDetect(theNode,DEFAULT_ENEMY_COLLISION_CTYPES))
		return;


			/*************************/
			/* SEE IF SHOOT SPIT NOW */
			/*************************/
			
	if (theNode->CanSpit)
	{
		if (theNode->SpitNowFlag)
		{
			theNode->SpitNowFlag = false;
			theNode->CanSpit = false;
			if (gRealLevel == LEVEL_NUM_HELL)
				ShootQueenFireball(theNode);
			else
				ShootSpit(theNode);
		}		
	}

			/*******************************************/
			/* IF DONE SPITTING, THEN FLY TO NEXT BASE */
			/*******************************************/

	theNode->SpitTimer -= fps;
	if (theNode->SpitTimer <= 0.0f)
	{
				/* PICK NEXT BASE TO FLY TO */
				
		if (++theNode->CurrentBase >= gNumQueenBases)				// inc base # and see if wrap
			theNode->CurrentBase = 0;
			
		for (b = 0; b < gNumQueenBases; b++)					// find base # in list
		{
			if (gQueenBaseID[b] == theNode->CurrentBase)
				break;		
		}
			
				/* CALC MIDPOINT TO FLY TO */
				
		gEndPoint.x = gQueenBase[b].x;
		gEndPoint.z = gQueenBase[b].y;
		gMidPoint.x = (gCoord.x + gQueenBase[b].x) * .5f;	
		gMidPoint.z = (gCoord.z + gQueenBase[b].y) * .5f;	
		gMidPoint.y = GetTerrainHeightAtCoord(gMidPoint.x, gMidPoint.z, FLOOR) + QUEENBEE_HOVER_HEIGHT;
		
		
				/* DO FLY ANIM */
				
		MorphToSkeletonAnim(theNode->Skeleton, QUEENBEE_ANIM_FLY, 6);
		theNode->Mode = QUEENBEE_MODE_FLYTOMID;
	}

	UpdateQueenBee(theNode);	
}


/********************** MOVE QUEENBEE: FLY ******************************/

static void  MoveQueenBee_Fly(ObjNode *theNode)
{
float	fps = gFramesPerSecondFrac;
TQ3Vector2D	aim;
Boolean	checkSolid = false;
	
			/* AIM AT PLAYER */
			
	TurnObjectTowardTarget(theNode, &gCoord, gMyCoord.x, gMyCoord.z, QUEENBEE_TURN_SPEED, false);			
	
	
	switch(theNode->Mode)
	{
					/********************/
					/* FLY TO MID POINT */
					/********************/
					
		case	QUEENBEE_MODE_FLYTOMID:
				
						/* CALC VECTOR TO TARGET */
							
				FastNormalizeVector2D(gMidPoint.x - gCoord.x, gMidPoint.z - gCoord.z, &aim);
					
				gDelta.x = aim.x * 400.0f;
				gDelta.z = aim.y * 400.0f;
				
				if (gCoord.y < gMidPoint.y)
					gDelta.y = 400.0f;
				else
				{
					gDelta.y = 0;
					checkSolid = true;
				}
					
				MoveEnemy(theNode);
				
						/* SEE IF REACHED TARGET */
						
				if (CalcQuickDistance(gCoord.x, gCoord.z, gMidPoint.x, gMidPoint.z) < 150.0f)
				{
					theNode->Mode = QUEENBEE_MODE_HOVER;
					theNode->WaitTimer = 3;
				}		
				break;
				
				
					/**************************************/
					/* HOVER WHILE ENEMIES DO THEIR THING */
					/**************************************/
					
		case	QUEENBEE_MODE_HOVER:
		
				ApplyFrictionToDeltas(5, &gDelta);
				MoveEnemy(theNode);
				checkSolid = true;

				theNode->WaitTimer -= fps;
				if (theNode->WaitTimer <= 0.0f)
				{
					theNode->Mode = QUEENBEE_MODE_LAND;				
				}
				break;
				
				
					/******************/
					/* LAND AT TARGET */
					/******************/
					
		case	QUEENBEE_MODE_LAND:
				
						/* CALC VECTOR TO TARGET */
							
				FastNormalizeVector2D(gEndPoint.x - gCoord.x, gEndPoint.z - gCoord.z, &aim);
					
				gDelta.x = aim.x * 400.0f;
				gDelta.z = aim.y * 400.0f;
								
				MoveEnemy(theNode);
				
						/* SEE IF REACHED TARGET */
						
				if (CalcQuickDistance(gCoord.x, gCoord.z, gEndPoint.x, gEndPoint.z) < 100.0f)
				{
					gDelta.x = gDelta.y = gDelta.z = 0;
					MorphToSkeletonAnim(theNode->Skeleton, QUEENBEE_ANIM_WAIT, 5);
					theNode->WaitTimer = 1.5;
				}		

				break;				
	}				
					

			/**********************/
			/* DO ENEMY COLLISION */
			/**********************/
				
	if (DoEnemyCollisionDetect(theNode,DEFAULT_ENEMY_COLLISION_CTYPES))
		return;

		/* IF HIT SOMETHING SOLID WHILE FLYING, THEN LAND */
		
	if (checkSolid && gTotalSides)
	{
		theNode->Mode = QUEENBEE_MODE_LAND;				
		theNode->WaitTimer = 0;
		gEndPoint.x = gCoord.x;
		gEndPoint.z = gCoord.z;
	}	

	UpdateQueenBee(theNode);		
}



/********************** MOVE QUEENBEE: ON BUTT ******************************/

static void  MoveQueenBee_OnButt(ObjNode *theNode)
{
float	fps = gFramesPerSecondFrac;

				/* MOVE IT */
				
	if (theNode->StatusBits & STATUS_BIT_ONGROUND)		// if on ground, add friction
		ApplyFrictionToDeltas(60.0,&gDelta);
	gDelta.y -= ENEMY_GRAVITY*fps;						// add gravity
	MoveEnemy(theNode);


				/* DO ENEMY COLLISION */
				
	if (DoEnemyCollisionDetect(theNode,DEFAULT_ENEMY_COLLISION_CTYPES))
		return;

	theNode->ButtTimer -= fps;
	if (theNode->ButtTimer <= 0.0f)
	{
		MorphToSkeletonAnim(theNode->Skeleton, QUEENBEE_ANIM_WAIT, 6);
		theNode->WaitTimer = 0;
	}

				/* UPDATE */
			
	UpdateQueenBee(theNode);		
}


/********************** MOVE QUEENBEE: DEATH ******************************/

static void  MoveQueenBee_Death(ObjNode *theNode)
{
float	fps = gFramesPerSecondFrac;

	theNode->DeathTimer -= fps;
	if (theNode->DeathTimer <= 0.0f)
	{
		if (--gNumQueensAlive <= 0)
			gAreaCompleted = true;
	}


				/* MOVE IT */
				
	if (theNode->StatusBits & STATUS_BIT_ONGROUND)		// if on ground, add friction
		ApplyFrictionToDeltas(60.0,&gDelta);
	gDelta.y -= ENEMY_GRAVITY*fps;		// add gravity
	MoveEnemy(theNode);


				/* DO ENEMY COLLISION */
				
	if (DoEnemyCollisionDetect(theNode,DEATH_ENEMY_COLLISION_CTYPES))
		return;


				/* UPDATE */
			
	UpdateQueenBee(theNode);		

}



//===============================================================================================================
//===============================================================================================================
//===============================================================================================================






#pragma mark -

/******************* BALL HIT QUEENBEE *********************/
//
// OUTPUT: true if enemy gets killed
//
// Worker bees cannot be knocked down.  Instead, it just pisses them off.
//

Boolean BallHitQueenBee(ObjNode *me, ObjNode *enemy)
{
Boolean	killed = false;

				
	if (me->Speed > QUEENBEE_KNOCKDOWN_SPEED)
	{	
		
				/*****************/
				/* KNOCK ON BUTT */
				/*****************/
					
		killed = KnockQueenBeeOnButt(enemy, me->Delta.x * .3f, me->Delta.z * .3f, .7);

		PlayEffect_Parms3D(EFFECT_POUND, &gCoord, kMiddleC+2, 2.0);
	}

	return(killed);		
}


/***************** KNOCK QUEEN BEE ON BUTT *****************/
//
// INPUT: dx,y,z = delta to apply to fireant 
//

Boolean KnockQueenBeeOnButt(ObjNode *enemy, float dx, float dz, float damage)
{
TQ3Vector3D		delta;

	if (enemy->Skeleton->AnimNum == QUEENBEE_ANIM_ONBUTT)		// see if already in butt mode
		return(false);

		/* DO BUTT ANIM */
		
	MorphToSkeletonAnim(enemy->Skeleton, QUEENBEE_ANIM_ONBUTT, 7.0);
	enemy->ButtTimer = 2.0;

			/* GET IT MOVING */
		
	enemy->Delta.x = dx;
	enemy->Delta.z = dz;
	enemy->Delta.y = 600;
	

		/* SLOW DOWN PLAYER */
		
	gDelta.x *= .2f;
	gDelta.y *= .2f;
	gDelta.z *= .2f;


			/*******************/
			/* SPARK EXPLOSION */
			/*******************/

			/* white sparks */
				
	int32_t pg = NewParticleGroup(
							PARTICLE_TYPE_FALLINGSPARKS,	// type
							PARTICLE_FLAGS_BOUNCE,		// flags
							500,						// gravity
							0,							// magnetism
							20,							// base scale
							.9,							// decay rate
							0,							// fade rate
							PARTICLE_TEXTURE_YELLOWBALL);	// texture

	for (int i = 0; i < 35; i++)
	{
		delta.x = (RandomFloat()-.5f) * 1000.0f;
		delta.y = (RandomFloat()-.5f) * 1000.0f;
		delta.z = (RandomFloat()-.5f) * 1000.0f;
		AddParticleToGroup(pg, &enemy->Coord, &delta, RandomFloat() + 1.0f, FULL_ALPHA);
	}



		/* HURT & SEE IF KILLED */
			
	if (EnemyGotHurt(enemy, damage))
		return(true);
		
	gInfobarUpdateBits |= UPDATE_BOSS;
		
	return(false);
}




/****************** KILL FIRE QUEENBEE *********************/
//
// OUTPUT: true if ObjNode was deleted
//

Boolean KillQueenBee(ObjNode *theNode)
{	
TQ3Vector3D		delta;

		/* STOP BUZZ */
		
	if (theNode->EffectChannel != -1)
		StopAChannel(&theNode->EffectChannel);

			/* DEACTIVATE */
			
	theNode->TerrainItemPtr = nil;				// dont ever come back
	theNode->CType = CTYPE_MISC;
	
	MorphToSkeletonAnim(theNode->Skeleton, QUEENBEE_ANIM_DEATH, 8);
	
	
			/*******************/
			/* SPARK EXPLOSION */
			/*******************/

			/* white sparks */
				
	int32_t pg = NewParticleGroup(
							PARTICLE_TYPE_FALLINGSPARKS,	// type
							PARTICLE_FLAGS_BOUNCE,		// flags
							400,						// gravity
							0,							// magnetism
							20,							// base scale
							.8,							// decay rate
							0,							// fade rate
							PARTICLE_TEXTURE_YELLOWBALL);	// texture

	if (pg != -1)
	{
		for (int i = 0; i < 60; i++)
		{
			delta.x = (RandomFloat()-.5f) * 1400.0f;
			delta.y = (RandomFloat()-.5f) * 1400.0f;
			delta.z = (RandomFloat()-.5f) * 1400.0f;
			AddParticleToGroup(pg, &theNode->Coord, &delta, RandomFloat() + 1.0f, FULL_ALPHA);
		}
	}

	theNode->DeathTimer = 4;
	
	return(false);
}





/***************** UPDATE QUEENBEE ************************/

static void UpdateQueenBee(ObjNode *theNode)
{
		/* SEE IF DO BUZZ */
		
	if (theNode->Skeleton->AnimNum == QUEENBEE_ANIM_FLY)
	{
		if (theNode->EffectChannel == -1)
			theNode->EffectChannel = PlayEffect_Parms3D(EFFECT_BUZZ, &theNode->Coord, kMiddleC-3, 3.0);
		else
			Update3DSoundChannel(EFFECT_BUZZ, &theNode->EffectChannel, &theNode->Coord);
	}
	
		/* SEE IF TURN OFF BUZZ */
		
	else
	{
		if (theNode->EffectChannel != -1)
		{
			StopAChannel(&theNode->EffectChannel);
		}
	}

	UpdateEnemy(theNode);
}


#pragma mark -

/******************** FIND QUEEN BASES *******************/
//
// Scans the map item list for queen bases and adds them to the list.
//

static void FindQueenBases(void)
{
long					i;
TerrainItemEntryType	*itemPtr;

	gNumQueenBases = 0;

	itemPtr = *gMasterItemList; 										

	for (i= 0; i < gNumTerrainItems; i++)
	{
		if (itemPtr[i].type == MAP_ITEM_QUEENBEEBASE)					
		{
			gQueenBase[gNumQueenBases].x = itemPtr[i].x * MAP2UNIT_VALUE;	// convert to world coords
			gQueenBase[gNumQueenBases].y = itemPtr[i].y * MAP2UNIT_VALUE;
			gQueenBaseID[gNumQueenBases] = itemPtr[i].parm[0];				// remember ID#
			
			gNumQueenBases++;
			
			if (gNumQueenBases >= MAX_QUEEN_BASES)							// see if @ max
				break;
		}
	}
	
	GAME_ASSERT_MESSAGE(gNumQueenBases > 0, "No bases found");
}

#pragma mark -

/********************** SHOOT SPIT ***********************/

static void ShootSpit(ObjNode *bee)
{
float	r;
ObjNode	*spit;
static const TQ3Point3D off = {0,-25,-55};

		/*******************/	
		/* CREATE SPIT WAD */
		/*******************/	

	FindCoordOnJoint(bee, QUEENBEE_JOINT_HEAD, &off, &gNewObjectDefinition.coord);
		
	gNewObjectDefinition.group 		= MODEL_GROUP_LEVELSPECIFIC;
	gNewObjectDefinition.type 		= HIVE_MObjType_HoneyBlob;
	gNewObjectDefinition.flags 		= 0;
	gNewObjectDefinition.slot 		= 550;
	gNewObjectDefinition.moveCall 	= MoveQueenSpit;
	gNewObjectDefinition.rot 		= 0;
	gNewObjectDefinition.scale 		= .15;		
	spit = MakeNewDisplayGroupObject(&gNewObjectDefinition);
	if (spit == nil)
		return;
			
	spit->WobbleBase = spit->Scale.x;
				
		/* SET COLLISION INFO */

	spit->CType = CTYPE_VISCOUS;
	spit->CBits = CBITS_TOUCHABLE;	
	SetObjectCollisionBounds(spit,300,0,-200,200,200,-200);

	spit->BoundingSphere.radius = 250;		// Source port fix: Give it a generous culling sphere

			/* SET DELTAS */
			
	r = bee->Rot.y;

	spit->Delta.x = -sin(r) * SPIT_SPEED;
	spit->Delta.z = -cos(r) * SPIT_SPEED;
	spit->Delta.y = 250;
	
	spit->HasSpawned = false;
	spit->SpitTimer	= 80;
}


/**************** MOVE QUEEN SPIT ******************/

static void MoveQueenSpit(ObjNode *theNode)
{
float	fps = gFramesPerSecondFrac;
float	y;
float	base;

	GetObjectInfo(theNode);

	theNode->SpitTimer -= fps;
	
			/****************/
			/* MAKE GO AWAY */
			/****************/
			
	if (theNode->SpitTimer < 0.0f)
	{
		gCoord.y -= 40.0 * fps;
		if (gCoord.y < GetTerrainHeightAtCoord(gCoord.x, gCoord.z, FLOOR) - 400.0f)
		{
			DeleteObject(theNode);
			return;
		}	
	}
	
			/*******************/
			/* ACTIVE SPIT WAD */
			/*******************/
	else
	{

			/* MOVE */
				
		ApplyFrictionToDeltas(5, &gDelta);
		gDelta.y -= 800.0f * fps;					// gravity
		gCoord.x += gDelta.x * fps;
		gCoord.y += gDelta.y * fps;
		gCoord.z += gDelta.z * fps;
		
		y = GetTerrainHeightAtCoord(gCoord.x, gCoord.z, FLOOR) + 20.0f;
		if (gCoord.y < y)
		{
			gDelta.y *= -.3f;
			gCoord.y = y;
		}

			/* FADE */
			
//		if (theNode->Health < .95f)
//		{
//			theNode->Health += fps * .05f;
//			if (theNode->Health > .95f)
//				theNode->Health = .95f;
//		
//			MakeObjectTransparent(theNode, theNode->Health);
//		}

			/* SCALE */
					
		base = theNode->WobbleBase;
		
		theNode->WobbleX += fps * 4.5f;
		theNode->WobbleY -= fps * 5.0f;
		theNode->WobbleZ += fps * 4.0f;
		
		theNode->Scale.x = base + sin(theNode->WobbleX) * .2f * base;
		theNode->Scale.y = base + sin(theNode->WobbleY) * .2f * base;
		theNode->Scale.z = base + cos(theNode->WobbleZ) * .2f * base;
		
		if (theNode->WobbleBase < 2.2f)
			theNode->WobbleBase += fps * .5f;
		
			/***************/
			/* SPAWN ENEMY */
			/***************/
			
		else
		{	
			if (!theNode->HasSpawned)
			{			
				theNode->HasSpawned = true;

					/* SEE WHICH ENEMY TO SPAWN */
								
				if (gTheQueen->Health < (QUEENBEE_HEALTH/2))
				{
					MakeFlyingBee(&gCoord);
				}
				else
					MakeLarvaEnemy(gCoord.x, gCoord.z);
			}
		}
	}
	
			/* UPDATE */

	UpdateObject(theNode);
}


#pragma mark -

/******************* SHOOT QUEEN FIREBALL (Hell level) ***********************/

static void ShootQueenFireball(ObjNode *bee)
{
ObjNode		*newObj;
TQ3Vector3D	delta;
static const TQ3Point3D off = {0,-25,-55};

	FindCoordOnJoint(bee, QUEENBEE_JOINT_HEAD, &off, &gNewObjectDefinition.coord);

	gNewObjectDefinition.genre    = EVENT_GENRE;
	gNewObjectDefinition.flags    = 0;
	gNewObjectDefinition.slot     = 500;
	gNewObjectDefinition.moveCall = MoveQueenFireball;
	newObj = MakeNewObject(&gNewObjectDefinition);
	if (newObj == nil)
		return;

	newObj->CType  = CTYPE_HURTME;
	newObj->CBits  = CBITS_TOUCHABLE;
	SetObjectCollisionBounds(newObj, 70, -70, -70, 70, 70, -70);

	newObj->Damage = .3f;

	newObj->SparkTimer = 0;
	newObj->PGroupA    =
	newObj->PGroupB    = -1;

		/* AIM AT PLAYER */

	delta.x = gMyCoord.x - bee->Coord.x;
	delta.y = (gMyCoord.y + 50.0f) - bee->Coord.y;
	delta.z = gMyCoord.z - bee->Coord.z;
	FastNormalizeVector(delta.x, delta.y, delta.z, &delta);
	delta.x *= SPIT_SPEED * 2.0f;
	delta.y *= SPIT_SPEED * 2.0f;
	delta.z *= SPIT_SPEED * 2.0f;
	newObj->Delta = delta;

	PlayEffect_Parms3D(EFFECT_PLASMABURST, &newObj->Coord, kMiddleC, 2.0);
}


/********************* MOVE QUEEN FIREBALL **********************/

static void MoveQueenFireball(ObjNode *theNode)
{
int			i;
TQ3Vector3D	delta;
float		fps = gFramesPerSecondFrac;

	GetObjectInfo(theNode);

	gCoord.x += gDelta.x * fps;
	gCoord.y += gDelta.y * fps;
	gCoord.z += gDelta.z * fps;

	if (gCoord.y <= GetTerrainHeightAtCoord(gCoord.x, gCoord.z, FLOOR))
	{
		ExplodeQueenFireball(theNode);
		return;
	}

	if (DoSimpleBoxCollision(gCoord.y+20, gCoord.y-20, gCoord.x-20, gCoord.x+20, gCoord.z+20, gCoord.z-20, CTYPE_MISC|CTYPE_PLAYER))
	{
		ExplodeQueenFireball(theNode);
		return;
	}

	UpdateObject(theNode);

	theNode->SparkTimer -= fps;
	if (theNode->SparkTimer <= 0.0f)
	{
		theNode->SparkTimer = .03f;

		if (!VerifyParticleGroup(theNode->PGroupA))
		{
			theNode->PGroupA = NewParticleGroup(
								PARTICLE_TYPE_FALLINGSPARKS,
								PARTICLE_FLAGS_HOT,
								0,
								0,
								10,
								-10.0f,
								2.0f,
								PARTICLE_TEXTURE_BLUEFIRE);
		}
		if (theNode->PGroupA != -1)
		{
			delta.x = delta.y = delta.z = 0;
			AddParticleToGroup(theNode->PGroupA, &gCoord, &delta, RandomFloat()*2.0f + 1.0f, FULL_ALPHA);
		}

		if (theNode->PGroupB == -1)
		{
			theNode->PGroupB = NewParticleGroup(
								PARTICLE_TYPE_FALLINGSPARKS,
								PARTICLE_FLAGS_HOT,
								900,
								0,
								10,
								1.6f,
								0,
								PARTICLE_TEXTURE_WHITE);
		}
		if (theNode->PGroupB != -1)
		{
			for (i = 0; i < 3; i++)
			{
				delta.x = (RandomFloat()-.5f) * 700.0f;
				delta.y = (RandomFloat()-.5f) * 700.0f;
				delta.z = (RandomFloat()-.5f) * 700.0f;
				AddParticleToGroup(theNode->PGroupB, &gCoord, &delta, RandomFloat()*2.0f + 2.0f, FULL_ALPHA);
			}
		}
	}
}


/****************** EXPLODE QUEEN FIREBALL ***********************/

static void ExplodeQueenFireball(ObjNode *theNode)
{
TQ3Vector3D	delta;

	int32_t pg = NewParticleGroup(
					PARTICLE_TYPE_FALLINGSPARKS,
					PARTICLE_FLAGS_BOUNCE|PARTICLE_FLAGS_HURTPLAYER|PARTICLE_FLAGS_HOT,
					400,
					0,
					40,
					0,
					.7f,
					PARTICLE_TEXTURE_BLUEFIRE);

	if (pg != -1)
	{
		for (int i = 0; i < 60; i++)
		{
			delta.x = (RandomFloat()-.5f) * 1400.0f;
			delta.y = (RandomFloat()-.5f) * 1400.0f;
			delta.z = (RandomFloat()-.5f) * 1400.0f;
			AddParticleToGroup(pg, &theNode->Coord, &delta, RandomFloat() + 1.0f, FULL_ALPHA);
		}
	}

	PlayEffect_Parms3D(EFFECT_PLASMAEXPLODE, &theNode->Coord, kMiddleC-4, 6.0);
	DeleteObject(theNode);
}









