-- What every check shares: it counts its expectations, then logs its verdict and quits. The runner reads the verdict
-- ("CHECK PASSED <name>") and the exit code; a failed expectation is logged as an error.
local Check = {}
Check.__index = Check

function Check.New(sName)
	return setmetatable({ sName = sName, nCount = 0, tFailures = {} }, Check)
end

function Check:Expect(bCondition, sWhat)
	self.nCount = self.nCount + 1
	if not bCondition then
		table.insert(self.tFailures, sWhat)
	end
	return bCondition
end

function Check:Equal(vActual, vExpected, sWhat)
	return self:Expect(vActual == vExpected, string.format("%s: %s, expected %s", sWhat, tostring(vActual), tostring(vExpected)))
end

function Check:Near(nActual, nExpected, sWhat, nTolerance)
	local bNear = type(nActual) == "number" and math.abs(nActual - nExpected) <= (nTolerance or 0.001)
	return self:Expect(bNear, string.format("%s: %s, expected %s", sWhat, tostring(nActual), tostring(nExpected)))
end

function Check:Finish()
	if #self.tFailures == 0 then
		Debug.Log(string.format("CHECK PASSED %s (%d expectations)", self.sName, self.nCount))
	else
		for _, sFailure in ipairs(self.tFailures) do
			Debug.LogError(string.format("CHECK FAILED %s: %s", self.sName, sFailure))
		end
	end
	Application.Quit()
end

return Check
