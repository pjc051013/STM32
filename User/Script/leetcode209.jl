# leetcode209.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/9
=#

#function main(target,nums)
#    sum = 0
#    sumb = 0
#    lista = []
#    listb = []
#    listc = []
#    tar = target >> 1
#    a = 0
#    b = 0
#    for i in nums
#        if i > target
#            return 1
#        end
#        sum += i
#        if i < tar
#            push!(lista,i)
#            elseif i > tar
#            push!(listb,i)
#            sumb += i
#            b += 1
#            else
#            push!(listc,i)
#        end
#        for j in listc
#            push!(listb,j)
#            b += 1
#            sumb += j
#        end
#    end
#    println("one iteration")
#    if sum < target
#        return 0
#    end
#    if sumb == target
#        return b
#    end
#    if sumb > target
#        return main(target,listb)
#        else
#        return b + main(target - sumb,lista)
#    end
##    while true
##        tar = listc[1]
##        for i in listc
##            if i < tar
##                push!(lista,i)
##            end
##            if i >= tar
##                b += 1
##                push!(listb,i)
##                sumb += i
##            end
##        end
##        if sumb == target
##            return b
##        end
##        if sumb > target
##            listc = listb
##            else
##            listc = lista
##        end
##        a += b
##        b = 0
##        lista = []
##        listb = []
##    end
#
#
#    println("Hello, Julia!")
#end

function main(target,nums)
    sum = 0
    len = 0
    for i in nums
        if i >= target
            return 1
        end
        sum += i
    end
    if sum < target
        return 0
    end
    if sum == target
        return length(nums)
    end
    for a in 2:length(nums)
        suma = 0
        for j in 1:length(nums) - a + 1
            for k in 1:a
                suma += nums[j-1+k]
            end
            if suma >= target
                return a
            end
            suma = 0
        end
    end

end

println(main(7,[2,3,1,2,4,3]))
