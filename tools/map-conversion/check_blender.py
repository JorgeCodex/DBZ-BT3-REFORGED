import sys
from pathlib import Path

import bpy
from mathutils import Vector


source, preview = (Path(p) for p in sys.argv[sys.argv.index("--") + 1:])
for obj in list(bpy.data.objects):
    bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.import_scene.gltf(filepath=str(source))
meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
images = [img for img in bpy.data.images if img.packed_file]
print("Validated Blender import", [(obj.name, len(obj.data.polygons)) for obj in meshes], len(images), flush=True)
assert len(meshes) == 6, [obj.name for obj in meshes]
assert sum(len(obj.data.polygons) for obj in meshes) >= 40500
assert len(images) == 100 and all(all(img.size) for img in images)

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 4
scene.render.resolution_x = 780
scene.render.resolution_y = 650
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(preview)
scene.view_settings.view_transform = "Standard"
camera = bpy.data.objects.new("Preview camera", bpy.data.cameras.new("Preview camera"))
scene.collection.objects.link(camera)
camera.location = (250, -2150, 3150)
camera.rotation_euler = (Vector((0, 0, 0)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 4950
camera.data.clip_end = 40000
scene.camera = camera
for obj in meshes:
    obj.hide_render = obj.name.startswith("Cielo")
bpy.ops.render.render(write_still=True)
