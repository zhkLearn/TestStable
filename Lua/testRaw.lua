print("\n1:")

--local sraw = require "StableRaw"
sraw.init()  -- init lightuserdata metatable


print("\n2:")

local t = sraw.create()


print("\n3:")

local subT = {1, 2, 3}
t.hello = {world = true, "one", "two", 3, False = false, testT = {true, 3, testttt = {"Alpha", nil, 6}}} 


print("\n4:")

t.subT = subT
sraw.dump(t)

print("\n5:")

function dumpSTable(tbl)
	for k,v in pairs(tbl) do  -- or use sraw.pairs
		if type(v) == "userdata" then
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

print("\n6:")
