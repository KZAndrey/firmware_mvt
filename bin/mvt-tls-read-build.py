"""Register before PlatformIO builds source objects (pre extra script)."""
Import("env")

def mvt_tls_read_no_lto(build_env, node):
    adapter_env = build_env.Clone()
    adapter_env.Append(CCFLAGS=["-fno-lto"])
    return adapter_env.Object(node)

env.AddBuildMiddleware(mvt_tls_read_no_lto, "*MvtTlsRead.cpp")
