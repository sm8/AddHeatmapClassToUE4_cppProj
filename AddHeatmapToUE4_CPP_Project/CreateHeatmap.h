// SM

#pragma once

#include "GameFramework/Actor.h"
#include "Engine.h"	//Req'd???
#include "CreateHeatmap.generated.h"

UCLASS()
class HEATMAP100_API ACreateHeatmap : public AActor {
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACreateHeatmap();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void chkStaticMeshComps(TActorIterator<AActor> &ActorItr, FVector &orig, FVector &maxB, FVector &highMax);
	void chkBrushComps(FVector &orig, FVector &maxB, FVector &highMax);
	void updateMaxExtent(AActor *actor, FVector & orig, FVector & maxB, FVector & highMax);

	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	UPROPERTY(EditAnywhere)
		FString nameOfPlatform;
	UPROPERTY(EditAnywhere)
		FString nameOfPlayerToTrack;
	UPROPERTY(EditAnywhere)
		unsigned int widthOfHeatmapInPixels;
	UPROPERTY(EditAnywhere)
		unsigned int heightOfHeatmapInPixels;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerMovementColour)
		FColor playerMoveCol;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = StaticObjectColour)
		FColor staticObjCol;

	float totTime, prevTime, maxTimeAtPos, lowestTimeAtPos;	//for Heatmap 
	struct PosData {
		float x, y, z, dt;
		PosData(float nx, float ny, float nz, float ndt) { x = nx; y = ny; z = nz; dt = ndt; }
	};
	TArray<PosData> playerPos;
	TArray<FString> playerPositions;
	unsigned int *plyrPosCount, highFreq;
	FString allPlayerPos;
	float maxX, maxY, minX, minY, playerRad, chkSqVal;
	unsigned int w, h;
	struct ActorAndBounds {
		AActor *actor;
		FVector maxBounds, org;
		ActorAndBounds(AActor* a, FVector o, FVector mb) { actor = a; org = o; maxBounds = mb; }
	};
	TArray<ActorAndBounds*> staticActors;
	void SaveTexture2DDebug(const uint8* PPixelData, int width, int height, FString Filename);
	void addStaticBoundsToHeatmap(uint8 *pixels, float gx, float gy);
	void set32BitPixel(uint8 *pixels, int pos, uint8 r, uint8 g, uint8 b, uint8 a);
	
	template <class T>  //so any number data type can be used
	void outputArrayCSVfile(int w, int h, T *pixels, FString filename);

	void updateLastPositionInArrays();
	void updatePositionData(FVector &newLocation);
	void addPlayerPosToTextArray(FVector & newLocation, float diffT);
	unsigned int getGridPos(float rx, float minX, float gx);
	void saveHeatmap();
	void clearArrays(uint8 * pixels);
	bool checkBindingIsSetup(FString axisMapName);
	float calcDiffInTime();
	void analyseTextFilePositions();
	uint8* createHeatMapData();

	struct Extent {
		FVector topL, botR;
		Extent() { topL = botR = FVector::ZeroVector;  }
	};
	Extent maxExtent;
	void getMaxExtent(Extent &currHigh, FVector org, FVector box);
	FVector prevLoc;
	APawn *player;	//Pawn needed as BindAxis used
	AActor *platform;
	bool heatMapProcessed, textFileAnalysed;
	const int BPP = 4; //Bytes per pixel for PNG output
};



