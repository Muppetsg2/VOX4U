# VOX4U

Import [MagicaVoxel](https://ephtracy.github.io/index.html?page=mv_main) `.vox` format file to [Unreal Engine 5](https://www.unrealengine.com/).

Original Creator Repositorium: [VOX4U Original](https://github.com/mik14a/VOX4U)

Tested with **Unreal Engine 5.5.4**

## Description

Import voxel objects to **StaticMesh** or **Voxel** — no need to use any other digital content creation tools. During import, you can also scale the object to match your desired scene size or proportions.

### Static Mesh

![StaticMesh](./Resources/StaticMesh.png)

Generate a static mesh from voxel data provides flexible material handling options:
- Single-material mode – uses one material with a texture generated from the color palette
- Multi-material mode – preserves individual MagicaVoxel material properties, with the additional option to replace the standard Color Node with a texture generated from the palette.

#### Mesh optimized

![Optimized](./Resources/OptimizedMesh.png)

Mesh generation use [a monotone decomposition algorithm](https://0fps.net/2012/07/07/meshing-minecraft-part-2/).

### Voxel

![Voxel](./Resources/VoxelMesh.png)

Generate a voxel asset based on an Instanced Static Mesh Component, and automatically create a cube mesh and materials based on the palette. Each cube uses a different material, preserving MagicaVoxel material properties. Optionally, the Color Node in the material can be replaced with a texture generated from the palette.

If runtime access to the Voxel Actor is not required, the runtime module can be removed from the `.uplugin` file, allowing packaging without the runtime module.

## VOX Format

Supported version: **200 and below**

**Supported chunks:**
- All basic chunks
- MATL

**Unsupported chunks:**
- rCAM
- rOBJ
- IMAP
- nTRN
- nGRP
- nSHP
- LAYR
- NOTE

**Supported materials:**
- Diffuse
- Metal
- Glass
- Emissive

## Usage

Drag & Drop vox file to content panel or open import dialog and select
MagicaVoxel(*.vox) files.

## Install

```sh
cd {YourUnrealProject}
git clone https://github.com/mik14a/VOX4U.git ./Plugins/VOX4U
```

Or add submodule to your project.

```sh
cd {YourUnrealProject}
git submodule add https://github.com/mik14a/VOX4U.git ./Plugins/VOX4U
```
If create package without c++ access. Copy to `Engine/Plugins/Runtime` directory.

## Debugging

If you experience build issues with the plugin, use the [UnrealEnginePluginMigrationTool](https://github.com/mickexd/UnrealEnginePluginMigrationTool) to rebuild it and review the console output for detailed error messages.

## License

[MIT License](https://github.com/Muppetsg2/VOX4U/blob/master/LICENSE)