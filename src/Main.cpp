//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12MeshletRender.h"
#include <iostream>

void EnableConsole()
{
    AllocConsole(); // Allocate a console
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout); // Redirect stdout to console
    std::cout.clear(); // Ensure cout is in a good state
}

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    //EnableConsole();
    D3D12MeshletRender sample(1920, 1080, L"Nene Engine");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}
