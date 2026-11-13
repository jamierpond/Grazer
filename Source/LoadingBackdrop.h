#pragma once

#include <eacp/GPU/GPU.h>

#include <optional>

// A slow, near-black domain-warped field with a faint moving sheen, drawn over
// a tab until its first page has loaded so a fresh tab never flashes grey.
struct LoadingBackdropShader final : eacp::GPU::ShaderProgram
{
    LoadingBackdropShader();

    void define() override;

    eacp::GPU::Uniform<eacp::GPU::Float> time;
    eacp::GPU::Uniform<eacp::GPU::Float> aspect;
    eacp::GPU::Uniform<eacp::GPU::InputBuffer> fluidState;
    eacp::GPU::Uniform<eacp::GPU::Float> gridColumns;
    eacp::GPU::Uniform<eacp::GPU::Float> gridRows;
    eacp::GPU::Uniform<eacp::GPU::Float> viewColumns;
    eacp::GPU::Uniform<eacp::GPU::Float> viewRows;
    eacp::GPU::Uniform<eacp::GPU::Float> marginCells;

    EACP_SHADER(time,
                aspect,
                fluidState,
                gridColumns,
                gridRows,
                viewColumns,
                viewRows,
                marginCells)

private:
    eacp::GPU::Float3 makeColour(float red, float green, float blue);
    eacp::GPU::Float hashLatticePoint(const eacp::GPU::Float2& point);
    eacp::GPU::Float sampleValueNoise(const eacp::GPU::Float2& point);
    eacp::GPU::Float2 advanceToNextOctave(const eacp::GPU::Float2& point);
    eacp::GPU::Float sampleFractalNoise(const eacp::GPU::Float2& start);
    eacp::GPU::Float2 calculateWarpedPoint(const eacp::GPU::Float2& point,
                                           const eacp::GPU::Float& drift);
    eacp::GPU::Float calculateSheen(const eacp::GPU::Float& height,
                                    const eacp::GPU::Float& drift);
    eacp::GPU::Float calculateVignette(const eacp::GPU::Float2& screenPoint);
    eacp::GPU::Float4 readFluidCell(const eacp::GPU::Float2& cell);
    eacp::GPU::Float2
        sampleDisplacementInCells(const eacp::GPU::Float2& screenPoint);
};

// Semi-Lagrangian advection of velocity and displacement (xy velocity in cells
// per second, zw displacement in cells), plus the cursor stir, dissipation and
// relaxation. Reads outside the grid clamp to the border cell: outflow.
struct FluidAdvectKernel final : eacp::GPU::ComputeProgram
{
    FluidAdvectKernel();

    void define() override;

    eacp::GPU::Uniform<eacp::GPU::InputBuffer> source;
    eacp::GPU::Uniform<eacp::GPU::OutputBuffer> destination;
    eacp::GPU::Uniform<eacp::GPU::Float> gridColumns;
    eacp::GPU::Uniform<eacp::GPU::Float> gridRows;
    eacp::GPU::Uniform<eacp::GPU::Float> timeStep;
    eacp::GPU::Uniform<eacp::GPU::Float2> stirFrom;
    eacp::GPU::Uniform<eacp::GPU::Float2> stirTo;
    eacp::GPU::Uniform<eacp::GPU::Float2> stirVelocity;
    eacp::GPU::Uniform<eacp::GPU::Float> stirStrength;

    EACP_SHADER(source,
                destination,
                gridColumns,
                gridRows,
                timeStep,
                stirFrom,
                stirTo,
                stirVelocity,
                stirStrength)

private:
    eacp::GPU::Float4 readCell(const eacp::GPU::Float2& cell);
    eacp::GPU::Float4 sampleBilinear(const eacp::GPU::Float2& point);
    eacp::GPU::Float calculateStirFalloff(const eacp::GPU::Float2& cell);
};

struct FluidDivergenceKernel final : eacp::GPU::ComputeProgram
{
    FluidDivergenceKernel();

    void define() override;

    eacp::GPU::Uniform<eacp::GPU::InputBuffer> state;
    eacp::GPU::Uniform<eacp::GPU::OutputBuffer> divergence;
    eacp::GPU::Uniform<eacp::GPU::Float> gridColumns;
    eacp::GPU::Uniform<eacp::GPU::Float> gridRows;

    EACP_SHADER(state, divergence, gridColumns, gridRows)

private:
    eacp::GPU::Float2 readVelocity(const eacp::GPU::Float& column,
                                   const eacp::GPU::Float& row);
};

// Pressure is zero outside the grid, so flow leaves through the border freely.
struct FluidPressureKernel final : eacp::GPU::ComputeProgram
{
    FluidPressureKernel();

    void define() override;

    eacp::GPU::Uniform<eacp::GPU::InputBuffer> pressure;
    eacp::GPU::Uniform<eacp::GPU::InputBuffer> divergence;
    eacp::GPU::Uniform<eacp::GPU::OutputBuffer> nextPressure;
    eacp::GPU::Uniform<eacp::GPU::Float> gridColumns;
    eacp::GPU::Uniform<eacp::GPU::Float> gridRows;

    EACP_SHADER(pressure, divergence, nextPressure, gridColumns, gridRows)
};

struct FluidProjectKernel final : eacp::GPU::ComputeProgram
{
    FluidProjectKernel();

    void define() override;

    eacp::GPU::Uniform<eacp::GPU::InputBuffer> state;
    eacp::GPU::Uniform<eacp::GPU::InputBuffer> pressure;
    eacp::GPU::Uniform<eacp::GPU::OutputBuffer> projected;
    eacp::GPU::Uniform<eacp::GPU::Float> gridColumns;
    eacp::GPU::Uniform<eacp::GPU::Float> gridRows;

    EACP_SHADER(state, pressure, projected, gridColumns, gridRows)
};

struct FluidGridSize
{
    int viewColumns = 0;
    int viewRows = 0;
    int columns = 0;
    int rows = 0;

    int getCellCount() const { return columns * rows; }
};

struct FluidBuffers
{
    eacp::GPU::Buffer state;
    eacp::GPU::Buffer advectedState;
    eacp::GPU::Buffer divergence;
    eacp::GPU::Buffer pressure;
    eacp::GPU::Buffer nextPressure;
};

struct LoadingBackdrop final : eacp::GPU::GPUView
{
    LoadingBackdrop();

    void showOver(eacp::Graphics::View& viewToCover);
    void hide();

    void update(eacp::Threads::FrameTime frameTime) override;
    void render(eacp::GPU::Frame& frame) override;

private:
    void readCursorPosition();
    void resetFluid(const FluidGridSize& size);
    void updateStir();
    void simulateFluid(eacp::GPU::Frame& frame);
    eacp::Graphics::Point convertPointToCells(eacp::Graphics::Point point) const;

    LoadingBackdropShader shader;
    FluidAdvectKernel advectKernel;
    FluidDivergenceKernel divergenceKernel;
    FluidPressureKernel pressureKernel;
    FluidProjectKernel projectKernel;

    std::optional<FluidBuffers> fluidBuffers;
    FluidGridSize gridSize;
    bool isFluidResetPending = true;

    double elapsedSeconds = 0.0;
    float simulationTimeStep = 0.f;

    bool isCursorInside = false;
    bool hasPreviousCursorCells = false;
    eacp::Graphics::Point cursorPoint;
    eacp::Graphics::Point previousCursorCells;
    eacp::Graphics::Point stirFromCells;
    eacp::Graphics::Point stirToCells;
    eacp::Graphics::Point stirVelocityCellsPerSecond;
    float stirStrength = 0.f;
    float viewHeightInPoints = 1.f;
};
