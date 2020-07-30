local F = require 'feather.shared'
local Enumset = require 'feather.enumset'
local Element = require 'feather.element'
local V = require 'feather.virtual'

local M = {}
M.Idx = uint16

struct M.Msg {
  kind : M.Idx
  subkind : M.Idx
  union {}
}
M.Msg.typelist = {}

struct M.Result {
  union {}
}
M.Result.typelist = {}

local msgenum = {}

-- Go through all our message functions and generate the parameter and result unions
for k, v in pairs(Element.virtualinfo.info) do
    local f = Element.methods[k]
    local s = struct{}
    s.name = "Msg"..k
    k = F.camelCase(k)
    s.entries = f.definition.parameters:map(function(e) return {field = e.symbol.displayname, type = e.symbol.type} end)
    table.remove(s.entries, 1) -- Remove self
    M.Msg.entries[3]:insert({field = k, type = s})
    M.Msg.typelist[v.index] = {field = k, type = s}
    M.Result.entries[1]:insert({field = k, type = f.type.returntype})
    M.Result.typelist[v.index] = f.type.returntype
    table.insert(msgenum, string.upper(k))
    table.insert(msgenum, v.index)
end

M.Kind = Enumset(msgenum, M.Idx)
M.Msg.entries[1].type = M.Kind -- Set this to the correct enumeration type

if terralib.sizeof(M.Result) ~= terralib.sizeof(intptr) then
  error("Msg.Result ("..terralib.sizeof(M.Result)..") should be the size of a pointer ("..terralib.sizeof(intptr)..")")
end

M.Behavior = {&Element, &opaque, &opaque, &M.Msg} -> M.Result

-- Our default message handler just calls the appropriate vftable pointer and maps our parameters back to it.
terra M.DefaultBehavior(self : &Element, w : &opaque, ui : &opaque, m : &M.Msg) : M.Result
  switch [int](m.kind.val) do
    escape 
      for k, v in pairs(M.Kind.methods) do
        local idx = M.Kind.enum_values[k]
        local args = M.Msg.typelist[idx].type.entries:map(function(x) return `m.[M.Msg.typelist[idx].field].[x.field] end)
        emit(quote case [idx] then self:virtual([ Element.virtualinfo.names[idx + 1] ])(self, [args]) end end)
      end
    end
  end
  var r : M.Result;
  r.[M.Msg.typelist[0].field] = -1;
  return r
end

return M