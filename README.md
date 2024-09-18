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

## Controls
WASD + Space + Ctrl to move, Alt to unlock the cursor.

## Showcase
https://github.com/user-attachments/assets/447f4425-b7ab-48fc-8955-1ada4bed7fe7

![grass scene](https://github.com/user-attachments/assets/d656c4a0-edc9-4089-9ce9-8ffa35dae562)

![menger scene](https://github.com/user-attachments/assets/889fea43-7c81-4baa-8add-24070df83815)

![dragon scene](https://github.com/user-attachments/assets/7f4560c7-46e1-4565-a067-32feb8f5636e)

![minecraft scene](https://github.com/user-attachments/assets/21322e64-10d2-44a4-97d5-fee59943cb3d)

## References
- [Amanatides & Woo “A Fast Voxel Traversal Algorithm For Ray Tracing”](https://www.researchgate.net/publication/2611491_A_Fast_Voxel_Traversal_Algorithm_for_Ray_Tracing)
- [MagicaVoxel file loading](https://github.com/jpaver/opengametools)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [Temporal Reprojection Anti-Aliasing in INSIDE](https://www.youtube.com/watch?v=2XXS5UyNjjU)
- [Learning OpenGL](https://learnopengl.com)
