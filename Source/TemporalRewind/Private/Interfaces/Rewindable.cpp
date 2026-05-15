// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#include "Interfaces/Rewindable.h"

void IRewindable::ApplyStateInterpolated(
	const FTemporalSnapshot& Before, const FTemporalSnapshot& After, float Alpha)
{
	// Default: snap to the floor snapshot. Existing actors require no changes.
	Execute_ApplyState(_getUObject(), Before);
}
