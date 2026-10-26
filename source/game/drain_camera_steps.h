/* Generated from MapEvent_DrainageValveInteract case 9. */
bool sh_drain_camera_sample(int32_t ticks, bool ball, ShPathPoint *position, ShPathPoint *look)
{
    int i, lightPosIdx;
    ShPathPoint lightPos, lookAtPos;
for (i = 1; i < 6; i++)
            {
                if (ticks < s_drain_camera_times[i])
                {
                    break;
                }
            }

            if (i == 6 || (!ball && i > 3))
            {
                return false;
            }
            else
            {
                lightPosIdx = i >> 1;

                if (ball)
                {
                    lightPosIdx += 2;
                }

                if (i & 0x1)
                {
                    lightPos.vx = s_drain_camera_positions[lightPosIdx].vx;
                    lightPos.vy = s_drain_camera_positions[lightPosIdx].vy;
                    lightPos.vz = s_drain_camera_positions[lightPosIdx].vz;

                    lookAtPos.vx = s_drain_camera_looks[lightPosIdx].vx;
                    lookAtPos.vy = s_drain_camera_looks[lightPosIdx].vy;
                    lookAtPos.vz = s_drain_camera_looks[lightPosIdx].vz;
                }
                else
                {
                    lightPos.vx = s_drain_camera_positions[lightPosIdx - 1].vx + (((s_drain_camera_positions[lightPosIdx].vx - s_drain_camera_positions[lightPosIdx - 1].vx) * (ticks - s_drain_camera_times[i - 1])) / (s_drain_camera_times[i] - s_drain_camera_times[i - 1]));
                    lightPos.vy = s_drain_camera_positions[lightPosIdx - 1].vy + (((s_drain_camera_positions[lightPosIdx].vy - s_drain_camera_positions[lightPosIdx - 1].vy) * (ticks - s_drain_camera_times[i - 1])) / (s_drain_camera_times[i] - s_drain_camera_times[i - 1]));
                    lightPos.vz = s_drain_camera_positions[lightPosIdx - 1].vz + (((s_drain_camera_positions[lightPosIdx].vz - s_drain_camera_positions[lightPosIdx - 1].vz) * (ticks - s_drain_camera_times[i - 1])) / (s_drain_camera_times[i] - s_drain_camera_times[i - 1]));

                    lookAtPos.vx = s_drain_camera_looks[lightPosIdx - 1].vx + (((s_drain_camera_looks[lightPosIdx].vx - s_drain_camera_looks[lightPosIdx - 1].vx) * (ticks - s_drain_camera_times[i - 1])) / (s_drain_camera_times[i] - s_drain_camera_times[i - 1]));
                    lookAtPos.vy = s_drain_camera_looks[lightPosIdx - 1].vy + (((s_drain_camera_looks[lightPosIdx].vy - s_drain_camera_looks[lightPosIdx - 1].vy) * (ticks - s_drain_camera_times[i - 1])) / (s_drain_camera_times[i] - s_drain_camera_times[i - 1]));
                    lookAtPos.vz = s_drain_camera_looks[lightPosIdx - 1].vz + (((s_drain_camera_looks[lightPosIdx].vz - s_drain_camera_looks[lightPosIdx - 1].vz) * (ticks - s_drain_camera_times[i - 1])) / (s_drain_camera_times[i] - s_drain_camera_times[i - 1]));
                }
        *position = lightPos;
        *look = lookAtPos;
        return true;
    }
}
