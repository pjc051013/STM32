# leetcode190.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/8
=#
using Printf
#function change(input,n,c)
#    input = c == 1 ? input | 1<<n : input & ~(1<<n)
#end
#function swap(input,a)
#    b = 31 - a
#    ca = input & 1<<a > 0
#    cb = input & 1<<b > 0
#    input = change(input,a,cb)
#    input = change(input,b,ca)
#    input
#end
#function main(input)
#    for i in 0:15
#        input = swap(input,i)
#    end
#    @printf("a:%d",input)
#end
#main(2147483644)
function reversebits(input)
    res = zero(UInt32)
    for i in 0:31
        res <<= 1
        res |= (input>>i) & 1
    end
    @printf("a:%d",res)
end
reversebits(0x7FFFFFFC)