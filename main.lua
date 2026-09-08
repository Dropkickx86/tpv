local imageinfo = require("imageinfo")
local drw = require("drw")

local filename, no_scaling, no_vert_scaling = ...
local img = imageinfo.load(filename)

if no_scaling == 0 then
    
end

drw.scrclear()
drw.picture_print(img)