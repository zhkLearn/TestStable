print("do testSharedTable.lua")
print("\n1:")

local t = SharedTable.new()


print("\n2:")

local subT = {1, 2, 3}
t.hello = {world = true, "one", "two", 3, False = false, testT = {true, 3, testttt = {"Alpha", nil, 6}}} 


print("\n3:")

t.subT = subT
t.hello[0] = nil

print("\ndump(t):")
SharedTable.dump(t, false)

print("\n4:")
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

t[0] = 10
t[1] = nil
t[2] = 20
print("\ndumpSTable(t):")
dumpSTable(t)

print("\n5:")
function dumpArraySTable(tbl)
	for k,v in ipairs(tbl) do  -- or use sraw.pairs
		if type(v) == "userdata" or type(v) == "table" then
			print(k)
			dumpArraySTable(v)
		else
			print(k, v)
		end
	end
end

print("\n6:")
print("\ndumpArraySTable(t):")
dumpArraySTable(t)

print("\ndump(t):")
SharedTable.dump(t, false)

SharedTable.share(t, "theSharedTable")

print("\nThe end.")


