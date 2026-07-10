# leetcode8.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/9
=#
function decode(input,len)
    output = 0
    for i in input[2:len-1]
        a = i - '0'
        output = output * 10 + a
    end
    return input[1] == '+' ? output : -output
end
function main(str)
    at = 2
    input = collect(str)
    h = 1
    buf = fill('+',64)
    while h <= length(str) && str[h] == ' '
        h += 1
    end
    if h > length(str)
        return 0
    end
    if input[h] == '+'
        h += 1
    elseif input[h] == '-'
        buf[1] = '-'
        h += 1
    end

    for i in input[h:length(input)]
        if i - '0' >= 0 && i - '0' <= 9
            buf[at] = i
            at += 1
            h += 1
        else
            break
        end
    end
    return decode(buf,at)
end

println(main("     987"))
#println(decode([UInt('4')-48,UInt('2')-48]))
