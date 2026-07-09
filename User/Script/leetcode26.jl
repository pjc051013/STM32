# leetcode26.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/8
=#

function main(input)
    k = 1
    for i in 2:length(input)
        k += (input[i] != input[k])
        input[k] = input[i]

    end
    println(input)
end

main([0,0,0,0,0,0,1,1,1,1,2,2,3,3,4])
