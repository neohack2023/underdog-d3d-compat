// SPDX-License-Identifier: MIT
// Tier 3 witness: 65 UAV descriptor slots, only slot 64 populated.
// Index is runtime uniform, no access to undefined slots.
RWStructuredBuffer<uint> outputs[65] : register(u0);
cbuffer Selector : register(b0) { uint selected; uint value; };
[numthreads(1,1,1)]
void main(uint3 tid : SV_DispatchThreadID) {
    outputs[NonUniformResourceIndex(selected)][0] = value;
}
