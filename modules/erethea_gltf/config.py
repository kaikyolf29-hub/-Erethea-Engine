def can_build(env, platform):
    # Reads the material extensions of glTF files that Godot's importer skips.
    if env["disable_3d"]:
        return False
    env.module_add_dependencies("erethea_gltf", ["gltf"])
    return True


def configure(env):
    pass
