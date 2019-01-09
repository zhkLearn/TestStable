print("do testSharedTable_in2states.lua")
print("\n1:")

local t = SharedTable.acquire("theSharedTable")

print("\ndump(t):")
SharedTable.dump(t, false)

print("\n2:")
local subT = {7, 8, 9}
t.newSub = subT
t.newSub2 = t.newSub

SharedTable.dump(t, false)

print("\nThe end.")


