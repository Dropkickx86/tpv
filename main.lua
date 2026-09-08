local imageinfo = require("imageinfo")
local drw = require("drw")

local filename = ...
local img = imageinfo.load(filename)

drw.scrclear()
drw.picture_print(img)