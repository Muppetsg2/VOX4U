// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#include "VoxelFactory.h"
#include <Editor.h>
#include <AssetRegistry/AssetRegistryModule.h>
#include <EditorFramework/AssetImportData.h>
//#include <Engine/SkeletalMesh.h>
#include <Engine/StaticMesh.h>
#include <Engine/Texture2D.h>
#include <HAL/FileManager.h>
#include <Materials/Material.h>
#include <Materials/MaterialExpressionTextureSample.h>
#include <Materials/MaterialExpressionVectorParameter.h>
#include <Materials/MaterialExpressionScalarParameter.h>
#include <Materials/MaterialExpressionMultiply.h>
#include <Materials/MaterialInstanceConstant.h>
#include <MaterialEditingLibrary.h>
#include <PhysicsEngine/BodySetup.h>
#include <PhysicsEngine/BoxElem.h>
#include <RawMesh.h>
#include "VOX.h"
#include "VoxAssetImportData.h"
#include "VoxImportOption.h"
#include "Voxel.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelFactory, Log, All)

UVoxelFactory::UVoxelFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ImportOption(nullptr)
	, bShowOption(true)
{
	Formats.Add(TEXT("vox;MagicaVoxel"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;
}

void UVoxelFactory::PostInitProperties()
{
	Super::PostInitProperties();
	ImportOption = NewObject<UVoxImportOption>(this, NAME_None, RF_NoFlags);
}

bool UVoxelFactory::DoesSupportClass(UClass* Class)
{
	return Class == UStaticMesh::StaticClass()
		// || Class == USkeletalMesh::StaticClass()
		|| Class == UVoxel::StaticClass();
}

UClass* UVoxelFactory::ResolveSupportedClass()
{
	UClass* Class = nullptr;
	if (ImportOption->VoxImportType == EVoxImportType::StaticMesh) 
	{
		Class = UStaticMesh::StaticClass();
	/*} 
	else if (ImportOption->VoxImportType == EVoxImportType::SkeletalMesh) 
	{
		Class = USkeletalMesh::StaticClass();*/
	} 
	else if (ImportOption->VoxImportType == EVoxImportType::Voxel) 
	{
		Class = UVoxel::StaticClass();
	}
	return Class;
}

UObject* UVoxelFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
	UObject* Result = nullptr;
	UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>();

	ImportSubsystem->OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	bool bImportAll = true;
	if (!bShowOption || ImportOption->GetImportOption(bImportAll))
	{
		bShowOption = !bImportAll;
		FBufferReader Reader((void*)Buffer, BufferEnd - Buffer, false);
		FVox Vox(GetCurrentFilename(), Reader, ImportOption);

		FName FinalName = InName;
		UObject* FinalParent = InParent;
		if (ImportOption->CustomAssetName.IsEmpty() == false)
		{
			const FString CustomName = ImportOption->CustomAssetName;
			FString ParentPath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());
			FString NewPackagePath = ParentPath / CustomName;

			UPackage* NewPackage = CreatePackage(*NewPackagePath);
			NewPackage->FullyLoad();

			FinalParent = NewPackage;
			FinalName = FName(*CustomName);

			UE_LOG(LogVoxelFactory, Log, TEXT("Custom asset name set: %s (package: %s)"), *FinalName.ToString(), *NewPackagePath);
		}

		switch (ImportOption->VoxImportType) 
		{
		case EVoxImportType::StaticMesh:
			Result = CreateStaticMesh(FinalParent, FinalName, Flags, &Vox);
			break;
		/*case EVoxImportType::SkeletalMesh:
			Result = CreateSkeletalMesh(FinalParent, FinalName, Flags, &Vox);
			break;*/
		case EVoxImportType::Voxel:
			Result = CreateVoxel(FinalParent, FinalName, Flags, &Vox);
			break;
		default:
			break;
		}
	}

	if (Result)
	{
		FAssetRegistryModule::AssetCreated(Result);
		Result->MarkPackageDirty();
	}
	ImportSubsystem->OnAssetPostImport.Broadcast(this, Result);

	return Result;
}

bool UVoxelFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
	//USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	UVoxel* Voxel = Cast<UVoxel>(Obj);

	const auto& AssetImportData = StaticMesh != nullptr ? StaticMesh->GetAssetImportData()
		//: SkeletalMesh != nullptr ? SkeletalMesh->GetAssetImportData()
		: Voxel != nullptr ? Voxel->GetAssetImportData()
		: nullptr;
	if (AssetImportData != nullptr) 
	{
		const auto& SourcePath = AssetImportData->GetFirstFilename();
		FString Path, Filename, Extension;
		FPaths::Split(SourcePath, Path, Filename, Extension);
		if (Extension.Compare("vox", ESearchCase::IgnoreCase) == 0) 
		{
			AssetImportData->ExtractFilenames(OutFilenames);
			return true;
		}
	}
	return false;
}

void UVoxelFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
	//USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	UVoxel* Voxel = Cast<UVoxel>(Obj);

	const auto& AssetImportData = StaticMesh ? StaticMesh->GetAssetImportData()
		//: SkeletalMesh ? SkeletalMesh->GetAssetImportData()
		: Voxel ? Voxel->GetAssetImportData()
		: nullptr;
	if (AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UVoxelFactory::Reimport(UObject* Obj)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
	//USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	UVoxel* Voxel = Cast<UVoxel>(Obj);

	const auto& AssetImportData = StaticMesh ? Cast<UVoxAssetImportData>(StaticMesh->GetAssetImportData())
		//: SkeletalMesh ? Cast<UVoxAssetImportData>(SkeletalMesh->GetAssetImportData())
		: Voxel ? Cast<UVoxAssetImportData>(Voxel->GetAssetImportData())
		: nullptr;
	if (!AssetImportData) 
	{
		return EReimportResult::Failed;
	}

	const auto& Filename = AssetImportData->GetFirstFilename();
	if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	auto Result = EReimportResult::Failed;
	auto OutCanceled = false;
	AssetImportData->ToVoxImportOption(*ImportOption);
	bShowOption = false;
	if (ImportObject(Obj->GetClass(), Obj->GetOuter(), *Obj->GetName(), RF_Public | RF_Standalone, Filename, nullptr, OutCanceled) != nullptr)
	{
		UE_LOG(LogVoxelFactory, Verbose, TEXT("Reimport successfully."));
		AssetImportData->Update(Filename);
		if (Obj->GetOuter())
		{
			Obj->GetOuter()->MarkPackageDirty();
		} 
		else
		{
			Obj->MarkPackageDirty();
		}
		Result = EReimportResult::Succeeded;
	} 
	else 
	{
		if (OutCanceled) 
		{
			UE_LOG(LogVoxelFactory, Warning, TEXT("Reimport canceled."));
		}
		else
		{
			UE_LOG(LogVoxelFactory, Warning, TEXT("Reimport failed."));
		}
		Result = EReimportResult::Failed;
	}
	return Result;
}

UStaticMesh* UVoxelFactory::CreateStaticMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox) const
{
	UE_LOG(LogVoxelFactory, Error, TEXT("Creating Static Mesh: %s, But wanted: %s"), *InName.ToString(), *ImportOption->CustomAssetName);

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(InParent, InName, Flags | RF_Public | RF_Standalone);
	if (!StaticMesh->GetAssetImportData() || !StaticMesh->GetAssetImportData()->IsA<UVoxAssetImportData>())
	{
		auto AssetImportData = NewObject<UVoxAssetImportData>(StaticMesh);
		AssetImportData->FromVoxImportOption(*ImportOption);
		StaticMesh->SetAssetImportData(AssetImportData);
	}

	FRawMesh RawMesh;
	if (!Vox->CreateOptimizedRawMesh(RawMesh, ImportOption))
	{
		UE_LOG(LogVoxelFactory, Warning, TEXT("Failed to create optimized raw mesh"));
	}

	if (ImportOption->bImportMaterial)
	{
		if (ImportOption->bOneMaterial)
		{
			UMaterialInterface* Material = CreateMaterial(InParent, InName, Flags, Vox);
			StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
		}
		else
		{
			TArray<uint8> Palette;
			GenerateMaterials(InParent, InName, Flags, Vox, Palette);
			
			FString BasePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());
			FString MeshResourcesFolderPath = BasePath;
			if (ImportOption->ResourcesSaveLocation == EVoxResourcesSaveLocation::SubFolder)
			{
				MeshResourcesFolderPath = BasePath / FString::Printf(TEXT("%s_Resources"), *InName.GetPlainNameString());
			}

			for (uint8 color : Palette)
			{
				FString MIPath = MeshResourcesFolderPath / FString::Printf(TEXT("%s_MI_%d.%s_MI_%d"), *InName.GetPlainNameString(), color, *InName.GetPlainNameString(), color);

				FString MaterialInstanceName = FString::Printf(TEXT("%s_MI%d"), *InName.GetPlainNameString(), color);
				UMaterialInstanceConstant* MaterialInstance = LoadObject<UMaterialInstanceConstant>(nullptr, *MIPath);
				if (MaterialInstance)
				{
					StaticMesh->GetStaticMaterials().Add(FStaticMaterial(MaterialInstance));
				}
				else
				{
					UE_LOG(LogVoxelFactory, Warning, TEXT("Could not find material instance at: %s"), *MIPath);
				}
			}
		}
	}

	BuildStaticMesh(StaticMesh, RawMesh);
	StaticMesh->GetAssetImportData()->Update(Vox->Filename);
	return StaticMesh;
}

/*
USkeletalMesh* UVoxelFactory::CreateSkeletalMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox) const
{
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(InParent, InName, Flags | RF_Public);
	if (!SkeletalMesh->GetAssetImportData() || !SkeletalMesh->GetAssetImportData()->IsA<UVoxAssetImportData>()) {
		auto AssetImportData = NewObject<UVoxAssetImportData>(SkeletalMesh);
		AssetImportData->FromVoxImportOption(*ImportOption);
		SkeletalMesh->SetAssetImportData(AssetImportData);
	}
	
	SkeletalMesh->GetAssetImportData()->Update(Vox->Filename);
	return SkeletalMesh;
}
*/

UVoxel* UVoxelFactory::CreateVoxel(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox) const
{
	UVoxel* Voxel = NewObject<UVoxel>(InParent, InName, Flags | RF_Public | RF_Standalone);
	if (!Voxel->GetAssetImportData() || !Voxel->GetAssetImportData()->IsA<UVoxAssetImportData>())
	{
		auto AssetImportData = NewObject<UVoxAssetImportData>(Voxel);
		AssetImportData->FromVoxImportOption(*ImportOption);
		Voxel->SetAssetImportData(AssetImportData);
	}
	Voxel->Size = Vox->Size;

	TArray<uint8> Palette;
	GenerateMaterials(InParent, InName, Flags, Vox, Palette);

	FString BasePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());
	FString MeshResourcesFolderPath = BasePath;
	if (ImportOption->ResourcesSaveLocation == EVoxResourcesSaveLocation::SubFolder)
	{
		MeshResourcesFolderPath = BasePath / FString::Printf(TEXT("%s_Resources"), *InName.GetPlainNameString());
	}

	for (uint8 color : Palette)
	{
		FRawMesh RawMesh;
		FVox::CreateMesh(RawMesh, ImportOption);
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(InParent, *FString::Printf(TEXT("%s_SM_%d"), *InName.GetPlainNameString(), color), Flags | RF_Public);

		if (ImportOption->bImportMaterial)
		{
			FString MIPath = MeshResourcesFolderPath / FString::Printf(TEXT("%s_MI_%d.%s_MI_%d"), *InName.GetPlainNameString(), color, *InName.GetPlainNameString(), color);

			FString MaterialInstanceName = FString::Printf(TEXT("%s_MI%d"), *InName.GetPlainNameString(), color);
			UMaterialInstanceConstant* MaterialInstance = LoadObject<UMaterialInstanceConstant>(nullptr, *MIPath);
			if (MaterialInstance)
			{
				StaticMesh->GetStaticMaterials().Add(FStaticMaterial(MaterialInstance));
			}
			else
			{
				UE_LOG(LogVoxelFactory, Warning, TEXT("Could not find material instance at: %s"), *MIPath);
			}

			if (ImportOption->bPaletteToTexture)
			{
				for (FVector2f& TexCoord : RawMesh.WedgeTexCoords[0])
				{
					TexCoord = FVector2f(((double)color + 0.5) / 256.0, 0.5);
				}
			}
		}

		BuildStaticMesh(StaticMesh, RawMesh);
		
		const FVector& Scale = ImportOption->GetBuildSettings().BuildScale3D;
		FKBoxElem BoxElem(Scale.X, Scale.Y, Scale.Z);
		StaticMesh->GetBodySetup()->AggGeom.BoxElems.Add(BoxElem);
		
		Voxel->Mesh.Add(StaticMesh);
	}
	for (const auto& cell : Vox->Voxel)
	{
		Voxel->Voxel.Add(cell.Key, Palette.IndexOfByKey(cell.Value));
		check(INDEX_NONE != Palette.IndexOfByKey(cell.Value));
	}
	Voxel->bXYCenter = ImportOption->bImportXYCenter;
	Voxel->CalcCellBounds();
	Voxel->GetAssetImportData()->Update(Vox->Filename);
	return Voxel;
}

UStaticMesh* UVoxelFactory::BuildStaticMesh(UStaticMesh* OutStaticMesh, FRawMesh& RawMesh) const
{
	check(OutStaticMesh);
	const FVector3f& Scale = FVector3f(ImportOption->GetBuildSettings().BuildScale3D);
	if (!Scale.Equals(FVector3f(1.0f)))
	{
		for (FVector3f& Vertex : RawMesh.VertexPositions)
		{
			Vertex = Vertex * Scale;
		}
	}

	FStaticMeshSourceModel& StaticMeshSourceModel = OutStaticMesh->AddSourceModel();
	StaticMeshSourceModel.BuildSettings = ImportOption->GetBuildSettings();
	StaticMeshSourceModel.BuildSettings.BuildScale3D = FVector(1.0f);
	StaticMeshSourceModel.RawMeshBulkData->SaveRawMesh(RawMesh);
	TArray<FText> Errors;
	OutStaticMesh->Build(false, &Errors);
	return OutStaticMesh;
}

UMaterialInterface* UVoxelFactory::CreateMaterial(UObject* InParent, FName& InName, EObjectFlags Flags, const FVox* Vox) const
{
	FString BasePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());

	FString MaterialPackageName = BasePath / FString::Printf(TEXT("%s_MT"), *InName.GetPlainNameString());
	UPackage* MaterialPackage = CreatePackage(*MaterialPackageName);
	MaterialPackage->FullyLoad();

	UMaterial* Material = NewObject<UMaterial>(MaterialPackage, *FString::Printf(TEXT("%s_MT"), *InName.GetPlainNameString()), Flags | RF_Public | RF_Standalone);

	FString TexturePackageName = BasePath / FString::Printf(TEXT("%s_TX"), *InName.GetPlainNameString());
	UPackage* TexturePackage = CreatePackage(*TexturePackageName);
	TexturePackage->FullyLoad();

	UTexture2D* Texture = NewObject<UTexture2D>(TexturePackage, *FString::Printf(TEXT("%s_TX"), *InName.GetPlainNameString()), Flags | RF_Public | RF_Standalone);
	if (Vox->CreateTexture(Texture, ImportOption))
	{
		Material->TwoSided = false;
		Material->SetShadingModel(MSM_DefaultLit);
		UMaterialExpressionTextureSample* Expression = NewObject<UMaterialExpressionTextureSample>(Material);
		auto EditorOnly = Material->GetEditorOnlyData();
		EditorOnly->ExpressionCollection.AddExpression(Expression);
		EditorOnly->BaseColor.Expression = Expression;
		Expression->Texture = Texture;
		Material->PostEditChange();

		FAssetRegistryModule::AssetCreated(Texture);
		TexturePackage->MarkPackageDirty();
	}

	FAssetRegistryModule::AssetCreated(Material);
	MaterialPackage->MarkPackageDirty();
	return Material;
}

void UVoxelFactory::GenerateMaterials(UObject* InParent, FName& InName, EObjectFlags Flags, const FVox* Vox, TArray<uint8>& OutPalette) const {
	OutPalette.Empty();
	for (const auto& cell : Vox->Voxel)
	{
		OutPalette.AddUnique(cell.Value);
	}

	FString BasePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());

	FString MeshResourcesFolderPath = BasePath;
	if (ImportOption->ResourcesSaveLocation == EVoxResourcesSaveLocation::SubFolder)
	{
		MeshResourcesFolderPath = BasePath / FString::Printf(TEXT("%s_Resources"), *InName.GetPlainNameString());
		IFileManager::Get().MakeDirectory(*MeshResourcesFolderPath, true);
	}

	FString MaterialPackageName = MeshResourcesFolderPath / FString::Printf(TEXT("%s_MT"), *InName.GetPlainNameString());
	UPackage* MaterialPackage = CreatePackage(*MaterialPackageName);
	MaterialPackage->FullyLoad();
	
	UMaterial* Material = NewObject<UMaterial>(MaterialPackage, *FString::Printf(TEXT("%s_MT"), *InName.GetPlainNameString()), Flags | RF_Public | RF_Standalone);
	Material->TwoSided = false;
	Material->SetShadingModel(MSM_DefaultLit);
	auto EditorOnly = Material->GetEditorOnlyData();
	
	UMaterialExpressionTextureSample* TextureExpression = nullptr;
	if (ImportOption->bPaletteToTexture)
	{
		FString TexturePackageName = MeshResourcesFolderPath / FString::Printf(TEXT("%s_TX"), *InName.GetPlainNameString());
		UPackage* TexturePackage = CreatePackage(*TexturePackageName);
		TexturePackage->FullyLoad();

		UTexture2D* Texture = NewObject<UTexture2D>(TexturePackage, *FString::Printf(TEXT("%s_TX"), *InName.GetPlainNameString()), Flags | RF_Public | RF_Standalone);

		if (Vox->CreateTexture(Texture, ImportOption))
		{
			TextureExpression = NewObject<UMaterialExpressionTextureSample>(Material);
			TextureExpression->Texture = Texture;
			TextureExpression->MaterialExpressionEditorX = -625;
			TextureExpression->MaterialExpressionEditorY = -60;
			EditorOnly->ExpressionCollection.AddExpression(TextureExpression);
			EditorOnly->BaseColor.Expression = TextureExpression;

			FAssetRegistryModule::AssetCreated(Texture);
			TexturePackage->MarkPackageDirty();
		}
		else
		{
			UE_LOG(LogVoxelFactory, Warning, TEXT("Failed to create palette texture. Creating material with color expression instead."));
		}
	}

	bool textureNotSampled = !ImportOption->bPaletteToTexture || nullptr == TextureExpression;

	auto MakeScalar = [&](const TCHAR* Name, float Default, int32 X, int32 Y)
    {
        auto* Param = NewObject<UMaterialExpressionScalarParameter>(Material);
        Param->ParameterName = Name;
        Param->DefaultValue = Default;
        Param->MaterialExpressionEditorX = X;
        Param->MaterialExpressionEditorY = Y;
        EditorOnly->ExpressionCollection.AddExpression(Param);
        return Param;
    };

	auto MakeColorVector = [&](const TCHAR* Name, FLinearColor Default, int32 X, int32 Y)
    {
        auto* Param = NewObject<UMaterialExpressionVectorParameter>(Material);
        Param->ParameterName = Name;
        Param->DefaultValue = Default;
        Param->MaterialExpressionEditorX = X;
        Param->MaterialExpressionEditorY = Y;
        EditorOnly->ExpressionCollection.AddExpression(Param);
        return Param;
    };

	// Roughness Expression
	auto* RoughnessExpression = MakeScalar(TEXT("Roughness"), 0.5f, -250, 220);
	EditorOnly->Roughness.Expression = RoughnessExpression;

	// Metallic Expression - [for Metal Material]
	auto* MetallicExpression = MakeScalar(TEXT("Metallic"), 0.0f, -250, 325);
	EditorOnly->Metallic.Expression = MetallicExpression;

	// Opacity Expression - [for Glass Material]
	auto* OpacityExpression = MakeScalar(TEXT("Opacity"), 1.0f, -625, 220);
	EditorOnly->Opacity.Expression = OpacityExpression;

	// Emission Expression - [for Emissive Material]
	auto* EmissionExpression = MakeScalar(TEXT("Emission"), 0.0f, -625, 535);

	// Emission Power Expression - [for Emissive Material]
	auto* EmissionPowerExpression = MakeScalar(TEXT("EmissionPower"), 0.0f, -625, 640);

	// Multiply Node Expression 1 - [for Emissive Material]
	UMaterialExpressionMultiply* MultiplyEmissionExpression1 = NewObject<UMaterialExpressionMultiply>(Material);
	MultiplyEmissionExpression1->MaterialExpressionEditorX = -375;
	MultiplyEmissionExpression1->MaterialExpressionEditorY = 520;
	MultiplyEmissionExpression1->A.Connect(0, EmissionExpression);
	MultiplyEmissionExpression1->B.Connect(0, EmissionPowerExpression);
	EditorOnly->ExpressionCollection.AddExpression(MultiplyEmissionExpression1);

	// Multiply Node Expression 2 - [for Emissive Material]
	UMaterialExpressionMultiply* MultiplyEmissionExpression2 = NewObject<UMaterialExpressionMultiply>(Material);
	MultiplyEmissionExpression2->MaterialExpressionEditorX = -250;
	MultiplyEmissionExpression2->MaterialExpressionEditorY = 430;
	MultiplyEmissionExpression2->B.Connect(0, MultiplyEmissionExpression1);

	if (textureNotSampled)
	{
		// Color Expression
		auto* ColorExpression = MakeColorVector(TEXT("Color"), FLinearColor::Gray, -250, 0);
		EditorOnly->BaseColor.Expression = ColorExpression;

		// Emission Color Expression - [for Emissive Material]
		auto* EmissionColorExpression = MakeColorVector(TEXT("EmissionColor"), FLinearColor::White, -625, 325);
		MultiplyEmissionExpression2->A.Connect(0, EmissionColorExpression);
	}
	else
	{
		MultiplyEmissionExpression2->A.Connect(0, TextureExpression);
	}

	EditorOnly->ExpressionCollection.AddExpression(MultiplyEmissionExpression2);
	EditorOnly->EmissiveColor.Expression = MultiplyEmissionExpression2;
	Material->PostEditChange();

	FAssetRegistryModule::AssetCreated(Material);
	MaterialPackage->MarkPackageDirty();

	for (uint8 color : OutPalette)
	{
		const auto& VoxMaterialData = Vox->Materials[color];
		FLinearColor LinearColor = FLinearColor::FromSRGBColor(Vox->Palette[color]);

		FString MIPackageName = MeshResourcesFolderPath / FString::Printf(TEXT("%s_MI_%d"), *InName.GetPlainNameString(), color);
		UPackage* MIPackage = CreatePackage(*MIPackageName);
		MIPackage->FullyLoad();

		UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>(MIPackage, *FString::Printf(TEXT("%s_MI_%d"), *InName.GetPlainNameString(), color), Flags | RF_Public | RF_Standalone);
		MaterialInstance->SetParentEditorOnly(Material);

		if (textureNotSampled)
		{
			MaterialInstance->SetVectorParameterValueEditorOnly(TEXT("Color"), LinearColor);
		}

		MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Roughness"), VoxMaterialData.Roughness);

		if (VoxMaterialData.Type == EVoxMaterialType::GLASS)
		{
			UE_LOG(LogVoxelFactory, Verbose, TEXT("Create Glass Material Instance: %s"), *MaterialInstance->GetName());
			MaterialInstance->BasePropertyOverrides.bOverride_BlendMode = true;
			MaterialInstance->BasePropertyOverrides.BlendMode = EBlendMode::BLEND_Translucent;
			MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Opacity"), 1.0f - VoxMaterialData.Transparency);

			MaterialInstance->UpdateOverridableBaseProperties();
		}
		else if (VoxMaterialData.Type == EVoxMaterialType::METAL)
		{
			UE_LOG(LogVoxelFactory, Verbose, TEXT("Create Metal Material Instance: %s"), *MaterialInstance->GetName());
			MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Metallic"), VoxMaterialData.Metallic);
		}
		else if (VoxMaterialData.Type == EVoxMaterialType::EMIT)
		{
			UE_LOG(LogVoxelFactory, Verbose, TEXT("Create Emissive Material Instance: %s"), *MaterialInstance->GetName());
			if (textureNotSampled)
			{
				MaterialInstance->SetVectorParameterValueEditorOnly(TEXT("EmissionColor"), LinearColor);
			}
			MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("EmissionPower"), VoxMaterialData.EmissionPower);
			MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Emission"), VoxMaterialData.Emissive);
		}

		UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);

		FAssetRegistryModule::AssetCreated(MaterialInstance);
		MIPackage->MarkPackageDirty();
	}
}