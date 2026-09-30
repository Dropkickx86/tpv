local imageinfo = require("imageinfo")
local drw = require("drw")

local filename, fast, no_scaling, no_vert_scaling, inline, progress = ...
local termw, termh = drw.getterminfo()
local img = imageinfo.load(filename)

if fast == 1 then
    goto render
end

if no_scaling ~= 1 then
    local scalefactor
    if(no_vert_scaling ~= 1 and (img.h / (termh * 2)) > img.w / termw) then
        scalefactor = math.ceil(img.h / (termh * 2))
    else
        scalefactor = math.ceil(img.w / termw)
    end
    if scalefactor > 1 then
        local actions = 0
        local rowindex = 0
        local colindex
        for row = 1, img.h, scalefactor do
            rowindex = rowindex + 1
            colindex = 0
            for col = 1, img.w, scalefactor do
                colindex = colindex + 1
                for ch = 1, img.c, 1 do
                    local val = 0
                    local amount = 0
                    local rowoffset = 0
                    while row + rowoffset <= img.h and rowoffset < scalefactor do
                        local coloffset = 0
                        while col + coloffset <= img.w and coloffset < scalefactor do
                            val = val + imageinfo.get(img, row + rowoffset, col + coloffset, ch)
                            amount = amount + 1
                            coloffset = coloffset + 1
                            if progress == 1 then
                                if actions % 100000 == 0 then
                                    io.write("\r\027[K")
                                    io.flush()
                                    io.write('[')
                                    local perc = math.floor((actions / (img.w * img.h * img.c)) * 100)
                                    local filled = 0
                                    while perc > 5 do
                                        io.write('#')
                                        perc = perc - 5
                                        filled = filled + 1
                                    end
                                    for _ = filled,20,1 do
                                        io.write('-')
                                    end
                                    io.write(']')
                                    io.flush()
                                end
                                actions = actions + 1
                            end
                        end
                        rowoffset = rowoffset + 1
                    end
                    imageinfo.set(img, rowindex, colindex, ch, math.floor((val / amount) + 0.5), math.ceil(img.w / scalefactor))
                end
            end
        end
        img.w = colindex
        img.h = rowindex
    end
end

::render::
if inline ~= 1 then
    drw.scrclear()
end
drw.picture_print(img, fast)