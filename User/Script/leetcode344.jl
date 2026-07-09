# leetcode344.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/8
=#

function main(input::Vector{Char})
    r = length(input)
    l = 1
    while r > l
        input[r] , input[l] = input[l] , input[r]
        r -= 1
        l += 1
    end
    println(input)
end

main(['h','e','l','l','o'])
