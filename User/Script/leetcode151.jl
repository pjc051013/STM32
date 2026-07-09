# leetcode151.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/8
=#

function main(input)
    a = copy(input)
    at = 1
    len = length(input)
    h = t = len
    for i in len:-1:1
        if input[i] == ' '
            if h < t
                l = t - h
                a[at:at + l - 1] = input[h+1:t]
                at += l
                a[at] = ' '
                at += 1
            end
            t = h - 1
        end
        h -= 1
    end
    println(String(input))
    println(String(a[1:at - 2]))
end

main(collect("a good   example"))