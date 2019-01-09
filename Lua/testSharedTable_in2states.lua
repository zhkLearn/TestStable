print("\n1:")

local t = SharedTable.acquire("theSharedTable")

print("\ndump(t):")
SharedTable.dump(t, false)

print("\nend")


