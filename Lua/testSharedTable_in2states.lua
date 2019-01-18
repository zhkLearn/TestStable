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

print("\n3:")
local t3 = SharedTable.new()
t3.subT = t.newSub
SharedTable.dump(t3, false)

t3 = {1, 2, 3}
SharedTable.dump(t3, false)

print("\nThe end.")


