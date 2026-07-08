# leetcode136.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/8
=#
using Printf
function main(input)
    a = 0
    for i in input
        a = i ⊻ a
    end
    a
    @printf("a:%d",a)
end

main([4,1,2,1,2])
