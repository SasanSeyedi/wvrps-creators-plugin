import unreal
sub = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
unreal.log("RXTEST subsystem ok, signed_in=" + str(sub.is_signed_in()))
