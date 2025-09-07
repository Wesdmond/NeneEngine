#pragma once
#include "DX12App.h"

class NeneApp : public DX12App
{
public:
    NeneApp(HINSTANCE mhAppInst, HWND mhMainWnd);
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

protected:
    void PopulateCommandList();    
};