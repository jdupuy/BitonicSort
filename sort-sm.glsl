#define THREAD_COUNT 64u

uniform uint u_ArraySize;
uniform uvec2 u_LoopValue;

layout(std430, binding = LOCATION_ARRAY) buffer ArrayBuffer {
    uint u_ArrayData[];
};

layout(local_size_x = THREAD_COUNT, local_size_y = 1, local_size_z = 1) in;

shared uint s_ArrayData[THREAD_COUNT * 2];

void LoadData()
{
    s_ArrayData[2 * gl_LocalInvocationID.x + 0] = u_ArrayData[2 * gl_GlobalInvocationID.x + 0];
    s_ArrayData[2 * gl_LocalInvocationID.x + 1] = u_ArrayData[2 * gl_GlobalInvocationID.x + 1];
    barrier();
}

void SortData()
{
    const uint threadID = gl_LocalInvocationID.x;
    const uint d2 = u_LoopValue.y;

    for (uint d1 = u_LoopValue.x; d1 >= 1u; d1/= 2u) {
        const uint mask = 0xFFFFFFFEu * d1;
        const uint i1 = ((threadID << 1) & mask) | (threadID & ~(mask >> 1));
        const uint i2 = i1 | d1;
        const uint t1 = s_ArrayData[i1];
        const uint t2 = s_ArrayData[i2];
        const uint minValue = t1 < t2 ? t1 : t2;
        const uint maxValue = t1 < t2 ? t2 : t1;

        if ((gl_GlobalInvocationID.x & d2) == 0u) {
            s_ArrayData[i1] = minValue;
            s_ArrayData[i2] = maxValue;
        } else {
            s_ArrayData[i1] = maxValue;
            s_ArrayData[i2] = minValue;
        }

        barrier();
    }
}

void SaveData()
{
    u_ArrayData[2 * gl_GlobalInvocationID.x + 0] = s_ArrayData[2 * gl_LocalInvocationID.x + 0];
    u_ArrayData[2 * gl_GlobalInvocationID.x + 1] = s_ArrayData[2 * gl_LocalInvocationID.x + 1];
}

void main()
{
    if (gl_GlobalInvocationID.x >= u_ArraySize / 2u)
        return;

    LoadData();
    SortData();
    SaveData();
}
