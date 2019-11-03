inline move_spec
DefaultMoveSpec(void)
{
    move_spec result = {};
    result.isUnitMaxAccelVector = false;
    result.speed = 1.0f;
    result.drag = 0.0f;

    return result;
}


