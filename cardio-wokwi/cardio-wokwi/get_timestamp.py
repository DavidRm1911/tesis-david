Import("env")
import time
env.Append(CPPDEFINES=[("BUILD_TIMESTAMP", str(int(time.time())))])
