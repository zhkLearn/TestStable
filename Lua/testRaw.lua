print("\n1:")

--local sraw = require "StableRaw"
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
fromC = nil

print("\n6:")

local t2 = sraw.new()
t2.values = {"Monday", "Tuesday", "Wednesday"}
sraw.dump(t2, true)

