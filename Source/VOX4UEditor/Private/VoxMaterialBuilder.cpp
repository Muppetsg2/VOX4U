// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxMaterialBuilder.h"
#include "Engine/Texture2D.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Math/Float16Color.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxMaterialBuilder, Log, All)

namespace VoxMaterialBuilderPrivate
{
	// Material parameter names, shared by the parent material and its instances.
	const FName ParamColor(TEXT("Color"));
	const FName ParamRoughness(TEXT("Roughness"));
	const FName ParamMetallic(TEXT("Metallic"));
	const FName ParamSpecular(TEXT("Specular"));
	const FName ParamEmission(TEXT("Emission"));
	const FName ParamEmissionScale(TEXT("EmissionScale"));
	const FName ParamOpacity(TEXT("Opacity"));
	const FName ParamIOR(TEXT("IOR"));

	/**
	 * A 1 pixel high lookup texture. Uncompressed and point sampled: block compression and bilinear
	 * filtering would blend neighbouring palette entries.
	 */
	UTexture2D* WriteLookupTexture(const FVoxAssetLocation& Location, const FString& Name, int32 Width,
		ETextureSourceFormat Format, const uint8* Data, bool bSRGB, TextureCompressionSettings Compression)
	{
		bool bCreated = false;
		UTexture2D* Texture = VoxAssets::FindOrCreate<UTexture2D>(Location.ResourcesPath, Name, Location.Flags, bCreated);
		if (!Texture)
		{
			return nullptr;
		}

		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Width, 1, 1, 1, Format, Data);
		Texture->SRGB = bSRGB;
		Texture->CompressionSettings = Compression;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->LODGroup = TEXTUREGROUP_Pixels2D;
		Texture->Filter = TF_Nearest;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;
		Texture->PostEditChange();

		VoxAssets::Finish(Texture, bCreated);
		return Texture;
	}

	template <class T>
	T* AddExpression(UMaterial* Material, int32 X, int32 Y)
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass(), X, Y));
	}

	UMaterialExpressionScalarParameter* AddScalar(UMaterial* Material, FName Name, float Default, int32 X, int32 Y)
	{
		UMaterialExpressionScalarParameter* Parameter = AddExpression<UMaterialExpressionScalarParameter>(Material, X, Y);
		Parameter->ParameterName = Name;
		Parameter->DefaultValue = Default;
		return Parameter;
	}

	UMaterialExpressionTextureSample* AddTextureSample(UMaterial* Material, UTexture2D* Texture, bool bLinear, int32 X, int32 Y)
	{
		UMaterialExpressionTextureSample* Sample = AddExpression<UMaterialExpressionTextureSample>(Material, X, Y);
		Sample->Texture = Texture;
		Sample->SamplerType = bLinear ? SAMPLERTYPE_LinearColor : SAMPLERTYPE_Color;
		return Sample;
	}

	/** A * B, inputs given as (expression, output name). */
	UMaterialExpressionMultiply* AddMultiply(UMaterial* Material, UMaterialExpression* A, const FString& AOutput,
		UMaterialExpression* B, const FString& BOutput, int32 X, int32 Y)
	{
		UMaterialExpressionMultiply* Multiply = AddExpression<UMaterialExpressionMultiply>(Material, X, Y);
		UMaterialEditingLibrary::ConnectMaterialExpressions(A, AOutput, Multiply, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(B, BOutput, Multiply, TEXT("B"));
		return Multiply;
	}
}

FVoxSurface FVoxSurface::FromMaterial(const v4c_material& Material)
{
	const bool bMetal = Material.type == V4C_MATERIAL_METAL || Material.type == V4C_MATERIAL_BLEND;
	const bool bTransmissive = Material.type == V4C_MATERIAL_GLASS || Material.type == V4C_MATERIAL_BLEND;

	FVoxSurface Surface;
	Surface.Roughness = FMath::Clamp(Material.roughness, 0.0f, 1.0f);
	Surface.Metallic = bMetal ? FMath::Clamp(Material.metallic, 0.0f, 1.0f) : 0.0f;
	// MagicaVoxel specular is 0..2 with 1 as default, Unreal's 0..1 with 0.5.
	Surface.Specular = bMetal ? FMath::Clamp(Material.specular * 0.5f, 0.0f, 1.0f) : 0.5f;
	// _emit is the strength, _flux the power (shown as 1..5 in MagicaVoxel, stored as 0..4).
	Surface.Emission = Material.type == V4C_MATERIAL_EMIT
		? FMath::Max(0.0f, Material.emissive) * (1.0f + FMath::Max(0.0f, Material.emission_power))
		: 0.0f;

	if (Material.type == V4C_MATERIAL_MEDIA)
	{
		// A participating medium has no surface; approximate it by its density (0..0.1).
		Surface.Opacity = FMath::Clamp(Material.density * 10.0f, 0.05f, 1.0f);
		Surface.bTranslucent = true;
	}
	else if (bTransmissive && Material.transparency > 0.001f)
	{
		Surface.Opacity = 1.0f - FMath::Clamp(Material.transparency, 0.0f, 1.0f);
		Surface.IOR = FMath::Max(1.0f, Material.ior);
		Surface.bTranslucent = true;
	}
	return Surface;
}

FVoxMaterialBuilder::FVoxMaterialBuilder(const FVoxScene& InScene, const FVoxImportSettings& InSettings, const FVoxAssetLocation& InLocation)
	: Scene(InScene)
	, Settings(InSettings)
	, Location(InLocation)
{
}

FString FVoxMaterialBuilder::AssetName(EVoxAssetType ObjectType, int32 Color, const FString& Description) const
{
	FVoxNameFormatArgs Args;
	Args.BaseName = Location.BaseName;
	Args.Color = Color;
	Args.Description = Description;
	return FVoxNameFormatter::Format(ObjectType, Args, Location.Naming);
}

UMaterialInterface* FVoxMaterialBuilder::GetMaterial(uint32 Material)
{
	if (!Settings.bImportMaterial || !Scene.IsValid() || Material >= Scene->num_materials)
	{
		return nullptr;
	}

	if (Settings.bOneMaterial)
	{
		return GetOneMaterial();
	}

	if (UMaterialInterface** Found = Instances.Find(Material))
	{
		return *Found;
	}
	UMaterialInterface* Instance = CreateMaterialInstance(Material);
	Instances.Add(Material, Instance);
	return Instance;
}

UTexture2D* FVoxMaterialBuilder::GetPaletteTexture()
{
	if (!PaletteTexture)
	{
		const int32 Width = static_cast<int32>(Scene->num_materials);
		TArray<FColor> Texels;
		Texels.SetNumUninitialized(Width);
		for (int32 Index = 0; Index < Width; ++Index)
		{
			Texels[Index] = Scene.GetColor(static_cast<uint32>(Index));
		}

		// FColor is laid out as BGRA in memory.
		PaletteTexture = VoxMaterialBuilderPrivate::WriteLookupTexture(Location, AssetName(EVoxAssetType::Texture, -1, TEXT("Palette")), Width,
			TSF_BGRA8, reinterpret_cast<const uint8*>(Texels.GetData()), true, TC_EditorIcon);
	}
	return PaletteTexture;
}

UTexture2D* FVoxMaterialBuilder::GetPropertiesTexture()
{
	if (!PropertiesTexture)
	{
		// R roughness, G metallic, B specular, A emission (unbounded, hence half floats).
		const int32 Width = static_cast<int32>(Scene->num_materials);
		TArray<FFloat16Color> Texels;
		Texels.SetNumUninitialized(Width);
		for (int32 Index = 0; Index < Width; ++Index)
		{
			const FVoxSurface Surface = FVoxSurface::FromMaterial(Scene->materials[Index]);
			Texels[Index] = FFloat16Color(FLinearColor(Surface.Roughness, Surface.Metallic, Surface.Specular, Surface.Emission));
		}

		PropertiesTexture = VoxMaterialBuilderPrivate::WriteLookupTexture(Location, AssetName(EVoxAssetType::Texture, -1, TEXT("Properties")), Width,
			TSF_RGBA16F, reinterpret_cast<const uint8*>(Texels.GetData()), false, TC_HDR);
	}
	return PropertiesTexture;
}

UMaterial* FVoxMaterialBuilder::GetOneMaterial()
{
	if (bSharedMaterialDone)
	{
		return SharedMaterial;
	}
	bSharedMaterialDone = true;

	UTexture2D* Palette = GetPaletteTexture();
	UTexture2D* Properties = GetPropertiesTexture();

	bool bCreated = false;
	UMaterial* Material = VoxAssets::FindOrCreate<UMaterial>(Location.ResourcesPath, AssetName(EVoxAssetType::Material, -1, TEXT("")), Location.Flags, bCreated);
	if (Material && bCreated && Palette && Properties)
	{
		// BaseColor = palette, Roughness / Metallic / Specular = properties.rgb,
		// Emissive = palette * properties.a * EmissionScale. Both textures are sampled with UV0.
		UMaterialExpressionTextureSample* PaletteSample = VoxMaterialBuilderPrivate::AddTextureSample(Material, Palette, false, -700, 0);
		UMaterialExpressionTextureSample* PropertiesSample = VoxMaterialBuilderPrivate::AddTextureSample(Material, Properties, true, -700, 300);
		UMaterialExpressionScalarParameter* EmissionScale = VoxMaterialBuilderPrivate::AddScalar(Material, VoxMaterialBuilderPrivate::ParamEmissionScale, DefaultEmissionScale, -700, 600);

		UMaterialEditingLibrary::ConnectMaterialProperty(PaletteSample, TEXT("RGB"), MP_BaseColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(PropertiesSample, TEXT("R"), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(PropertiesSample, TEXT("G"), MP_Metallic);
		UMaterialEditingLibrary::ConnectMaterialProperty(PropertiesSample, TEXT("B"), MP_Specular);

		UMaterialExpressionMultiply* Emission = VoxMaterialBuilderPrivate::AddMultiply(Material, PaletteSample, TEXT("RGB"), PropertiesSample, TEXT("A"), -400, 450);
		UMaterialExpressionMultiply* Emissive = VoxMaterialBuilderPrivate::AddMultiply(Material, Emission, TEXT(""), EmissionScale, TEXT(""), -200, 500);
		UMaterialEditingLibrary::ConnectMaterialProperty(Emissive, TEXT(""), MP_EmissiveColor);

		UMaterialEditingLibrary::RecompileMaterial(Material);
	}
	VoxAssets::Finish(Material, bCreated);

	SharedMaterial = Material;
	return SharedMaterial;
}

UMaterial* FVoxMaterialBuilder::GetParentMaterial()
{
	if (bSharedMaterialDone)
	{
		return SharedMaterial;
	}
	bSharedMaterialDone = true;

	UTexture2D* Palette = Settings.bPaletteToTexture ? GetPaletteTexture() : nullptr;

	bool bCreated = false;
	UMaterial* Material = VoxAssets::FindOrCreate<UMaterial>(Location.ResourcesPath, AssetName(EVoxAssetType::Material, -1, TEXT("")), Location.Flags, bCreated);
	if (Material && bCreated)
	{
		// An existing parent material is kept as it is (it may have been edited), only new ones get the graph.
		UMaterialExpression* Color = nullptr;
		FString ColorOutput;
		if (Palette)
		{
			Color = VoxMaterialBuilderPrivate::AddTextureSample(Material, Palette, false, -700, 0);
			ColorOutput = TEXT("RGB");
		}
		else
		{
			UMaterialExpressionVectorParameter* ColorParameter = VoxMaterialBuilderPrivate::AddExpression<UMaterialExpressionVectorParameter>(Material, -700, 0);
			ColorParameter->ParameterName = VoxMaterialBuilderPrivate::ParamColor;
			ColorParameter->DefaultValue = FLinearColor::Gray;
			Color = ColorParameter;
		}
		UMaterialEditingLibrary::ConnectMaterialProperty(Color, ColorOutput, MP_BaseColor);

		UMaterialEditingLibrary::ConnectMaterialProperty(VoxMaterialBuilderPrivate::AddScalar(Material, VoxMaterialBuilderPrivate::ParamRoughness, 0.5f, -400, 150), TEXT(""), MP_Roughness);
		UMaterialEditingLibrary::ConnectMaterialProperty(VoxMaterialBuilderPrivate::AddScalar(Material, VoxMaterialBuilderPrivate::ParamMetallic, 0.0f, -400, 250), TEXT(""), MP_Metallic);
		UMaterialEditingLibrary::ConnectMaterialProperty(VoxMaterialBuilderPrivate::AddScalar(Material, VoxMaterialBuilderPrivate::ParamSpecular, 0.5f, -400, 350), TEXT(""), MP_Specular);
		// Used by the instances that switch to translucent (glass, blend, media).
		UMaterialEditingLibrary::ConnectMaterialProperty(VoxMaterialBuilderPrivate::AddScalar(Material, VoxMaterialBuilderPrivate::ParamOpacity, 1.0f, -400, 450), TEXT(""), MP_Opacity);
		UMaterialEditingLibrary::ConnectMaterialProperty(VoxMaterialBuilderPrivate::AddScalar(Material, VoxMaterialBuilderPrivate::ParamIOR, 1.0f, -400, 550), TEXT(""), MP_Refraction);

		UMaterialExpressionScalarParameter* Emission = VoxMaterialBuilderPrivate::AddScalar(Material, VoxMaterialBuilderPrivate::ParamEmission, 0.0f, -700, 650);
		UMaterialExpressionScalarParameter* EmissionScale = VoxMaterialBuilderPrivate::AddScalar(Material, VoxMaterialBuilderPrivate::ParamEmissionScale, DefaultEmissionScale, -700, 750);
		UMaterialExpressionMultiply* Strength = VoxMaterialBuilderPrivate::AddMultiply(Material, Emission, TEXT(""), EmissionScale, TEXT(""), -450, 700);
		UMaterialExpressionMultiply* Emissive = VoxMaterialBuilderPrivate::AddMultiply(Material, Color, ColorOutput, Strength, TEXT(""), -200, 650);
		UMaterialEditingLibrary::ConnectMaterialProperty(Emissive, TEXT(""), MP_EmissiveColor);

		UMaterialEditingLibrary::RecompileMaterial(Material);
	}
	VoxAssets::Finish(Material, bCreated);

	SharedMaterial = Material;
	return SharedMaterial;
}

UMaterialInterface* FVoxMaterialBuilder::CreateMaterialInstance(uint32 Material)
{
	UMaterial* Parent = GetParentMaterial();
	if (!Parent)
	{
		return nullptr;
	}

	const v4c_material& VoxMaterial = Scene->materials[Material];
	const FVoxSurface Surface = FVoxSurface::FromMaterial(VoxMaterial);

	bool bCreated = false;
	const FString Name = AssetName(EVoxAssetType::MaterialInstance, VoxMaterial.palette_index, TEXT(""));
	UMaterialInstanceConstant* Instance = VoxAssets::FindOrCreate<UMaterialInstanceConstant>(Location.ResourcesPath, Name, Location.Flags, bCreated);
	if (!Instance)
	{
		return Parent;
	}

	UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, Parent);
	if (!Settings.bPaletteToTexture)
	{
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, VoxMaterialBuilderPrivate::ParamColor, FLinearColor(Scene.GetColor(Material)));
	}
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, VoxMaterialBuilderPrivate::ParamRoughness, Surface.Roughness);
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, VoxMaterialBuilderPrivate::ParamMetallic, Surface.Metallic);
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, VoxMaterialBuilderPrivate::ParamSpecular, Surface.Specular);
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, VoxMaterialBuilderPrivate::ParamEmission, Surface.Emission);
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, VoxMaterialBuilderPrivate::ParamOpacity, Surface.Opacity);
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, VoxMaterialBuilderPrivate::ParamIOR, Surface.IOR);

	Instance->BasePropertyOverrides.bOverride_BlendMode = Surface.bTranslucent;
	Instance->BasePropertyOverrides.BlendMode = Surface.bTranslucent ? BLEND_Translucent : BLEND_Opaque;

	UE_LOG(LogVoxMaterialBuilder, Verbose, TEXT("Material instance %s (type %d)"), *Name, static_cast<int32>(VoxMaterial.type));

	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	VoxAssets::Finish(Instance, bCreated);
	return Instance;
}
