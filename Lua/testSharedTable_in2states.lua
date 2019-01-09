print("do testSharedTable_in2states.lua")
print("\n1:")

local t = SharedTable.acquire("theSharedTable")

print("\ndump(t):")
SharedTable.dump(t, false)

print("\nThe end.")


