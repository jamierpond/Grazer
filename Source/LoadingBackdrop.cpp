#include "LoadingBackdrop.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

constexpr auto fluidViewRowCount = 90;
// Cells simulated beyond each edge of the view, so swirls carry on out of frame
// and the outflow border's own artefacts are never on screen.
constexpr auto fluidMarginCellCount = 32;
constexpr auto velocityDissipationPerSecond = 0.15f;
constexpr auto displacementRelaxationPerSecond = 0.3f;
constexpr auto maximumDisplacementCells = 28.f;
constexpr auto stirRadiusCells = 9.f;
constexpr auto stirGain = 1.f;
constexpr auto stirFollowRatePerSecond = 30.f;
constexpr auto maximumStirSpeedCellsPerSecond = 400.f;
constexpr auto maximumTimeStepSeconds = 1.f / 30.f;
constexpr auto pressureIterationCount = 20;
static_assert(pressureIterationCount % 2 == 0,
              "an even count leaves the solved pressure in the pressure buffer");

constexpr auto displacementFilterOffsetCells = 0.5f;
constexpr auto displacementFilterTapWeight = 0.25f;
constexpr auto minimumSegmentLengthSquared = 1.0e-4f;
constexpr auto centralDifferenceScale = 0.5f;
constexpr auto pressureNeighbourCount = 4.f;

UInt calculateCellIndex(const Float& column, const Float& row, const Float& columns)
{
    return toUInt(row * columns + column);
}

Float2 clampToGrid(const Float2& cell, const Float& columns, const Float& rows)
{
    return float2(clamp(cell.x(), 0.f, columns - 1.f),
                  clamp(cell.y(), 0.f, rows - 1.f));
}

Float4 sampleCellsBilinear(const InputBuffer& buffer,
                           const Float2& point,
                           const Float& columns,
                           const Float& rows)
{
    auto base = floor(point);
    auto within = point - base;

    auto readClamped = [&](float columnOffset, float rowOffset)
    {
        auto cell = clampToGrid(
            float2(base.x() + columnOffset, base.y() + rowOffset), columns, rows);
        return buffer.read4(calculateCellIndex(cell.x(), cell.y(), columns));
    };

    auto top = mix(readClamped(0.f, 0.f), readClamped(1.f, 0.f), within.x());
    auto bottom = mix(readClamped(0.f, 1.f), readClamped(1.f, 1.f), within.x());
    return mix(top, bottom, within.y());
}

// Outside the grid reads as zero pressure: the open boundary.
Float readPressureOrZeroOutside(const InputBuffer& pressure,
                                const Float& column,
                                const Float& row,
                                const Float& columns,
                                const Float& rows)
{
    auto insideFactor = toFloat(column >= 0.f) * toFloat(column < columns)
                        * toFloat(row >= 0.f) * toFloat(row < rows);
    auto cell = clampToGrid(float2(column, row), columns, rows);
    return pressure[calculateCellIndex(cell.x(), cell.y(), columns)] * insideFactor;
}

FluidGridSize calculateGridSize(float aspect)
{
    auto size = FluidGridSize {};
    size.viewRows = fluidViewRowCount;
    size.viewColumns =
        std::max(1, (int) std::lround((float) fluidViewRowCount * aspect));
    size.columns = size.viewColumns + 2 * fluidMarginCellCount;
    size.rows = size.viewRows + 2 * fluidMarginCellCount;
    return size;
}

bool isSameGridSize(const FluidGridSize& first, const FluidGridSize& second)
{
    return first.columns == second.columns && first.rows == second.rows;
}

GPU::Buffer makeZeroedStorageBuffer(int floatCount)
{
    auto zeros = std::vector<float>((std::size_t) floatCount, 0.f);
    auto bytes = (std::int64_t) (sizeof(float) * zeros.size());
    return GPU::Buffer(
        Device::shared(), zeros.data(), bytes, GPU::BufferUsage::Storage);
}

Graphics::Point limitLength(Graphics::Point vector, float maximumLength)
{
    auto vectorLength = vector.length();

    if (vectorLength <= maximumLength)
        return vector;

    auto scale = maximumLength / vectorLength;
    return {vector.x * scale, vector.y * scale};
}

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
Float4 LoadingBackdropShader::readFluidCell(const Float2& cell)
{
    auto clamped = clampToGrid(cell, gridColumns, gridRows);
    return fluidState.read4(
        calculateCellIndex(clamped.x(), clamped.y(), gridColumns));
}

// The grid's row 0 is the top of the view, so screen y flips on the way in.
Float2 LoadingBackdropShader::sampleDisplacementInCells(const Float2& screenPoint)
{
    auto column =
        Float(marginCells) + (screenPoint.x() * 0.5f + 0.5f) * viewColumns - 0.5f;
    auto row =
        Float(marginCells) + (0.5f - screenPoint.y() * 0.5f) * viewRows - 0.5f;
    auto readAtOffset = [&](float columnOffset, float rowOffset)
    {
        return sampleCellsBilinear(fluidState,
                                   float2(column + columnOffset, row + rowOffset),
                                   gridColumns,
                                   gridRows);
    };

    // Four bilinear taps half a cell apart: a smoother filter that also cancels
    // the collocated pressure solve's cell-to-cell checkerboard.
    auto sampled =
        (readAtOffset(-displacementFilterOffsetCells, -displacementFilterOffsetCells)
         + readAtOffset(displacementFilterOffsetCells,
                        -displacementFilterOffsetCells)
         + readAtOffset(-displacementFilterOffsetCells,
                        displacementFilterOffsetCells)
         + readAtOffset(displacementFilterOffsetCells,
                        displacementFilterOffsetCells))
        * displacementFilterTapWeight;
    return float2(sampled.z(), sampled.w());
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
    auto displacement = sampleDisplacementInCells(screenPoint);
    auto displacedPoint = screenPoint
                          - float2(displacement.x() * 2.f / viewColumns,
                                   -displacement.y() * 2.f / viewRows);
    auto fieldPoint = displacedPoint * float2(aspect, constant(1.f)) * fieldScale;
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

FluidAdvectKernel::FluidAdvectKernel()
{
    compile();
}

Float4 FluidAdvectKernel::readCell(const Float2& cell)
{
    auto clamped = clampToGrid(cell, gridColumns, gridRows);
    return source.read4(calculateCellIndex(clamped.x(), clamped.y(), gridColumns));
}

Float4 FluidAdvectKernel::sampleBilinear(const Float2& point)
{
    return sampleCellsBilinear(source, point, gridColumns, gridRows);
}

// A soft brush along the cursor's path this frame, so a fast flick leaves no gaps.
Float FluidAdvectKernel::calculateStirFalloff(const Float2& cell)
{
    auto segment = Float2(stirTo) - stirFrom;
    auto alongSegment = clamp(
        dot(cell - stirFrom, segment)
            / max(dot(segment, segment), constant(minimumSegmentLengthSquared)),
        0.f,
        1.f);
    auto closest = Float2(stirFrom) + segment * alongSegment;
    auto distanceToPath = length(cell - closest);
    return (1.f - smoothstep(0.f, stirRadiusCells, distanceToPath)) * stirStrength;
}

void FluidAdvectKernel::define()
{
    auto position = threadPosition();
    auto column = toFloat(position.x);
    auto row = toFloat(position.y);
    auto cell = float2(column, row);

    auto here = readCell(cell);
    auto velocityHere = float2(here.x(), here.y());
    auto sampled = sampleBilinear(cell - velocityHere * timeStep);

    auto velocityDecay = exp(Float(timeStep) * -velocityDissipationPerSecond);
    auto displacementDecay = exp(Float(timeStep) * -displacementRelaxationPerSecond);

    auto decayedVelocity = float2(sampled.x(), sampled.y()) * velocityDecay;
    auto stirAmount = calculateStirFalloff(cell);
    auto velocity =
        decayedVelocity + (Float2(stirVelocity) - decayedVelocity) * stirAmount;

    auto drifted =
        (float2(sampled.z(), sampled.w()) + velocity * timeStep) * displacementDecay;
    auto driftedLength = length(drifted);
    auto displacement = drifted * maximumDisplacementCells
                        / max(driftedLength, constant(maximumDisplacementCells));

    write(destination,
          calculateCellIndex(column, row, gridColumns),
          float4(velocity.x(), velocity.y(), displacement.x(), displacement.y()));
}

FluidDivergenceKernel::FluidDivergenceKernel()
{
    compile();
}

// Clamped to the border cell: zero-gradient velocity, so flow passes out.
Float2 FluidDivergenceKernel::readVelocity(const Float& column, const Float& row)
{
    auto clamped = clampToGrid(float2(column, row), gridColumns, gridRows);
    auto cell =
        state.read4(calculateCellIndex(clamped.x(), clamped.y(), gridColumns));
    return float2(cell.x(), cell.y());
}

void FluidDivergenceKernel::define()
{
    auto position = threadPosition();
    auto column = toFloat(position.x);
    auto row = toFloat(position.y);

    auto left = readVelocity(column - 1.f, row);
    auto right = readVelocity(column + 1.f, row);
    auto above = readVelocity(column, row - 1.f);
    auto below = readVelocity(column, row + 1.f);

    auto value =
        ((right.x() - left.x()) + (below.y() - above.y())) * centralDifferenceScale;
    write(divergence, calculateCellIndex(column, row, gridColumns), value);
}

FluidPressureKernel::FluidPressureKernel()
{
    compile();
}

void FluidPressureKernel::define()
{
    auto position = threadPosition();
    auto column = toFloat(position.x);
    auto row = toFloat(position.y);

    auto readNeighbour = [&](const Float& neighbourColumn, const Float& neighbourRow)
    {
        return readPressureOrZeroOutside(
            pressure, neighbourColumn, neighbourRow, gridColumns, gridRows);
    };

    auto neighbourSum =
        readNeighbour(column - 1.f, row) + readNeighbour(column + 1.f, row)
        + readNeighbour(column, row - 1.f) + readNeighbour(column, row + 1.f);

    auto index = calculateCellIndex(column, row, gridColumns);
    write(nextPressure,
          index,
          (neighbourSum - divergence[index]) / pressureNeighbourCount);
}

FluidProjectKernel::FluidProjectKernel()
{
    compile();
}

void FluidProjectKernel::define()
{
    auto position = threadPosition();
    auto column = toFloat(position.x);
    auto row = toFloat(position.y);

    auto readNeighbour = [&](const Float& neighbourColumn, const Float& neighbourRow)
    {
        return readPressureOrZeroOutside(
            pressure, neighbourColumn, neighbourRow, gridColumns, gridRows);
    };

    auto gradient =
        float2(readNeighbour(column + 1.f, row) - readNeighbour(column - 1.f, row),
               readNeighbour(column, row + 1.f) - readNeighbour(column, row - 1.f))
        * centralDifferenceScale;

    auto index = calculateCellIndex(column, row, gridColumns);
    auto cell = state.read4(index);
    write(projected,
          index,
          float4(
              cell.x() - gradient.x(), cell.y() - gradient.y(), cell.z(), cell.w()));
}

LoadingBackdrop::LoadingBackdrop()
{
    shader.setVertices(fullScreenQuad);
    shader.prepare(sampleCount());
    advectKernel.prepare();
    divergenceKernel.prepare();
    pressureKernel.prepare();
    projectKernel.prepare();
    // Never a click or focus target: AppKit makes a clicked view first
    // responder, which took the keys from the page under it. The cursor that
    // stirs the fluid is read in update() instead.
    setIgnoresMouseEvents(true);
    setVisible(false);
}

void LoadingBackdrop::showOver(Graphics::View& parent)
{
    removeFromParent();
    parent.addSubview(*this);
    setBounds(parent.getLocalBounds());
    isFluidResetPending = true;
    hasPreviousCursorCells = false;
    setVisible(true);
    setContinuous(true);
}

void LoadingBackdrop::hide()
{
    setContinuous(false);
    setVisible(false);
    removeFromParent();
}

void LoadingBackdrop::readCursorPosition()
{
    auto position = getMousePosition();
    auto isInside = getLocalBounds().contains(position);

    if (isInside && !isCursorInside)
        hasPreviousCursorCells = false;

    isCursorInside = isInside;
    cursorPoint = position;
}

Graphics::Point LoadingBackdrop::convertPointToCells(Graphics::Point point) const
{
    auto cellsPerPoint = (float) fluidViewRowCount / viewHeightInPoints;
    return {point.x * cellsPerPoint + (float) fluidMarginCellCount,
            point.y * cellsPerPoint + (float) fluidMarginCellCount};
}

void LoadingBackdrop::updateStir()
{
    stirStrength = 0.f;

    if (!isCursorInside || simulationTimeStep <= 0.f)
    {
        hasPreviousCursorCells = false;
        return;
    }

    auto currentCursorCells = convertPointToCells(cursorPoint);

    if (!hasPreviousCursorCells)
    {
        previousCursorCells = currentCursorCells;
        hasPreviousCursorCells = true;
        return;
    }

    auto travelled = Graphics::Point {currentCursorCells.x - previousCursorCells.x,
                                      currentCursorCells.y - previousCursorCells.y};
    auto velocity = limitLength({travelled.x / simulationTimeStep * stirGain,
                                 travelled.y / simulationTimeStep * stirGain},
                                maximumStirSpeedCellsPerSecond);

    stirFromCells = previousCursorCells;
    stirToCells = currentCursorCells;
    stirVelocityCellsPerSecond = velocity;

    if (travelled.length() > 0.f)
        stirStrength = 1.f - std::exp(-stirFollowRatePerSecond * simulationTimeStep);

    previousCursorCells = currentCursorCells;
}

void LoadingBackdrop::update(Threads::FrameTime frameTime)
{
    elapsedSeconds = std::fmod(elapsedSeconds + frameTime.delta, timeWrapSeconds);
    simulationTimeStep = std::min((float) frameTime.delta, maximumTimeStepSeconds);
    readCursorPosition();
    updateStir();
}

void LoadingBackdrop::resetFluid(const FluidGridSize& size)
{
    gridSize = size;
    auto cellCount = size.getCellCount();
    constexpr auto floatsPerStateCell = 4;

    fluidBuffers.reset();
    fluidBuffers.emplace(FluidBuffers {
        makeZeroedStorageBuffer(cellCount * floatsPerStateCell),
        makeZeroedStorageBuffer(cellCount * floatsPerStateCell),
        makeZeroedStorageBuffer(cellCount),
        makeZeroedStorageBuffer(cellCount),
        makeZeroedStorageBuffer(cellCount),
    });

    isFluidResetPending = false;
}

void LoadingBackdrop::simulateFluid(Frame& frame)
{
    auto& buffers = *fluidBuffers;
    auto columns = (float) gridSize.columns;
    auto rows = (float) gridSize.rows;

    auto pass = frame.beginCompute();

    advectKernel.source = buffers.state;
    advectKernel.destination = buffers.advectedState;
    advectKernel.gridColumns = columns;
    advectKernel.gridRows = rows;
    advectKernel.timeStep = simulationTimeStep;
    advectKernel.stirFrom = Maths::Vec2 {stirFromCells.x, stirFromCells.y};
    advectKernel.stirTo = Maths::Vec2 {stirToCells.x, stirToCells.y};
    advectKernel.stirVelocity =
        Maths::Vec2 {stirVelocityCellsPerSecond.x, stirVelocityCellsPerSecond.y};
    advectKernel.stirStrength = stirStrength;
    pass.dispatch(advectKernel, gridSize.columns, gridSize.rows);

    divergenceKernel.state = buffers.advectedState;
    divergenceKernel.divergence = buffers.divergence;
    divergenceKernel.gridColumns = columns;
    divergenceKernel.gridRows = rows;
    pass.dispatch(divergenceKernel, gridSize.columns, gridSize.rows);

    pressureKernel.divergence = buffers.divergence;
    pressureKernel.gridColumns = columns;
    pressureKernel.gridRows = rows;

    for (auto iteration = 0; iteration < pressureIterationCount; ++iteration)
    {
        auto isEvenIteration = iteration % 2 == 0;
        pressureKernel.pressure =
            isEvenIteration ? buffers.pressure : buffers.nextPressure;
        pressureKernel.nextPressure =
            isEvenIteration ? buffers.nextPressure : buffers.pressure;
        pass.dispatch(pressureKernel, gridSize.columns, gridSize.rows);
    }

    projectKernel.state = buffers.advectedState;
    projectKernel.pressure = buffers.pressure;
    projectKernel.projected = buffers.state;
    projectKernel.gridColumns = columns;
    projectKernel.gridRows = rows;
    pass.dispatch(projectKernel, gridSize.columns, gridSize.rows);
}

void LoadingBackdrop::render(Frame& frame)
{
    auto logicalSize = frame.logicalSize();
    auto aspect = logicalSize.x / logicalSize.y;
    viewHeightInPoints = logicalSize.y;

    auto wantedGridSize = calculateGridSize(aspect);

    if (isFluidResetPending || !fluidBuffers
        || !isSameGridSize(gridSize, wantedGridSize))
        resetFluid(wantedGridSize);

    simulateFluid(frame);

    shader.time = static_cast<float>(elapsedSeconds);
    shader.aspect = aspect;
    shader.fluidState = fluidBuffers->state;
    shader.gridColumns = (float) gridSize.columns;
    shader.gridRows = (float) gridSize.rows;
    shader.viewColumns = (float) gridSize.viewColumns;
    shader.viewRows = (float) gridSize.viewRows;
    shader.marginCells = (float) fluidMarginCellCount;

    auto pass = frame.beginPass({Graphics::Color {abyssRed, abyssGreen, abyssBlue}});
    pass.draw(shader);
}
