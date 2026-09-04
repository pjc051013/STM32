function Roll(theta)
    c = cos(theta)
    s = sin(theta)
    return [1 0 0; 0 c -s; 0 s c]
end
function Pitch(theta)
    c = cos(theta)
    s = sin(theta)
    return [c 0 s; 0 1 0; -s 0 c]
end
function Yaw(theta)
    c = cos(theta)
    s = sin(theta)
    return [c -s 0; s c 0; 0 0 1]
end