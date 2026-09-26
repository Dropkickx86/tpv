local imageinfo = require("imageinfo")
local drw = require("drw")

local filename, fast, no_scaling, no_vert_scaling, progress = ...
local termw, termh = drw.getterminfo()
local img = imageinfo.load(filename)

if fast == 1 then
    drw.scrclear()
    drw.fastprint(img)
    return
end

if no_scaling ~= 1 then
    io.write('[')
    for i=1,20,1 do
        io.write('-')
    end
    io.write(']')
    io.flush()
    local total_index = 0
    local actions = 0
    local final_h = img.h
    local final_w = img.w
    while final_w > termw or (no_vert_scaling ~= 1 and final_h > (termh * 2)) do
        final_w = math.ceil(final_w / 2)
        final_h = math.ceil(final_h / 2)
    end
    local cur_h = img.h
    local cur_w = img.w
    while cur_w > final_w do
        cur_h = math.ceil(cur_h / 2)
        cur_w = math.ceil(cur_w / 2)
        total_index = total_index + (cur_h * cur_w * img.c)
    end
    
    while img.w > termw or (no_vert_scaling ~= 1 and img.h > (termh * 2)) do
        local row = 0
        local index = 1
        while row < img.h do
            local col = 0
            while col < img.w do
                local channel = 1
                while channel <= img.c do
                    if row == (img.h - 1) and col == (img.w - 1) then
                        img[index] = img[(col * img.c) + (row * img.w * img.c) + channel]
                    elseif row == (img.h -1) then
                        img[index] = math.floor(((
                            img[(col * img.c) + (row * img.w * img.c) + channel] +
                            img[(col * img.c) + (row * img.w * img.c) + channel + img.c]
                        ) / 2) + 0.5)
                    elseif col == (img.w - 1) then
                        img[index] = math.floor(((
                            img[(col * img.c) + (row * img.w * img.c) + channel] +
                            img[(col * img.c) + ((row + 1) * img.w * img.c) + channel]
                        ) / 2) + 0.5)
                    else
                        img[index] = math.floor(((
                        img[(col * img.c) + (row * img.w * img.c) + channel] +
                        img[(col * img.c) + (row * img.w * img.c) + channel + img.c] +
                        img[(col * img.c) + ((row + 1) * img.w * img.c) + channel] +
                        img[(col * img.c) + ((row + 1) * img.w * img.c) + channel + img.c]
                        ) / 4) + 0.5)
                    end
                    if progress == 1 and actions % 100000 == 0 then
                        io.write("\r\027[K")
                        io.flush()
                        io.write('[')
                        local perc = math.floor((actions / total_index) * 100)
                        local filled = 0
                        while perc > 5 do
                            io.write('#')
                            perc = perc - 5
                            filled = filled + 1
                        end
                        for i=filled,20,1 do
                            io.write('-')
                        end
                        io.write(']')
                        io.flush()
                    end
                    actions = actions + 1
                    channel = channel + 1
                    index = index + 1
                end
                col = col + 2
            end
            row = row + 2
        end
        img.w = math.ceil(img.w / 2)
        img.h = math.ceil(img.h / 2)
    end
end

drw.scrclear()
drw.picture_print(img)

