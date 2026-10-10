// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.cpp"
#include <cstdlib>
#include <cmath>

void require(bool ok) { if (!ok) std::abort(); }
void test_native_draw(float* a, std::size_t n) {
    if (a) for (std::size_t i = 0; i < n; ++i) a[i] = .75f;
}
double test_native_shake() { return .8; }
double test_native_parameter(unsigned index) {
    return index == ED_FM_SUSPENSION_0_WHEEL_YAW ? 16.0 * 3.14159265358979323846 / 180.0 : .5;
}
int main() {
    using Draw = void(*)(float*, std::size_t);
    using Shake = double(*)();
    auto draw = reinterpret_cast<Draw>(f23b_select_callback("ed_fm_set_draw_args_v2",
        reinterpret_cast<FARPROC>(&test_native_draw)));
    auto shake = reinterpret_cast<Shake>(f23b_select_callback("ed_fm_get_shake_amplitude",
        reinterpret_cast<FARPROC>(&test_native_shake)));
    runtime.output.controls = {};
    runtime.output.controls.left_aileron = -.2;
    std::array<float, 1100> args{};
    draw(args.data(), args.size());
    require(runtime.input.landing_flaps); // native input captured before override
    require(args[9] == 0 && args[10] == 0 && args[11] == -.2f);
    for (std::size_t i = 0; i < args.size(); ++i)
        if (i < 9 || i > 18) require(args[i] == .75f);
    runtime.output.controls.flap = .3;
    draw(args.data(), args.size());
    require(args[9] == .3f && args[10] == .3f);
    std::array<float, 9> small{};
    draw(small.data(), small.size());
    for (float a : small) require(a == .75f);
    draw(nullptr, 0);
    runtime.body.weight_on_wheels = false;
    runtime.output.shake_amplitude = 0;
    require(shake() == 0); // unused native FCS's .8 must not shake smooth flight
    runtime.output.shake_amplitude = .35;
    require(shake() == .35); // real independent-model buffet is preserved
    runtime.body.weight_on_wheels = true;
    require(shake() == .8); // native runway/ground effects retained
    // DCS receives the wheelbase-scaled nosewheel angle, and argument 2 shows the same angle.
    using Parameter = double(*)(unsigned);
    auto parameter = reinterpret_cast<Parameter>(f23b_select_callback("ed_fm_get_param",
        reinterpret_cast<FARPROC>(&test_native_parameter)));
    const double native = test_native_parameter(ED_FM_SUSPENSION_0_WHEEL_YAW);
    const double scaled = f23b::bridge::nose_wheel_yaw(native);
    require(parameter(ED_FM_SUSPENSION_0_WHEEL_YAW) == scaled && scaled > native);
    require(parameter(ED_FM_SUSPENSION_1_WHEEL_YAW) == .5);
    draw(args.data(), args.size());
    require(std::abs(args[2] - .75 * scaled / native) < 1e-6);
    for (std::size_t i = 0; i < args.size(); ++i)
        if (i != 2 && (i < 9 || i > 18)) require(args[i] == .75f);
}
