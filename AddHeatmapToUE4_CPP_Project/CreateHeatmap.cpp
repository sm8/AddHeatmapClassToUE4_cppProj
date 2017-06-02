// SM

#include "Heatmap100.h"
#include "CreateHeatmap.h"
#include "FileHelpers.h"	//for file I/O
#include "HighResScreenShot.h"

//
// Taken from: https://forums.unrealengine.com/showthread.php?104816-How-to-save-UTexture2D-to-PNG-file
//
void ACreateHeatmap::SaveTexture2DDebug(const uint8* PPixelData, int w, int h, FString Filename){
	TArray<FColor> OutBMP;
	//int w = width;
	//int h = height;

	OutBMP.InsertZeroed(0, w*h);

	for (int i = 0; i < (w*h); ++i) {
		uint8 R = PPixelData[i * 4 + 2];
		uint8 G = PPixelData[i * 4 + 1];
		uint8 B = PPixelData[i * 4 + 0];
		uint8 A = PPixelData[i * 4 + 3];

		OutBMP[i].R = R;
		OutBMP[i].G = G;
		OutBMP[i].B = B;
		OutBMP[i].A = A;
	}

	FIntPoint DestSize(w, h);
	FString ResultPath;
	FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
	bool bSaved = HighResScreenshotConfig.SaveImage(Filename, OutBMP, DestSize, &ResultPath);

	UE_LOG(LogTemp, Warning, TEXT("SaveTexture2DDebug: %d %d"), w, h);
	UE_LOG(LogTemp, Warning, TEXT("SaveTexture2DDebug: %s %d"), *ResultPath, bSaved == true ? 1 : 0);
}

// Sets default values
ACreateHeatmap::ACreateHeatmap(){
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance.
	PrimaryActorTick.bCanEverTick = true;

	totTime = prevTime = maxTimeAtPos = playerRad = maxX = 0.0f;
	nameOfPlatform = "Floor";
	nameOfPlayerToTrack = "player0";
	player = NULL;
	platform = NULL;
	prevLoc = FVector::ZeroVector;
	heatMapProcessed = textFileAnalysed = false;
	widthOfHeatmapInPixels = heightOfHeatmapInPixels = 1024;
	playerMoveCol = FColor(0, 100, 10, 255);	//default colours for player movement
	staticObjCol = FColor(50, 50, 50, 100);	//by default, draw STATIC obj in grey
}

void ACreateHeatmap::getMaxExtent(Extent &currHigh, FVector org, FVector box) {
	float xt = (org - box).X;	//calc bounds of static objs
	float yt = (org + box).Y;
	float xb = (org + box).X;
	float yb = (org - box).Y;
	if (xt < currHigh.topL.X) currHigh.topL.X = xt;
	if (xb > currHigh.botR.X) currHigh.botR.X = xb;
	if (yt > currHigh.topL.Y) currHigh.topL.Y = yt;
	if (yb < currHigh.botR.Y) currHigh.botR.Y = yb;
}

// Called when the game starts or when spawned
void ACreateHeatmap::BeginPlay(){
	Super::BeginPlay();
	
	FVector orig, maxB, highMax = FVector::ZeroVector; 
	w = widthOfHeatmapInPixels; h = heightOfHeatmapInPixels;
	for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr) {
		if (ActorItr->GetName().Contains(nameOfPlatform)) {	//get sizes for Heatmap
			platform = *ActorItr;
			ActorItr->GetActorBounds(true, orig, maxB);	//get bounds of platform
			maxX = maxB.X; maxY = maxB.Y;
			minX = -maxX; minY = -maxY;
		} else if (ActorItr->GetName().Contains(nameOfPlayerToTrack)) {
			player = (APawn*)*ActorItr;
			player->GetActorBounds(true, orig, maxB);	//get bounds of player
			playerRad = maxB.X;
		}
		else {	//find ALL STATIC mesh comps, so can chk whether static mobility
			chkStaticMeshComps(ActorItr, orig, maxB, highMax);
			chkBrushComps(orig, maxB, highMax);
		}
	}
	if (maxX == 0.0f) {	//if platform NOT found then use largest bounds found
		maxX = highMax.X; maxY = highMax.Y;
		minX = -maxX; minY = -maxY;
		for (int i = 0; i < staticActors.Num(); i++) {
			ActorAndBounds *ab = staticActors[i];
			if (ab->actor->GetName() == platform->GetName())
				staticActors.RemoveAt(i);
		}
	}
	if (maxX < maxExtent.botR.X) maxX = maxExtent.botR.X; //ensure static objs are in bounds
	if (maxY < maxExtent.topL.Y) maxY = maxExtent.topL.Y;
	if (minX > maxExtent.topL.X) minX = maxExtent.topL.X;
	if (minY > maxExtent.botR.Y) minY = maxExtent.botR.Y;

	if (player == NULL) {	//if player NOT found, use default player
		player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		player->GetActorBounds(true, orig, maxB);
		playerRad = maxB.X;
	}
	if (checkBindingIsSetup("SaveHeatmap")) {
		player->InputComponent->BindAxis(TEXT("SaveHeatmap"));
		player->InputComponent->BindAxis(TEXT("AnalyseTextFile")); 
	}
	else {
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, "Heatmap cannot be created. Axis mapping: SaveHeatmap has NOT been setup. Use Project settings, Input to set it up");
		return;
	}
	float minGrid = FMath::Min((maxX - minX) / (float)w, (maxY - minY) / (float)h);
	chkSqVal = minGrid * minGrid  * 0.25f;	//use half grid size squared for calc of pos chg
}

void ACreateHeatmap::chkBrushComps(FVector &orig, FVector &maxB, FVector &highMax) {
	for (TActorIterator<ABrush> ActorItr(GetWorld()); ActorItr; ++ActorItr) {
		TArray<UBrushComponent*> Components;
		ActorItr->GetComponents<UBrushComponent>(Components);
		FString n = ActorItr->GetName();
		for (int32 i = 0; i < Components.Num(); i++) {
			UBrushComponent* StaticMeshComponent = Components[i];
			if (!StaticMeshComponent->Mobility && n != nameOfPlatform) //assume static is 0
				updateMaxExtent(*ActorItr, orig, maxB, highMax);
		}
	}
}

void ACreateHeatmap::updateMaxExtent(AActor *actor, FVector & orig, FVector & maxB, FVector & highMax){
	actor->GetActorBounds(true, orig, maxB);	//chk bounds of static obj
	getMaxExtent(maxExtent, orig, maxB);
	float dx = maxExtent.botR.X - maxExtent.topL.X;
	float dy = maxExtent.topL.Y - maxExtent.botR.Y;
	if (dx*dy > highMax.X*highMax.Y) {
		platform = actor;
		highMax = maxExtent.botR; //in case platform not found
	}
	if (maxB.X > 0.0f && maxB.Y > 0.0f)	//Eg sky sphere is static but has 0 bounds
		staticActors.Add(new ActorAndBounds(actor, orig, maxB));
}

void ACreateHeatmap::chkStaticMeshComps(TActorIterator<AActor> &ActorItr, FVector &orig, FVector &maxB, FVector &highMax){
	TArray<UStaticMeshComponent*> Components;
	ActorItr->GetComponents<UStaticMeshComponent>(Components);
	//			FString n = ActorItr->GetName();
	for (int32 i = 0; i<Components.Num(); i++) {
		UStaticMeshComponent* StaticMeshComponent = Components[i];
		if (!StaticMeshComponent->Mobility) //assume static is 0
			updateMaxExtent(*ActorItr, orig, maxB, highMax);
	}
}

bool ACreateHeatmap::checkBindingIsSetup(FString axisMapName){
	const UInputSettings* InputSettings = GetDefault<UInputSettings>();
	TArray<FName> axisMappingNames;
	InputSettings->GetAxisNames(axisMappingNames);
	for (int i = 0; i < axisMappingNames.Num(); i++) 
		if (axisMappingNames[i].ToString() == axisMapName)
			return true;
	return false;
}

// Called every frame
void ACreateHeatmap::Tick( float DeltaTime ){
	Super::Tick( DeltaTime );

	totTime += DeltaTime;	//used for Heatmap

	FVector newLocation = player->GetActorLocation();	//get PLAYER POS!!!!
	if ((newLocation - prevLoc).SizeSquared2D() > chkSqVal) {
		updatePositionData(newLocation);	//for Heatmap
		prevLoc = newLocation;
	}

	if (player->InputComponent->GetAxisValue(TEXT("SaveHeatmap")) != 0.0f)
		saveHeatmap();
	if (player->InputComponent->GetAxisValue(TEXT("AnalyseTextFile")))
		analyseTextFilePositions();
}

void ACreateHeatmap::saveHeatmap(){
	if (!heatMapProcessed) {
		updateLastPositionInArrays();
		//FString p = FPlatformMisc::GameDir();	//get base folder of project
		//FFileHelper::SaveStringToFile(allPlayerPos, *FString::Printf(TEXT("%sHeatmapPos.txt"), *p)); //save pos & dt
		//analyseTextFilePositions();
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, TEXT("Heatmap data being generated"));
		uint8 *pixels = createHeatMapData();

		FString p = FPlatformMisc::GameDir();	//get base folder of project
		FFileHelper::SaveStringToFile(allPlayerPos, *FString::Printf(TEXT("%sHeatmapPos.txt"), *p)); //save pos & dt
	//	outputArrayCSVfile(w, h, pixels, p + "Heatmap.csv");	//for testing / debugging
		SaveTexture2DDebug(pixels, w, h, p + "Heatmap.png");	//create Heatmap as PNG
		clearArrays(pixels);
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, *FString::Printf(TEXT("Heatmap created in: %sHeatmap.png"), *p));
		heatMapProcessed = true;
	}
}

uint8* ACreateHeatmap::createHeatMapData() {
	uint8 *pixels = new uint8[w*h * BPP];	//4*8bits for each colour & alpha
	plyrPosCount = new unsigned int[w*h];	//calc freqs in each pos
	for (unsigned int i = 0; i < w*h * BPP; i++) pixels[i] = 0;	//init to 0
	for (unsigned int i = 0; i < w*h; i++) plyrPosCount[i] = 0;	
	lowestTimeAtPos += 0.001f;	//in case of rounding errors
	highFreq = 0;

	float gx = (maxX - minX) / (float)w;  	//calc 'grid' sizes
	float gy = (maxY - minY) / (float)h;
	addStaticBoundsToHeatmap(pixels, gx, gy);	//add static objs to Heatmap

	const int NUM_CHKS = w / 7;	//num of times to calc ang around ctr
	float angle = FMath::DegreesToRadians(360.0f / ((float)NUM_CHKS));
	float tr, rx, ry;
	int xp, yp, pxp=0, pyp=0;
	TMap<int, int> posMap;
	for (int i = 0; i < playerPos.Num(); i++) {	//process each player pos
//		tr = 0.0f;	//start at ctr. This is inefficient!
		tr = gx*0.5f;
//		tr = gx;
		posMap.Empty();
		rx = playerPos[i].x; ry = playerPos[i].y;
		xp = getGridPos(rx, minX, gx);	//calc array pos
		yp = getGridPos(ry, minY, gy);
		plyrPosCount[xp + yp*w] += playerPos[i].dt / lowestTimeAtPos + 1;
		posMap.Add(xp + yp*w);
		while (tr < playerRad) {
			for (int k = 0; k < NUM_CHKS; k++) {	//chk around ctr
				rx = playerPos[i].x; ry = playerPos[i].y;
				rx += tr*FMath::Cos((float)k*angle);
				ry += tr*FMath::Sin((float)k*angle);
				xp = getGridPos(rx, minX, gx);	//calc array pos
				yp = getGridPos(ry, minY, gy);
//				if (xp == pxp && yp == pyp) continue;	//if prev pos already found exit
				//uint8 newColour = (uint8)(254.0f * playerPos[i].dt / maxTimeAtPos) + 1;
				//int oldColour = pixels[BPP * (xp + yp*w) + 2];
				//if (oldColour > newColour) newColour = oldColour;	//Red
				//set32BitPixel(pixels, BPP * (xp + yp*w), newColour, playerMoveCol.G, playerMoveCol.B, playerMoveCol.A);
				if (posMap.Contains(xp + yp*w)) continue;
				plyrPosCount[xp + yp*w] += playerPos[i].dt / lowestTimeAtPos + 1;	//calc freq at each pt in array for PNG
//				UE_LOG(LogTemp, Warning, TEXT("plyrPosCount[%d] = %d"), xp + yp*w, plyrPosCount[xp + yp*w]);
				if (plyrPosCount[xp + yp*w] > highFreq) highFreq = plyrPosCount[xp + yp*w];
//				plyrPosCount[xp + yp*w] = newColour;	//calc freq at each pt in array for PNG
//				pxp = xp; pyp = yp;
				posMap.Add(xp + yp*w);
			}
			tr += gx*0.5f;	//increase rad by half grid width each time
//			tr += gx;	//increase rad by half grid width each time
		}
//		if (i == 0) break;	//**** for testing
	}

	for (unsigned int i = 0; i < w*h; i++) {
		if(plyrPosCount[i] > 0)
			set32BitPixel(pixels, BPP * i, (int)(244.0f * (float)plyrPosCount[i]/ (float)highFreq) + 10, playerMoveCol.G, playerMoveCol.B, playerMoveCol.A);
//			set32BitPixel(pixels, BPP * i, plyrPosCount[i], playerMoveCol.G, playerMoveCol.B, playerMoveCol.A);
	}
//	outputArrayCSVfile(w, h, plyrPosCount, "plyrPosCount.csv");
	delete[] plyrPosCount;
	return pixels;
}

void ACreateHeatmap::clearArrays(uint8 * pixels){
	delete pixels;	//clear dyn memory
	allPlayerPos.Empty();
	playerPos.Empty();
	playerPositions.Empty();
}

//
// Draw STATIC objs on Heatmap, in grey - assume AABBs. 
// 
void ACreateHeatmap::addStaticBoundsToHeatmap(uint8 * pixels, float gx, float gy){
	float tx, ty, rx, ry;
	for (int32 i = 0; i < staticActors.Num(); i++) {	//****test array
//		UE_LOG(LogTemp, Warning, TEXT("Static actor name:  %s maxB: %s orig: %s"), *staticActors[i]->actor->GetName(), *staticActors[i]->maxBounds.ToString(), *staticActors[i]->org.ToString());
		tx = ty = 0.0f;
		while (tx <= staticActors[i]->maxBounds.X) {
			rx = staticActors[i]->org.X - staticActors[i]->maxBounds.X + 2.0f*tx;
			ty = 0.0f;
			while (ty <= staticActors[i]->maxBounds.Y) {
				ry = staticActors[i]->org.Y - staticActors[i]->maxBounds.Y + 2.0f*ty;
				int xp = getGridPos(rx, minX, gx);	//calc array pos
				int yp = getGridPos(ry, minY, gy);
				set32BitPixel(pixels, BPP * (xp + yp*w), staticObjCol.R, staticObjCol.G, staticObjCol.B, staticObjCol.A);
				ty += gy*0.5f;	//increase rad by half grid width each time
			}
			tx += gx*0.5f;
		}
	}
}

void ACreateHeatmap::set32BitPixel(uint8 *pixels, int pos, uint8 r, uint8 g, uint8 b, uint8 a){
	pixels[pos + 2] = r;	//draw STATIC obj in grey
	pixels[pos + 1] = g;
	pixels[pos] = b;
	pixels[pos + 3] = a;
}

//
// For TESTING purposes.  Numeric array output in rows and can be viewed in Excel
// for pixel output, mult w or h by BPP
template <class T>
void ACreateHeatmap::outputArrayCSVfile(int w, int h, T *pixels, FString filename){
	FString arrayOut = "Num,Value\n";
	for (int i = 0; i < w*h ; i++)
		arrayOut += FString::Printf(TEXT("%d,%d\n"), i, pixels[i]);
	arrayOut += FString::Printf(TEXT("\nMin,=MIN(B2:B%d)"), w*h + 1);	//For Excel, output Summary stats
	arrayOut += FString::Printf(TEXT("\nMax,=MAX(B2:B%d)"), w*h + 1);
	arrayOut += FString::Printf(TEXT("\nAvg,=AVERAGE(B2:B%d)\n"), w*h + 1);
	FFileHelper::SaveStringToFile(arrayOut, *filename);
}

float ACreateHeatmap::calcDiffInTime() {
	float diffT = totTime - prevTime;	//get LAST pos
	if (diffT > maxTimeAtPos) maxTimeAtPos = diffT;
	return diffT;
}

void ACreateHeatmap::updateLastPositionInArrays(){
	float diffT = calcDiffInTime();
	playerPos[playerPos.Num() - 1].dt = diffT;	//change last array time
	FVector newLocation = player->GetActorLocation();
	playerPositions.RemoveAt(playerPositions.Num() - 1);	//remove last pos
	addPlayerPosToTextArray(newLocation, diffT);
}

void ACreateHeatmap::updatePositionData(FVector &newLocation){
	float diffT = calcDiffInTime();
	PosData p(newLocation.X, newLocation.Y, newLocation.Z, diffT);
	playerPos.Add(p);	//add to dyn array
	addPlayerPosToTextArray(newLocation, diffT);
	prevTime = totTime;
}

void ACreateHeatmap::addPlayerPosToTextArray(FVector &newLocation, float diffT){
	FString d = FString::Printf(TEXT("%f,%f,%f,%f"), newLocation.X, newLocation.Y, newLocation.Z, diffT);
	playerPositions.Add(d);	//add to dyn string array
	allPlayerPos += d + "\r\n";	//append for txt output
}

unsigned int ACreateHeatmap::getGridPos(float rx, float minXpos, float gx) {
	float epsilon = 0.001f;	//for possible error in calcs
	float px = (rx - minXpos) / (gx + epsilon);	//calc grid pos
	return (unsigned int)px + 1;
}

void ACreateHeatmap::analyseTextFilePositions() {
	if (!textFileAnalysed) {
		TArray<FString> playerPositionsAsStr;
		FString p = FPlatformMisc::GameDir();
		FFileHelper::LoadANSITextFileToStrings(*FString::Printf(TEXT("%sHeatmapPos.txt"), *p), NULL, playerPositionsAsStr);
		UE_LOG(LogTemp, Warning, TEXT("Loaded HeatmapPos.txt"));
		float f[4];
		int s;
		playerPos.Empty();
		maxTimeAtPos = 0.0f;
		lowestTimeAtPos = 9999.9f;
		for (int i = 0; i < playerPositionsAsStr.Num(); i++) {
			s = 0;
			if (!playerPositionsAsStr[i].IsEmpty()) {	//get x,y,z & dt vals
				for (int j = 0; j < 3; j++) {
					int pos = playerPositionsAsStr[i].Find(",", ESearchCase::IgnoreCase, ESearchDir::FromStart, s);
					f[j] = FCString::Atof(*playerPositionsAsStr[i].Mid(s, pos));
					s = pos + 1;
				}
				f[3] = FCString::Atof(*playerPositionsAsStr[i].Mid(s));
				playerPos.Add(PosData(f[0], f[1], f[2], f[3]));
				if (f[3] > maxTimeAtPos) maxTimeAtPos = f[3];	//in case dt is diff to that found above
				if (f[3] < lowestTimeAtPos) lowestTimeAtPos = f[3];	//find lowest dt
			}
		}
		uint8 *pixels = createHeatMapData();
		SaveTexture2DDebug(pixels, w, h, p + "Heatmap.png");	//create Heatmap as PNG
		clearArrays(pixels);
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, *FString::Printf(TEXT("From text file, Heatmap in: %sHeatmap.png"), *p));
		textFileAnalysed = true;
	}
}


