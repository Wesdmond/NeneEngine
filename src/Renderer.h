#pragma once
#include <memory>


#define DECLARE_SINGLETON(className) \
public: \
    static className& GetInstance() { \
        static className instance; \
        return instance; \
    } \
private: \
    className() = default; \
    ~className() = default; \
    className(const className&) = delete; \
    className& operator=(const className&) = delete;


class Renderer
{
public:
    virtual void Initialize() = 0;
    virtual void Render() = 0;
    virtual void Shutdown() = 0;
    virtual ~Renderer() = default;

protected:
    Renderer() = default;
};

enum class RenderAPI { DirectX12, Vulkan };

class RendererFactory
{
public:
    static std::unique_ptr<Renderer> CreateRenderer(RenderAPI api);
};