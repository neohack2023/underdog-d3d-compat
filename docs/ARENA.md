# Underdog Arena | original hardware compatibility fixture

This is an original interactive 3D renderer demo, not a completed commercial game.

The scene loads an owned 12-triangle OBJ cube, builds a median-split CPU BVH and uploads its triangle and node arrays to GPU-accessible buffers. A D3D12 compute shader performs GPU ray/triangle traversal, writes color into a structured UAV buffer, and copies the buffer into the swap-chain backbuffer with a 128 by 128 RGBA8 placed-footprint transfer. The DXGI swap chain presents the rendered scene in an 800 by 450 window.

Use left and right arrows to move the camera. The command tools/run_arena_hd520.cmd starts a bounded manual demonstration on Windows after the project has been built. Run from the repository root.

Accepted on Intel HD Graphics 520, driver 22.20.16.4836: 120 consecutive Presents with deterministic moving-camera input and healthy D3D12 device. Median GPU BVH render+copy was approximately 1.366 ms at 128 by 128. These results are not game FPS or a promise on other devices.

Known limitation: the first typed-texture UAV pipeline failed at CreateComputePipelineState (E_INVALIDARG) on this driver. The accepted path uses a known-working structured UAV buffer and aligned GPU CopyTextureRegion into the real swap chain. The failure is retained in local AIOS test evidence.

This reference application does not include third-party protected-game interception, driver replacement, or anti-cheat modifications. Upstream game adaptation is a separate gate.

Validated additional OBJ mesh scene options: 800 triangles (1,023 BVH nodes, 60 Presents) and 3,200 triangles (4,095 BVH nodes, 60 Presents). Both had moving-camera acceptance runs and no device removal. Specify the mesh path as the fourth argument to the executable, after frame count and camera mode.
