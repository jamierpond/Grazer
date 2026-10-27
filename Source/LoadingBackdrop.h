#pragma once

#include <eacp/GPU/GPU.h>

// A slow, near-black domain-warped field with a faint moving sheen, drawn over
// a tab until its first page has loaded so a fresh tab never flashes grey.
struct LoadingBackdropShader final : eacp::GPU::ShaderProgram
{
    LoadingBackdropShader();

    void define() override;

    eacp::GPU::Uniform<eacp::GPU::Float> time;
    eacp::GPU::Uniform<eacp::GPU::Float> aspect;

    EACP_SHADER(time, aspect)

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
};

struct LoadingBackdrop final : eacp::GPU::GPUView
{
    LoadingBackdrop();

    void showOver(eacp::Graphics::View& parent);
    void hide();

    void update(eacp::Threads::FrameTime frameTime) override;
    void render(eacp::GPU::Frame& frame) override;

private:
    LoadingBackdropShader shader;
    double elapsedSeconds = 0.0;
};
