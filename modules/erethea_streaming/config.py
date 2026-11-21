def can_build(env, platform):
    env.module_add_dependencies("erethea_streaming", ["texture_streaming"])
    return env["rendering_device"]


def configure(env):
    pass
