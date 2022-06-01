
uniform uint u_ArraySize;
uniform uvec2 u_LoopValue;

layout(std430, binding = LOCATION_ARRAY) buffer ArrayBuffer {
    uint u_ArrayData[];
};

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

void main()
{
    const uint threadID = gl_GlobalInvocationID.x;

    if (threadID >= u_ArraySize / 2)
        return;

    const uint d1 = u_LoopValue.x;
    const uint d2 = u_LoopValue.y;
    const uint mask = 0xFFFFFFFEu * d1;
    const uint i1 = ((threadID << 1) & mask) | (threadID & ~(mask >> 1));
    const uint i2 = i1 | d1;
    const uint t1 = u_ArrayData[i1];
    const uint t2 = u_ArrayData[i2];
    const uint minValue = t1 < t2 ? t1 : t2;
    const uint maxValue = t1 < t2 ? t2 : t1;

    if ((threadID & d2) == 0u) {
        u_ArrayData[i1] = minValue;
        u_ArrayData[i2] = maxValue;
    } else {
        u_ArrayData[i1] = maxValue;
        u_ArrayData[i2] = minValue;
    }
}
