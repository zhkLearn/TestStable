print("\n1:")

local t = SharedTable.new()


print("\n2:")

local subT = {1, 2, 3}
t.hello = {world = true, "one", "two", 3, False = false, testT = {true, 3, testttt = {"Alpha", nil, 6}}} 


print("\n3:")

t.subT = subT
t.hello[0] = nil
SharedTable.dump(t, false)

