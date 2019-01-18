print("\n1:")

local sraw = require "StableRaw"
print("\n1.1:")

--sraw.init()  -- init lightuserdata metatable


print("\n2:")

local t = sraw.new()


print("\n3:")

local subT = {1, 2, 3}
t.hello = {world = true, "one", "two", 3, False = false, testT = {true, 3, testttt = {"Alpha", nil, 6}}} 


print("\n4:")

t.subT = subT
sraw.dump(t, true)

print("\n5:")

----[[

function dumpSTable(tbl)
	for k,v in pairs(tbl) do  -- or use sraw.pairs
		if type(v) == "userdata" or type(v) == "table" then
			print(k)
			dumpSTable(v)
		else
			print(k, v)
		end
	end
end

dumpSTable(t)

sraw.share(t)

local fromC = sraw.acquire("tname")
dumpSTable(fromC)
fromC = nil

print("\n6:")

local t2 = {nil, "Monday", "Tuesday", "Wednesday"}
t2[-1] = 12123
t2[0] = 123
t2[6] = 123
dumpSTable(t2)
print(#t2)

local t3 = sraw.new()
t3.values = t2
sraw.dump(t3, false)

--]]--






