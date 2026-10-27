#include "LoadingBackdrop.h"

#include <cmath>

using namespace eacp;
using namespace eacp::GPU;

namespace
{
struct QuadCorner
{
    Maths::Vec2 position;
};

constexpr auto fullScreenQuad = EA::Array<QuadCorner, 6> {
    {{-1.f, -1.f}},
    {{1.f, -1.f}},
    {{-1.f, 1.f}},
    {{1.f, -1.f}},
    {{1.f, 1.f}},
    {{-1.f, 1.f}},
};

constexpr auto hashDotX = 127.1f;
constexpr auto hashDotY = 311.7f;
constexpr auto hashScale = 43758.5453f;

constexpr auto octaveCount = 5;
constexpr auto firstOctaveAmplitude = 0.5f;
constexpr auto octaveGain = 0.5f;
constexpr auto octaveLacunarity = 2.02f;
constexpr auto octaveRotationCosine = 0.8f;
constexpr auto octaveRotationSine = 0.6f;
constexpr auto octaveShift = 17.3f;

constexpr auto fieldScale = 1.6f;
constexpr auto driftSpeed = 0.035f;
constexpr auto firstWarpStrength = 1.8f;
constexpr auto secondWarpStrength = 2.4f;
constexpr auto firstWarpOffsetX = 5.2f;
constexpr auto firstWarpOffsetY = 1.3f;
constexpr auto secondWarpOffsetX = 1.7f;
constexpr auto secondWarpOffsetY = 9.2f;
constexpr auto secondWarpDriftX = 0.9f;
constexpr auto secondWarpDriftY = -0.6f;

constexpr auto abyssRed = 0.020f;
constexpr auto abyssGreen = 0.024f;
constexpr auto abyssBlue = 0.040f;
constexpr auto navyRed = 0.060f;
constexpr auto navyGreen = 0.090f;
constexpr auto navyBlue = 0.190f;
constexpr auto charcoalRed = 0.140f;
constexpr auto charcoalGreen = 0.145f;
constexpr auto charcoalBlue = 0.175f;
constexpr auto navyAmountScale = 1.4f;
constexpr auto charcoalAmountLow = 0.45f;
constexpr auto charcoalAmountHigh = 0.95f;
constexpr auto charcoalMaximumAmount = 0.6f;

constexpr auto sheenBandFrequency = 9.f;
constexpr auto sheenSpeed = 0.25f;
constexpr auto sheenSharpness = 12.f;
constexpr auto sheenStrength = 0.14f;
constexpr auto sheenRed = 0.55f;
constexpr auto sheenGreen = 0.65f;
constexpr auto sheenBlue = 0.85f;

constexpr auto vignetteInnerRadius = 0.4f;
constexpr auto vignetteOuterRadius = 1.6f;
constexpr auto vignetteDarkening = 0.45f;

constexpr auto timeWrapSeconds = 3600.0;

} // namespace

Float3 LoadingBackdropShader::makeColour(float red, float green, float blue)
{
    return float3(constant(red), constant(green), constant(blue));
}

// A pseudo-random value in [0, 1) per integer lattice point.
Float LoadingBackdropShader::hashLatticePoint(const Float2& point)
{
    auto projected = dot(point, float2(constant(hashDotX), constant(hashDotY)));
    return fract(sin(projected) * hashScale);
}

// Smoothly interpolated value noise in [0, 1).
Float LoadingBackdropShader::sampleValueNoise(const Float2& point)
{
    auto cell = floor(point);
    auto within = fract(point);
    auto eased = within * within * (3.f - within * 2.f);

    auto bottomLeft = hashLatticePoint(cell);
    auto bottomRight = hashLatticePoint(cell + float2(constant(1.f), constant(0.f)));
    auto topLeft = hashLatticePoint(cell + float2(constant(0.f), constant(1.f)));
    auto topRight = hashLatticePoint(cell + float2(constant(1.f), constant(1.f)));

    auto bottom = mix(bottomLeft, bottomRight, eased.x());
    auto top = mix(topLeft, topRight, eased.x());
    return mix(bottom, top, eased.y());
}

// Rotates and scales each octave so lattice artefacts don't line up.
Float2 LoadingBackdropShader::advanceToNextOctave(const Float2& point)
{
    auto rotated =
        float2(point.x() * octaveRotationCosine - point.y() * octaveRotationSine,
               point.x() * octaveRotationSine + point.y() * octaveRotationCosine);
    return rotated * octaveLacunarity + octaveShift;
}

// Fractal Brownian motion: summed octaves of value noise, roughly [0, 1).
Float LoadingBackdropShader::sampleFractalNoise(const Float2& start)
{
    auto point = start;
    auto amplitude = firstOctaveAmplitude;
    auto total = sampleValueNoise(point) * amplitude;

    for (auto octave = 1; octave < octaveCount; ++octave)
    {
        point = advanceToNextOctave(point);
        amplitude *= octaveGain;
        total = total + sampleValueNoise(point) * amplitude;
    }

    return total;
}

// Inigo Quilez's two-stage domain warp: noise displacing the input of noise.
Float2 LoadingBackdropShader::calculateWarpedPoint(const Float2& point,
                                                   const Float& drift)
{
    auto firstWarp = float2(
        sampleFractalNoise(point + float2(drift, constant(0.f))),
        sampleFractalNoise(
            point + float2(constant(firstWarpOffsetX), constant(firstWarpOffsetY))));

    auto secondInput = point + firstWarp * firstWarpStrength;
    auto secondWarp = float2(
        sampleFractalNoise(secondInput
                           + float2(drift * secondWarpDriftX + secondWarpOffsetX,
                                    drift * secondWarpDriftY + secondWarpOffsetY)),
        sampleFractalNoise(secondInput + float2(constant(0.f), drift)));

    return point + secondWarp * secondWarpStrength;
}

// A thin bright band wherever the height crosses a slowly travelling contour.
Float LoadingBackdropShader::calculateSheen(const Float& height, const Float& drift)
{
    auto band =
        abs(sin(height * sheenBandFrequency + drift * (sheenSpeed / driftSpeed)));
    return pow(1.f - band, sheenSharpness) * sheenStrength;
}

Float LoadingBackdropShader::calculateVignette(const Float2& screenPoint)
{
    auto distanceFromCentre = length(screenPoint);
    return 1.f
           - smoothstep(vignetteInnerRadius, vignetteOuterRadius, distanceFromCentre)
                 * vignetteDarkening;
}
LoadingBackdropShader::LoadingBackdropShader()
{
    compile();
}

void LoadingBackdropShader::define()
{
    auto corner = vertexInput(&QuadCorner::position);
    setPosition(float4(corner, 0.f, 1.f));

    auto screenPoint = varying(corner);
    auto fieldPoint = screenPoint * float2(aspect, constant(1.f)) * fieldScale;
    auto drift = Float(time) * driftSpeed;

    auto height = sampleFractalNoise(calculateWarpedPoint(fieldPoint, drift));

    auto navyAmount = clamp(height * navyAmountScale, 0.f, 1.f);
    auto charcoalAmount = smoothstep(charcoalAmountLow, charcoalAmountHigh, height)
                          * charcoalMaximumAmount;

    auto abyss = makeColour(abyssRed, abyssGreen, abyssBlue);
    auto navy = makeColour(navyRed, navyGreen, navyBlue);
    auto charcoal = makeColour(charcoalRed, charcoalGreen, charcoalBlue);
    auto sheenColour = makeColour(sheenRed, sheenGreen, sheenBlue);

    auto base = mix(mix(abyss, navy, navyAmount), charcoal, charcoalAmount);
    auto lit = base + sheenColour * calculateSheen(height, drift);

    setFragment(float4(lit * calculateVignette(screenPoint), 1.f));
}

LoadingBackdrop::LoadingBackdrop()
{
    shader.setVertices(fullScreenQuad);
    shader.prepare(sampleCount());
    setVisible(false);
}

void LoadingBackdrop::showOver(Graphics::View& parent)
{
    removeFromParent();
    parent.addSubview(*this);
    setBounds(parent.getLocalBounds());
    setVisible(true);
    setContinuous(true);
}

void LoadingBackdrop::hide()
{
    setContinuous(false);
    setVisible(false);
    removeFromParent();
}

void LoadingBackdrop::update(Threads::FrameTime frameTime)
{
    elapsedSeconds = std::fmod(elapsedSeconds + frameTime.delta, timeWrapSeconds);
}

void LoadingBackdrop::render(Frame& frame)
{
    auto logicalSize = frame.logicalSize();
    shader.time = static_cast<float>(elapsedSeconds);
    shader.aspect = logicalSize.x / logicalSize.y;

    auto pass = frame.beginPass({Graphics::Color {abyssRed, abyssGreen, abyssBlue}});
    pass.draw(shader);
}
