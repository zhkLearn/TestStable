print("\n1:")

--local sraw = require "StableRaw"

sraw.init()  -- init lightuserdata metatable
print("\n2:")

local t = sraw.create()
print("\n3:")

--local t2 = sraw.create()
--sraw.set(t2, "world", true)
--sraw.set(t, "hello", t2)

local subT = {1, 2, 3}

t.hello = {world = true, "one", "two", 3, False = false, testT = {true, 3, testttt = {"Alpha", nil, 6}}} 
-- If you don't want to set explicit, use this :
--  sraw.set(t,"hello", { world = true })
print("\n4:")

t.subTable = subT
-- print(t.hello.world)
-- or use this :
-- hello = sraw.get(t,"hello") ; print( sraw.get(hello, "world"))

-- you can send t (a lightuserdata) to other lua state. (thread safe)


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


print("\n6:")

