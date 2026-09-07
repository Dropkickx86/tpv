local imageinfo = require("imageinfo")
local drw = require("drw")

local filename = ...
local img = imageinfo.load(filename)

output = {}
output[1] = {img.h, img.w}

for row=2, img.h + 1, 1 do
    output[row] = {}
     for col=1, img.w, 1 do
        output[row][col] = "0x"..string.format("%x", img[((row - 2) * (img.w * 4)) + ((col - 1) * 4) + 1])..string.format("%x", img[((row - 2) * (img.w * 4)) + ((col - 1) * 4) + 2])..string.format("%x", img[((row - 2) * (img.w * 4)) + ((col - 1) * 4) + 3])..string.format("%x", img[((row - 2) * (img.w * 4)) + ((col - 1) * 4) + 4])
     end
end

drw.scrclear()
drw.picture_print(img)








-- Format: 
-- rows, cols 
-- [pixeldata]
test = {
    { 8, 8 },
    { 0xFF0000FF, 0xFF0000EE, 0xFF0000DD, 0xFF0000CC, 0xFF0000BB, 0xFF0000AA, 0xFF000099, 0xFF000088 },
    { 0xFF0000EE, 0xFF0000DD, 0xFF0000CC, 0xFF0000BB, 0xFF0000AA, 0xFF000099, 0xFF000088, 0xFF000077 },
    { 0xFF0000DD, 0xFF0000CC, 0xFF0000BB, 0xFF0000AA, 0xFF000099, 0xFF000088, 0xFF000077, 0xFF000066 },
    { 0xFF0000CC, 0xFF0000BB, 0xFF0000AA, 0xFF000099, 0xFF000088, 0xFF000077, 0xFF000066, 0xFF000055 },
    { 0xFF0000BB, 0xFF0000AA, 0xFF000099, 0xFF000088, 0xFF000077, 0xFF000066, 0xFF000055, 0xFF000044 },
    { 0xFF0000AA, 0xFF000099, 0xFF000088, 0xFF000077, 0xFF000066, 0xFF000055, 0xFF000044, 0xFF000033 },
    { 0xFF000099, 0xFF000088, 0xFF000077, 0xFF000066, 0xFF000055, 0xFF000044, 0xFF000033, 0xFF000022 },
    { 0xFF000088, 0xFF000077, 0xFF000066, 0xFF000055, 0xFF000044, 0xFF000033, 0xFF000022, 0xFF000011 },
}

--picture_print("test")