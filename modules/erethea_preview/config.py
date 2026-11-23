def can_build(env, platform):
    # The preview loads glTF files at runtime and draws them in 3D. The
    # Erethea quality presets are used when that module is built too.
    if env["disable_3d"]:
        return False
    env.module_add_dependencies("erethea_preview", ["gltf"])
    env.module_add_dependencies("erethea_preview", ["erethea_quality"], True)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "EretheaPreview",
    ]


def get_doc_path():
    return "doc_classes"
