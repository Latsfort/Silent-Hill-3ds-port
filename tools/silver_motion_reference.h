/* Generated from MapEvent_PianoPuzzleInteract cases 12 and 13. */
static void reference_swing(void) {

            Event_WaitTimer(Q12(0.5f), false);

            g_WorldObject_SilverH.rotation.vx += Q12_MULT_PRECISE(g_DeltaTime, Q12(0.0555f));
            g_WorldObject_SilverH.position.vy  = FP_FROM((Math_Cos(g_WorldObject_SilverH.rotation.vx) * Q12_ANGLE(-43.2f)), Q12_SHIFT) - Q12(2.5601f);
            g_WorldObject_SilverH.position.vz  = FP_FROM((Math_Sin(g_WorldObject_SilverH.rotation.vx) * Q12_ANGLE(-43.2f)), Q12_SHIFT) + Q12(23.73f);
            
}
static void reference_fall(void) {

            D_800DD4FC                 += g_GravitySpeed;
            g_WorldObject_SilverH.rotation.vx += Q12_MULT_PRECISE(g_DeltaTime, Q12(0.75f));
            g_WorldObject_SilverH.position.vy += Q12_MULT_PRECISE(g_DeltaTime, D_800DD4FC);
            g_WorldObject_SilverH.position.vz += Q12_MULT_PRECISE(g_DeltaTime, Q12(-0.5f));

            if (g_WorldObject_SilverH.position.vy >= Q12(0.0f))
            {
                SD_Call(1417);
                SysWork_StateStepIncrement(0);
            }
            
}
