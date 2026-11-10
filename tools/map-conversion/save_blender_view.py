import sys
from pathlib import Path

import bpy
from mathutils import Vector


source, output = (Path(arg) for arg in sys.argv[sys.argv.index("--") + 1:])
for obj in list(bpy.data.objects):
    bpy.data.objects.remove(obj, do_unlink=True)

bpy.ops.import_scene.gltf(filepath=str(source))
bpy.ops.object.select_all(action="DESELECT")
bpy.context.view_layer.objects.active = None
bpy.data.objects["Cielo y fondo"].hide_set(True)

view_center = Vector((0, 0, 40))
view_offset = Vector((250, -2150, 3150))
rotation = (-view_offset).to_track_quat("-Z", "Y")
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type != "VIEW_3D":
            continue
        view = area.spaces.active
        view.clip_start = 5
        view.clip_end = 50000
        view.shading.type = "MATERIAL"
        view.overlay.show_floor = False
        view.overlay.show_axis_x = False
        view.overlay.show_axis_y = False
        view.region_3d.view_perspective = "ORTHO"
        view.region_3d.view_location = view_center
        view.region_3d.view_rotation = rotation
        view.region_3d.view_distance = 6400

bpy.ops.wm.save_as_mainfile(filepath=str(output))
bpy.ops.wm.open_mainfile(filepath=str(output))
meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
assert len(meshes) == 6 and sum(len(obj.data.polygons) for obj in meshes) == 41067
assert sum(bool(image.packed_file) for image in bpy.data.images) == 100
assert bpy.data.objects["Cielo y fondo"].hide_get()
assert not bpy.context.selected_objects
view = next(area.spaces.active for area in bpy.context.screen.areas if area.type == "VIEW_3D")
assert view.shading.type == "MATERIAL"
assert view.region_3d.view_perspective == "ORTHO"
assert view.clip_start == 5 and view.clip_end == 50000
print("Verified saved scene, 100 packed textures, and overview settings", flush=True)
