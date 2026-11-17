// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxImporter.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "VoxAssetImportData.h"
#include "VoxMaterialBuilder.h"
#include "VoxMeshBuilder.h"
#include "Voxel.h"
#include "VoxelAnimation.h"
#include "VoxelAnimationComponent.h"
#include "VoxelComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxImporter, Log, All)

#define LOCTEXT_NAMESPACE "VoxImporter"

namespace VoxImporterPrivate
{
	/** A component variable name: letters, digits and underscores, not starting with a digit, unique in the Blueprint. */
	FName MakeComponentName(const FString& Wanted, TSet<FName>& Used)
	{
		FString Name;
		for (const TCHAR Char : Wanted)
		{
			Name.AppendChar(FChar::IsAlnum(Char) || Char == TEXT('_') ? Char : TEXT('_'));
		}
		if (Name.IsEmpty())
		{
			Name = TEXT("Node");
		}
		if (FChar::IsDigit(Name[0]))
		{
			Name = TEXT("N_") + Name;
		}

		FName Unique(*Name);
		for (int32 Suffix = 1; Used.Contains(Unique); ++Suffix)
		{
			Unique = FName(*FString::Printf(TEXT("%s_%d"), *Name, Suffix));
		}
		Used.Add(Unique);
		return Unique;
	}

	USCS_Node* AddComponentNode(USimpleConstructionScript* SCS, USCS_Node* Parent, UClass* Class, const FString& Name, TSet<FName>& Used)
	{
		USCS_Node* Node = SCS->CreateNode(Class, MakeComponentName(Name, Used));
		if (Parent)
		{
			Parent->AddChildNode(Node);
		}
		else
		{
			SCS->AddNode(Node);
		}
		return Node;
	}

	void SetRelativeTransform(USceneComponent* Template, const FTransform& Transform)
	{
		Template->SetRelativeLocation_Direct(Transform.GetLocation());
		Template->SetRelativeRotation_Direct(Transform.Rotator());
		Template->SetRelativeScale3D_Direct(Transform.GetScale3D());
	}

	/** Assets created by a failed build are not kept. */
	void Discard(UObject* Object)
	{
		if (Object)
		{
			Object->ClearFlags(RF_Public | RF_Standalone);
		}
	}
}

FVoxImporter::FVoxImporter(const FVoxImportSettings& InSettings, const FVoxAssetLocation& InLocation, const FString& InSourceFile)
	: Settings(InSettings)
	, Location(InLocation)
	, SourceFile(InSourceFile)
{
}

FVoxImporter::~FVoxImporter() = default;

bool FVoxImporter::LoadScene(const uint8* Data, int64 Size, bool bMeshes, EVoxPivot Pivot)
{
	FVoxLoadParams Params;
	Params.bXForward = Settings.bImportXForward;
	// Voxel assets work on the voxel grid (the scale is applied to the cube meshes instead); a scene of voxel
	// assets keeps the scale for its node transforms.
	Params.Scale = Settings.ImportType == EVoxImportType::Voxel ? 1.0f : Settings.Scale;
	Params.Pivot = Pivot;
	Params.bGenerateMeshes = bMeshes;
	Params.bKeepVoxels = !bMeshes;

	FText LoadError;
	if (!Scene.Load(Data, Size, Params, LoadError))
	{
		Error = FText::Format(LOCTEXT("ReadError", "Could not read {0}: {1}"), FText::FromString(FPaths::GetCleanFilename(SourceFile)), LoadError);
		UE_LOG(LogVoxImporter, Error, TEXT("%s"), *Error.ToString());
		return false;
	}

	Materials = MakeUnique<FVoxMaterialBuilder>(Scene, Settings, Location);
	return true;
}

const v4c_model* FVoxImporter::FindModel(int32 FileIndex) const
{
	if (!Scene.IsValid() || FileIndex < 0)
	{
		return nullptr;
	}
	for (uint32 Index = 0; Index < Scene->num_models; ++Index)
	{
		if (Scene->models[Index].file_index == static_cast<uint32>(FileIndex))
		{
			return &Scene->models[Index];
		}
	}
	return nullptr;
}

FString FVoxImporter::AssetName(EVoxAssetType Type, int32 ModelIndex, int32 Color, const FString& BaseName) const
{
	FVoxNameFormatArgs Args;
	Args.BaseName = BaseName.IsEmpty() ? Location.BaseName : BaseName;
	Args.ModelId = ModelIndex;
	Args.Color = Color;
	return FVoxNameFormatter::Format(Type, Args, Location.Naming);
}

FString FVoxImporter::FrameBaseName(uint32 Frame) const
{
	return FString::Printf(TEXT("%s_F%u"), *Location.BaseName, Frame);
}

bool FVoxImporter::ShouldImportAnimation() const
{
	return Settings.bImportAnimation && Scene.HasAnimation();
}

template <class TAsset>
void FVoxImporter::UpdateImportData(TAsset* Asset, int32 ModelIndex, int32 AnimationFrame, const FVoxImportSettings& AssetSettings) const
{
	UVoxAssetImportData* ImportData = Cast<UVoxAssetImportData>(Asset->GetAssetImportData());
	if (!ImportData)
	{
		ImportData = NewObject<UVoxAssetImportData>(Asset, NAME_None, RF_NoFlags);
		Asset->SetAssetImportData(ImportData);
	}
	ImportData->FromSettings(AssetSettings, Location.BaseName, ModelIndex, AnimationFrame);
	ImportData->Update(SourceFile);
}

TArray<UObject*> FVoxImporter::Import(const uint8* Data, int64 Size)
{
	FScopedSlowTask SlowTask(10.0f, FText::Format(LOCTEXT("ImportingFile", "Importing {0}"), FText::FromString(FPaths::GetCleanFilename(SourceFile))));
	SlowTask.MakeDialogDelayed(0.5f);

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("ReadingFile", "Reading the .vox file"));
	// Voxel assets work on the voxel grid; scene nodes place models whose origin is the MagicaVoxel pivot.
	const bool bSceneVoxels = Settings.ImportType == EVoxImportType::Scene && Settings.SceneModels == EVoxSceneModels::Voxel;
	const bool bMeshes = Settings.ImportType != EVoxImportType::Voxel && !bSceneVoxels;
	const EVoxPivot Pivot = Settings.ImportType == EVoxImportType::StaticMesh ? Settings.Pivot : EVoxPivot::MagicaVoxel;
	if (Settings.ImportType == EVoxImportType::Voxel || bSceneVoxels)
	{
		UseColourMaterials();
	}
	if (!LoadScene(Data, Size, bMeshes, Pivot))
	{
		return TArray<UObject*>();
	}

	SlowTask.EnterProgressFrame(9.0f);
	switch (Settings.ImportType)
	{
	case EVoxImportType::Voxel:
		return ImportVoxels();
	case EVoxImportType::Scene:
		return ImportScene();
	case EVoxImportType::StaticMesh:
	default:
		return !Settings.bSeparateModels && ShouldImportAnimation() ? ImportAnimationFrames() : ImportStaticMeshes();
	}
}

bool FVoxImporter::Reimport(UObject* Asset, int32 ModelIndex, int32 AnimationFrame, const uint8* Data, int64 Size)
{
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		if (!LoadScene(Data, Size, true, Settings.Pivot))
		{
			return false;
		}
		StaticMesh->Modify();
		if (!BuildStaticMesh(StaticMesh, ModelIndex, AnimationFrame))
		{
			return false;
		}
		UpdateImportData(StaticMesh, ModelIndex, AnimationFrame, Settings);
		return true;
	}

	if (UVoxel* Voxel = Cast<UVoxel>(Asset))
	{
		// Voxel assets are always stored as such (also the models of a scene), whatever older data says.
		Settings.ImportType = EVoxImportType::Voxel;
		UseColourMaterials();
		if (!LoadScene(Data, Size, false, EVoxPivot::MagicaVoxel))
		{
			return false;
		}
		// A Scale changed on the asset after the import survives the reimport.
		const float AssetScale = Voxel->Scale;
		if (!BuildVoxel(Voxel, ModelIndex))
		{
			return false;
		}
		if (AssetScale > 0.f)
		{
			Voxel->Scale = AssetScale;
			Voxel->RefreshComponents();
		}
		UpdateImportData(Voxel, ModelIndex, INDEX_NONE, Settings);
		return true;
	}

	Error = FText::Format(LOCTEXT("CannotReimport", "{0} cannot be reimported from a .vox file."), FText::FromString(Asset->GetName()));
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Static meshes
// ---------------------------------------------------------------------------------------------------------------------

bool FVoxImporter::BuildStaticMesh(UStaticMesh* StaticMesh, int32 ModelIndex, int32 AnimationFrame)
{
	FVoxMeshData MeshData;
	bool bBuilt = false;
	if (ModelIndex >= 0)
	{
		// One model: vox4c already placed the pivot.
		const v4c_model* Model = FindModel(ModelIndex);
		if (!Model)
		{
			Error = FText::Format(LOCTEXT("NoModel", "{0} has no model {1}."), FText::FromString(FPaths::GetCleanFilename(SourceFile)), FText::AsNumber(ModelIndex));
			return false;
		}
		bBuilt = VoxMeshBuilder::BuildMesh(Scene, Model->mesh, FVector3f::ZeroVector, MeshData);
	}
	else
	{
		// Every model of a frame, baked by vox4c as placed in the scene.
		const uint32 Frame = Scene.ClampFrame(AnimationFrame >= 0 ? AnimationFrame : Settings.Frame);
		FVoxBakedMesh Baked;
		if (Scene.BakeFrame(Frame, Settings.bImportHidden, Baked))
		{
			// The frames of an animation share one origin, a single mesh is centred on itself.
			const FVector3f Offset = AnimationFrame >= 0
				? GetAnimationPivotOffset()
				: VoxMeshBuilder::GetPivotOffset(VoxMeshBuilder::GetBounds(Baked.Mesh), Settings.Pivot);
			bBuilt = VoxMeshBuilder::BuildMesh(Scene, Baked.Mesh, Offset, MeshData);
		}
	}

	if (!bBuilt)
	{
		Error = FText::Format(LOCTEXT("NothingToBuild", "{0}: nothing to build for {1} (the models are empty or hidden)."), FText::FromString(FPaths::GetCleanFilename(SourceFile)), FText::FromString(StaticMesh->GetName()));
		return false;
	}

	VoxMeshBuilder::CommitStaticMesh(StaticMesh, MeshData, Scene,
		[this](uint32 Material) { return Materials->GetMaterial(Material); },
		Settings.bGenerateLightmapUVs);
	return true;
}

TArray<UObject*> FVoxImporter::ImportStaticMeshes()
{
	TArray<int32> ModelIndices;
	if (Settings.bSeparateModels)
	{
		for (uint32 Index = 0; Index < Scene->num_models; ++Index)
		{
			if (Scene->models[Index].mesh.num_indices > 0)
			{
				ModelIndices.Add(static_cast<int32>(Scene->models[Index].file_index));
			}
		}
	}
	else
	{
		ModelIndices.Add(INDEX_NONE);
	}

	FScopedSlowTask SlowTask(static_cast<float>(ModelIndices.Num()));

	TArray<UObject*> Results;
	for (const int32 ModelIndex : ModelIndices)
	{
		const FString Name = AssetName(EVoxAssetType::StaticMesh, ModelIndex);
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("BuildingAsset", "Building {0}"), FText::FromString(Name)));

		bool bCreated = false;
		UStaticMesh* StaticMesh = VoxAssets::FindOrCreate<UStaticMesh>(Location.PackagePath, Name, Location.Flags, bCreated);
		if (!StaticMesh)
		{
			continue;
		}

		if (!BuildStaticMesh(StaticMesh, ModelIndex))
		{
			UE_LOG(LogVoxImporter, Error, TEXT("%s"), *Error.ToString());
			if (bCreated)
			{
				VoxImporterPrivate::Discard(StaticMesh);
			}
			continue;
		}

		UpdateImportData(StaticMesh, ModelIndex, INDEX_NONE, Settings);
		VoxAssets::Finish(StaticMesh, bCreated);
		Results.Add(StaticMesh);
	}
	return Results;
}

// ---------------------------------------------------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------------------------------------------------

void FVoxImporter::GetDistinctFrames(TArray<uint32>& OutFrames, TMap<uint32, uint32>& OutFrameOf) const
{
	OutFrames.Reset();
	OutFrameOf.Reset();

	// A frame is identified by the models drawn in it and their matrices.
	TArray<TArray<float>> Signatures;
	TArray<v4c_instance> Instances;
	for (uint32 Frame = Scene.GetAnimationFirstFrame(); Frame <= Scene.GetAnimationLastFrame(); ++Frame)
	{
		Scene.GetFrameInstances(Frame, Settings.bImportHidden, Instances);
		TArray<float> Signature;
		Signature.Reserve(Instances.Num() * 17);
		for (const v4c_instance& Instance : Instances)
		{
			Signature.Add(static_cast<float>(Instance.model));
			Signature.Append(Instance.matrix, 16);
		}

		const int32 Existing = Signatures.IndexOfByKey(Signature);
		if (Existing != INDEX_NONE)
		{
			OutFrameOf.Add(Frame, OutFrames[Existing]);
			continue;
		}
		Signatures.Add(MoveTemp(Signature));
		OutFrames.Add(Frame);
		OutFrameOf.Add(Frame, Frame);
	}
}

FVector3f FVoxImporter::GetAnimationPivotOffset()
{
	if (!bAnimationPivotValid)
	{
		// Bounds of everything drawn during the animation.
		FBox3f Bounds(ForceInit);
		TArray<uint32> Frames;
		TMap<uint32, uint32> FrameOf;
		GetDistinctFrames(Frames, FrameOf);
		for (const uint32 Frame : Frames)
		{
			FVoxBakedMesh Baked;
			if (Scene.BakeFrame(Frame, Settings.bImportHidden, Baked))
			{
				Bounds += VoxMeshBuilder::GetBounds(Baked.Mesh);
			}
		}
		AnimationPivotOffset = VoxMeshBuilder::GetPivotOffset(Bounds, Settings.Pivot);
		bAnimationPivotValid = true;
	}
	return AnimationPivotOffset;
}

UVoxelAnimation* FVoxImporter::WriteAnimation(TArray<UObject*> Models, TArray<FVoxelAnimationTrack> Tracks, bool bInterpolate)
{
	bool bCreated = false;
	UVoxelAnimation* Animation = VoxAssets::FindOrCreate<UVoxelAnimation>(Location.PackagePath,
		AssetName(EVoxAssetType::Animation, INDEX_NONE), Location.Flags, bCreated);
	if (!Animation)
	{
		return nullptr;
	}

	Animation->FirstFrame = static_cast<int32>(Scene.GetAnimationFirstFrame());
	Animation->LastFrame = static_cast<int32>(Scene.GetAnimationLastFrame());
	if (bCreated)
	{
		// An existing asset keeps the speed it was given.
		Animation->FramesPerSecond = Settings.AnimationFramesPerSecond;
	}
	Animation->bInterpolate = bInterpolate;
	Animation->Models.Reset();
	for (UObject* Model : Models)
	{
		Animation->Models.Add(Model);
	}
	Animation->Tracks = MoveTemp(Tracks);
	Animation->PostEditChange();

	VoxAssets::Finish(Animation, bCreated);
	return Animation;
}

TArray<UObject*> FVoxImporter::ImportAnimationFrames()
{
	// One static mesh per distinct frame of the animation range, all with the same origin, and an
	// animation that switches between them.
	TArray<uint32> Frames;
	TMap<uint32, uint32> FrameOf;
	GetDistinctFrames(Frames, FrameOf);

	FScopedSlowTask SlowTask(static_cast<float>(Frames.Num() + 1));

	TArray<UObject*> Results;
	TMap<uint32, int32> MeshOfFrame;
	TArray<UObject*> Meshes;
	for (const uint32 Frame : Frames)
	{
		const FString Name = AssetName(EVoxAssetType::StaticMesh, INDEX_NONE, INDEX_NONE, FrameBaseName(Frame));
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("BuildingAsset", "Building {0}"), FText::FromString(Name)));

		bool bCreated = false;
		UStaticMesh* StaticMesh = VoxAssets::FindOrCreate<UStaticMesh>(Location.PackagePath, Name, Location.Flags, bCreated);
		if (!StaticMesh)
		{
			continue;
		}
		if (!BuildStaticMesh(StaticMesh, INDEX_NONE, static_cast<int32>(Frame)))
		{
			// A frame where nothing is drawn shows no mesh.
			if (bCreated)
			{
				VoxImporterPrivate::Discard(StaticMesh);
			}
			continue;
		}

		UpdateImportData(StaticMesh, INDEX_NONE, static_cast<int32>(Frame), Settings);
		VoxAssets::Finish(StaticMesh, bCreated);
		MeshOfFrame.Add(Frame, Meshes.Add(StaticMesh));
		Results.Add(StaticMesh);
	}

	SlowTask.EnterProgressFrame(1.0f);
	if (Meshes.Num() > 0)
	{
		FVoxelAnimationTrack Track; // ComponentName None: the root / first static mesh component
		for (uint32 Frame = Scene.GetAnimationFirstFrame(); Frame <= Scene.GetAnimationLastFrame(); ++Frame)
		{
			const int32* MeshIndex = MeshOfFrame.Find(FrameOf.FindRef(Frame));
			Track.ModelIndices.Add(MeshIndex ? *MeshIndex : INDEX_NONE);
		}

		TArray<FVoxelAnimationTrack> Tracks;
		Tracks.Add(MoveTemp(Track));
		if (UVoxelAnimation* Animation = WriteAnimation(Meshes, MoveTemp(Tracks), false))
		{
			Results.Add(Animation);
		}
	}
	return Results;
}

// ---------------------------------------------------------------------------------------------------------------------
// Voxel assets
// ---------------------------------------------------------------------------------------------------------------------

void FVoxImporter::UseColourMaterials()
{
	// One Material and Palette To Texture tell the colours apart by the UVs of the cubes; all voxels share one
	// cube, so every colour gets its own material instance (Color parameter) instead.
	Settings.bOneMaterial = false;
	Settings.bPaletteToTexture = false;
}

UStaticMesh* FVoxImporter::BuildVoxelCube(uint32 Material)
{
	if (VoxelCube)
	{
		return VoxelCube;
	}

	bool bCreated = false;
	UStaticMesh* Cube = VoxAssets::FindOrCreate<UStaticMesh>(Location.ResourcesPath,
		AssetName(EVoxAssetType::StaticMesh, INDEX_NONE, INDEX_NONE, Location.BaseName + TEXT("_Cube")), Location.Flags, bCreated);
	if (!Cube)
	{
		return nullptr;
	}

	FVoxMeshData MeshData;
	VoxMeshBuilder::BuildCube(Scene, Material, Settings.Scale, MeshData);
	VoxMeshBuilder::CommitStaticMesh(Cube, MeshData, Scene,
		[this](uint32 CubeMaterial) { return Materials->GetMaterial(CubeMaterial); },
		false);

	// Simple box collision of one voxel.
	Cube->CreateBodySetup();
	if (UBodySetup* BodySetup = Cube->GetBodySetup())
	{
		BodySetup->Modify();
		BodySetup->RemoveSimpleCollision();
		BodySetup->AggGeom.BoxElems.Add(FKBoxElem(Settings.Scale));
		BodySetup->InvalidatePhysicsData();
		BodySetup->CreatePhysicsMeshes();
	}

	VoxAssets::Finish(Cube, bCreated);
	VoxelCube = Cube;
	return Cube;
}

bool FVoxImporter::BuildVoxel(UVoxel* Voxel, int32 ModelIndex)
{
	// Cell -> material
	TMap<FIntVector, uint32> Cells;
	FIntVector Size = FIntVector::ZeroValue;
	FIntVector MergedOrigin = FIntVector::ZeroValue; // scene cell of grid cell 0 (merged models)

	if (ModelIndex >= 0)
	{
		const v4c_model* Model = FindModel(ModelIndex);
		if (!Model || !Model->voxels)
		{
			Error = FText::Format(LOCTEXT("NoModel", "{0} has no model {1}."), FText::FromString(FPaths::GetCleanFilename(SourceFile)), FText::AsNumber(ModelIndex));
			return false;
		}
		Cells.Reserve(Model->num_voxels);
		for (uint32 Index = 0; Index < Model->num_voxels; ++Index)
		{
			const v4c_voxel& V = Model->voxels[Index];
			Cells.Add(FIntVector(V.x, V.y, V.z), V.material);
		}
		Size = FIntVector(Model->size[0], Model->size[1], Model->size[2]);
	}
	else
	{
		// Models placed as in the scene: rotations are signed permutations and translations whole voxels,
		// so every voxel centre lands in the middle of a cell of the merged grid.
		TArray<v4c_instance> Instances;
		Scene.GetFrameInstances(Scene.ClampFrame(Settings.Frame), Settings.bImportHidden, Instances);

		FIntVector Min(TNumericLimits<int32>::Max());
		FIntVector Max(TNumericLimits<int32>::Lowest());
		TMap<FIntVector, uint32> WorldCells;
		for (const v4c_instance& Instance : Instances)
		{
			const v4c_model& Model = Scene->models[Instance.model];
			const float* M = Instance.matrix;
			for (uint32 Index = 0; Index < Model.num_voxels; ++Index)
			{
				const v4c_voxel& V = Model.voxels[Index];
				const float P[3] = { V.x + 0.5f - Model.pivot[0], V.y + 0.5f - Model.pivot[1], V.z + 0.5f - Model.pivot[2] };
				const FIntVector Cell(
					FMath::FloorToInt(M[0] * P[0] + M[4] * P[1] + M[8] * P[2] + M[12]),
					FMath::FloorToInt(M[1] * P[0] + M[5] * P[1] + M[9] * P[2] + M[13]),
					FMath::FloorToInt(M[2] * P[0] + M[6] * P[1] + M[10] * P[2] + M[14]));
				WorldCells.Add(Cell, V.material);
				Min = FIntVector(FMath::Min(Min.X, Cell.X), FMath::Min(Min.Y, Cell.Y), FMath::Min(Min.Z, Cell.Z));
				Max = FIntVector(FMath::Max(Max.X, Cell.X), FMath::Max(Max.Y, Cell.Y), FMath::Max(Max.Z, Cell.Z));
			}
		}

		Cells.Reserve(WorldCells.Num());
		for (const TPair<FIntVector, uint32>& Cell : WorldCells)
		{
			Cells.Add(Cell.Key - Min, Cell.Value);
		}
		if (WorldCells.Num() > 0)
		{
			Size = Max - Min + FIntVector(1, 1, 1);
			MergedOrigin = Min;
		}
	}

	if (Cells.Num() == 0)
	{
		Error = FText::Format(LOCTEXT("NoVoxels", "{0}: no voxels for {1} (the models are empty or hidden)."), FText::FromString(FPaths::GetCleanFilename(SourceFile)), FText::FromString(Voxel->GetName()));
		return false;
	}

	// One cube mesh per material, in material order.
	TArray<uint32> UsedMaterials;
	for (const TPair<FIntVector, uint32>& Cell : Cells)
	{
		UsedMaterials.AddUnique(Cell.Value);
	}
	UsedMaterials.Sort();

	// One cube for all voxels, one material instance per colour (edits made in the Voxel Editor are replaced).
	Voxel->Modify();
	Voxel->Meshes.Reset(); // per-colour cubes of assets imported before 0.6.0
	Voxel->Materials.Reset();
	Voxel->Mesh = BuildVoxelCube(UsedMaterials[0]);
	TMap<uint32, uint8> MeshIndices;
	for (const uint32 Material : UsedMaterials)
	{
		// At most 255 materials are used (palette index 0 is "empty"), so the index fits in a byte.
		MeshIndices.Add(Material, static_cast<uint8>(Voxel->Materials.Num()));
		Voxel->Materials.Add(Materials->GetMaterial(Material));
	}

	Voxel->Voxels.Reset();
	Voxel->Voxels.Reserve(Cells.Num());
	for (const TPair<FIntVector, uint32>& Cell : Cells)
	{
		Voxel->Voxels.Add(Cell.Key, MeshIndices[Cell.Value]);
	}

	// Origin of the voxel asset, in voxels from the grid corner (as for static meshes).
	FVector Pivot = FVector::ZeroVector;
	switch (Settings.Pivot)
	{
		case EVoxPivot::MagicaVoxel:
			if (const v4c_model* Model = ModelIndex >= 0 ? FindModel(ModelIndex) : nullptr)
			{
				Pivot = FVoxScene::ToVector(Model->pivot); // floor(size / 2), what scene transforms expect
			}
			else
			{
				Pivot = -FVector(MergedOrigin); // origin of the MagicaVoxel scene
			}
			break;
		case EVoxPivot::Center:
			Pivot = FVector(Size) * 0.5;
			break;
		case EVoxPivot::BottomCenter:
			Pivot = FVector(Size.X * 0.5, Size.Y * 0.5, 0.0);
			break;
		case EVoxPivot::Corner:
		default:
			break;
	}

	Voxel->Size = Size;
	Voxel->bUsePivot = true;
	Voxel->Pivot = Pivot;
	Voxel->Scale = Settings.Scale; // the cubes are built with this size
	Voxel->bXYCenter = Settings.Pivot != EVoxPivot::Corner;
	Voxel->CalcCellBounds();
	Voxel->MarkPackageDirty();
	Voxel->RefreshComponents(); // actors in levels show the reimported voxels
	return true;
}

TArray<UObject*> FVoxImporter::ImportVoxels()
{
	TArray<int32> ModelIndices;
	if (Settings.bSeparateModels)
	{
		for (uint32 Index = 0; Index < Scene->num_models; ++Index)
		{
			if (Scene->models[Index].num_voxels > 0)
			{
				ModelIndices.Add(static_cast<int32>(Scene->models[Index].file_index));
			}
		}
	}
	else
	{
		ModelIndices.Add(INDEX_NONE);
	}

	FScopedSlowTask SlowTask(static_cast<float>(ModelIndices.Num()));

	TArray<UObject*> Results;
	for (const int32 ModelIndex : ModelIndices)
	{
		const FString Name = AssetName(EVoxAssetType::Voxel, ModelIndex);
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("BuildingAsset", "Building {0}"), FText::FromString(Name)));

		bool bCreated = false;
		UVoxel* Voxel = VoxAssets::FindOrCreate<UVoxel>(Location.PackagePath, Name, Location.Flags, bCreated);
		if (!Voxel)
		{
			continue;
		}

		if (!BuildVoxel(Voxel, ModelIndex))
		{
			UE_LOG(LogVoxImporter, Error, TEXT("%s"), *Error.ToString());
			if (bCreated)
			{
				VoxImporterPrivate::Discard(Voxel);
			}
			continue;
		}

		UpdateImportData(Voxel, ModelIndex, INDEX_NONE, Settings);
		VoxAssets::Finish(Voxel, bCreated);
		Results.Add(Voxel);
	}
	return Results;
}

// ---------------------------------------------------------------------------------------------------------------------
// Scene (Blueprint)
// ---------------------------------------------------------------------------------------------------------------------

TArray<UObject*> FVoxImporter::ImportScene()
{
	const uint32 Frame = Scene.ClampFrame(Settings.Frame);

	// One static mesh or voxel asset per model shown by an imported node, with the MagicaVoxel pivot so that
	// the node transforms place it; it is reimported like a separate model.
	const bool bVoxels = Settings.SceneModels == EVoxSceneModels::Voxel;
	FVoxImportSettings ModelSettings = Settings;
	ModelSettings.ImportType = bVoxels ? EVoxImportType::Voxel : EVoxImportType::StaticMesh;
	ModelSettings.Pivot = EVoxPivot::MagicaVoxel;
	ModelSettings.bSeparateModels = true;
	TGuardValue<EVoxPivot> PivotGuard(Settings.Pivot, EVoxPivot::MagicaVoxel);

	// Models shown by the imported nodes: at Frame, and with an animation in every frame of it.
	TArray<uint32> UsedModels;
	{
		TArray<uint32> Nodes;
		Scene.GetModelNodes(Frame, Settings.bImportHidden, Nodes);
		for (const uint32 Node : Nodes)
		{
			UsedModels.AddUnique(static_cast<uint32>(v4c_sample_node_model(Scene.Get(), Node, Frame)));
		}
		if (ShouldImportAnimation())
		{
			for (uint32 AnimFrame = Scene.GetAnimationFirstFrame(); AnimFrame <= Scene.GetAnimationLastFrame(); ++AnimFrame)
			{
				Scene.GetModelNodes(AnimFrame, Settings.bImportHidden, Nodes);
				for (const uint32 Node : Nodes)
				{
					UsedModels.AddUnique(static_cast<uint32>(v4c_sample_node_model(Scene.Get(), Node, AnimFrame)));
				}
			}
		}
		UsedModels.Sort();
	}

	FScopedSlowTask SlowTask(static_cast<float>(UsedModels.Num() + 1));

	TArray<UObject*> ModelAssets;
	TMap<int32, UObject*> Models;
	for (const uint32 ModelSlot : UsedModels)
	{
		SlowTask.EnterProgressFrame(1.0f);
		const v4c_model& Model = Scene->models[ModelSlot];
		const int32 ModelIndex = static_cast<int32>(Model.file_index);
		if (Models.Contains(ModelIndex) || Model.num_voxels == 0)
		{
			continue;
		}

		const FString ModelName = AssetName(bVoxels ? EVoxAssetType::Voxel : EVoxAssetType::StaticMesh, ModelIndex);
		SlowTask.FrameMessage = FText::Format(LOCTEXT("BuildingAsset", "Building {0}"), FText::FromString(ModelName));

		bool bCreated = false;
		UObject* Asset = nullptr;
		bool bBuilt = false;
		if (bVoxels)
		{
			UVoxel* Voxel = VoxAssets::FindOrCreate<UVoxel>(Location.PackagePath, ModelName, Location.Flags, bCreated);
			bBuilt = Voxel && BuildVoxel(Voxel, ModelIndex);
			if (bBuilt)
			{
				UpdateImportData(Voxel, ModelIndex, INDEX_NONE, ModelSettings);
			}
			Asset = Voxel;
		}
		else
		{
			UStaticMesh* StaticMesh = VoxAssets::FindOrCreate<UStaticMesh>(Location.PackagePath, ModelName, Location.Flags, bCreated);
			bBuilt = StaticMesh && BuildStaticMesh(StaticMesh, ModelIndex);
			if (bBuilt)
			{
				UpdateImportData(StaticMesh, ModelIndex, INDEX_NONE, ModelSettings);
			}
			Asset = StaticMesh;
		}

		if (!bBuilt)
		{
			if (Asset)
			{
				UE_LOG(LogVoxImporter, Error, TEXT("%s"), *Error.ToString());
				if (bCreated)
				{
					VoxImporterPrivate::Discard(Asset);
				}
			}
			continue;
		}

		VoxAssets::Finish(Asset, bCreated);
		Models.Add(ModelIndex, Asset);
		ModelAssets.Add(Asset);
	}

	// The Blueprint
	const FString Name = AssetName(EVoxAssetType::Blueprint, INDEX_NONE);
	SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("BuildingAsset", "Building {0}"), FText::FromString(Name)));
	const FString PackageName = Location.PackagePath / Name;
	const FString ObjectPath = PackageName + TEXT(".") + Name;

	UBlueprint* Blueprint = nullptr;
	bool bCreated = false;
	if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Blueprint = Cast<UBlueprint>(Existing);
		if (!Blueprint || !Blueprint->SimpleConstructionScript || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(AActor::StaticClass()))
		{
			UE_LOG(LogVoxImporter, Error, TEXT("%s already exists and is not an actor Blueprint, the scene was not created."), *ObjectPath);
			Blueprint = nullptr;
		}
		else
		{
			Blueprint->Modify();
		}
	}
	else
	{
		UPackage* Package = CreatePackage(*PackageName);
		Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, FName(*Name), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		bCreated = Blueprint != nullptr;
	}

	TArray<UObject*> Results;
	if (Blueprint)
	{
		UVoxelAnimation* Animation = BuildBlueprint(Blueprint, Models, Frame);
		VoxAssets::Finish(Blueprint, bCreated);
		Results.Add(Blueprint);
		if (Animation)
		{
			Results.Add(Animation);
		}
	}
	Results.Append(ModelAssets);
	return Results;
}

UVoxelAnimation* FVoxImporter::BuildBlueprint(UBlueprint* Blueprint, const TMap<int32, UObject*>& Models, uint32 Frame)
{
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

	// Animation: every node is sampled in every frame of the range, the ones that change get a track.
	const bool bAnimate = ShouldImportAnimation();
	const v4c_sampling Sampling = Settings.AnimationSampling == EVoxAnimationSampling::Smooth ? V4C_SAMPLE_SMOOTH : V4C_SAMPLE_MAGICAVOXEL;
	const uint32 FirstFrame = Scene.GetAnimationFirstFrame();
	const uint32 LastFrame = Scene.GetAnimationLastFrame();
	const bool bVoxels = Settings.SceneModels == EVoxSceneModels::Voxel;
	TArray<UObject*> AnimationModels;
	TMap<int32, int32> AnimationModelOfFileModel;
	for (const TPair<int32, UObject*>& Entry : Models)
	{
		AnimationModelOfFileModel.Add(Entry.Key, AnimationModels.Add(Entry.Value));
	}
	TArray<FVoxelAnimationTrack> Tracks;

	auto AssetOfSlot = [&](int32 ModelSlot) -> UObject*
	{
		UObject* const* Found = ModelSlot >= 0 ? Models.Find(static_cast<int32>(Scene->models[ModelSlot].file_index)) : nullptr;
		return Found ? *Found : nullptr;
	};

	// Rebuild from scratch: remove the components of a previous import (children were added after their parents).
	auto ExistingNodes = SCS->GetAllNodes();
	for (int32 Index = ExistingNodes.Num() - 1; Index >= 0; --Index)
	{
		if (USCS_Node* Node = ExistingNodes[Index])
		{
			SCS->RemoveNode(Node);
		}
	}

	TSet<FName> UsedNames;
	USCS_Node* RootNode = VoxImporterPrivate::AddComponentNode(SCS, nullptr, USceneComponent::StaticClass(), TEXT("Root"), UsedNames);

	// MagicaVoxel nodes, in pre-order: a parent always comes before its children.
	TArray<USCS_Node*> NodeMap;
	NodeMap.SetNumZeroed(Scene->num_nodes);
	for (uint32 NodeIndex = 0; NodeIndex < Scene->num_nodes; ++NodeIndex)
	{
		const v4c_node& Node = Scene->nodes[NodeIndex];
		const bool bVisible = v4c_node_is_visible(Scene.Get(), NodeIndex);
		if (!bVisible && !Settings.bImportHidden)
		{
			continue;
		}

		USCS_Node* Parent = RootNode;
		if (Node.parent >= 0)
		{
			Parent = NodeMap[Node.parent];
			if (!Parent)
			{
				continue; // the parent was skipped
			}
		}

		const int32 ModelSlot = v4c_sample_node_model(Scene.Get(), NodeIndex, Frame);
		UObject* ModelAsset = AssetOfSlot(ModelSlot);

		// Track of this node over the animation range.
		FVoxelAnimationTrack Track;
		bool bHasModel = ModelAsset != nullptr;
		if (bAnimate)
		{
			bool bTransformChanges = false;
			bool bModelChanges = false;
			for (uint32 AnimFrame = FirstFrame; AnimFrame <= LastFrame; ++AnimFrame)
			{
				const FTransform Transform = FVoxScene::ToTransform(v4c_sample_node_transform_at(Scene.Get(), NodeIndex, static_cast<float>(AnimFrame), Sampling));
				const int32 AnimSlot = v4c_sample_node_model(Scene.Get(), NodeIndex, AnimFrame);
				const int32* AnimModel = AnimSlot >= 0 ? AnimationModelOfFileModel.Find(static_cast<int32>(Scene->models[AnimSlot].file_index)) : nullptr;
				Track.Transforms.Add(Transform);
				Track.ModelIndices.Add(AnimModel ? *AnimModel : INDEX_NONE);
				bTransformChanges |= !Transform.Equals(Track.Transforms[0], 1.e-4f);
				bModelChanges |= Track.ModelIndices.Last() != Track.ModelIndices[0];
				bHasModel |= AnimModel != nullptr;
			}
			if (!bTransformChanges)
			{
				Track.Transforms.Reset();
			}
			if (!bModelChanges)
			{
				Track.ModelIndices.Reset();
			}
		}

		FString Name;
		if (Node.name && Node.name[0])
		{
			Name = UTF8_TO_TCHAR(Node.name);
		}
		else if (ModelSlot >= 0)
		{
			Name = FString::Printf(TEXT("Model_%u"), Scene->models[ModelSlot].file_index);
		}
		else
		{
			Name = NodeIndex == 0 ? FString(TEXT("Scene")) : FString::Printf(TEXT("Group_%u"), NodeIndex);
		}

		UClass* ComponentClass = USceneComponent::StaticClass();
		if (bHasModel)
		{
			ComponentClass = bVoxels ? UVoxelComponent::StaticClass() : UStaticMeshComponent::StaticClass();
		}
		USCS_Node* ComponentNode = VoxImporterPrivate::AddComponentNode(SCS, Parent, ComponentClass, Name, UsedNames);
		NodeMap[NodeIndex] = ComponentNode;
		if (Track.Transforms.Num() > 0 || Track.ModelIndices.Num() > 0)
		{
			Track.ComponentName = ComponentNode->GetVariableName();
			Tracks.Add(MoveTemp(Track));
		}

		USceneComponent* Template = Cast<USceneComponent>(ComponentNode->ComponentTemplate);
		if (!Template)
		{
			continue;
		}
		VoxImporterPrivate::SetRelativeTransform(Template, FVoxScene::ToTransform(
			v4c_sample_node_transform_at(Scene.Get(), NodeIndex, static_cast<float>(Frame), bAnimate ? Sampling : V4C_SAMPLE_MAGICAVOXEL)));

		if (UStaticMeshComponent* MeshTemplate = Cast<UStaticMeshComponent>(Template))
		{
			MeshTemplate->SetStaticMesh(Cast<UStaticMesh>(ModelAsset));
		}
		else if (UVoxelComponent* VoxelTemplate = Cast<UVoxelComponent>(Template))
		{
			VoxelTemplate->SetVoxel(Cast<UVoxel>(ModelAsset)); // a template only stores the asset
		}
		if (!bVisible)
		{
			Template->SetVisibility(false);
			Template->SetHiddenInGame(true);
		}
	}

	// The sun
	const v4c_sun& Sun = Scene->render.sun;
	if (Settings.bImportSun && Sun.present)
	{
		USCS_Node* SunNode = VoxImporterPrivate::AddComponentNode(SCS, RootNode, UDirectionalLightComponent::StaticClass(), TEXT("Sun"), UsedNames);
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(SunNode->ComponentTemplate))
		{
			// A directional light shines along its +X axis; vox4c gives the direction the light travels.
			const FVector Direction = FVoxScene::ToVector(Sun.direction);
			Light->SetRelativeRotation_Direct(FRotationMatrix::MakeFromX(Direction).Rotator());
			Light->Intensity = Sun.intensity * Settings.LightIntensityScale;
			Light->LightColor = FColor(Sun.color.r, Sun.color.g, Sun.color.b);
			if (Sun.type == V4C_LIGHT_AREA)
			{
				Light->LightSourceAngle = Sun.angular_diameter; // soft shadows of a sun with a size
			}
		}
	}

	// Camera slots a view was saved into
	if (Settings.bImportCameras)
	{
		constexpr float AspectRatio = 16.0f / 9.0f;
		for (uint32 CameraIndex = 0; CameraIndex < Scene->num_cameras; ++CameraIndex)
		{
			const v4c_camera& Camera = Scene->cameras[CameraIndex];
			if (!Camera.saved)
			{
				continue;
			}

			float Position[3], Forward[3], Up[3];
			v4c_camera_pose(Scene.Get(), &Camera, Position, Forward, Up);

			USCS_Node* CameraNode = VoxImporterPrivate::AddComponentNode(SCS, RootNode, UCameraComponent::StaticClass(),
				FString::Printf(TEXT("Camera_%u"), Camera.id), UsedNames);
			UCameraComponent* Component = Cast<UCameraComponent>(CameraNode->ComponentTemplate);
			if (!Component)
			{
				continue;
			}

			Component->SetRelativeLocation_Direct(FVoxScene::ToVector(Position));
			Component->SetRelativeRotation_Direct(FRotationMatrix::MakeFromXZ(FVoxScene::ToVector(Forward), FVoxScene::ToVector(Up)).Rotator());

			// MagicaVoxel stores the vertical field of view, Unreal uses the horizontal one.
			const float VerticalFov = Scene->render.has_lens ? Scene->render.fov : Camera.fov;
			const float HorizontalFov = FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalFov) * 0.5f) * AspectRatio));
			Component->SetAspectRatio(AspectRatio);
			Component->SetFieldOfView(HorizontalFov);

			if (Camera.mode == V4C_CAMERA_ORTHOGRAPHIC || Camera.mode == V4C_CAMERA_ISOMETRIC)
			{
				Component->SetProjectionMode(ECameraProjectionMode::Orthographic);
				Component->SetOrthoWidth(2.0f * Camera.frustum * AspectRatio);
			}
		}
	}

	// The component that plays the animation.
	UVoxelAnimation* Animation = nullptr;
	if (Tracks.Num() > 0)
	{
		Animation = WriteAnimation(AnimationModels, MoveTemp(Tracks), Settings.AnimationSampling == EVoxAnimationSampling::Smooth);
	}
	if (Animation)
	{
		// Animated components are moved and get new meshes while playing.
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (USceneComponent* Template = Node ? Cast<USceneComponent>(Node->ComponentTemplate) : nullptr)
			{
				Template->Mobility = EComponentMobility::Movable;
			}
		}

		USCS_Node* AnimationNode = VoxImporterPrivate::AddComponentNode(SCS, nullptr, UVoxelAnimationComponent::StaticClass(), TEXT("VoxelAnimation"), UsedNames);
		if (UVoxelAnimationComponent* Player = Cast<UVoxelAnimationComponent>(AnimationNode->ComponentTemplate))
		{
			Player->Animation = Animation;
			Player->Frame = static_cast<float>(FMath::Clamp(Frame, FirstFrame, LastFrame));
		}
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Animation;
}

#undef LOCTEXT_NAMESPACE
