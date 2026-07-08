# leetcode190.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/8
=#
using Printf
function change(input,n,c)
    input = c == 1 ? input | 1<<(n-1) : input & ~(1<<(n-1))
end
function swap(input,a,b)
    ca = input & 1<<(a-1) > 0
    cb = input & 1<<(b-1) > 0
    input = change(input,a,cb)
    input = change(input,b,ca)
    input
end
function main(input)
    for i in 1:16
        input = swap(input,i,33-i)
    end
    @printf("a:%d",input)
end
main(2147483644)
