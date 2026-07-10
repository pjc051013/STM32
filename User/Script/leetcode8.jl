# leetcode8.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/9
=#
function decode(input)
    output = 0
    for i in input
        output = output * 10 + i
    end
    if output > typemax(Int32)
        return typemax(Int32)
    elseif output < typemin(Int32)
        return typemin(Int32)
    else
        return output
    end
end
function main(str)
    input = collect(str)
    h = 1
    neg = 1
    output = Int32[]
    while h <= length(str) && str[h] == ' '
        h += 1
    end
    if h > length(str)
        return 0
    end
    if input[h] == '+'
        h += 1
    elseif input[h] == '-'
        neg = -1
        h += 1
    end

    for i in input[h:length(input)]
        if i - '0' >= 0 && i - '0' <= 9

            push!(output,i - '0')
            h += 1
        else
            return neg * decode(output)
        end
    end
    return neg * decode(output)
end

println(main("     987"))
#println(decode([UInt('4')-48,UInt('2')-48]))
