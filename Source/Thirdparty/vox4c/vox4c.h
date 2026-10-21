/*
    vox4c.h - v1.1.0 - single-header MagicaVoxel (.vox) loader for C99 / C++

    Features
      * Models (SIZE/XYZI), palette (RGBA), materials (MATL, legacy MATT)
      * Full scene graph (nTRN/nGRP/nSHP) collapsed into an engine friendly node tree
      * Files of every MagicaVoxel version (0.98 and older to 0.99.7): scene graph
        synthesized like MagicaVoxel does, old material keys and MATT converted
      * Layers (LAYR) with name / colour / hidden flag
      * Render settings (rOBJ: background, sun, sky light, lens) and camera slots (rCAM)
      * Animation: transform and model keyframes (_f), looping (_loop),
        legacy 0.98 PACK animations, the editor's animation range (META);
        optional loading of one single frame; MagicaVoxel or smooth sampling
        at fractional frames, per-frame instance lists and baked frame meshes
      * Optional monotone meshing (0fps "Meshing in Minecraft, part 2")
        with float vertex arrays or 8 byte packed vertices (GPU friendly),
        32 or 16-bit indices, at load time or on demand per model (keep
        only the meshes of the frames you need)
      * Optional "used materials only" palette compaction
      * Output coordinate system selection (Z-up/Y-up, left/right handed, custom)
      * No dependencies apart from the C standard library, no libm required

    Usage
      In exactly ONE .c/.cpp file:

          #define VOX4C_IMPLEMENTATION
          #include "vox4c.h"

      Everywhere else just #include "vox4c.h".

          v4c_load_options opt;
          v4c_default_load_options(&opt);
          opt.flags        |= V4C_FLAG_GENERATE_MESHES | V4C_FLAG_USED_MATERIALS_ONLY;
          opt.coord_system  = V4C_COORDS_Y_UP_RIGHT_HANDED;

          v4c_scene* scene = NULL;
          v4c_result res = v4c_load_file("model.vox", &opt, &scene);
          if (res != V4C_SUCCESS) { printf("%s\n", v4c_result_string(res)); return 1; }
          ...
          v4c_free_scene(scene);

      Animation (load without V4C_FLAG_GENERATE_MESHES to keep only voxels):

          v4c_instance inst[64];
          uint32_t n = v4c_frame_instances(scene, frame, V4C_SAMPLE_SMOOTH, true, inst, 64);
          for (i = 0; i < n && i < 64; ++i) {
              v4c_model* m = &scene->models[inst[i].model];
              if (m->mesh.num_vertices == 0)
                  v4c_build_model_mesh(scene, inst[i].model, V4C_FLAG_16BIT_INDICES);
              draw(&m->mesh, inst[i].matrix, inst[i].mirrored);
          }

    CMake
      vox4c::vox4c  header only, define VOX4C_IMPLEMENTATION in one file
      vox4c::lib    static library with the implementation, no define needed
                    (VOX4C_BUILD_LIBRARY=ON, see README.md)

    Configuration macros (define before including the implementation)
      VOX4C_STATIC                     make all functions static
      V4C_MALLOC / V4C_REALLOC / V4C_FREE   custom allocator (define all three)
      V4C_NO_STDIO                     remove v4c_load_file (no <stdio.h>)

    Conventions (see README.md for details)
      * Matrices are float[16], column-major, column vectors (p' = M * p),
        i.e. the OpenGL / glm layout: element (row r, column c) is m[c * 4 + r].
      * v4c_transform::rotation is a row-major 3x3 matrix, p' = R * p + t.
      * Triangles are wound counter-clockwise around the outward normal
        (cross(v1 - v0, v2 - v0) points outwards) in the numeric output
        coordinates. That is what OpenGL/glTF/Godot/Unity/Unreal expect.
        Use V4C_FLAG_CLOCKWISE_WINDING for the opposite.

    Credits
      * Monotone meshing: Mikola Lysenko, "Meshing in Minecraft, part 2"
        (https://0fps.net/2012/07/07/meshing-minecraft-part-2/) and the
        MonotoneMesh implementation of mik14a's VoxImporter.
      * Scene graph / animation semantics cross-checked against
        opengametools' ogt_vox.h (Justin Paver).

    License: MIT (see end of file)
*/

#ifndef VOX4C_H
#define VOX4C_H

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

/* VOX4C_PREBUILT is defined by the CMake target vox4c::lib (a static library
   that already contains the implementation). */
#if defined(VOX4C_PREBUILT) && defined(VOX4C_STATIC)
#error "vox4c: VOX4C_STATIC cannot be used with the prebuilt library vox4c::lib (link vox4c::vox4c instead)"
#endif

#ifndef VOX4C_API
#ifdef VOX4C_STATIC
#define VOX4C_API static
#else
#define VOX4C_API extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define VOX4C_VERSION_MAJOR 1
#define VOX4C_VERSION_MINOR 1
#define VOX4C_VERSION_PATCH 0

#define V4C_PALETTE_SIZE   256  /* number of palette entries in a .vox file       */
#define V4C_MAX_MODEL_SIZE 256  /* max model extent per axis (voxel coords are u8) */
#define V4C_ALL_FRAMES     (-1) /* v4c_load_options::frame : load every keyframe   */

/* ------------------------------------------------------------------------- */
/* Results                                                                   */
/* ------------------------------------------------------------------------- */

typedef enum v4c_result
{
    V4C_SUCCESS = 0,
    V4C_ERROR_INVALID_ARGUMENT, /* NULL pointer, invalid option value              */
    V4C_ERROR_FILE_IO,          /* file could not be opened / read                 */
    V4C_ERROR_OUT_OF_MEMORY,
    V4C_ERROR_INVALID_FORMAT, /* not a .vox file (bad magic)                     */
    V4C_ERROR_CORRUPTED_DATA, /* truncated chunk, impossible sizes, ...          */
    V4C_ERROR_FRAME_NOT_FOUND /* requested frame is outside the animation range  */
} v4c_result;

/* ------------------------------------------------------------------------- */
/* Load options                                                              */
/* ------------------------------------------------------------------------- */

/* Build a triangle mesh for every model using monotone meshing. */
#define V4C_FLAG_GENERATE_MESHES (1u << 0)
/* Keep only materials referenced by at least one voxel of a loaded model.
   Voxel / mesh material indices are remapped to the compacted list, the
   original palette index is kept in v4c_material::palette_index. Without
   this flag the scene always contains all 256 materials and the material
   index equals the palette index (0 is the unused "empty" slot). */
#define V4C_FLAG_USED_MATERIALS_ONLY (1u << 1)
/* Do not keep the voxel lists (useful when you only want meshes).
   v4c_model::num_voxels is still filled, v4c_model::voxels is NULL. */
#define V4C_FLAG_DISCARD_VOXELS (1u << 2)
/* Emit clockwise triangles (cross(v1 - v0, v2 - v0) points inwards). */
#define V4C_FLAG_CLOCKWISE_WINDING (1u << 3)
/* Store mesh vertices as 8 byte v4c_packed_vertex (v4c_mesh::packed) instead
   of the float arrays (positions / normals / texcoords / materials stay NULL).
   About 4x less vertex memory; decode on the GPU (see README) or with
   v4c_unpack_vertex / v4c_mesh_vertex. Implies V4C_FLAG_GENERATE_MESHES. */
#define V4C_FLAG_PACKED_VERTICES (1u << 4)
/* Store the indices of meshes with at most 65536 vertices as uint16_t
   (v4c_mesh::indices16, v4c_mesh::indices stays NULL); larger meshes keep
   uint32_t indices. Halves the index memory. Read either kind with
   v4c_mesh_index(). */
#define V4C_FLAG_16BIT_INDICES (1u << 5)

/* Signed axis, used for custom coordinate systems. */
typedef enum v4c_axis
{
    V4C_AXIS_POS_X = 0,
    V4C_AXIS_NEG_X,
    V4C_AXIS_POS_Y,
    V4C_AXIS_NEG_Y,
    V4C_AXIS_POS_Z,
    V4C_AXIS_NEG_Z
} v4c_axis;

/* Output coordinate system. MagicaVoxel itself is right handed and Z-up,
   the front of a model faces -Y. Every preset keeps "up" pointing up and
   maps the front of the model onto the conventional front of the target. */
typedef enum v4c_coord_system
{
    /* MagicaVoxel (native), Blender, 3ds Max. out = ( x,  y, z)            */
    V4C_COORDS_Z_UP_RIGHT_HANDED = 0,
    /* Unreal Engine (X forward, Y right). out = (-y, -x, z), front -> +X   */
    V4C_COORDS_Z_UP_LEFT_HANDED,
    /* OpenGL, glTF, Godot, three.js. out = ( x,  z, -y), front -> +Z       */
    V4C_COORDS_Y_UP_RIGHT_HANDED,
    /* Unity. out = (-x, z, -y), front -> +Z                                */
    V4C_COORDS_Y_UP_LEFT_HANDED,
    /* Use v4c_load_options::custom_axes                                    */
    V4C_COORDS_CUSTOM
} v4c_coord_system;

#define V4C_COORDS_MAGICAVOXEL V4C_COORDS_Z_UP_RIGHT_HANDED
#define V4C_COORDS_BLENDER     V4C_COORDS_Z_UP_RIGHT_HANDED
#define V4C_COORDS_UNREAL      V4C_COORDS_Z_UP_LEFT_HANDED
#define V4C_COORDS_OPENGL      V4C_COORDS_Y_UP_RIGHT_HANDED
#define V4C_COORDS_GLTF        V4C_COORDS_Y_UP_RIGHT_HANDED
#define V4C_COORDS_GODOT       V4C_COORDS_Y_UP_RIGHT_HANDED
#define V4C_COORDS_UNITY       V4C_COORDS_Y_UP_LEFT_HANDED

/* Where the origin of a generated mesh (and of v4c_model::pivot) lies. */
typedef enum v4c_pivot
{
    /* MagicaVoxel pivot, floor(size / 2). Scene transforms place meshes
       directly (v4c_model::scene_offset is zero). Default. */
    V4C_PIVOT_MAGICAVOXEL = 0,
    /* Minimum corner of the model's bounding box. */
    V4C_PIVOT_CORNER,
    /* Exact centre of the model's bounding box. */
    V4C_PIVOT_CENTER,
    /* Centre on the horizontal axes, bottom on the up axis (MagicaVoxel Z). */
    V4C_PIVOT_BOTTOM_CENTER
} v4c_pivot;

typedef struct v4c_load_options
{
    uint32_t flags;                  /* V4C_FLAG_*                                      */
    int32_t  frame;                  /* V4C_ALL_FRAMES or frame index to load, up to    */
                                     /* the last keyframe or the end of the animation   */
                                     /* range, whichever is later                       */
    v4c_coord_system coord_system;   /* output coordinate system                        */
    v4c_axis         custom_axes[3]; /* V4C_COORDS_CUSTOM: output x/y/z = signed source */
                                     /* (MagicaVoxel) axis, e.g. {+X, +Z, -Y}           */
    float     scale;                 /* size of one voxel in output units (> 0)         */
    v4c_pivot pivot;                 /* mesh / model origin                             */
} v4c_load_options;

/* ------------------------------------------------------------------------- */
/* Scene data                                                                */
/* ------------------------------------------------------------------------- */

typedef struct v4c_color
{
    uint8_t r, g, b, a;
} v4c_color;

/* Material types of MagicaVoxel's renderer. A MATL chunk without _type (or
   with an unknown one) is diffuse. */
typedef enum v4c_material_type
{
    V4C_MATERIAL_DIFFUSE = 0, /* _diffuse                                           */
    V4C_MATERIAL_METAL,       /* _metal                                             */
    V4C_MATERIAL_GLASS,       /* _glass  : transparent, refracting surface          */
    V4C_MATERIAL_EMIT,        /* _emit   : light source                             */
    V4C_MATERIAL_BLEND,       /* _blend  : mix of diffuse / metal / glass (_metal,  */
                              /*           _trans, _sp, ... used together)          */
    V4C_MATERIAL_MEDIA        /* _media  : participating medium ("cloud"), no       */
                              /*           surface, see media_type / density / phase */
} v4c_material_type;

/* Volumetric medium inside glass, blend and media materials. */
typedef enum v4c_media_type
{
    V4C_MEDIA_ABSORB = 0, /* default (no _media_type key), _media = 0           */
    V4C_MEDIA_SCATTER,    /* _media_type = _scatter, _media = 1                 */
    V4C_MEDIA_EMIT,       /* _media_type = _emit,    _media = 2                 */
    V4C_MEDIA_SSS         /* _media_type = _sss,     _media = 3 (subsurface)    */
} v4c_media_type;

/* All values are read as stored in the file. MagicaVoxel omits keys whose
   value is at its default, the defaults below are used for missing keys. */
typedef struct v4c_material
{
    v4c_color         color;         /* palette colour                                        */
    uint8_t           palette_index; /* index in the original 256 colour palette              */
    bool              plastic;       /* older _plastic / _type _plastic        default false   */
    v4c_material_type type;          /* _type                                  default diffuse */
    float             roughness;     /* _rough                                 default 0.1     */
    float             metallic;      /* _metal (older: _weight of a metal)     default 0       */
    float             specular;      /* _sp (older metal: _spec + 1)           default 1       */
    float             ior;           /* index of refraction:                   default 1.3     */
                                     /* _ior + 1 || _ri (the file's _ior is n - 1)             */
    float          transparency;     /* _trans || _alpha (older: _weight)      default 0       */
    float          emissive;         /* _emit (older: _weight), strength       default 0       */
    float          emission_power;   /* _flux  (power exponent)                default 0       */
    float          ldr;              /* _ldr   (low dynamic range emission)    default 0       */
    v4c_media_type media_type;       /* _media_type || _media                  default absorb  */
    float          density;          /* _d (medium density; 0 if left out)     default 0.05    */
    float          phase;            /* _g     (scattering anisotropy,         default 0       */
                                     /*         Henyey-Greenstein g, -1..1)                    */
                                     /* older files are converted like MagicaVoxel 0.99.7     */
                                     /* does it, see docs/vox-file-format.md, part II          */
    /* raw values of keys written only by older MagicaVoxel versions; the ones
       with a current equivalent are also converted into the fields above */
    float weight;      /* _weight                                default 1       */
    float attenuation; /* _att                                   default 1       */
} v4c_material;

/* One voxel in output coordinates: 0 <= x < size[0] etc. */
typedef struct v4c_voxel
{
    uint8_t x, y, z, material;
} v4c_voxel;

/* Compact mesh vertex (V4C_FLAG_PACKED_VERTICES), 8 bytes, no padding:
     offset 0  uint16 x, y, z   voxel-grid corner, 0..256, output axes
     offset 6  uint8  normal    v4c_axis: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z
     offset 7  uint8  material  index into v4c_scene::materials
   Decoding (identical to the float arrays of the unpacked mesh):
     position = (xyz - model->pivot) * scene->scale
     normal   = unit vector of the signed axis
     texcoord = ((material + 0.5) / scene->num_materials, 0.5)            */
typedef struct v4c_packed_vertex
{
    uint16_t x, y, z;
    uint8_t  normal;
    uint8_t  material;
} v4c_packed_vertex;

/* Range of mesh indices that share one material. */
typedef struct v4c_submesh
{
    uint32_t material;    /* index into v4c_scene::materials */
    uint32_t first_index; /* offset into v4c_mesh::indices (or indices16) */
    uint32_t num_indices;
} v4c_submesh;

typedef struct v4c_mesh
{
    /* arrays first, counts last: no padding on 64-bit targets */
    float* positions;             /* 3 floats per vertex, relative to the model pivot       */
    float* normals;               /* 3 floats per vertex (flat, axis aligned)               */
    float* texcoords;             /* 2 floats per vertex, samples the palette texture       */
                                  /* (width = num_materials, height = 1, see                */
                                  /* v4c_palette_texture)                                   */
    uint8_t*           materials; /* 1 material index per vertex                            */
    v4c_packed_vertex* packed;    /* V4C_FLAG_PACKED_VERTICES only, the four arrays above   */
                                  /* are NULL then                                          */
    uint32_t*    indices;         /* 3 per triangle, sorted by material                     */
    uint16_t*    indices16;       /* V4C_FLAG_16BIT_INDICES: used instead of indices        */
    v4c_submesh* submeshes;       /* one entry per material used by this mesh               */
    uint32_t     num_vertices;
    uint32_t     num_indices;
    uint32_t     num_submeshes;
    bool         clockwise; /* triangles wound clockwise (V4C_FLAG_CLOCKWISE_WINDING)  */
} v4c_mesh;

typedef struct v4c_model
{
    uint32_t size[3];      /* extent in voxels, output axes                          */
    float    pivot[3];     /* mesh origin in voxel-grid units (grid corner = 0)      */
                           /* mesh position = (grid position - pivot) * scale        */
    float scene_offset[3]; /* translation from mesh space into the node's space.     */
                           /* Zero for V4C_PIVOT_MAGICAVOXEL.                        */
    uint32_t   num_voxels;
    uint32_t   file_index; /* index of the model inside the .vox file                */
    v4c_voxel* voxels;     /* NULL with V4C_FLAG_DISCARD_VOXELS                      */
    v4c_mesh   mesh;       /* empty unless V4C_FLAG_GENERATE_MESHES                  */
} v4c_model;

typedef struct v4c_transform
{
    float translation[3];
    float rotation[9];   /* row-major 3x3, p' = R * p + t. Signed permutation that may  */
                         /* contain a reflection (MagicaVoxel allows mirroring).        */
    float quaternion[4]; /* x, y, z, w - proper rotation part                           */
    float scale[3];      /* +-1: rotation = rotate(quaternion) * diag(scale)            */
} v4c_transform;

typedef struct v4c_transform_key
{
    uint32_t      frame;
    v4c_transform transform;
} v4c_transform_key;

typedef struct v4c_model_key
{
    uint32_t frame;
    uint32_t model;
} v4c_model_key;

/* Scene graph node. MagicaVoxel's Transform -> (Group | Shape) chains are
   collapsed, so every node is "a transform with children and/or a model",
   which maps 1:1 onto engine entities / game objects / actors.
   Nodes are stored in pre-order: a parent always has a lower index than its
   children, so a single forward loop can create entities parent-first. */
typedef struct v4c_node
{
    char*              name;           /* NULL if unnamed                               */
    uint32_t*          children;       /* indices into v4c_scene::nodes                 */
    v4c_transform_key* transform_keys; /* >= 1, sorted by frame                         */
    v4c_model_key*     model_keys;     /* none = node has no model, sorted by frame     */
    int32_t            parent;         /* index into v4c_scene::nodes, -1 for the root  */
    int32_t            layer;          /* index into v4c_scene::layers or -1            */
    uint32_t           num_children;
    uint32_t           num_transform_keys;
    uint32_t           num_model_keys;
    bool               hidden;
    bool               transform_loop; /* _loop of the transform animation              */
    bool               model_loop;     /* _loop of the model animation                  */
} v4c_node;

typedef struct v4c_layer
{
    char*     name; /* NULL if unnamed */
    v4c_color color;
    int32_t   id; /* layer id stored in the file */
    bool      hidden;
} v4c_layer;

/* ---- Render settings (rOBJ chunks) ---------------------------------------

   MagicaVoxel stores the settings of its renderer next to the scene. vox4c
   reads the ones that describe lighting and the look of the image; all
   positions and directions are converted to the output coordinate system.  */

typedef enum v4c_light_type
{
    V4C_LIGHT_DIRECTIONAL = 0, /* _area == 0: parallel rays, sharp shadows              */
    V4C_LIGHT_AREA             /* _area  > 0: a distant disk, soft shadows              */
} v4c_light_type;

/* The sun as a disk light far away (V4C_LIGHT_AREA). Its power is chosen so
   that it lights the target with the same irradiance as the directional light
   of the same `intensity` would (exact on the disk axis). All photometric
   values use the units of v4c_sun::intensity, scale them together. */
typedef struct v4c_area_light
{
    float position[3]; /* centre of the emitting disk                                   */
    float normal[3];   /* the emitting side faces the scene (= v4c_sun::direction)       */
    float radius;      /* disk radius                                                    */
    float distance;    /* from the target to the centre of the disk                     */
    float power;       /* total emitted flux (one-sided Lambertian disk)                 */
    float intensity;   /* radiant intensity along the normal, = power / pi               */
    float radiance;    /* emitted radiance, = power / (pi * disk area)                   */
} v4c_area_light;

/* rOBJ _type = _inf: MagicaVoxel's sun. */
typedef struct v4c_sun
{
    bool           present;    /* false if the file has no sun settings            */
    bool           disk;       /* _disk: the sun disk is visible in the sky         */
    v4c_light_type type;       /* DIRECTIONAL when area == 0, AREA otherwise        */
    v4c_color      color;      /* _k, 0..255 (alpha 255)                            */
    float          intensity;  /* _i, use as the directional light intensity        */
    float          elevation;  /* _angle[0], degrees above the horizon              */
    float          azimuth;    /* _angle[1], degrees: 0 front, 90 right, 180 back,  */
                               /* 270 left (MagicaVoxel's front is -Y)              */
    float area;                /* _area, as stored                                  */
    float angular_diameter;    /* apparent size of the sun in degrees, area * 43.5  */
                               /* (checked against the penumbra of a render)        */
    float direction[3];        /* unit vector in which the light travels (from the  */
                               /* sun towards the scene), output axes               */
    v4c_area_light area_light; /* AREA only: placed at 100x the scene's bounding    */
                               /* radius around its centre (loaded frame, or frame  */
                               /* 0), see v4c_sun_area_light for other placements  */
} v4c_sun;

/* Lens of the MagicaVoxel renderer (_lens _proj). Verified with renders:
     PERSPECTIVE    pinhole; fov is the vertical field of view
     STEREOGRAPHIC  r = tan(theta / 2) / tan(fov / 4) in units of half the image
                    height (theta: angle from the viewing direction), so fov is
                    again the vertical field of view; covers almost the whole sphere
     PANORAMA       equirectangular, the viewing direction in the centre: 180
                    degrees over the image height and the same degrees per
                    pixel horizontally (360 degrees for a 2:1 image, 180 for a
                    square one, wider images repeat the directions every
                    2 * height pixels); fov is not used
   See v4c_camera_ray / v4c_camera_project. */
typedef enum v4c_projection
{
    V4C_PROJECTION_PERSPECTIVE   = 0,
    V4C_PROJECTION_STEREOGRAPHIC = 1,
    V4C_PROJECTION_PANORAMA      = 2,
    V4C_PROJECTION_UNKNOWN       = 3
} v4c_projection;

typedef struct v4c_render_settings
{
    v4c_sun        sun;               /* _inf                                              */
    v4c_color      ambient_color;     /* _uni: uniform sky light (_k)                      */
    v4c_color      background;        /* _bg: background colour (_color)                   */
    float          ambient_intensity; /* _uni: _i                                          */
    float          fov;               /* _lens / _len: field of view of the render, degrees */
    float          aperture;          /* _lens _aperture / _len _dof: depth of field       */
    v4c_projection projection;        /* _lens _proj (_len: _sg = stereographic)           */
    int32_t        projection_index;  /* _proj as stored                                   */
    bool           has_ambient;
    bool           has_background;  /* _bg: a background colour is stored                */
    bool           show_background; /* _setting _bg_c (_bg _enable): the render shows it */
    bool           has_lens;
} v4c_render_settings;

/* ---- Cameras (rCAM chunks) -------------------------------------------------

   The camera slots of MagicaVoxel (up to 10). A slot the user never saved a
   view into is stored with default values and has `saved == false`. The live
   view of the editor is not stored in the file, only these slots. */

typedef enum v4c_camera_mode
{
    V4C_CAMERA_PERSPECTIVE = 0, /* pers */
    V4C_CAMERA_FREE,            /* free */
    V4C_CAMERA_PANORAMA,        /* pano */
    V4C_CAMERA_ORTHOGRAPHIC,    /* orth */
    V4C_CAMERA_ISOMETRIC,       /* iso  */
    V4C_CAMERA_UNKNOWN
} v4c_camera_mode;

typedef struct v4c_camera
{
    float focus[3];     /* point the camera orbits around / looks at (output coords) */
    float angle[3];     /* _angle as stored, degrees: pitch (negative looks down),   */
                        /* yaw (positive orbits to the right), a third value that    */
                        /* is not a roll and is not used by v4c_camera_pose          */
    float radius;       /* distance from focus to the camera (scaled)                */
    float frustum;      /* perspective / free: tan(fov / 2);                         */
                        /* orthographic / isometric: half of the view height at the  */
                        /* focus (scaled)                                             */
    float fov;          /* vertical field of view saved with the slot, degrees; a    */
                        /* render uses scene->render.fov                              */
    uint32_t        id; /* slot index                                                */
    v4c_camera_mode mode;
    bool            saved; /* false for an untouched slot (all values default)         */
} v4c_camera;

typedef struct v4c_scene
{
    v4c_model*    models;
    v4c_material* materials;
    v4c_node*     nodes; /* nodes[0] is the root when num_nodes > 0             */
    v4c_layer*    layers;
    v4c_camera*   cameras; /* rCAM slots, sorted by id                            */
    uint32_t      num_models;
    uint32_t      num_materials; /* 256, or the number of used materials                */
    uint32_t      num_nodes;
    uint32_t      num_layers;
    uint32_t      num_cameras;

    uint32_t file_version;
    uint32_t num_frames;   /* last keyframe + 1, >= 1                             */
    uint32_t anim_first;   /* animation range of the editor's timeline (META      */
    uint32_t anim_last;    /* _anim_range); without it MagicaVoxel's default      */
                           /* 0 .. 30 (or up to the last keyframe if later)       */
    int32_t  loaded_frame; /* V4C_ALL_FRAMES or the single loaded frame           */
    float    scale;
    v4c_axis axes[3];     /* mapping used: output axis i = axes[i] of MagicaVoxel */
    bool     left_handed; /* output coordinate system handedness                 */

    v4c_render_settings render; /* background, sun, ambient light, lens (rOBJ)         */
} v4c_scene;

/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */

VOX4C_API void       v4c_default_load_options(v4c_load_options* options);
VOX4C_API v4c_result v4c_load_memory(const void* data, size_t size, const v4c_load_options* options,
                                     v4c_scene** out_scene);
#ifndef V4C_NO_STDIO
VOX4C_API v4c_result v4c_load_file(const char* path, const v4c_load_options* options, v4c_scene** out_scene);
#endif
VOX4C_API void        v4c_free_scene(v4c_scene* scene);
VOX4C_API const char* v4c_result_string(v4c_result result);

/* Animation sampling, MagicaVoxel semantics: rotations and models snap to the
   previous keyframe, translations are interpolated linearly and truncated to
   whole voxels, _loop wraps the frame index into the node's keyframe range. */
VOX4C_API v4c_transform v4c_sample_node_transform(const v4c_scene* scene, uint32_t node, uint32_t frame);
VOX4C_API int32_t       v4c_sample_node_model(const v4c_scene* scene, uint32_t node, uint32_t frame); /* -1 = none */

/* How to sample an animation at a fractional frame.
   V4C_SAMPLE_MAGICAVOXEL  as MagicaVoxel plays it: the frame is rounded down
                           and sampled like v4c_sample_node_transform().
   V4C_SAMPLE_SMOOTH       for playback in an engine: translations are
                           interpolated without rounding to whole voxels,
                           rotations are interpolated (slerp) between keys,
                           a looping node blends its last key back into the
                           first. Mirroring (scale) still snaps, and models
                           (frame-by-frame animation) always snap. */
typedef enum v4c_sampling
{
    V4C_SAMPLE_MAGICAVOXEL = 0,
    V4C_SAMPLE_SMOOTH
} v4c_sampling;

/* Local transform of a node at a fractional frame. */
VOX4C_API v4c_transform v4c_sample_node_transform_at(const v4c_scene* scene, uint32_t node, float frame,
                                                     v4c_sampling sampling);

/* Matrices (column-major float[16]). */
VOX4C_API void v4c_transform_to_matrix(const v4c_transform* transform, float out[16]);
VOX4C_API void v4c_node_local_matrix(const v4c_scene* scene, uint32_t node, uint32_t frame, float out[16]);
VOX4C_API void v4c_node_world_matrix(const v4c_scene* scene, uint32_t node, uint32_t frame, float out[16]);
/* World matrix including the model's scene_offset: multiply mesh vertices by
   this to place them in the world. Returns false if the node has no model. */
VOX4C_API bool v4c_node_model_matrix(const v4c_scene* scene, uint32_t node, uint32_t frame, float out[16]);

/* false if the node, one of its ancestors or their layers are hidden. */
VOX4C_API bool v4c_node_is_visible(const v4c_scene* scene, uint32_t node);

/* One model drawn in a frame: everything needed to render it. */
typedef struct v4c_instance
{
    float    matrix[16]; /* mesh space -> world (as v4c_node_model_matrix)     */
    uint32_t node;
    uint32_t model;    /* index into scene->models                             */
    bool     mirrored; /* the matrix mirrors: flip the triangle winding        */
} v4c_instance;

/* The models drawn at `frame`, in node order (parents first). Writes up to
   `capacity` instances to `out` (may be NULL) and returns how many there
   are, so a first call with capacity 0 sizes the array. */
VOX4C_API uint32_t v4c_frame_instances(const v4c_scene* scene, float frame, v4c_sampling sampling, bool visible_only,
                                       v4c_instance* out, uint32_t capacity);

/* Meshes on demand. Builds model->mesh from model->voxels (so the scene must
   be loaded without V4C_FLAG_DISCARD_VOXELS), replacing a mesh it already
   has; `flags` takes V4C_FLAG_PACKED_VERTICES, V4C_FLAG_CLOCKWISE_WINDING and
   V4C_FLAG_16BIT_INDICES. The result equals a mesh generated while loading.
   Load an animation without V4C_FLAG_GENERATE_MESHES and build (and free)
   the meshes of the frames you need to keep only those in memory. */
VOX4C_API v4c_result v4c_build_model_mesh(v4c_scene* scene, uint32_t model, uint32_t flags);
VOX4C_API void       v4c_free_model_mesh(v4c_scene* scene, uint32_t model);

/* Bakes all models drawn at `frame` into one mesh in world space (float
   arrays, one submesh per material). Models without a mesh are meshed
   temporarily from their voxels (V4C_ERROR_INVALID_ARGUMENT if they were
   discarded). `flags` takes V4C_FLAG_CLOCKWISE_WINDING and
   V4C_FLAG_16BIT_INDICES. Free the result with v4c_free_mesh(). */
VOX4C_API v4c_result v4c_bake_frame(const v4c_scene* scene, float frame, v4c_sampling sampling, bool visible_only,
                                    uint32_t flags, v4c_mesh* out);
VOX4C_API void       v4c_free_mesh(v4c_mesh* mesh);

/* Index `i` of a mesh, whether stored as 32 or 16 bits. */
VOX4C_API uint32_t v4c_mesh_index(const v4c_mesh* mesh, uint32_t i);

/* Axis aligned bounds (output coordinates) of the models placed in the scene
   at `frame`, optionally only of visible nodes. Returns false if there is no
   model (min / max are then zero). Uses the model extents, meshes not needed. */
VOX4C_API bool v4c_scene_bounds(const v4c_scene* scene, uint32_t frame, bool visible_only, float min[3], float max[3]);

/* Places scene->render.sun as a disk light `distance` away from `target`
   (output coordinates), see v4c_area_light. Returns false if the file has no
   sun or its area is 0 (a directional light has no size). */
VOX4C_API bool v4c_sun_area_light(const v4c_scene* scene, const float target[3], float distance, v4c_area_light* out);

/* Position, viewing direction and up vector (output coordinates, unit
   vectors) of a camera slot: use them with any look-at function, together
   with camera->fov. Any output pointer may be NULL. */
VOX4C_API void v4c_camera_pose(const v4c_scene* scene, const v4c_camera* camera, float position[3], float forward[3],
                               float up[3]);

/* Primary ray of pixel (px, py) of a width x height image rendered from a
   camera slot the way MagicaVoxel renders it. Pixel coordinates are
   continuous, (0, 0) is the top left corner of the image and the centre of
   pixel (i, j) is (i + 0.5, j + 0.5). `projection` and `fov` (degrees) are
   usually scene->render.projection / scene->render.fov. Orthographic and
   isometric slots give parallel rays (origin varies, frustum = half height),
   the other modes start all rays at the camera position. `dir` is a unit
   vector; returns false for a pixel without a ray (outside a panorama). */
VOX4C_API bool v4c_camera_ray(const v4c_scene* scene, const v4c_camera* camera, v4c_projection projection, float fov,
                              uint32_t width, uint32_t height, float px, float py, float origin[3], float dir[3]);

/* Inverse of v4c_camera_ray: the pixel position of a point (output
   coordinates); in a panorama wider than 2:1 the copy nearest to the image
   centre. Returns false if the point cannot be seen (behind a
   perspective camera, exactly behind a stereographic one, at the camera). */
VOX4C_API bool v4c_camera_project(const v4c_scene* scene, const v4c_camera* camera, v4c_projection projection,
                                  float fov, uint32_t width, uint32_t height, const float point[3], float pixel[2]);

/* Writes num_materials RGBA8 texels (the palette texture used by texcoords). */
VOX4C_API void v4c_palette_texture(const v4c_scene* scene, uint8_t* out_rgba);

/* Decodes a packed vertex of `model`. Any output pointer may be NULL. */
VOX4C_API void v4c_unpack_vertex(const v4c_scene* scene, const v4c_model* model, const v4c_packed_vertex* vertex,
                                 float position[3], float normal[3], float texcoord[2]);
/* Reads vertex `index` of model->mesh in either format (packed or not).
   Any output pointer may be NULL. Returns the material index. */
VOX4C_API uint32_t v4c_mesh_vertex(const v4c_scene* scene, const v4c_model* model, uint32_t index, float position[3],
                                   float normal[3], float texcoord[2]);

#ifdef __cplusplus
}
#endif

#endif /* VOX4C_H */

/* ========================================================================= */
/*                             IMPLEMENTATION                                */
/* ========================================================================= */

#ifdef VOX4C_IMPLEMENTATION
#ifdef VOX4C_PREBUILT
#error \
    "vox4c: this target links the prebuilt library vox4c::lib, which already contains the implementation - remove '#define VOX4C_IMPLEMENTATION' (or link vox4c::vox4c to compile the implementation yourself)"
#endif
#ifndef VOX4C_IMPLEMENTATION_DONE
#define VOX4C_IMPLEMENTATION_DONE

#include <string.h>

#if defined(V4C_MALLOC) && defined(V4C_REALLOC) && defined(V4C_FREE)
/* user allocator */
#elif !defined(V4C_MALLOC) && !defined(V4C_REALLOC) && !defined(V4C_FREE)
#include <stdlib.h>
#define V4C_MALLOC(size)       malloc(size)
#define V4C_REALLOC(ptr, size) realloc(ptr, size)
#define V4C_FREE(ptr)          free(ptr)
#else
#error "vox4c: define all of V4C_MALLOC, V4C_REALLOC and V4C_FREE, or none of them"
#endif

#ifndef V4C_NO_STDIO
#include <stdio.h>
#endif

#define V4C__MAX_NODE_DEPTH 1024
#define V4C__MAX_OUT_NODES  (1u << 22) /* guards against exponential DAG expansion */

/* MagicaVoxel default palette, 0xAABBGGRR, indexed directly by colour index. */
static const uint32_t v4c__default_palette[256] = {
    0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff, 0xffccccff,
    0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff, 0xff6699ff, 0xff3399ff,
    0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff, 0xff0066ff, 0xffff33ff, 0xffcc33ff,
    0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff, 0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff,
    0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc, 0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc,
    0xff99cccc, 0xff66cccc, 0xff33cccc, 0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc,
    0xff0099cc, 0xffff66cc, 0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc,
    0xff9933cc, 0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc,
    0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99, 0xffcccc99,
    0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999, 0xff669999, 0xff339999,
    0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699, 0xff006699, 0xffff3399, 0xffcc3399,
    0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099, 0xffcc0099, 0xff990099, 0xff660099, 0xff330099,
    0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66, 0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66,
    0xff99cc66, 0xff66cc66, 0xff33cc66, 0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966,
    0xff009966, 0xffff6666, 0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366,
    0xff993366, 0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
    0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33, 0xffcccc33,
    0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933, 0xff669933, 0xff339933,
    0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633, 0xff006633, 0xffff3333, 0xffcc3333,
    0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033, 0xffcc0033, 0xff990033, 0xff660033, 0xff330033,
    0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00, 0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00,
    0xff99cc00, 0xff66cc00, 0xff33cc00, 0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900,
    0xff009900, 0xffff6600, 0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300,
    0xff993300, 0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000,
    0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044, 0xff000022,
    0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700, 0xff005500, 0xff004400,
    0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000, 0xff880000, 0xff770000, 0xff550000,
    0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd, 0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777,
    0xff555555, 0xff444444, 0xff222222, 0xff111111
};

/* ------------------------------------------------------------------------- */
/* Small utilities                                                           */
/* ------------------------------------------------------------------------- */

/* Grows *ptr_addr (any T*) to hold at least `need` elements. */
static int v4c__reserve(void* ptr_addr, uint32_t* cap, uint32_t need, size_t elem)
{
    void*    old_ptr;
    void*    new_ptr;
    uint32_t new_cap;
    if (need <= *cap)
        return 1;

    new_cap = *cap < 16u ? 16u : *cap;
    while (new_cap < need)
    {
        if (new_cap > 0x7fffffffu)
        {
            new_cap = need;
            break;
        }
        new_cap *= 2u;
    }
    if ((size_t)new_cap > ((size_t)-1) / elem)
        return 0;

    memcpy(&old_ptr, ptr_addr, sizeof(old_ptr));
    new_ptr = V4C_REALLOC(old_ptr, (size_t)new_cap * elem);
    if (!new_ptr)
        return 0;

    memcpy(ptr_addr, &new_ptr, sizeof(new_ptr));
    *cap = new_cap;
    return 1;
}
#define V4C__RESERVE(ptr, cap, need) v4c__reserve(&(ptr), &(cap), (need), sizeof(*(ptr)))

static void* v4c__calloc(size_t count, size_t elem)
{
    void* p;
    if (count == 0)
        count = 1;

    if (count > ((size_t)-1) / elem)
        return NULL;

    p = V4C_MALLOC(count * elem);
    if (p)
        memset(p, 0, count * elem);

    return p;
}

static float v4c__sqrtf(float x)
{
    float r;
    int   i;
    if (x <= 0.0f)
        return 0.0f;

    r = x > 1.0f ? x : 1.0f;
    for (i = 0; i < 32; ++i)
        r = 0.5f * (r + x / r);

    return r;
}

/* sin / cos without libm: reduction to [-pi/4, pi/4] + Taylor series, ~1e-15 */
static void v4c__sincos(double x, double* out_sin, double* out_cos)
{
    const double  pi_2 = 1.57079632679489661923;
    const double  q    = x / pi_2;
    const int64_t k    = (int64_t)(q >= 0.0 ? q + 0.5 : q - 0.5);
    const double  r = x - (double)k * pi_2, r2 = r * r;
    const double  sn =
        r *
        (1.0 - r2 / 6.0 *
                   (1.0 - r2 / 20.0 * (1.0 - r2 / 42.0 * (1.0 - r2 / 72.0 * (1.0 - r2 / 110.0 * (1.0 - r2 / 156.0))))));
    double cs =
        1.0 -
        r2 / 2.0 *
            (1.0 -
             r2 / 12.0 *
                 (1.0 - r2 / 30.0 * (1.0 - r2 / 56.0 * (1.0 - r2 / 90.0 * (1.0 - r2 / 132.0 * (1.0 - r2 / 182.0))))));
    switch ((int)(k & 3))
    {
    case 0:
        *out_sin = sn;
        *out_cos = cs;
        break;
    case 1:
        *out_sin = cs;
        *out_cos = -sn;
        break;
    case 2:
        *out_sin = -sn;
        *out_cos = -cs;
        break;
    default:
        *out_sin = -cs;
        *out_cos = sn;
        break;
    }
}

static double v4c__deg_to_rad(double d)
{
    return d * (3.14159265358979323846 / 180.0);
}

static double v4c__sqrt(double x)
{
    double r;
    int    i;
    if (!(x > 0.0))
        return 0.0;

    r = x > 1.0 ? x : 1.0;
    for (i = 0; i < 64; ++i)
    {
        const double n = 0.5 * (r + x / r);
        if (n == r)
            break;
        r = n;
    }
    return r;
}

/* atan: three half-angle reductions atan(x) = 2 atan(x / (1 + sqrt(1 + x^2)))
   bring |x| below tan(pi / 32), then a Taylor series (error ~1e-16) */
static double v4c__atan(double x)
{
    double x2, t, sum = 0.0, term;
    int    i, k;
    for (i = 0; i < 3; ++i)
        x = x / (1.0 + v4c__sqrt(1.0 + x * x));

    x2   = x * x;
    term = x;
    for (k = 0; k < 12; ++k)
    {
        sum += term / (double)(2 * k + 1);
        term *= -x2;
    }
    t = sum * 8.0;
    return t;
}

static double v4c__atan2(double y, double x)
{
    const double pi = 3.14159265358979323846;
    if (x > 0.0)
        return v4c__atan(y / x);

    if (x < 0.0)
        return y >= 0.0 ? v4c__atan(y / x) + pi : v4c__atan(y / x) - pi;

    return y > 0.0 ? 0.5 * pi : (y < 0.0 ? -0.5 * pi : 0.0);
}

static float v4c__roundf(float x)
{
    return x >= 0.0f ? (float)(int64_t)(x + 0.5f) : -(float)(int64_t)(-x + 0.5f);
}

typedef struct v4c__str
{
    const char* p;
    uint32_t    n;
} v4c__str;

static int v4c__str_eq(v4c__str s, const char* lit)
{
    const size_t n = strlen(lit);
    return s.n == n && memcmp(s.p, lit, n) == 0;
}

static char* v4c__str_dup(v4c__str s)
{
    char* out;
    if (!s.p || s.n == 0)
        return NULL;

    out = (char*)V4C_MALLOC((size_t)s.n + 1);
    if (out)
    {
        memcpy(out, s.p, s.n);
        out[s.n] = '\0';
    }
    return out;
}

static int v4c__is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Locale independent number parsing (strtod honours LC_NUMERIC, e.g. pl_PL). */
static double v4c__parse_number(v4c__str s, uint32_t* pos, int* ok)
{
    uint32_t i   = *pos;
    double   v   = 0.0;
    int      neg = 0, digits = 0;
    while (i < s.n && v4c__is_space(s.p[i]))
        ++i;

    if (i < s.n && (s.p[i] == '+' || s.p[i] == '-'))
    {
        neg = s.p[i] == '-';
        ++i;
    }

    while (i < s.n && s.p[i] >= '0' && s.p[i] <= '9')
    {
        v = v * 10.0 + (double)(s.p[i] - '0');
        ++i;
        ++digits;
    }

    if (i < s.n && (s.p[i] == '.' || s.p[i] == ','))
    {
        double f = 0.1;
        ++i;
        while (i < s.n && s.p[i] >= '0' && s.p[i] <= '9')
        {
            v += (double)(s.p[i] - '0') * f;
            f *= 0.1;
            ++i;
            ++digits;
        }
    }

    if (digits && i < s.n && (s.p[i] == 'e' || s.p[i] == 'E'))
    {
        int      eneg = 0, e = 0, edigits = 0;
        uint32_t j = i + 1;
        if (j < s.n && (s.p[j] == '+' || s.p[j] == '-'))
        {
            eneg = s.p[j] == '-';
            ++j;
        }

        while (j < s.n && s.p[j] >= '0' && s.p[j] <= '9')
        {
            if (e < 400)
                e = e * 10 + (s.p[j] - '0');
            ++j;
            ++edigits;
        }

        if (edigits)
        {
            i = j;
            while (e-- > 0)
                v = eneg ? v * 0.1 : v * 10.0;
        }
    }
    *pos = i;
    *ok  = digits > 0;
    return neg ? -v : v;
}

static float v4c__parse_float(v4c__str s, float def)
{
    uint32_t pos = 0;
    int      ok;
    double   v = v4c__parse_number(s, &pos, &ok);
    if (!ok)
        return def;

    if (v > 3.0e38)
        v = 3.0e38;

    if (v < -3.0e38)
        v = -3.0e38;

    return (float)v;
}

static int v4c__parse_ints(v4c__str s, int32_t* out, int max_count)
{
    uint32_t pos   = 0;
    int      count = 0;
    while (count < max_count)
    {
        int    ok;
        double v = v4c__parse_number(s, &pos, &ok);
        if (!ok)
            break;

        if (v > 2147483647.0)
            v = 2147483647.0;

        if (v < -2147483648.0)
            v = -2147483648.0;

        out[count++] = (int32_t)v;
    }
    return count;
}

static int v4c__parse_floats(v4c__str s, float* out, int max_count)
{
    uint32_t pos   = 0;
    int      count = 0;
    while (count < max_count)
    {
        int    ok;
        double v = v4c__parse_number(s, &pos, &ok);
        if (!ok)
            break;

        if (v > 3.0e38)
            v = 3.0e38;

        if (v < -3.0e38)
            v = -3.0e38;

        out[count++] = (float)v;
    }
    return count;
}

/* "r g b" -> colour, alpha 255 */
static v4c_color v4c__parse_rgb(v4c__str s, v4c_color def)
{
    int32_t rgb[3];
    int     k;
    if (v4c__parse_ints(s, rgb, 3) != 3)
        return def;

    for (k = 0; k < 3; ++k)
        rgb[k] = rgb[k] < 0 ? 0 : (rgb[k] > 255 ? 255 : rgb[k]);

    def.r = (uint8_t)rgb[0];
    def.g = (uint8_t)rgb[1];
    def.b = (uint8_t)rgb[2];
    def.a = 255;
    return def;
}

static bool v4c__parse_bool(v4c__str s)
{
    int32_t v = 0;
    if (v4c__str_eq(s, "true"))
        return true;

    return (v4c__parse_ints(s, &v, 1) == 1 && v != 0) != 0;
}

/* ------------------------------------------------------------------------- */
/* Binary reader                                                             */
/* ------------------------------------------------------------------------- */

typedef struct v4c__reader
{
    const uint8_t* cur;
    const uint8_t* end;
    int            ok;
} v4c__reader;

static size_t v4c__remaining(const v4c__reader* r)
{
    return (size_t)(r->end - r->cur);
}

static const uint8_t* v4c__read_bytes(v4c__reader* r, size_t n)
{
    const uint8_t* p;
    if (!r->ok || v4c__remaining(r) < n)
    {
        r->ok = 0;
        return NULL;
    }
    p = r->cur;
    r->cur += n;
    return p;
}

static uint32_t v4c__read_u32(v4c__reader* r)
{
    const uint8_t* p = v4c__read_bytes(r, 4);
    if (!p)
        return 0;

    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t v4c__read_i32(v4c__reader* r)
{
    uint32_t u = v4c__read_u32(r);
    int32_t  i;
    memcpy(&i, &u, sizeof(i));
    return i;
}

static float v4c__read_f32(v4c__reader* r)
{
    uint32_t u = v4c__read_u32(r);
    float    f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

static v4c__str v4c__read_string(v4c__reader* r)
{
    v4c__str       s;
    const uint32_t n = v4c__read_u32(r);
    const uint8_t* p = v4c__read_bytes(r, n);
    s.p              = p ? (const char*)p : "";
    s.n              = p ? n : 0;
    return s;
}

/* ------------------------------------------------------------------------- */
/* Parse context                                                             */
/* ------------------------------------------------------------------------- */

typedef struct v4c__kv
{
    v4c__str key, value;
} v4c__kv;

typedef struct v4c__dict
{
    v4c__kv* items;
    uint32_t count, cap;
} v4c__dict;

typedef struct v4c__raw_model
{
    int32_t        size[3];
    const uint8_t* xyzi;
    uint32_t       num_voxels;
    int            has_xyzi;
} v4c__raw_model;

typedef struct v4c__raw_frame
{
    uint32_t frame;
    int8_t   rotation[9]; /* row-major */
    int32_t  translation[3];
} v4c__raw_frame;

enum
{
    V4C__NODE_TRN = 1,
    V4C__NODE_GRP = 2,
    V4C__NODE_SHP = 3
};

typedef struct v4c__raw_node
{
    v4c__str        name;     /* nTRN */
    v4c__raw_frame* frames;   /* nTRN */
    int32_t*        children; /* nGRP */
    v4c_model_key*  models;   /* nSHP */
    int32_t         id;
    int32_t         child_id; /* nTRN */
    int32_t         layer_id; /* nTRN */
    uint32_t        num_frames;
    uint32_t        num_children;
    uint32_t        num_models;
    uint8_t         type;
    uint8_t         on_stack; /* cycle detection while building */
    bool            hidden;   /* nTRN */
    bool            loop;     /* nTRN and nSHP */
} v4c__raw_node;

typedef struct v4c__raw_layer
{
    int32_t   id;
    v4c_color color;
    v4c__str  name;
    bool      hidden;
} v4c__raw_layer;

typedef struct v4c__ctx
{
    v4c_result       result;
    v4c_load_options opt;
    uint32_t         version;

    v4c__dict dict;

    v4c__raw_model* models;
    uint32_t        num_models, cap_models;
    v4c__raw_node*  nodes;
    uint32_t        num_nodes, cap_nodes;
    v4c__raw_layer* layers;
    uint32_t        num_layers, cap_layers;

    v4c_color    palette[256];
    int          has_palette;
    int          has_pack;
    int          has_anim_range;
    uint32_t     anim_range[2]; /* META _anim_range */
    v4c_material matl[256];
    uint8_t      has_matl[256];
    v4c_material matt[256];
    uint8_t      has_matt[256];

    /* rOBJ / rCAM, still in MagicaVoxel coordinates and voxel units */
    v4c_render_settings render;
    v4c_camera*         cams;
    uint32_t            num_cams, cap_cams;

    /* coordinate conversion: out[i] = sign[i] * in[src[i]] */
    int src[3];
    int sign[3];

    /* output under construction */
    v4c_node* out_nodes;
    uint32_t  num_out_nodes, cap_out_nodes;
} v4c__ctx;

static void v4c__fail(v4c__ctx* c, v4c_result r)
{
    if (c->result == V4C_SUCCESS)
        c->result = r;
}

static int v4c__read_dict(v4c__ctx* c, v4c__reader* r)
{
    uint32_t       i;
    const uint32_t n = v4c__read_u32(r);
    c->dict.count    = 0;
    if (!r->ok)
        return 0;

    if (n > v4c__remaining(r) / 8u)
    {
        r->ok = 0;
        return 0;
    }

    if (!V4C__RESERVE(c->dict.items, c->dict.cap, n))
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return 0;
    }

    for (i = 0; i < n; ++i)
    {
        c->dict.items[i].key   = v4c__read_string(r);
        c->dict.items[i].value = v4c__read_string(r);
    }
    c->dict.count = r->ok ? n : 0;
    return r->ok;
}

static const v4c__str* v4c__dict_get(const v4c__ctx* c, const char* key)
{
    uint32_t i;
    for (i = 0; i < c->dict.count; ++i)
        if (v4c__str_eq(c->dict.items[i].key, key))
            return &c->dict.items[i].value;
    return NULL;
}

static void v4c__default_material(v4c_material* m)
{
    memset(m, 0, sizeof(*m));
    m->type           = V4C_MATERIAL_DIFFUSE;
    m->roughness      = 0.1f;
    m->metallic       = 0.0f;
    m->specular       = 1.0f;
    m->ior            = 1.3f;
    m->transparency   = 0.0f;
    m->emissive       = 0.0f;
    m->emission_power = 0.0f;
    m->ldr            = 0.0f;
    m->media_type     = V4C_MEDIA_ABSORB;
    m->density        = 0.05f;
    m->phase          = 0.0f;
    m->weight         = 1.0f;
    m->attenuation    = 1.0f;
    m->plastic        = false;
}

static void v4c__decode_rotation(uint32_t bits, int8_t R[9])
{
    const uint32_t i0 = bits & 3u, i1 = (bits >> 2) & 3u;
    uint32_t       i2;
    memset(R, 0, 9);
    if (i0 > 2u || i1 > 2u || i0 == i1)
    {
        R[0] = R[4] = R[8] = 1;
        return;
    }
    i2        = 3u - i0 - i1;
    R[0 + i0] = (int8_t)((bits & (1u << 4)) ? -1 : 1);
    R[3 + i1] = (int8_t)((bits & (1u << 5)) ? -1 : 1);
    R[6 + i2] = (int8_t)((bits & (1u << 6)) ? -1 : 1);
}

static v4c__raw_node* v4c__new_raw_node(v4c__ctx* c, int32_t id, uint8_t type)
{
    v4c__raw_node* n;
    if (!V4C__RESERVE(c->nodes, c->cap_nodes, c->num_nodes + 1))
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    n = &c->nodes[c->num_nodes++];
    memset(n, 0, sizeof(*n));
    n->id       = id;
    n->type     = type;
    n->child_id = -1;
    n->layer_id = -1;
    return n;
}

/* ------------------------------------------------------------------------- */
/* Chunk parsers                                                             */
/* ------------------------------------------------------------------------- */

static void v4c__chunk_size(v4c__ctx* c, v4c__reader* r)
{
    v4c__raw_model* m;
    const int32_t   x = v4c__read_i32(r), y = v4c__read_i32(r), z = v4c__read_i32(r);
    if (!r->ok)
        return;
    if (!V4C__RESERVE(c->models, c->cap_models, c->num_models + 1))
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }
    m = &c->models[c->num_models++];
    memset(m, 0, sizeof(*m));
    m->size[0] = x;
    m->size[1] = y;
    m->size[2] = z;
}

static void v4c__chunk_xyzi(v4c__ctx* c, v4c__reader* r)
{
    v4c__raw_model* m;
    const uint32_t  n = v4c__read_u32(r);
    if (!r->ok)
        return;

    if (n > v4c__remaining(r) / 4u)
    {
        r->ok = 0;
        return;
    }

    if (c->num_models == 0 || c->models[c->num_models - 1].has_xyzi)
        return; /* XYZI without a matching SIZE: ignore */
    m             = &c->models[c->num_models - 1];
    m->xyzi       = v4c__read_bytes(r, (size_t)n * 4u);
    m->num_voxels = n;
    m->has_xyzi   = 1;
}

static void v4c__chunk_rgba(v4c__ctx* c, v4c__reader* r)
{
    uint32_t i, n = (uint32_t)(v4c__remaining(r) / 4u);
    if (n > 256u)
        n = 256u;

    for (i = 0; i < n; ++i)
    {
        const uint8_t* p   = v4c__read_bytes(r, 4);
        v4c_color*     col = &c->palette[(i + 1u) & 255u]; /* file colour i -> palette index i + 1 */
        col->r             = p[0];
        col->g             = p[1];
        col->b             = p[2];
        col->a             = p[3];
    }
    c->palette[0].a = 0;
    c->has_palette  = 1;
}

/* Materials of MagicaVoxel 0.98 (MATT) and 0.99.1 - 0.99.4 (MATL with _weight),
   converted the way MagicaVoxel 0.99.7 converts them when it opens such a file
   (checked on files re-saved by 0.99.7, see docs/vox-file-format.md, L.4):
     _weight          metal -> _metal, glass -> _trans, emit -> _emit
     _spec            metal: _sp = _spec + 1; ignored for the other types
     _type _plastic   metal with _metal 0 and _ior = _spec_p
     glass, _media    _d from _att (glass) or _weight (media), see v4c__legacy_density;
                      _media becomes a scattering medium
     _g0, _g1, _gw    _g = _gw * _g1 + (1 - _gw) * _g0
     _glow, _unit     dropped (like _plastic 1 of 0.99.1, kept in 'plastic')
   A current key in the same chunk always wins. */
typedef struct v4c__legacy_mat
{
    float weight, spec, spec_p, att, g0, g1, gw;
    int   has_weight, has_spec, has_spec_p, has_att, has_lobes, plastic_type;
    int   has_sp, has_d, has_g; /* current keys present in the same chunk */
} v4c__legacy_mat;

/* Density of a converted medium (from _weight) or glass (from _att), as 0.99.7
   computes it: d = -ln(1 - 0.4 x), capped at 0.1, the end of its density slider.
   Exact for every medium and glass re-saved by 0.99.7 (e.g. 0.01, 0.05, 0.1,
   0.2, 0.22 -> 0.00400803, 0.0202027, 0.040822, 0.0833816, 0.0921153; 0.25 to
   1 -> 0.1; glass _att 0 -> 0). No libm: with y = x / (2 - x),
   -ln(1 - x) = 2 atanh(y), and y < 0.05 below the cap. */
static float v4c__legacy_density(float w)
{
    double x, y, y2;
    if (!(w > 0.0f))
        return 0.0f;

    x = 0.4 * (double)w;
    if (x >= 0.0951626) /* 1 - e^-0.1 */
        return 0.1f;

    y  = x / (2.0 - x);
    y2 = y * y;
    return (float)(2.0 * y * (1.0 + y2 * (1.0 / 3.0 + y2 * (1.0 / 5.0 + y2 * (1.0 / 7.0)))));
}

static void v4c__legacy_material(v4c_material* m, const v4c__legacy_mat* l)
{
    if (l->plastic_type)
    {
        m->type     = V4C_MATERIAL_METAL;
        m->plastic  = true;
        m->metallic = 0.0f;
        if (l->has_spec_p)
            m->ior = 1.0f + l->spec_p;
    }
    else if (l->has_weight)
    {
        if (m->type == V4C_MATERIAL_METAL)
            m->metallic = l->weight;
        else if (m->type == V4C_MATERIAL_GLASS)
            m->transparency = l->weight;
        else if (m->type == V4C_MATERIAL_EMIT)
            m->emissive = l->weight;
    }

    if (m->type == V4C_MATERIAL_METAL && !l->plastic_type && l->has_spec && !l->has_sp)
        m->specular = 1.0f + l->spec;

    if (m->type == V4C_MATERIAL_GLASS && l->has_att && !l->has_d)
        m->density = v4c__legacy_density(l->att);

    if (m->type == V4C_MATERIAL_MEDIA)
    {
        m->media_type = V4C_MEDIA_SCATTER;
        if (!l->has_d)
            m->density = v4c__legacy_density(l->weight);
    }

    if (l->has_lobes && !l->has_g)
        m->phase = l->gw * l->g1 + (1.0f - l->gw) * l->g0;
}

static float v4c__dict_float(v4c__ctx* c, const char* key, float fallback, int* found)
{
    const v4c__str* v = v4c__dict_get(c, key);
    *found            = v != NULL;
    return v ? v4c__parse_float(*v, fallback) : fallback;
}

/* MATL of 0.99.1 - 0.99.4: recognised by the _weight key, which they always write */
static void v4c__matl_legacy_keys(v4c__ctx* c, v4c_material* m)
{
    const v4c__str* type = v4c__dict_get(c, "_type");
    v4c__legacy_mat l;
    int             has_g0, has_g1, has_gw, modern_weight;
    if (v4c__dict_get(c, "_weight") == NULL)
        return;

    memset(&l, 0, sizeof(l));
    if (m->type == V4C_MATERIAL_METAL)
        modern_weight = v4c__dict_get(c, "_metal") != NULL;
    else if (m->type == V4C_MATERIAL_GLASS)
        modern_weight = v4c__dict_get(c, "_trans") != NULL || v4c__dict_get(c, "_alpha") != NULL;
    else
        modern_weight = v4c__dict_get(c, "_emit") != NULL;

    l.weight       = v4c__dict_float(c, "_weight", 1.0f, &l.has_weight);
    l.has_weight   = l.has_weight && !modern_weight;
    l.spec         = v4c__dict_float(c, "_spec", 0.5f, &l.has_spec);
    l.spec_p       = v4c__dict_float(c, "_spec_p", 0.5f, &l.has_spec_p);
    l.att          = v4c__dict_float(c, "_att", 0.0f, &l.has_att);
    l.g0           = v4c__dict_float(c, "_g0", 0.0f, &has_g0);
    l.g1           = v4c__dict_float(c, "_g1", 0.0f, &has_g1);
    l.gw           = v4c__dict_float(c, "_gw", 0.0f, &has_gw);
    l.has_lobes    = has_g0 && has_g1 && has_gw;
    l.plastic_type = type != NULL && v4c__str_eq(*type, "_plastic");
    l.has_sp       = v4c__dict_get(c, "_sp") != NULL;
    l.has_d        = v4c__dict_get(c, "_d") != NULL;
    l.has_g        = v4c__dict_get(c, "_g") != NULL;
    v4c__legacy_material(m, &l);
}

static void v4c__chunk_matl(v4c__ctx* c, v4c__reader* r)
{
    const v4c__str* v;
    v4c_material*   m;
    int32_t         id = v4c__read_i32(r);
    if (!v4c__read_dict(c, r))
        return;

    /* ids are 1..256, 256 being the unused colour index 0; 0.99.1 also writes an
       id 0, which MagicaVoxel 0.99.7 ignores like any other id out of range */
    if (id < 1 || id > 256)
        return;

    id &= 0xFF;
    m               = &c->matl[id];
    c->has_matl[id] = 1;

    if ((v = v4c__dict_get(c, "_type")) != NULL)
    {
        if (v4c__str_eq(*v, "_metal"))
            m->type = V4C_MATERIAL_METAL;
        else if (v4c__str_eq(*v, "_glass"))
            m->type = V4C_MATERIAL_GLASS;
        else if (v4c__str_eq(*v, "_emit"))
            m->type = V4C_MATERIAL_EMIT;
        else if (v4c__str_eq(*v, "_blend"))
            m->type = V4C_MATERIAL_BLEND;
        else if (v4c__str_eq(*v, "_media"))
            m->type = V4C_MATERIAL_MEDIA;
        else
            m->type = V4C_MATERIAL_DIFFUSE;

        if (v4c__str_eq(*v, "_plastic")) /* 0.99.3 - 0.99.4: a type of its own */
            m->plastic = true;
    }

    if ((v = v4c__dict_get(c, "_rough")) != NULL)
        m->roughness = v4c__parse_float(*v, m->roughness);

    if ((v = v4c__dict_get(c, "_metal")) != NULL)
        m->metallic = v4c__parse_float(*v, m->metallic);

    if ((v = v4c__dict_get(c, "_sp")) != NULL)
        m->specular = v4c__parse_float(*v, m->specular);

    /* MagicaVoxel writes both: _ior = n - 1 and _ri = n */
    if ((v = v4c__dict_get(c, "_ior")) != NULL)
        m->ior = v4c__parse_float(*v, m->ior - 1.0f) + 1.0f;
    else if ((v = v4c__dict_get(c, "_ri")) != NULL)
        m->ior = v4c__parse_float(*v, m->ior);

    if ((v = v4c__dict_get(c, "_trans")) != NULL || (v = v4c__dict_get(c, "_alpha")) != NULL)
        m->transparency = v4c__parse_float(*v, m->transparency);

    if ((v = v4c__dict_get(c, "_emit")) != NULL)
        m->emissive = v4c__parse_float(*v, m->emissive);

    if ((v = v4c__dict_get(c, "_flux")) != NULL)
        m->emission_power = v4c__parse_float(*v, m->emission_power);

    if ((v = v4c__dict_get(c, "_ldr")) != NULL)
        m->ldr = v4c__parse_float(*v, m->ldr);

    if ((v = v4c__dict_get(c, "_media_type")) != NULL)
    {
        if (v4c__str_eq(*v, "_scatter"))
            m->media_type = V4C_MEDIA_SCATTER;
        else if (v4c__str_eq(*v, "_emit"))
            m->media_type = V4C_MEDIA_EMIT;
        else if (v4c__str_eq(*v, "_sss"))
            m->media_type = V4C_MEDIA_SSS;
        else
            m->media_type = V4C_MEDIA_ABSORB;
    }
    else if ((v = v4c__dict_get(c, "_media")) != NULL)
    {
        int32_t media = 0;
        if (v4c__parse_ints(*v, &media, 1) == 1 && media >= 0 && media <= 3)
            m->media_type = (v4c_media_type)media;
    }

    /* MagicaVoxel leaves _d out when it is 0 (new palettes write 0.05 explicitly) */
    if ((v = v4c__dict_get(c, "_d")) != NULL)
        m->density = v4c__parse_float(*v, m->density);
    else
        m->density = 0.0f;

    if ((v = v4c__dict_get(c, "_g")) != NULL)
        m->phase = v4c__parse_float(*v, m->phase);

    if ((v = v4c__dict_get(c, "_weight")) != NULL)
        m->weight = v4c__parse_float(*v, m->weight);

    if ((v = v4c__dict_get(c, "_att")) != NULL)
        m->attenuation = v4c__parse_float(*v, m->attenuation);

    if ((v = v4c__dict_get(c, "_plastic")) != NULL)
        m->plastic = v4c__parse_bool(*v);

    v4c__matl_legacy_keys(c, m);
}

/* Legacy material chunk (MagicaVoxel <= 0.98), best effort. MATL wins. */
static void v4c__chunk_matt(v4c__ctx* c, v4c__reader* r)
{
    v4c_material*   m;
    int32_t         id     = v4c__read_i32(r);
    const int32_t   type   = v4c__read_i32(r);
    const float     weight = v4c__read_f32(r);
    const uint32_t  bits   = v4c__read_u32(r);
    float           values[7];
    v4c__legacy_mat l;

    int i;
    for (i = 0; i < 7; ++i)
        values[i] = (bits & (1u << i)) ? v4c__read_f32(r) : 0.0f;

    if (!r->ok)
        return;

    id &= 0xFF;
    m               = &c->matt[id];
    c->has_matt[id] = 1;
    switch (type)
    {
    case 1: m->type = V4C_MATERIAL_METAL; break;
    case 2: m->type = V4C_MATERIAL_GLASS; break;
    case 3: m->type = V4C_MATERIAL_EMIT; break;
    default: m->type = V4C_MATERIAL_DIFFUSE; break;
    }

    m->weight = weight;
    if (bits & (1u << 0))
        m->plastic = values[0] > 0.5f;

    if (bits & (1u << 1))
        m->roughness = values[1];

    /* undocumented range: accept both n - 1 (like _ior) and n */
    if (bits & (1u << 3))
        m->ior = values[3] < 1.0f ? values[3] + 1.0f : values[3];

    if (bits & (1u << 4))
        m->attenuation = values[4];

    if (bits & (1u << 5))
        m->emission_power = values[5];

    /* bit 6 (glow) has no current equivalent; bit 7 (isTotalPower) carries no value */
    memset(&l, 0, sizeof(l));
    l.weight     = weight;
    l.has_weight = 1;
    l.spec       = values[2];
    l.has_spec   = (bits & (1u << 2)) != 0;
    l.att        = values[4];
    l.has_att    = (bits & (1u << 4)) != 0;
    v4c__legacy_material(m, &l);
}

static void v4c__chunk_ntrn(v4c__ctx* c, v4c__reader* r)
{
    const v4c__str* v;
    v4c__raw_node*  n;
    uint32_t        i;
    const int32_t   id = v4c__read_i32(r);
    if (!v4c__read_dict(c, r))
        return;

    if ((n = v4c__new_raw_node(c, id, V4C__NODE_TRN)) == NULL)
        return;

    if ((v = v4c__dict_get(c, "_name")) != NULL)
        n->name = *v;

    if ((v = v4c__dict_get(c, "_hidden")) != NULL)
        n->hidden = v4c__parse_bool(*v);

    if ((v = v4c__dict_get(c, "_loop")) != NULL)
        n->loop = v4c__parse_bool(*v);

    n->child_id = v4c__read_i32(r);
    (void)v4c__read_i32(r); /* reserved, -1 */
    n->layer_id   = v4c__read_i32(r);
    n->num_frames = v4c__read_u32(r);

    if (!r->ok)
        return;

    if (n->num_frames > v4c__remaining(r) / 4u)
    {
        r->ok = 0;
        return;
    }

    n->frames = (v4c__raw_frame*)v4c__calloc(n->num_frames, sizeof(v4c__raw_frame));
    if (!n->frames)
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }

    for (i = 0; i < n->num_frames; ++i)
    {
        v4c__raw_frame* f    = &n->frames[i];
        int32_t         t[3] = { 0, 0, 0 };
        int32_t         rot  = 0;
        if (!v4c__read_dict(c, r))
            return;

        if ((v = v4c__dict_get(c, "_r")) != NULL)
            v4c__parse_ints(*v, &rot, 1);
        else
            rot = 4; /* identity: row0 -> 0, row1 -> 1 */

        v4c__decode_rotation((uint32_t)rot, f->rotation);
        if ((v = v4c__dict_get(c, "_t")) != NULL)
            v4c__parse_ints(*v, t, 3);

        f->translation[0] = t[0];
        f->translation[1] = t[1];
        f->translation[2] = t[2];
        if ((v = v4c__dict_get(c, "_f")) != NULL)
        {
            int32_t fi = 0;
            v4c__parse_ints(*v, &fi, 1);
            f->frame = fi > 0 ? (uint32_t)fi : 0u;
        }
    }
}

static void v4c__chunk_ngrp(v4c__ctx* c, v4c__reader* r)
{
    v4c__raw_node* n;
    uint32_t       i;
    const int32_t  id = v4c__read_i32(r);
    if (!v4c__read_dict(c, r))
        return;

    if ((n = v4c__new_raw_node(c, id, V4C__NODE_GRP)) == NULL)
        return;

    n->num_children = v4c__read_u32(r);
    if (!r->ok)
        return;

    if (n->num_children > v4c__remaining(r) / 4u)
    {
        r->ok = 0;
        return;
    }

    n->children = (int32_t*)v4c__calloc(n->num_children, sizeof(int32_t));
    if (!n->children)
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }

    for (i = 0; i < n->num_children; ++i)
        n->children[i] = v4c__read_i32(r);
}

static void v4c__chunk_nshp(v4c__ctx* c, v4c__reader* r)
{
    const v4c__str* v;
    v4c__raw_node*  n;
    uint32_t        i;
    const int32_t   id = v4c__read_i32(r);
    if (!v4c__read_dict(c, r))
        return;

    if ((n = v4c__new_raw_node(c, id, V4C__NODE_SHP)) == NULL)
        return;

    if ((v = v4c__dict_get(c, "_loop")) != NULL)
        n->loop = v4c__parse_bool(*v);

    n->num_models = v4c__read_u32(r);
    if (!r->ok)
        return;

    if (n->num_models > v4c__remaining(r) / 8u)
    {
        r->ok = 0;
        return;
    }

    n->models = (v4c_model_key*)v4c__calloc(n->num_models, sizeof(v4c_model_key));
    if (!n->models)
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }

    for (i = 0; i < n->num_models; ++i)
    {
        const int32_t model = v4c__read_i32(r);
        if (!v4c__read_dict(c, r))
            return;

        n->models[i].model = model >= 0 ? (uint32_t)model : 0xFFFFFFFFu;
        if ((v = v4c__dict_get(c, "_f")) != NULL)
        {
            int32_t fi = 0;
            v4c__parse_ints(*v, &fi, 1);
            n->models[i].frame = fi > 0 ? (uint32_t)fi : 0u;
        }
    }
}

/* META: dictionary; _anim_range = "first last", the editor's timeline */
static void v4c__chunk_meta(v4c__ctx* c, v4c__reader* r)
{
    const v4c__str* v;
    int32_t         range[2];
    if (!v4c__read_dict(c, r))
        return;

    if ((v = v4c__dict_get(c, "_anim_range")) != NULL && v4c__parse_ints(*v, range, 2) == 2 && range[0] >= 0 &&
        range[1] >= range[0])
    {
        c->anim_range[0]  = (uint32_t)range[0];
        c->anim_range[1]  = (uint32_t)range[1];
        c->has_anim_range = 1;
    }
}

static void v4c__chunk_layr(v4c__ctx* c, v4c__reader* r)
{
    const v4c__str* v;
    v4c__raw_layer* l;
    const int32_t   id = v4c__read_i32(r);
    if (!v4c__read_dict(c, r))
        return;

    if (!V4C__RESERVE(c->layers, c->cap_layers, c->num_layers + 1))
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }

    l = &c->layers[c->num_layers++];
    memset(l, 0, sizeof(*l));
    l->id      = id;
    l->color.r = l->color.g = l->color.b = l->color.a = 255;
    if ((v = v4c__dict_get(c, "_name")) != NULL)
        l->name = *v;

    if ((v = v4c__dict_get(c, "_hidden")) != NULL)
        l->hidden = v4c__parse_bool(*v);

    if ((v = v4c__dict_get(c, "_color")) != NULL)
    {
        int32_t rgb[3] = { 255, 255, 255 };
        int     k;
        v4c__parse_ints(*v, rgb, 3);
        for (k = 0; k < 3; ++k)
            rgb[k] = rgb[k] < 0 ? 0 : (rgb[k] > 255 ? 255 : rgb[k]);

        l->color.r = (uint8_t)rgb[0];
        l->color.g = (uint8_t)rgb[1];
        l->color.b = (uint8_t)rgb[2];
    }
}

/* rOBJ: one dictionary per renderer setting group, keyed by _type */
static void v4c__chunk_robj(v4c__ctx* c, v4c__reader* r)
{
    const v4c__str* type;
    const v4c__str* v;
    if (!v4c__read_dict(c, r) || (type = v4c__dict_get(c, "_type")) == NULL)
        return;

    if (v4c__str_eq(*type, "_inf"))
    {
        v4c_sun* sun = &c->render.sun;
        float    ang[2];
        sun->present   = true;
        sun->intensity = 0.7f; /* MagicaVoxel's defaults */
        sun->area      = 0.7f;
        sun->elevation = 50.0f;
        sun->azimuth   = 50.0f;
        sun->color.r = sun->color.g = sun->color.b = sun->color.a = 255;
        if ((v = v4c__dict_get(c, "_i")) != NULL)
            sun->intensity = v4c__parse_float(*v, sun->intensity);

        if ((v = v4c__dict_get(c, "_area")) != NULL)
            sun->area = v4c__parse_float(*v, sun->area);

        if ((v = v4c__dict_get(c, "_k")) != NULL)
            sun->color = v4c__parse_rgb(*v, sun->color);

        if ((v = v4c__dict_get(c, "_disk")) != NULL)
            sun->disk = v4c__parse_bool(*v);

        if ((v = v4c__dict_get(c, "_angle")) != NULL && v4c__parse_floats(*v, ang, 2) == 2)
        {
            sun->elevation = ang[0];
            sun->azimuth   = ang[1];
        }
    }
    else if (v4c__str_eq(*type, "_uni"))
    {
        c->render.has_ambient       = true;
        c->render.ambient_intensity = 0.7f;
        c->render.ambient_color.r = c->render.ambient_color.g = c->render.ambient_color.b = 255;
        c->render.ambient_color.a                                                         = 255;
        if ((v = v4c__dict_get(c, "_i")) != NULL)
            c->render.ambient_intensity = v4c__parse_float(*v, 0.7f);

        if ((v = v4c__dict_get(c, "_k")) != NULL)
            c->render.ambient_color = v4c__parse_rgb(*v, c->render.ambient_color);
    }
    else if (v4c__str_eq(*type, "_bg"))
    {
        c->render.has_background = true;
        c->render.background.r = c->render.background.g = c->render.background.b = 0;
        c->render.background.a                                                   = 255;
        if ((v = v4c__dict_get(c, "_color")) != NULL)
            c->render.background = v4c__parse_rgb(*v, c->render.background);

        /* 0.99.1: the switch is part of the group */
        if ((v = v4c__dict_get(c, "_enable")) != NULL)
            c->render.show_background = v4c__parse_bool(*v);
    }
    else if (v4c__str_eq(*type, "_setting"))
    {
        if ((v = v4c__dict_get(c, "_bg_c")) != NULL)
            c->render.show_background = v4c__parse_bool(*v);
    }
    else if (v4c__str_eq(*type, "_lens") || v4c__str_eq(*type, "_len")) /* _len: older versions */
    {
        c->render.has_lens = true;
        c->render.fov      = 45.0f;
        c->render.aperture = 0.0f;
        if ((v = v4c__dict_get(c, "_fov")) != NULL)
            c->render.fov = v4c__parse_float(*v, 45.0f);

        if ((v = v4c__dict_get(c, "_aperture")) != NULL || (v = v4c__dict_get(c, "_dof")) != NULL)
            c->render.aperture = v4c__parse_float(*v, 0.0f);

        c->render.projection       = V4C_PROJECTION_PERSPECTIVE;
        c->render.projection_index = 0;
        if ((v = v4c__dict_get(c, "_proj")) != NULL)
            v4c__parse_ints(*v, &c->render.projection_index, 1);
        else if ((v = v4c__dict_get(c, "_sg")) != NULL && v4c__parse_bool(*v))
            c->render.projection_index = 1; /* older files: stereographic switch */

        if (c->render.projection_index >= 0 && c->render.projection_index <= 2)
            c->render.projection = (v4c_projection)c->render.projection_index;
        else
            c->render.projection = V4C_PROJECTION_UNKNOWN;
    }
}

/* rCAM: int32 camera id + dictionary */
static void v4c__chunk_rcam(v4c__ctx* c, v4c__reader* r)
{
    const v4c__str* v;
    v4c_camera*     cam;
    const int32_t   id = v4c__read_i32(r);
    uint32_t        i;
    if (!v4c__read_dict(c, r) || id < 0)
        return;

    for (i = 0; i < c->num_cams; ++i) /* a later chunk for the same slot wins */
        if (c->cams[i].id == (uint32_t)id)
            break;

    if (i == c->num_cams)
    {
        if (c->num_cams >= 1024u)
            return;
        if (!V4C__RESERVE(c->cams, c->cap_cams, c->num_cams + 1))
        {
            v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
            return;
        }
        ++c->num_cams;
    }
    cam = &c->cams[i];
    memset(cam, 0, sizeof(*cam));
    cam->id      = (uint32_t)id;
    cam->mode    = V4C_CAMERA_PERSPECTIVE;
    cam->fov     = 45.0f;
    cam->frustum = 0.414214f;
    if ((v = v4c__dict_get(c, "_mode")) != NULL)
    {
        if (v4c__str_eq(*v, "pers"))
            cam->mode = V4C_CAMERA_PERSPECTIVE;
        else if (v4c__str_eq(*v, "free"))
            cam->mode = V4C_CAMERA_FREE;
        else if (v4c__str_eq(*v, "pano"))
            cam->mode = V4C_CAMERA_PANORAMA;
        else if (v4c__str_eq(*v, "orth"))
            cam->mode = V4C_CAMERA_ORTHOGRAPHIC;
        else if (v4c__str_eq(*v, "iso"))
            cam->mode = V4C_CAMERA_ISOMETRIC;
        else
            cam->mode = V4C_CAMERA_UNKNOWN;
    }

    if ((v = v4c__dict_get(c, "_focus")) != NULL)
        v4c__parse_floats(*v, cam->focus, 3);

    if ((v = v4c__dict_get(c, "_angle")) != NULL)
        v4c__parse_floats(*v, cam->angle, 3);

    if ((v = v4c__dict_get(c, "_radius")) != NULL)
        cam->radius = v4c__parse_float(*v, 0.0f);

    if ((v = v4c__dict_get(c, "_frustum")) != NULL)
        cam->frustum = v4c__parse_float(*v, cam->frustum);

    if ((v = v4c__dict_get(c, "_fov")) != NULL)
        cam->fov = v4c__parse_float(*v, cam->fov);

    /* an untouched slot keeps the defaults: focus 0, angles 0, radius 0 */
    cam->saved = ((cam->radius != 0.0f || cam->focus[0] != 0.0f || cam->focus[1] != 0.0f || cam->focus[2] != 0.0f ||
                   cam->angle[0] != 0.0f || cam->angle[1] != 0.0f || cam->angle[2] != 0.0f) != 0);
}

static void v4c__parse(v4c__ctx* c, const uint8_t* data, size_t size)
{
    v4c__reader    r;
    const uint8_t* magic;
    r.cur = data;
    r.end = data + size;
    r.ok  = 1;

    magic = v4c__read_bytes(&r, 4);
    if (!magic || memcmp(magic, "VOX ", 4) != 0)
    {
        v4c__fail(c, V4C_ERROR_INVALID_FORMAT);
        return;
    }

    c->version = v4c__read_u32(&r);
    if (!r.ok)
    {
        v4c__fail(c, V4C_ERROR_INVALID_FORMAT);
        return;
    }

    /* Chunks are read sequentially: MAIN's children (and the children of any
       other chunk) simply follow its content, so a flat walk visits all. */
    while (v4c__remaining(&r) >= 12u && c->result == V4C_SUCCESS)
    {
        const uint8_t* id           = v4c__read_bytes(&r, 4);
        const uint32_t content_size = v4c__read_u32(&r);
        v4c__reader    sub;
        (void)v4c__read_u32(&r); /* children size */
        if (content_size > v4c__remaining(&r))
        {
            v4c__fail(c, V4C_ERROR_CORRUPTED_DATA);
            return;
        }

        sub.cur = r.cur;
        sub.end = r.cur + content_size;
        sub.ok  = 1;
        r.cur += content_size;

        if (memcmp(id, "SIZE", 4) == 0)
            v4c__chunk_size(c, &sub);
        else if (memcmp(id, "XYZI", 4) == 0)
            v4c__chunk_xyzi(c, &sub);
        else if (memcmp(id, "RGBA", 4) == 0)
            v4c__chunk_rgba(c, &sub);
        else if (memcmp(id, "MATL", 4) == 0)
            v4c__chunk_matl(c, &sub);
        else if (memcmp(id, "MATT", 4) == 0)
            v4c__chunk_matt(c, &sub);
        else if (memcmp(id, "nTRN", 4) == 0)
            v4c__chunk_ntrn(c, &sub);
        else if (memcmp(id, "nGRP", 4) == 0)
            v4c__chunk_ngrp(c, &sub);
        else if (memcmp(id, "nSHP", 4) == 0)
            v4c__chunk_nshp(c, &sub);
        else if (memcmp(id, "LAYR", 4) == 0)
            v4c__chunk_layr(c, &sub);
        else if (memcmp(id, "rOBJ", 4) == 0)
            v4c__chunk_robj(c, &sub);
        else if (memcmp(id, "rCAM", 4) == 0)
            v4c__chunk_rcam(c, &sub);
        else if (memcmp(id, "PACK", 4) == 0)
            c->has_pack = 1;
        else if (memcmp(id, "META", 4) == 0)
            v4c__chunk_meta(c, &sub);
        /* MAIN, NOTE, IMAP, ... are skipped */

        if (!sub.ok)
            v4c__fail(c, V4C_ERROR_CORRUPTED_DATA);
    }
}

/* ------------------------------------------------------------------------- */
/* Transforms                                                                */
/* ------------------------------------------------------------------------- */

static void v4c__identity_transform(v4c_transform* t)
{
    memset(t, 0, sizeof(*t));
    t->rotation[0] = t->rotation[4] = t->rotation[8] = 1.0f;
    t->quaternion[3]                                 = 1.0f;
    t->scale[0] = t->scale[1] = t->scale[2] = 1.0f;
}

/* Fills quaternion + scale from rotation (a signed permutation matrix). */
static void v4c__decompose(v4c_transform* t)
{
    const float* R = t->rotation;
    float        m[9];
    float        det, tr, s, *q = t->quaternion;
    int          i;
    det = R[0] * (R[4] * R[8] - R[5] * R[7]) - R[1] * (R[3] * R[8] - R[5] * R[6]) + R[2] * (R[3] * R[7] - R[4] * R[6]);
    for (i = 0; i < 9; ++i)
        m[i] = R[i];

    t->scale[0] = t->scale[1] = t->scale[2] = 1.0f;
    if (det < 0.0f)
    {
        /* R = Q * diag(-1, 1, 1)  =>  Q = R with the first column negated */
        t->scale[0] = -1.0f;
        m[0]        = -m[0];
        m[3]        = -m[3];
        m[6]        = -m[6];
    }

    tr = m[0] + m[4] + m[8];
    if (tr > 0.0f)
    {
        s    = v4c__sqrtf(tr + 1.0f) * 2.0f;
        q[3] = 0.25f * s;
        q[0] = (m[7] - m[5]) / s;
        q[1] = (m[2] - m[6]) / s;
        q[2] = (m[3] - m[1]) / s;
    }
    else if (m[0] > m[4] && m[0] > m[8])
    {
        s    = v4c__sqrtf(1.0f + m[0] - m[4] - m[8]) * 2.0f;
        q[3] = (m[7] - m[5]) / s;
        q[0] = 0.25f * s;
        q[1] = (m[1] + m[3]) / s;
        q[2] = (m[2] + m[6]) / s;
    }
    else if (m[4] > m[8])
    {
        s    = v4c__sqrtf(1.0f + m[4] - m[0] - m[8]) * 2.0f;
        q[3] = (m[2] - m[6]) / s;
        q[0] = (m[1] + m[3]) / s;
        q[1] = 0.25f * s;
        q[2] = (m[5] + m[7]) / s;
    }
    else
    {
        s    = v4c__sqrtf(1.0f + m[8] - m[0] - m[4]) * 2.0f;
        q[3] = (m[3] - m[1]) / s;
        q[0] = (m[2] + m[6]) / s;
        q[1] = (m[5] + m[7]) / s;
        q[2] = 0.25f * s;
    }
}

/* MagicaVoxel transform -> output coordinates: R' = C R C^T, t' = C t * scale */
static void v4c__convert_transform(const v4c__ctx* c, const int8_t R[9], const int32_t t[3], v4c_transform* out)
{
    int i, j;
    v4c__identity_transform(out);
    for (i = 0; i < 3; ++i)
    {
        for (j = 0; j < 3; ++j)
            out->rotation[i * 3 + j] = (float)(c->sign[i] * c->sign[j] * R[c->src[i] * 3 + c->src[j]]);

        out->translation[i] = (float)(c->sign[i] * t[c->src[i]]) * c->opt.scale;
    }
    v4c__decompose(out);
}

static uint32_t v4c__loop_frame(uint32_t first, uint32_t last, uint32_t frame)
{
    const uint32_t len = 1u + last - first;
    if (frame >= first)
        return first + (frame - first) % len;

    return last - (first - frame - 1u) % len;
}

static v4c_transform v4c__sample_transform(const v4c_transform_key* k, uint32_t n, bool loop, uint32_t frame,
                                           float scale)
{
    v4c_transform out;
    int32_t       i;
    if (n == 0)
    {
        v4c__identity_transform(&out);
        return out;
    }

    if (loop)
        frame = v4c__loop_frame(k[0].frame, k[n - 1].frame, frame);

    if (frame <= k[0].frame)
        return k[0].transform;

    if (frame >= k[n - 1].frame)
        return k[n - 1].transform;

    for (i = (int32_t)n - 2; i >= 0; --i)
    {
        if (frame >= k[i].frame)
        {
            const v4c_transform* next = &k[i + 1].transform;
            const float          f    = (float)(frame - k[i].frame) / (float)(k[i + 1].frame - k[i].frame);
            int                  a;
            out = k[i].transform; /* rotation snaps to the previous key */
            for (a = 0; a < 3; ++a)
            {
                /* interpolate in whole voxels, truncate towards zero (as MagicaVoxel) */
                const float v0     = scale > 0.0f ? v4c__roundf(out.translation[a] / scale) : out.translation[a];
                const float v1     = scale > 0.0f ? v4c__roundf(next->translation[a] / scale) : next->translation[a];
                const float v      = (float)(int64_t)(v0 * (1.0f - f) + v1 * f);
                out.translation[a] = scale > 0.0f ? v * scale : v;
            }
            return out;
        }
    }
    return k[0].transform;
}

static int32_t v4c__sample_model(const v4c_model_key* k, uint32_t n, bool loop, uint32_t frame)
{
    int32_t i;
    if (n == 0)
        return -1;

    if (loop)
        frame = v4c__loop_frame(k[0].frame, k[n - 1].frame, frame);

    for (i = (int32_t)n - 1; i > 0; --i)
        if (frame >= k[i].frame)
            return (int32_t)k[i].model;

    return (int32_t)k[0].model;
}

/* ---- fractional frames (v4c_sampling) ---- */

static double v4c__floor(double x)
{
    const double t = (double)(int64_t)x;
    return t > x ? t - 1.0 : t;
}

/* Rotation part from quaternion and mirroring: R = rotate(q) * diag(scale). */
static void v4c__rotation_from_quaternion(v4c_transform* t)
{
    const float x = t->quaternion[0], y = t->quaternion[1], z = t->quaternion[2], w = t->quaternion[3];
    float       q[9];
    int         r, col;
    q[0] = 1.0f - 2.0f * (y * y + z * z);
    q[1] = 2.0f * (x * y - z * w);
    q[2] = 2.0f * (x * z + y * w);
    q[3] = 2.0f * (x * y + z * w);
    q[4] = 1.0f - 2.0f * (x * x + z * z);
    q[5] = 2.0f * (y * z - x * w);
    q[6] = 2.0f * (x * z - y * w);
    q[7] = 2.0f * (y * z + x * w);
    q[8] = 1.0f - 2.0f * (x * x + y * y);
    for (r = 0; r < 3; ++r)
        for (col = 0; col < 3; ++col)
            t->rotation[r * 3 + col] = q[r * 3 + col] * t->scale[col];
}

static void v4c__quat_slerp(const float a[4], const float b[4], float t, float out[4])
{
    double dot = 0.0, sign = 1.0, w0, w1, len = 0.0;
    int    i;
    for (i = 0; i < 4; ++i)
        dot += (double)a[i] * (double)b[i];

    if (dot < 0.0) /* shortest arc */
    {
        dot  = -dot;
        sign = -1.0;
    }

    if (dot > 0.9995)
    {
        w0 = 1.0 - t;
        w1 = t;
    }
    else
    {
        const double theta = v4c__atan2(v4c__sqrt(1.0 - dot * dot), dot);
        double       s0, s1, st, c;
        v4c__sincos(theta, &st, &c);
        v4c__sincos((1.0 - t) * theta, &s0, &c);
        v4c__sincos(t * theta, &s1, &c);
        w0 = s0 / st;
        w1 = s1 / st;
    }

    for (i = 0; i < 4; ++i)
    {
        const double v = w0 * a[i] + sign * w1 * b[i];
        out[i]         = (float)v;
        len += v * v;
    }

    len = v4c__sqrt(len);
    if (len > 0.0)
        for (i = 0; i < 4; ++i)
            out[i] = (float)(out[i] / len);
}

static v4c_transform v4c__blend_transform(const v4c_transform* a, const v4c_transform* b, float t)
{
    v4c_transform out = *a;
    int           i;
    if (t <= 0.0f)
        return *a;

    if (t >= 1.0f)
        return *b;

    for (i = 0; i < 3; ++i)
        out.translation[i] = a->translation[i] + (b->translation[i] - a->translation[i]) * t;

    /* a change of mirroring cannot be interpolated: it snaps at the next key */
    if (a->scale[0] == b->scale[0] && a->scale[1] == b->scale[1] && a->scale[2] == b->scale[2])
    {
        v4c__quat_slerp(a->quaternion, b->quaternion, t, out.quaternion);
        v4c__rotation_from_quaternion(&out);
    }
    return out;
}

static uint32_t v4c__frame_index(float frame)
{
    if (!(frame > 0.0f)) /* also NaN */
        return 0u;

    return frame >= 4294967040.0f ? 0xFFFFFF00u : (uint32_t)frame;
}

static v4c_transform v4c__sample_transform_at(const v4c_transform_key* k, uint32_t n, bool loop, float frame,
                                              float scale, v4c_sampling sampling)
{
    double  f = frame > 0.0f ? (double)frame : 0.0;
    double  first, last;
    int32_t i;
    if (sampling != V4C_SAMPLE_SMOOTH || n < 2)
        return v4c__sample_transform(k, n, loop, v4c__frame_index(frame), scale);

    first = (double)k[0].frame;
    last  = (double)k[n - 1].frame;
    if (loop)
    {
        const double len = last - first + 1.0;
        f                = f - first;
        f                = first + (f - v4c__floor(f / len) * len);
        if (f > last) /* between the last key and the first one of the next cycle */
            return v4c__blend_transform(&k[n - 1].transform, &k[0].transform, (float)(f - last));
    }

    if (f <= first)
        return k[0].transform;

    if (f >= last)
        return k[n - 1].transform;

    for (i = (int32_t)n - 2; i >= 0; --i)
        if (f >= (double)k[i].frame)
            return v4c__blend_transform(&k[i].transform, &k[i + 1].transform,
                                        (float)((f - (double)k[i].frame) / (double)(k[i + 1].frame - k[i].frame)));

    return k[0].transform;
}

/* ------------------------------------------------------------------------- */
/* Scene graph construction                                                  */
/* ------------------------------------------------------------------------- */

static int v4c__cmp_raw_node(const v4c__raw_node* a, const v4c__raw_node* b)
{
    return a->id < b->id ? -1 : (a->id > b->id ? 1 : 0);
}

static void v4c__sort_raw_nodes(v4c__raw_node* n, uint32_t count)
{
    /* insertion sort is fine for typical counts; fall back to shell sort gaps */
    uint32_t gap, i, j;
    for (gap = count / 2u; gap > 0u; gap /= 2u)
    {
        for (i = gap; i < count; ++i)
        {
            v4c__raw_node tmp = n[i];
            for (j = i; j >= gap && v4c__cmp_raw_node(&n[j - gap], &tmp) > 0; j -= gap)
                n[j] = n[j - gap];
            n[j] = tmp;
        }
    }
}

static int32_t v4c__find_raw_node(const v4c__ctx* c, int32_t id)
{
    uint32_t lo = 0, hi = c->num_nodes;
    while (lo < hi)
    {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (c->nodes[mid].id < id)
            lo = mid + 1u;
        else
            hi = mid;
    }
    return (lo < c->num_nodes && c->nodes[lo].id == id) ? (int32_t)lo : -1;
}

static int32_t v4c__new_out_node(v4c__ctx* c, int32_t parent)
{
    v4c_node* n;
    if (!V4C__RESERVE(c->out_nodes, c->cap_out_nodes, c->num_out_nodes + 1))
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    n = &c->out_nodes[c->num_out_nodes];
    memset(n, 0, sizeof(*n));
    n->parent = parent;
    n->layer  = -1;
    return (int32_t)c->num_out_nodes++;
}

static int v4c__set_identity_key(v4c__ctx* c, v4c_node* n)
{
    n->transform_keys = (v4c_transform_key*)v4c__calloc(1, sizeof(v4c_transform_key));
    if (!n->transform_keys)
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return 0;
    }
    n->num_transform_keys = 1;
    v4c__identity_transform(&n->transform_keys[0].transform);
    return 1;
}

static int v4c__set_model_keys(v4c__ctx* c, v4c_node* n, const v4c__raw_node* shp)
{
    uint32_t i, j, count = 0;
    n->model_loop = shp->loop;
    if (shp->num_models == 0)
        return 1;

    n->model_keys = (v4c_model_key*)v4c__calloc(shp->num_models, sizeof(v4c_model_key));
    if (!n->model_keys)
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return 0;
    }

    for (i = 0; i < shp->num_models; ++i)
    {
        v4c_model_key key = shp->models[i];
        if (key.model >= c->num_models)
            continue; /* invalid reference */

        for (j = count; j > 0 && n->model_keys[j - 1].frame > key.frame; --j)
            n->model_keys[j] = n->model_keys[j - 1];

        n->model_keys[j] = key;
        ++count;
    }
    n->num_model_keys = count;
    return 1;
}

static int32_t v4c__find_layer(const v4c__ctx* c, int32_t id)
{
    uint32_t i;
    for (i = 0; i < c->num_layers; ++i)
        if (c->layers[i].id == id)
            return (int32_t)i;
    return -1;
}

static int32_t v4c__build_node(v4c__ctx* c, uint32_t raw_index, int32_t parent, uint32_t depth);

/* Fills out node `ni` from the raw node (TRN, or a GRP/SHP with no transform). */
static int v4c__fill_node(v4c__ctx* c, v4c__raw_node* rn, int32_t ni, uint32_t depth)
{
    v4c__raw_node* content = rn;
    v4c_node*      n       = &c->out_nodes[ni];

    if (rn->type == V4C__NODE_TRN)
    {
        uint32_t i, j, nf = rn->num_frames;
        int32_t  child;
        n->hidden         = rn->hidden;
        n->transform_loop = rn->loop;
        n->layer          = rn->layer_id >= 0 ? v4c__find_layer(c, rn->layer_id) : -1;
        n->name           = v4c__str_dup(rn->name);
        if (rn->name.n && !n->name)
        {
            v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
            return 0;
        }

        if (nf == 0)
        {
            if (!v4c__set_identity_key(c, n))
                return 0;
        }
        else
        {
            n->transform_keys = (v4c_transform_key*)v4c__calloc(nf, sizeof(v4c_transform_key));
            if (!n->transform_keys)
            {
                v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
                return 0;
            }

            for (i = 0; i < nf; ++i)
            {
                v4c_transform_key key;
                key.frame = rn->frames[i].frame;
                v4c__convert_transform(c, rn->frames[i].rotation, rn->frames[i].translation, &key.transform);
                for (j = i; j > 0 && n->transform_keys[j - 1].frame > key.frame; --j)
                    n->transform_keys[j] = n->transform_keys[j - 1];

                n->transform_keys[j] = key;
            }
            n->num_transform_keys = nf;
        }
        child = v4c__find_raw_node(c, rn->child_id);
        if (child < 0 || c->nodes[child].on_stack)
            return 1;

        content = &c->nodes[child];
        if (content->type == V4C__NODE_TRN)
        {
            /* non standard TRN -> TRN chain: keep the inner one as a child node */
            uint32_t* ch = (uint32_t*)v4c__calloc(1, sizeof(uint32_t));
            int32_t   cn;
            if (!ch)
            {
                v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
                return 0;
            }

            c->out_nodes[ni].children = ch;
            cn                        = v4c__build_node(c, (uint32_t)child, ni, depth + 1);
            if (cn >= 0)
            {
                ch[0]                         = (uint32_t)cn;
                c->out_nodes[ni].num_children = 1;
            }
            return c->result == V4C_SUCCESS;
        }
    }
    else
    {
        /* group or shape without a transform parent */
        if (!v4c__set_identity_key(c, n))
            return 0;
    }

    if (content->type == V4C__NODE_SHP)
        return v4c__set_model_keys(c, &c->out_nodes[ni], content);

    if (content->type == V4C__NODE_GRP && content->num_children > 0)
    {
        uint32_t  i;
        uint32_t* ch = (uint32_t*)v4c__calloc(content->num_children, sizeof(uint32_t));
        if (!ch)
        {
            v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
            return 0;
        }

        /* attach first so the array is freed with the node on failure */
        c->out_nodes[ni].children = ch;
        content->on_stack         = 1;
        for (i = 0; i < content->num_children && c->result == V4C_SUCCESS; ++i)
        {
            const int32_t raw = v4c__find_raw_node(c, content->children[i]);
            const int32_t cn  = raw >= 0 ? v4c__build_node(c, (uint32_t)raw, ni, depth + 1) : -1;
            if (cn >= 0)
                ch[c->out_nodes[ni].num_children++] = (uint32_t)cn;
        }
        content->on_stack = 0;
    }
    return c->result == V4C_SUCCESS;
}

/* Nodes may be shared (several transforms pointing at one shape is how
   MagicaVoxel instances models), so the graph is a DAG that is expanded into
   a tree. Only real cycles are rejected. */
static int32_t v4c__build_node(v4c__ctx* c, uint32_t raw_index, int32_t parent, uint32_t depth)
{
    int32_t ni;
    if (c->nodes[raw_index].on_stack || depth > V4C__MAX_NODE_DEPTH || c->result != V4C_SUCCESS)
        return -1;

    if (c->num_out_nodes >= V4C__MAX_OUT_NODES)
    {
        v4c__fail(c, V4C_ERROR_CORRUPTED_DATA);
        return -1;
    }

    if ((ni = v4c__new_out_node(c, parent)) < 0)
        return -1;

    c->nodes[raw_index].on_stack = 1;
    if (!v4c__fill_node(c, &c->nodes[raw_index], ni, depth))
        ni = -1;

    c->nodes[raw_index].on_stack = 0;
    return ni;
}

/* Scene without nTRN/nGRP/nSHP (files older than MagicaVoxel 0.99). */
static void v4c__synthesize_graph(v4c__ctx* c)
{
    const int32_t root = v4c__new_out_node(c, -1);
    uint32_t      i, num_children;
    uint32_t*     ch;
    int           anim = c->has_pack && c->num_models > 1; /* 0.98 PACK animation */
    if (root < 0 || !v4c__set_identity_key(c, &c->out_nodes[root]))
        return;

    num_children = anim ? 1u : c->num_models;
    if (num_children == 0)
        return;

    ch = (uint32_t*)v4c__calloc(num_children, sizeof(uint32_t));
    if (!ch)
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }

    c->out_nodes[root].children     = ch;
    c->out_nodes[root].num_children = num_children;
    for (i = 0; i < num_children; ++i)
    {
        const int32_t  ni = v4c__new_out_node(c, root);
        v4c_node*      n;
        uint32_t       k;
        const uint32_t nk = anim ? c->num_models : 1u;
        if (ni < 0)
            return;

        c->out_nodes[root].children[i] = (uint32_t)ni;
        n                              = &c->out_nodes[ni];
        if (!v4c__set_identity_key(c, n))
            return;

        /* MagicaVoxel 0.99+ converts these files by standing the model on the
           ground: translation (0, 0, floor(size.z / 2)), so the lowest voxel
           layer is at z = 0 (a PACK animation uses the first frame's size). */
        {
            static const int8_t identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
            const int32_t       z           = c->models[anim ? 0u : i].size[2];
            const int32_t       t[3]        = { 0, 0, z > 0 ? z / 2 : 0 };
            v4c__convert_transform(c, identity, t, &n->transform_keys[0].transform);
        }

        n->model_keys = (v4c_model_key*)v4c__calloc(nk, sizeof(v4c_model_key));
        if (!n->model_keys)
        {
            v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
            return;
        }

        n->num_model_keys = nk;
        for (k = 0; k < nk; ++k)
        {
            n->model_keys[k].frame = anim ? k : 0u;
            n->model_keys[k].model = anim ? k : i;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Monotone mesher                                                           */
/* ------------------------------------------------------------------------- */

typedef struct v4c__mpt
{
    int32_t x, y, next;
} v4c__mpt;

typedef struct v4c__mpoly
{
    int32_t  value, lhead, ltail, rhead, rtail;
    uint32_t count;
} v4c__mpoly;

typedef struct v4c__mrun
{
    int32_t value, l, r;
} v4c__mrun;

typedef struct v4c__mtri
{
    uint32_t v[3];
    uint32_t material;
} v4c__mtri;

typedef struct v4c__mstk
{
    uint32_t vertex;
    int32_t  x, y;
} v4c__mstk;

typedef struct v4c__mesher
{
    /* input */
    const uint8_t* grid; /* material + bias, 0 = empty */
    uint32_t       bias; /* 1 with compacted materials, 0 otherwise */
    uint32_t       size[3];
    float          pivot[3];
    float          scale;
    float          inv_materials;
    int            clockwise;
    /* scratch */
    v4c__mpt*   pts;
    uint32_t    num_pts, cap_pts;
    v4c__mpoly* polys;
    uint32_t    num_polys, cap_polys;
    v4c__mrun*  runs;
    uint32_t    num_runs, cap_runs;
    uint32_t*   frontier;
    uint32_t    num_frontier, cap_frontier;
    uint32_t*   next;
    uint32_t    num_next, cap_next;
    v4c__mstk*  stack;
    uint32_t    cap_stack;
    v4c__mtri*  tris;
    uint32_t    num_tris, cap_tris;
    /* output vertices */
    float*             positions;
    uint32_t           cap_positions;
    float*             normals;
    uint32_t           cap_normals;
    float*             texcoords;
    uint32_t           cap_texcoords;
    uint8_t*           materials;
    uint32_t           cap_materials;
    v4c_packed_vertex* packed;
    uint32_t           cap_packed;
    int                use_packed;
    uint32_t           num_vertices;
    int                ok;
} v4c__mesher;

static int32_t v4c__mpt_add(v4c__mesher* m, int32_t x, int32_t y)
{
    v4c__mpt* p;
    if (!V4C__RESERVE(m->pts, m->cap_pts, m->num_pts + 1))
    {
        m->ok = 0;
        return -1;
    }
    p       = &m->pts[m->num_pts];
    p->x    = x;
    p->y    = y;
    p->next = -1;
    return (int32_t)m->num_pts++;
}

static void v4c__mpoly_push(v4c__mesher* m, uint32_t poly, int right, int32_t x, int32_t y)
{
    const int32_t p = v4c__mpt_add(m, x, y);
    if (p < 0)
        return;

    if (right)
    {
        m->pts[m->polys[poly].rtail].next = p;
        m->polys[poly].rtail              = p;
    }
    else
    {
        m->pts[m->polys[poly].ltail].next = p;
        m->polys[poly].ltail              = p;
    }
    m->polys[poly].count++;
}

static int v4c__mpoly_new(v4c__mesher* m, const v4c__mrun* run, int32_t y)
{
    v4c__mpoly* p;
    int32_t     l, r;
    if (!V4C__RESERVE(m->polys, m->cap_polys, m->num_polys + 1) || !V4C__RESERVE(m->next, m->cap_next, m->num_next + 1))
    {
        m->ok = 0;
        return 0;
    }
    l = v4c__mpt_add(m, run->l, y);
    r = v4c__mpt_add(m, run->r, y);
    if (l < 0 || r < 0)
        return 0;
    p        = &m->polys[m->num_polys];
    p->value = run->value;
    p->lhead = p->ltail = l;
    p->rhead = p->rtail    = r;
    p->count               = 2;
    m->next[m->num_next++] = m->num_polys++;
    return 1;
}

static void v4c__mpoly_merge(v4c__mesher* m, uint32_t poly, int32_t l, int32_t r, int32_t y)
{
    const int32_t ll = m->pts[m->polys[poly].ltail].x;
    const int32_t lr = m->pts[m->polys[poly].rtail].x;
    if (ll != l)
    {
        v4c__mpoly_push(m, poly, 0, ll, y);
        v4c__mpoly_push(m, poly, 0, l, y);
    }

    if (lr != r)
    {
        v4c__mpoly_push(m, poly, 1, lr, y);
        v4c__mpoly_push(m, poly, 1, r, y);
    }
}

static void v4c__mpoly_close(v4c__mesher* m, uint32_t poly, int32_t y)
{
    const int32_t ll = m->pts[m->polys[poly].ltail].x;
    const int32_t lr = m->pts[m->polys[poly].rtail].x;
    v4c__mpoly_push(m, poly, 0, ll, y);
    v4c__mpoly_push(m, poly, 1, lr, y);
}

static uint32_t v4c__mesh_vertex(v4c__mesher* m, int d, int u, int v, int32_t k, int32_t x, int32_t y, int positive,
                                 uint32_t material)
{
    const uint32_t i = m->num_vertices;
    float          p[3];
    if (m->use_packed)
    {
        int32_t            g[3];
        v4c_packed_vertex* pv;
        if (!V4C__RESERVE(m->packed, m->cap_packed, i + 1))
        {
            m->ok = 0;
            return 0;
        }

        g[d]         = k;
        g[u]         = x;
        g[v]         = y;
        pv           = &m->packed[i];
        pv->x        = (uint16_t)g[0];
        pv->y        = (uint16_t)g[1];
        pv->z        = (uint16_t)g[2];
        pv->normal   = (uint8_t)(d * 2 + (positive ? 0 : 1));
        pv->material = (uint8_t)material;
        m->num_vertices++;
        return i;
    }

    if (!V4C__RESERVE(m->positions, m->cap_positions, (i + 1) * 3u) ||
        !V4C__RESERVE(m->normals, m->cap_normals, (i + 1) * 3u) ||
        !V4C__RESERVE(m->texcoords, m->cap_texcoords, (i + 1) * 2u) ||
        !V4C__RESERVE(m->materials, m->cap_materials, i + 1))
    {
        m->ok = 0;
        return 0;
    }
    p[d]                            = (float)k;
    p[u]                            = (float)x;
    p[v]                            = (float)y;
    m->positions[i * 3 + 0]         = (p[0] - m->pivot[0]) * m->scale;
    m->positions[i * 3 + 1]         = (p[1] - m->pivot[1]) * m->scale;
    m->positions[i * 3 + 2]         = (p[2] - m->pivot[2]) * m->scale;
    m->normals[i * 3 + 0]           = 0.0f;
    m->normals[i * 3 + 1]           = 0.0f;
    m->normals[i * 3 + 2]           = 0.0f;
    m->normals[i * 3 + (uint32_t)d] = positive ? 1.0f : -1.0f;
    m->texcoords[i * 2 + 0]         = ((float)material + 0.5f) * m->inv_materials;
    m->texcoords[i * 2 + 1]         = 0.5f;
    m->materials[i]                 = (uint8_t)material;
    ++m->num_vertices;
    return i;
}

static void v4c__mesh_tri(v4c__mesher* m, const v4c__mstk* a, const v4c__mstk* b, const v4c__mstk* c, int positive,
                          uint32_t material)
{
    const int64_t cross         = (int64_t)(b->x - a->x) * (c->y - a->y) - (int64_t)(c->x - a->x) * (b->y - a->y);
    const int     want_positive = positive ^ m->clockwise;
    v4c__mtri*    t;
    if (cross == 0)
        return; /* degenerate */

    if (!V4C__RESERVE(m->tris, m->cap_tris, m->num_tris + 1))
    {
        m->ok = 0;
        return;
    }

    t           = &m->tris[m->num_tris++];
    t->material = material;
    t->v[2]     = c->vertex;
    /* (u, v, d) is right handed, so a positive 2D area means a +d normal */
    if ((cross > 0) == (want_positive != 0))
    {
        t->v[0] = a->vertex;
        t->v[1] = b->vertex;
    }
    else
    {
        t->v[0] = b->vertex;
        t->v[1] = a->vertex;
    }
}

/* Triangulates one y-monotone polygon (left and right chains). */
static void v4c__mesh_polygon(v4c__mesher* m, uint32_t poly, int d, int u, int v, int32_t k)
{
    const v4c__mpoly* P        = &m->polys[poly];
    const int         positive = P->value > 0;
    const uint32_t    material = (uint32_t)(positive ? P->value : -P->value) - m->bias;
    uint32_t          head = 0, tail = 0;
    int32_t           li, ri;
    int               last_side = 1;
    v4c__mstk         cur;

    if (!V4C__RESERVE(m->stack, m->cap_stack, P->count))
    {
        m->ok = 0;
        return;
    }

    li               = P->lhead;
    ri               = P->rhead;
    cur.x            = m->pts[li].x;
    cur.y            = m->pts[li].y;
    cur.vertex       = v4c__mesh_vertex(m, d, u, v, k, cur.x, cur.y, positive, material);
    m->stack[tail++] = cur;
    cur.x            = m->pts[ri].x;
    cur.y            = m->pts[ri].y;
    cur.vertex       = v4c__mesh_vertex(m, d, u, v, k, cur.x, cur.y, positive, material);
    m->stack[tail++] = cur;
    li               = m->pts[li].next;
    ri               = m->pts[ri].next;

    while ((li >= 0 || ri >= 0) && m->ok)
    {
        int     side;
        int32_t pi;
        if (li < 0)
            side = 1;
        else if (ri >= 0)
            side = m->pts[li].y > m->pts[ri].y;
        else
            side = 0;

        pi         = side ? ri : li;
        cur.x      = m->pts[pi].x;
        cur.y      = m->pts[pi].y;
        cur.vertex = v4c__mesh_vertex(m, d, u, v, k, cur.x, cur.y, positive, material);

        if (side != last_side)
        {
            while (tail - head > 1u)
            {
                v4c__mesh_tri(m, &m->stack[head], &m->stack[head + 1], &cur, positive, material);
                ++head;
            }
        }
        else
        {
            while (tail - head > 1u)
            {
                const v4c__mstk* last = &m->stack[tail - 1];
                const v4c__mstk* prev = &m->stack[tail - 2];
                const int64_t    cross =
                    (int64_t)(last->x - cur.x) * (prev->y - cur.y) - (int64_t)(prev->x - cur.x) * (last->y - cur.y);
                if (side == (cross > 0))
                    break;

                if (cross != 0)
                    v4c__mesh_tri(m, last, prev, &cur, positive, material);

                --tail;
            }
        }
        m->stack[tail++] = cur;
        if (side)
            ri = m->pts[ri].next;
        else
            li = m->pts[li].next;
        last_side = side;
    }
}

static int v4c__grid_at(const v4c__mesher* m, const int32_t p[3])
{
    return m->grid[(size_t)p[0] + (size_t)m->size[0] * ((size_t)p[1] + (size_t)m->size[1] * (size_t)p[2])];
}

/* Runs of equal, non zero face values on row y of slice k. */
static void v4c__mesh_row(v4c__mesher* m, int d, int u, int v, int32_t k, int32_t y)
{
    int32_t       p[3], prev = 0, x;
    const int32_t su = (int32_t)m->size[u], sd = (int32_t)m->size[d];
    m->num_runs = 0;
    p[v]        = y;
    for (x = 0; x <= su; ++x)
    {
        int32_t value = 0;
        if (x < su)
        {
            int back = 0, front = 0;
            p[u] = x;
            if (k > 0)
            {
                p[d] = k - 1;
                back = v4c__grid_at(m, p);
            }

            if (k < sd)
            {
                p[d]  = k;
                front = v4c__grid_at(m, p);
            }

            if ((back != 0) != (front != 0))
                value = back ? back : -front; /* +: face looks along +d, -: along -d */
        }

        if (value != prev)
        {
            if (prev != 0)
                m->runs[m->num_runs - 1].r = x;

            if (value != 0)
            {
                if (!V4C__RESERVE(m->runs, m->cap_runs, m->num_runs + 1))
                {
                    m->ok = 0;
                    return;
                }
                m->runs[m->num_runs].value = value;
                m->runs[m->num_runs].l     = x;
                m->runs[m->num_runs].r     = x + 1;
                ++m->num_runs;
            }
        }
        prev = value;
    }
}

static void v4c__mesh_slice(v4c__mesher* m, int d, int u, int v, int32_t k)
{
    int32_t       y;
    const int32_t sv = (int32_t)m->size[v];
    uint32_t      i;
    m->num_pts      = 0;
    m->num_polys    = 0;
    m->num_frontier = 0;

    for (y = 0; y < sv && m->ok; ++y)
    {
        uint32_t nf = 0, nc = 0;
        v4c__mesh_row(m, d, u, v, k, y);
        m->num_next = 0;
        while (nf < m->num_frontier && nc < m->num_runs && m->ok)
        {
            const uint32_t   poly = m->frontier[nf];
            const v4c__mrun* run  = &m->runs[nc];
            const int32_t    pl   = m->pts[m->polys[poly].ltail].x;
            const int32_t    pr   = m->pts[m->polys[poly].rtail].x;
            if (run->r > pl && pr > run->l && run->value == m->polys[poly].value)
            {
                v4c__mpoly_merge(m, poly, run->l, run->r, y);
                if (!V4C__RESERVE(m->next, m->cap_next, m->num_next + 1))
                {
                    m->ok = 0;
                    return;
                }
                m->next[m->num_next++] = poly;
                ++nf;
                ++nc;
            }
            else
            {
                if (run->r <= pr)
                {
                    v4c__mpoly_new(m, run, y);
                    ++nc;
                }

                if (pr <= run->r)
                {
                    v4c__mpoly_close(m, poly, y);
                    ++nf;
                }
            }
        }
        for (; nf < m->num_frontier; ++nf)
            v4c__mpoly_close(m, m->frontier[nf], y);

        for (; nc < m->num_runs; ++nc)
            v4c__mpoly_new(m, &m->runs[nc], y);

        if (!m->ok)
            return;

        /* swap frontier buffers */
        {
            uint32_t* tp    = m->frontier;
            uint32_t  tc    = m->cap_frontier;
            m->frontier     = m->next;
            m->cap_frontier = m->cap_next;
            m->num_frontier = m->num_next;
            m->next         = tp;
            m->cap_next     = tc;
        }
    }
    for (i = 0; i < m->num_frontier; ++i)
        v4c__mpoly_close(m, m->frontier[i], sv);

    for (i = 0; i < m->num_polys && m->ok; ++i)
        v4c__mesh_polygon(m, i, d, u, v, k);
}

static void v4c__mesher_free_scratch(v4c__mesher* m)
{
    V4C_FREE(m->pts);
    V4C_FREE(m->polys);
    V4C_FREE(m->runs);
    V4C_FREE(m->frontier);
    V4C_FREE(m->next);
    V4C_FREE(m->stack);
    V4C_FREE(m->tris);
}

static void v4c__free_mesh(v4c_mesh* mesh)
{
    V4C_FREE(mesh->positions);
    V4C_FREE(mesh->normals);
    V4C_FREE(mesh->texcoords);
    V4C_FREE(mesh->materials);
    V4C_FREE(mesh->packed);
    V4C_FREE(mesh->indices);
    V4C_FREE(mesh->indices16);
    V4C_FREE(mesh->submeshes);
    memset(mesh, 0, sizeof(*mesh));
}

/* V4C_FLAG_16BIT_INDICES: replaces indices by indices16 when every index fits. */
static int v4c__mesh_to_16bit(v4c_mesh* mesh)
{
    uint32_t i;
    if (!mesh->indices || mesh->num_vertices > 65536u)
        return 1;

    mesh->indices16 = (uint16_t*)v4c__calloc(mesh->num_indices, sizeof(uint16_t));
    if (!mesh->indices16)
        return 0;

    for (i = 0; i < mesh->num_indices; ++i)
        mesh->indices16[i] = (uint16_t)mesh->indices[i];

    V4C_FREE(mesh->indices);
    mesh->indices = NULL;
    return 1;
}

static int v4c__build_mesh(v4c__mesher* m, uint32_t num_materials, v4c_mesh* out)
{
    uint32_t counts[256], offsets[256];
    uint32_t i, d, num_sub = 0;
    memset(out, 0, sizeof(*out));
    m->ok = 1;

    for (d = 0; d < 3u && m->ok; ++d)
    {
        const int u = (int)((d + 1u) % 3u), v = (int)((d + 2u) % 3u);
        int32_t   k;
        if (m->size[0] == 0 || m->size[1] == 0 || m->size[2] == 0)
            break;

        for (k = 0; k <= (int32_t)m->size[d] && m->ok; ++k)
            v4c__mesh_slice(m, (int)d, u, v, k);
    }

    if (!m->ok)
    {
        V4C_FREE(m->positions);
        V4C_FREE(m->normals);
        V4C_FREE(m->texcoords);
        V4C_FREE(m->materials);
        V4C_FREE(m->packed);
        return 0;
    }

    out->num_vertices = m->num_vertices;
    out->clockwise    = m->clockwise != 0;
    out->packed       = m->packed;
    out->positions    = m->positions;
    out->normals      = m->normals;
    out->texcoords    = m->texcoords;
    out->materials    = m->materials;

    /* bucket triangles by material */
    memset(counts, 0, sizeof(counts));
    for (i = 0; i < m->num_tris; ++i)
        ++counts[m->tris[i].material & 255u];

    for (i = 0; i < 256u; ++i)
        if (counts[i])
            ++num_sub;

    out->num_indices = m->num_tris * 3u;
    out->indices     = (uint32_t*)v4c__calloc(out->num_indices, sizeof(uint32_t));
    out->submeshes   = (v4c_submesh*)v4c__calloc(num_sub, sizeof(v4c_submesh));
    if (!out->indices || !out->submeshes)
    {
        v4c__free_mesh(out);
        return 0;
    }

    out->num_submeshes = num_sub;
    num_sub            = 0;
    {
        uint32_t running = 0;
        for (i = 0; i < 256u && i < num_materials; ++i)
        {
            offsets[i] = running;
            if (counts[i])
            {
                out->submeshes[num_sub].material    = i;
                out->submeshes[num_sub].first_index = running * 3u;
                out->submeshes[num_sub].num_indices = counts[i] * 3u;
                ++num_sub;
            }
            running += counts[i];
        }
    }

    for (i = 0; i < m->num_tris; ++i)
    {
        const uint32_t slot         = offsets[m->tris[i].material & 255u]++;
        out->indices[slot * 3u + 0] = m->tris[i].v[0];
        out->indices[slot * 3u + 1] = m->tris[i].v[1];
        out->indices[slot * 3u + 2] = m->tris[i].v[2];
    }

    if (out->num_indices == 0)
    {
        V4C_FREE(out->indices);
        out->indices = NULL;
    }

    if (out->num_submeshes == 0)
    {
        V4C_FREE(out->submeshes);
        out->submeshes = NULL;
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Scene assembly                                                            */
/* ------------------------------------------------------------------------- */

static void v4c__free_node(v4c_node* n)
{
    V4C_FREE(n->name);
    V4C_FREE(n->children);
    V4C_FREE(n->transform_keys);
    V4C_FREE(n->model_keys);
}

static void v4c__free_ctx(v4c__ctx* c)
{
    uint32_t i;
    for (i = 0; i < c->num_nodes; ++i)
    {
        V4C_FREE(c->nodes[i].frames);
        V4C_FREE(c->nodes[i].children);
        V4C_FREE(c->nodes[i].models);
    }
    V4C_FREE(c->nodes);
    V4C_FREE(c->models);
    V4C_FREE(c->layers);
    V4C_FREE(c->cams);
    V4C_FREE(c->dict.items);
    if (c->out_nodes)
    {
        for (i = 0; i < c->num_out_nodes; ++i)
            v4c__free_node(&c->out_nodes[i]);

        V4C_FREE(c->out_nodes);
    }
}

static int v4c__setup_axes(v4c__ctx* c)
{
    static const v4c_axis presets[4][3] = {
        { V4C_AXIS_POS_X, V4C_AXIS_POS_Y, V4C_AXIS_POS_Z }, /* Z up, right handed */
        { V4C_AXIS_NEG_Y, V4C_AXIS_NEG_X, V4C_AXIS_POS_Z }, /* Z up, left handed  */
        { V4C_AXIS_POS_X, V4C_AXIS_POS_Z, V4C_AXIS_NEG_Y }, /* Y up, right handed */
        { V4C_AXIS_NEG_X, V4C_AXIS_POS_Z, V4C_AXIS_NEG_Y }, /* Y up, left handed  */
    };
    const v4c_axis* axes;
    int             i, used = 0;
    if ((int)c->opt.coord_system >= 0 && (int)c->opt.coord_system < 4)
        axes = presets[c->opt.coord_system];
    else if (c->opt.coord_system == V4C_COORDS_CUSTOM)
        axes = c->opt.custom_axes;
    else
        return 0;

    for (i = 0; i < 3; ++i)
    {
        const int a = (int)axes[i];
        if (a < 0 || a > 5)
            return 0;

        c->src[i]  = a / 2;
        c->sign[i] = (a & 1) ? -1 : 1;
        if (used & (1 << c->src[i]))
            return 0;

        used |= 1 << c->src[i];
        c->opt.custom_axes[i] = axes[i];
    }
    return 1;
}

static int v4c__determinant_sign(const v4c__ctx* c)
{
    /* sign of a signed permutation = sign of the permutation * product of signs */
    const int perm = (c->src[0] == 0)   ? (c->src[1] == 1 ? 1 : -1)
                     : (c->src[0] == 1) ? (c->src[1] == 2 ? 1 : -1)
                                        : (c->src[1] == 0 ? 1 : -1);
    return perm * c->sign[0] * c->sign[1] * c->sign[2];
}

/* direction in MagicaVoxel axes -> output axes (no scale) */
static void v4c__axes_dir(const v4c_axis axes[3], const double in[3], float out[3])
{
    int i;
    for (i = 0; i < 3; ++i)
    {
        const int a = (int)axes[i];
        out[i]      = (float)((a & 1) ? -in[a >> 1] : in[a >> 1]);
    }
}

static void v4c__build_render(v4c__ctx* c, v4c_scene* s)
{
    uint32_t i, j;
    v4c_sun* sun;

    s->render = c->render;

    /* ---- cameras: sorted by slot, focus / radius converted ---- */
    for (i = 1; i < c->num_cams; ++i)
    {
        v4c_camera t = c->cams[i];
        for (j = i; j > 0 && c->cams[j - 1].id > t.id; --j)
            c->cams[j] = c->cams[j - 1];

        c->cams[j] = t;
    }

    for (i = 0; i < c->num_cams; ++i)
    {
        v4c_camera* cam = &c->cams[i];
        double      f[3];
        int         k;
        for (k = 0; k < 3; ++k)
            f[k] = (double)cam->focus[k] * (double)c->opt.scale;

        v4c__axes_dir(s->axes, f, cam->focus);
        cam->radius *= c->opt.scale;
        if (cam->mode == V4C_CAMERA_ORTHOGRAPHIC || cam->mode == V4C_CAMERA_ISOMETRIC)
            cam->frustum *= c->opt.scale; /* half the view height: a length */
    }
    s->cameras     = c->cams;
    s->num_cameras = c->num_cams;
    c->cams        = NULL;
    c->num_cams    = 0;

    /* ---- sun ---- */
    sun = &s->render.sun;
    if (sun->present)
    {
        double se, ce, sa, ca, light_dir[3];
        if (!(sun->area > 0.0f))
            sun->area = 0.0f;

        sun->type             = sun->area > 0.0f ? V4C_LIGHT_AREA : V4C_LIGHT_DIRECTIONAL;
        sun->angular_diameter = sun->area * 43.5f; /* ogt_vox: area 1.0 ~= 43.5 degrees */
        if (sun->angular_diameter > 179.0f)
            sun->angular_diameter = 179.0f;

        /* MagicaVoxel: azimuth 0 = in front (-Y), 90 = right (+X), 180 = back (+Y),
           270 = left (-X); elevation above the horizon (+Z) */
        v4c__sincos(v4c__deg_to_rad((double)sun->elevation), &se, &ce);
        v4c__sincos(v4c__deg_to_rad((double)sun->azimuth), &sa, &ca);
        /* towards the sun: (cos(el) sin(az), -cos(el) cos(az), sin(el)); the
           light travels the opposite way */
        light_dir[0] = -(ce * sa);
        light_dir[1] = ce * ca;
        light_dir[2] = -se;
        v4c__axes_dir(s->axes, light_dir, sun->direction);

        if (sun->type == V4C_LIGHT_AREA)
        {
            float mn[3], mx[3], centre[3] = { 0.0f, 0.0f, 0.0f }, radius = 0.0f;
            int   k;
            if (v4c_scene_bounds(s, 0, false, mn, mx))
            {
                float d2 = 0.0f;
                for (k = 0; k < 3; ++k)
                {
                    centre[k] = 0.5f * (mn[k] + mx[k]);
                    d2 += (mx[k] - mn[k]) * (mx[k] - mn[k]);
                }
                radius = 0.5f * v4c__sqrtf(d2);
            }

            if (radius < c->opt.scale)
                radius = c->opt.scale;

            v4c_sun_area_light(s, centre, 100.0f * radius, &sun->area_light);
        }
    }
}

static void v4c__build_scene(v4c__ctx* c, v4c_scene* s)
{
    uint32_t       i, j;
    uint32_t       max_frame   = 0;
    int32_t*       model_remap = NULL;
    uint8_t        used_colors[256];
    int16_t        color_remap[256];
    uint8_t*       grid     = NULL;
    size_t         grid_cap = 0;
    int            single   = c->opt.frame >= 0;
    uint32_t       flags    = c->opt.flags;
    const uint32_t bias     = (flags & V4C_FLAG_USED_MATERIALS_ONLY) ? 1u : 0u;

    s->file_version = c->version;
    s->scale        = c->opt.scale;
    s->loaded_frame = single ? c->opt.frame : V4C_ALL_FRAMES;
    for (i = 0; i < 3; ++i)
        s->axes[i] = c->opt.custom_axes[i];

    s->left_handed = v4c__determinant_sign(c) < 0;

    /* ---- palette defaults ---- */
    if (!c->has_palette)
    {
        for (i = 0; i < 256u; ++i)
        {
            const uint32_t col = v4c__default_palette[i];
            c->palette[i].r    = (uint8_t)(col & 0xFFu);
            c->palette[i].g    = (uint8_t)((col >> 8) & 0xFFu);
            c->palette[i].b    = (uint8_t)((col >> 16) & 0xFFu);
            c->palette[i].a    = (uint8_t)((col >> 24) & 0xFFu);
        }
    }

    /* ---- scene graph ---- */
    if (c->num_nodes > 0)
    {
        int32_t root;
        v4c__sort_raw_nodes(c->nodes, c->num_nodes);
        root = v4c__find_raw_node(c, 0);
        if (root < 0)
            root = 0;

        v4c__build_node(c, (uint32_t)root, -1, 0);
    }

    if (c->result != V4C_SUCCESS)
        return;

    if (c->num_out_nodes == 0)
        v4c__synthesize_graph(c);

    if (c->result != V4C_SUCCESS)
        return;

    for (i = 0; i < c->num_out_nodes; ++i)
    {
        const v4c_node* n = &c->out_nodes[i];
        for (j = 0; j < n->num_transform_keys; ++j)
            if (n->transform_keys[j].frame > max_frame)
                max_frame = n->transform_keys[j].frame;

        for (j = 0; j < n->num_model_keys; ++j)
            if (n->model_keys[j].frame > max_frame)
                max_frame = n->model_keys[j].frame;
    }
    s->num_frames = max_frame + 1u;
    /* without META the editor shows its default timeline 0 .. 30; MagicaVoxel 0.99.7
       writes exactly that when it re-saves an older file */
    s->anim_first = c->has_anim_range ? c->anim_range[0] : 0u;
    s->anim_last  = c->has_anim_range ? c->anim_range[1] : (max_frame > 30u ? max_frame : 30u);

    /* ---- single frame: sample every node, keep one key ---- */
    if (single)
    {
        /* any frame up to the end of the keys or of the editor's range: looping
           nodes wrap, the others hold their last key (as in MagicaVoxel) */
        const uint32_t frame = (uint32_t)c->opt.frame;
        if (frame > max_frame && frame > s->anim_last)
        {
            v4c__fail(c, V4C_ERROR_FRAME_NOT_FOUND);
            return;
        }

        for (i = 0; i < c->num_out_nodes; ++i)
        {
            v4c_node* n = &c->out_nodes[i];
            n->transform_keys[0].transform =
                v4c__sample_transform(n->transform_keys, n->num_transform_keys, n->transform_loop, frame, c->opt.scale);
            n->transform_keys[0].frame = frame;
            n->num_transform_keys      = 1;
            if (n->num_model_keys)
            {
                n->model_keys[0].model =
                    (uint32_t)v4c__sample_model(n->model_keys, n->num_model_keys, n->model_loop, frame);
                n->model_keys[0].frame = frame;
                n->num_model_keys      = 1;
            }
            n->transform_loop = false;
            n->model_loop     = false;
        }
    }

    /* ---- models to keep: all, or only those referenced by the single frame ---- */
    model_remap = (int32_t*)v4c__calloc(c->num_models, sizeof(int32_t));
    if (!model_remap)
    {
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }

    for (i = 0; i < c->num_models; ++i)
        model_remap[i] = single ? -1 : 0;

    if (single)
        for (i = 0; i < c->num_out_nodes; ++i)
            for (j = 0; j < c->out_nodes[i].num_model_keys; ++j)
                model_remap[c->out_nodes[i].model_keys[j].model] = 0;

    s->num_models = 0;
    for (i = 0; i < c->num_models; ++i)
        if (model_remap[i] == 0)
            model_remap[i] = (int32_t)s->num_models++;

    for (i = 0; i < c->num_out_nodes; ++i)
        for (j = 0; j < c->out_nodes[i].num_model_keys; ++j)
            c->out_nodes[i].model_keys[j].model = (uint32_t)model_remap[c->out_nodes[i].model_keys[j].model];

    /* ---- materials ---- */
    memset(used_colors, 0, sizeof(used_colors));
    if (flags & V4C_FLAG_USED_MATERIALS_ONLY)
    {
        for (i = 0; i < c->num_models; ++i)
        {
            const v4c__raw_model* rm = &c->models[i];
            if (model_remap[i] < 0 || !rm->xyzi)
                continue;
            for (j = 0; j < rm->num_voxels; ++j)
            {
                const uint8_t* p = rm->xyzi + (size_t)j * 4u;
                if (p[3] != 0 && (int32_t)p[0] < rm->size[0] && (int32_t)p[1] < rm->size[1] &&
                    (int32_t)p[2] < rm->size[2])
                    used_colors[p[3]] = 1;
            }
        }
    }
    else
    {
        memset(used_colors, 1, sizeof(used_colors));
    }

    s->num_materials = 0;
    for (i = 0; i < 256u; ++i)
        color_remap[i] = (int16_t)(used_colors[i] ? (int32_t)s->num_materials++ : -1);

    s->materials = (v4c_material*)v4c__calloc(s->num_materials, sizeof(v4c_material));
    if (!s->materials)
    {
        V4C_FREE(model_remap);
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }

    for (i = 0; i < 256u; ++i)
    {
        v4c_material* m;
        if (color_remap[i] < 0)
            continue;

        m = &s->materials[color_remap[i]];
        if (c->has_matl[i])
            *m = c->matl[i];
        else if (c->has_matt[i])
            *m = c->matt[i];
        else
            v4c__default_material(m);

        m->color         = c->palette[i];
        m->palette_index = (uint8_t)i;
    }

    /* ---- models: voxels, pivots, meshes ---- */
    s->models = (v4c_model*)v4c__calloc(s->num_models, sizeof(v4c_model));
    if (!s->models)
    {
        V4C_FREE(model_remap);
        v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        return;
    }

    for (i = 0; i < c->num_models && c->result == V4C_SUCCESS; ++i)
    {
        const v4c__raw_model* rm = &c->models[i];
        v4c_model*            om;
        uint32_t              sm[3], so[3], a, count = 0;
        size_t                volume;
        float                 mv_pivot[3];
        if (model_remap[i] < 0)
            continue;

        om             = &s->models[model_remap[i]];
        om->file_index = i;
        for (a = 0; a < 3u; ++a)
        {
            const int32_t v = rm->size[a];
            sm[a]           = v < 0 ? 0u : (v > V4C_MAX_MODEL_SIZE ? (uint32_t)V4C_MAX_MODEL_SIZE : (uint32_t)v);
        }

        for (a = 0; a < 3u; ++a)
        {
            const uint32_t sa  = sm[c->src[a]];
            const uint32_t mvp = sa / 2u; /* MagicaVoxel pivot: floor(size / 2) */
            so[a]              = sa;
            om->size[a]        = sa;
            mv_pivot[a]        = (float)(c->sign[a] > 0 ? mvp : sa - mvp);
        }

        for (a = 0; a < 3u; ++a)
        {
            switch (c->opt.pivot)
            {
            case V4C_PIVOT_CORNER: om->pivot[a] = 0.0f; break;
            case V4C_PIVOT_CENTER: om->pivot[a] = (float)so[a] * 0.5f; break;
            case V4C_PIVOT_BOTTOM_CENTER:
                if (c->src[a] == 2)
                    om->pivot[a] = c->sign[a] > 0 ? 0.0f : (float)so[a];
                else
                    om->pivot[a] = (float)so[a] * 0.5f;
                break;
            default: om->pivot[a] = mv_pivot[a]; break;
            }
            om->scene_offset[a] = (om->pivot[a] - mv_pivot[a]) * c->opt.scale;
        }

        /* Dense grid in output coordinates, 0 = empty. Full palette: the value is
           the palette index itself (index 0 is never a voxel). Compacted: at most
           255 materials, stored as material + 1. */
        volume = (size_t)so[0] * so[1] * so[2];
        if (volume == 0)
            continue;

        if (volume > grid_cap)
        {
            V4C_FREE(grid);
            grid     = (uint8_t*)V4C_MALLOC(volume);
            grid_cap = grid ? volume : 0;
            if (!grid)
            {
                v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
                break;
            }
        }

        memset(grid, 0, volume);
        for (j = 0; rm->xyzi && j < rm->num_voxels; ++j)
        {
            const uint8_t* p = rm->xyzi + (size_t)j * 4u;
            uint32_t       o[3];
            if (p[3] == 0 || p[0] >= sm[0] || p[1] >= sm[1] || p[2] >= sm[2] || color_remap[p[3]] < 0)
                continue;

            for (a = 0; a < 3u; ++a)
            {
                const uint32_t in = p[c->src[a]];
                o[a]              = c->sign[a] > 0 ? in : sm[c->src[a]] - 1u - in;
            }
            grid[o[0] + (size_t)so[0] * (o[1] + (size_t)so[1] * o[2])] = (uint8_t)(color_remap[p[3]] + (int)bias);
        }

        for (j = 0; j < volume; ++j)
            if (grid[j])
                ++count;

        om->num_voxels = count;
        if (!(flags & V4C_FLAG_DISCARD_VOXELS) && count > 0)
        {
            uint32_t x, y, z, n = 0;
            om->voxels = (v4c_voxel*)v4c__calloc(count, sizeof(v4c_voxel));
            if (!om->voxels)
            {
                v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
                break;
            }

            for (z = 0; z < so[2]; ++z)
                for (y = 0; y < so[1]; ++y)
                    for (x = 0; x < so[0]; ++x)
                    {
                        const uint8_t g = grid[x + (size_t)so[0] * (y + (size_t)so[1] * z)];
                        if (!g)
                            continue;

                        om->voxels[n].x        = (uint8_t)x;
                        om->voxels[n].y        = (uint8_t)y;
                        om->voxels[n].z        = (uint8_t)z;
                        om->voxels[n].material = (uint8_t)(g - bias);
                        ++n;
                    }
        }

        if ((flags & (V4C_FLAG_GENERATE_MESHES | V4C_FLAG_PACKED_VERTICES)) && count > 0)
        {
            v4c__mesher m;
            int         ok;
            memset(&m, 0, sizeof(m));
            m.grid          = grid;
            m.bias          = bias;
            m.scale         = c->opt.scale;
            m.inv_materials = 1.0f / (float)(s->num_materials ? s->num_materials : 1u);
            m.clockwise     = (flags & V4C_FLAG_CLOCKWISE_WINDING) ? 1 : 0;
            m.use_packed    = (flags & V4C_FLAG_PACKED_VERTICES) ? 1 : 0;
            for (a = 0; a < 3u; ++a)
            {
                m.size[a]  = so[a];
                m.pivot[a] = om->pivot[a];
            }
            ok = v4c__build_mesh(&m, s->num_materials, &om->mesh);
            v4c__mesher_free_scratch(&m);
            if (ok && (flags & V4C_FLAG_16BIT_INDICES))
                ok = v4c__mesh_to_16bit(&om->mesh);

            if (!ok)
                v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        }
    }
    V4C_FREE(grid);
    V4C_FREE(model_remap);
    if (c->result != V4C_SUCCESS)
        return;

    /* ---- layers ---- */
    if (c->num_layers)
    {
        s->layers = (v4c_layer*)v4c__calloc(c->num_layers, sizeof(v4c_layer));
        if (!s->layers)
        {
            v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
            return;
        }

        s->num_layers = c->num_layers;
        for (i = 0; i < c->num_layers; ++i)
        {
            s->layers[i].id     = c->layers[i].id;
            s->layers[i].color  = c->layers[i].color;
            s->layers[i].hidden = c->layers[i].hidden;
            s->layers[i].name   = v4c__str_dup(c->layers[i].name);
            if (c->layers[i].name.n && !s->layers[i].name)
            {
                v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
                return;
            }
        }
    }

    /* ---- hand over nodes ---- */
    s->nodes         = c->out_nodes;
    s->num_nodes     = c->num_out_nodes;
    c->out_nodes     = NULL;
    c->num_out_nodes = 0;

    /* ---- render settings and cameras (needs the finished nodes for bounds) ---- */
    v4c__build_render(c, s);
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

VOX4C_API void v4c_default_load_options(v4c_load_options* o)
{
    if (!o)
        return;

    memset(o, 0, sizeof(*o));
    o->flags          = 0;
    o->frame          = V4C_ALL_FRAMES;
    o->coord_system   = V4C_COORDS_Z_UP_RIGHT_HANDED;
    o->custom_axes[0] = V4C_AXIS_POS_X;
    o->custom_axes[1] = V4C_AXIS_POS_Y;
    o->custom_axes[2] = V4C_AXIS_POS_Z;
    o->scale          = 1.0f;
    o->pivot          = V4C_PIVOT_MAGICAVOXEL;
}

VOX4C_API void v4c_free_scene(v4c_scene* s)
{
    uint32_t i;
    if (!s)
        return;

    for (i = 0; s->models && i < s->num_models; ++i)
    {
        V4C_FREE(s->models[i].voxels);
        v4c__free_mesh(&s->models[i].mesh);
    }

    for (i = 0; s->nodes && i < s->num_nodes; ++i)
        v4c__free_node(&s->nodes[i]);

    for (i = 0; s->layers && i < s->num_layers; ++i)
        V4C_FREE(s->layers[i].name);

    V4C_FREE(s->models);
    V4C_FREE(s->materials);
    V4C_FREE(s->nodes);
    V4C_FREE(s->layers);
    V4C_FREE(s->cameras);
    V4C_FREE(s);
}

VOX4C_API v4c_result v4c_load_memory(const void* data, size_t size, const v4c_load_options* options,
                                     v4c_scene** out_scene)
{
    v4c__ctx*  c;
    v4c_scene* s;
    v4c_result result;
    if (!out_scene)
        return V4C_ERROR_INVALID_ARGUMENT;

    *out_scene = NULL;
    if (!data)
        return V4C_ERROR_INVALID_ARGUMENT;

    c = (v4c__ctx*)v4c__calloc(1, sizeof(v4c__ctx));
    if (!c)
        return V4C_ERROR_OUT_OF_MEMORY;

    if (options)
        c->opt = *options;
    else
        v4c_default_load_options(&c->opt);

    if (!(c->opt.scale > 0.0f) || (int)c->opt.pivot < 0 || (int)c->opt.pivot > V4C_PIVOT_BOTTOM_CENTER ||
        !v4c__setup_axes(c))
    {
        V4C_FREE(c);
        return V4C_ERROR_INVALID_ARGUMENT;
    }

    {
        uint32_t i;
        for (i = 0; i < 256u; ++i)
        {
            v4c__default_material(&c->matl[i]);
            v4c__default_material(&c->matt[i]);
        }
    }

    v4c__parse(c, (const uint8_t*)data, size);

    s = NULL;
    if (c->result == V4C_SUCCESS)
    {
        s = (v4c_scene*)v4c__calloc(1, sizeof(v4c_scene));
        if (!s)
            v4c__fail(c, V4C_ERROR_OUT_OF_MEMORY);
        else
            v4c__build_scene(c, s);
    }

    result = c->result;
    if (result != V4C_SUCCESS)
    {
        v4c_free_scene(s);
        s = NULL;
    }
    v4c__free_ctx(c);
    V4C_FREE(c);
    *out_scene = s;
    return result;
}

#ifndef V4C_NO_STDIO
VOX4C_API v4c_result v4c_load_file(const char* path, const v4c_load_options* options, v4c_scene** out_scene)
{
    FILE*      f;
    long       len;
    uint8_t*   buf;
    v4c_result r;
    if (!out_scene)
        return V4C_ERROR_INVALID_ARGUMENT;

    *out_scene = NULL;
    if (!path)
        return V4C_ERROR_INVALID_ARGUMENT;

#if defined(_MSC_VER) && _MSC_VER >= 1400
    if (fopen_s(&f, path, "rb") != 0)
        f = NULL;
#else
    f = fopen(path, "rb");
#endif

    if (!f)
        return V4C_ERROR_FILE_IO;

    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return V4C_ERROR_FILE_IO;
    }

    buf = (uint8_t*)V4C_MALLOC(len > 0 ? (size_t)len : 1u);
    if (!buf)
    {
        fclose(f);
        return V4C_ERROR_OUT_OF_MEMORY;
    }

    if (len > 0 && fread(buf, 1, (size_t)len, f) != (size_t)len)
    {
        V4C_FREE(buf);
        fclose(f);
        return V4C_ERROR_FILE_IO;
    }

    fclose(f);
    r = v4c_load_memory(buf, (size_t)len, options, out_scene);
    V4C_FREE(buf);
    return r;
}
#endif

VOX4C_API const char* v4c_result_string(v4c_result r)
{
    switch (r)
    {
    case V4C_SUCCESS: return "success";
    case V4C_ERROR_INVALID_ARGUMENT: return "invalid argument";
    case V4C_ERROR_FILE_IO: return "file could not be read";
    case V4C_ERROR_OUT_OF_MEMORY: return "out of memory";
    case V4C_ERROR_INVALID_FORMAT: return "not a MagicaVoxel .vox file";
    case V4C_ERROR_CORRUPTED_DATA: return "corrupted or truncated .vox data";
    case V4C_ERROR_FRAME_NOT_FOUND: return "requested animation frame does not exist";
    }
    return "unknown error";
}

VOX4C_API v4c_transform v4c_sample_node_transform(const v4c_scene* s, uint32_t node, uint32_t frame)
{
    const v4c_node* n;
    if (!s || node >= s->num_nodes)
    {
        v4c_transform t;
        v4c__identity_transform(&t);
        return t;
    }
    n = &s->nodes[node];
    return v4c__sample_transform(n->transform_keys, n->num_transform_keys, n->transform_loop, frame, s->scale);
}

VOX4C_API int32_t v4c_sample_node_model(const v4c_scene* s, uint32_t node, uint32_t frame)
{
    const v4c_node* n;
    if (!s || node >= s->num_nodes)
        return -1;

    n = &s->nodes[node];
    return v4c__sample_model(n->model_keys, n->num_model_keys, n->model_loop, frame);
}

VOX4C_API void v4c_transform_to_matrix(const v4c_transform* t, float out[16])
{
    int r, col;
    for (col = 0; col < 3; ++col)
    {
        for (r = 0; r < 3; ++r)
            out[col * 4 + r] = t->rotation[r * 3 + col];

        out[col * 4 + 3] = 0.0f;
    }
    out[12] = t->translation[0];
    out[13] = t->translation[1];
    out[14] = t->translation[2];
    out[15] = 1.0f;
}

static void v4c__mat4_mul(const float a[16], const float b[16], float out[16])
{
    float r[16];
    int   i, j, k;
    for (j = 0; j < 4; ++j)
        for (i = 0; i < 4; ++i)
        {
            float sum = 0.0f;
            for (k = 0; k < 4; ++k)
                sum += a[k * 4 + i] * b[j * 4 + k];

            r[j * 4 + i] = sum;
        }
    memcpy(out, r, sizeof(r));
}

VOX4C_API void v4c_node_local_matrix(const v4c_scene* s, uint32_t node, uint32_t frame, float out[16])
{
    v4c_transform t = v4c_sample_node_transform(s, node, frame);
    v4c_transform_to_matrix(&t, out);
}

VOX4C_API void v4c_node_world_matrix(const v4c_scene* s, uint32_t node, uint32_t frame, float out[16])
{
    uint32_t guard = 0;
    int32_t  p;
    v4c_node_local_matrix(s, node, frame, out);
    if (!s || node >= s->num_nodes)
        return;

    p = s->nodes[node].parent;
    while (p >= 0 && (uint32_t)p < s->num_nodes && guard++ < s->num_nodes)
    {
        float parent[16];
        v4c_node_local_matrix(s, (uint32_t)p, frame, parent);
        v4c__mat4_mul(parent, out, out);
        p = s->nodes[p].parent;
    }
}

VOX4C_API bool v4c_node_model_matrix(const v4c_scene* s, uint32_t node, uint32_t frame, float out[16])
{
    float         offset[16];
    const int32_t model = v4c_sample_node_model(s, node, frame);
    v4c_node_world_matrix(s, node, frame, out);
    if (model < 0 || (uint32_t)model >= s->num_models)
        return false;

    memset(offset, 0, sizeof(offset));
    offset[0] = offset[5] = offset[10] = offset[15] = 1.0f;
    offset[12]                                      = s->models[model].scene_offset[0];
    offset[13]                                      = s->models[model].scene_offset[1];
    offset[14]                                      = s->models[model].scene_offset[2];
    v4c__mat4_mul(out, offset, out);
    return true;
}

VOX4C_API bool v4c_node_is_visible(const v4c_scene* s, uint32_t node)
{
    uint32_t guard = 0;
    int32_t  n     = (int32_t)node;
    if (!s || node >= s->num_nodes)
        return false;

    while (n >= 0 && (uint32_t)n < s->num_nodes && guard++ <= s->num_nodes)
    {
        const v4c_node* nd = &s->nodes[n];
        if (nd->hidden)
            return false;

        if (nd->layer >= 0 && (uint32_t)nd->layer < s->num_layers && s->layers[nd->layer].hidden)
            return false;

        n = nd->parent;
    }
    return true;
}

VOX4C_API v4c_transform v4c_sample_node_transform_at(const v4c_scene* s, uint32_t node, float frame,
                                                     v4c_sampling sampling)
{
    const v4c_node* n;
    if (!s || node >= s->num_nodes)
    {
        v4c_transform t;
        v4c__identity_transform(&t);
        return t;
    }
    n = &s->nodes[node];
    return v4c__sample_transform_at(n->transform_keys, n->num_transform_keys, n->transform_loop, frame, s->scale,
                                    sampling);
}

/* Same composition order as v4c_node_world_matrix (identical results). */
static void v4c__node_world_matrix_at(const v4c_scene* s, uint32_t node, float frame, v4c_sampling sampling,
                                      float out[16])
{
    uint32_t      guard = 0;
    int32_t       p;
    v4c_transform t = v4c_sample_node_transform_at(s, node, frame, sampling);
    v4c_transform_to_matrix(&t, out);
    p = s->nodes[node].parent;
    while (p >= 0 && (uint32_t)p < s->num_nodes && guard++ < s->num_nodes)
    {
        float parent[16];
        t = v4c_sample_node_transform_at(s, (uint32_t)p, frame, sampling);
        v4c_transform_to_matrix(&t, parent);
        v4c__mat4_mul(parent, out, out);
        p = s->nodes[p].parent;
    }
}

VOX4C_API uint32_t v4c_frame_instances(const v4c_scene* s, float frame, v4c_sampling sampling, bool visible_only,
                                       v4c_instance* out, uint32_t capacity)
{
    const uint32_t fi    = v4c__frame_index(frame);
    uint32_t       count = 0, n;
    if (!s)
        return 0;

    for (n = 0; n < s->num_nodes; ++n)
    {
        const int32_t model = v4c_sample_node_model(s, n, fi);
        if (model < 0 || (uint32_t)model >= s->num_models || (visible_only && !v4c_node_is_visible(s, n)))
            continue;

        if (out && count < capacity)
        {
            v4c_instance* in = &out[count];
            float         offset[16];
            const float*  M = in->matrix;
            v4c__node_world_matrix_at(s, n, frame, sampling, in->matrix);
            memset(offset, 0, sizeof(offset));
            offset[0] = offset[5] = offset[10] = offset[15] = 1.0f;
            offset[12]                                      = s->models[model].scene_offset[0];
            offset[13]                                      = s->models[model].scene_offset[1];
            offset[14]                                      = s->models[model].scene_offset[2];
            v4c__mat4_mul(in->matrix, offset, in->matrix);
            in->node     = n;
            in->model    = (uint32_t)model;
            in->mirrored = M[0] * (M[5] * M[10] - M[9] * M[6]) - M[4] * (M[1] * M[10] - M[9] * M[2]) +
                               M[8] * (M[1] * M[6] - M[5] * M[2]) <
                           0.0f;
        }
        ++count;
    }
    return count;
}

/* Meshes model `om` of scene `s` from its voxels into `out` (empty if it has none). */
static int v4c__mesh_model(const v4c_scene* s, const v4c_model* om, uint32_t flags, v4c_mesh* out)
{
    v4c__mesher    m;
    uint8_t*       grid;
    const uint32_t bias = s->num_materials >= 256u ? 0u : 1u; /* as when loading */
    size_t         volume;
    uint32_t       i, a;
    int            ok;
    memset(out, 0, sizeof(*out));
    out->clockwise = (flags & V4C_FLAG_CLOCKWISE_WINDING) != 0;
    if (!om->voxels || om->num_voxels == 0)
        return 1;

    volume = (size_t)om->size[0] * om->size[1] * om->size[2];
    grid   = (uint8_t*)v4c__calloc(volume ? volume : 1u, 1);
    if (!grid)
        return 0;

    for (i = 0; i < om->num_voxels; ++i)
    {
        const v4c_voxel* v = &om->voxels[i];
        if (v->x < om->size[0] && v->y < om->size[1] && v->z < om->size[2])
            grid[v->x + (size_t)om->size[0] * (v->y + (size_t)om->size[1] * v->z)] = (uint8_t)(v->material + bias);
    }

    memset(&m, 0, sizeof(m));
    m.grid          = grid;
    m.bias          = bias;
    m.scale         = s->scale;
    m.inv_materials = 1.0f / (float)(s->num_materials ? s->num_materials : 1u);
    m.clockwise     = (flags & V4C_FLAG_CLOCKWISE_WINDING) ? 1 : 0;
    m.use_packed    = (flags & V4C_FLAG_PACKED_VERTICES) ? 1 : 0;
    for (a = 0; a < 3u; ++a)
    {
        m.size[a]  = om->size[a];
        m.pivot[a] = om->pivot[a];
    }

    ok = v4c__build_mesh(&m, s->num_materials, out);
    v4c__mesher_free_scratch(&m);
    V4C_FREE(grid);
    if (ok && (flags & V4C_FLAG_16BIT_INDICES))
        ok = v4c__mesh_to_16bit(out);

    if (!ok)
        v4c__free_mesh(out);

    return ok;
}

VOX4C_API v4c_result v4c_build_model_mesh(v4c_scene* s, uint32_t model, uint32_t flags)
{
    v4c_model* om;
    v4c_mesh   mesh;
    if (!s || model >= s->num_models)
        return V4C_ERROR_INVALID_ARGUMENT;

    om = &s->models[model];
    if (!om->voxels && om->num_voxels > 0) /* loaded with V4C_FLAG_DISCARD_VOXELS */
        return V4C_ERROR_INVALID_ARGUMENT;

    if (!v4c__mesh_model(s, om, flags, &mesh))
        return V4C_ERROR_OUT_OF_MEMORY;

    v4c__free_mesh(&om->mesh);
    om->mesh = mesh;
    return V4C_SUCCESS;
}

VOX4C_API void v4c_free_model_mesh(v4c_scene* s, uint32_t model)
{
    if (s && model < s->num_models)
        v4c__free_mesh(&s->models[model].mesh);
}

VOX4C_API void v4c_free_mesh(v4c_mesh* mesh)
{
    if (mesh)
        v4c__free_mesh(mesh);
}

VOX4C_API uint32_t v4c_mesh_index(const v4c_mesh* mesh, uint32_t i)
{
    if (!mesh || i >= mesh->num_indices)
        return 0;

    if (mesh->indices16)
        return mesh->indices16[i];

    return mesh->indices ? mesh->indices[i] : 0u;
}

/* v4c_bake_frame, step 1: the mesh of every model drawn in the frame (its own, or a
   temporary one built from the voxels into temp[]) and the index count per material */
static v4c_result v4c__bake_collect(const v4c_scene* s, const v4c_instance* inst, uint32_t num_inst,
                                    const v4c_mesh** meshes, v4c_mesh* temp, uint32_t counts[256],
                                    uint64_t* total_vertices, uint64_t* total_indices)
{
    uint32_t i, k;
    memset(counts, 0, 256u * sizeof(uint32_t));
    *total_vertices = 0;
    *total_indices  = 0;
    for (i = 0; i < num_inst; ++i)
    {
        const uint32_t  m = inst[i].model;
        const v4c_mesh* mesh;
        if (!meshes[m])
        {
            const v4c_model* om = &s->models[m];
            if (om->mesh.num_vertices > 0 || om->num_voxels == 0)
                meshes[m] = &om->mesh;
            else if (!om->voxels) /* neither a mesh nor voxels (V4C_FLAG_DISCARD_VOXELS) */
                return V4C_ERROR_INVALID_ARGUMENT;
            else if (v4c__mesh_model(s, om, 0, &temp[m]))
                meshes[m] = &temp[m];
            else
                return V4C_ERROR_OUT_OF_MEMORY;
        }
        mesh = meshes[m];
        *total_vertices += mesh->num_vertices;
        *total_indices += mesh->num_indices;
        for (k = 0; k < mesh->num_submeshes; ++k)
            counts[mesh->submeshes[k].material & 255u] += mesh->submeshes[k].num_indices;
    }

    if (*total_vertices > 0xFFFFFFFFu || *total_indices > 0xFFFFFFFFu)
        return V4C_ERROR_OUT_OF_MEMORY;

    return V4C_SUCCESS;
}

/* v4c_bake_frame, step 2: allocates `out` and writes every instance into it, the
   triangles grouped by material; on failure the caller frees `out` */
static v4c_result v4c__bake_fill(const v4c_scene* s, const v4c_instance* inst, uint32_t num_inst,
                                 const v4c_mesh* const* meshes, const uint32_t counts[256], uint32_t num_vertices,
                                 uint32_t num_indices, int want_cw, v4c_mesh* out)
{
    uint32_t offsets[256];
    uint32_t i, k, num_sub = 0, base = 0, running = 0;
    for (k = 0; k < 256u; ++k)
        if (counts[k])
            ++num_sub;

    out->num_vertices  = num_vertices;
    out->num_indices   = num_indices;
    out->num_submeshes = num_sub;
    out->positions     = (float*)v4c__calloc((size_t)num_vertices * 3u, sizeof(float));
    out->normals       = (float*)v4c__calloc((size_t)num_vertices * 3u, sizeof(float));
    out->texcoords     = (float*)v4c__calloc((size_t)num_vertices * 2u, sizeof(float));
    out->materials     = (uint8_t*)v4c__calloc((size_t)num_vertices, 1);
    out->indices       = (uint32_t*)v4c__calloc((size_t)num_indices, sizeof(uint32_t));
    out->submeshes     = (v4c_submesh*)v4c__calloc(num_sub, sizeof(v4c_submesh));
    if (!out->positions || !out->normals || !out->texcoords || !out->materials || !out->indices || !out->submeshes)
        return V4C_ERROR_OUT_OF_MEMORY;

    num_sub = 0;
    for (k = 0; k < 256u; ++k)
    {
        offsets[k] = running;
        if (counts[k])
        {
            out->submeshes[num_sub].material    = k;
            out->submeshes[num_sub].first_index = running;
            out->submeshes[num_sub].num_indices = counts[k];
            ++num_sub;
        }
        running += counts[k];
    }

    for (i = 0; i < num_inst; ++i)
    {
        const v4c_mesh* mesh = meshes[inst[i].model];
        const float*    M    = inst[i].matrix;
        const int       flip = ((int)inst[i].mirrored ? 1 : 0) ^ ((int)mesh->clockwise ? 1 : 0) ^ want_cw;
        v4c_model       om   = s->models[inst[i].model];
        uint32_t        v;
        om.mesh = *mesh; /* v4c_mesh_vertex reads the temporary mesh too */
        for (v = 0; v < mesh->num_vertices; ++v)
        {
            float          p[3], nrm[3], tc[2];
            float*         op  = &out->positions[(size_t)(base + v) * 3u];
            float*         on  = &out->normals[(size_t)(base + v) * 3u];
            const uint32_t mat = v4c_mesh_vertex(s, &om, v, p, nrm, tc);
            int            r;
            for (r = 0; r < 3; ++r)
            {
                op[r] = M[0 * 4 + r] * p[0] + M[1 * 4 + r] * p[1] + M[2 * 4 + r] * p[2] + M[3 * 4 + r];
                on[r] = M[0 * 4 + r] * nrm[0] + M[1 * 4 + r] * nrm[1] + M[2 * 4 + r] * nrm[2];
            }
            out->texcoords[(size_t)(base + v) * 2u + 0] = tc[0];
            out->texcoords[(size_t)(base + v) * 2u + 1] = tc[1];
            out->materials[base + v]                    = (uint8_t)mat;
        }

        for (k = 0; k < mesh->num_submeshes; ++k)
        {
            const v4c_submesh* sub = &mesh->submeshes[k];
            uint32_t*          dst = &out->indices[offsets[sub->material & 255u]];
            uint32_t           t;
            for (t = 0; t + 2u < sub->num_indices; t += 3u)
            {
                const uint32_t i0 = v4c_mesh_index(mesh, sub->first_index + t) + base;
                const uint32_t i1 = v4c_mesh_index(mesh, sub->first_index + t + 1u) + base;
                const uint32_t i2 = v4c_mesh_index(mesh, sub->first_index + t + 2u) + base;
                dst[t]            = i0;
                dst[t + 1u]       = flip ? i2 : i1;
                dst[t + 2u]       = flip ? i1 : i2;
            }
            offsets[sub->material & 255u] += sub->num_indices;
        }
        base += mesh->num_vertices;
    }
    return V4C_SUCCESS;
}

VOX4C_API v4c_result v4c_bake_frame(const v4c_scene* s, float frame, v4c_sampling sampling, bool visible_only,
                                    uint32_t flags, v4c_mesh* out)
{
    v4c_instance*    inst   = NULL;
    const v4c_mesh** meshes = NULL;
    v4c_mesh*        temp   = NULL;
    uint32_t         counts[256];
    uint64_t         total_vertices = 0, total_indices = 0;
    uint32_t         num_inst, i;
    const int        want_cw = (flags & V4C_FLAG_CLOCKWISE_WINDING) != 0;
    v4c_result       res     = V4C_SUCCESS;
    if (!s || !out)
        return V4C_ERROR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));
    out->clockwise = want_cw != 0;
    num_inst       = v4c_frame_instances(s, frame, sampling, visible_only, NULL, 0);
    if (num_inst == 0)
        return V4C_SUCCESS;

    inst   = (v4c_instance*)v4c__calloc(num_inst, sizeof(v4c_instance));
    meshes = (const v4c_mesh**)v4c__calloc(s->num_models, sizeof(const v4c_mesh*));
    temp   = (v4c_mesh*)v4c__calloc(s->num_models, sizeof(v4c_mesh));
    if (!inst || !meshes || !temp)
    {
        res = V4C_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        num_inst = v4c_frame_instances(s, frame, sampling, visible_only, inst, num_inst);
        res      = v4c__bake_collect(s, inst, num_inst, meshes, temp, counts, &total_vertices, &total_indices);
    }

    /* an empty frame (models without voxels) stays an empty mesh */
    if (res == V4C_SUCCESS && total_vertices > 0 && total_indices > 0)
    {
        res = v4c__bake_fill(s, inst, num_inst, meshes, counts, (uint32_t)total_vertices, (uint32_t)total_indices,
                             want_cw, out);
        if (res == V4C_SUCCESS && (flags & V4C_FLAG_16BIT_INDICES) && !v4c__mesh_to_16bit(out))
            res = V4C_ERROR_OUT_OF_MEMORY;
    }

    /* single clean-up for every path: the temporary meshes and work arrays always,
       the result only on failure */
    if (temp)
        for (i = 0; i < s->num_models; ++i)
            v4c__free_mesh(&temp[i]);

    V4C_FREE(temp);
    V4C_FREE((void*)meshes);
    V4C_FREE(inst);
    if (res != V4C_SUCCESS)
        v4c__free_mesh(out);

    return res;
}

VOX4C_API bool v4c_scene_bounds(const v4c_scene* s, uint32_t frame, bool visible_only, float mn[3], float mx[3])
{
    uint32_t n;
    bool     any = false;
    int      k;
    if (!s || !mn || !mx)
        return false;

    for (k = 0; k < 3; ++k)
        mn[k] = mx[k] = 0.0f;

    for (n = 0; n < s->num_nodes; ++n)
    {
        const v4c_model* m;
        float            M[16], lo[3], hi[3];
        int32_t          model = v4c_sample_node_model(s, n, frame);
        uint32_t         corner;
        if (model < 0 || (uint32_t)model >= s->num_models || (visible_only && !v4c_node_is_visible(s, n)))
            continue;

        if (!v4c_node_model_matrix(s, n, frame, M))
            continue;

        m = &s->models[model];
        for (k = 0; k < 3; ++k)
        {
            lo[k] = (0.0f - m->pivot[k]) * s->scale;
            hi[k] = ((float)m->size[k] - m->pivot[k]) * s->scale;
        }

        for (corner = 0; corner < 8u; ++corner)
        {
            float p[3], w[3];
            p[0] = (corner & 1u) ? hi[0] : lo[0];
            p[1] = (corner & 2u) ? hi[1] : lo[1];
            p[2] = (corner & 4u) ? hi[2] : lo[2];
            for (k = 0; k < 3; ++k)
                w[k] = M[0 * 4 + k] * p[0] + M[1 * 4 + k] * p[1] + M[2 * 4 + k] * p[2] + M[3 * 4 + k];

            for (k = 0; k < 3; ++k)
            {
                if (!any || w[k] < mn[k])
                    mn[k] = w[k];
                if (!any || w[k] > mx[k])
                    mx[k] = w[k];
            }
            any = true;
        }
    }
    return any;
}

VOX4C_API bool v4c_sun_area_light(const v4c_scene* s, const float target[3], float distance, v4c_area_light* out)
{
    const v4c_sun* sun;
    double         sa, ca, e, d;
    int            k;
    if (!s || !target || !out || !(distance > 0.0f))
        return false;

    sun = &s->render.sun;
    if (!sun->present || !(sun->area > 0.0f))
        return false;

    /* half of the apparent diameter */
    v4c__sincos(v4c__deg_to_rad(0.5 * (double)sun->angular_diameter), &sa, &ca);
    e = (double)sun->intensity; /* irradiance of the directional light */
    d = (double)distance;
    for (k = 0; k < 3; ++k)
    {
        out->normal[k]   = sun->direction[k];
        out->position[k] = target[k] - sun->direction[k] * distance;
    }
    out->distance = distance;
    out->radius   = (float)(d * sa / ca);
    /* a Lambertian disk of radiance L subtending the half angle a gives the
       irradiance E = pi * L * sin^2(a) on its axis; its flux is pi * L * area */
    out->radiance  = (float)(e / (3.14159265358979323846 * sa * sa));
    out->power     = (float)(3.14159265358979323846 * d * d * e / (ca * ca));
    out->intensity = (float)(d * d * e / (ca * ca));
    return true;
}

VOX4C_API void v4c_camera_pose(const v4c_scene* s, const v4c_camera* cam, float position[3], float forward[3],
                               float up[3])
{
    double sp, cp, sy, cy, f[3], u[3];
    float  fo[3], uo[3];
    int    k;
    if (!s || !cam)
        return;

    v4c__sincos(v4c__deg_to_rad((double)cam->angle[0]), &sp, &cp);
    v4c__sincos(v4c__deg_to_rad((double)cam->angle[1]), &sy, &cy);
    /* MagicaVoxel axes, verified against MagicaVoxel screenshots: angles 0 0 0
       look along +Y at the front of the model with +Z up; negative pitch looks
       down (pitch -45 sees the top and the front face); positive yaw orbits the
       camera to the right around +Z (yaw 90 sees the +X face). The third
       angle is not a roll (isometric slots store e.g. 34 without any tilt of
       the view) and is not applied. */
    {
        /* pitch around X, then yaw around Z */
        const double f0[3] = { 0.0, cp, sp };  /* forward after pitch */
        const double u0[3] = { 0.0, -sp, cp }; /* up after pitch      */
        f[0]               = f0[0] * cy - f0[1] * sy;
        f[1]               = f0[0] * sy + f0[1] * cy;
        f[2]               = f0[2];
        u[0]               = u0[0] * cy - u0[1] * sy;
        u[1]               = u0[0] * sy + u0[1] * cy;
        u[2]               = u0[2];
    }
    v4c__axes_dir(s->axes, f, fo);
    v4c__axes_dir(s->axes, u, uo);
    for (k = 0; k < 3; ++k)
    {
        if (position)
            position[k] = cam->focus[k] - fo[k] * cam->radius;

        if (forward)
            forward[k] = fo[k];

        if (up)
            up[k] = uo[k];
    }
}

/* camera basis in output coordinates, in double */
static int v4c__camera_basis(const v4c_scene* s, const v4c_camera* cam, double pos[3], double r[3], double u[3],
                             double f[3])
{
    float p[3], fw[3], up[3];
    int   k;
    if (!s || !cam)
        return 0;

    v4c_camera_pose(s, cam, p, fw, up);
    for (k = 0; k < 3; ++k)
    {
        pos[k] = p[k];
        f[k]   = fw[k];
        u[k]   = up[k];
    }

    /* right = forward x up in a right handed system, up x forward in a left handed one */
    r[0] = f[1] * u[2] - f[2] * u[1];
    r[1] = f[2] * u[0] - f[0] * u[2];
    r[2] = f[0] * u[1] - f[1] * u[0];
    if (s->left_handed)
        for (k = 0; k < 3; ++k)
            r[k] = -r[k];

    return 1;
}

VOX4C_API bool v4c_camera_ray(const v4c_scene* s, const v4c_camera* cam, v4c_projection projection, float fov,
                              uint32_t width, uint32_t height, float px, float py, float origin[3], float dir[3])
{
    const double pi = 3.14159265358979323846;
    double       pos[3], r[3], u[3], f[3], d[3], sx, sy, len;
    int          k;
    if (!origin || !dir || width == 0 || height == 0 || !v4c__camera_basis(s, cam, pos, r, u, f))
        return false;

    /* image plane: vertical -1..1 over the height, horizontal scaled by the aspect */
    sx = ((double)px - 0.5 * (double)width) / (0.5 * (double)height);
    sy = (0.5 * (double)height - (double)py) / (0.5 * (double)height);
    if (cam->mode == V4C_CAMERA_PANORAMA)
        projection = V4C_PROJECTION_PANORAMA;

    if (cam->mode == V4C_CAMERA_ORTHOGRAPHIC || cam->mode == V4C_CAMERA_ISOMETRIC)
    {
        const double half = (double)cam->frustum;
        for (k = 0; k < 3; ++k)
        {
            origin[k] = (float)(pos[k] + (r[k] * sx + u[k] * sy) * half);
            dir[k]    = (float)f[k];
        }
        return true;
    }

    if (projection == V4C_PROJECTION_PANORAMA)
    {
        /* pi / height radians per pixel on both axes; wider than 2:1 repeats
           the directions every 2 * height pixels (checked with renders) */
        const double az = sx * 0.5 * pi;
        const double el = sy * 0.5 * pi;
        double       saz, caz, sel, cel;
        if (el > 0.5 * pi || el < -0.5 * pi)
            return false;

        v4c__sincos(az, &saz, &caz);
        v4c__sincos(el, &sel, &cel);
        for (k = 0; k < 3; ++k)
            d[k] = f[k] * cel * caz + r[k] * cel * saz + u[k] * sel;
    }
    else if (projection == V4C_PROJECTION_STEREOGRAPHIC)
    {
        double sq, cq, q2;
        v4c__sincos(v4c__deg_to_rad(0.25 * (double)fov), &sq, &cq);
        sx *= sq / cq;
        sy *= sq / cq;
        q2 = sx * sx + sy * sy;
        for (k = 0; k < 3; ++k)
            d[k] = ((1.0 - q2) * f[k] + 2.0 * sx * r[k] + 2.0 * sy * u[k]) / (1.0 + q2);
    }
    else /* perspective (and unknown lenses) */
    {
        double st, ct;
        v4c__sincos(v4c__deg_to_rad(0.5 * (double)fov), &st, &ct);
        for (k = 0; k < 3; ++k)
            d[k] = f[k] + (r[k] * sx + u[k] * sy) * (st / ct);
    }

    len = v4c__sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(len > 0.0))
        return false;

    for (k = 0; k < 3; ++k)
    {
        origin[k] = (float)pos[k];
        dir[k]    = (float)(d[k] / len);
    }
    return true;
}

VOX4C_API bool v4c_camera_project(const v4c_scene* s, const v4c_camera* cam, v4c_projection projection, float fov,
                                  uint32_t width, uint32_t height, const float point[3], float pixel[2])
{
    const double pi = 3.14159265358979323846;
    double       pos[3], r[3], u[3], f[3], d[3], dr, du, df, sx, sy;
    int          k;
    if (!point || !pixel || width == 0 || height == 0 || !v4c__camera_basis(s, cam, pos, r, u, f))
        return false;

    for (k = 0; k < 3; ++k)
        d[k] = (double)point[k] - pos[k];

    dr = d[0] * r[0] + d[1] * r[1] + d[2] * r[2];
    du = d[0] * u[0] + d[1] * u[1] + d[2] * u[2];
    df = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
    if (cam->mode == V4C_CAMERA_PANORAMA)
        projection = V4C_PROJECTION_PANORAMA;

    if (cam->mode == V4C_CAMERA_ORTHOGRAPHIC || cam->mode == V4C_CAMERA_ISOMETRIC)
    {
        if (!(cam->frustum > 0.0f))
            return false;
        sx = dr / (double)cam->frustum;
        sy = du / (double)cam->frustum;
    }
    else if (projection == V4C_PROJECTION_PANORAMA)
    {
        const double len = v4c__sqrt(dr * dr + du * du + df * df);
        if (!(len > 0.0))
            return false;

        /* the copy of the direction nearest to the image centre */
        pixel[0] = (float)(0.5 * (double)width + v4c__atan2(dr, df) / pi * (double)height);
        pixel[1] = (float)((0.5 - v4c__atan2(du, v4c__sqrt(dr * dr + df * df)) / pi) * (double)height);
        return true;
    }
    else if (projection == V4C_PROJECTION_STEREOGRAPHIC)
    {
        const double len = v4c__sqrt(dr * dr + du * du + df * df);
        double       sq, cq, den;
        if (!(len > 0.0))
            return false;

        den = 1.0 + df / len;
        if (!(den > 1e-12))
            return false;

        v4c__sincos(v4c__deg_to_rad(0.25 * (double)fov), &sq, &cq);
        sx = (dr / len) / den * (cq / sq);
        sy = (du / len) / den * (cq / sq);
    }
    else
    {
        double st, ct;
        if (!(df > 0.0))
            return false;

        v4c__sincos(v4c__deg_to_rad(0.5 * (double)fov), &st, &ct);
        sx = dr / df * (ct / st);
        sy = du / df * (ct / st);
    }

    pixel[0] = (float)(0.5 * (double)width + sx * 0.5 * (double)height);
    pixel[1] = (float)(0.5 * (double)height - sy * 0.5 * (double)height);
    return true;
}

VOX4C_API void v4c_palette_texture(const v4c_scene* s, uint8_t* out_rgba)
{
    uint32_t i;
    if (!s || !out_rgba)
        return;

    for (i = 0; i < s->num_materials; ++i)
    {
        out_rgba[i * 4 + 0] = s->materials[i].color.r;
        out_rgba[i * 4 + 1] = s->materials[i].color.g;
        out_rgba[i * 4 + 2] = s->materials[i].color.b;
        out_rgba[i * 4 + 3] = s->materials[i].color.a;
    }
}

VOX4C_API void v4c_unpack_vertex(const v4c_scene* s, const v4c_model* model, const v4c_packed_vertex* vertex,
                                 float position[3], float normal[3], float texcoord[2])
{
    if (!s || !model || !vertex)
        return;

    if (position)
    {
        position[0] = ((float)vertex->x - model->pivot[0]) * s->scale;
        position[1] = ((float)vertex->y - model->pivot[1]) * s->scale;
        position[2] = ((float)vertex->z - model->pivot[2]) * s->scale;
    }

    if (normal)
    {
        const uint32_t axis = (uint32_t)(vertex->normal >> 1) % 3u;
        normal[0] = normal[1] = normal[2] = 0.0f;
        normal[axis]                      = (vertex->normal & 1u) ? -1.0f : 1.0f;
    }

    if (texcoord)
    {
        texcoord[0] = ((float)vertex->material + 0.5f) * (1.0f / (float)(s->num_materials ? s->num_materials : 1u));
        texcoord[1] = 0.5f;
    }
}

VOX4C_API uint32_t v4c_mesh_vertex(const v4c_scene* s, const v4c_model* model, uint32_t index, float position[3],
                                   float normal[3], float texcoord[2])
{
    const v4c_mesh* m;
    uint32_t        a;
    if (!s || !model || index >= model->mesh.num_vertices)
        return 0;

    m = &model->mesh;
    if (m->packed)
    {
        v4c_unpack_vertex(s, model, &m->packed[index], position, normal, texcoord);
        return m->packed[index].material;
    }

    for (a = 0; a < 3u; ++a)
    {
        if (position)
            position[a] = m->positions[index * 3u + a];

        if (normal)
            normal[a] = m->normals[index * 3u + a];
    }

    if (texcoord)
    {
        texcoord[0] = m->texcoords[index * 2u + 0];
        texcoord[1] = m->texcoords[index * 2u + 1];
    }
    return m->materials[index];
}

#endif /* VOX4C_IMPLEMENTATION_DONE */
#endif /* VOX4C_IMPLEMENTATION */

/*
MIT License

Copyright (c) 2025-2026 Muppetsg2

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
