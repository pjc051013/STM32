# leetcode65.jl
# Julia Script

#=
Description: 
Author: pjc_k
Date: 2026/7/10
=#

nums = ["2", "0089", "-0.1", "+3.14", "4.", "-.9", "2e10", "-90E3", "3e+7", "+6e-1", "53.5e93", "-123.456e789","abc", "1a", "1e", "e3", "99e2.5", "--6", "-+3", "95a54e53"]
@enum Stage EMPTY SIGN1 INT1 DOT DEC EXP SIGN2 INT2
errors = ["Illegal Character","Multiple Dots","Wrong Place of dot","Multiple Signs","Wrong Place of Sign","Multiple Exponent","No Number Before Exponent","Does Not Have an End"]
function main(s::String)
    sign = dot = exp = dgt = a = 0
    stage = EMPTY
    for i in s
        a += 1
        if ! ((isdigit(i)) || (i == 'e') || (i == 'E') || (i == '.') || (i == '+') || (i == '-'))
            return false,errors[1]
        end
        if isdigit(i)
            dgt += 1
            if stage == EMPTY
                stage = INT1
            elseif stage == SIGN1
                stage = INT1
            elseif stage == DOT
                stage = DEC
            elseif stage == EXP || stage == SIGN2
                stage = INT2
            end
        end
        if i == '.'
            if dot == 1
                return false,errors[2]
            end
            dot += 1
            if stage == EMPTY || stage == SIGN1 || stage == INT1
                stage = DOT
            else
                return false,errors[3]
            end
        end
        if i == '+' || i == '-'
            if sign == 1
                return false,errors[4]
            end
            sign +=1
            if a == 1
                stage = SIGN1
            elseif stage == EXP
                stage = SIGN2
            else
                return false,errors[5]
            end
        end
        if i == 'e' || i == 'E'
            sign = 0
            if exp == 1
                return false,errors[6]
            end
            exp +=1
            if stage == SIGN1 || dgt == 0 || stage == EMPTY
                return false,errors[7]
            end
            stage = EXP
        end
    end
    if dgt == 0 || stage == EXP || stage == SIGN2 || stage == SIGN1
        return false,errors[8]
    end
    return true
end
for i in nums
    println(i,":",main(i))
end