// SPDX-License-Identifier: MIT
// Each 32-thread dispatch tests actual DXIL 6.1 subgroup wave operations.
RWStructuredBuffer<uint2> outputData : register(u0);
[numthreads(32,1,1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint subgroupCount = WaveGetLaneCount();
    uint activeCount = WaveActiveSum(1u);
    outputData[tid.x] = uint2(subgroupCount, activeCount);
}
