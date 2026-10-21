// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxScene.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxScene, Log, All)

#define LOCTEXT_NAMESPACE "VoxScene"

FVoxScene::~FVoxScene()
{
	if (Scene)
	{
		v4c_free_scene(Scene);
		Scene = nullptr;
	}
}

bool FVoxScene::Load(const uint8* Data, int64 Size, const FVoxLoadParams& Params, FText& OutError)
{
	if (Scene)
	{
		v4c_free_scene(Scene);
		Scene = nullptr;
	}

	if (!Data || Size <= 0)
	{
		OutError = LOCTEXT("EmptyFile", "The file is empty.");
		return false;
	}

	v4c_load_options Options;
	v4c_default_load_options(&Options);

	Options.flags = V4C_FLAG_USED_MATERIALS_ONLY;
	if (Params.bGenerateMeshes)
	{
		// vox4c winds triangles counter-clockwise around the outward normal by default;
		// Unreal computes face normals as (P2 - P0) x (P1 - P0), so it needs the opposite order.
		Options.flags |= V4C_FLAG_GENERATE_MESHES | V4C_FLAG_CLOCKWISE_WINDING;
	}
	if (!Params.bKeepVoxels)
	{
		Options.flags |= V4C_FLAG_DISCARD_VOXELS;
	}

	Options.frame = V4C_ALL_FRAMES;
	Options.scale = FMath::Max(Params.Scale, 0.001f);

	if (Params.bXForward)
	{
		// Unreal: X forward, Y right, Z up. out = (-y, -x, z), the model's front looks at +X.
		Options.coord_system = V4C_COORDS_UNREAL;
	}
	else
	{
		// Mirror X only (MagicaVoxel is right handed, Unreal left handed); the front keeps facing -Y.
		Options.coord_system = V4C_COORDS_CUSTOM;
		Options.custom_axes[0] = V4C_AXIS_NEG_X;
		Options.custom_axes[1] = V4C_AXIS_POS_Y;
		Options.custom_axes[2] = V4C_AXIS_POS_Z;
	}

	switch (Params.Pivot)
	{
	case EVoxPivot::Corner:       Options.pivot = V4C_PIVOT_CORNER; break;
	case EVoxPivot::Center:       Options.pivot = V4C_PIVOT_CENTER; break;
	case EVoxPivot::BottomCenter: Options.pivot = V4C_PIVOT_BOTTOM_CENTER; break;
	case EVoxPivot::MagicaVoxel:
	default:                      Options.pivot = V4C_PIVOT_MAGICAVOXEL; break;
	}

	const v4c_result Result = v4c_load_memory(Data, static_cast<size_t>(Size), &Options, &Scene);
	if (Result != V4C_SUCCESS)
	{
		OutError = GetResultText(Result);
		Scene = nullptr;
		return false;
	}

	UE_LOG(LogVoxScene, Verbose, TEXT("Loaded vox version %u: %u models, %u materials, %u nodes, %u frames"),
		Scene->file_version, Scene->num_models, Scene->num_materials, Scene->num_nodes, Scene->num_frames);
	return true;
}

FText FVoxScene::GetResultText(v4c_result Result)
{
	switch (Result)
	{
	case V4C_SUCCESS:                return LOCTEXT("Success", "No error.");
	case V4C_ERROR_INVALID_ARGUMENT: return LOCTEXT("InvalidArgument", "Invalid load options.");
	case V4C_ERROR_FILE_IO:          return LOCTEXT("FileIO", "The file could not be read.");
	case V4C_ERROR_OUT_OF_MEMORY:    return LOCTEXT("OutOfMemory", "Out of memory.");
	case V4C_ERROR_INVALID_FORMAT:   return LOCTEXT("InvalidFormat", "This is not a MagicaVoxel .vox file.");
	case V4C_ERROR_CORRUPTED_DATA:   return LOCTEXT("CorruptedData", "The file is damaged or truncated.");
	case V4C_ERROR_FRAME_NOT_FOUND:  return LOCTEXT("FrameNotFound", "The animation has no such frame.");
	default:                         return FText::FromString(UTF8_TO_TCHAR(v4c_result_string(Result)));
	}
}

bool FVoxScene::BakeFrame(uint32 Frame, bool bIncludeHidden, FVoxBakedMesh& Out) const
{
	v4c_free_mesh(&Out.Mesh);
	FMemory::Memzero(Out.Mesh);
	if (!Scene)
	{
		return false;
	}
	// Mirrored instances are re-wound by vox4c, so the whole mesh is clockwise like the model meshes.
	const v4c_result Result = v4c_bake_frame(Scene, static_cast<float>(Frame), V4C_SAMPLE_MAGICAVOXEL, !bIncludeHidden,
		V4C_FLAG_CLOCKWISE_WINDING, &Out.Mesh);
	if (Result != V4C_SUCCESS)
	{
		UE_LOG(LogVoxScene, Warning, TEXT("Baking frame %u failed: %s"), Frame, *GetResultText(Result).ToString());
		return false;
	}
	return Out.Mesh.num_indices > 0;
}

void FVoxScene::GetFrameInstances(uint32 Frame, bool bIncludeHidden, TArray<v4c_instance>& OutInstances) const
{
	OutInstances.Reset();
	if (!Scene)
	{
		return;
	}
	// First call sizes the array, second fills it.
	const uint32 Count = v4c_frame_instances(Scene, static_cast<float>(Frame), V4C_SAMPLE_MAGICAVOXEL, !bIncludeHidden, nullptr, 0);
	OutInstances.SetNumZeroed(static_cast<int32>(Count));
	if (Count > 0)
	{
		v4c_frame_instances(Scene, static_cast<float>(Frame), V4C_SAMPLE_MAGICAVOXEL, !bIncludeHidden, OutInstances.GetData(), Count);
	}
}

uint32 FVoxScene::GetAnimationFirstFrame() const
{
	return Scene ? Scene->anim_first : 0;
}

uint32 FVoxScene::GetAnimationLastFrame() const
{
	return Scene ? FMath::Max(Scene->anim_first, Scene->anim_last) : 0;
}

bool FVoxScene::HasAnimation() const
{
	if (!Scene)
	{
		return false;
	}
	for (uint32 Node = 0; Node < Scene->num_nodes; ++Node)
	{
		const v4c_node& NodeData = Scene->nodes[Node];
		if (NodeData.num_transform_keys > 1 || NodeData.num_model_keys > 1)
		{
			return true;
		}
	}
	return false;
}

uint32 FVoxScene::ClampFrame(int32 Frame) const
{
	// Up to the last keyframe or the end of the editor's animation range, whichever is later.
	const int32 LastKey = Scene && Scene->num_frames > 0 ? static_cast<int32>(Scene->num_frames) - 1 : 0;
	const int32 LastFrame = FMath::Max(LastKey, static_cast<int32>(GetAnimationLastFrame()));
	return static_cast<uint32>(FMath::Clamp(Frame, 0, LastFrame));
}

void FVoxScene::GetModelNodes(uint32 Frame, bool bIncludeHidden, TArray<uint32>& OutNodes) const
{
	OutNodes.Reset();
	if (!Scene)
	{
		return;
	}

	for (uint32 Node = 0; Node < Scene->num_nodes; ++Node)
	{
		if (v4c_sample_node_model(Scene, Node, Frame) < 0)
		{
			continue;
		}
		if (!bIncludeHidden && !v4c_node_is_visible(Scene, Node))
		{
			continue;
		}
		OutNodes.Add(Node);
	}
}

FColor FVoxScene::GetColor(uint32 Material) const
{
	if (!Scene || Material >= Scene->num_materials)
	{
		return FColor::White;
	}
	const v4c_color& Color = Scene->materials[Material].color;
	return FColor(Color.r, Color.g, Color.b, Color.a);
}

FVector2f FVoxScene::GetPaletteUV(uint32 Material) const
{
	const float Width = Scene && Scene->num_materials > 0 ? static_cast<float>(Scene->num_materials) : 1.0f;
	return FVector2f((static_cast<float>(Material) + 0.5f) / Width, 0.5f);
}

FName FVoxScene::GetMaterialSlotName(uint32 Material) const
{
	const uint32 PaletteIndex = Scene && Material < Scene->num_materials ? Scene->materials[Material].palette_index : Material;
	return FName(*FString::Printf(TEXT("VoxMaterial_%u"), PaletteIndex));
}

FTransform FVoxScene::ToTransform(const v4c_transform& Transform)
{
	// vox4c: p' = rotate(quaternion) * diag(scale) * p + translation, which is exactly the
	// scale -> rotation -> translation order of FTransform.
	const FQuat Rotation(Transform.quaternion[0], Transform.quaternion[1], Transform.quaternion[2], Transform.quaternion[3]);
	return FTransform(Rotation.GetNormalized(), ToVector(Transform.translation), ToVector(Transform.scale));
}

#undef LOCTEXT_NAMESPACE
