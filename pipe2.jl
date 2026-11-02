# ==========================================
# pipe_fusion.jl
# Description: 矩形4传感器阵列 - 固定 Xp(物理归零), 反演 Yp 与电流 I
#
# 阵列几何 (2x2 矩形, 单位: 米)。传感器2 位于原点, 增益标定以它为基准:
#   传感器0 (0.0, 0.5)      传感器1 (0.1, 0.5)
#   传感器2 (0.0, 0.0) 原点  传感器3 (0.1, 0.0)
#
# 设计依据(详见讨论结论, 不要在不理解原因的情况下改动):
#   1. Xp 不参与解算，而是固定为 XP_FIXED：
#      两列传感器 (传感器0/传感器2 一列, 传感器1/传感器3 一列) 关于列中点对称。
#      物理调整设备位置直到两列读数相等，即可保证 Xp 精确对准列中点，且该对准
#      对残余误差极宽容(厘米级偏移在约30m标距下可忽略，参见蒙特卡洛验证：偏移
#      5cm时Yp偏差仍<0.01m)。固定 Xp 能把原本与 Yp 耦合(自由解算时 corr≈-0.35)
#      的那部分自由度让给 Yp，实测 Yp 精度提升约 15%~26%。
#   2. 阵列旋转失准角 angle 不建模、不反演：
#      矩形布局下 4 个传感器对 angle 的响应都是同一个 cos(angle) 标量因子，
#      在数学上与电流 I 完全简并——任何角度残差只会让反演出的 I 产生
#      1/cos(angle) 倍衰减，对 Yp 无偏差影响(此结论对任意角度大小精确成立，
#      不只是小角度近似)。因此无需反演 angle，只需机械对准控制在几度以内，
#      避免 cos(angle) 衰减带来的信噪比损失(标准差按 1/cos(angle) 增长)。
#   3. I 必须参与解算，即使并不关心其数值：
#      在该远场(约30m标距，传感器间距仅0.1~0.5m)几何下，Yp 与 I 高度简并
#      (corr≈-1.0)。若把 I 固定为假设值，哪怕 1% 的误差也会造成约 1%
#      距离量级(约0.3m@30m)的 Yp 偏差，且近似线性放大。因此 I 只能通过
#      联合估计消除简并，不能当作已知常量。
# ==========================================

using LinearAlgebra
using ForwardDiff
using Random

# 1. 传感器在阵列坐标系中的【绝对几何位置】(单位: 米)，2x2 矩形排布。
const SENSOR_POS = [
    [0.0, 0.5],   # 传感器 0
    [0.1, 0.5],   # 传感器 1
    [0.0, 0.0],   # 传感器 2 (原点，增益基准)
    [0.1, 0.0],   # 传感器 3
]
const N_SENSORS = length(SENSOR_POS)
const GAIN_REF_INDEX = 3  # 传感器2 (原点) 作为增益标定基准

# Xp 固定值：两列 x 坐标 (0.0 与 0.1) 的中点，物理对准后管线应位于此处。
const XP_FIXED = 0.05

# 阵列旋转失准角：只用于仿真生成数据，验证"不反演也不影响Yp"这一结论。
# 求解器完全不使用这个量。
const ALIGNMENT_ANGLE = deg2rad(2.0)

# 传感器增益误差：影响的是反演位置的绝对值(系统性偏移)，可通过已知位置标定
# 消除到很高精度(见 calibrate_relative_gain)，不是 Yp 随机波动(std)的来源。
const gain_error = [0.999, 1.0001, 1.0, 1.0002]  # 传感器增益误差，理想值为1.0

# 传感器噪声标准差（单位：nT）：由 30m 处观测到的 Yp 标准差 ±1.5m 反推得到。
# 反推方法：在 true_Yp=-30, true_I=1 处对 forward_fields 求雅可比 J，
# Fisher信息矩阵 FIM=(1/σ²)J'J，std(Yp)=sqrt(inv(J'J)[1,1])·σ。
# 单位噪声(σ=1nT)对应 std(Yp)=276.80 m，故 σ = 1.5/276.80 ≈ 0.00542 nT。
const noise = [0.00542, 0.00542, 0.00542, 0.00542]

# 2. 核心正演模型：给定 [Yp, I]，计算4个传感器【理应读出】的理论磁场值。
# Xp 固定为 XP_FIXED，不作为状态量。
function forward_fields(state)
    Y_p, I = state
    μ0 = 4π * 1e-7
    scale = 1e9  # T 转 nT
    f = zeros(eltype(state), N_SENSORS)
    for i in 1:N_SENSORS
        X_s, Y_s = SENSOR_POS[i]
        # 计算管线到当前传感器的二维物理距离 r（Xp 固定，不参与求导）
        r = sqrt((XP_FIXED - X_s)^2 + (Y_p - Y_s)^2)
        # 毕奥-萨伐尔定律 (满足自动微分，避免 r=0 导致NaN，实际中r应>0)
        f[i] = ((μ0 * I) / (2π * (r + 1e-9))) * scale
    end
    return f
end

# 仅用于仿真：生成带有 Xp 残余偏移、角度失准的"真实"理论读数。
function forward_fields_truth(Xp_true, Yp_true, I_true, angle_true)
    μ0 = 4π * 1e-7
    scale = 1e9
    f = zeros(Float64, N_SENSORS)
    for i in 1:N_SENSORS
        X_s, Y_s = SENSOR_POS[i]
        r = sqrt((Xp_true - X_s)^2 + (Yp_true - Y_s)^2)
        f[i] = ((μ0 * I_true) / (2π * (r + 1e-9))) * scale * cos(angle_true)
    end
    return f
end

# 参考 pipe.jl 的两点比值反推思路：把每一行的两个传感器取平均，当成一个
# "虚拟探头"（因为 Xp 固定在两列中点，行内两个传感器到管线的距离严格相等，
# 平均只降噪不引入偏差）。两行的 y 坐标不同、到管线的水平偏移 dx 相同，
# 由两行读数之比可以闭式反推 Yp，再代回任一行方程反推 I，最后取两行平均。
# 该闭式解只用于生成阻尼高斯-牛顿的初值（冷启动），本身不追求最高精度。
function closed_form_initial_guess(meas)
    μ0 = 4π * 1e-7
    scale = 1e9
    k = μ0 * scale / (2π)

    y1 = SENSOR_POS[1][2]  # 行1 (传感器0/1) 的 y 坐标
    y2 = SENSOR_POS[3][2]  # 行2 (传感器2/3) 的 y 坐标
    dx = abs(XP_FIXED - SENSOR_POS[1][1])  # 两列相对 XP_FIXED 的水平偏移(对称，相等)

    m1 = (meas[1] + meas[2]) / 2  # 行1 平均读数
    m2 = (meas[3] + meas[4]) / 2  # 行2 平均读数

    ρ = m1 / m2
    A = ρ^2 - 1

    # 由 r1²=dx²+(Yp-y1)², r2²=dx²+(Yp-y2)², r2=ρ·r1 消去 r1,r2 得到的一元二次方程：
    # A·Yp² - 2·(A·y1 + (y1-y2))·Yp + [A·(dx²+y1²) + (y1²-y2²)] = 0
    a = A
    b = -2 * (A * y1 + (y1 - y2))
    c = A * (dx^2 + y1^2) + (y1^2 - y2^2)

    candidates = Float64[]
    if abs(a) < 1e-9
        # A≈0 (两行读数接近，说明 Yp 接近两行中点)，退化为线性方程
        if abs(b) > 1e-12
            push!(candidates, -c / b)
        else
            push!(candidates, (y1 + y2) / 2)
        end
    else
        disc = b^2 - 4a*c
        if disc >= 0
            sq = sqrt(disc)
            push!(candidates, (-b + sq) / (2a))
            push!(candidates, (-b - sq) / (2a))
        else
            push!(candidates, (y1 + y2) / 2)  # 判别式为负(噪声导致)时退回中点
        end
    end

    # 用实际残差挑选物理解（可能有两个根，取拟合更好的一个）。
    best_Yp = candidates[1]
    best_cost = Inf
    for Yp_candidate in candidates
        r1 = sqrt(dx^2 + (Yp_candidate - y1)^2)
        r2 = sqrt(dx^2 + (Yp_candidate - y2)^2)
        I1 = m1 * r1 / k
        I2 = m2 * r2 / k
        I_candidate = (I1 + I2) / 2
        cost = sum((meas .- forward_fields([Yp_candidate, I_candidate])).^2)
        if cost < best_cost
            best_cost = cost
            best_Yp = Yp_candidate
        end
    end

    r1 = sqrt(dx^2 + (best_Yp - y1)^2)
    r2 = sqrt(dx^2 + (best_Yp - y2)^2)
    best_I = ((m1 * r1 / k) + (m2 * r2 / k)) / 2

    return [best_Yp, best_I]
end

# 以传感器2(原点)为基准，根据已知标定状态估计各通道相对增益。
function calibrate_relative_gain(meas_ref, calibration_state)
    ideal_ref = forward_fields(calibration_state)
    reference_response = meas_ref[GAIN_REF_INDEX] / ideal_ref[GAIN_REF_INDEX]
    gain = ones(Float64, N_SENSORS)
    for i in 1:N_SENSORS
        i == GAIN_REF_INDEX && continue
        gain[i] = (meas_ref[i] / ideal_ref[i]) / reference_response
    end
    return gain
end

# 3. 高斯-牛顿迭代求解器（状态量仅 [Yp, I]）
# meas_M: 实际4个传感器采到的原始磁场数据 [m0, m1, m2, m3]
# x_init: 初始猜测值 [Y_guess, I_guess]
function gauss_newton_fusion(meas_M, x_init; max_iter=100, tol=1e-6)
    x = copy(x_init)
    parameter_scale = [30.0, 1.0]
    damping = 10.0
    measurement_weight = 1.0 ./ noise
    cost(y) = sum(((meas_M - forward_fields(y)) .* measurement_weight).^2)

    println("--- 开始高斯-牛顿迭代融合 ---")
    for iter in 1:max_iter
        # 计算当前猜测下的理论磁场值
        f_x = forward_fields(x)

        # 计算残差 (Residual): 实际值 - 理论值
        residual = meas_M - f_x

        # 【核心】利用 ForwardDiff 自动求出 4×2 的雅可比矩阵 J
        J = ForwardDiff.jacobian(forward_fields, x)

        # 在归一化参数空间中求阻尼最小二乘步，避免 J'J 放大病态性。
        J_scaled = J .* reshape(parameter_scale, 1, :)
        J_weighted = J_scaled .* reshape(measurement_weight, :, 1)
        augmented_residual = vcat(residual .* measurement_weight, zeros(2))
        current_cost = cost(x)
        damping_step = damping
        augmented_J = vcat(J_weighted, sqrt(damping_step) * Matrix{Float64}(I, 2, 2))
        Δz = augmented_J \ augmented_residual
        Δx = parameter_scale .* Δz
        candidate = x .+ Δx
        candidate_cost = cost(candidate)

        # 拒绝增大残差的候选步，并逐步提高阻尼。
        while candidate_cost >= current_cost && damping_step < 1e12
            damping_step *= 10.0
            augmented_J = vcat(J_weighted, sqrt(damping_step) * Matrix{Float64}(I, 2, 2))
            Δz = augmented_J \ augmented_residual
            Δx = parameter_scale .* Δz
            candidate = x .+ Δx
            candidate_cost = cost(candidate)
        end
        if candidate_cost >= current_cost
            @warn "阻尼迭代无法继续降低残差"
            break
        end
        x .= candidate
        damping = max(damping_step / 3.0, 1e-12)

        # 打印迭代过程，方便用习惯的 p 观察收敛
        err_norm = norm(Δx)
        println("Iter $iter: 状态变化量幅度 = $err_norm, 当前估计 = $(round.(x, digits=4))")

        if err_norm < tol
            println("算法收敛！")

            # 按各传感器的噪声标准差传播测量误差。
            # 注意：J_final 是在真实(未缩放)参数空间下求的雅可比，
            # 因此协方差已经是真实单位，不能再乘 parameter_scale（历史bug）。
            J_final = ForwardDiff.jacobian(forward_fields, x)
            J_final_weighted = J_final .* reshape(measurement_weight, :, 1)
            Σ_x = pinv(J_final_weighted' * J_final_weighted)
            total_std = sqrt.(diag(Σ_x))

            return x, total_std
        end
    end
    @warn "未能达到收敛精度，返回当前计算值"
    return x, zeros(2)
end

# ==================== 测试验证 ====================
function main()
    Random.seed!(20260909)

    # 设定真实的管线物理状态。Xp 真值略微偏离 XP_FIXED，模拟物理对准的残余误差；
    # angle 真值非零，模拟机械旋转失准；两者都只影响仿真数据生成，不进入求解器。
    true_Xp = XP_FIXED + 0.01   # 1cm 对准残余误差
    true_Yp = -30.0
    true_I = 1.0

    println("管线真实状态：Xp=$(true_Xp)m (对准目标=$(XP_FIXED)m), Yp=$(true_Yp)m, I=$(true_I)A, " *
            "旋转失准=$(rad2deg(ALIGNMENT_ANGLE))°")

    # 已知标定状态：假设物理对准完美(Xp=XP_FIXED)、角度为零，位置和电流可控。
    calibration_state = [true_Yp, true_I]
    calibration_samples = 100
    ideal_calibration = forward_fields(calibration_state)
    calibration_sum = zeros(N_SENSORS)
    for _ in 1:calibration_samples
        calibration_sum .+= gain_error .* ideal_calibration .+ randn(N_SENSORS) .* noise
    end
    calibration_meas = calibration_sum ./ calibration_samples
    gain_calibrated = calibrate_relative_gain(calibration_meas, calibration_state)

    # 运行时采集：真实读数包含 Xp 残余偏移、角度失准、增益误差和噪声。
    ideal_meas = forward_fields_truth(true_Xp, true_Yp, true_I, ALIGNMENT_ANGLE)
    sim_meas = gain_error .* ideal_meas .+ randn(N_SENSORS) .* noise
    println("传感器增益误差 gain_error = ", gain_error)
    println("传感器噪声标准差 noise = ", noise, " nT")
    println("标定测量均值 = ", round.(calibration_meas, digits=4), " nT")
    println("估计相对增益 = ", round.(gain_calibrated, digits=6))
    println("理想读数 (m0, m1, m2, m3) = ", round.(ideal_meas, digits=4), " nT")
    println("模拟传感器读数 (m0, m1, m2, m3) = ", round.(sim_meas, digits=2), " nT\n")
    sim_meas ./= gain_calibrated
    println("增益补偿读数 (m0, m1, m2, m3) = ", round.(sim_meas, digits=2), " nT\n")

    # 用闭式解算法(参考 pipe.jl 的两点比值反推)得到初值，作为高斯-牛顿的冷启动点。
    x_guess = closed_form_initial_guess(sim_meas)
    println("闭式解算冷启动初值: Yp0=", round(x_guess[1], digits=4),
            " m, I0=", round(x_guess[2], digits=4), " A\n")

    # 运行融合算法（只解 Yp 和 I，Xp 固定为 XP_FIXED，不解算 angle）
    est_state, est_std = gauss_newton_fusion(sim_meas, x_guess)

    println("\n=== 融合解算结果 ===")
    println("固定位置 Xp: ", XP_FIXED, " m (物理对准归零，不解算)")
    println("反演位置 Yp: ", round(est_state[1], digits=4), " m (± ", round(est_std[1], digits=6), " m)")
    println("反演电流  I: ", round(est_state[2], digits=4), " A (± ", round(est_std[2], digits=6), " A)  [仅为消除与Yp的简并而解算，数值本身不关心]")
end

main()
