local orequire = _G.require
print(_G, orequire)
local yue = require('_inc')
yue.uninstall "lustache"
print(_G, orequire, yue.original.require)
assert(orequire == yue.original.require)
local lustache = require('lustache', '1.2-1')

view_model = {
  title = "Joe",
  calc = function ()
    return 2 + 4;
  end
}

output = lustache:render("{{title}} spends {{calc}}", view_model)
print('output == ', output)
assert(output == 'Joe spends 6')

