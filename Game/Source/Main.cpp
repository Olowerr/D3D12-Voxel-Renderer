
#include "App.h"

int main()
{
	srand((uint32_t)time(nullptr));

	App voxelWorld;
	voxelWorld.run();

	return 0;
}
