def can_build(env, platform):
    # Ray traced effects need the RenderingDevice (Forward+), and 3D.
    return env["rendering_device"] and not env["disable_3d"]


def configure(env):
    pass


def get_doc_classes():
    return [
        "EretheaRayTracedShadows",
    ]


def get_doc_path():
    return "doc_classes"
