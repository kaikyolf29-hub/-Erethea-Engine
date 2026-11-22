def can_build(env, platform):
    # The assistant panel lives in the editor and acts through the MCP tools.
    if not env.editor_build:
        return False
    env.module_add_dependencies("erethea_assistant", ["erethea_mcp"])
    return True


def configure(env):
    pass
