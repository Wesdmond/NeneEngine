#pragma once
#include "DX12App.h"
#include "Common/GeometryGenerator.h"
#include "Common/Camera.h"

class NeneApp : public DX12App
{
public:
    NeneApp(HINSTANCE mhAppInst, HWND mhMainWnd);
    bool Initialize() override;
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

protected:
    void PopulateCommandList();
    bool LoadAssets();

private:
    Camera m_camera;
};