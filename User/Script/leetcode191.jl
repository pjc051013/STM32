# leetcode191.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/8
=#
using Printf
a = (1,2,4,8,16,32,64,128,256)
function main(input)
    k = 0
    for i in a
        k += i & input == i
    end
    @printf("a:%d",k)
end

main(5)
