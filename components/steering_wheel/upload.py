Import("env")

def on_upload(source, target, env):
    bin_path = str(source[0])

    env.Execute(f"st-flash write {bin_path} 0x08000000")

env.Replace(UPLOADCMD=on_upload)
