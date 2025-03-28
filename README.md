# Unnamed Voxel Path Tracer Engine
A real-time voxel path-tracer using a fast DDA voxel traversal algorithm and spatiotemporal reprojection.

## Scene Structure
The scene consists of low level 8x8x8 voxel grids called 'bricks' and a high level 'brick map' grid that specifies which brick to use in each cell.

- Scenes are loaded entirely from .scene and MagicaVoxel .vox files.
- Each brick can contain up to 15 materials, with color, emission, and roughness properties loaded from MagicaVoxel.
- Each brick-map can contain up to 15 bricks that should be specified in the pallet in order (pallet colors 1-15).
- The camera is initialized to the saved camera in the 0 slot in the brick-map file.
- The sky can be either a sample sky shader, or a uniform color loaded from the brick-map pallet in index 255 (with an emission mat applied).

### Scene file
A scene file should start with the brick-map MagicaVoxel file, followed by 'sky' or 'color' depending on the sky choice, and then all of the brick MagicaVoxel files in order, all seperated by whitespaces (see assets folder for examples).
If the 'sky' setting is selected it should be followed with the sun strength.

## Controls
WASD + Space + Ctrl to move, Alt to unlock the cursor. 

Right-click to place brick, Left-click to destroy.

## Showcase
https://github.com/user-attachments/assets/d9015675-0146-43d1-ad05-2ede722969cf

https://github.com/user-attachments/assets/8badfe50-44cb-48e6-b318-e0515b3b5902

https://github.com/user-attachments/assets/8576d92c-cb66-40fa-bfd5-c26ba504d4b2





![grass scene](https://github.com/user-attachments/assets/a43d77ef-5d65-442c-8975-39d3a3d87edb)

![menger scene](https://github.com/user-attachments/assets/125587a6-624c-4843-b7d6-348da8f12803)

![dragon scene](https://github.com/user-attachments/assets/7f4560c7-46e1-4565-a067-32feb8f5636e)

![minecraft scene](https://github.com/user-attachments/assets/031a6b56-c7ec-47bd-a630-3a4191ea3899)

## What's Next?
- Better material lighting
- Add higher level 4x4x4 bitmap
- Overhaul scene representation to use a Sparse Voxel Octree?
- Minecraft world/schematic loading?

## References
- [Amanatides & Woo “A Fast Voxel Traversal Algorithm For Ray Tracing”](https://www.researchgate.net/publication/2611491_A_Fast_Voxel_Traversal_Algorithm_for_Ray_Tracing)
- [MagicaVoxel file loading](https://github.com/jpaver/opengametools)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [Temporal Reprojection Anti-Aliasing in INSIDE](https://www.youtube.com/watch?v=2XXS5UyNjjU)
- [Learning OpenGL](https://learnopengl.com)
