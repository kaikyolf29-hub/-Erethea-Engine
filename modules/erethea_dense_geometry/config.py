def can_build(env, platform):
    # Dense meshes are split when the editor imports them.
    return env.editor_build and not env["disable_3d"]


def configure(env):
    pass
